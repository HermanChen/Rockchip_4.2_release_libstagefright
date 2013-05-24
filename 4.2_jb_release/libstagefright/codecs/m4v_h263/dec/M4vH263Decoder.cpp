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
#define LOG_TAG "M4vH263Decoder"
#include <utils/Log.h>
#include <stdlib.h> // for free
#include "ESDS.h"
#include "M4vH263Decoder.h"

#include "mp4dec_api.h"

#if ON2_M4V_h263_DEC
#include "vpu_global.h"
#define MAX_STREAM_LENGHT 1024*500
#endif
#undef ALOGD
#define ALOGD
#include <OMX_Component.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/Utils.h>
#include <cutils/properties.h>
#define SUPPORT_DIV3_DIVX_3IV2 1

namespace android {

M4vH263Decoder::M4vH263Decoder(const sp<MediaSource> &source)
    : mSource(source),
      mStarted(false),
	  aFramecount(0),
      mInputBuffer(NULL),
      mNumSamplesOutput(0),
      mTargetTimeUs(-1),
      mIsDiv3(0){

    ALOGD("M4vH263Decoder\n");
#if !ON2_M4V_h263_DEC
    mHandle = new tagvideoDecControls;
    memset(mHandle, 0, sizeof(tagvideoDecControls));
#endif
    mFormat = new MetaData;
    uint32_t deInt = 0;
    mFormat->setInt32(kKeyDeIntrelace, deInt);
    mFormat->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_RAW);
    if(mSource != NULL)
	{
    CHECK(mSource->getFormat()->findInt32(kKeyWidth, &mWidth));
    CHECK(mSource->getFormat()->findInt32(kKeyHeight, &mHeight));
		mFormat->setInt32(kKeyWidth, mWidth);
		mFormat->setInt32(kKeyHeight, mHeight);
	}
	else
	{
		ALOGD("Wimo_Mjpeg--------------------");

		ALOGE("mWidth = 0x%x,mHeight = 0x%x\n",mWidth,mHeight);
	}
    // We'll ignore the dimension advertised by the source, the decoder
    // appears to require us to always start with the default dimensions
    // of 352 x 288 to operate correctly and later react to changes in
    // the dimensions as needed.
   // mWidth = 352;
   // mHeight = 288;
#if ON2_M4V_h263_DEC
    pOn2Dec = NULL;
    Extrabuf = NULL;
#endif

    mFormat->setInt32(kKeyWidth, mWidth);
    mFormat->setInt32(kKeyHeight, mHeight);
    ALOGI("mWidth = 0x%x,mHeight = 0x%x\n",mWidth,mHeight);
    mFormat->setInt32(kKeyColorFormat, OMX_COLOR_FormatYUV420SemiPlanar);
    mFormat->setCString(kKeyDecoderComponent, "M4vH263Decoder");
}

void M4vH263Decoder::SetParameterForWimo(const sp<MediaSource> &source)
{
	ALOGE("Into AVCDecoder::SetParameterForWimo \n");
    int32_t width, height;
    mSource = source ;
	CHECK(mSource->getFormat()->findInt32(kKeyWidth, &width));
    CHECK(mSource->getFormat()->findInt32(kKeyHeight, &height));
    mFormat->setInt32(kKeyWidth, width);
    mFormat->setInt32(kKeyHeight, height);
	ALOGE("width = %d ,height = %d \n",width,height);	 
    ALOGE("Endof AVCDecoder::SetParameterForWimo \n");
  return ;
}
M4vH263Decoder::~M4vH263Decoder() {
    if (mStarted) {
        stop();
    }
#if ON2_M4V_h263_DEC
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
#else
    delete mHandle;
    mHandle = NULL;
#endif
}

