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
#define LOG_TAG "AVCDecoder"
#include <utils/Log.h>

#undef ALOGD
#define ALOGD
#include "AVCDecoder.h"

#include "avcdec_api.h"
#include "avcdec_int.h"

#include <OMX_Component.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ABitReader.h>
#include "avc_utils.h"

#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/Utils.h>
#include <media/stagefright/foundation/hexdump.h>
#if ON2_AVC_DEC
#define MAX_STREAM_LENGHT 1024*500
#endif

namespace android {

static const char kStartCode[4] = { 0x00, 0x00, 0x00, 0x01 };

static int32_t Malloc(void *userData, int32_t size, int32_t attrs) {
    return reinterpret_cast<int32_t>(malloc(size));
}

static void Free(void *userData, int32_t ptr) {
    free(reinterpret_cast<void *>(ptr));
}
 void AVCDecoder::SetParameterForWimo(const sp<MediaSource> &source)
{
    ALOGE("Into AVCDecoder::SetParameterForWimo \n");
    int32_t width, height;
    int64_t durationUs;
    mSource = source ;
	CHECK(mSource->getFormat()->findInt32(kKeyWidth, &width));
   CHECK(mSource->getFormat()->findInt32(kKeyHeight, &height));
    mFormat->setInt32(kKeyWidth, width);
    mFormat->setInt32(kKeyHeight, height);
    mSource->getFormat()->findInt32(kKeyAvcSendDts, &AvcSendDtsFlag);
    if (mSource->getFormat()->findInt64(kKeyDuration, &durationUs))
    {
        mFormat->setInt64(kKeyDuration, durationUs);
    }

	ALOGE("width = %d ,height = %d \n",width,height);
    ALOGE("Endof AVCDecoder::SetParameterForWimo \n");
  return ;
}
AVCDecoder::AVCDecoder(const sp<MediaSource> &source)
    : mSource(source),
      mStarted(false),
      mHandle(new tagAVCHandle),
      mInputBuffer(NULL),
      mBuffer(NULL),
      mAnchorTimeUs(0),
      mNumSamplesOutput(0),
      mPendingSeekTimeUs(-1),
      mPreTimeUs(0),
      mPendingSeekMode(MediaSource::ReadOptions::SEEK_CLOSEST_SYNC),
      mTargetTimeUs(-1),
      pOn2Dec(NULL),
      mSeeking(false),
      mSPSSeen(false),
      mPPSSeen(false),
      mts_en(0),
      mFirstIFrm(false){
    memset(mHandle, 0, sizeof(tagAVCHandle));
    mHandle->AVCObject = NULL;
    mHandle->userData = this;
    mHandle->CBAVC_DPBAlloc = ActivateSPSWrapper;
    mHandle->CBAVC_FrameBind = BindFrameWrapper;
    mHandle->CBAVC_FrameUnbind = UnbindFrame;
    mHandle->CBAVC_Malloc = Malloc;
    mHandle->CBAVC_Free = Free;

    mFormat = new MetaData;
    uint32_t deInt = 0;
    AvcSendDtsFlag = 0;
    SameTimeCount = 0;
    mTimestamps.clear();
    mFormat->setInt32(kKeyDeIntrelace, deInt);
    mFormat->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_RAW);
    int32_t width, height;
    if(mSource != NULL)
    {
  	ALOGD("wimo1 avcdecoder");
    CHECK(mSource->getFormat()->findInt32(kKeyWidth, &width));
    CHECK(mSource->getFormat()->findInt32(kKeyHeight, &height));
    	mSource->getFormat()->findInt32(kKeyAvcSendDts, &AvcSendDtsFlag);
    	mSource->getFormat()->findInt32(kKeyisTs, &mts_en);
    mFormat->setInt32(kKeyWidth, width);
    mFormat->setInt32(kKeyHeight, height);
    }
    else
    {
 	ALOGD("wimo avcdecoder");
    }
#if ON2_AVC_DEC
    vpu_api_init(&sDecApi, OMX_ON2_VIDEO_CodingAVC);
    pOn2Dec = sDecApi.get_class_On2Decoder();
	mFormat->setInt32(kKeyColorFormat, OMX_COLOR_FormatYUV420SemiPlanar);

