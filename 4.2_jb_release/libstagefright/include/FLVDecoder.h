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

#ifndef FLV_DECODER_H_

#define FLV_DECODER_H_

#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaSource.h>
#include <utils/Vector.h>
#include <vpu_api.h>

class On2_H263Decoder;

namespace android {

struct FLVDecoder : public MediaSource,
                    public MediaBufferObserver {
    FLVDecoder(const sp<MediaSource> &source);

    virtual status_t start(MetaData *params);
    virtual status_t stop();

    virtual sp<MetaData> getFormat();

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options);

    virtual void signalBufferReturned(MediaBuffer *buffer);
    int32_t FlvGetResolution(int32_t *aWidth, int32_t *aHeight, uint8_t *aBuffer);

protected:
    virtual ~FLVDecoder();

private:
    sp<MediaSource> mSource;
    bool mStarted;
    VPU_API sDecApi;
    void *pOn2Dec;
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
    uint8_t *streamInbuf;
    bool _success;
    uint32_t getwhFlg;
    FLVDecoder(const FLVDecoder &);
    FLVDecoder &operator=(const FLVDecoder &);
};

}  // namespace android

#endif  // AVC_DECODER_H_