void M4vH263Decoder::allocateFrames(int32_t width, int32_t height) {
    ALOGD("allocateFrames \n");
    size_t frameSize =
        (((width + 15) & - 16) * ((height + 15) & - 16) * 3) / 2;

    for (uint32_t i = 0; i < 2; ++i) {
        mFrames[i] = new MediaBuffer(frameSize);
        mFrames[i]->setObserver(this);
    }

    PVSetReferenceYUV(
            mHandle,
            (uint8_t *)mFrames[1]->data());
}

#if ON2_M4V_h263_DEC
static __inline
int32_t video_fill_hdr(uint8_t *dst,uint8_t *src, uint32_t size, int64_t time, uint32_t type, uint32_t num)
{
    VPU_BITSTREAM h;
    uint32_t TimeLow = (uint32_t)(time/1000);
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
#endif
status_t M4vH263Decoder::start(MetaData *) {
    CHECK(!mStarted);
    ALOGD("start \n");
    const char *mime = NULL;
    sp<MetaData> meta = mSource->getFormat();
    CHECK(meta->findCString(kKeyMIMEType, &mime));

#if (SUPPORT_DIV3_DIVX_3IV2 ==0)
    int32_t isDivX = 0;
    if (mSource->getFormat()->findInt32(kKeyIsDivX, &isDivX)) {
        if (isDivX) {
            ALOGV("user set Divx not support");
            return ERROR_UNSUPPORTED;
        }
    }

    int32_t isDiv3 = 0;
    if (mSource->getFormat()->findInt32(kKeyIsDiv3, &isDiv3)) {
        if (isDiv3) {
            ALOGV("user set Div3 not support");
            return ERROR_UNSUPPORTED;
        }
    }

    int32_t is3iv2 =0;
    if (mSource->getFormat()->findInt32(kKeyIs3iv2, &is3iv2)) {
        if (is3iv2) {
            ALOGV("user set 3iv2 not support");
            return ERROR_UNSUPPORTED;
        }
    }
#endif

    int32_t tmp = 0;
    if (mSource->getFormat()->findInt32(kKeyIsDiv3, &tmp)) {
        if (tmp) {
            char value[PROPERTY_VALUE_MAX];
            bool div3Support = true;
            if(property_get("media.cfg.div3.support", value, NULL)){
                if (strstr(value, "true")) {
                    div3Support = true;
                } else {
                    div3Support = false;
                }
            }
            if (div3Support == false) {
                ALOGI("Div3 not support");
                return ERROR_UNSUPPORTED;
            }
        }
    }
    MP4DecodingMode mode;
    if (!strcasecmp(MEDIA_MIMETYPE_VIDEO_MPEG4, mime)) {
        mode = MPEG4_MODE;
    } else if (!strcasecmp(MEDIA_MIMETYPE_VIDEO_H263, mime)){
        mode = H263_MODE;
    } else {
        CHECK(!strcasecmp(MEDIA_MIMETYPE_VIDEO_MJPEG, mime));
        mode = MJPEG_MODE;
    }
#ifdef M4V_DEBUG
    fp = NULL;
    fp = fopen("/sdcard/mpeg4stream.bin","wb+");
#endif
    uint32_t type;
    const void *data = NULL;
    size_t size = 0;
    uint8_t *vol_data[1] = {0};
    int32_t vol_size = 0;
    if (meta->findData(kKeyESDS, &type, &data, &size)) {
        ESDS esds((const uint8_t *)data, size);

        const void *codec_specific_data;
        size_t codec_specific_data_size;
        if (esds.InitCheck() !=OK) {
            codec_specific_data = data;
            codec_specific_data_size = size;
        } else {
	        esds.getCodecSpecificInfo(
	                &codec_specific_data, &codec_specific_data_size);
        }

        vol_data[0] = (uint8_t *) malloc(codec_specific_data_size);
        memcpy(vol_data[0], codec_specific_data, codec_specific_data_size);
#if ON2_M4V_h263_DEC
        Extrabuf = (uint8_t*)malloc(codec_specific_data_size);
        ExtraLen = codec_specific_data_size;
        memcpy(Extrabuf,codec_specific_data,ExtraLen);
 #endif
        vol_size = codec_specific_data_size;
    } else {
        vol_data[0] = NULL;
        vol_size = 0;

    }
#if !ON2_M4V_h263_DEC
    Bool success = PVInitVideoDecoder(
            mHandle, vol_data, &vol_size, 1, mWidth, mHeight, mode);
    if (vol_data[0]) free(vol_data[0]);

    if (success != PV_TRUE) {
        ALOGW("PVInitVideoDecoder failed. Unsupported content?");
        return ERROR_UNSUPPORTED;
    }

    MP4DecodingMode actualMode = PVGetDecBitstreamMode(mHandle);
    if (mode != actualMode) {
        PVCleanUpVideoDecoder(mHandle);
        return UNKNOWN_ERROR;
    }

    PVSetPostProcType((VideoDecControls *) mHandle, 0);

    int32_t width, height;
    PVGetVideoDimensions(mHandle, &width, &height);
    if (mode == H263_MODE && (width == 0 || height == 0)) {
        width = 352;
        height = 288;
    }
    allocateFrames(width, height);
#endif
#if ON2_M4V_h263_DEC
    if(mode == H263_MODE)
    {
        VPU_GENERIC vpug;
        vpug.CodecType = VPU_CODEC_DEC_H263;
        vpug.ImgHeight = 0;
        vpug.ImgWidth = 0;
        mYuvLen = ((mWidth + 15)&(~15)) * ((mHeight + 15)&(~15)) *3/2;
        pOn2Dec = NULL;
        streamInbuf = NULL;
        streamInbuf = (uint8_t*)malloc(MAX_STREAM_LENGHT);
        _success = true;
        vpu_api_init(&sDecApi, OMX_ON2_VIDEO_CodingH263);
        pOn2Dec = sDecApi.get_class_On2Decoder();

    	if(pOn2Dec){
            if(sDecApi.init_class_On2Decoder_M4VH263(pOn2Dec, &vpug)) {
                ALOGE("init H263 decoder fail in start");
                _success = false;
            }
    	} else {
            _success = false;
        }
        if(mWidth%176 != 0 ||mHeight%144 != 0)
        {
            /* Sub QCIF */
            if ((mWidth == 0x80) && (mHeight == 0x60)) {
                _success = true;
            } else {
                _success = false;
            }
        }
        ALOGD("H263DecInit_OMX ok\n");
    }
    else if(mode == MPEG4_MODE)
    {
        VPU_GENERIC vpug;
        vpug.ImgHeight = 0;
        vpug.ImgWidth = 0;
        mYuvLen = ((mWidth + 15)&(~15)) * ((mHeight + 15)&(~15)) *3/2;
        pOn2Dec = NULL;
        streamInbuf = NULL;
        streamInbuf = (uint8_t*)malloc(MAX_STREAM_LENGHT);
        _success = true;
        meta->findInt32(kKeyIsDiv3,&mIsDiv3);
        if(mIsDiv3)
        {
			vpug.ImgHeight = (mHeight + 15)&(~15);
        	       vpug.ImgWidth = (mWidth + 15)&(~15);
			vpug.CodecType = VPU_CODEC_DEC_DIVX3;
        }
        else
        {
			vpug.CodecType = VPU_CODEC_DEC_MPEG4;
        }
        vpu_api_init(&sDecApi, OMX_ON2_VIDEO_CodingMPEG4);
        pOn2Dec = sDecApi.get_class_On2Decoder();

        if(pOn2Dec){
            if(sDecApi.init_class_On2Decoder_M4VH263(pOn2Dec, &vpug)) {
                ALOGE("init H263 decoder fail in start");
                _success = false;
            }
        } else {
            _success = false;
        }
        ALOGD("Mpeg4DecInit_OMX ok\n");
    }else {
        VPU_GENERIC vpug;
        vpug.ImgHeight = 0;
        vpug.ImgWidth = 0;
        mYuvLen = ((mWidth + 15)&(~15)) * ((mHeight + 15)&(~15)) *3/2;
        pOn2Dec = NULL;
        streamInbuf = NULL;
        streamInbuf = (uint8_t*)malloc(MAX_STREAM_LENGHT);
        _success = true;
        vpug.CodecType = VPU_CODEC_DEC_MJPEG;
        vpu_api_init(&sDecApi, OMX_ON2_VIDEO_CodingMJPEG);
        pOn2Dec = sDecApi.get_class_On2Decoder();

        if(pOn2Dec){
            if(sDecApi.init_class_On2Decoder(pOn2Dec)) {
                ALOGE("init jpeg decoder fail in start");
                _success = false;
            }
        } else {
            _success = false;
        }
        ALOGD("JpegDecInit_OMX ok\n");
    }
    ModeFlag = (uint32_t)mode;
    if(_success == false)
    {
        return !OK;
    }
#endif

    if(mSource->start())
    {
		return !OK;
    }

    mNumSamplesOutput = 0;
    mTargetTimeUs = -1;
    mStarted = true;

    return OK;
}

status_t M4vH263Decoder::stop() {
    CHECK(mStarted);
      ALOGD("stop \n");
    if (mInputBuffer) {
        mInputBuffer->release();
        mInputBuffer = NULL;
    }
#ifdef M4V_DEBUG
    if(fp)
    {
        fclose(fp);
        fp = NULL;
    }
#endif
#if ON2_M4V_h263_DEC
    if(Extrabuf)
    {
        free(Extrabuf);
        Extrabuf = NULL;
    }
#endif
    mSource->stop();
    mStarted = false;
#if !ON2_M4V_h263_DEC
    releaseFrames();

    return (PVCleanUpVideoDecoder(mHandle) == PV_TRUE)? OK: UNKNOWN_ERROR;
#else
    return OK;
#endif
}

sp<MetaData> M4vH263Decoder::getFormat() {
    return mFormat;
}

status_t M4vH263Decoder::read(
        MediaBuffer **out, const ReadOptions *options) {
    *out = NULL;

    bool seeking = false;
    int64_t seekTimeUs;
    ReadOptions::SeekMode mode;
    if (options && options->getSeekTo(&seekTimeUs, &mode)) {
        seeking = true;
//        CHECK_EQ((int)PVResetVideoDecoder(mHandle), PV_TRUE);
    }

    MediaBuffer *inputBuffer = NULL;
    status_t err = mSource->read(&inputBuffer, options);
    if (err != OK) {
        return err;
    }

    if (seeking) {
        int64_t targetTimeUs;
        if (inputBuffer->meta_data()->findInt64(kKeyTargetTime, &targetTimeUs)
                && targetTimeUs >= 0) {
            mTargetTimeUs = targetTimeUs;
        } else {
            mTargetTimeUs = -1;
        }
#if !ON2_M4V_h263_DEC
        CHECK_EQ(PVResetVideoDecoder(mHandle), PV_TRUE);
#else
     int seekstatus = 0;
     if(inputBuffer->meta_data()->findInt32(kKeySeekFail, &seekstatus) && seekstatus)
     {
        ALOGV("no process");
     }
     else
     {
#if !ON2_M4V_h263_DEC
        CHECK_EQ(PVResetVideoDecoder(mHandle), PV_TRUE);
#else
     sDecApi.reset_class_On2Decoder(pOn2Dec);
#endif
     }
#endif
    }

    uint8_t *bitstream =
        (uint8_t *) inputBuffer->data() + inputBuffer->range_offset();

    uint32_t timestamp = 0xFFFFFFFF;
    uint32_t bufferSize = inputBuffer->range_length();
    uint32_t useExtTimestamp = 0;
    if(bufferSize <5)
    {
        *out = new MediaBuffer(0);
        inputBuffer->release();
        inputBuffer = NULL;
        return OK;
    }
#if ON2_M4V_h263_DEC
    if((MP4DecodingMode)ModeFlag == H263_MODE)
    {
        int64_t timeUs;
        MediaBuffer * aOutBuffer;
        uint32_t aOutputLength = 0;
        aOutBuffer = new MediaBuffer(sizeof(VPU_FRAME));
        uint8_t* pOutBuf = (uint8_t*)aOutBuffer->data();
        CHECK(inputBuffer->meta_data()->findInt64(kKeyTime, &timeUs));
        if((bufferSize + sizeof(VPU_BITSTREAM)) > MAX_STREAM_LENGHT)
        {
           streamInbuf = (uint8_t *)realloc((void*)streamInbuf, (bufferSize+sizeof(VPU_BITSTREAM)));
        }
        video_fill_hdr(streamInbuf, bitstream, bufferSize,timeUs, 0, 0);
        bufferSize += sizeof(VPU_BITSTREAM);
#ifdef M4V_DEBUG
   if(fp)
   {
       fwrite(streamInbuf,1,bufferSize,fp);
       fclose(fp);
       fp = NULL;
   }
#endif
        memset(pOutBuf,0,sizeof(VPU_FRAME));
        if(sDecApi.dec_oneframe_class_On2Decoder(
                pOn2Dec,
                pOutBuf,
                &aOutputLength,
                streamInbuf,
                &bufferSize)) {
            inputBuffer->release();
            inputBuffer = NULL;
            aOutBuffer->releaseframe();
            return UNKNOWN_ERROR;
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
            inputBuffer->release();
            inputBuffer = NULL;
            return OK;
        }
        if(aOutputLength)
    	{
    		VPU_FRAME *frame;
        	frame = (VPU_FRAME *)pOutBuf;
            int32_t width = frame->DisplayWidth;
            int32_t height = frame->DisplayHeight;
            int64_t outputTime = ((int64_t)(frame->ShowTime.TimeHigh) <<32) | ((int64_t)(frame->ShowTime.TimeLow));
            outputTime *=1000;
		    aOutBuffer->meta_data()->setInt64(kKeyTime,outputTime);
		    *out = aOutBuffer;
    	}
    	else
    	{
             aOutBuffer->release();
             *out = new MediaBuffer(0);
             err = OK;
    	}
        inputBuffer->release();
        inputBuffer = NULL;
    }
    else if((MP4DecodingMode)ModeFlag == MPEG4_MODE)
    {
        int64_t timeUs;
        MediaBuffer * aOutBuffer;
        uint32_t aOutputLength = 0;
        aOutBuffer = new MediaBuffer(sizeof(VPU_FRAME));
        uint8_t* pOutBuf = (uint8_t*)aOutBuffer->data();
        CHECK(inputBuffer->meta_data()->findInt64(kKeyTime, &timeUs));
        if(!aFramecount)
        {
            aFramecount++;
            uint8 *Tempbuf = NULL;
            uint32 *Tem1,*Tem2;
            if(Extrabuf)
            {
            Tem1 = (uint32 *)Extrabuf;
            Tem2 = (uint32 *)bitstream;
            if(Tem1[0] != Tem2[0])
            {
                Tempbuf = (uint8 *)malloc(bufferSize + ExtraLen);
                memcpy(Tempbuf,Extrabuf,ExtraLen);
                memcpy(Tempbuf+ExtraLen,bitstream,bufferSize);
                bufferSize += ExtraLen;
                }
            }
        if((bufferSize + sizeof(VPU_BITSTREAM)) > MAX_STREAM_LENGHT)
        {
           streamInbuf = (uint8_t *)realloc((void*)streamInbuf, (bufferSize+sizeof(VPU_BITSTREAM)));
            }
            if(Tempbuf)
            {
                video_fill_hdr(streamInbuf, Tempbuf, bufferSize,timeUs, 0, 0);
                free(Tempbuf);
                Tempbuf = NULL;
            }
            else
            {
               if((bufferSize + sizeof(VPU_BITSTREAM)) > MAX_STREAM_LENGHT)
                {
                    streamInbuf = (uint8_t *)realloc((void*)streamInbuf, (bufferSize+sizeof(VPU_BITSTREAM)));
                }
                video_fill_hdr(streamInbuf, bitstream, bufferSize,timeUs, 0, 0);
            }
            bufferSize += sizeof(VPU_BITSTREAM);
        }
        else
        {
            if((bufferSize + sizeof(VPU_BITSTREAM)) > MAX_STREAM_LENGHT)
            {
               streamInbuf = (uint8_t *)realloc((void*)streamInbuf, (bufferSize+sizeof(VPU_BITSTREAM)));
            }
        video_fill_hdr(streamInbuf, bitstream, bufferSize,timeUs, 0, 0);
        bufferSize += sizeof(VPU_BITSTREAM);
        }
#ifdef M4V_DEBUG
       if(fp)
       {
           fwrite(streamInbuf,1,bufferSize,fp);
           fflush(fp);
       }
#endif
       memset(pOutBuf,0,sizeof(VPU_FRAME));
       if(sDecApi.dec_oneframe_class_On2Decoder(
               pOn2Dec,
               pOutBuf,
               &aOutputLength,
               streamInbuf,
               &bufferSize)) {
            inputBuffer->release();
            inputBuffer = NULL;
            aOutBuffer->releaseframe();
            return  UNKNOWN_ERROR;
       }
        bool skipFrame = false;
        if (mTargetTimeUs >= 0) {
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
            inputBuffer->release();
            inputBuffer = NULL;
            return OK;
        }
        if(aOutputLength)
    	{
    	   VPU_FRAME *frame;
           frame = (VPU_FRAME *)pOutBuf;
           int32_t width = frame->DisplayWidth;
           int32_t height = frame->DisplayHeight;
           if ((!mIsDiv3) &&
		   		((width != mWidth) || (height != mHeight))) {

                inputBuffer->release();
                inputBuffer = NULL;
                aOutBuffer->release();
                aOutBuffer = NULL;
                mWidth = width;
                mHeight = height;
                mFormat->setInt32(kKeyWidth, mWidth);
                mFormat->setInt32(kKeyHeight, mHeight);
                mFormat->setRect(kKeyCropRect, 0, 0, mWidth - 1, mHeight - 1);
                return INFO_FORMAT_CHANGED;
            }
           int64_t outputTime = ((int64_t)(frame->ShowTime.TimeHigh) <<32) | ((int64_t)(frame->ShowTime.TimeLow));
           outputTime *=1000;
			aOutBuffer->meta_data()->setInt64(kKeyTime,outputTime);
    		*out = aOutBuffer;
    	}
    	else
    	{
             aOutBuffer->release();
             *out = new MediaBuffer(0);
             err = OK;
    	}
        inputBuffer->release();
        inputBuffer = NULL;
    }else { //MJPEG
        int64_t timeUs;
        MediaBuffer * aOutBuffer;
        uint32_t aOutputLength = 0;
        aOutBuffer = new MediaBuffer(sizeof(VPU_FRAME));
        uint8_t* pOutBuf = (uint8_t*)aOutBuffer->data();
        CHECK(inputBuffer->meta_data()->findInt64(kKeyTime, &timeUs));
        memset(pOutBuf,0,sizeof(VPU_FRAME));
        if(sDecApi.dec_oneframe_class_On2Decoder(
                pOn2Dec,
                pOutBuf,
                &aOutputLength,
                bitstream,
                &bufferSize)){
            inputBuffer->release();
            inputBuffer = NULL;
            aOutBuffer->releaseframe();
            return  UNKNOWN_ERROR;
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
            inputBuffer->release();
            inputBuffer = NULL;
            return OK;
        }
        if(aOutputLength)
    	{
            int32_t oldWidth = 0;
            int32_t oldHeight = 0;
    		VPU_FRAME *frame;
        	frame = (VPU_FRAME *)pOutBuf;
           int32_t width = frame->DisplayWidth;
           int32_t height = frame->DisplayHeight;
    		int64_t outputTime = timeUs;
            frame->FrameType = 0;
    		aOutBuffer->meta_data()->setInt64(kKeyTime,outputTime);
            if((mFormat->findInt32(kKeyWidth, &oldWidth)) &&
                (mFormat->findInt32(kKeyHeight, &oldHeight))) {

                if ((oldWidth != frame->DisplayWidth) ||
                        (oldHeight != frame->DisplayHeight)) {

                    ALOGI("MJPEG set new width: %d, height: %d",
                        frame->DisplayWidth, frame->DisplayHeight);

                    mFormat->setInt32(kKeyWidth, frame->DisplayWidth);
                    mFormat->setInt32(kKeyHeight, frame->DisplayHeight);
                    aOutBuffer->releaseframe();//INFO Changed ,the output must be NULL;

                    return INFO_FORMAT_CHANGED;
                }
            }
    		*out = aOutBuffer;
    	}
    	else
    	{
             aOutBuffer->release();
             *out = new MediaBuffer(0);
             err = OK;
    	}
        inputBuffer->release();
        inputBuffer = NULL;
    }
#else
    if (PVDecodeVideoFrame(
                mHandle, &bitstream, &timestamp, &bufferSize,
                &useExtTimestamp,
                (uint8_t *)mFrames[mNumSamplesOutput & 0x01]->data())
            != PV_TRUE) {
        ALOGE("failed to decode video frame.");

        inputBuffer->release();
        inputBuffer = NULL;

        return UNKNOWN_ERROR;
    }

    int32_t disp_width, disp_height;
    PVGetVideoDimensions(mHandle, &disp_width, &disp_height);

    int32_t buf_width, buf_height;
    PVGetBufferDimensions(mHandle, &buf_width, &buf_height);

    if (buf_width != mWidth || buf_height != mHeight) {
        ++mNumSamplesOutput;  // The client will never get to see this frame.

        inputBuffer->release();
        inputBuffer = NULL;

        mWidth = buf_width;
        mHeight = buf_height;
        mFormat->setInt32(kKeyWidth, mWidth);
        mFormat->setInt32(kKeyHeight, mHeight);

        CHECK_LE(disp_width, buf_width);
        CHECK_LE(disp_height, buf_height);

        mFormat->setRect(kKeyCropRect, 0, 0, disp_width - 1, disp_height - 1);

        return INFO_FORMAT_CHANGED;
    }

    int64_t timeUs;
    CHECK(inputBuffer->meta_data()->findInt64(kKeyTime, &timeUs));

    inputBuffer->release();
    inputBuffer = NULL;

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
    } else {
        *out = mFrames[mNumSamplesOutput & 0x01];
        (*out)->add_ref();
        (*out)->meta_data()->setInt64(kKeyTime, timeUs);
    }

    ++mNumSamplesOutput;
#endif

    return OK;
}

void M4vH263Decoder::releaseFrames() {
    ALOGD("releaseFrames \n");
    for (size_t i = 0; i < sizeof(mFrames) / sizeof(mFrames[0]); ++i) {
        MediaBuffer *buffer = mFrames[i];

        buffer->setObserver(NULL);
        buffer->release();

        mFrames[i] = NULL;
    }
}

void M4vH263Decoder::signalBufferReturned(MediaBuffer *buffer) {
    ALOGD("signalBufferReturned");
}


}  // namespace android
