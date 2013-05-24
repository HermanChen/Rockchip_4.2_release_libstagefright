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

#ifndef DTS_DECODER_H_

#define DTS_DECODER_H_


#include <media/stagefright/MediaSource.h>

typedef struct AudioTrackSwitch{
    bool    flag;
    int64_t timeUs;
}AudioTrackSwitch;
struct tPVDTSDecoderExt;
class dts_dec;
namespace android {

struct MediaBufferGroup;

struct DTSDecoder : public MediaSource {
   DTSDecoder(const sp<MediaSource> &source);

    virtual status_t start(MetaData *params);
    virtual status_t stop();

    virtual sp<MetaData> getFormat();

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options);

protected:
    virtual ~DTSDecoder();

private:
    sp<MediaSource> mSource;
    sp<MetaData> mMeta;
    int32_t mNumChannels;
	int32_t mSampleRate;
	int32_t mFrameCount;
    bool mStarted;
	bool mDecInit;
    MediaBufferGroup *mBufferGroup;
	dts_dec * mDtsDec;
    tPVDTSDecoderExt *mConfig;
    int64_t mAnchorTimeUs;
    int64_t mNumFramesOutput;
    AudioTrackSwitch mTrkSwitch;

    MediaBuffer *mInputBuffer;
	MediaBuffer *mFirstOutputBuf;
	
    void init();

    DTSDecoder(const DTSDecoder &);
    DTSDecoder &operator=(const DTSDecoder &);
};

}  // namespace android

#endif  // AC3_DECODER_H_
