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

#ifndef M2V_DECODER_H_

#define M2V_DECODER_H_

#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaSource.h>
#include <utils/Vector.h>
#include <vpu_api.h>


class On2_M2vDecoder;

namespace android {
	
struct M2VDecoder : public MediaSource,
                    public MediaBufferObserver {
    M2VDecoder(const sp<MediaSource> &source);

    virtual status_t start(MetaData *params);
    virtual status_t stop();

    virtual sp<MetaData> getFormat();

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options);

    virtual void signalBufferReturned(MediaBuffer *buffer);

protected:
    virtual ~M2VDecoder();

private:
    sp<MediaSource> mSource;
    bool mStarted;
    sp<MetaData> mFormat;
    MediaBuffer *mInputBuffer;
    int64_t mAnchorTimeUs;
    int64_t mNumFramesOutput;
    int64_t mPendingSeekTimeUs;
    MediaSource::ReadOptions::SeekMode mPendingSeekMode;
    int64_t mTargetTimeUs;
		int32_t mWidth;
		int32_t mHeight;
		uint32_t mYuvLen;
    VPU_API sDecApi;
    void *pOn2Dec;
    bool _success;
    M2VDecoder(const M2VDecoder &);
    M2VDecoder &operator=(const M2VDecoder &);
};

}  // namespace android

#endif  // AVC_DECODER_H_
