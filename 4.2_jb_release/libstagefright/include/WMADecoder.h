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

#ifndef WMA_DECODER_H_

#define WMA_DECODER_H_


#include <media/stagefright/MediaSource.h>

struct CPvWMA_Decoder;
struct tPVWMADecoderExternal;
namespace android {

struct MediaBufferGroup;

struct WMADecoder : public MediaSource {
   WMADecoder(const sp<MediaSource> &source);

    virtual status_t start(MetaData *params);
    virtual status_t stop();

    virtual sp<MetaData> getFormat();

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options);

protected:
    virtual ~WMADecoder();

private:
    sp<MediaSource> mSource;
    sp<MetaData> mMeta;
    int32_t mNumChannels;
	int32_t mSampleRate;
    bool mStarted;
	uint32_t mInitFlag;
    MediaBufferGroup *mBufferGroup;
	CPvWMA_Decoder* mAudioWMADecoder;
    tPVWMADecoderExternal *mWMADecExt;
    int64_t mAnchorTimeUs;
    int64_t mNumFramesOutput;

    MediaBuffer *mInputBuffer;
	MediaBuffer *mFirstOutputBuf;
    int64_t mReAsynTime;
    int64_t mReAsynThreshHold;

    void init();

    WMADecoder(const WMADecoder &);
    WMADecoder &operator=(const WMADecoder &);
};

}  // namespace android

#endif  // WMA_DECODER_H_
