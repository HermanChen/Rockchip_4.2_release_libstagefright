/*
 * Copyright (C) 2011 The Android Open Source Project
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
#define LOG_TAG "SoftMP3"
#include <utils/Log.h>

#include "SoftMP3.h"

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaDefs.h>

#include "include/pvmp3decoder_api.h"

namespace android {

template<class T>
static void InitOMXParams(T *params) {
    params->nSize = sizeof(T);
    params->nVersion.s.nVersionMajor = 1;
    params->nVersion.s.nVersionMinor = 0;
    params->nVersion.s.nRevision = 0;
    params->nVersion.s.nStep = 0;
}

SoftMP3::SoftMP3(
        const char *name,
        const OMX_CALLBACKTYPE *callbacks,
        OMX_PTR appData,
        OMX_COMPONENTTYPE **component)
    : SimpleSoftOMXComponent(name, callbacks, appData, component),
      mConfig(new tPVMP3DecoderExternal),
      mDecoderBuf(NULL),
      mAnchorTimeUs(0),
      mNumFramesOutput(0),
      mNumChannels(2),
      mSamplingRate(44100),
      mSignalledError(false),
      mOutputPortSettingsChange(NONE) {
    initPorts();
    initDecoder();
}

SoftMP3::~SoftMP3() {
    if (mDecoderBuf != NULL) {
        free(mDecoderBuf);
        mDecoderBuf = NULL;
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

void SoftMP3::initPorts() {
    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOMXParams(&def);

    def.nPortIndex = 0;
    def.eDir = OMX_DirInput;
    def.nBufferCountMin = kNumBuffers;
    def.nBufferCountActual = def.nBufferCountMin;
    def.nBufferSize = 100*1024;
    def.bEnabled = OMX_TRUE;
    def.bPopulated = OMX_FALSE;
    def.eDomain = OMX_PortDomainAudio;
    def.bBuffersContiguous = OMX_FALSE;
    def.nBufferAlignment = 1;

    def.format.audio.cMIMEType =
        const_cast<char *>(MEDIA_MIMETYPE_AUDIO_MPEG);

    def.format.audio.pNativeRender = NULL;
    def.format.audio.bFlagErrorConcealment = OMX_FALSE;
    def.format.audio.eEncoding = OMX_AUDIO_CodingMP3;

    addPort(def);

    def.nPortIndex = 1;
    def.eDir = OMX_DirOutput;
    def.nBufferCountMin = kNumBuffers;
    def.nBufferCountActual = def.nBufferCountMin;
    def.nBufferSize = kOutputBufferSize;
    def.bEnabled = OMX_TRUE;
    def.bPopulated = OMX_FALSE;
    def.eDomain = OMX_PortDomainAudio;
    def.bBuffersContiguous = OMX_FALSE;
    def.nBufferAlignment = 2;

    def.format.audio.cMIMEType = const_cast<char *>("audio/raw");
    def.format.audio.pNativeRender = NULL;
    def.format.audio.bFlagErrorConcealment = OMX_FALSE;
    def.format.audio.eEncoding = OMX_AUDIO_CodingPCM;

    addPort(def);
}

void SoftMP3::initDecoder() {
    mConfig->equalizerType = flat;
    mConfig->crcEnabled = false;

    uint32_t memRequirements = pvmp3_decoderMemRequirements();
    mDecoderBuf = malloc(memRequirements);

    pvmp3_InitDecoder(mConfig, mDecoderBuf);
    mIsFirst = true;
	
#if SUPPORT_NO_ENOUGH_MAIN_DATA
	memset((void *)&mBufExt,0,sizeof(mBufExt));
	mIsFormatChange = false;
	mFrameCount = 0;
	mDropFrameCount = 0;
#endif	
}

OMX_ERRORTYPE SoftMP3::internalGetParameter(
        OMX_INDEXTYPE index, OMX_PTR params) {
    switch (index) {
        case OMX_IndexParamAudioPcm:
        {
            OMX_AUDIO_PARAM_PCMMODETYPE *pcmParams =
                (OMX_AUDIO_PARAM_PCMMODETYPE *)params;

            if (pcmParams->nPortIndex > 1) {
                return OMX_ErrorUndefined;
            }

            pcmParams->eNumData = OMX_NumericalDataSigned;
            pcmParams->eEndian = OMX_EndianBig;
            pcmParams->bInterleaved = OMX_TRUE;
            pcmParams->nBitPerSample = 16;
            pcmParams->ePCMMode = OMX_AUDIO_PCMModeLinear;
            pcmParams->eChannelMapping[0] = OMX_AUDIO_ChannelLF;
            pcmParams->eChannelMapping[1] = OMX_AUDIO_ChannelRF;

            pcmParams->nChannels = mNumChannels;
            pcmParams->nSamplingRate = mSamplingRate;

            return OMX_ErrorNone;
        }

        default:
            return SimpleSoftOMXComponent::internalGetParameter(index, params);
    }
}

OMX_ERRORTYPE SoftMP3::internalSetParameter(
        OMX_INDEXTYPE index, const OMX_PTR params) {
    switch (index) {
        case OMX_IndexParamStandardComponentRole:
        {
            const OMX_PARAM_COMPONENTROLETYPE *roleParams =
                (const OMX_PARAM_COMPONENTROLETYPE *)params;

            if (strncmp((const char *)roleParams->cRole,
                        "audio_decoder.mp3",
                        OMX_MAX_STRINGNAME_SIZE - 1)) {
                return OMX_ErrorUndefined;
            }

            return OMX_ErrorNone;
        }

        default:
            return SimpleSoftOMXComponent::internalSetParameter(index, params);
    }
}

void SoftMP3::onQueueFilled(OMX_U32 portIndex) {
    if (mSignalledError || mOutputPortSettingsChange != NONE) {
        return;
    }

    List<BufferInfo *> &inQueue = getPortQueue(0);
    List<BufferInfo *> &outQueue = getPortQueue(1);

    while (!inQueue.empty() && !outQueue.empty()) {
        BufferInfo *inInfo = *inQueue.begin();
        OMX_BUFFERHEADERTYPE *inHeader = inInfo->mHeader;

        BufferInfo *outInfo = *outQueue.begin();
        OMX_BUFFERHEADERTYPE *outHeader = outInfo->mHeader;

        if (inHeader->nFlags & OMX_BUFFERFLAG_EOS) {
            inQueue.erase(inQueue.begin());
            inInfo->mOwnedByUs = false;
            notifyEmptyBufferDone(inHeader);

            // pad the end of the stream with 529 samples, since that many samples
            // were trimmed off the beginning when decoding started
            outHeader->nFilledLen = kPVMP3DecoderDelay * mNumChannels * sizeof(int16_t);
            memset(outHeader->pBuffer, 0, outHeader->nFilledLen);
            outHeader->nFlags = OMX_BUFFERFLAG_EOS;

            outQueue.erase(outQueue.begin());
            outInfo->mOwnedByUs = false;
            notifyFillBufferDone(outHeader);
            return;
        }

       
#if SUPPORT_NO_ENOUGH_MAIN_DATA
			if(mIsFirst)
			{
				 mAnchorTimeUs = inHeader->nTimeStamp;
           		 mNumFramesOutput = 0;
			}
			
			if(mBufExt.buf)//if buf != NULL ,the input buf always be mBufExt.buf.
			{
				uint32_t copyLen = 0;
				if((TMP_BUF_SIZE -mBufExt.len) >=  inHeader->nFilledLen)
				{
					copyLen = inHeader->nFilledLen;
				}
				else
				{
					copyLen = TMP_BUF_SIZE -mBufExt.len;
				}
		
				uint8 * src =  (uint8_t *)inHeader->pBuffer + inHeader->nOffset;;
				memcpy(mBufExt.buf + mBufExt.len,src,copyLen);
				mBufExt.len += copyLen;
		
				if(copyLen == inHeader->nFilledLen)
				{
					inInfo->mOwnedByUs = false;
		            inQueue.erase(inQueue.begin());
		            inInfo = NULL;
		            notifyEmptyBufferDone(inHeader);
		            inHeader = NULL;
				}
				else
				{
					inHeader->nOffset += copyLen;
					inHeader->nFilledLen -= copyLen;
				}
		
				mConfig->pInputBuffer = mBufExt.buf;
				mConfig->inputBufferCurrentLength = mBufExt.len;
			}
			else
			{
				mConfig->pInputBuffer =
           		 inHeader->pBuffer + inHeader->nOffset;

       			 mConfig->inputBufferCurrentLength = inHeader->nFilledLen;
			}
#else
		if (inHeader->nOffset == 0) {
            mAnchorTimeUs = inHeader->nTimeStamp;
            mNumFramesOutput = 0;
        }
		mConfig->pInputBuffer =
            inHeader->pBuffer + inHeader->nOffset;

        mConfig->inputBufferCurrentLength = inHeader->nFilledLen;
#endif


        mConfig->inputBufferMaxLength = 0;
        mConfig->inputBufferUsedLength = 0;

        mConfig->outputFrameSize = kOutputBufferSize / sizeof(int16_t);

        mConfig->pOutputBuffer =
            reinterpret_cast<int16_t *>(outHeader->pBuffer);

        ERROR_CODE decoderErr;
        if ((decoderErr = pvmp3_framedecoder(mConfig, mDecoderBuf))
                != NO_DECODING_ERROR) {
            ALOGV("mp3 decoder returned error %d", decoderErr);
#if SUPPORT_NO_ENOUGH_MAIN_DATA
			if(mIsFirst)
				mDropFrameCount = 4;
			if(mConfig->inputBufferUsedLength > mConfig->inputBufferCurrentLength)
				mConfig->inputBufferUsedLength = mConfig->inputBufferCurrentLength;

	        if (decoderErr != NO_ENOUGH_MAIN_DATA_ERROR) {

				mFrameCount++;
				ALOGE("--->error------>decoderErrr %d mFrameCount%d inputlen %d usedlen %d",decoderErr,mFrameCount,mConfig->inputBufferCurrentLength,mConfig->inputBufferUsedLength);

				if(mFrameCount < 10)
				{
					mConfig->outputFrameSize  = 0;
					
					if(mBufExt.buf)
					{
						mBufExt.len -= mConfig->inputBufferUsedLength;
						memcpy(mBufExt.buf,mBufExt.buf+ mConfig->inputBufferUsedLength,mBufExt.len);
					}
					else
					{
						if(inHeader)
						{
							inHeader->nOffset += mConfig->inputBufferUsedLength;
							inHeader->nFilledLen -= mConfig->inputBufferUsedLength;
							
							if(inHeader->nFilledLen == 0)
							{
								inInfo->mOwnedByUs = false;
					            inQueue.erase(inQueue.begin());
					            inInfo = NULL;
					            notifyEmptyBufferDone(inHeader);
					            inHeader = NULL;
							}
						}
					}

				}
				else
				{
					if(inHeader)
					{
						inInfo->mOwnedByUs = false;
			            inQueue.erase(inQueue.begin());
			            inInfo = NULL;
			            notifyEmptyBufferDone(inHeader);
			            inHeader = NULL;
					}
					
					if(mBufExt.buf)
					{
						mBufExt.len = 0;
					}
					mConfig->outputFrameSize  = 0;
					mFrameCount = 0;
				}

				continue;
	        }
			
			mConfig->inputBufferUsedLength = 0;
			if(mBufExt.buf == NULL)
			{
				mBufExt.buf = (uint8*)malloc(TMP_BUF_SIZE);
				mBufExt.len = 0;
			}

			if(mBufExt.len == TMP_BUF_SIZE)
			{
				ALOGE("1024 bytes has err = NO_ENOUGH_MAIN_DATA_ERROR ");
                notify(OMX_EventError, OMX_ErrorUndefined, decoderErr, NULL);
                mSignalledError = true;
                return;
			}
			else
			{
				if(mBufExt.len == 0 && inHeader)
				{
					uint8 * src =  (uint8_t *)inHeader->pBuffer + inHeader->nOffset;
					mBufExt.len = inHeader->nFilledLen;
					ALOGE("first used len %d used len %d cur len %d mConfig->samplingRate %d src %2x%2x%2x%2x",
						mBufExt.len,mConfig->inputBufferUsedLength,mConfig->inputBufferCurrentLength,mConfig->samplingRate,src[0],src[1],src[2],src[3]);
					memcpy(mBufExt.buf,src,mBufExt.len);
					inInfo->mOwnedByUs = false;
		            inQueue.erase(inQueue.begin());
		            inInfo = NULL;
		            notifyEmptyBufferDone(inHeader);
		            inHeader = NULL;
				}
			}
			continue;
#else
			if (decoderErr != NO_ENOUGH_MAIN_DATA_ERROR
                        && decoderErr != SIDE_INFO_ERROR) {
                ALOGE("mp3 decoder returned error %d", decoderErr);

                notify(OMX_EventError, OMX_ErrorUndefined, decoderErr, NULL);
                mSignalledError = true;
                return;
            }

            if (mConfig->outputFrameSize == 0) {
                mConfig->outputFrameSize = kOutputBufferSize / sizeof(int16_t);
            }

            // This is recoverable, just ignore the current frame and
            // play silence instead.
            memset(outHeader->pBuffer,
                   0,
                   mConfig->outputFrameSize * sizeof(int16_t));

            mConfig->inputBufferUsedLength = inHeader->nFilledLen;

#endif      
        }
#if SUPPORT_NO_ENOUGH_MAIN_DATA
		else if (!mIsFormatChange && (mConfig->samplingRate != mSamplingRate
                || mConfig->num_channels != mNumChannels)) {
            mIsFormatChange = true;
#else
		else if (mConfig->samplingRate != mSamplingRate
                || mConfig->num_channels != mNumChannels) {
#endif
			mSamplingRate = mConfig->samplingRate;
            mNumChannels = mConfig->num_channels;

            notify(OMX_EventPortSettingsChanged, 1, 0, NULL);
            mOutputPortSettingsChange = AWAITING_DISABLED;
            return;
        }

        if (mIsFirst) {
            mIsFirst = false;
			if(!mIsFormatChange)
				mIsFormatChange = true;
            // The decoder delay is 529 samples, so trim that many samples off
            // the start of the first output buffer. This essentially makes this
            // decoder have zero delay, which the rest of the pipeline assumes.
            outHeader->nOffset = kPVMP3DecoderDelay * mNumChannels * sizeof(int16_t);

            if (mConfig->outputFrameSize * sizeof(int16_t) >=outHeader->nOffset) {
                outHeader->nFilledLen =
                    mConfig->outputFrameSize * sizeof(int16_t) - outHeader->nOffset;
            } else {
                outHeader->nFilledLen = 0;
            }

        } else {
            outHeader->nOffset = 0;
            outHeader->nFilledLen = mConfig->outputFrameSize * sizeof(int16_t);
        }
		if(mConfig && mConfig->samplingRate)
	        outHeader->nTimeStamp =
	            mAnchorTimeUs
	                + (mNumFramesOutput * 1000000ll) / mConfig->samplingRate;
		else
			 outHeader->nTimeStamp = mAnchorTimeUs;

        outHeader->nFlags = 0;
#if SUPPORT_NO_ENOUGH_MAIN_DATA
		if(mDropFrameCount > 0)
		{
			mDropFrameCount--;
			memset(outHeader->pBuffer + outHeader->nOffset,0,outHeader->nFilledLen);
		}
		if(mBufExt.len)
		{
			mBufExt.len -= mConfig->inputBufferUsedLength;
			memcpy(mBufExt.buf,mBufExt.buf+ mConfig->inputBufferUsedLength,mBufExt.len);
			
			if(inHeader)
			{
				uint32_t copyLen = 0;
				if((TMP_BUF_SIZE -mBufExt.len) >=  inHeader->nFilledLen)
				{
					copyLen = inHeader->nFilledLen;
				}
				else
				{
					copyLen = TMP_BUF_SIZE -mBufExt.len;
				}
	
				uint8 * src =  (uint8_t *)inHeader->pBuffer + inHeader->nOffset;
				memcpy(mBufExt.buf + mBufExt.len,src,copyLen);
				mBufExt.len += copyLen;
				if(copyLen == inHeader->nFilledLen)
				{
					inInfo->mOwnedByUs = false;
		            inQueue.erase(inQueue.begin());
		            inInfo = NULL;
		            notifyEmptyBufferDone(inHeader);
		            inHeader = NULL;
				}
				else
				{
					inHeader->nOffset += copyLen;
					inHeader->nFilledLen -= copyLen;
				}
	
			}
		}
		else
		{
			inHeader->nOffset += mConfig->inputBufferUsedLength;
	        inHeader->nFilledLen -= mConfig->inputBufferUsedLength;

	        if (inHeader->nFilledLen == 0) {
	            inInfo->mOwnedByUs = false;
	            inQueue.erase(inQueue.begin());
	            inInfo = NULL;
	            notifyEmptyBufferDone(inHeader);
	            inHeader = NULL;
	        }
		}
#else
		 CHECK_GE(inHeader->nFilledLen, mConfig->inputBufferUsedLength);

        inHeader->nOffset += mConfig->inputBufferUsedLength;
        inHeader->nFilledLen -= mConfig->inputBufferUsedLength;

        if (inHeader->nFilledLen == 0) {
            inInfo->mOwnedByUs = false;
            inQueue.erase(inQueue.begin());
            inInfo = NULL;
            notifyEmptyBufferDone(inHeader);
            inHeader = NULL;
        }
#endif

		if(mNumChannels)
			mNumFramesOutput += mConfig->outputFrameSize / mNumChannels;

        outInfo->mOwnedByUs = false;
        outQueue.erase(outQueue.begin());
        outInfo = NULL;
        notifyFillBufferDone(outHeader);
        outHeader = NULL;
    }
}

void SoftMP3::onPortFlushCompleted(OMX_U32 portIndex) {
    if (portIndex == 0) {
        // Make sure that the next buffer output does not still
        // depend on fragments from the last one decoded.
        pvmp3_InitDecoder(mConfig, mDecoderBuf);
        mIsFirst = true;

#if SUPPORT_NO_ENOUGH_MAIN_DATA
//when the audio do seek ,the len must be reset;
		if(mBufExt.buf)
		{
			mBufExt.len = 0;
		}
#endif
    }
}

void SoftMP3::onPortEnableCompleted(OMX_U32 portIndex, bool enabled) {
    if (portIndex != 1) {
        return;
    }

    switch (mOutputPortSettingsChange) {
        case NONE:
            break;

        case AWAITING_DISABLED:
        {
            CHECK(!enabled);
            mOutputPortSettingsChange = AWAITING_ENABLED;
            break;
        }

        default:
        {
            CHECK_EQ((int)mOutputPortSettingsChange, (int)AWAITING_ENABLED);
            CHECK(enabled);
            mOutputPortSettingsChange = NONE;
            break;
        }
    }
}

}  // namespace android

android::SoftOMXComponent *createSoftOMXComponent(
        const char *name, const OMX_CALLBACKTYPE *callbacks,
        OMX_PTR appData, OMX_COMPONENTTYPE **component) {
    return new android::SoftMP3(name, callbacks, appData, component);
}
