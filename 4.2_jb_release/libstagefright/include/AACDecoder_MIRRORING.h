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

#ifndef AAC_DECODER_MIRRORING_H

#define AAC_DECODER_MIRRORING_H

#include <media/stagefright/MediaSource.h>

struct tPVMP4AudioDecoderExternal;

namespace android {

struct MediaBufferGroup;
struct MetaData;

struct AACDecoder_MIRRORING : public MediaSource {
    AACDecoder_MIRRORING(const sp<MediaSource> &source);

    virtual status_t start(MetaData *params=NULL);
    virtual status_t stop();

    virtual sp<MetaData> getFormat();
	void* ThreadWrapper(void *);
		static void* rec_data(void* me);

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options);

protected:
    virtual ~AACDecoder_MIRRORING();

private:
    sp<MetaData>    mMeta;
    sp<MediaSource> mSource;
    bool mStarted;

    MediaBufferGroup *mBufferGroup;

    tPVMP4AudioDecoderExternal *mConfig;
    void *mDecoderBuf;
    int64_t mAnchorTimeUs;
    int64_t mNumSamplesOutput;
    status_t mInitCheck;
    int64_t  mNumDecodedBuffers;
    int32_t  mUpsamplingFactor;
    int32_t  wimo_flag;
    int64_t  start_timeUs;
    MediaBuffer *mInputBuffer;
    int64_t  last_timeUs;
    int64_t  Waiting_timeUs;
	int		 adjust_flag;
	int64_t  last_adujst_time;
	int listenFd;
    int clientSocketFd;
	 pthread_t mThread;
	 int connect_flag;
	int isAccpetSuccess;
	int64_t  gpu_adjustment;
	int64_t  Video_timeUs;
	int64_t  Cur_TimeUs;
	int server_sockfd;
	int client_sockfd ;
    status_t initCheck();
    AACDecoder_MIRRORING(const AACDecoder_MIRRORING &);
    AACDecoder_MIRRORING &operator=(const AACDecoder_MIRRORING &);
};

}  // namespace android

#endif  // AAC_DECODER_H_
