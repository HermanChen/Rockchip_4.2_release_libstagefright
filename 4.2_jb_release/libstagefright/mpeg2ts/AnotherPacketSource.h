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

#ifndef ANOTHER_PACKET_SOURCE_H_

#define ANOTHER_PACKET_SOURCE_H_

#include <media/stagefright/foundation/ABase.h>
#include <media/stagefright/MediaSource.h>
#include <utils/threads.h>
#include <utils/List.h>

#include "ATSParser.h"

namespace android {

struct ABuffer;

struct AnotherPacketSource : public MediaSource {
    AnotherPacketSource(const sp<MetaData> &meta);

    void setFormat(const sp<MetaData> &meta);

    virtual status_t start(MetaData *params = NULL);
    virtual status_t stop();
    virtual sp<MetaData> getFormat();

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options = NULL);

    bool hasBufferAvailable(status_t *finalResult);

    int64_t getCurrentPackTime();
		
	int64_t getBufferedDurationUs(status_t *finalResult);

    uint32_t numBufferAvailable(int32_t *mUseMem = NULL);
    status_t nextBufferTime(int64_t *timeUs);

    void queueAccessUnit(const sp<ABuffer> &buffer);

	 void queueAccessUnit(MediaBuffer *buffer);
    void queueDiscontinuity(
            ATSParser::DiscontinuityType type, const sp<AMessage> &extra);

    void signalEOS(status_t result);
    void setLastTime(uint64_t timeus);
	uint32_t  quen_memUsed;
    uint32_t quen_num;
    status_t dequeueAccessUnit(sp<ABuffer> *buffer);
    void clear();
    bool mVideoFlag;
    bool mIsAudio;
    bool discontinuityFlag;
    int32_t mType;
    uint32_t mProgramID;
    unsigned mElementaryPID;
    uint64_t lastTimestamp;
    bool IsAbufferFlag;
protected:
    virtual ~AnotherPacketSource();

private:
    Mutex mLock;
    Condition mCondition;

    sp<MetaData> mFormat;
    List<sp<ABuffer> > mBuffers;
    Vector<MediaBuffer *> mMediaBuffers;
    status_t mEOSResult;

    bool wasFormatChange(int32_t discontinuityType) const;
    DISALLOW_EVIL_CONSTRUCTORS(AnotherPacketSource);
};


}  // namespace android

#endif  // ANOTHER_PACKET_SOURCE_H_
