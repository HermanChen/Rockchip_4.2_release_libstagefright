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

#ifndef CMOV_EXTRACTOR_H_

#define CMOV_EXTRACTOR_H_

#include <media/stagefright/MediaExtractor.h>
#include <utils/Vector.h>
#include <media/stagefright/Utils.h>
#include "ffmpg_api.h"
#include "utils/Log.h"
#include "xmedia_demuxers.h"

namespace android {

struct AMessage;
class String8;
struct CmovDataSourceReader;
struct CmovSource;
struct CmovExtractor : public MediaExtractor {
    CmovExtractor(const sp<DataSource> &source);

    virtual size_t countTracks();

    virtual sp<MediaSource> getTrack(size_t index);

    virtual sp<MetaData> getTrackMetaData(
            size_t index, uint32_t flags);

    virtual sp<MetaData> getMetaData();

    uint32 audioExtraSize;//add by Charles Chen
    uint8 audioExtraData[AVI_MAX_AUDIO_WAVFMT_SIZE];
    bool bWavCodecPrivateSend;

    WmaInfo      mWmaHead;
protected:
    virtual ~CmovExtractor();

private:
    friend struct CmovSource;

    struct TrackInfo {
        unsigned long mTrackNum;
        sp<MetaData> mMeta;
    };

    Vector<TrackInfo> mTracks;

    sp<DataSource> mDataSource;
    CmovDataSourceReader *mReaderVideo;
    CmovDataSourceReader *mReaderAudio;
    bool mExtractedThumbnails;
    bool _success;
    uint8* pVideoStrfData;
    WmaFormat    mWmaFormat;
    void addTracks();
    AVFormatContext *ic_video;
    AVFormatContext *ic_audio;
    int64_t fileoffset;
    int32_t Streamindex[MAX_STREAMS];
    CmovExtractor(const CmovExtractor &);
    CmovExtractor &operator=(const CmovExtractor &);
};

bool SniffCmov(
        const sp<DataSource> &source, String8 *mimeType, float *confidence,
        sp<AMessage> *);

}  // namespace android

#endif  //CMOV_EXTRACTOR_H_
