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

#ifndef MP3_DECODER_H_

#define MP3_DECODER_H_

#include <media/stagefright/MediaSource.h>
#define SUPPORT_NO_ENOUGH_MAIN_DATA 1
#if SUPPORT_NO_ENOUGH_MAIN_DATA
#define TMP_BUF_SIZE (1024*4)
typedef struct tPVMP3BUFEXT
{
	uint8_t * buf;//the buf space is TMP_BUF_SIZE bytes
	uint32_t len;//now the buf has len bytes data
}tPVMP3BUFEXT;
#endif

struct tPVMP3DecoderExternal;

namespace android {

struct MediaBufferGroup;

struct MP3Decoder : public MediaSource {
    MP3Decoder(const sp<MediaSource> &source);

    virtual status_t start(MetaData *params);
    virtual status_t stop();

    virtual sp<MetaData> getFormat();

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options);

protected:
    virtual ~MP3Decoder();

private:
    sp<MediaSource> mSource;
    sp<MetaData> mMeta;
    int32_t mNumChannels;
    int32_t mSampleRate;
    int32_t mNumDecodedBuffers;
	int32_t mFrameCount;
    bool mStarted;
	int32_t mDropFrameCount;
    MediaBufferGroup *mBufferGroup;

    tPVMP3DecoderExternal *mConfig;
    void *mDecoderBuf;
    int64_t mAnchorTimeUs;
    int64_t mNumFramesOutput;

    MediaBuffer *mInputBuffer;
#if SUPPORT_NO_ENOUGH_MAIN_DATA
	tPVMP3BUFEXT mBufExt;
#endif
    void init();

    MP3Decoder(const MP3Decoder &);
    MP3Decoder &operator=(const MP3Decoder &);
};

}  // namespace android

#endif  // MP3_DECODER_H_
