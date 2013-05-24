/*
 * Copyright (C) 2012 The Android Open Source Project
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
#define LOG_TAG "SoftRaw"
#include <utils/Log.h>

#include "SoftRaw.h"

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/hexdump.h>
#include <dlfcn.h>
namespace android {
#define WIFI_DISPLAY_ENABLE_RAW	1

template<class T>
static void InitOMXParams(T *params) {
    params->nSize = sizeof(T);
    params->nVersion.s.nVersionMajor = 1;
    params->nVersion.s.nVersionMinor = 0;
    params->nVersion.s.nRevision = 0;
    params->nVersion.s.nStep = 0;
}

SoftRaw::SoftRaw(
        const char *name,
        const OMX_CALLBACKTYPE *callbacks,
        OMX_PTR appData,
        OMX_COMPONENTTYPE **component)
    : SimpleSoftOMXComponent(name, callbacks, appData, component),
      mSignalledError(false),
      mChannelCount(2),
      mSampleRate(44100) ,
       wifidisplay_flag(0),
      wifidisplay_addr(0){


	
#if WIFI_DISPLAY_ENABLE_RAW
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
    initPorts();
    CHECK_EQ(initDecoder(), (status_t)OK);
}

SoftRaw::~SoftRaw() {
	
#if WIFI_DISPLAY_ENABLE_RAW
		if(omx_rs_txt != NULL)
		{
			fclose(omx_rs_txt);
			omx_rs_txt = NULL;
		}
		mStreamSource = NULL;
#endif
}

void SoftRaw::initPorts() {
    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOMXParams(&def);

    def.nPortIndex = 0;
    def.eDir = OMX_DirInput;
    def.nBufferCountMin = kNumBuffers;
    def.nBufferCountActual = def.nBufferCountMin;
    def.nBufferSize = 32 * 1024;
    def.bEnabled = OMX_TRUE;
    def.bPopulated = OMX_FALSE;
    def.eDomain = OMX_PortDomainAudio;
    def.bBuffersContiguous = OMX_FALSE;
    def.nBufferAlignment = 1;

    def.format.audio.cMIMEType = const_cast<char *>("audio/raw");
    def.format.audio.pNativeRender = NULL;
    def.format.audio.bFlagErrorConcealment = OMX_FALSE;
    def.format.audio.eEncoding = OMX_AUDIO_CodingPCM;

    addPort(def);

    def.nPortIndex = 1;
    def.eDir = OMX_DirOutput;
    def.nBufferCountMin = kNumBuffers;
    def.nBufferCountActual = def.nBufferCountMin;
    def.nBufferSize = 32 * 1024;
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

status_t SoftRaw::initDecoder() {
    return OK;
}

OMX_ERRORTYPE SoftRaw::internalGetParameter(
        OMX_INDEXTYPE index, OMX_PTR params) {
    switch (index) {
        case OMX_IndexParamAudioPcm:
        {
            OMX_AUDIO_PARAM_PCMMODETYPE *pcmParams =
                (OMX_AUDIO_PARAM_PCMMODETYPE *)params;

            if (pcmParams->nPortIndex != 0 && pcmParams->nPortIndex != 1) {
                return OMX_ErrorUndefined;
            }

            pcmParams->eNumData = OMX_NumericalDataSigned;
            pcmParams->eEndian = OMX_EndianBig;
            pcmParams->bInterleaved = OMX_TRUE;
            pcmParams->nBitPerSample = 16;
            pcmParams->ePCMMode = OMX_AUDIO_PCMModeLinear;
            pcmParams->eChannelMapping[0] = OMX_AUDIO_ChannelLF;
            pcmParams->eChannelMapping[1] = OMX_AUDIO_ChannelRF;

            pcmParams->nChannels = mChannelCount;
            pcmParams->nSamplingRate = mSampleRate;

            return OMX_ErrorNone;
        }

        default:
            return SimpleSoftOMXComponent::internalGetParameter(index, params);
    }
}

OMX_ERRORTYPE SoftRaw::internalSetParameter(
        OMX_INDEXTYPE index, const OMX_PTR params) {
    switch (index) {
        case OMX_IndexParamStandardComponentRole:
        {
            const OMX_PARAM_COMPONENTROLETYPE *roleParams =
                (const OMX_PARAM_COMPONENTROLETYPE *)params;

            if (strncmp((const char *)roleParams->cRole,
                        "audio_decoder.raw",
                        OMX_MAX_STRINGNAME_SIZE - 1)) {
                return OMX_ErrorUndefined;
            }

            return OMX_ErrorNone;
        }

        case OMX_IndexParamAudioPcm:
        {
            const OMX_AUDIO_PARAM_PCMMODETYPE *pcmParams =
                (OMX_AUDIO_PARAM_PCMMODETYPE *)params;

            if (pcmParams->nPortIndex != 0) {
                return OMX_ErrorUndefined;
            }

            mChannelCount = pcmParams->nChannels;
            mSampleRate = pcmParams->nSamplingRate;
			ALOGD("mChannelCount %d mSampleRate %d",mChannelCount,mSampleRate);
            return OMX_ErrorNone;
        }
		case OMX_IndexConfigOtherStats:
		{
			const OMX_AUDIO_PARAM_PCMMODETYPE *pcmParams =
                (OMX_AUDIO_PARAM_PCMMODETYPE *)params;

			wifidisplay_flag = pcmParams->nChannels;
			wifidisplay_addr = pcmParams->nSamplingRate;
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

void SoftRaw::onQueueFilled(OMX_U32 portIndex) {
    if (mSignalledError) {
        return;
    }

    List<BufferInfo *> &inQueue = getPortQueue(0);
    List<BufferInfo *> &outQueue = getPortQueue(1);
	int64_t timeUs;
	int	set_flag = 0;
	if(omx_rs_txt != NULL)
					
				{
				
					fprintf(omx_rs_txt,"SoftRaw inqueue size %d\n",inQueue.size());
					fflush(omx_rs_txt); 		
									
				}
    while (!inQueue.empty() && !outQueue.empty()) {
        BufferInfo *inInfo = *inQueue.begin();
        OMX_BUFFERHEADERTYPE *inHeader = inInfo->mHeader;

        BufferInfo *outInfo = *outQueue.begin();
        OMX_BUFFERHEADERTYPE *outHeader = outInfo->mHeader;
		
#if WIFI_DISPLAY_ENABLE_RAW
		if(mStreamSource != NULL && wifidisplay_flag == 1)
		{
			int64_t sys_start_time;
			int64_t raw_start_time;
			int64_t sys_time;
			int status;
			int64_t wifi_time,wifi_sys_time;
			timeUs = inHeader->nTimeStamp;
			wifi_time = wifi_sys_time = 0;
			status = mStreamSource((void *)wifidisplay_addr,&wifi_sys_time,&wifi_time);
			
			if(wifi_sys_time == 0 && wifi_time == 0)
			{
				if(omx_rs_txt != NULL)
					
				{
				
					fprintf(omx_rs_txt,"SoftRaw normal dec delay 300 ms start %lld	%lld  %lld sys %lld timeUs %lld last %lld delta %lld frame_interval %lld\n"
					,sys_start_time,start_time,raw_start_time,sys_time,timeUs,last_timeUs,sys_time-sys_start_time-(timeUs - raw_start_time),frame_interval);
					fflush(omx_rs_txt); 		
									
				}
				sys_start_time	=	start_time;
				raw_start_time	=	start_timeUs;
				last_timeUs 	=	inHeader->nTimeStamp;
				if(status == -1)
					ALOGD("Error : Streamingsource can't match softraw decoder");
			}
			else
			{
				wifi_start_time = wifi_sys_time;
				wifi_audio_start_time = wifi_time;
				if(mSampleRate == 0)
				{
					ALOGD("samplingRate %d",mSampleRate);
					return;
				}
				frame_interval = ((inHeader->nFilledLen / (2 * mChannelCount)) * 1000 )/ mSampleRate;
				if(frame_interval == 0 || mSampleRate == 0)
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
					ALOGV("SOFTRaw start time %lld wifi_start_time %d",start_time,wifi_start_time);
				}
				ALOGV("SOFTRaw start time %lld wifi_start_time %lld mConfig->samplingRate %d",start_time,wifi_start_time,mConfig->samplingRate);
				sys_start_time =	wifi_start_time;
				raw_start_time =	wifi_audio_start_time;
			
				int retrtptxt;
				timeUs = inHeader->nTimeStamp;
				if((retrtptxt = access("data/test/omx_rs_txt_file",0)) == 0)
				{
					if(omx_rs_txt == NULL)
						omx_rs_txt = fopen("data/test/omx_rs_txt.txt","ab");
				}	  
				
				{
					if(sys_start_time + (timeUs - raw_start_time) < sys_time - 100000ll )
					{	
						if(last_timeUs < timeUs )//loop tntil the real timeUs catch up with the old setted one	, if there is no data,the old setted is also faster than the real timeUs.so it's okay
						{
							if(sys_start_time + (timeUs - raw_start_time) < sys_time - 300000ll || (sys_time - last_adujst_time > 20000000ll && 
							sys_start_time + (timeUs - raw_start_time) < sys_time - 100000ll  ))//recalculate the timeUs.
							{
								if(omx_rs_txt != NULL)
					
								{
									if(sys_time - last_adujst_time > 20000000ll && 
							sys_start_time + (timeUs - raw_start_time) < sys_time - 100000ll   && sys_time-sys_start_time-(timeUs - raw_start_time) > 100000ll)
									fprintf(omx_rs_txt,"SoftRAW adjust start %lld  %lld  %lld sys %lld %lld timeUs %lld last %lld delta %lld  %lld frame_interval %lld\n"
									,sys_start_time,start_time,raw_start_time,last_adujst_time,sys_time,timeUs,last_timeUs,sys_time-sys_start_time-(timeUs - raw_start_time),
									sys_time-last_adujst_time,frame_interval);
									else
									fprintf(omx_rs_txt,"SoftRAW before dec delay 300 ms start %lld	%lld  %lld sys %lld %lld timeUs %lld last %lld delta %lld  %lld frame_interval %lld\n"
									,sys_start_time,start_time,raw_start_time,last_adujst_time,sys_time,timeUs,last_timeUs,sys_time-sys_start_time-(timeUs - raw_start_time),
									sys_time-last_adujst_time,frame_interval);
									fflush(omx_rs_txt); 		
													
								}
								timeUs +=((sys_time - sys_start_time - (timeUs - raw_start_time) ) / frame_interval) *frame_interval;					
								set_flag = 1;
								last_adujst_time = sys_time;
							}
							
							else
							{
								if(omx_rs_txt != NULL)
					
								{
								
									fprintf(omx_rs_txt,"SoftRAW before dec delay 100-300 ms start %lld	%lld  %lld sys %lld %lld timeUs %lld last %lld delta %lld  %lld frame_interval %lld\n"
									,sys_start_time,start_time,raw_start_time,last_adujst_time,sys_time,timeUs,last_timeUs,sys_time-sys_start_time-(timeUs - raw_start_time),
									sys_time-last_adujst_time,frame_interval);
									fflush(omx_rs_txt); 		
													
								}
								
							}
							
						
						}
						else
						{
							if(omx_rs_txt != NULL)
					
							{
							
								fprintf(omx_rs_txt,"SoftRAW before drop start %lld	%lld  %lld sys %lld %lld timeUs %lld last %lld delta %lld  %lld frame_interval %lld\n"
									,sys_start_time,start_time,raw_start_time,last_adujst_time,sys_time,timeUs,last_timeUs,sys_time-sys_start_time-(timeUs - raw_start_time),
									sys_time-last_adujst_time,frame_interval);
								fflush(omx_rs_txt); 		
							}	
							{
								inInfo->mOwnedByUs = false;
								inQueue.erase(inQueue.begin());
								notifyEmptyBufferDone(inHeader);
							}
							continue;
						}
					}
					else
					{
						if(omx_rs_txt != NULL)
					
							{
							
								fprintf(omx_rs_txt,"SoftRAW before less than 100ms start %lld  %lld  %lld sys %lld %lld timeUs %lld last %lld delta %lld  %lld frame_interval %lld\n"
									,sys_start_time,start_time,raw_start_time,last_adujst_time,sys_time,timeUs,last_timeUs,sys_time-sys_start_time-(timeUs - raw_start_time),
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
        CHECK_GE(outHeader->nAllocLen, inHeader->nFilledLen);
        memcpy(outHeader->pBuffer,
               inHeader->pBuffer + inHeader->nOffset,
               inHeader->nFilledLen);
#if WIFI_DISPLAY_ENABLE_RAW
		if(set_flag == 1)
			memset(outHeader->pBuffer,0, inHeader->nFilledLen);
#endif

        outHeader->nFlags = inHeader->nFlags;
        outHeader->nOffset = 0;
        outHeader->nFilledLen = inHeader->nFilledLen;
        outHeader->nTimeStamp = inHeader->nTimeStamp;

        bool sawEOS = (inHeader->nFlags & OMX_BUFFERFLAG_EOS) != 0;

        inQueue.erase(inQueue.begin());
        inInfo->mOwnedByUs = false;
        notifyEmptyBufferDone(inHeader);

        outQueue.erase(outQueue.begin());
        outInfo->mOwnedByUs = false;
        notifyFillBufferDone(outHeader);

        if (sawEOS) {
            break;
        }
    }
}

}  // namespace android

android::SoftOMXComponent *createSoftOMXComponent(
        const char *name, const OMX_CALLBACKTYPE *callbacks,
        OMX_PTR appData, OMX_COMPONENTTYPE **component) {
    return new android::SoftRaw(name, callbacks, appData, component);
}
