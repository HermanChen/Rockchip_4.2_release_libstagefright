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
#define LOG_TAG "FLVDecoder"
#include <utils/Log.h>

#include "FLVDecoder.h"

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


static __inline
int32_t video_fill_hdr(uint8_t *dst,uint8_t *src, uint32_t size, int64_t time, uint32_t type, uint32_t num)
{
    VPU_BITSTREAM h;
    uint32_t TimeLow = (uint32_t)(time);
    h.StartCode = BSWAP(VPU_BITSTREAM_START_CODE);
    h.SliceLength= BSWAP(size);
    h.SliceTime.TimeLow = BSWAP(TimeLow);
    h.SliceTime.TimeHigh= 0;
    h.SliceType= BSWAP(type);
    h.SliceNum= BSWAP(num);
    h.Res[0] = 0;
    h.Res[1] = 0;
    memcpy(dst, &h, sizeof(VPU_BITSTREAM));
    memcpy((uint8_t*)(dst + sizeof(VPU_BITSTREAM)),src, size);
    return 0;
}

FLVDecoder::FLVDecoder(const sp<MediaSource> &source)
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
    ALOGV("new FLVDecoder in");
    VPU_GENERIC vpug;

    vpug.CodecType = VPU_CODEC_DEC_SORESONSPARKLE;
    vpug.ImgHeight = 0;
    vpug.ImgWidth = 0;
    getwhFlg = 0;
    mFormat = new MetaData;
    uint32_t deInt = 0;
    mFormat->setInt32(kKeyDeIntrelace, deInt);
    mFormat->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_RAW);

    CHECK(mSource->getFormat()->findInt32(kKeyWidth, &mWidth));
    CHECK(mSource->getFormat()->findInt32(kKeyHeight, &mHeight));

    mFormat->setInt32(kKeyWidth, mWidth);
    mFormat->setInt32(kKeyHeight, mHeight);

	mYuvLen = ((mWidth+15)&~15)*((mHeight+15)&~15)*3/2;

	mFormat->setInt32(kKeyColorFormat, OMX_COLOR_FormatYUV420SemiPlanar);

    mFormat->setCString(kKeyDecoderComponent, "FLVDecoder");


    int64_t durationUs;
    if (mSource->getFormat()->findInt64(kKeyDuration, &durationUs)) {
        mFormat->setInt64(kKeyDuration, durationUs);
    }
    streamInbuf = NULL;
    streamInbuf = (uint8_t*)malloc(MAX_STREAM_LENGHT);
    vpu_api_init(&sDecApi, OMX_ON2_VIDEO_CodingH263);
    pOn2Dec = sDecApi.get_class_On2Decoder();

	if(pOn2Dec){
        if(sDecApi.init_class_On2Decoder_M4VH263(pOn2Dec,&vpug)) {
            ALOGE("init flv decoder fail in FLVDecoder construct");
            _success = false;
        }
	} else {
	    _success = false;
	}
	 ALOGV("new FLVDecoder out");
}

FLVDecoder::~FLVDecoder() {
    if (mStarted) {
        stop();
    }

	if( pOn2Dec){
		sDecApi.deinit_class_On2Decoder(pOn2Dec);
        sDecApi.destroy_class_On2Decoder(pOn2Dec);
		pOn2Dec = NULL;
	}
    if(streamInbuf)
	{
		free(streamInbuf);
		streamInbuf = NULL;
	}
}

status_t FLVDecoder::start(MetaData *) {
    CHECK(!mStarted);
	ALOGV("FLVDecoder::start in");

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
	ALOGV("FLVDecoder::start out ");

    return OK;
}


status_t FLVDecoder::stop() {
    CHECK(mStarted);

    if (mInputBuffer) {
        mInputBuffer->release();
        mInputBuffer = NULL;
    }

    mSource->stop();

    mStarted = false;

    return OK;
}

sp<MetaData> FLVDecoder::getFormat() {
    return mFormat;
}

