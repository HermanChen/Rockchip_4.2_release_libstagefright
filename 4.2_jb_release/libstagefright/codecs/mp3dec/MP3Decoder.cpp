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

#include "MP3Decoder.h"

#include "include/pvmp3decoder_api.h"

#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MetaData.h>
#undef LOG_TAG
#define LOG_TAG "MP3Decoder"
namespace android {

MP3Decoder::MP3Decoder(const sp<MediaSource> &source)
    : mSource(source),
      mNumChannels(0),
      mSampleRate(0),
      mNumDecodedBuffers(0),
      mFrameCount(0),
      mStarted(false),
      mDropFrameCount(0),
      mBufferGroup(NULL),
      mConfig(new tPVMP3DecoderExternal),
      mDecoderBuf(NULL),
      mAnchorTimeUs(0),
      mNumFramesOutput(0),
      mInputBuffer(NULL) {

    init();
}

void MP3Decoder::init() {
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

    mMeta->setCString(kKeyDecoderComponent, "MP3Decoder");
#if SUPPORT_NO_ENOUGH_MAIN_DATA
	memset((void *)&mBufExt,0,sizeof(mBufExt));
#endif
}

MP3Decoder::~MP3Decoder() {
    if (mStarted) {
        stop();
    }

    delete mConfig;
    mConfig = NULL;
#if SUPPORT_NO_ENOUGH_MAIN_DATA
	if(mBufExt.buf)
	{
		free(mBufExt.buf);
		mBufExt.buf = NULL;
		mBufExt.len = 0;
	}
#endif
}

status_t MP3Decoder::start(MetaData *params) {
    CHECK(!mStarted);

    mBufferGroup = new MediaBufferGroup;
    mBufferGroup->add_buffer(new MediaBuffer(4608 * 2));

    mConfig->equalizerType = flat;
    mConfig->crcEnabled = false;

    uint32_t memRequirements = pvmp3_decoderMemRequirements();
    mDecoderBuf = malloc(memRequirements);

    pvmp3_InitDecoder(mConfig, mDecoderBuf);

    mSource->start();

    mAnchorTimeUs = 0;
    mNumFramesOutput = 0;
    mStarted = true;

    return OK;
}

status_t MP3Decoder::stop() {
    CHECK(mStarted);

    if (mInputBuffer) {
        mInputBuffer->release();
        mInputBuffer = NULL;
    }

    free(mDecoderBuf);
    mDecoderBuf = NULL;

    delete mBufferGroup;
    mBufferGroup = NULL;

    mSource->stop();

    mStarted = false;

    return OK;
}

sp<MetaData> MP3Decoder::getFormat() {
    return mMeta;
}

status_t MP3Decoder::read(
        MediaBuffer **out, const ReadOptions *options) {
    status_t err;

    *out = NULL;

    int64_t seekTimeUs;
    ReadOptions::SeekMode mode;
    if (options && options->getSeekTo(&seekTimeUs, &mode)) {
        CHECK(seekTimeUs >= 0);
		mDropFrameCount = 4;
        mNumFramesOutput = 0;
		mFrameCount = 0;
        if (mInputBuffer) {
            mInputBuffer->release();
            mInputBuffer = NULL;
        }
#if SUPPORT_NO_ENOUGH_MAIN_DATA
//when the audio do seek ,the len must be reset;
	if(mBufExt.buf)
	{
		mBufExt.len = 0;
	}
#endif
        // Make sure that the next buffer output does not still
        // depend on fragments from the last one decoded.
        pvmp3_InitDecoder(mConfig, mDecoderBuf);
    } else {
        seekTimeUs = -1;
    }

NEED_INIT:
    if (mInputBuffer == NULL) {
        err = mSource->read(&mInputBuffer, options);

        if (err != OK) {
            return err;
        }

        int64_t timeUs;
        if (mInputBuffer->meta_data()->findInt64(kKeyTime, &timeUs)) {
            if(mAnchorTimeUs != timeUs)
            {
            mAnchorTimeUs = timeUs;
            mNumFramesOutput = 0;
            }
        } else {
            // We must have a new timestamp after seeking.
            CHECK(seekTimeUs < 0);
        }
    }

    MediaBuffer *buffer;
    CHECK_EQ(mBufferGroup->acquire_buffer(&buffer), (status_t)OK);
#if SUPPORT_NO_ENOUGH_MAIN_DATA
	if(mBufExt.buf)//if buf != NULL ,the input buf always be mBufExt.buf.
	{
		uint32_t copyLen = 0;
		if((TMP_BUF_SIZE -mBufExt.len) >=  mInputBuffer->range_length())
		{
			copyLen = mInputBuffer->range_length();
		}
		else
		{
			copyLen = TMP_BUF_SIZE -mBufExt.len;
		}

		uint8 * src =  (uint8_t *)mInputBuffer->data() + mInputBuffer->range_offset();
		memcpy(mBufExt.buf + mBufExt.len,src,copyLen);
		mBufExt.len += copyLen;

		if(copyLen == mInputBuffer->range_length())
		{
			mInputBuffer->release();
			mInputBuffer = NULL;
		}
		else
		{
			 mInputBuffer->set_range(mInputBuffer->range_offset() + copyLen, mInputBuffer->range_length() - copyLen);
		}

		mConfig->pInputBuffer = mBufExt.buf;
		mConfig->inputBufferCurrentLength = mBufExt.len;
	}
	else
	{
		mConfig->pInputBuffer =
	        (uint8_t *)mInputBuffer->data() + mInputBuffer->range_offset();
		mConfig->inputBufferCurrentLength = mInputBuffer->range_length();
	}
#else
    mConfig->pInputBuffer =
        (uint8_t *)mInputBuffer->data() + mInputBuffer->range_offset();
	mConfig->inputBufferCurrentLength = mInputBuffer->range_length();
#endif

    mConfig->inputBufferMaxLength = 0;
    mConfig->inputBufferUsedLength = 0;

    mConfig->outputFrameSize = buffer->size() / sizeof(int16_t);
    mConfig->pOutputBuffer = static_cast<int16_t *>(buffer->data());

    ERROR_CODE decoderErr;
    if ((decoderErr = pvmp3_framedecoder(mConfig, mDecoderBuf))
            != NO_DECODING_ERROR) {
        ALOGV("mp3 decoder returned error %d", decoderErr);
		if(mConfig->inputBufferUsedLength > mConfig->inputBufferCurrentLength)
			mConfig->inputBufferUsedLength = mConfig->inputBufferCurrentLength;

        if (decoderErr != NO_ENOUGH_MAIN_DATA_ERROR) {

			mFrameCount++;
			ALOGE("--->error------>decoderErrr %d mFrameCount%d inputlen %d usedlen %d",decoderErr,mFrameCount,mConfig->inputBufferCurrentLength,mConfig->inputBufferUsedLength);

			if(mFrameCount < 10)
			{
				mConfig->outputFrameSize  = 0;

				buffer->set_range(0, 0);
				buffer->meta_data()->setInt64(kKeyTime,mAnchorTimeUs);
				if(mBufExt.buf)
				{
					mBufExt.len -= mConfig->inputBufferUsedLength;
					memcpy(mBufExt.buf,mBufExt.buf+ mConfig->inputBufferUsedLength,mBufExt.len);
				}
				else
				{
					if(mInputBuffer)
					{
						mInputBuffer->set_range(mInputBuffer->range_offset()+mConfig->inputBufferUsedLength, mInputBuffer->range_length()-mConfig->inputBufferUsedLength);
						if(mInputBuffer->range_length() == 0)
						{
							mInputBuffer->release();
							mInputBuffer = NULL;
						}
					}
				}

			}
			else
			{
				if(mInputBuffer)
				{
					 mInputBuffer->release();
		       		 mInputBuffer = NULL;
				}
				if(mBufExt.buf)
				{
					mBufExt.len = 0;
				}
				mConfig->outputFrameSize  = 0;
				mFrameCount = 0;
				buffer->set_range(0, 0);
				buffer->meta_data()->setInt64(kKeyTime,mAnchorTimeUs);
			}

			if(mNumDecodedBuffers == 0)
			{
				buffer->release();
				goto NEED_INIT;
			}

			*out = buffer;
			return OK;
        }
#if SUPPORT_NO_ENOUGH_MAIN_DATA
	mConfig->inputBufferUsedLength = 0;
	if(mBufExt.buf == NULL)
	{
		mBufExt.buf = (uint8*)malloc(TMP_BUF_SIZE);
		mBufExt.len = 0;
	}

	if(mBufExt.len == TMP_BUF_SIZE)
	{
		ALOGE("1024 bytes has err = NO_ENOUGH_MAIN_DATA_ERROR ");
		buffer->release();
        buffer = NULL;

        mInputBuffer->release();
        mInputBuffer = NULL;

        return UNKNOWN_ERROR;
	}
	else
	{
		if(mBufExt.len == 0 && mInputBuffer)
		{
			uint8 * src =  (uint8_t *)mInputBuffer->data() + mInputBuffer->range_offset();
			mBufExt.len = mInputBuffer->range_length();
			ALOGE("first copy len %d used len %d cur len %d mConfig->samplingRate %d src %2x%2x%2x%2x",
				mBufExt.len,mConfig->inputBufferUsedLength,mConfig->inputBufferCurrentLength,mConfig->samplingRate,src[0],src[1],src[2],src[3]);
			memcpy(mBufExt.buf,src,mBufExt.len);
			mInputBuffer->release();
			mInputBuffer = NULL;
		}
	}
#else
        // This is recoverable, just ignore the current frame and
        // play silence instead.
        memset(buffer->data(), 0, mConfig->outputFrameSize * sizeof(int16_t));
        mConfig->inputBufferUsedLength = mInputBuffer->range_length();
#endif
    }else {
        mNumDecodedBuffers++;

        if (mNumDecodedBuffers == 1) {
            if (mConfig) {
                if (mNumChannels != mConfig->num_channels) {
                    mMeta->setInt32(kKeyChannelCount, mConfig->num_channels);

                    buffer->release();
                    return INFO_FORMAT_CHANGED;
                } else if (mSampleRate != mConfig->samplingRate) {
                    mMeta->setInt32(kKeySampleRate, mConfig->samplingRate);

                    buffer->release();
                    return INFO_FORMAT_CHANGED;
                }
            }
        }
    }

	if(mDropFrameCount > 0)
	{
		mDropFrameCount--;
		memset(buffer->data(),0,mConfig->outputFrameSize * sizeof(int16_t));
	}
    buffer->set_range(
            0, mConfig->outputFrameSize * sizeof(int16_t));
#if SUPPORT_NO_ENOUGH_MAIN_DATA
	if(mBufExt.len)
	{
		/*if(mBufExt.len <= mConfig->inputBufferUsedLength)
			ALOGE("buf len %d used len %d",mBufExt.len,mConfig->inputBufferUsedLength);*/
		mBufExt.len -= mConfig->inputBufferUsedLength;
		memcpy(mBufExt.buf,mBufExt.buf+ mConfig->inputBufferUsedLength,mBufExt.len);
		if(mInputBuffer)
		{
			uint32_t copyLen = 0;
			if((TMP_BUF_SIZE -mBufExt.len) >=  mInputBuffer->range_length())
			{
				copyLen = mInputBuffer->range_length();
			}
			else
			{
				copyLen = TMP_BUF_SIZE -mBufExt.len;
			}

			uint8 * src =  (uint8_t *)mInputBuffer->data() + mInputBuffer->range_offset();
			memcpy(mBufExt.buf + mBufExt.len,src,copyLen);
			mBufExt.len += copyLen;
			if(copyLen == mInputBuffer->range_length())
			{
				mInputBuffer->release();
				mInputBuffer = NULL;
			}
			else
			{
				 mInputBuffer->set_range(mInputBuffer->range_offset() + copyLen, mInputBuffer->range_length() - copyLen);
			}

		}
	}
	else
	{
		mInputBuffer->set_range(
		mInputBuffer->range_offset() + mConfig->inputBufferUsedLength,
		mInputBuffer->range_length() - mConfig->inputBufferUsedLength);

		if (mInputBuffer->range_length() == 0) {
			mInputBuffer->release();
			mInputBuffer = NULL;
		}
	}
#else
    mInputBuffer->set_range(
            mInputBuffer->range_offset() + mConfig->inputBufferUsedLength,
            mInputBuffer->range_length() - mConfig->inputBufferUsedLength);

    if (mInputBuffer->range_length() == 0) {
        mInputBuffer->release();
        mInputBuffer = NULL;
    }
#endif

    if (buffer && mConfig)
	if (mConfig->samplingRate){
     	   buffer->meta_data()->setInt64(
                kKeyTime,
                mAnchorTimeUs
                    + (mNumFramesOutput * 1000000) / mConfig->samplingRate);
    } else {
	   buffer->meta_data()->setInt64(
                kKeyTime,
        		    0ll);

    }


    if (mNumChannels) {
        mNumFramesOutput += mConfig->outputFrameSize / mNumChannels;
    }

    *out = buffer;

    return OK;
}

}  // namespace android
