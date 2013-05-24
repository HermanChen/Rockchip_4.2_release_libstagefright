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

#ifndef AVC_DECODER_H_

#define AVC_DECODER_H_

#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaSource.h>
#include <utils/Vector.h>
#include <utils/List.h>
#include <vpu_api.h>
struct tagAVCHandle;
struct ABuffer;

namespace android {

#define ON2_AVC_DEC	1

struct AVCDecoder : public MediaSource,
                    public MediaBufferObserver {
    AVCDecoder(const sp<MediaSource> &source);
    void SetParameterForWimo(const sp<MediaSource> &source);

    virtual status_t start(MetaData *params);
    virtual status_t stop();

    virtual sp<MetaData> getFormat();

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options);

    virtual void signalBufferReturned(MediaBuffer *buffer);

protected:
    virtual ~AVCDecoder();

private:
    sp<MediaSource> mSource;
    bool mStarted;

    sp<MetaData> mFormat;
    sp<ABuffer> mBuffer;

    Vector<MediaBuffer *> mCodecSpecificData;

    tagAVCHandle *mHandle;
    Vector<MediaBuffer *> mFrames;
    MediaBuffer *mInputBuffer;

    int64_t mAnchorTimeUs;
    int64_t mNumSamplesOutput;
    int64_t mPendingSeekTimeUs;
    MediaSource::ReadOptions::SeekMode mPendingSeekMode;

    int64_t mTargetTimeUs;
    bool mSeeking;

    List<int64_t> mTimestamps;
    int64_t lastTimestamps;
    int32_t AvcSendDtsFlag;
    int32_t SameTimeCount;
    bool mSPSSeen;
    bool mPPSSeen;

    void addCodecSpecificData(const uint8_t *data, size_t size);

    static int32_t ActivateSPSWrapper(
            void *userData, unsigned int sizeInMbs, unsigned int numBuffers);

    static int32_t BindFrameWrapper(
            void *userData, int32_t index, uint8_t **yuv);

    static void UnbindFrame(void *userData, int32_t index);

    int32_t activateSPS(
            unsigned int sizeInMbs, unsigned int numBuffers);

    int32_t bindFrame(int32_t index, uint8_t **yuv);

    void releaseFrames();

#ifdef MUT_SLICE
    MediaBuffer *dequeueOneFrame();
#endif
    MediaBuffer *drainOutputBuffer();

#if ON2_AVC_DEC
	int32_t mWidth;
	int32_t mHeight;
	int32_t mYuvLen;
    VPU_API sDecApi;
    void *pOn2Dec;
    uint8_t *streamInbuf;
    int32_t _success;
    int64_t mPreTimeUs;
	int32_t mts_en;
    bool    mFirstIFrm;
#endif
#ifdef AVC_DEBUG
		FILE *fp;
#endif
    AVCDecoder(const AVCDecoder &);
    AVCDecoder &operator=(const AVCDecoder &);
};

}  // namespace android

#endif  // AVC_DECODER_H_
