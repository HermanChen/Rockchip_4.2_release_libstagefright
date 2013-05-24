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

#include "AACDecoder.h"
#define LOG_TAG "AACDecoder"

#include "../../include/ESDS.h"

#include "pvmp4audiodecoder_api.h"

#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MetaData.h>
#define WRITE_FILE 0
#if WRITE_FILE
	static FILE * aacFp = NULL;
#endif
namespace android {

AACDecoder::AACDecoder(const sp<MediaSource> &source)
    : mSource(source),
      mStarted(false),
      mBufferGroup(NULL),
      mConfig(new tPVMP4AudioDecoderExternal),
      mDecoderBuf(NULL),
      mAnchorTimeUs(0),
      mNumSamplesOutput(0),
      mInputBuffer(NULL) {

    sp<MetaData> srcFormat = mSource->getFormat();
#if WRITE_FILE
	//char msg[255];
	//msg = snprintf(msg,sizeof(msg),getenv("FLASH_STORAGE");
	//strcat()
	aacFp = fopen("/data/data/aac.dat","wb");
	if(aacFp)
		ALOGE("------>>create file success");
#endif

    int32_t sampleRate;
    CHECK(srcFormat->findInt32(kKeySampleRate, &sampleRate));

    mMeta = new MetaData;
    mMeta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_RAW);

    // We'll always output stereo, regardless of how many channels are
    // present in the input due to decoder limitations.
    mMeta->setInt32(kKeyChannelCount, 2);
    mMeta->setInt32(kKeySampleRate, sampleRate);

    int64_t durationUs;
    if (srcFormat->findInt64(kKeyDuration, &durationUs)) {
        mMeta->setInt64(kKeyDuration, durationUs);
    }
    mMeta->setCString(kKeyDecoderComponent, "AACDecoder");

    mInitCheck = initCheck();
}

status_t AACDecoder::initCheck() {
    memset(mConfig, 0, sizeof(tPVMP4AudioDecoderExternal));
    mConfig->outputFormat = OUTPUTFORMAT_16PCM_INTERLEAVED;
    mConfig->aacPlusEnabled = 1;

    // The software decoder doesn't properly support mono output on
    // AACplus files. Always output stereo.
    mConfig->desiredChannels = 2;

    UInt32 memRequirements = PVMP4AudioDecoderGetMemRequirements();
    mDecoderBuf = malloc(memRequirements);

    status_t err = PVMP4AudioDecoderInitLibrary(mConfig, mDecoderBuf);
    if (err != MP4AUDEC_SUCCESS) {
        ALOGE("Failed to initialize MP4 audio decoder");
        return UNKNOWN_ERROR;
    }

    uint32_t type;
    const void *data;
    size_t size;
    sp<MetaData> meta = mSource->getFormat();
    if (meta->findData(kKeyESDS, &type, &data, &size)) {
        ESDS esds((const char *)data, size);
        CHECK_EQ(esds.InitCheck(), (status_t)OK);

        const void *codec_specific_data;
        size_t codec_specific_data_size;
        esds.getCodecSpecificInfo(
                &codec_specific_data, &codec_specific_data_size);

        mConfig->pInputBuffer = (UChar *)codec_specific_data;
        mConfig->inputBufferCurrentLength = codec_specific_data_size;
        mConfig->inputBufferMaxLength = 0;
		UChar * spec = (UChar *)codec_specific_data;
		for(int i = 0; i < codec_specific_data_size; i++)
		{
			ALOGE("spec[%d] = 0x%x",i+1,spec[i]);
		}

        if (PVMP4AudioDecoderConfig(mConfig, mDecoderBuf)
                != MP4AUDEC_SUCCESS) {
            return ERROR_UNSUPPORTED;
        }
    }else if(meta->findData(kKeyAacInfo,&type,&data,&size)){

     	mConfig->pInputBuffer = (UChar *)data;
        mConfig->inputBufferCurrentLength = size;
        mConfig->inputBufferMaxLength = 0;

        if (PVMP4AudioDecoderConfig(mConfig, mDecoderBuf)
                != MP4AUDEC_SUCCESS) {
            return ERROR_UNSUPPORTED;
        }
    }

    return OK;
}

