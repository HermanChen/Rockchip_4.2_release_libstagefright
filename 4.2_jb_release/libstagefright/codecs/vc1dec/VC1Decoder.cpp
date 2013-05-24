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

#include "VC1Decoder.h"

#include <OMX_Component.h>

#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/Utils.h>
#include <media/stagefright/foundation/hexdump.h>

#include "vpu_global.h"


#define LOG_TAG "VC1Decoder"
#include <utils/Log.h>

#define VC1DECODER_DEBUG 0
#if VC1DECODER_DEBUG
#define VC1DEC_LOG ALOGD
#else
#undef VC1DEC_LOG
#define VC1DEC_LOG
#endif

#define MAX_STREAM_LENGHT 1024*500


namespace android {



VC1Decoder::VC1Decoder(const sp<MediaSource> &source)
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
      lastTimestamps(0),
      SameTimeCount(0),
      pOn2Dec(NULL),
      mSeeking(false),
      _success(true){
    VC1DEC_LOG("construction in");
    mFormat = new MetaData;
    uint32_t deInt = 0;
    mFormat->setInt32(kKeyDeIntrelace, deInt);
    mFormat->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_RAW);

    CHECK(mSource->getFormat()->findInt32(kKeyWidth, &mWidth));
    CHECK(mSource->getFormat()->findInt32(kKeyHeight, &mHeight));

    mFormat->setInt32(kKeyWidth, mWidth);
    mFormat->setInt32(kKeyHeight, mHeight);
    VC1DEC_LOG("video width = %d, height = %d", mWidth, mHeight);

	mYuvLen = ((mWidth+15)&(~15))* ((mHeight+15)&(~15))*3/2;

	mFormat->setInt32(kKeyColorFormat, OMX_COLOR_FormatYUV420SemiPlanar);

    mFormat->setCString(kKeyDecoderComponent, "VC1Decoder");


    int64_t durationUs;
    if (mSource->getFormat()->findInt64(kKeyDuration, &durationUs)) {
        mFormat->setInt64(kKeyDuration, durationUs);
    }

    vpu_api_init(&sDecApi, OMX_ON2_VIDEO_CodingWMV);
    pOn2Dec = sDecApi.get_class_On2Decoder();
    if(pOn2Dec == NULL){
        ALOGE("init vc1 decoder fail in VC1Decoder construct");
		_success = false;
	}

    VC1DEC_LOG("construction out, _success = %d",_success);
}

VC1Decoder::~VC1Decoder() {
    if (mStarted) {
        stop();
    }

    if( pOn2Dec){
        sDecApi.deinit_class_On2Decoder(pOn2Dec);
        sDecApi.destroy_class_On2Decoder(pOn2Dec);
        pOn2Dec = NULL;
	}
}

status_t VC1Decoder::start(MetaData *) {
    CHECK(!mStarted);
	VC1DEC_LOG("start in");

    uint32_t type;
    const void *data;
    size_t size;
    int32_t extraDataSize = 0;
    sp<MetaData> meta = mSource->getFormat();
	meta->findInt32(kKeyVC1ExtraSize, &extraDataSize);

    if (meta->findData(kKeyVC1, &type, &data, &size))
    {
        VC1DEC_LOG("kKeyVC1 data size = %d", size);
        uint8_t* ptr = (uint8_t*)data;
        int tmpSize = 0;
        int i = 0;
        tmpSize = size >5?5:size;
        for(; i <tmpSize; i++)
        {
           VC1DEC_LOG("keyData[%d]---->0x%X", i, ptr[i]);
        }
    	if(pOn2Dec){
            if(sDecApi.init_class_On2Decoder_VC1(pOn2Dec, ptr, size, extraDataSize)) {
                _success = false;
            }
    	} else {
            _success = false;
        }
    }
    else
    {
    	if(pOn2Dec){
            if(sDecApi.init_class_On2Decoder_VC1(pOn2Dec, NULL, size, extraDataSize)) {
                _success = false;
            }
    	} else {
            _success = false;
        }
    }

    if(!_success)
    {
        ALOGE("start----> return error for _success is false");
		return UNKNOWN_ERROR;
    }

    mSource->start();
    mAnchorTimeUs = 0;
    mNumFramesOutput = 0;
    mPendingSeekTimeUs = -1;
    mPendingSeekMode = ReadOptions::SEEK_CLOSEST_SYNC;
    mTargetTimeUs = -1;
    mStarted = true;

    return OK;
}


