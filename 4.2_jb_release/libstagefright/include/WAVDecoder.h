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

#ifndef WAV_DECODER_H_

#define WAV_DECODER_H_


#include <media/stagefright/MediaSource.h>

#define MSADPCM_MAX_PCM_LENGTH          2048
#define IMAADPCM_MAX_PCM_LENGTH         4096

#define PCM_OUT_NUM 2048
typedef struct
{
    uint16_t    FormatTag;
    uint16_t    Channels;
    uint32_t    SamplesPerSec;
    uint32_t    AvgBytesPerSec;
    uint16_t    BlockAlign;
    uint16_t    BitsPerSample;
    uint16_t    Size;
	uint16_t	  SamplesPerBlock;

}WaveFormatExStruct;

enum PVWavAudioFormats
{
    PVWAV_UNKNOWN_AUDIO_FORMAT = 0,
    PVWAV_PCM_AUDIO_FORMAT = 1,//0x01
    PVWAV_ADPCM_FORMAT = 2,//0x02
    PVWAV_ITU_G711_ALAW = 6,
    PVWAV_ITU_G711_ULAW = 7,
    PVWAV_IMA_ADPCM_FORMAT = 17//0x11
};

typedef enum
{
	WAVDEC_SUCCESS			 =	0,
	WAVDEC_END				 = 10,
	WAVDEC_ERROR			 = 20,
	WAVDEC_INCOMPLETE_FRAME  = 30
} tPVWAVDecoderErrorCode;

struct MSADPCM_PRIVATE;
struct tPCM;

namespace android {

struct MediaBufferGroup;

struct WAVDecoder : public MediaSource {
    WAVDecoder(const sp<MediaSource> &source);

    virtual status_t start(MetaData *params);
    virtual status_t stop();

    virtual sp<MetaData> getFormat();

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options);

protected:
    virtual ~WAVDecoder();

private:
    sp<MediaSource> mSource;
    sp<MetaData> mMeta;
    int32_t mNumChannels;
	int32_t mSampleRate;
    bool mStarted;

    MediaBufferGroup *mBufferGroup;

	WaveFormatExStruct * mWAVDecExt;
	uint32_t mNeedInputNumLen;
	void * mNeedInputBuf;
	MSADPCM_PRIVATE * mMsAdpcm;
	tPCM * mPCM;
	
    int64_t mAnchorTimeUs;
    int64_t mNumFramesOutput;

    MediaBuffer *mInputBuffer;

    void init();

    WAVDecoder(const WAVDecoder &);
    WAVDecoder &operator=(const WAVDecoder &);
};

}  // namespace android

#endif  // AC3_DECODER_H_
