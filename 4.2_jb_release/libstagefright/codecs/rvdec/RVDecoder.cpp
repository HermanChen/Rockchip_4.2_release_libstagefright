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
#define LOG_TAG "RVDecoder"
#include <utils/Log.h>

#undef ALOGD
#define ALOGD
#undef ALOGI
#define ALOGI
#include "RVDecoder.h"

#include <OMX_Component.h>

#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/Utils.h>
#include <media/stagefright/foundation/hexdump.h>

#include "vpu_global.h"
#define MAX_STREAM_LENGHT 1024*500


namespace android {



RVDecoder::RVDecoder(const sp<MediaSource> &source)
     :mSource(source),
      mStarted(false),
      mInputBuffer(NULL),
      mAnchorTimeUs(0),
      mNumFramesOutput(0),
      mPendingSeekTimeUs(-1),
      mPendingSeekMode(MediaSource::ReadOptions::SEEK_CLOSEST_SYNC),
      mTargetTimeUs(-1),
      mWidth(0),
      mHeight(0),
      mYuvLen(0),
      pOn2Dec(NULL),
      _success(true){
   ALOGI("RVDecoder::RVDecoder in");
    uint32_t deInt = 0;
    mFormat = new MetaData;
    mFormat->setInt32(kKeyDeIntrelace, deInt);
    mFormat->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_RAW);

    CHECK(mSource->getFormat()->findInt32(kKeyWidth, &mWidth));
    CHECK(mSource->getFormat()->findInt32(kKeyHeight, &mHeight));

    mFormat->setInt32(kKeyWidth, mWidth);
    mFormat->setInt32(kKeyHeight, mHeight);

	mYuvLen = ((mWidth+15)&(~15))* ((mHeight+15)&(~15))*3/2;

	mFormat->setInt32(kKeyColorFormat, OMX_COLOR_FormatYUV420SemiPlanar);

    mFormat->setCString(kKeyDecoderComponent, "RVDecoder");


    int64_t durationUs;
    if (mSource->getFormat()->findInt64(kKeyDuration, &durationUs)) {
        mFormat->setInt64(kKeyDuration, durationUs);
    }

    vpu_api_init(&sDecApi, OMX_ON2_VIDEO_CodingRV);
    pOn2Dec = sDecApi.get_class_On2Decoder();

    if(pOn2Dec){
        if(sDecApi.init_class_On2Decoder(pOn2Dec)) {
            ALOGE("init rv decoder fail in RVDecoder construct");
		_success = false;
	}

    } else {
		_success = false;
	}
	 ALOGI("RVDecoder::RVDecoder out");
}

RVDecoder::~RVDecoder() {
    if (mStarted) {
        stop();
    }

    if( pOn2Dec){
        sDecApi.deinit_class_On2Decoder(pOn2Dec);
        sDecApi.destroy_class_On2Decoder(pOn2Dec);
        pOn2Dec = NULL;
	}
}

status_t RVDecoder::start(MetaData *) {
    CHECK(!mStarted);
	ALOGI("RVDecoder::start in");

    uint32_t type;
    const void *data;
    size_t size;
    sp<MetaData> meta = mSource->getFormat();
    if(!_success)
		return UNKNOWN_ERROR;

    mSource->start();
    mAnchorTimeUs = 0;
    mNumFramesOutput = 0;
    mPendingSeekTimeUs = -1;
    mPendingSeekMode = ReadOptions::SEEK_CLOSEST_SYNC;
    mTargetTimeUs = -1;
    mStarted = true;
	ALOGI("RVDecoder::start out ");

    return OK;
}


status_t RVDecoder::stop() {
    CHECK(mStarted);

    if (mInputBuffer) {
        mInputBuffer->release();
        mInputBuffer = NULL;
    }

    mSource->stop();

    mStarted = false;

    return OK;
}

sp<MetaData> RVDecoder::getFormat() {
    return mFormat;
}

