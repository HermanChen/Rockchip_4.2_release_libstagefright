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

#ifndef A_TS_PARSER_H_

#define A_TS_PARSER_H_

#include <sys/types.h>

#include <media/stagefright/foundation/ABase.h>
#include <media/stagefright/foundation/AMessage.h>
#include <utils/KeyedVector.h>
#include <utils/Vector.h>
#include <utils/RefBase.h>

namespace android {

struct ABitReader;
struct ABuffer;
struct MediaSource;

struct ATSParser : public RefBase {
    enum DiscontinuityType {
        DISCONTINUITY_NONE              = 0,
        DISCONTINUITY_TIME              = 1,
        DISCONTINUITY_AUDIO_FORMAT      = 2,
        DISCONTINUITY_VIDEO_FORMAT      = 4,
        DISCONTINUITY_ABSOLUTE_TIME     = 8,

        DISCONTINUITY_SEEK              = DISCONTINUITY_TIME,

        // For legacy reasons this also implies a time discontinuity.
        DISCONTINUITY_FORMATCHANGE      =
            DISCONTINUITY_AUDIO_FORMAT
                | DISCONTINUITY_VIDEO_FORMAT
                | DISCONTINUITY_TIME,
       DISCONTINUITY_PLUSTIME              = 16,
    };

    enum Flags {
        // The 90kHz clock (PTS/DTS) is absolute, i.e. PTS=0 corresponds to
        // a media time of 0.
        // If this flag is _not_ specified, the first PTS encountered in a
        // program of this stream will be assumed to correspond to media time 0
        // instead.
        TS_TIMESTAMPS_ARE_ABSOLUTE = 1,
        // Video PES packets contain exactly one (aligned) access unit.
        ALIGNED_VIDEO_DATA         = 2,
    };

    ATSParser(uint32_t flags = 0);
    void set_player_type(int type);

    status_t feedTSPacket(const void *data, size_t size,uint32_t seekflag);

    status_t feedTSPacket(const void *data, size_t size);
    void signalDiscontinuity(
            DiscontinuityType type, const sp<AMessage> &extra);

    void signalEOS(status_t finalResult);
	void signalSeek();
    void createLiveProgramID(unsigned AudioPID,unsigned AudioType,unsigned VideoPID,unsigned VideoType);
    enum SourceType {
        VIDEO,
        AUDIO
    };
    sp<MediaSource> getSource(SourceType type,uint32_t& ProgramID,unsigned& elementaryPID);

    sp<MediaSource> getSource(SourceType type);

    int64_t getTimeus(uint32_t ProgramID,unsigned elementaryPID);
    void Start(unsigned AudioPID,unsigned VideoPID);
    bool PTSTimeDeltaEstablished();
    Vector<int32_t> mPIDbuffer;



    enum {
        // From ISO/IEC 13818-1: 2000 (E), Table 2-29
        STREAMTYPE_RESERVED             = 0x00,
        STREAMTYPE_MPEG1_VIDEO          = 0x01,
        STREAMTYPE_MPEG2_VIDEO          = 0x02,
        STREAMTYPE_MPEG1_AUDIO          = 0x03,
        STREAMTYPE_MPEG2_AUDIO          = 0x04,
        STREAMTYPE_MPEG2_AUDIO_ADTS     = 0x0f,
		STREAMTYPE_MPEG4_VIDEO          = 0x10,
		STREAMTYPE_MPEG2_AUDIO_LATM     = 0x11,
        STREAMTYPE_H264                 = 0x1b,
		STREAMTYPE_AC3                  = 0x81,
		STREAMTYPE_TruHD                = 0x83,
		STREAMTYPE_PCM_AUDIO            = 0x80,
		STREAMTYPE_DTS                  = 0x7b,
		STREAMTYPE_DTS1                 = 0x82,
		STREAMTYPE_DTS2                 = 0x8a,
		STREAMTYPE_DTS_HD               = 0x85,
		STREAMTYPE_DTS_HD_MASTER        = 0x86,
		STREAMTYPE_VC1                  = 0xea,
		STREAMTYPE_PCM                  = 0x80
    };

protected:
    virtual ~ATSParser();
private:
    struct Program;
    struct Stream;
    struct PSISection;

    uint32_t mFlags;
#ifdef TS_DEBUG
    FILE * fp;
#endif
    Vector<sp<Program> > mPrograms;
    KeyedVector<unsigned, sp<PSISection> > mPSISections;

    int64_t mAbsoluteTimeAnchorUs;

    size_t mNumTSPacketsParsed;

    void parseProgramAssociationTable(ABitReader *br);
    void parseProgramMap(ABitReader *br);
    void parsePES(ABitReader *br);
    size_t kTSPacketSize;
    uint32_t seekFlag;
    unsigned mAudioPID;
    unsigned mVideoPID;
    bool playStart;
    int   player_type;
    status_t parsePID(
        ABitReader *br, unsigned PID,
        unsigned continuity_counter,
        unsigned payload_unit_start_indicator);

    void parseAdaptationField(ABitReader *br, unsigned PID);
    status_t parseTS(ABitReader *br);

    void updatePCR(unsigned PID, uint64_t PCR, size_t byteOffsetFromStart);

    uint64_t mPCR[2];
    size_t mPCRBytes[2];
    int64_t mSystemTimeUs[2];
    size_t mNumPCRs;

    DISALLOW_EVIL_CONSTRUCTORS(ATSParser);
};

}  // namespace android

#endif  // A_TS_PARSER_H_
