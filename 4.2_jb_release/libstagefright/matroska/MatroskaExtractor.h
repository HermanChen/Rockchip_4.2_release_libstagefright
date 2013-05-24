/*
 * Copyright (C) 2010 The Android Open Source Project
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

#ifndef MATROSKA_EXTRACTOR_H_

#define MATROSKA_EXTRACTOR_H_

#include <media/stagefright/MediaExtractor.h>
#include <media/stagefright/MediaBuffer.h>

#include <utils/Vector.h>
#include "dvdsubdec.h"

#define MKV_SUB_LANGUAGE_LEN 5
#define MKV_SUB_NAME_LEN 100

#define MKV_SKIP_SEEK_TIME_DELTA_US 500000  /* 500 ms*/
#define MKV_SKIP_SEEK_SYS_TIME_DELTA_US 5000000ll  /* 5s */

namespace mkvparser {
struct Segment;
};

namespace android {

typedef struct MKVTrackDecSpeciInfo
{
	int32_t	Audio_extradata_size;
	const uint8_t	*Audio_extradata;
	int32_t	aac_type;
	int32_t	aac_header_len;
}MKVAACDecSpeciInfo;

typedef enum
{
    VIDEO_TRACK_TYPE        = 0x1,
    AUDIO_TRACK_TYPE        = 0x2,
    SUBTITLE_TRACK_TYPE     = 0x11,

    TRACK_TYPE_BUTT     ,
}MATROSKA_TRACK_TYPE;

typedef struct BuiltInSubDataBuf
{
    uint8_t* data;
    uint32_t size;
    int64_t  timeMs;
    uint32_t durationMs;
    uint32_t capability;
}BuiltInSubDataBuf;

typedef struct SubtitleTrkInfo{
    long    trackNumber;
    char    nameAsUTF8[MKV_SUB_NAME_LEN];
    char    codecId[MKV_SUB_NAME_LEN];
    uint8_t codecIdLen;
    uint8_t nameLen;
    char language[MKV_SUB_LANGUAGE_LEN];

}SubtitleTrkInfo;

typedef struct AudioTrkInfo{
    long    trackNumber;
    char    codecId[MKV_SUB_NAME_LEN];
    uint8_t codecIdLen;
    char language[MKV_SUB_LANGUAGE_LEN];

}AudioTrkInfo;


typedef struct AvcFrameBuffer {
    uint8_t* buf;
    uint32_t capability;
}AvcFrameBuffer;

typedef enum {
  MATROSKA_TRACK_ENCODING_COMP_ZLIB        = 0,
  MATROSKA_TRACK_ENCODING_COMP_BZLIB       = 1,
  MATROSKA_TRACK_ENCODING_COMP_LZO         = 2,
  MATROSKA_TRACK_ENCODING_COMP_HEADERSTRIP = 3,
} MatroskaTrackEncodingCompAlgo;

typedef enum {
  ZLIB_INFLATE_END_FAIL                    = -2,
  ZLIB_INFLATE_INIT_FAIL                   = -1,
  ZLIB_DEC_OK                              = 0,
} ZlibDecBufErrCode;

typedef struct
{
    uint16_t  FormatTag;
    uint16_t  Channels;
    uint32_t  SamplesPerSec;
    uint32_t  AvgBytesPerSec;
    uint16_t  BlockAlign;
    uint16_t  BitsPerSample;
    uint16_t  Size;
    uint16_t  SamplesPerBlock;

}MkvWaveFormatExStruct;

typedef struct
{
    uint32_t    start_code;
    uint32_t    size;
    uint32_t    timel;
    uint32_t    timeh;
    uint32_t    type;
    uint32_t    slices;
    uint32_t    retFlag;
    uint32_t    res[1];
}MkvMpeg2VideoSteamHead;

typedef struct
{
    uint32_t    headLen;
    uint8_t*    data;

}MkvVC1StreamRCVInfo;

typedef struct
{
    uint32_t    len;
    uint8_t*    data;
    uint8_t     sendFlag;
}MkvVC1ExtraInfo;

struct AMessage;
class String8;

struct DataSourceReader;
struct MatroskaSource;

struct MatroskaExtractor : public MediaExtractor {
    MatroskaExtractor(const sp<DataSource> &source);

    virtual size_t countTracks();

    size_t countSubtitleTracks();

    size_t countAudioTracks();

    virtual sp<MediaSource> getTrack(size_t index);

    virtual sp<MetaData> getTrackMetaData(
            size_t index, uint32_t flags);

    virtual sp<MetaData> getMetaData();
    bool isSelectSubtitleSupport();

    MkvWaveFormatExStruct mWavFormat;
    BuiltInSubDataBuf mSubDataBuf;
    bool mVideoEndFlag;
    Vector<MediaSource*> mSourceQue;
    MediaSource* m_SelectSubtileSrc;

protected:
    virtual ~MatroskaExtractor();

private:
    friend struct MatroskaSource;

    struct TrackInfo {
        unsigned long mTrackNum;
        sp<MetaData> mMeta;
    };
    Vector<TrackInfo> mTracks;

    sp<DataSource> mDataSource;
    DataSourceReader *mReader;
    mkvparser::Segment *mSegment;
    bool mExtractedThumbnails;

    MKVAACDecSpeciInfo dec_specinfo;
    bool mAudioUnSupported;
    bool mVideoUnSupported;
    MkvVC1StreamRCVInfo mVC1RcvHead;
    MkvVC1ExtraInfo mVC1ExtraInfo;
    bool mHaveBuiltInSubtitle;
    Vector<SubtitleTrkInfo> mSubTrackInfo;
    Vector<AudioTrkInfo> mAudioTrackInfo;

    void addVorbisCodecInfo(
        const sp<MetaData> &meta,
        const void *_codecPrivate, size_t codecPrivateSize);

    void addTracks();
    void findThumbnails();
    void processAACDecoderSpecificInfo(const uint8_t* data,
            size_t size,
            const double sampleRate,
            long long channelNum);

    int32_t  MkvAACGetConfig();
    void sendBuiltInSubtitleInfo();
    void sendAudioTrackInfo();
    void selectBuiltInSubtitleTrack(int aIdx);

	ZlibDecBufErrCode zLibDecBuf(uint8_t* srcData,
            size_t srcSize,
            uint8_t** dstData,
            size_t* dstBufSize,
            size_t *dstDataSize);

    void makeVC1RcvHead(uint32_t codecFourCC,
            uint8_t* codecPrivateData,
            uint32_t codecPrivateSize,
            uint32_t width,
            uint32_t height);

    MatroskaExtractor(const MatroskaExtractor &);
    MatroskaExtractor &operator=(const MatroskaExtractor &);
};

bool SniffMatroska(
        const sp<DataSource> &source, String8 *mimeType, float *confidence,
        sp<AMessage> *);

}  // namespace android

#endif  // MATROSKA_EXTRACTOR_H_