status_t FLVDecoder::read(
        MediaBuffer **out, const ReadOptions *options) {
    *out = NULL;

    int64_t seekTimeUs;
    ReadOptions::SeekMode mode;
    if (options && options->getSeekTo(&seekTimeUs, &mode)) {
        ALOGV("seek requested to %lld us (%.2f secs)", seekTimeUs, seekTimeUs / 1E6);

        CHECK(seekTimeUs >= 0);
        mPendingSeekTimeUs = seekTimeUs;
        mPendingSeekMode = mode;

        sDecApi.reset_class_On2Decoder(pOn2Dec);
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


    uint8_t* pInput = (uint8_t*)mInputBuffer->data() + mInputBuffer->range_offset();
    uint32_t aInBufSize = mInputBuffer->range_length();
    int64_t inputTime = 0LL;
    if(!getwhFlg)
    {
        int32_t aHeight,aWidth;
        FlvGetResolution(&aWidth,&aHeight,pInput);
        mYuvLen = ((aWidth+15)&~15)*((aHeight+15)&~15)*3/2;
    }
    MediaBuffer *aOutBuf = new MediaBuffer(sizeof(VPU_FRAME));
    uint8_t * aOutBuffer = (uint8_t *)aOutBuf->data();
    uint32_t aOutputLength = 0;
    mInputBuffer->meta_data()->findInt64(kKeyTime, &inputTime);
    if((aInBufSize + sizeof(VPU_BITSTREAM)) > MAX_STREAM_LENGHT)
    {
       streamInbuf = (uint8_t *)realloc((void*)streamInbuf, (aInBufSize+sizeof(VPU_BITSTREAM)));
    }
    video_fill_hdr(streamInbuf, pInput, aInBufSize,inputTime, 0, 0);

    aInBufSize += sizeof(VPU_BITSTREAM);
    memset(aOutBuffer,0,sizeof(VPU_FRAME));

	if(sDecApi.dec_oneframe_class_On2Decoder(
            pOn2Dec,
            aOutBuffer,
            &aOutputLength,
            streamInbuf,
            &aInBufSize)) {
        aOutBuf->releaseframe();
		mInputBuffer->release();
		mInputBuffer = NULL;
		ALOGE("flvdec failed");
        return UNKNOWN_ERROR;
    }
    if(!getwhFlg)
    {
        int32_t aHeight,aWidth;
        getwhFlg = 1;
        FlvGetResolution(&aWidth,&aHeight,pInput);
        getwhFlg = 1;
        if((mHeight != aHeight)|| (mWidth != aWidth))
        {
            mHeight = aHeight;
            mWidth = aWidth;
            mFormat->setInt32(kKeyWidth, mWidth);
            mFormat->setInt32(kKeyHeight, mHeight);
            if(mInputBuffer)
        	{
        		mInputBuffer->release();
        		mInputBuffer = NULL;
        	}
            if(aOutBuf)
            {
                aOutBuf->releaseframe();
                aOutBuf = NULL;
            }
            return INFO_FORMAT_CHANGED;
        }
    }

    if(mInputBuffer)
    {
        mInputBuffer->release();
        mInputBuffer = NULL;
    }

    if(aOutputLength)
        mNumFramesOutput++;

    aOutBuf->meta_data()->setInt64(kKeyTime,inputTime);

    if(aOutputLength <= 0)
        aOutBuf->set_range(0, 0);

    *out = aOutBuf;

    return OK;
}

void FLVDecoder::signalBufferReturned(MediaBuffer *buffer) {
}

int32_t FLVDecoder::FlvGetResolution(int32_t *aWidth, int32_t *aHeight, uint8_t *aBuffer)
{
	uint8_t PicSize;
	if ((aBuffer[0]!=0)||(aBuffer[1]!=0)||((aBuffer[2]&0x80)!=0x80))
	{
		return false;
	}
	else
	{
       PicSize = (((aBuffer[3] & 0x03)<<1) | (aBuffer[4]>>7));
        switch (PicSize)
		{
			case	2	:
				*aWidth = 352;
				*aHeight = 288;
				break;
			case	3	:
				*aWidth = 176;
				*aHeight = 144;
				break;
			case	4	:
				*aWidth = 128;
				*aHeight = 96;
				break;
			case	5	:
				*aWidth = 320;
				*aHeight = 240;
				break;
			case	6	:
				*aWidth = 160;
				*aHeight = 120;
				break;
			case	0	:
				*aWidth = (((aBuffer[4]&0x7f)<<1) | (aBuffer[5]>>7));
				*aHeight = (((aBuffer[5]&0x7f)<<1) | (aBuffer[6]>>7));
				break;
			case	1	:
				*aWidth = ((((uint16_t)(aBuffer[4]&0x7f))<<9)|(((uint16_t)(aBuffer[5]))<<1)|(((uint16_t)(aBuffer[6]))>>7));
				*aHeight = ((((uint16_t)(aBuffer[6]&0x7f))<<9)|(((uint16_t)(aBuffer[7]))<<1)|(((uint16_t)(aBuffer[8]))>>7));
				break;
			case	7	:
			default		:
				return false;
		}
		return true;
	}
}
}  // namespace android
