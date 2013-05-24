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

#ifndef AC3_DECODER_H_

#define AC3_DECODER_H_


#include <media/stagefright/MediaSource.h>

struct tAC3;

namespace android {

struct MediaBufferGroup;

struct AC3Decoder : public MediaSource {
    AC3Decoder(const sp<MediaSource> &source);

    virtual status_t start(MetaData *params);
    virtual status_t stop();

    virtual sp<MetaData> getFormat();

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options);

protected:
    virtual ~AC3Decoder();

private:
    sp<MediaSource> mSource;
    sp<MetaData> mMeta;
    int32_t mNumChannels;
	int32_t mSampleRate;
    bool mStarted;

    MediaBufferGroup *mBufferGroup;

    tAC3 *mConfig;
    int64_t mAnchorTimeUs;
    int64_t mNumFramesOutput;

    MediaBuffer *mInputBuffer;

	int64_t	reAsyTime;
	int64_t	preframeTime;
	size_t	reAsynEn;
	size_t	reAsynSize;

    void init();

    AC3Decoder(const AC3Decoder &);
    AC3Decoder &operator=(const AC3Decoder &);
};

}  // namespace android

#endif  // AC3_DECODER_H_