    streamInbuf = NULL;
    _success = true;

    if(pOn2Dec) {
        if(sDecApi.init_class_On2Decoder_AVC(pOn2Dec,0)) {
            ALOGE("init avc decoder fail in AVCDecoder construct");
					_success = false;
		}
    } else {
				_success = false;
	}
    mFormat->setCString(kKeyDecoderComponent, "AVCDecoder");

 #else
    mFormat->setInt32(kKeyColorFormat, OMX_COLOR_FormatYUV420Planar);
    mFormat->setCString(kKeyDecoderComponent, "AVCDecoder");
 #endif
		ALOGD("AvcDecInit_OMX ok\n");

    int64_t durationUs;
    if (mSource != NULL && mSource->getFormat()->findInt64(kKeyDuration, &durationUs)) {
        mFormat->setInt64(kKeyDuration, durationUs);
    }
}

AVCDecoder::~AVCDecoder() {
    if (mStarted) {
        stop();
    }
#if ON2_AVC_DEC
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
    mTimestamps.clear();
#endif
    PVAVCCleanUpDecoder(mHandle);

    delete mHandle;
    mHandle = NULL;
}

static __inline
size_t GetNAL_Config(uint8** bitstream, int32* size)
{
    int i = 0;
    int j;
    uint8* nal_unit = *bitstream;
    int count = 0;
    while (nal_unit[i++] == 0 && i < *size)
    {
    }
    if (nal_unit[i-1] == 1)
    {
        *bitstream = nal_unit + i;
    }
    else
    {
        j = *size;
        *size = 0;
		ALOGD("no SC at the beginning, not supposed to happen");
        return j;  // no SC at the beginning, not supposed to happen
    }
    j = i;
    while (i < *size)
    {
        if (count >= 2 && nal_unit[i] == 0x01)
        {
            i -= 2;
            break;
        }
        if (nal_unit[i])
            count = 0;
        else
            count++;
        i++;
    }
    *size -= i;
    return (i -j);
}
status_t AVCDecoder::start(MetaData *) {
    CHECK(!mStarted);

    uint32_t type;
    const void *data;
    size_t size;
    sp<MetaData> meta = mSource->getFormat();
	{
		int width,height;
	 	CHECK(mSource->getFormat()->findInt32(kKeyWidth, &width));
	 	CHECK(mSource->getFormat()->findInt32(kKeyHeight, &height));
	 	mFormat->setInt32(kKeyWidth, width);
	 	mFormat->setInt32(kKeyHeight, height);
    }
#ifdef AVC_DEBUG
    fp = NULL;
    fp = fopen("/sdcard/H264stream1.bin","wb+");
#endif
    if ((meta->findData(kKeyAVCC, &type, &data, &size)) && size) {
        // Parse the AVCDecoderConfigurationRecord

        uint8_t *ptr = (uint8_t *)data;

        CHECK(size >= 7);
        uint8_t *tp = ptr;
        if (tp[0] == 0 && tp[1] == 0)
        {
           uint8* tmp_ptr = tp;
           uint8* buffer_begin = tp;
           int16 length = 0;
           int initbufsize = size;
           int tConfigSize = 0;
           do
           {
               tmp_ptr += length;
               length = GetNAL_Config(&tmp_ptr, &initbufsize);
               addCodecSpecificData(tmp_ptr, length);
           }
           while (initbufsize > 0);
        }
        else
        {
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
    }
    mSource->start();
#if ON2_AVC_DEC
	if(_success == false)
	{
		return !OK;
	}
#endif
    mAnchorTimeUs = 0;
    mNumSamplesOutput = 0;
    mPendingSeekTimeUs = -1;
    mPendingSeekMode = ReadOptions::SEEK_CLOSEST_SYNC;
    mTargetTimeUs = -1;
    mSPSSeen = false;
    mPPSSeen = false;
    mStarted = true;

    return OK;
}

void AVCDecoder::addCodecSpecificData(const uint8_t *data, size_t size) {
    MediaBuffer *buffer = new MediaBuffer(size + 4);
    memcpy(buffer->data(), kStartCode, 4);
    memcpy((uint8_t *)buffer->data() + 4, data, size);
    buffer->set_range(0, size + 4);

    mCodecSpecificData.push(buffer);
}

status_t AVCDecoder::stop() {
	if(mStarted == 0)
		return OK;
    CHECK(mStarted);

#ifdef AVC_DEBUG
	if(fp)
	  fclose(fp);
    fp = NULL;
#endif
    for (size_t i = 0; i < mCodecSpecificData.size(); ++i) {
        (*mCodecSpecificData.editItemAt(i)).release();
    }
    mCodecSpecificData.clear();

    if (mInputBuffer) {
        mInputBuffer->release();
        mInputBuffer = NULL;
    }

    mSource->stop();

    releaseFrames();

    mStarted = false;

    return OK;
}

sp<MetaData> AVCDecoder::getFormat() {
    return mFormat;
}

static void findNALFragment(
        const MediaBuffer *buffer, const uint8_t **fragPtr, size_t *fragSize) {
    const uint8_t *data =
        (const uint8_t *)buffer->data() + buffer->range_offset();

    size_t size = buffer->range_length();

    CHECK(size >= 4);
    CHECK(!memcmp(kStartCode, data, 4));

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

MediaBuffer *AVCDecoder::drainOutputBuffer() {
    int32_t index;
    int32_t Release;
    AVCFrameIO Output;
    Output.YCbCr[0] = Output.YCbCr[1] = Output.YCbCr[2] = NULL;
    AVCDec_Status status = PVAVCDecGetOutput(mHandle, &index, &Release, &Output);

    if (status != AVCDEC_SUCCESS) {
        ALOGV("PVAVCDecGetOutput returned error %d", status);
        return NULL;
    }

    CHECK(index >= 0);
    CHECK(index < (int32_t)mFrames.size());

    MediaBuffer *mbuf = mFrames.editItemAt(index);

    bool skipFrame = false;

    if (mTargetTimeUs >= 0) {
        int64_t timeUs;
        CHECK(mbuf->meta_data()->findInt64(kKeyTime, &timeUs));
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

    if (!skipFrame) {
        mbuf->set_range(0, mbuf->size());
        mbuf->add_ref();

        return mbuf;
    }

    return new MediaBuffer(0);
}
#if ON2_AVC_DEC
static __inline
int32 video_fill_hdr(uint8_t *dst,uint8_t *src, uint32 size)
{
#if 0
	VPU_BITSTREAM h;
	memset(&h,0,sizeof(VPU_BITSTREAM));
	h.StartCode = 0x524b5642;//VPU_BITSTREAM_START_CODE;
	h.SliceLength= BSWAP(size);
	memcpy(dst, &h, sizeof(VPU_BITSTREAM));
	memcpy((uint8*)(dst + sizeof(VPU_BITSTREAM)),src, size);
	return 0;
#else
	uint head = 0x01000000;
	memcpy(dst,&head,4);
	memcpy((uint8*)(dst + 4),src, size);
	return 0;
#endif
}
#endif

status_t AVCDecoder::read(
        MediaBuffer **out, const ReadOptions *options) {
    *out = NULL;
    struct timeval timeFirst,timesec;
      gettimeofday(&timeFirst, NULL);
    int64_t seekTimeUs;
    ReadOptions::SeekMode mode;
#ifdef MUT_SLICE
    int64_t inputTime = 0LL;
#endif
    if (options && options->getSeekTo(&seekTimeUs, &mode)) {
        ALOGV("seek requested to %lld us (%.2f secs)", seekTimeUs, seekTimeUs / 1E6);

        CHECK(seekTimeUs >= 0);
        mPendingSeekTimeUs = seekTimeUs;
        mPendingSeekMode = mode;

        if (mInputBuffer) {
            mInputBuffer->release();
            mInputBuffer = NULL;
        }

        PVAVCDecReset(mHandle);
    }
 	
    if (mInputBuffer == NULL) {
        ALOGV("fetching new input buffer.");

        bool seeking = false;

        if (!mCodecSpecificData.isEmpty()) {
            mInputBuffer = mCodecSpecificData.editItemAt(0);
            mCodecSpecificData.removeAt(0);
        } else {
            for (;;) {
                if (mPendingSeekTimeUs >= 0) {
                    ALOGV("reading data from timestamp %lld (%.2f secs)",
                         mPendingSeekTimeUs, mPendingSeekTimeUs / 1E6);
                }

                ReadOptions seekOptions;
                if (mPendingSeekTimeUs >= 0) {
                    seeking = true;
                    mSeeking = true;
#ifdef MUT_SLICE
                    mBuffer->setRange(0, 0);
#endif
                    seekOptions.setSeekTo(mPendingSeekTimeUs, mPendingSeekMode);
                    mPendingSeekTimeUs = -1;
                }

                status_t err = OK;

                if (seekOptions.getSeekTo(&seekTimeUs, &mode)) {
                    err = mSource->read(&mInputBuffer, &seekOptions);
                } else {
                    err = mSource->read(&mInputBuffer, NULL);
                }

                seekOptions.clearSeekTo();

                if (err != OK) {
#if  ON2_AVC_DEC
                    *out = NULL;
#else
					*out = drainOutputBuffer();
#endif
                    return (*out == NULL)  ? err : (status_t)OK;
                }

                if (mInputBuffer->range_length() > 0) {
#ifdef MUT_SLICE
                    int size = mInputBuffer->range_length();
                    uint8_t *data = (uint8_t*)mInputBuffer->data();
                    size_t neededSize = (mBuffer == NULL ? 0 : mBuffer->size()) + size;
                    if (mBuffer == NULL || neededSize > mBuffer->capacity()) {
                        neededSize = (neededSize + 65535) & ~65535;
                        ALOGV("resizing buffer to size %d", neededSize);
                        sp<ABuffer> buffer = new ABuffer(neededSize);
                        if (mBuffer != NULL) {
                            memcpy(buffer->data(), mBuffer->data(), mBuffer->size());
                            buffer->setRange(0, mBuffer->size());
                        } else {
                            buffer->setRange(0, 0);
                        }
                        mBuffer = buffer;
                    }
                    memcpy(mBuffer->data() + mBuffer->size(), data, size);
                    mBuffer->setRange(0, mBuffer->size() + size);
                    mInputBuffer->meta_data()->findInt64(kKeyTime, &inputTime);
                    mInputBuffer->release();
                    mInputBuffer = NULL;
#endif
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



#if ON2_AVC_DEC
    status_t err = UNKNOWN_ERROR;
    MediaBuffer * aOutBuffer = NULL;
    uint32_t aOutputLength = 0;
#ifndef MUT_SLICE
	size_t inSize = mInputBuffer->range_length();
	uint8_t * inPtr = NULL;

	inPtr = (uint8_t *)mInputBuffer->data() + mInputBuffer->range_offset();//othe type data,not pass the first nal type
#else
    if(mInputBuffer == NULL){
        mInputBuffer = dequeueOneFrame();
    }
    if(mInputBuffer == NULL){
         if(*out == NULL)
    			*out = new MediaBuffer(0);
         return OK;
    }
    size_t inSize = mInputBuffer->range_length();
	uint8_t * inPtr = NULL;
	inPtr = (uint8_t *)mInputBuffer->data() + mInputBuffer->range_offset();//othe type data,not pass the first nal type
#endif
#ifdef AVC_DEBUG
	if(inPtr && fp)
		fwrite(inPtr,1,inSize,fp);
    fflush(fp);
#endif

	if (pOn2Dec) {
        aOutBuffer = new MediaBuffer(sizeof(VPU_FRAME));
#ifndef MUT_SLICE
		int64_t inputTime = 0LL;
		mInputBuffer->meta_data()->findInt64(kKeyTime, &inputTime);
#endif
		ALOGV("----->inputTime %lld",inputTime);
        if (mSeeking) {
            mTimestamps.clear();
            lastTimestamps = -1;
            mPreTimeUs = 0;
        }
        if(AvcSendDtsFlag)
        {
            ALOGV("push the media time");
            if(inputTime != lastTimestamps)
            {
                mTimestamps.push_back(inputTime);
                lastTimestamps = inputTime;
            }
        }
		uint8_t* pOutBuf = (uint8_t*)aOutBuffer->data();
        memset(pOutBuf,0,sizeof(VPU_FRAME));
         if (mSeeking) {
            int seekstatus = 0;
            if(mInputBuffer->meta_data()->findInt32(kKeySeekFail, &seekstatus) && seekstatus)
            {
                ALOGE("no REST");
            }
            else
            {
                sDecApi.reset_class_On2Decoder(pOn2Dec);
            }
        }

        if(sDecApi.dec_oneframe_class_On2Decoder_WithTimeStamp(
                pOn2Dec,pOutBuf,
                (uint32_t *)&aOutputLength,
                inPtr,
                (uint32_t *)&inSize,
                (int64_t*)&inputTime) == -1) {

            mInputBuffer->release();
            mInputBuffer = NULL;
            aOutBuffer->release();
            return  NO_MEMORY;
        }
        if (mSeeking) {
            int seekstatus = 0;
            if(mInputBuffer->meta_data()->findInt32(kKeySeekFail, &seekstatus) && seekstatus)
            {
                ALOGE("no seek");
                mSeeking = false;
            }
            else
            {
                mSeeking = false;
                aOutputLength = 0;
                aOutBuffer->releaseframe();
                aOutBuffer = NULL;
            }
        }
		if(aOutputLength)
		{
			VPU_FRAME *frame = (VPU_FRAME *)pOutBuf;
            int64_t outputTime = 0;
            if(AvcSendDtsFlag)
            {
                ALOGV("pop the media time");
                if(mTimestamps.size())
                {
                    outputTime = *mTimestamps.begin();
                    mTimestamps.erase(mTimestamps.begin());
                }
                if (mFirstIFrm == false) {
                    while (outputTime <=inputTime) {
                        outputTime = *mTimestamps.begin();
                        mTimestamps.erase(mTimestamps.begin());
                    }

                    mFirstIFrm = true;

                }
            }
            else
            {
                outputTime = inputTime;
            }

            int32_t oldWidth = 0;
            int32_t oldHeight = 0;

            if((mFormat->findInt32(kKeyWidth, &oldWidth)) &&
                (mFormat->findInt32(kKeyHeight, &oldHeight))) {

                if ((oldWidth != frame->DisplayWidth) ||
                        (oldHeight != frame->DisplayHeight)) {

                    mFormat->setInt32(kKeyWidth, frame->DisplayWidth);
                    mFormat->setInt32(kKeyHeight, frame->DisplayHeight);
                    aOutBuffer->releaseframe();//INFO Changed ,the output must be NULL;

                    return INFO_FORMAT_CHANGED;
                }
            }

            ALOGV("----->outputTime %lld \n",outputTime);
            if ((mPreTimeUs >0) && (outputTime <mPreTimeUs)) {
                ALOGV("AVC output frame timestamp revert, outputTime: %lld, mPreTimeUs: %lld", outputTime, mPreTimeUs);
                aOutBuffer->meta_data()->setInt32(kKeyTimeRevert, 1);
            }
            if(mPreTimeUs == outputTime)
            {
                SameTimeCount++;
            }
            else
            {
                SameTimeCount = 0;
            mPreTimeUs = outputTime;
            }
			aOutBuffer->meta_data()->setInt64(kKeyTime,(outputTime + SameTimeCount*33*1000));
            aOutBuffer->setObserver(this);
            aOutBuffer->add_ref();
			*out = aOutBuffer;
            err = OK;
		}
		else
		{
            if(aOutBuffer)
            {
                aOutBuffer->release();
                aOutBuffer = NULL;
            }
            if(*out == NULL)
    			*out = new MediaBuffer(0);
            err = OK;
		}
	}

    if (inSize < 2)
    {
    	mInputBuffer->release();
    	mInputBuffer = NULL;
    }
    return err;
#else
	const uint8_t *fragPtr;
    size_t fragSize;
    findNALFragment(mInputBuffer, &fragPtr, &fragSize);

    bool releaseFragment = true;
    status_t err = UNKNOWN_ERROR;

    int nalType;
    int nalRefIdc;
    AVCDec_Status res =
        PVAVCDecGetNALType(
                const_cast<uint8_t *>(fragPtr), fragSize,
                &nalType, &nalRefIdc);

    if (res != AVCDEC_SUCCESS) {
        ALOGE("cannot determine nal type");
    } else if (nalType == AVC_NALTYPE_SPS || nalType == AVC_NALTYPE_PPS
                || (mSPSSeen && mPPSSeen)) {
        switch (nalType) {
            case AVC_NALTYPE_SPS:
            {
                mSPSSeen = true;

                res = PVAVCDecSeqParamSet(
                        mHandle, const_cast<uint8_t *>(fragPtr),
                        fragSize);

                if (res != AVCDEC_SUCCESS) {
                    ALOGV("PVAVCDecSeqParamSet returned error %d", res);
                    break;
                }

                AVCDecObject *pDecVid = (AVCDecObject *)mHandle->AVCObject;

                int32_t width =
                    (pDecVid->seqParams[0]->pic_width_in_mbs_minus1 + 1) * 16;

                int32_t height =
                    (pDecVid->seqParams[0]->pic_height_in_map_units_minus1 + 1) * 16;

                int32_t crop_left, crop_right, crop_top, crop_bottom;
                if (pDecVid->seqParams[0]->frame_cropping_flag)
                {
                    crop_left = 2 * pDecVid->seqParams[0]->frame_crop_left_offset;
                    crop_right =
                        width - (2 * pDecVid->seqParams[0]->frame_crop_right_offset + 1);

                    if (pDecVid->seqParams[0]->frame_mbs_only_flag)
                    {
                        crop_top = 2 * pDecVid->seqParams[0]->frame_crop_top_offset;
                        crop_bottom =
                            height -
                            (2 * pDecVid->seqParams[0]->frame_crop_bottom_offset + 1);
                    }
                    else
                    {
                        crop_top = 4 * pDecVid->seqParams[0]->frame_crop_top_offset;
                        crop_bottom =
                            height -
                            (4 * pDecVid->seqParams[0]->frame_crop_bottom_offset + 1);
                    }
                } else {
                    crop_bottom = height - 1;
                    crop_right = width - 1;
                    crop_top = crop_left = 0;
                }

                int32_t prevCropLeft, prevCropTop;
                int32_t prevCropRight, prevCropBottom;
                if (!mFormat->findRect(
                            kKeyCropRect,
                            &prevCropLeft, &prevCropTop,
                            &prevCropRight, &prevCropBottom)) {
                    prevCropLeft = prevCropTop = 0;
                    prevCropRight = width - 1;
                    prevCropBottom = height - 1;
                }

                int32_t oldWidth, oldHeight;
                CHECK(mFormat->findInt32(kKeyWidth, &oldWidth));
                CHECK(mFormat->findInt32(kKeyHeight, &oldHeight));

                if (oldWidth != width || oldHeight != height
                        || prevCropLeft != crop_left
                        || prevCropTop != crop_top
                        || prevCropRight != crop_right
                        || prevCropBottom != crop_bottom) {
                    mFormat->setRect(
                            kKeyCropRect,
                            crop_left, crop_top, crop_right, crop_bottom);

                    mFormat->setInt32(kKeyWidth, width);
                    mFormat->setInt32(kKeyHeight, height);

                    err = INFO_FORMAT_CHANGED;
                } else {
                    *out = new MediaBuffer(0);
                    err = OK;
                }
                break;
            }

            case AVC_NALTYPE_PPS:
            {
                mPPSSeen = true;

                res = PVAVCDecPicParamSet(
                        mHandle, const_cast<uint8_t *>(fragPtr),
                        fragSize);

                if (res != AVCDEC_SUCCESS) {
                    ALOGV("PVAVCDecPicParamSet returned error %d", res);
                    break;
                }

                *out = new MediaBuffer(0);

                err = OK;
                break;
            }

            case AVC_NALTYPE_SLICE:
            case AVC_NALTYPE_IDR:
            {
                res = PVAVCDecodeSlice(
                        mHandle, const_cast<uint8_t *>(fragPtr),
                        fragSize);

                if (res == AVCDEC_PICTURE_OUTPUT_READY) {
                    MediaBuffer *mbuf = drainOutputBuffer();
                    if (mbuf == NULL) {
                        break;
                    }

                    *out = mbuf;

                    // Do _not_ release input buffer yet.

                    releaseFragment = false;
                    err = OK;
                    break;
                }

                if (res == AVCDEC_PICTURE_READY || res == AVCDEC_SUCCESS) {
                    *out = new MediaBuffer(0);

                    err = OK;
                } else {
                    ALOGV("PVAVCDecodeSlice returned error %d", res);
                }
                break;
            }

            case AVC_NALTYPE_SEI:
            {
                res = PVAVCDecSEI(
                        mHandle, const_cast<uint8_t *>(fragPtr),
                        fragSize);

                if (res != AVCDEC_SUCCESS) {
                    break;
                }

                *out = new MediaBuffer(0);

                err = OK;
                break;
            }

            case AVC_NALTYPE_AUD:
            case AVC_NALTYPE_FILL:
            case AVC_NALTYPE_EOSEQ:
            {
                *out = new MediaBuffer(0);

                err = OK;
                break;
            }

            default:
            {
                ALOGE("Should not be here, unknown nalType %d", nalType);
                CHECK(!"Should not be here");
                break;
            }
        }
    } else {
        // We haven't seen SPS or PPS yet.

        *out = new MediaBuffer(0);
        err = OK;
    }

    if (releaseFragment) {
        size_t offset = mInputBuffer->range_offset();
        if (fragSize + 4 == mInputBuffer->range_length()) {
            mInputBuffer->release();
            mInputBuffer = NULL;
        } else {
            mInputBuffer->set_range(
                    offset + fragSize + 4,
                    mInputBuffer->range_length() - fragSize - 4);
        }
    }

    return err;
#endif
}

// static
int32_t AVCDecoder::ActivateSPSWrapper(
        void *userData, unsigned int sizeInMbs, unsigned int numBuffers) {
    return static_cast<AVCDecoder *>(userData)->activateSPS(sizeInMbs, numBuffers);
}

// static
int32_t AVCDecoder::BindFrameWrapper(
        void *userData, int32_t index, uint8_t **yuv) {
    return static_cast<AVCDecoder *>(userData)->bindFrame(index, yuv);
}

// static
void AVCDecoder::UnbindFrame(void *userData, int32_t index) {
}

int32_t AVCDecoder::activateSPS(
        unsigned int sizeInMbs, unsigned int numBuffers) {
    CHECK(mFrames.isEmpty());

    size_t frameSize = (sizeInMbs << 7) * 3;
    for (unsigned int i = 0; i < numBuffers; ++i) {
        MediaBuffer *buffer = new MediaBuffer(frameSize);
        buffer->setObserver(this);

        mFrames.push(buffer);
    }

    return 1;
}

int32_t AVCDecoder::bindFrame(int32_t index, uint8_t **yuv) {
    CHECK(index >= 0);
    CHECK(index < (int32_t)mFrames.size());

    CHECK(mInputBuffer != NULL);
    int64_t timeUs;
    CHECK(mInputBuffer->meta_data()->findInt64(kKeyTime, &timeUs));
    mFrames[index]->meta_data()->setInt64(kKeyTime, timeUs);

    *yuv = (uint8_t *)mFrames[index]->data();

    return 1;
}

void AVCDecoder::releaseFrames() {
    for (size_t i = 0; i < mFrames.size(); ++i) {
        MediaBuffer *buffer = mFrames.editItemAt(i);

        buffer->setObserver(NULL);
        buffer->release();
    }
    mFrames.clear();
}

#ifdef MUT_SLICE
struct NALPosition {
    size_t nalOffset;
    size_t nalSize;
};
MediaBuffer *AVCDecoder::dequeueOneFrame() {
    const uint8_t *data = mBuffer->data();
    size_t size = mBuffer->size();
    Vector<NALPosition> nals;
    size_t totalSize = 0;
    status_t err;
    const uint8_t *nalStart;
    size_t nalSize;
    bool foundSlice = false;
    while ((err = getNextNALUnit(&data, &size, &nalStart, &nalSize)) == OK) {
        CHECK_GT(nalSize, 0u);
        unsigned nalType = nalStart[0] & 0x1f;
        bool flush = false;
        if (nalType == 1 || nalType == 5) {
            if (foundSlice) {
                ABitReader br(nalStart + 1, nalSize);
                unsigned first_mb_in_slice = parseUE(&br);
                if (first_mb_in_slice == 0) {
                    flush = true;
                }
            }
            foundSlice = true;
        } else if ((nalType == 9 || nalType == 7) && foundSlice) {
            flush = true;
        }
        if (flush) {
            size_t auSize = 4 * nals.size() + totalSize;
            MediaBuffer * accessUnit = new MediaBuffer(auSize);
            size_t dstOffset = 0;
            for (size_t i = 0; i < nals.size(); ++i) {
                const NALPosition &pos = nals.itemAt(i);
                unsigned nalType = mBuffer->data()[pos.nalOffset] & 0x1f;
                memcpy(accessUnit->data() + dstOffset, "\x00\x00\x00\x01", 4);
                memcpy(accessUnit->data() + dstOffset + 4,
                       mBuffer->data() + pos.nalOffset,
                       pos.nalSize);
                dstOffset += pos.nalSize + 4;
            }
            size_t nextScan = 0;
            const NALPosition &pos = nals.itemAt(nals.size() - 1);
            nextScan = pos.nalOffset + pos.nalSize;
            memmove(mBuffer->data(),
                    mBuffer->data() + nextScan,
                    mBuffer->size() - nextScan);
            mBuffer->setRange(0, mBuffer->size() - nextScan);
            int64_t timeUs = 0;
            accessUnit->meta_data()->setInt64(kKeyTime, timeUs);
            return accessUnit;
        }
        NALPosition pos;
        pos.nalOffset = nalStart - mBuffer->data();
        pos.nalSize = nalSize;
        nals.push(pos);
        totalSize += nalSize;
    }
    return NULL;
}
#endif
void AVCDecoder::signalBufferReturned(MediaBuffer *buffer) {
    buffer->setObserver(0);

    CHECK_EQ(buffer->refcount(), 0);

    buffer->release();
    buffer = NULL;
}

}  // namespace android