AACDecoder::~AACDecoder() {
    if (mStarted) {
        stop();
    }
#if WRITE_FILE
		if(aacFp)
		{
			fclose(aacFp);
			aacFp = NULL;
		}
#endif

    delete mConfig;
    mConfig = NULL;
}

status_t AACDecoder::start(MetaData *params) {
    CHECK(!mStarted);

    mBufferGroup = new MediaBufferGroup;
    mBufferGroup->add_buffer(new MediaBuffer(4096 * 2));

    mSource->start();

    mAnchorTimeUs = 0;
    mNumSamplesOutput = 0;
    mStarted = true;
    mNumDecodedBuffers = 0;
    mUpsamplingFactor = 2;

    return OK;
}

status_t AACDecoder::stop() {
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

sp<MetaData> AACDecoder::getFormat() {
    return mMeta;
}

status_t AACDecoder::read(
        MediaBuffer **out, const ReadOptions *options) {
    /*
     ** end of aac audio stream in the case of initCheck is not OK,
     ** avoid abnormal playing later.   @Jun 16, 2011. by hbb
     */
    if(mInitCheck != OK) {
        ALOGE("mInitCheck is not OK, so end aac audio stream");
        return ERROR_END_OF_STREAM;
    }

    status_t err;

    *out = NULL;

    int64_t seekTimeUs;
    ReadOptions::SeekMode mode;
    if (options && options->getSeekTo(&seekTimeUs, &mode)) {
        CHECK(seekTimeUs >= 0);

        mNumSamplesOutput = 0;

        if (mInputBuffer) {
            mInputBuffer->release();
            mInputBuffer = NULL;
        }

        // Make sure that the next buffer output does not still
        // depend on fragments from the last one decoded.
        PVMP4AudioDecoderResetBuffer(mDecoderBuf);
    } else {
        seekTimeUs = -1;
    }

repeat:
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
            mNumSamplesOutput = 0;
            }
        } else {
            // We must have a new timestamp after seeking.
            CHECK(seekTimeUs < 0);
        }
    }

    MediaBuffer *buffer;
    CHECK_EQ(mBufferGroup->acquire_buffer(&buffer), (status_t)OK);

    mConfig->pInputBuffer =
        (UChar *)mInputBuffer->data() + mInputBuffer->range_offset();

    mConfig->inputBufferCurrentLength = mInputBuffer->range_length();
    mConfig->inputBufferMaxLength = 0;
    mConfig->inputBufferUsedLength = 0;
    mConfig->remainderBits = 0;

    mConfig->pOutputBuffer = static_cast<Int16 *>(buffer->data());
    mConfig->pOutputBuffer_plus = &mConfig->pOutputBuffer[2048];
#if WRITE_FILE
	if(aacFp)
		fwrite(mConfig->pInputBuffer,1, mConfig->inputBufferCurrentLength,aacFp);
