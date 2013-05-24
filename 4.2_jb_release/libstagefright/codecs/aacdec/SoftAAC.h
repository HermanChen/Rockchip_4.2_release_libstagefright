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

#ifndef SOFT_AAC_H_

#define SOFT_AAC_H_

#include "SimpleSoftOMXComponent.h"
#include "neaacdec.h"

struct tPVMP4AudioDecoderExternal;

namespace android {

struct SoftAAC : public SimpleSoftOMXComponent {
    SoftAAC(const char *name,
            const OMX_CALLBACKTYPE *callbacks,
            OMX_PTR appData,
            OMX_COMPONENTTYPE **component);

protected:
    virtual ~SoftAAC();

    virtual OMX_ERRORTYPE internalGetParameter(
            OMX_INDEXTYPE index, OMX_PTR params);

    virtual OMX_ERRORTYPE internalSetParameter(
            OMX_INDEXTYPE index, const OMX_PTR params);

    virtual void onQueueFilled(OMX_U32 portIndex);
    virtual void onPortFlushCompleted(OMX_U32 portIndex);
    virtual void onPortEnableCompleted(OMX_U32 portIndex, bool enabled);

private:
    enum {
        kNumInputBuffers        = 4,
        kNumOutputBuffers       = 4,
    };

    tPVMP4AudioDecoderExternal *mConfig;
    bool mIsADTS;
    void *mDecoderBuf;

    size_t mInputBufferCount;
    size_t mUpsamplingFactor;
    int64_t mAnchorTimeUs;
    int64_t mNumSamplesOutput;
	int mUseFaadDecoder;
	NeAACDecHandle hAac;
	NeAACDecConfigurationPtr conf;	
 	int64_t start_time ;
	int64_t start_timeUs ;
	int64_t wifi_start_time ;
	int64_t wifi_audio_start_time;
	int64_t last_timeUs;
	int64_t frame_interval;
	int32_t wifidisplay_flag;
	int32_t wifidisplay_addr;
	FILE *omx_rs_txt ;
	void* widi_handle;
	int (*mStreamSource)(void* ptr,int64_t *start_time,int64_t *audio_start_time);
    bool mSignalledError;

    enum {
        NONE,
        AWAITING_DISABLED,
        AWAITING_ENABLED
    } mOutputPortSettingsChange;

    int64_t mPreTimeUs;
	int64_t last_adujst_time;
    void initPorts();
    status_t initDecoder();
    bool isConfigured() const;

    DISALLOW_EVIL_CONSTRUCTORS(SoftAAC);
};

}  // namespace android

#endif  // SOFT_AAC_H_
