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

#ifndef RA_DECODER_H_

#define RA_DECODER_H_


#include <media/stagefright/MediaSource.h>

//#define RA_ENABLE_OUTPUT_CACHE // reduce frame size to 2048

#ifdef RA_ENABLE_OUTPUT_CACHE
#define RA_OUTPUT_BUF_SIZE (10240)
#define RA_OUTPUT_CACHE_SIZE (20480*4)
typedef struct
{
	unsigned char *pCache;
	int 		  readOffset;
	int 		  writeOffset;
	int           size;
} sRaOutputCache;
#else
#define RA_OUTPUT_BUF_SIZE (20480 * 2)
#endif

#define RA_MAX_OUTPUT_BUF_SIZE (20480*2)

struct RMAudioStram;

namespace android {

struct MediaBufferGroup;

struct RADecoder : public MediaSource {
    RADecoder(const sp<MediaSource> &source);

    virtual status_t start(MetaData *params);
    virtual status_t stop();

    virtual sp<MetaData> getFormat();

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options);

protected:
    virtual ~RADecoder();

private:
    sp<MediaSource> mSource;
    sp<MetaData> mMeta;
    int32_t mNumChannels;
	int32_t mSampleRate;
    bool mStarted;

    MediaBufferGroup *mBufferGroup;

	bool   mDecInit;
	RMAudioStram *mConfig;

#ifdef RA_ENABLE_OUTPUT_CACHE
	sRaOutputCache RaOutputCache;
#endif

    int64_t mAnchorTimeUs;
    int64_t mNumFramesOutput;
    int64_t mNumFramesDecoder;

    MediaBuffer *mInputBuffer;

    int64_t	reAsyTime;
	int64_t	preframeTime;
	size_t	reAsynEn;
	size_t	reAsynSize;

    void init();

    RADecoder(const RADecoder &);
    RADecoder &operator=(const RADecoder &);
};

}  // namespace android

#endif  // AC3_DECODER_H_
