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

//#define LOG_NDEBUG 0
#define LOG_TAG "AVCDecoder_FLASH"
#include <utils/Log.h>

#include "AVCDecoder_FLASH.h"

#include "avcdec_api.h"
#include "avcdec_int.h"

#include <OMX_Component.h>

#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/Utils.h>
#include <media/stagefright/foundation/hexdump.h>

#if 1
#define ALOGE ALOGV
#else
#undef ALOGE
#undef ALOGD
#define ALOGE ALOGV
#define ALOGD ALOGV
#endif

#define LOG_TAG "sf_avc_dec"
#define FUN() ALOGE("-------->%s<------------- ",__func__);

namespace android {

#define DEBUG 0
#if DEBUG
static FILE* fp = NULL;
#endif

static const char kStartCode[4] = { 0x00, 0x00, 0x00, 0x01 };


AVCDecoder_FLASH::AVCDecoder_FLASH(const sp<MediaSource> &source)
    : mSource(source),
      mStarted(false),
      mInputBuffer(NULL),
      mPendingSeekTimeUs(-1),
      mPendingSeekMode(MediaSource::ReadOptions::SEEK_CLOSEST_SYNC),
      mTargetTimeUs(-1),
      mSPSSeen(false),
      mPPSSeen(false),
      mts_en(0),
      mUseInputTime(true),
      pOn2Dec(NULL){
     FUN()
#if DEBUG
	 if(fp == NULL)
	 {
	 	fp = fopen("/sdcard/avc.dat","wb");
	 }
#endif

    mFormat = new MetaData;
    mFormat->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_RAW);

    int32_t width, height;
	mSource->getFormat()->findInt32(kKeyisTs, &mts_en);
    CHECK(mSource->getFormat()->findInt32(kKeyWidth, &width));
    CHECK(mSource->getFormat()->findInt32(kKeyHeight, &height));
    mFormat->setInt32(kKeyWidth, width);
    mFormat->setInt32(kKeyHeight, height);

	//mFormat->setInt32(kKeyColorFormat,  0x7FA30C00  );
    //mFormat->setInt32(kKeyColorFormat, OMX_COLOR_FormatYUV420SemiPlanar);
    mFormat->setInt32(kKeyColorFormat, OMX_COLOR_FormatYUV420Planar);
    //mFormat->setInt32(kKeyColorFormat, OMX_COLOR_FormatYUV420SemiPlanar);//RK¸øTPIµÄ

#if ON2_AVC_FLASH_DEC
		mWidth = width;
		mHeight = height;
		ALOGV("--->flash request w %d h%d",mWidth,mHeight);
		mYuvLen = ((mWidth + 15)&(~15)) * ((mHeight + 15)&(~15)) *3/2;
		//mYuvLen = mWidth  *  mHeight *3 / 2;
		if(1)
		{
			_success = true;
			if(pOn2Dec)
			{
				if(sDecApi.init_class_On2Decoder_AVC(pOn2Dec,0))
				{
					ALOGE("OMX_ErrorUnsupportedSetting \n ");
					_success = false;
				}
			}
			else
			{
				_success = false;
			}
			mFormat->setCString(kKeyDecoderComponent,"OMX.Nvidia.h264.decode");
            //"OMX.rockchip.video.decoder.avc"
            //"OMX.Nvidia.h264.decode");
            //"OMX.qcom.video.decoder.avc");
		}
		else
		{
		 	mFormat->setCString(kKeyDecoderComponent,"AVCDecoder_FLASH");
		}
		ALOGV("AvcDecInit_OMX ok\n");
#endif
    int64_t durationUs;
    if (mSource->getFormat()->findInt64(kKeyDuration, &durationUs)) {
        mFormat->setInt64(kKeyDuration, durationUs);
    }
}

AVCDecoder_FLASH::~AVCDecoder_FLASH() {
	FUN()
    if (mStarted) {
        stop();
    }
#if ON2_AVC_FLASH_DEC
    if (pOn2Dec) {
        sDecApi.deinit_class_On2Decoder(pOn2Dec);
        sDecApi.destroy_class_On2Decoder(pOn2Dec);
        pOn2Dec = NULL;
	}
#endif
}