status_t VC1Decoder::stop() {
    CHECK(mStarted);

    if (mInputBuffer) {
        mInputBuffer->release();
        mInputBuffer = NULL;
    }

    mSource->stop();

    mStarted = false;

    return OK;
}

sp<MetaData> VC1Decoder::getFormat() {
    return mFormat;
}

status_t VC1Decoder::read(
        MediaBuffer **out, const ReadOptions *options) {
    *out = NULL;

    int64_t seekTimeUs;
    ReadOptions::SeekMode mode;
    if (options && options->getSeekTo(&seekTimeUs, &mode)) {
        VC1DEC_LOG("seek requested to %lld us (%.2f secs)", seekTimeUs, seekTimeUs / 1E6);

        CHECK(seekTimeUs >= 0);
        mPendingSeekTimeUs = seekTimeUs;
        mPendingSeekMode = mode;

        if (mInputBuffer) {
            mInputBuffer->release();
            mInputBuffer = NULL;
        }
    }

    if (mInputBuffer == NULL) {
        ALOGV("fetching new input buffer.");

        bool seeking = false;

        for (;;) {
            if (mPendingSeekTimeUs >= 0) {
                VC1DEC_LOG("reading data from timestamp %lld (%.2f secs)",
                mPendingSeekTimeUs, mPendingSeekTimeUs / 1E6);
            }

            ReadOptions seekOptions;
            if (mPendingSeekTimeUs >= 0) {
                seeking = true;
                mSeeking = true;

                seekOptions.setSeekTo(mPendingSeekTimeUs, mPendingSeekMode);
                mPendingSeekTimeUs = -1;
                lastTimestamps = 0;
                SameTimeCount = 0;
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
	int64_t inputTime = 0LL;
	mInputBuffer->meta_data()->findInt64(kKeyTime, &inputTime);
	int64_t outputTime = 0LL;
	/*if(mNumFramesOutput == 0)
	{
		 pInput += 16;
        aInBufSize -= 16;
	}*/
	//VC1DEC_LOG("before decoder vc1 inputlen %d",aInBufSize);
	//VC1DEC_LOG("inpout data %c  %c %c %c",pInput[0],pInput[1],pInput[2],pInput[3]);
    memset(aOutBuffer,0,sizeof(VPU_FRAME));
	if(sDecApi.dec_oneframe_class_On2Decoder_WithTimeStamp(
            pOn2Dec,
            aOutBuffer,
            (uint32_t*)&aOutputLength,
            pInput,
            &aInBufSize,
            (int64_t*)&inputTime)) {
        aOutBuf->releaseframe();
		mInputBuffer->release();
		mInputBuffer = NULL;
		ALOGE("vc1dec failed");
        return UNKNOWN_ERROR;
    }
	//VC1DEC_LOG("after decoder vc1 aInBufSize %d aOutputLength %d ",aInBufSize,aOutputLength);

    if (mSeeking)
    {
        mSeeking = false;
        aOutputLength = 0;
        aOutBuf->releaseframe();
        aOutBuf = NULL;
        sDecApi.reset_class_On2Decoder(pOn2Dec);
    }

	if(mInputBuffer)
	{
		mInputBuffer->release();
		mInputBuffer = NULL;
	}

	if(aOutputLength)
	{
    	mNumFramesOutput++;
    	VPU_FRAME *frame = (VPU_FRAME *)aOutBuffer;
    	outputTime = ((int64_t)(frame->ShowTime.TimeHigh) <<32) | ((int64_t)(frame->ShowTime.TimeLow));
        if(outputTime == lastTimestamps || outputTime < lastTimestamps)
            {
                SameTimeCount++;
            }
            else
            {
                SameTimeCount = 0;
            lastTimestamps = outputTime;
            }
        aOutBuf->meta_data()->setInt64(kKeyTime,(lastTimestamps+SameTimeCount*40*1000));
	}

	if(aOutputLength <= 0)
	{
        *out = new MediaBuffer(0);
        return OK;
	}

	*out = aOutBuf;
    //VC1DEC_LOG("out one frame, ts = %lld", outputTime);
    //LOGE("vc1 out one frame, ts = %lld", outputTime);
	return OK;
}

void VC1Decoder::signalBufferReturned(MediaBuffer *buffer) {
}

}  // namespace android
