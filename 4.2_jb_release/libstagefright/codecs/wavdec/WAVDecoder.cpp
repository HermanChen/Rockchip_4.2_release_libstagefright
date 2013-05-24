/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "WAVDecoder.h"


#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MetaData.h>
#include"PCM.h"



#define WAV_OUTPUT_LEN 8192
namespace android {

WAVDecoder::WAVDecoder(const sp<MediaSource> &source)
    : mSource(source),
      mNumChannels(0),
      mSampleRate(0),
      mStarted(false),
      mBufferGroup(NULL),
      mWAVDecExt(NULL),
      mNeedInputNumLen(0),
      mNeedInputBuf(NULL),
      mMsAdpcm(NULL),
      mPCM(NULL),
      mAnchorTimeUs(0),
      mNumFramesOutput(0),
      mInputBuffer(NULL) {
    init();
}

void WAVDecoder::init() {
    sp<MetaData> srcFormat = mSource->getFormat();

    CHECK(srcFormat->findInt32(kKeyChannelCount, &mNumChannels));
    CHECK(srcFormat->findInt32(kKeySampleRate, &mSampleRate));

    mMeta = new MetaData;
    mMeta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_RAW);
    mMeta->setInt32(kKeyChannelCount, mNumChannels);
    mMeta->setInt32(kKeySampleRate, mSampleRate);

    int64_t durationUs;
    if (srcFormat->findInt64(kKeyDuration, &durationUs)) {
        mMeta->setInt64(kKeyDuration, durationUs);
    }

    mMeta->setCString(kKeyDecoderComponent, "WAVDecoder");
}

WAVDecoder::~WAVDecoder() {
    if (mStarted) {
        stop();
    }

	if (mWAVDecExt)
    {
        delete mWAVDecExt;
        mWAVDecExt = NULL;
    }
	 if(mMsAdpcm)
	 {
	 	delete mMsAdpcm;
	    mMsAdpcm = NULL;
	 }
	 if(mPCM)
	 {
	 	delete mPCM;
	    mPCM = NULL;
	 }
	 if(mNeedInputBuf)
	 {
	 	free(mNeedInputBuf);
		mNeedInputBuf = NULL;
	 }
}

status_t WAVDecoder::start(MetaData *params) {
    CHECK(!mStarted);

	mWAVDecExt = new WaveFormatExStruct;

	if(!mWAVDecExt)
		return UNKNOWN_ERROR;
	memset(mWAVDecExt,0,sizeof(WaveFormatExStruct));
    mBufferGroup = new MediaBufferGroup;
    mBufferGroup->add_buffer(new MediaBuffer(WAV_OUTPUT_LEN * 2));
	mSource->start();
    mAnchorTimeUs = 0;
    mNumFramesOutput = 0;
    mStarted = true;

    return OK;
}

status_t WAVDecoder::stop() {
    CHECK(mStarted);

    if (mInputBuffer) {
        mInputBuffer->release();
        mInputBuffer = NULL;
    }

    delete mBufferGroup;
    mBufferGroup = NULL;

    mSource->stop();

    mStarted = false;

    return OK;
}

sp<MetaData> WAVDecoder::getFormat() {
    return mMeta;
}

