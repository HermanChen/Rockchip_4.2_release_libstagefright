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
#define LOG_TAG "SoftAAC"
#include <utils/Log.h>

#include "SoftAAC.h"

#include "pvmp4audiodecoder_api.h"

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/hexdump.h>
#include <media/stagefright/MediaErrors.h>
#include <dlfcn.h>
#define WRITE_FILE 0
#if WRITE_FILE
static FILE * aacFp = NULL;
#endif

namespace android {

template<class T>
static void InitOMXParams(T *params) {
    params->nSize = sizeof(T);
    params->nVersion.s.nVersionMajor = 1;
    params->nVersion.s.nVersionMinor = 0;
    params->nVersion.s.nRevision = 0;
    params->nVersion.s.nStep = 0;
}
#define  WIFI_DISPLAY_ENABLE	1
SoftAAC::SoftAAC(
        const char *name,
        const OMX_CALLBACKTYPE *callbacks,
        OMX_PTR appData,
        OMX_COMPONENTTYPE **component)
    : SimpleSoftOMXComponent(name, callbacks, appData, component),
      mConfig(new tPVMP4AudioDecoderExternal),
      mIsADTS(false),
      mDecoderBuf(NULL),
      mInputBufferCount(0),
      mUpsamplingFactor(2),
      mAnchorTimeUs(0),
      mPreTimeUs(0),
      mNumSamplesOutput(0),
      mUseFaadDecoder(0),
      hAac(NULL),
      conf(NULL),
      mSignalledError(false),
      mOutputPortSettingsChange(NONE),
      wifidisplay_flag(0),
      wifidisplay_addr(0){
    initPorts();
#if WIFI_DISPLAY_ENABLE
	widi_handle = NULL;
	mStreamSource = NULL;
	omx_rs_txt = NULL;
	widi_handle = dlopen("libmediaplayerservice.so", RTLD_LAZY | RTLD_LOCAL);
	if(widi_handle == NULL)
	{
		ALOGE("libmediaplayerservice can't be loaded");
	}
	else
		mStreamSource = (int (*)(void* ptr,int64_t *start_time,int64_t *audio_start_time))::dlsym(widi_handle, "Wifidisplay_get_Time");
	if(mStreamSource==NULL)
	{
		ALOGE("StreamingSource don't exit");
	}
	if(widi_handle!=NULL)
		dlclose(widi_handle);
	start_time = 0;
	start_timeUs = 0;
	wifi_start_time = 0;
	last_timeUs = 0;
	last_adujst_time = 0;
#endif
    CHECK_EQ(initDecoder(), (status_t)OK);
#if WRITE_FILE
    ALOGE("create file: /sdcard/aac.dat");
    aacFp = fopen("/sdcard/aac.dat","wb");
    if(aacFp)
        ALOGE("------>>create file success");
    else
        ALOGE("------>>create file fail");
#endif
}

SoftAAC::~SoftAAC() {
    free(mDecoderBuf);
    mDecoderBuf = NULL;
#if WIFI_DISPLAY_ENABLE
	if(omx_rs_txt != NULL)
	{
		fclose(omx_rs_txt);
		omx_rs_txt = NULL;
	}
	mStreamSource = NULL;
#endif
    delete mConfig;
    mConfig = NULL;
	if(mUseFaadDecoder == 1 && hAac != NULL)
		NeAACDecClose(hAac);
#if WRITE_FILE
    if(aacFp) {
        fclose(aacFp);
        aacFp = NULL;
    }
#endif
}

void SoftAAC::initPorts() {
    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOMXParams(&def);

    def.nPortIndex = 0;
    def.eDir = OMX_DirInput;
    def.nBufferCountMin = kNumInputBuffers;
    def.nBufferCountActual = def.nBufferCountMin;
    def.nBufferSize = 100*1024;
    def.bEnabled = OMX_TRUE;
    def.bPopulated = OMX_FALSE;
    def.eDomain = OMX_PortDomainAudio;
    def.bBuffersContiguous = OMX_FALSE;
    def.nBufferAlignment = 1;

    def.format.audio.cMIMEType = const_cast<char *>("audio/aac");
    def.format.audio.pNativeRender = NULL;
    def.format.audio.bFlagErrorConcealment = OMX_FALSE;
    def.format.audio.eEncoding = OMX_AUDIO_CodingAAC;

    addPort(def);

    def.nPortIndex = 1;
    def.eDir = OMX_DirOutput;
    def.nBufferCountMin = kNumOutputBuffers;
    def.nBufferCountActual = def.nBufferCountMin;
    def.nBufferSize = 100*1024;
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

status_t SoftAAC::initDecoder() {
    memset(mConfig, 0, sizeof(tPVMP4AudioDecoderExternal));
    mConfig->outputFormat = OUTPUTFORMAT_16PCM_INTERLEAVED;
    mConfig->aacPlusEnabled = 1;

    // The software decoder doesn't properly support mono output on
    // AACplus files. Always output stereo.
    mConfig->desiredChannels = 2;

    UInt32 memRequirements = PVMP4AudioDecoderGetMemRequirements();
    mDecoderBuf = malloc(memRequirements);

    Int err = PVMP4AudioDecoderInitLibrary(mConfig, mDecoderBuf);
    if (err != MP4AUDEC_SUCCESS) {
        ALOGE("Failed to initialize MP4 audio decoder");
        return UNKNOWN_ERROR;
    }

    return OK;
}

OMX_ERRORTYPE SoftAAC::internalGetParameter(
        OMX_INDEXTYPE index, OMX_PTR params) {
    switch (index) {
        case OMX_IndexParamAudioAac:
        {
            OMX_AUDIO_PARAM_AACPROFILETYPE *aacParams =
                (OMX_AUDIO_PARAM_AACPROFILETYPE *)params;

            if (aacParams->nPortIndex != 0) {
                return OMX_ErrorUndefined;
            }

            aacParams->nBitRate = 0;
            aacParams->nAudioBandWidth = 0;
            aacParams->nAACtools = 0;
            aacParams->nAACERtools = 0;
            aacParams->eAACProfile = OMX_AUDIO_AACObjectMain;

            aacParams->eAACStreamFormat =
                mIsADTS
                    ? OMX_AUDIO_AACStreamFormatMP4ADTS
                    : OMX_AUDIO_AACStreamFormatMP4FF;

            aacParams->eChannelMode = OMX_AUDIO_ChannelModeStereo;

            if (!isConfigured()) {
                aacParams->nChannels = 1;
                aacParams->nSampleRate = 44100;
                aacParams->nFrameLength = 0;
            } else {
                aacParams->nChannels = mConfig->encodedChannels;
                aacParams->nSampleRate = mConfig->samplingRate;
                aacParams->nFrameLength = mConfig->frameLength;
            }

            return OMX_ErrorNone;
        }

        case OMX_IndexParamAudioPcm:
        {
            OMX_AUDIO_PARAM_PCMMODETYPE *pcmParams =
                (OMX_AUDIO_PARAM_PCMMODETYPE *)params;

            if (pcmParams->nPortIndex != 1) {
                return OMX_ErrorUndefined;
            }

            pcmParams->eNumData = OMX_NumericalDataSigned;
            pcmParams->eEndian = OMX_EndianBig;
            pcmParams->bInterleaved = OMX_TRUE;
            pcmParams->nBitPerSample = 16;
            pcmParams->ePCMMode = OMX_AUDIO_PCMModeLinear;
            pcmParams->eChannelMapping[0] = OMX_AUDIO_ChannelLF;
            pcmParams->eChannelMapping[1] = OMX_AUDIO_ChannelRF;

            if (!isConfigured()) {
                pcmParams->nChannels = 1;
                pcmParams->nSamplingRate = 44100;
            } else {
                pcmParams->nChannels = mConfig->desiredChannels;
                pcmParams->nSamplingRate = mConfig->samplingRate;
            }

            return OMX_ErrorNone;
        }

        default:
            return SimpleSoftOMXComponent::internalGetParameter(index, params);
    }
}

OMX_ERRORTYPE SoftAAC::internalSetParameter(
        OMX_INDEXTYPE index, const OMX_PTR params) {
    switch (index) {
        case OMX_IndexParamStandardComponentRole:
        {
            const OMX_PARAM_COMPONENTROLETYPE *roleParams =
                (const OMX_PARAM_COMPONENTROLETYPE *)params;

            if (strncmp((const char *)roleParams->cRole,
                        "audio_decoder.aac",
                        OMX_MAX_STRINGNAME_SIZE - 1)) {
                return OMX_ErrorUndefined;
            }

            return OMX_ErrorNone;
        }

        case OMX_IndexParamAudioAac:
        {
            const OMX_AUDIO_PARAM_AACPROFILETYPE *aacParams =
                (const OMX_AUDIO_PARAM_AACPROFILETYPE *)params;

            if (aacParams->nPortIndex != 0) {
                return OMX_ErrorUndefined;
            }

            if (aacParams->eAACStreamFormat == OMX_AUDIO_AACStreamFormatMP4FF) {
                mIsADTS = false;
            } else if (aacParams->eAACStreamFormat
                        == OMX_AUDIO_AACStreamFormatMP4ADTS) {
                mIsADTS = true;
            } else {
                return OMX_ErrorUndefined;
            }

            return OMX_ErrorNone;
        }

        case OMX_IndexParamAudioPcm:
        {
            const OMX_AUDIO_PARAM_PCMMODETYPE *pcmParams =
                (OMX_AUDIO_PARAM_PCMMODETYPE *)params;

            if (pcmParams->nPortIndex != 1) {
                return OMX_ErrorUndefined;
            }

            return OMX_ErrorNone;
        }
        case OMX_IndexConfigOtherStats:
		{
			const OMX_AUDIO_PARAM_AACPROFILETYPE *aacParams =
                (OMX_AUDIO_PARAM_AACPROFILETYPE *)params;

			wifidisplay_flag = aacParams->nChannels;
			wifidisplay_addr = aacParams->nSampleRate;
			ALOGD("SoftAAC OMX_IndexParamAudioG723 wifidisplay_flag %d wifidisplay_addr %x",wifidisplay_flag,wifidisplay_addr);
			if(wifidisplay_flag == 0 || wifidisplay_addr == 0)
				return OMX_ErrorUndefined;
			else
        	    return OMX_ErrorNone;
		}
        default:
            return SimpleSoftOMXComponent::internalSetParameter(index, params);
    }
}

bool SoftAAC::isConfigured() const {
    return mInputBufferCount > 0;
}

void SoftAAC::onQueueFilled(OMX_U32 portIndex) {
    if (mSignalledError || mOutputPortSettingsChange != NONE) {
        return;
    }

	int64_t timeUs;
	int		set_flag = 0;
    List<BufferInfo *> &inQueue = getPortQueue(0);
    List<BufferInfo *> &outQueue = getPortQueue(1);

    if (portIndex == 0 && mInputBufferCount == 0) {
        ++mInputBufferCount;

        BufferInfo *info = *inQueue.begin();
        OMX_BUFFERHEADERTYPE *header = info->mHeader;

        mConfig->pInputBuffer = header->pBuffer + header->nOffset;
        mConfig->inputBufferCurrentLength = header->nFilledLen;
        mConfig->inputBufferMaxLength = 0;

#if WRITE_FILE
        if(aacFp) {
            fwrite(&mConfig->inputBufferCurrentLength, 1, 4, aacFp);
            fwrite(mConfig->pInputBuffer,1, mConfig->inputBufferCurrentLength,aacFp);
        }
#endif
        Int err = PVMP4AudioDecoderConfig(mConfig, mDecoderBuf);
        if (err != MP4AUDEC_SUCCESS) {
            ALOGE("PVMP4AudioDecoderConfig failed. err = %x", err);
			mUseFaadDecoder = 1; // support FAAC Decoder, 0: no, 1: yes
			if(mUseFaadDecoder == 1)
	        {
		        ALOGE("Try Faad Decoder.\n");
				unsigned long cap = NeAACDecGetCapabilities();
				hAac = NeAACDecOpen();
				conf = NeAACDecGetCurrentConfiguration(hAac);
				NeAACDecSetConfiguration(hAac, conf);// Initialise the library using one of the initialization functions
			    unsigned long samplerate;
			    unsigned char channels;
				err = NeAACDecInit2(hAac, header->pBuffer + header->nOffset, header->nFilledLen, &samplerate, &channels);
				if (err != 0)
				{
					ALOGE("NeAACDecInit2 failed: err = %d.\n", err);
					NeAACDecClose(hAac);
					hAac = NULL;
            mSignalledError = true;
            notify(OMX_EventError, OMX_ErrorUndefined, err, NULL);
            return;
				}
				mConfig->encodedChannels = channels;
				mConfig->samplingRate = samplerate;
				mConfig->frameLength = 2048;
				mConfig->desiredChannels = channels;
				ALOGI("samplerate = %d, channels = %d", samplerate, channels);
			}else {
	            mSignalledError = true;
	            notify(OMX_EventError, OMX_ErrorUndefined, err, NULL);
	            return;
			}
        }

        inQueue.erase(inQueue.begin());
        info->mOwnedByUs = false;
        notifyEmptyBufferDone(header);

        notify(OMX_EventPortSettingsChanged, 1, 0, NULL);
        mOutputPortSettingsChange = AWAITING_DISABLED;
        return;
    }

    while (!inQueue.empty() && !outQueue.empty()) {
        BufferInfo *inInfo = *inQueue.begin();
        OMX_BUFFERHEADERTYPE *inHeader = inInfo->mHeader;

        BufferInfo *outInfo = *outQueue.begin();
        OMX_BUFFERHEADERTYPE *outHeader = outInfo->mHeader;

        if (inHeader->nFlags & OMX_BUFFERFLAG_EOS) {
            inQueue.erase(inQueue.begin());
            inInfo->mOwnedByUs = false;
            notifyEmptyBufferDone(inHeader);

            outHeader->nFilledLen = 0;
            outHeader->nFlags = OMX_BUFFERFLAG_EOS;

            outQueue.erase(outQueue.begin());
            outInfo->mOwnedByUs = false;
            notifyFillBufferDone(outHeader);
            return;
        }
#if WIFI_DISPLAY_ENABLE
		if(mStreamSource != NULL && wifidisplay_flag == 1)
		{
			int64_t sys_start_time;
			int64_t aac_start_time;
			int64_t sys_time;
			int	status;
			int64_t wifi_time,wifi_sys_time;
			wifi_time = wifi_sys_time = 0;
			timeUs = inHeader->nTimeStamp;
			status = mStreamSource((void *)wifidisplay_addr,&wifi_sys_time,&wifi_time);
			
			if(wifi_sys_time == 0 && wifi_time == 0)
			{
				if(omx_rs_txt != NULL)
					
				{
				
					fprintf(omx_rs_txt,"SoftAAC normal dec delay 300 ms start %lld  %lld  %lld sys %lld timeUs %lld last %lld delta %lld frame_interval %lld\n"
					,sys_start_time,start_time,aac_start_time,sys_time,timeUs,last_timeUs,sys_time-sys_start_time-(timeUs - aac_start_time),frame_interval);
					fflush(omx_rs_txt); 		
									
				}
				sys_start_time 	= 	start_time;
				aac_start_time 	= 	start_timeUs;
				last_timeUs 	= 	inHeader->nTimeStamp;
				if(status == -1)
					ALOGD("Error : Streamingsource can't match softaac decoder");
			}
			else
			{
				wifi_start_time = wifi_sys_time;
				wifi_audio_start_time = wifi_time;
				if(mConfig->samplingRate == 0)
				{
					ALOGD("mConfig->samplingRate %d",mConfig->samplingRate);
					return;
				}
				frame_interval = (1024 * 1000 )/ mConfig->samplingRate ;
				if(frame_interval == 0 || mConfig->samplingRate == 0)
				{
					return;
				}
				if(start_timeUs == 0)
					start_timeUs = inHeader->nTimeStamp;
				sys_time = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;	
					
				if(start_time == 0)
				{
					start_time = sys_time -(inHeader->nTimeStamp - start_timeUs);
					last_adujst_time = sys_time;
				}
				if(start_time > sys_time -(inHeader->nTimeStamp - start_timeUs))
				{
					start_time = sys_time - (inHeader->nTimeStamp - start_timeUs);
					ALOGV("SOFTAac start time %lld wifi_start_time %d",start_time,wifi_start_time);
				}
				ALOGV("SOFTAac start time %lld wifi_start_time %lld mConfig->samplingRate %d",start_time,wifi_start_time,mConfig->samplingRate);
				sys_start_time =	wifi_start_time;
				aac_start_time = 	wifi_audio_start_time;
			
				int retrtptxt;
				timeUs = inHeader->nTimeStamp;
				if((retrtptxt = access("data/test/omx_rs_txt_file",0)) == 0)
				{
					if(omx_rs_txt == NULL)
						omx_rs_txt = fopen("data/test/omx_rs_txt.txt","ab");
				}	  
				
				{
					if(sys_start_time + (timeUs - aac_start_time) < sys_time - 100000ll )
					{	
						if(last_timeUs < timeUs )//loop tntil the real timeUs catch up with the old setted one  , if there is no data,the old setted is also faster than the real timeUs.so it's okay
						{
							if(sys_start_time + (timeUs - aac_start_time) < sys_time - 300000ll || (sys_time - last_adujst_time > 20000000ll && 
							sys_start_time + (timeUs - aac_start_time) < sys_time - 100000ll  ))//recalculate the timeUs.
							{
								if(omx_rs_txt != NULL)
					
								{
									if(sys_time - last_adujst_time > 20000000ll && 
							sys_start_time + (timeUs - aac_start_time) < sys_time - 100000ll   && sys_time-sys_start_time-(timeUs - aac_start_time) > 100000ll)
									fprintf(omx_rs_txt,"SoftAAC adjust start %lld  %lld  %lld sys %lld %lld timeUs %lld last %lld delta %lld  %lld frame_interval %lld\n"
									,sys_start_time,start_time,aac_start_time,last_adujst_time,sys_time,timeUs,last_timeUs,sys_time-sys_start_time-(timeUs - aac_start_time),
									sys_time-last_adujst_time,frame_interval);
									else
									fprintf(omx_rs_txt,"SoftAAC before dec delay 300 ms start %lld  %lld  %lld sys %lld %lld timeUs %lld last %lld delta %lld  %lld frame_interval %lld\n"
									,sys_start_time,start_time,aac_start_time,last_adujst_time,sys_time,timeUs,last_timeUs,sys_time-sys_start_time-(timeUs - aac_start_time),
									sys_time-last_adujst_time,frame_interval);
									fflush(omx_rs_txt); 		
													
								}
								timeUs +=((sys_time - sys_start_time - (timeUs - aac_start_time) ) / frame_interval) *frame_interval;					
								set_flag = 1;
								last_adujst_time = sys_time;
							}
							
							else
							{
								if(omx_rs_txt != NULL)
					
								{
								
									fprintf(omx_rs_txt,"SoftAAC before dec delay 100-300 ms start %lld  %lld  %lld sys %lld %lld timeUs %lld last %lld delta %lld  %lld frame_interval %lld\n"
									,sys_start_time,start_time,aac_start_time,last_adujst_time,sys_time,timeUs,last_timeUs,sys_time-sys_start_time-(timeUs - aac_start_time),
									sys_time-last_adujst_time,frame_interval);
									fflush(omx_rs_txt); 		
													
								}
								
							}
							
						
						}
						else
						{
							if(omx_rs_txt != NULL)
					
							{
							
								fprintf(omx_rs_txt,"SoftAAC before drop start %lld  %lld  %lld sys %lld %lld timeUs %lld last %lld delta %lld  %lld frame_interval %lld\n"
									,sys_start_time,start_time,aac_start_time,last_adujst_time,sys_time,timeUs,last_timeUs,sys_time-sys_start_time-(timeUs - aac_start_time),
									sys_time-last_adujst_time,frame_interval);
								fflush(omx_rs_txt);			
							}	
							{
					            inInfo->mOwnedByUs = false;
					            inQueue.erase(inQueue.begin());
					            inInfo = NULL;
					            notifyEmptyBufferDone(inHeader);
					            inHeader = NULL;
					        }
							continue;
						}
					}
					else
					{
						if(omx_rs_txt != NULL)
					
							{
							
								fprintf(omx_rs_txt,"SoftAAC before less than 100ms start %lld  %lld  %lld sys %lld %lld timeUs %lld last %lld delta %lld  %lld frame_interval %lld\n"
									,sys_start_time,start_time,aac_start_time,last_adujst_time,sys_time,timeUs,last_timeUs,sys_time-sys_start_time-(timeUs - aac_start_time),
									sys_time-last_adujst_time,frame_interval);
								fflush(omx_rs_txt);			
												
							}	
					}  
				}
				inHeader->nTimeStamp = timeUs;
				last_timeUs = timeUs;
	        	if(sys_time - last_adujst_time > 20000000ll)
					last_adujst_time = sys_time;
			}
		}
		else if(mStreamSource == NULL && wifidisplay_flag == 1)
			ALOGD("wifidisplay error because no streamingsoure");
#endif		
        if ((inHeader->nOffset == 0) && (mPreTimeUs !=inHeader->nTimeStamp)) {
            mAnchorTimeUs = inHeader->nTimeStamp;
            mNumSamplesOutput = 0;
        }

        mPreTimeUs = inHeader->nTimeStamp;

        size_t adtsHeaderSize = 0;
        if (mIsADTS) {
            // skip 30 bits, aac_frame_length follows.
            // ssssssss ssssiiip ppffffPc ccohCCll llllllll lll?????

            const uint8_t *adtsHeader = inHeader->pBuffer + inHeader->nOffset;

            bool signalError = false;
            if (inHeader->nFilledLen < 7) {
                ALOGE("Audio data too short to contain even the ADTS header. "
                      "Got %ld bytes.", inHeader->nFilledLen);
                hexdump(adtsHeader, inHeader->nFilledLen);
                signalError = true;
            } else {
                bool protectionAbsent = (adtsHeader[1] & 1);

                unsigned aac_frame_length =
                    ((adtsHeader[3] & 3) << 11)
                    | (adtsHeader[4] << 3)
                    | (adtsHeader[5] >> 5);

                if (inHeader->nFilledLen < aac_frame_length) {
                    ALOGE("Not enough audio data for the complete frame. "
                          "Got %ld bytes, frame size according to the ADTS "
                          "header is %u bytes.",
                          inHeader->nFilledLen, aac_frame_length);
                    hexdump(adtsHeader, inHeader->nFilledLen);
                    signalError = true;
                } else {
                    adtsHeaderSize = (protectionAbsent ? 7 : 9);

                    mConfig->pInputBuffer =
                        (UChar *)adtsHeader + adtsHeaderSize;

                    mConfig->inputBufferCurrentLength =
                        aac_frame_length - adtsHeaderSize;

                    inHeader->nOffset += adtsHeaderSize;
                    inHeader->nFilledLen -= adtsHeaderSize;
                }
            }

            if (signalError) {
                mSignalledError = true;

                notify(OMX_EventError, OMX_ErrorStreamCorrupt,
                       ERROR_MALFORMED, NULL);

                return;
            }
        } else {
            mConfig->pInputBuffer = inHeader->pBuffer + inHeader->nOffset;
            mConfig->inputBufferCurrentLength = inHeader->nFilledLen;
        }

        mConfig->inputBufferMaxLength = 0;
        mConfig->inputBufferUsedLength = 0;
        mConfig->remainderBits = 0;

        mConfig->pOutputBuffer =
            reinterpret_cast<Int16 *>(outHeader->pBuffer + outHeader->nOffset);

        mConfig->pOutputBuffer_plus = &mConfig->pOutputBuffer[2048];
        mConfig->repositionFlag = false;

#if WRITE_FILE
        if(aacFp) {
            fwrite(&mConfig->inputBufferCurrentLength, 1, 4, aacFp);
            fwrite(mConfig->pInputBuffer,1, mConfig->inputBufferCurrentLength,aacFp);
        }
#endif
        Int32 prevSamplingRate = mConfig->samplingRate;
        Int decoderErr = MP4AUDEC_SUCCESS;
		NeAACDecFrameInfo hInfo;
		void *samplebuffer;
		if(mUseFaadDecoder == 0) {
			if(mConfig->isMutilChannle)
				decoderErr = PVMP4AudioDecodeFrameSixChannel(mConfig, mDecoderBuf);
			else
				decoderErr = PVMP4AudioDecodeFrame(mConfig, mDecoderBuf);
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
        if (decoderErr == MP4AUDEC_SUCCESS && mInputBufferCount <= 2) {
            ALOGV("audio/extended audio object type: %d + %d",
                mConfig->audioObjectType, mConfig->extendedAudioObjectType);
            ALOGV("aac+ upsampling factor: %d desired channels: %d",
                mConfig->aacPlusUpsamplingFactor, mConfig->desiredChannels);

            if (mInputBufferCount == 1) {
                mUpsamplingFactor = mConfig->aacPlusUpsamplingFactor;
                // Check on the sampling rate to see whether it is changed.
                if (mConfig->samplingRate != prevSamplingRate) {
                    ALOGW("Sample rate was %d Hz, but now is %d Hz",
                            prevSamplingRate, mConfig->samplingRate);

                    // We'll hold onto the input buffer and will decode
                    // it again once the output port has been reconfigured.

                    // We're going to want to revisit this input buffer, but
                    // may have already advanced the offset. Undo that if
                    // necessary.
                    inHeader->nOffset -= adtsHeaderSize;
                    inHeader->nFilledLen += adtsHeaderSize;

                    notify(OMX_EventPortSettingsChanged, 1, 0, NULL);
                    mOutputPortSettingsChange = AWAITING_DISABLED;
                    return;
                }
            } else {  // mInputBufferCount == 2
                if (mConfig->extendedAudioObjectType == MP4AUDIO_AAC_LC ||
                    mConfig->extendedAudioObjectType == MP4AUDIO_LTP) {
                    if (mUpsamplingFactor == 2) {
                        // The stream turns out to be not aacPlus mode anyway
                        ALOGW("Disable AAC+/eAAC+ since extended audio object "
                             "type is %d",
                             mConfig->extendedAudioObjectType);
                        mConfig->aacPlusEnabled = 0;
                    }
                } else {
                    if (mUpsamplingFactor == 1) {
                        // aacPlus mode does not buy us anything, but to cause
                        // 1. CPU load to increase, and
                        // 2. a half speed of decoding
                        ALOGW("Disable AAC+/eAAC+ since upsampling factor is 1");
                        mConfig->aacPlusEnabled = 0;
                    }
                }
            }
        }
		}else {
			samplebuffer = NeAACDecDecode(hAac, &hInfo, inHeader->pBuffer + inHeader->nOffset, inHeader->nFilledLen);
			if(hInfo.channels > 6) {
				ALOGW("Now do not support channels > 6.\n");
				hInfo.error = -1;
			}
			if ((hInfo.error == 0) && (hInfo.samples > 0))
			{
				decoderErr = MP4AUDEC_SUCCESS;
				memcpy(outHeader->pBuffer + outHeader->nOffset, samplebuffer, hInfo.samples*2);
			} else if (hInfo.error != 0) {
				ALOGE("NeAACDecDecode error: error = %d.\n", hInfo.error);
				decoderErr = MP4AUDEC_INVALID_FRAME;
			} else {
				decoderErr = MP4AUDEC_SUCCESS;
			}
			mConfig->inputBufferUsedLength = hInfo.bytesconsumed;
		}

        size_t numOutBytes =
            mConfig->frameLength * sizeof(int16_t) * mConfig->desiredChannels;
		if (mUseFaadDecoder == 1) {
			numOutBytes = hInfo.samples*2;
		}

        if (decoderErr == MP4AUDEC_SUCCESS) {
            CHECK_LE(mConfig->inputBufferUsedLength, inHeader->nFilledLen);

            inHeader->nFilledLen -= mConfig->inputBufferUsedLength;
            inHeader->nOffset += mConfig->inputBufferUsedLength;
        } else {
            ALOGW("AAC decoder returned error %d, substituting silence",
                 decoderErr);

            memset(outHeader->pBuffer + outHeader->nOffset, 0, numOutBytes);

            // Discard input buffer.
            inHeader->nFilledLen = 0;

            // fall through
        }

        if (decoderErr == MP4AUDEC_SUCCESS || mNumSamplesOutput > 0) {
            // We'll only output data if we successfully decoded it or
            // we've previously decoded valid data, in the latter case
            // (decode failed) we'll output a silent frame.

			if (mUseFaadDecoder == 0) {
            if (mUpsamplingFactor == 2) {
                if (mConfig->desiredChannels == 1) {
                    memcpy(&mConfig->pOutputBuffer[1024],
                           &mConfig->pOutputBuffer[2048],
                           numOutBytes * 2);
                }
                numOutBytes *= 2;
            }
			}
#if WIFI_DISPLAY_ENABLE
			if(set_flag == 1)
				memset(outHeader->pBuffer,0,numOutBytes);
#endif
            outHeader->nFilledLen = numOutBytes;
            outHeader->nFlags = 0;

            outHeader->nTimeStamp =
                mAnchorTimeUs
                    + (mNumSamplesOutput * 1000000ll) / mConfig->samplingRate;

			if (mUseFaadDecoder == 0) {
            mNumSamplesOutput += mConfig->frameLength * mUpsamplingFactor;
			} else {
				if ((hInfo.channels >2) && (hInfo.channels <=6)) {
                    mNumSamplesOutput += (hInfo.samples / hInfo.channels);
				} else {
				    mNumSamplesOutput += hInfo.samples>>1;
				}
			}

            outInfo->mOwnedByUs = false;
            outQueue.erase(outQueue.begin());
            outInfo = NULL;
            notifyFillBufferDone(outHeader);
            outHeader = NULL;
        }

        if (inHeader->nFilledLen == 0) {
            inInfo->mOwnedByUs = false;
            inQueue.erase(inQueue.begin());
            inInfo = NULL;
            notifyEmptyBufferDone(inHeader);
            inHeader = NULL;
        }

        if (decoderErr == MP4AUDEC_SUCCESS) {
            ++mInputBufferCount;
        }
    }
}

void SoftAAC::onPortFlushCompleted(OMX_U32 portIndex) {
    if (portIndex == 0) {
        // Make sure that the next buffer output does not still
        // depend on fragments from the last one decoded.
        PVMP4AudioDecoderResetBuffer(mDecoderBuf);
    }
}

void SoftAAC::onPortEnableCompleted(OMX_U32 portIndex, bool enabled) {
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
    return new android::SoftAAC(name, callbacks, appData, component);
}
