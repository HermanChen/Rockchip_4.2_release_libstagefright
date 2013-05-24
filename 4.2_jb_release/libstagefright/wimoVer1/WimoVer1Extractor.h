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

#ifndef WIMOVER1_EXTRACTOR_H_

#define WIMOVER1_EXTRACTOR_H_

#include <media/stagefright/MediaExtractor.h>
#include <utils/Vector.h>

#include "WimoVer1demux.h"

#define WIMOVER1_MAX_AUDIO_WAVFMT_SIZE 20

namespace android {

struct AMessage;
class String8;

struct WimoVer1DataSourceReader;
struct WimoVer1Source;

typedef struct {
    uint16    iVideoWidth;
    uint16    iVideoHeight;
} WimoVer1Format;

typedef struct{
	uint32 iVideoPreTS;
	uint32 iAudioPreTS;
	uint32 iAudioActualTS;
	uint32 iVideoActualTS;
}WimoVer1SeekInfo;

struct WimoVer1Extractor : public MediaExtractor {
    WimoVer1Extractor(const sp<DataSource> &source);

    virtual size_t countTracks();

    virtual sp<MediaSource> getTrack(size_t index);

    virtual sp<MetaData> getTrackMetaData(
            size_t index, uint32 flags);

    virtual sp<MetaData> getMetaData();

protected:
    virtual ~WimoVer1Extractor();

private:
    friend struct WimoVer1Source;

    struct TrackInfo {
        unsigned long mtrackID;
        sp<MetaData> mMeta;
    };

    Vector<TrackInfo> mTracks;

    sp<DataSource> mDataSource;
    WimoVer1DataSourceReader *mReaderHandle;

    bool mExtractedThumbnails;

    void addTracks();
    void findThumbnails();

    WimoVer1Extractor(const WimoVer1Extractor &);
    WimoVer1Extractor &operator=(const WimoVer1Extractor &);

    WimoVer1Demux *wimoVer1Dmx;

    WimoVer1Format sWimoVer1Format;

    uint32 wimoVer1PCMExtraSize;
    uint8 wimoVer1PCMExtraData[WIMOVER1_MAX_AUDIO_WAVFMT_SIZE];

    WimoVer1SeekInfo sSeekInfo;			//time stample information

    WIMO_VER1_FF_FILE *fileHandle;

    bool bWavCodecPrivateSend;
    bool bSuccess;

};

bool SniffWimoVer1(
        const sp<DataSource> &source, String8 *mimeType, float *confidence,
        sp<AMessage> *);

}  // namespace android

#endif  // MPG_EXTRACTOR_H_