status_t AVCDecoder_FLASH::start(MetaData *) {
    CHECK(!mStarted);
	FUN()
    uint32_t type;
    const void *data;
    size_t size;
    sp<MetaData> meta = mSource->getFormat();
    if (meta->findData(kKeyAVCC, &type, &data, &size)) {
        // Parse the AVCDecoder_FLASHConfigurationRecord

        const uint8_t *ptr = (const uint8_t *)data;

        CHECK(size >= 7);
        CHECK_EQ(ptr[0], 1);  // configurationVersion == 1
        uint8_t profile = ptr[1];
        uint8_t level = ptr[3];

        // There is decodable content out there that fails the following
        // assertion, let's be lenient for now...
        // CHECK((ptr[4] >> 2) == 0x3f);  // reserved

        size_t lengthSize = 1 + (ptr[4] & 3);

        // commented out check below as H264_QVGA_500_NO_AUDIO.3gp
        // violates it...
        // CHECK((ptr[5] >> 5) == 7);  // reserved

        size_t numSeqParameterSets = ptr[5] & 31;

        ptr += 6;
        size -= 6;

        for (size_t i = 0; i < numSeqParameterSets; ++i) {
            CHECK(size >= 2);
            size_t length = U16_AT(ptr);

            ptr += 2;
            size -= 2;

            CHECK(size >= length);

            addCodecSpecificData(ptr, length);

            ptr += length;
            size -= length;
        }

        CHECK(size >= 1);
        size_t numPictureParameterSets = *ptr;
        ++ptr;
        --size;

        for (size_t i = 0; i < numPictureParameterSets; ++i) {
            CHECK(size >= 2);
            size_t length = U16_AT(ptr);

            ptr += 2;
            size -= 2;

            CHECK(size >= length);

            addCodecSpecificData(ptr, length);

            ptr += length;
            size -= length;
        }
    }

    mSource->start();
#if ON2_AVC_FLASH_DEC
	if(_success == false)
	{
		return !OK;
	}
#endif
    mPendingSeekTimeUs = -1;
    mPendingSeekMode = ReadOptions::SEEK_CLOSEST_SYNC;
    mTargetTimeUs = -1;
    mSPSSeen = false;
    mPPSSeen = false;
    mStarted = true;
	mInputTime.push(0LL);
    return OK;
}

void AVCDecoder_FLASH::addCodecSpecificData(const uint8_t *data, size_t size) {
	FUN()
    MediaBuffer *buffer = new MediaBuffer(size + 4);
    memcpy(buffer->data(), kStartCode, 4);
    memcpy((uint8_t *)buffer->data() + 4, data, size);
    buffer->set_range(0, size + 4);

    mCodecSpecificData.push(buffer);
}

status_t AVCDecoder_FLASH::stop() {
    CHECK(mStarted);
	FUN()

    for (size_t i = 0; i < mCodecSpecificData.size(); ++i) {
        (*mCodecSpecificData.editItemAt(i)).release();
    }
    mCodecSpecificData.clear();

    if (mInputBuffer) {
        mInputBuffer->release();
        mInputBuffer = NULL;
    }

    mSource->stop();

    mStarted = false;

    return OK;
}

sp<MetaData> AVCDecoder_FLASH::getFormat() {
	FUN()
    return mFormat;
}

static void findNALFragment(
        const MediaBuffer *buffer, const uint8_t **fragPtr, size_t *fragSize) {
    const uint8_t *data =
        (const uint8_t *)buffer->data() + buffer->range_offset();

    size_t size = buffer->range_length();
    CHECK(size >= 4);
    size_t offset = 4;
    while (offset + 3 < size && memcmp(kStartCode, &data[offset], 4)) {
        ++offset;
    }

    *fragPtr = &data[4];
    if (offset + 3 >= size) {
        *fragSize = size - 4;
    } else {
        *fragSize = offset - 4;
    }
}


