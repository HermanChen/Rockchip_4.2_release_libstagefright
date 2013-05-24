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

#ifndef MIRRORING_WRITER_H_

#define MIRRORING_WRITER_H_

#define AUTO_NETWORK_ADAPTER
#include <media/stagefright/foundation/ABase.h>
#include <media/stagefright/foundation/AHandlerReflector.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/MediaWriter.h>
#include <media/stagefright/SenderSource.h>
#include <../../../media/libstagefright/include/AVCEncoder.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <linux/tcp.h>

namespace android {

struct ABuffer;
typedef struct
{
   int width;
   int height;
   int rc_mode;
   int bitRate;
   int framerate;
	int	qp;
	int  enableCabac;
	int  cabacinitIdc;
	int  reserved[8];

}AVCEncParams;
typedef struct {
	int64_t now;
	int64_t old;
	int64_t delt;
}CalcData;
typedef struct {
	CalcData post;
	CalcData send;
	CalcData postAvcFrame;
	int64_t curSendTimeus;
	int64_t curSystemTimeUs;
	int64_t curPostTimeus;
	int64_t deltPostTimeus;
	int lastPostDataSize;
	int64_t timeDelay;
	int64_t delt_length;
	int64_t avgSendRate;
	int64_t sendRates[30];
	int sendRatePos;
	int64_t historyBitrate[10];
	int HBPos;//historyBitratePos
	int discard_flag;
	int desktopFlag;
	int64_t lastBitrate;
	int lastQP;
	int setIFrameFlag;
	int discardNeedIframe_flag;
}AutoAdapterData;
struct MIRRORINGWriter : public MediaWriter {

    MIRRORINGWriter(unsigned long addr, unsigned short local_port,unsigned short remort_port);//,unsigned a);


    virtual status_t addSource(const sp<MediaSource> &source);
    virtual status_t start(MetaData *param = NULL);
    virtual status_t stop();
    virtual status_t pause();
    virtual bool reachedEOS();
    virtual status_t dump(int fd, const Vector<String16>& args);

    void onMessageReceived(const sp<AMessage> &msg);
	int		HasVideo(){return (mAVCEncoder == NULL) ? 0 : 1;};
	static MediaWriter* createInstanceEx(unsigned long addr, unsigned short local_port,unsigned short remort_port);

protected:
    virtual ~MIRRORINGWriter();
private:
    enum {
        kWhatSourceNotify = 'noti'
    };
	void init();

    void writeTS();
    void writeProgramAssociationTable();
    void writeProgramMap();
    void writeAccessUnit(int32_t sourceIndex, const sp<ABuffer> &buffer);

#ifdef AUTO_NETWORK_ADAPTER
	static void *ThreadWrapper(void *me);
	void* decideByUnsendData();
	int64_t calcDeltData();
	int64_t calculateAvgSendRate();
	float calculateSendIncrease(int pos);
	static int isSyncFrame(sp<ABuffer> *buffer);
	status_t setAvcencBitRate(int64_t bitrate);
	float calculateEncRatePara(int bitrate);
	int calcMaxBitrate (int db);
	void* decideByDelayTime();
	int isDelayBetter(int64_t *delays); 
	int isDelayWorse(int64_t *delays, int pos);
#endif
    ssize_t internalWrite(const void *data, size_t size,int AudioStream=0);
    struct SourceInfo;

    FILE *mFile;
    FILE *cmpFile;
    void *mWriteCookie;
    ssize_t (*mWriteFunc)(void *cookie, const void *data, size_t size);

    sp<ALooper> mLooper;
    sp<ALooper> mLooper_Audio;
    sp<AHandlerReflector<MIRRORINGWriter> > mReflector;
    sp<AHandlerReflector<MIRRORINGWriter> > mReflector_Audio;
	SenderSource *mSenderSource;
	unsigned long rx_addr;
	unsigned short tx_port;
	unsigned short rx_port;
	unsigned short rx_flag;
	int		ts_packet_num;
	int	mWriter_Type;
    int packet_num;
    uint8_t* udp_buffer;
    uint8_t* sender_buffer[2];
    bool mStarted;
	int64_t start_time;
	bool end_flag;
	bool connect_flag;
	int Snd_drpo_flag;
    unsigned char programmap_asso[376];
    unsigned int access_unit_num;	
    Vector<sp<SourceInfo> > mSources;
    size_t mNumSourcesDone;

    int64_t mNumTSPacketsWritten;
    int64_t mNumTSPacketsBeforeMeta;

	AVCEncoder *mAVCEncoder;
	AVCEncParams mAVCEncParams;

	
	int	reserved[32];


    DISALLOW_EVIL_CONSTRUCTORS(MIRRORINGWriter);
};

}  // namespace android

#endif  // MPEG2TS_WRITER_H_