#endif
	//ALOGE("inputlen %d input[0] = %x input[1]%x",  mConfig->inputBufferCurrentLength,(mConfig->pInputBuffer)[0],(mConfig->pInputBuffer)[1]);
    Int decoderErr = MP4AUDEC_SUCCESS;
	if(mConfig->isMutilChannle)
	{
		decoderErr = PVMP4AudioDecodeFrameSixChannel(mConfig, mDecoderBuf);
	}
	else
	{
		decoderErr = PVMP4AudioDecodeFrame(mConfig, mDecoderBuf);
	}


	if (mInputBuffer != NULL) {
		   mInputBuffer->set_range(
				   mInputBuffer->range_offset() + mConfig->inputBufferUsedLength,
				   mInputBuffer->range_length() - mConfig->inputBufferUsedLength);
		   if (mInputBuffer->range_length() <= 3) {
			   mInputBuffer->release();
			   mInputBuffer = NULL;
		   }
	   }

	//if the input data no enough,will drop this frame inputdata. get the next frame data.
	if(decoderErr != MP4AUDEC_SUCCESS)
	{

		 if(mInputBuffer)
		{
		   mInputBuffer->release();
		   mInputBuffer = NULL;
		}

		 if(buffer)
		{
			buffer->release();
			buffer = NULL;
		}
		 goto repeat;

	}

    /*
     * AAC+/eAAC+ streams can be signalled in two ways: either explicitly
     * or implicitly, according to MPEG4 spec. AAC+/eAAC+ is a dual
     * rate system and the sampling rate in the final output is actually
     * doubled compared with the core AAC decoder sampling rate.
     *
     * Explicit signalling is done by explicitly defining SBR audio object
     * type in the bitstream. Implicit signalling is done by embedding
     * SBR content in AAC extension payload specific to SBR, and hence
     * requires an AAC decoder to perform pre-checks on actual audio frames.
     *
     * Thus, we could not say for sure whether a stream is
     * AAC+/eAAC+ until the first data frame is decoded.
     */
    if (++mNumDecodedBuffers <= 2) {
        ALOGV("audio/extended audio object type: %d + %d",
            mConfig->audioObjectType, mConfig->extendedAudioObjectType);
        ALOGV("aac+ upsampling factor: %d desired channels: %d",
            mConfig->aacPlusUpsamplingFactor, mConfig->desiredChannels);

        CHECK(mNumDecodedBuffers > 0);
        if (mNumDecodedBuffers == 1) {
            mUpsamplingFactor = mConfig->aacPlusUpsamplingFactor;
            // Check on the sampling rate to see whether it is changed.
            int32_t sampleRate;
            CHECK(mMeta->findInt32(kKeySampleRate, &sampleRate));
			ALOGV("--->aac samplerae %d",sampleRate);
            if (mConfig->samplingRate != sampleRate) {
                mMeta->setInt32(kKeySampleRate, mConfig->samplingRate);
                ALOGV("Sample rate was %d Hz, but now is %d Hz",
                        sampleRate, mConfig->samplingRate);
                buffer->release();
               // mInputBuffer->release();
               // mInputBuffer = NULL;
                return INFO_FORMAT_CHANGED;
            }
        } else {  // mNumDecodedBuffers == 2
            if (mConfig->extendedAudioObjectType == MP4AUDIO_AAC_LC ||
                mConfig->extendedAudioObjectType == MP4AUDIO_LTP) {
                if (mUpsamplingFactor == 2) {
                    // The stream turns out to be not aacPlus mode anyway
                    ALOGV("Disable AAC+/eAAC+ since extended audio object type is %d",
                        mConfig->extendedAudioObjectType);
                    mConfig->aacPlusEnabled = 0;
                }
            } else {
                if (mUpsamplingFactor == 1) {
                    // aacPlus mode does not buy us anything, but to cause
                    // 1. CPU load to increase, and
                    // 2. a half speed of decoding
                    ALOGV("Disable AAC+/eAAC+ since upsampling factor is 1");
                    mConfig->aacPlusEnabled = 0;
                }
            }
        }
    }

    size_t numOutBytes =
        mConfig->frameLength * sizeof(int16_t) * mConfig->desiredChannels;
    if (mUpsamplingFactor == 2) {
        if (mConfig->desiredChannels == 1) {
            memcpy(&mConfig->pOutputBuffer[1024], &mConfig->pOutputBuffer[2048], numOutBytes * 2);
        }
        numOutBytes *= 2;
    }

    if (decoderErr != MP4AUDEC_SUCCESS) {
        ALOGW("AAC decoder returned error %d, substituting silence", decoderErr);

        memset(buffer->data(), 0, numOutBytes);

        // Discard input buffer.
        if(mInputBuffer)
        {
        mInputBuffer->release();
        mInputBuffer = NULL;
        }
        // fall through
    }

    buffer->set_range(0, numOutBytes);



    buffer->meta_data()->setInt64(
            kKeyTime,
            mAnchorTimeUs
                + (mNumSamplesOutput * 1000000) / mConfig->samplingRate);

    mNumSamplesOutput += mConfig->frameLength;

    *out = buffer;

    return OK;
}

}  // namespace android