status_t WAVDecoder::read(
        MediaBuffer **out, const ReadOptions *options) {
    status_t err;

    *out = NULL;

    int64_t seekTimeUs;
    ReadOptions::SeekMode mode;
    bool bSeekFlag = false;
    if (options && options->getSeekTo(&seekTimeUs, &mode)) {
        CHECK(seekTimeUs >= 0);
		//when the user seek the video,then will entry into this case,so you must do some reset fun in this case
		//by Charles Chenjan at 13th,2011
        mNumFramesOutput = 0;
        bSeekFlag = true;

        if (mInputBuffer) {
            mInputBuffer->release();
            mInputBuffer = NULL;
        }
		mAnchorTimeUs = -1LL;//-1 mean after seek ,need Assignment first frame time after seek

    } else {
        seekTimeUs = -1;
    }

readdata:
    if (mInputBuffer == NULL) {
        if (bSeekFlag == true) {
            err = mSource->read(&mInputBuffer, options);
            bSeekFlag = false;
        } else {
            err = mSource->read(&mInputBuffer, NULL);
        }

        if (err != OK) {
            return err;
        }


		if (mWAVDecExt->FormatTag == PVWAV_UNKNOWN_AUDIO_FORMAT)
		{


			memcpy(mWAVDecExt,mInputBuffer->data(),sizeof(WaveFormatExStruct));
			            //Output Port Parameters
			mInputBuffer->release();
			mInputBuffer = NULL;

			ALOGI("SR %d ch %d fm %d",mWAVDecExt->SamplesPerSec,mWAVDecExt->Channels,mWAVDecExt->FormatTag);
			//Set the Resize flag to send the port settings changed callback
			if(mWAVDecExt->FormatTag == PVWAV_PCM_AUDIO_FORMAT)
			{
				mNeedInputNumLen = PCM_OUT_NUM * sizeof(int16_t) *mWAVDecExt->Channels;

				if(mWAVDecExt->BitsPerSample == 8)
					mNeedInputNumLen = mNeedInputNumLen >> 1;
			}
			else if(mWAVDecExt->FormatTag == PVWAV_ADPCM_FORMAT || mWAVDecExt->FormatTag == PVWAV_IMA_ADPCM_FORMAT)
			{
				mPCM = new tPCM;
				if(!mPCM)//creat failed ,return;
				{
					return UNKNOWN_ERROR;
				}

				mPCM->usByteRate = mWAVDecExt->AvgBytesPerSec;
			    mPCM->usSampleRate = (unsigned short)mWAVDecExt->SamplesPerSec;
			    mPCM->usBytesPerBlock = mWAVDecExt->BlockAlign;

			    mPCM->ucChannels = (unsigned char)mWAVDecExt->Channels;
			    mPCM->sPCMHeader.ucChannels = (unsigned long)mWAVDecExt->Channels;

			    mPCM->usSamplesPerBlock = mWAVDecExt->SamplesPerBlock;;
			    mPCM->sPCMHeader.usSamplesPerBlock = (unsigned short)mWAVDecExt->SamplesPerBlock;

			    mPCM->uBitsPerSample = mWAVDecExt->BitsPerSample;
			    mPCM->sPCMHeader.uBitsPerSample = (unsigned short)mWAVDecExt->BitsPerSample;

			    mPCM->wFormatTag = mWAVDecExt->FormatTag;
				mPCM->ulLength = 0x7fffffff;//data chunk length
				if(mWAVDecExt->FormatTag == PVWAV_ADPCM_FORMAT)
				{
					if (mPCM->ulLength == 0xffffffff)
						mPCM->ulLength = 0x7fffffff;
					mMsAdpcm = new MSADPCM_PRIVATE;
					if(!mMsAdpcm)
					{
						return UNKNOWN_ERROR;
					}
					omx_msadpcm_dec_init (mPCM, mMsAdpcm);
				}
				else
					mWAVDecExt->Channels = 2;//the IMA decoder,though the channel is mone or stero , the out data is stero data

				mNeedInputNumLen = mWAVDecExt->BlockAlign;
			}
			else if(mWAVDecExt->FormatTag == PVWAV_UNKNOWN_AUDIO_FORMAT)
			{
				return UNKNOWN_ERROR;
			}
			if(mNeedInputBuf== NULL)
				mNeedInputBuf = malloc(mNeedInputNumLen);

			if((mNumChannels != mWAVDecExt->Channels)||(mSampleRate != mWAVDecExt->SamplesPerSec))
			{
				mNumChannels = mWAVDecExt->Channels;
				mSampleRate = mWAVDecExt->SamplesPerSec;
				mMeta->setInt32(kKeySampleRate, mSampleRate);
				mMeta->setInt32(kKeyChannelCount, mNumChannels);
				return INFO_FORMAT_CHANGED;
			}

			goto readdata;
		}

        int64_t timeUs;
        if (mInputBuffer->meta_data()->findInt64(kKeyTime, &timeUs)) {
			//if the audio codec can't make sure that one in one out,will not change the mAnchorTimeUs/mNumFramesOutput value
			//you could add the tmp time para Charles Chen jan,13th ,2011
			if(mAnchorTimeUs == -1LL)
			{
				 mAnchorTimeUs = timeUs;
				 mNumFramesOutput = 0;
			}
        } else {
            // We must have a new timestamp after seeking.
            CHECK(seekTimeUs < 0);
        }
    }


	void * inputBuf = NULL;
	//process the input buf data len is enough or not;
	if(mInputBuffer->range_length() <  mNeedInputNumLen)
	{
		uint32_t nextLen = mNeedInputNumLen - mInputBuffer->range_length();
		uint32_t startPos = 0;

		inputBuf = mNeedInputBuf;

		do{
			memcpy(mNeedInputBuf + startPos,mInputBuffer->data()+mInputBuffer->range_offset(),mInputBuffer->range_length());
			startPos += mInputBuffer->range_length();//src buf start pos
			mInputBuffer->release();
			mInputBuffer = NULL;

            if (bSeekFlag == true) {
                err = mSource->read(&mInputBuffer, options);
                bSeekFlag = false;
            } else {
                err = mSource->read(&mInputBuffer, NULL);
            }

		    if (err != OK) {
		        return err;
		    }
			if(mInputBuffer->range_length() < nextLen)
			{
				nextLen -= mInputBuffer->range_length();
			}
			else
			{
				memcpy(mNeedInputBuf + startPos,mInputBuffer->data()+mInputBuffer->range_offset(),nextLen);
				mInputBuffer->set_range(mInputBuffer->range_offset()+nextLen, mInputBuffer->range_length()-nextLen);
				nextLen = 0;
			}

		}while(nextLen);
	}
	else
	{
		inputBuf = mInputBuffer->data()+mInputBuffer->range_offset();
		mInputBuffer->set_range(mInputBuffer->range_offset()+ mNeedInputNumLen, mInputBuffer->range_length()-mNeedInputNumLen);
	}



    MediaBuffer *buffer;
    mBufferGroup->acquire_buffer(&buffer);

	uint32_t outputLen = 0;
	if(mWAVDecExt->FormatTag == PVWAV_PCM_AUDIO_FORMAT)
	{
		if(mWAVDecExt->BitsPerSample == 16)
		{
			outputLen = mNeedInputNumLen;
			memcpy(buffer->data(), inputBuf, mNeedInputNumLen);
		}
		else if(mWAVDecExt->BitsPerSample == 8)
		{
			outputLen = mNeedInputNumLen <<1;
			memset(buffer->data(),0,outputLen);
			char* dst = (char*)buffer->data();
			char* src = (char*)inputBuf;
			for(int i = 0; i < mNeedInputNumLen; i++)
				dst[i*2] = src[i];
		}
	}
	else if(mWAVDecExt->FormatTag == PVWAV_ADPCM_FORMAT)
	{
		memcpy(mMsAdpcm->block,inputBuf,mNeedInputNumLen);
		omx_msadpcm_decode_block (mMsAdpcm);
		outputLen = mMsAdpcm->samplesperblock*2*mMsAdpcm->channels;
		memcpy(buffer->data(),mMsAdpcm->samples,outputLen);
	}
	else if(mWAVDecExt->FormatTag == PVWAV_IMA_ADPCM_FORMAT)
	{
		outputLen  = 4*OMX_IMAADPCM_DEC((char *)inputBuf, mNeedInputNumLen, mPCM);
		memcpy(buffer->data(), mPCM->OutputBuff, outputLen);
	}


	if (mInputBuffer->range_length() == 0) {
        mInputBuffer->release();
        mInputBuffer = NULL;
    }
	buffer->set_range(0, outputLen);
    buffer->meta_data()->setInt64(
            kKeyTime,
            mAnchorTimeUs
                + (mNumFramesOutput * 1000000) / mSampleRate);

    mNumFramesOutput += outputLen / (2*mNumChannels);

    *out = buffer;

    return OK;
}

}  // namespace android
