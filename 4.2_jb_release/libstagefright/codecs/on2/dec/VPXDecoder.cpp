/*
 * Copyright (C) 2010 The Android Open Source Project
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
#define LOG_TAG "VPXDecoder"
#include <utils/Log.h>

#include "VPXDecoder.h"

#include <OMX_Component.h>

#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/Utils.h>

#include "vpx/vpx_decoder.h"
#include "vpx/vpx_codec.h"
#include "vpx/vp8dx.h"

#if ON2_VP8_DEC
#include "vpu_global.h"
#define MAX_STREAM_LENGHT 1024*500
#define VP6_CODEC       1
#define VP8_CODEC       2
#endif
namespace android {

VPXDecoder::VPXDecoder(const sp<MediaSource> &source)
    : mSource(source),
      mStarted(false),
      mBufferSize(0),
      mCtx(NULL),
      mBufferGroup(NULL),
      mTargetTimeUs(-1) {
    sp<MetaData> inputFormat = source->getFormat();
    const char *mime;
    CHECK(inputFormat->findCString(kKeyMIMEType, &mime));
//    CHECK(!strcasecmp(mime, MEDIA_MIMETYPE_VIDEO_VPX));

    CHECK(inputFormat->findInt32(kKeyWidth, &mWidth));
    CHECK(inputFormat->findInt32(kKeyHeight, &mHeight));

    if (!strcasecmp(MEDIA_MIMETYPE_VIDEO_VP6, mime)) {
        codecid = VP6_CODEC;
        CHECK(inputFormat->findInt32(kKeyVp6CodecId, &codecinfo));
        //LOGE("codecinfo=0x%x", codecinfo);
    }else if (!strcasecmp(MEDIA_MIMETYPE_VIDEO_VPX, mime)){
        codecid = VP8_CODEC;
    }else{
        codecid = 0;
    }

    mBufferSize = (mWidth * mHeight * 3) / 2;

    mFormat = new MetaData;
    uint32_t deInt = 0;
    mFormat->setInt32(kKeyDeIntrelace, deInt);
    mFormat->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_RAW);
    mFormat->setInt32(kKeyWidth, mWidth);
    mFormat->setInt32(kKeyHeight, mHeight);
    mFormat->setInt32(kKeyColorFormat, OMX_COLOR_FormatYUV420SemiPlanar);
    mFormat->setCString(kKeyDecoderComponent, "VPXDecoder");

#if ON2_VP8_DEC
    mYuvLen = ((mWidth + 15)&(~15)) * ((mHeight + 15)&(~15)) *3/2;
    pOn2Dec = NULL;
    streamInbuf = NULL;
    streamInbuf = (uint8_t*)malloc(MAX_STREAM_LENGHT);
    if(codecid == VP6_CODEC) {
        _success = true;
        vpu_api_init(&sDecApi, OMX_ON2_VIDEO_CodingVP6);
        pOn2Dec = sDecApi.get_class_On2Decoder();

        if(pOn2Dec){
            if(sDecApi.init_class_On2Decoder_VP6(pOn2Dec, codecinfo)) {
                ALOGE("init vp6 decoder fail in VPXDecoder construct");
                _success = false;
            }
        } else {
            _success = false;
        }
    }else if(codecid == VP8_CODEC)
    {
        _success = true;
        vpu_api_init(&sDecApi, OMX_ON2_VIDEO_CodingVPX);
        pOn2Dec = sDecApi.get_class_On2Decoder();

        if(pOn2Dec){
            if(sDecApi.init_class_On2Decoder(pOn2Dec)) {
                ALOGE("init vp8 decoder fail in VPXDecoder construct");
                _success = false;
            }
        } else {
            _success = false;
        }
    }

    ALOGD("VpXDecInit_OMX ok\n");
#endif
    int64_t durationUs;
    if (inputFormat->findInt64(kKeyDuration, &durationUs)) {
        mFormat->setInt64(kKeyDuration, durationUs);
    }
}

VPXDecoder::~VPXDecoder() {
    if (mStarted) {
        stop();
    }
#if ON2_VP8_DEC
	if(pOn2Dec) {
        sDecApi.deinit_class_On2Decoder(pOn2Dec);
        sDecApi.destroy_class_On2Decoder(pOn2Dec);
        pOn2Dec = NULL;
	}


	if(streamInbuf)
	{
		free(streamInbuf);
		streamInbuf = NULL;
	}
#endif
}
#if ON2_VP8_DEC
static __inline
int32_t video_fill_hdr(uint8_t *dst,uint8_t *src, uint32_t size, int64_t time, uint32_t type, uint32_t num)
{
    VPU_BITSTREAM h;
    uint32_t TimeLow = (uint32_t)(time);
    h.StartCode = VPU_BITSTREAM_START_CODE;
    h.SliceLength= BSWAP(size);
    h.SliceTime.TimeLow = TimeLow;
    h.SliceTime.TimeHigh= 0;
    h.SliceType= BSWAP(type);
    h.SliceNum= BSWAP(num);
    h.Res[0] = 0;
    h.Res[1] = 0;
    memcpy(dst, &h, sizeof(VPU_BITSTREAM));
    memcpy((uint8_t*)(dst + sizeof(VPU_BITSTREAM)),src, size);
    return 0;
}
#endif

status_t VPXDecoder::start(MetaData *) {
    if (mStarted) {
        return UNKNOWN_ERROR;
    }
#if ON2_VP8_DEC
    if(_success = false)
    {
        return !OK;
    }
#endif

    status_t err = mSource->start();

    if (err != OK) {
        return err;
    }
#if ON2_VP8_DEC

#else
    mCtx = new vpx_codec_ctx_t;
    vpx_codec_err_t vpx_err;
    if ((vpx_err = vpx_codec_dec_init(
                (vpx_codec_ctx_t *)mCtx, &vpx_codec_vp8_dx_algo, NULL, 0))) {
        ALOGE("on2 decoder failed to initialize. (%d)", vpx_err);

        mSource->stop();

        return UNKNOWN_ERROR;
    }

    mBufferGroup = new MediaBufferGroup;
    mBufferGroup->add_buffer(new MediaBuffer(mBufferSize));
    mBufferGroup->add_buffer(new MediaBuffer(mBufferSize));
#endif

    mTargetTimeUs = -1;

    mStarted = true;

    return OK;
}

status_t VPXDecoder::stop() {
    if (!mStarted) {
        return UNKNOWN_ERROR;
    }

#if ON2_VP8_DEC
#else
    delete mBufferGroup;
    mBufferGroup = NULL;

    vpx_codec_destroy((vpx_codec_ctx_t *)mCtx);
    delete (vpx_codec_ctx_t *)mCtx;
    mCtx = NULL;
#endif
    mSource->stop();

    mStarted = false;

    return OK;
}

sp<MetaData> VPXDecoder::getFormat() {
    return mFormat;
}

status_t VPXDecoder::read(
        MediaBuffer **out, const ReadOptions *options) {
    *out = NULL;

    bool seeking = false;
    int64_t seekTimeUs;
    ReadOptions::SeekMode seekMode;
    if (options && options->getSeekTo(&seekTimeUs, &seekMode)) {
        seeking = true;
    }

RETRY_READ:
    MediaBuffer *input;
    status_t err = mSource->read(&input, options);

    if (err != OK) {
        if (err != ERROR_END_OF_STREAM) {
            ALOGE("mSource->read fail");
        }
        return err;
    }


    if (seeking) {
        int64_t targetTimeUs;
        if (input->meta_data()->findInt64(kKeyTargetTime, &targetTimeUs)
                && targetTimeUs >= 0) {
            mTargetTimeUs = targetTimeUs;
        } else {
            mTargetTimeUs = -1;
        }
        sDecApi.reset_class_On2Decoder(pOn2Dec);
    }
    if (seeking && (codecid == VP8_CODEC)) {
        uint8_t* pBuf = NULL;
        if (input) {
            pBuf = (uint8_t*)(input->data() + input->range_offset());
            if (pBuf && ((*pBuf) & 1)) {
                input->release();
                input = NULL;
                goto RETRY_READ;
            }
            seeking = false;
        }
    }
#if ON2_VP8_DEC
    int64_t timeUs;
    uint32_t buflen;
    MediaBuffer * aOutBuffer;
    uint32_t aOutputLength = 0;
    uint8_t *inputbuf;

    aOutBuffer = new MediaBuffer(sizeof(VPU_FRAME));
    uint8_t* pOutBuf = (uint8_t*)aOutBuffer->data();
    CHECK(input->meta_data()->findInt64(kKeyTime, &timeUs));
    buflen = input->range_length();

    if(codecid == VP8_CODEC)
    {
        if((buflen + sizeof(VPU_BITSTREAM)) > MAX_STREAM_LENGHT)
        {
           streamInbuf = (uint8_t *)realloc((void*)streamInbuf, (buflen+sizeof(VPU_BITSTREAM)));
        }
        video_fill_hdr(streamInbuf, (uint8_t *)input->data() + input->range_offset(), buflen,timeUs, 0, 0);
        buflen += sizeof(VPU_BITSTREAM);
        memset(pOutBuf,0,sizeof(VPU_FRAME));

        sDecApi.dec_oneframe_class_On2Decoder(
            pOn2Dec,
            pOutBuf,
            &aOutputLength,
            streamInbuf,
            &buflen);
    }else if(codecid == VP6_CODEC){
        inputbuf = (uint8_t *)input->data() + input->range_offset();
        memset(pOutBuf,0,sizeof(VPU_FRAME));

        sDecApi.dec_oneframe_class_On2Decoder(
            pOn2Dec,
            pOutBuf,
            &aOutputLength,
            inputbuf,
            &buflen);
    }


    bool skipFrame = false;
    if (mTargetTimeUs >= 0) {
        CHECK(timeUs <= mTargetTimeUs);
        if (timeUs < mTargetTimeUs) {
            skipFrame = true;
            ALOGV("skipping frame at %lld us", timeUs);
        } else {
            ALOGV("found target frame at %lld us", timeUs);
            mTargetTimeUs = -1;
        }
    }
    if (skipFrame) {
        *out = new MediaBuffer(0);
        aOutBuffer->releaseframe();
        return OK;
    }
    if(aOutputLength)
	{
		VPU_FRAME *frame;
    	frame = (VPU_FRAME *)aOutBuffer;
       int32_t width = frame->DisplayWidth;
       int32_t height = frame->DisplayHeight;
		int64_t outputTime = timeUs;
		aOutBuffer->meta_data()->setInt64(kKeyTime,outputTime);
		*out = aOutBuffer;
	}
	else
	{
         aOutBuffer->releaseframe();
         *out = new MediaBuffer(0);
         err = OK;
	}
    input->release();
    input = NULL;
#else
    if (vpx_codec_decode(
                (vpx_codec_ctx_t *)mCtx,
                (uint8_t *)input->data() + input->range_offset(),
                input->range_length(),
                NULL,
                0)) {
        ALOGE("on2 decoder failed to decode frame.");
        input->release();
        input = NULL;

        return UNKNOWN_ERROR;
    }

    ALOGV("successfully decoded 1 or more frames.");

    int64_t timeUs;
    CHECK(input->meta_data()->findInt64(kKeyTime, &timeUs));

    input->release();
    input = NULL;

    bool skipFrame = false;

    if (mTargetTimeUs >= 0) {
        CHECK(timeUs <= mTargetTimeUs);

        if (timeUs < mTargetTimeUs) {
            // We're still waiting for the frame with the matching
            // timestamp and we won't return the current one.
            skipFrame = true;

            ALOGV("skipping frame at %lld us", timeUs);
        } else {
            ALOGV("found target frame at %lld us", timeUs);

            mTargetTimeUs = -1;
        }
    }

    if (skipFrame) {
        *out = new MediaBuffer(0);
        return OK;
    }

    vpx_codec_iter_t iter = NULL;
    vpx_image_t *img = vpx_codec_get_frame((vpx_codec_ctx_t *)mCtx, &iter);

    if (img == NULL) {
        ALOGI("on2 decoder did not return a frame.");

        *out = new MediaBuffer(0);
        return OK;
    }

    CHECK_EQ(img->fmt, IMG_FMT_I420);

    int32_t width = img->d_w;
    int32_t height = img->d_h;

    if (width != mWidth || height != mHeight) {
        ALOGI("Image dimensions changed, width = %d, height = %d",
             width, height);

        mWidth = width;
        mHeight = height;
        mFormat->setInt32(kKeyWidth, width);
        mFormat->setInt32(kKeyHeight, height);

        mBufferSize = (mWidth * mHeight * 3) / 2;
        delete mBufferGroup;
        mBufferGroup = new MediaBufferGroup;
        mBufferGroup->add_buffer(new MediaBuffer(mBufferSize));
        mBufferGroup->add_buffer(new MediaBuffer(mBufferSize));

        return INFO_FORMAT_CHANGED;
    }

    MediaBuffer *output;
    CHECK_EQ(mBufferGroup->acquire_buffer(&output), OK);

    const uint8_t *srcLine = (const uint8_t *)img->planes[PLANE_Y];
    uint8_t *dst = (uint8_t *)output->data();
    for (size_t i = 0; i < img->d_h; ++i) {
        memcpy(dst, srcLine, img->d_w);

        srcLine += img->stride[PLANE_Y];
        dst += img->d_w;
    }

    srcLine = (const uint8_t *)img->planes[PLANE_U];
    for (size_t i = 0; i < img->d_h / 2; ++i) {
        memcpy(dst, srcLine, img->d_w / 2);

        srcLine += img->stride[PLANE_U];
        dst += img->d_w / 2;
    }

    srcLine = (const uint8_t *)img->planes[PLANE_V];
    for (size_t i = 0; i < img->d_h / 2; ++i) {
        memcpy(dst, srcLine, img->d_w / 2);

        srcLine += img->stride[PLANE_V];
        dst += img->d_w / 2;
    }

    output->set_range(0, (width * height * 3) / 2);

    output->meta_data()->setInt64(kKeyTime, timeUs);

    *out = output;
#endif

    return OK;
}

}  // namespace android