status_t AVCDecoder_FLASH::read(
        MediaBuffer **out, const ReadOptions *options) {
     // FUN
     //ALOGI("AVCDecoder_FLASH::read in");
    *out = NULL;
    int64_t seekTimeUs;
    ReadOptions::SeekMode mode;
    if (options && options->getSeekTo(&seekTimeUs, &mode)) {
        ALOGD("seek requested to %lld us (%.2f secs)", seekTimeUs, seekTimeUs / 1E6);

        CHECK(seekTimeUs >= 0);
        mPendingSeekTimeUs = seekTimeUs;
        mPendingSeekMode = mode;

        if (mInputBuffer) {
            mInputBuffer->release();
            mInputBuffer = NULL;
        }

     }

    if (mInputBuffer == NULL) {
       // ALOGE("fetching new input buffer.");

        bool seeking = false;

        if (!mCodecSpecificData.isEmpty()) {
            mInputBuffer = mCodecSpecificData.editItemAt(0);
            mCodecSpecificData.removeAt(0);
        } else {
        	int errCount = 0;
            for (;;) {
                if (mPendingSeekTimeUs >= 0) {
                    ALOGD("reading data from timestamp %lld (%.2f secs)",
                         mPendingSeekTimeUs, mPendingSeekTimeUs / 1E6);
                }

                ReadOptions seekOptions;
                if (mPendingSeekTimeUs >= 0) {
                    seeking = true;
					ALOGI("--->seeek");
                    seekOptions.setSeekTo(mPendingSeekTimeUs, mPendingSeekMode);
                    mPendingSeekTimeUs = -1;
                }
				//ALOGI("----read in");
                status_t err = mSource->read(&mInputBuffer, &seekOptions);
				//ALOGI("---read out");
                seekOptions.clearSeekTo();

                if (err != OK) {

					errCount++;
					ALOGE("--------->errCount %d read data err %d",errCount,err);
					if(errCount <= 2)
						continue;
					ALOGE("----I want quit the decoder report the err %d",err);
                    *out = NULL;
                    return (*out == NULL)  ? err : (status_t)OK;
                }
			//	ALOGI("mInputBuffer len %d",mInputBuffer->range_length());
                if (mInputBuffer->range_length() > 0) {
                    break;
                }

                mInputBuffer->release();
                mInputBuffer = NULL;
            }
        }

        if (seeking) {
            int64_t targetTimeUs;
            if (mInputBuffer->meta_data()->findInt64(kKeyTargetTime, &targetTimeUs)
                    && targetTimeUs >= 0) {
                mTargetTimeUs = targetTimeUs;
            } else {
                mTargetTimeUs = -1;
            }
        }
    }

    const uint8_t *fragPtr;
    size_t fragSize;
    findNALFragment(mInputBuffer, &fragPtr, &fragSize);
	//ALOGE("---->minput len %d mRefCount %d",mInputBuffer->range_length(),IRefCount);
	//bool releaseFragment = true;
    status_t err = UNKNOWN_ERROR;

    int nalType;
    int nalRefIdc;
    AVCDec_Status res =
        PVAVCDecGetNALType(
                const_cast<uint8_t *>(fragPtr), fragSize,
                &nalType, &nalRefIdc);

    if (res != AVCDEC_SUCCESS) {
        ALOGV("cannot determine nal type");
    } else if (nalType == AVC_NALTYPE_SPS || nalType == AVC_NALTYPE_PPS
                || (mSPSSeen && mPPSSeen)) {
	//ALOGE("--->nalType %d fragSize %d fragPtr %p",nalType,fragSize,fragPtr);
#if ON2_AVC_FLASH_DEC
	size_t inSize = 0;
	uint8_t * inPtr = NULL;
	if(nalType == AVC_NALTYPE_SEI)
	{
		inSize = mInputBuffer->range_length()-fragSize -4;
		inPtr = (uint8_t *)fragPtr +fragSize;//othe type data,not pass the first nal type
	}
	else
	{
		inSize = mInputBuffer->range_length();
		inPtr = (uint8_t *)fragPtr -4;//othe type data,not pass the first nal type
	}
	MediaBuffer * aOutBuffer;
	uint32_t aOutputLength = 10;//tell the decoder this is the flash player version
#if DEBUG
	if(inPtr && fp)
		fwrite(inPtr,1,inSize,fp);
#endif

	int64_t inputTime =0LL;
	mInputBuffer->meta_data()->findInt64(kKeyTime, &inputTime);
	if(mUseInputTime && mInputTime.end()!= inputTime)
	{
			//when inputTime happen out of order at not seek time,then set use the decoder time

			if(mInputTime.end() > inputTime && inputTime != mTargetTimeUs)
			{
				ALOGI("---->the inputtime %lld the end value %lld targetTimeUs %lld the sizequeue %d type %d",inputTime,mInputTime.end(),mTargetTimeUs,mInputTime.size(),nalType);
				mUseInputTime = false;
			}

			mInputTime.push(inputTime);

	}

	//ALOGE("inputTime %lld",inputTime);
	if(pOn2Dec)
	{
		aOutBuffer = new MediaBuffer(mYuvLen);
		uint8_t* pOutBuf = (uint8_t*)aOutBuffer->data();

        int ret = sDecApi.dec_oneframe_class_On2Decoder_WithTimeStamp(
                pOn2Dec,
                pOutBuf,
                (uint32_t *)&aOutputLength,
                inPtr,
                (uint32_t *)&inSize,
                (int64_t*)(&inputTime));
        if(ret == -2)
        {
            if(mUseInputTime)
				mInputTime.pop(inputTime);
        }
		if(aOutputLength)
		{
			//ALOGI("-->outputTime %lld",inputTime);
			if(mUseInputTime)
				mInputTime.pop(inputTime);

		  	//ALOGI("-->outputTime %lld the sizequeue %d",inputTime,mInputTime.size());

			aOutBuffer->meta_data()->setInt64(kKeyTime,inputTime);
			//aOutBuffer->set_range(0,mYuvLen);
			*out = aOutBuffer;
		}
		else
		{
			 aOutBuffer->release();
		}
	}
#endif
        switch (nalType) {
            case AVC_NALTYPE_SPS:
            {
                mSPSSeen = true;
       			if(*out == NULL)
               		 *out = new MediaBuffer(0);
                err = OK;
                break;
            }

		case AVC_NALTYPE_PPS:
			  mPPSSeen = true;
		case AVC_NALTYPE_SLICE:
        case AVC_NALTYPE_IDR:

		case AVC_NALTYPE_SEI:
		case AVC_NALTYPE_AUD:


		if(*out == NULL)
			*out = new MediaBuffer(0);
		err =  OK;
		break;
        default:
        {
            ALOGE("Should not be here, unknown nalType %d", nalType);
            break;
        }
        }
    } else {


        *out = new MediaBuffer(0);
        err = OK;
    }


	mInputBuffer->release();
	mInputBuffer = NULL;

    return err;
}


void AVCDecoder_FLASH::signalBufferReturned(MediaBuffer *buffer) {
}

}  // namespace android