status_t RVDecoder::read(
        MediaBuffer **out, const ReadOptions *options) {
    *out = NULL;
	//LOGI("RVDecoder::read in");

    int64_t seekTimeUs;
    ReadOptions::SeekMode mode;
    if (options && options->getSeekTo(&seekTimeUs, &mode)) {
        ALOGV("seek requested to %lld us (%.2f secs)", seekTimeUs, seekTimeUs / 1E6);

        CHECK(seekTimeUs >= 0);
        mPendingSeekTimeUs = seekTimeUs;
        mPendingSeekMode = mode;

        if (mInputBuffer) {
            mInputBuffer->release();
            mInputBuffer = NULL;
        }
		sDecApi.reset_class_On2Decoder(pOn2Dec);
    }

    if (mInputBuffer == NULL) {
        ALOGV("fetching new input buffer.");

        bool seeking = false;

            for (;;) {
                if (mPendingSeekTimeUs >= 0) {
                    ALOGV("reading data from timestamp %lld (%.2f secs)",
                         mPendingSeekTimeUs, mPendingSeekTimeUs / 1E6);
                }

                ReadOptions seekOptions;
                if (mPendingSeekTimeUs >= 0) {
                    seeking = true;

                    seekOptions.setSeekTo(mPendingSeekTimeUs, mPendingSeekMode);
                    mPendingSeekTimeUs = -1;
                }
                status_t err = mSource->read(&mInputBuffer, &seekOptions);
                seekOptions.clearSeekTo();

                if (err != OK) {

                    *out = NULL;
                    return (*out == NULL)  ? err : (status_t)OK;
                }

                if (mInputBuffer->range_length() > 0) {
                    break;
                }

                mInputBuffer->release();
                mInputBuffer = NULL;
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

	MediaBuffer *aOutBuf = new MediaBuffer(sizeof(VPU_FRAME));
	uint8_t * aOutBuffer = (uint8_t *)aOutBuf->data();
	uint32_t aOutputLength = 0;
	uint8_t * pInput = (uint8_t *)mInputBuffer->data();
	uint32_t aInBufSize = mInputBuffer->range_length();
	//int64_t inputTime = 0LL;
	//mInputBuffer->meta_data()->findInt64(kKeyTime, &inputTime);
	int64_t outputTime = 0LL;
	/*if(mNumFramesOutput == 0)
	{
		 pInput += 16;
        aInBufSize -= 16;
	}*/
	//LOGI("before decoder RV inputlen %d",aInBufSize);
	//LOGI("inpout data %c  %c %c %c",pInput[0],pInput[1],pInput[2],pInput[3]);
    memset(aOutBuffer,0,sizeof(VPU_FRAME));
	if(sDecApi.dec_oneframe_class_On2Decoder(
            pOn2Dec,
            aOutBuffer,
            (uint32_t*)&aOutputLength,
            pInput,
            &aInBufSize)) {
        aOutBuf->releaseframe();
		mInputBuffer->release();
		mInputBuffer = NULL;
		ALOGE("rvdec failed");
        return UNKNOWN_ERROR;
    }
	//LOGI("after decoder RV aInBufSize %d aOutputLength %d inputtime %lld",aInBufSize,aOutputLength,inputTime);
	if(mInputBuffer)
	{
		mInputBuffer->release();
		mInputBuffer = NULL;
	}

	if(aOutputLength)
		mNumFramesOutput++;

	VPU_FRAME *frame = (VPU_FRAME *)aOutBuffer;
	outputTime = ((int64_t)(frame->ShowTime.TimeHigh) <<32) | ((int64_t)(frame->ShowTime.TimeLow));
	outputTime *=1000;
	aOutBuf->meta_data()->setInt64(kKeyTime,outputTime);
	//LOGE("out one frame, ts = %lld", outputTime);
	//if((mNumFramesOutput == 1))
	{
		uint32_t iDisplayWidth = 0;
		uint32_t iDisplayHeight = 0;
		//LOGE("get rv w h");
		sDecApi.get_width_Height_class_On2Decoder_RV(
		        pOn2Dec, (uint32_t *)&iDisplayWidth,(uint32_t *)&iDisplayHeight);
		//LOGE("get old w =%d h = %d new w= %d h = %d" ,mWidth,mHeight,iDisplayWidth,iDisplayHeight);
		if((mWidth != iDisplayWidth) || (mHeight != iDisplayHeight))
		{
			mWidth = iDisplayWidth;
			mHeight = iDisplayHeight;
			mFormat->setInt32(kKeyWidth, mWidth);
			mFormat->setInt32(kKeyHeight, mHeight);
			aOutBuf->releaseframe();//INFO Changed ,the output must be NULL;
			return INFO_FORMAT_CHANGED;
		}
		//LOGE("after get rv w h");
	}

	if(aOutputLength <= 0)
		aOutBuf->set_range(0, 0);
	*out = aOutBuf;
	//LOGI("RVDecoder::read out");
	return OK;
}

void RVDecoder::signalBufferReturned(MediaBuffer *buffer) {
}

}  // namespace android
