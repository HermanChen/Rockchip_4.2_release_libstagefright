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

#ifndef SOFT_RAW_H_

#define SOFT_RAW_H_

#include "SimpleSoftOMXComponent.h"

struct tPVMP4AudioDecoderExternal;

namespace android {

struct SoftRaw : public SimpleSoftOMXComponent {
    SoftRaw(const char *name,
            const OMX_CALLBACKTYPE *callbacks,
            OMX_PTR appData,
            OMX_COMPONENTTYPE **component);

protected:
    virtual ~SoftRaw();

    virtual OMX_ERRORTYPE internalGetParameter(
            OMX_INDEXTYPE index, OMX_PTR params);

    virtual OMX_ERRORTYPE internalSetParameter(
            OMX_INDEXTYPE index, const OMX_PTR params);

    virtual void onQueueFilled(OMX_U32 portIndex);

private:
    enum {
        kNumBuffers = 4
    };

    bool mSignalledError;

    int32_t mChannelCount;
    int32_t mSampleRate;
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

    int64_t mPreTimeUs;
	int64_t last_adujst_time;
    void initPorts();
    status_t initDecoder();

    DISALLOW_EVIL_CONSTRUCTORS(SoftRaw);
};

}  // namespace android

#endif  // SOFT_RAW_H_
