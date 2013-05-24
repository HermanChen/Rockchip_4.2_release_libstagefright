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

#ifndef MPEG4_EXTRACTOR_H_

#define MPEG4_EXTRACTOR_H_

#include <media/stagefright/MediaExtractor.h>
#include <utils/Vector.h>
#include <utils/String8.h>

#define QT_PCM
#define QT_MAX_AUDIO_WAVFMT_SIZE		20

namespace android {

struct AMessage;
class DataSource;
class SampleTable;
class String8;
struct MPEG4Source;
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

class MPEG4Extractor : public MediaExtractor {
public:
    // Extractor assumes ownership of "source".
    MPEG4Extractor(const sp<DataSource> &source);

    virtual size_t countTracks();
    virtual sp<MediaSource> getTrack(size_t index);
    virtual sp<MetaData> getTrackMetaData(size_t index, uint32_t flags);

    virtual sp<MetaData> getMetaData();

    // for DRM
    virtual char* getDrmTrackInfo(size_t trackID, int *len);
#ifdef QT_PCM
    uint32_t audioExtraSize;//add by Charles Chen
    uint8_t audioExtraData[QT_MAX_AUDIO_WAVFMT_SIZE];
    bool bWavCodecPrivateSend;
#endif
protected:
    virtual ~MPEG4Extractor();

private:
    friend struct MPEG4Source;
    struct Track {
        Track *next;
        sp<MetaData> meta;
        uint32_t timescale;
        sp<SampleTable> sampleTable;
        bool includes_expensive_metadata;
        bool skipTrack;
    };

    sp<DataSource> mDataSource;
	bool mHaveMetadata;
    status_t mInitCheck;
    bool mHasVideo;
    long stream_num;
    long long   min_off[16];

    Track *mFirstTrack, *mLastTrack;

    sp<MetaData> mFileMetaData;

    Vector<uint32_t> mPath;
    String8 mLastCommentMean;
    String8 mLastCommentName;
    String8 mLastCommentData;

    status_t readMetaData();
    status_t parseChunk(off64_t *offset, int depth);
    status_t parseMetaData(off64_t offset, size_t size);

    status_t updateAudioTrackInfoFromESDS_MPEG4Audio(
            const void *esds_data, size_t esds_size);

    static status_t verifyTrack(Track *track);

    struct SINF {
        SINF *next;
        uint16_t trackID;
        uint8_t IPMPDescriptorID;
        ssize_t len;
        char *IPMPData;
    };

    SINF *mFirstSINF;

    bool mIsDrm;
    status_t parseDrmSINF(off64_t *offset, off64_t data_offset);

    status_t parseTrackHeader(off64_t data_offset, off64_t data_size);

    Track *findTrackByMimePrefix(const char *mimePrefix);
    int rk_mov_lang_to_iso639(unsigned code, char to[4]);

    MPEG4Extractor(const MPEG4Extractor &);
    MPEG4Extractor &operator=(const MPEG4Extractor &);
};

bool SniffMPEG4(
        const sp<DataSource> &source, String8 *mimeType, float *confidence,
        sp<AMessage> *);

}  // namespace android

#endif  // MPEG4_EXTRACTOR_H_
