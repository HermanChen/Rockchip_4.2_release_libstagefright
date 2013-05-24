#ifndef WIMOVER1DEMUX_H_
#define WIMOVER1DEMUX_H_

#include <media/stagefright/DataSource.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/MediaDebug.h>
#include <utils/Vector.h>

#ifndef int8
#define int8	char
#endif

#ifndef int16
#define int16	short
#endif

#ifndef int32
#define int32	int
#endif

#ifndef uint8
#define uint8	unsigned char
#endif

#ifndef uint16
#define uint16 unsigned short
#endif

#ifndef uint32
#define uint32 unsigned int
#endif

#ifndef uint64
#define uint64 long long
#endif

#ifndef oscl_malloc
#define oscl_malloc malloc
#endif

#ifndef oscl_free
#define oscl_free	free
#endif

#ifndef oscl_memcpy
#define oscl_memcpy memcpy
#endif

#ifndef oscl_memset
#define oscl_memset memset
#endif

namespace android {

class WimoVer1Demux;

typedef enum {
    WIMO_VER1_CODEC_ID_UNKNOW = -1,
    WIMO_VER1_CODEC_ID_MJPEG,
    WIMO_VER1_CODEC_ID_WAV,
}WimoCodecId;

typedef enum {
    WIMO_STREAM_TYPE_NONE,
    WIMO_STREAM_TYPE_VIDEO,
    WIMO_STREAM_TYPE_AUDIO,
} WimoStreamType;

typedef enum {
    WIMO_STREAM_IDX_UNKNOW = -1,
    WIMO_STREAM_IDX_VIDEO,
    WIMO_STREAM_IDX_AUDIO,
} WimoStreamIdx;


typedef struct WimoPacket {
    int idx;
    WimoStreamType type;
    uint16 preVideoPktSeq;
    uint16 sequence;
    uint16 preVideoPktFrame_num;
    uint16 frame_num;
	uint16 brokenFlag;
    int size;
    int64_t time;
    uint8* data;
} WimoPacket;

typedef enum {
    WIMO_READ_FRAME_FAIL = -1,
    WIMO_READ_FRAME_SUCCESS = 0,
    WIMO_NO_FRAME_IN_QUE_CACHE,
    WIMO_READ_FRAME_END_OF_STREAM
}WimoReadFrmResult;


/**
* This is a structure for using buffer when reading the files.
*/
typedef struct WimoReadFileBuffer
{
    /* capability of the buffer */
    uint32 capability;

    /* ptr of the buffer*/
    uint8* buf;

    /* current offset of the file */
    uint64 fileOff;

    /* current offset of the buffer */
    uint64 offset;

    /* current buffer offset in file */
    uint64 buf_off_in_file;

    /* current size of data in buffer */
    //uint32 size;

    /* current remain size of data in buffer */
    uint32 remain;

}WimoReadFileBuffer;

typedef struct WimoReadVideoBuffer
{
    /* capability of the buffer */
    uint32 capability;

    /* ptr of the buffer*/
    uint8* buf;

    /* current offset of the buffer */
    uint32 offset;

    /* current size of data in buffer */
    uint32 size;

    /* current buffer data is broken */
    uint32 broken;
}WimoReadVideoBuf;

typedef struct WimoSkipBuffer
{
    /* capability of the buffer */
    uint32 capability;

    /* ptr of the buffer*/
    uint8* buf;
}WimoSkipBuffer;

typedef enum
{
    WIMO_UPDATE_FILE_BUF_FAIL = -1,

    WIMO_UPDATE_FILE_BUF_OK,

    WIMO_UPDATE_FILE_END,

}WimoUpdataErrorCode;

typedef enum {
    WIMO_CHK_FRMEND_ERR = -1,
    WIMO_CHK_FRMEND_NO_END,
    WIMO_CHK_FRMEND_END,
    WIMO_CHK_FRMEND_PES_HDR
} WimoChkFrmEndState;

#ifndef XMEDIA_BITSTREAM_START_CODE
#define XMEDIA_BITSTREAM_START_CODE         (0x42564b52) /* RKVB, rockchip video bitstream */
#endif

#define WIMOVER1_MAX_STREAM_NUM  2

#define BSWAP32(x) \
	((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) | \
      (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))

class IWimoVer1Reader
{
public:
    virtual int Read(long long position, long length, unsigned char* buffer) = 0;
    virtual int Length(long long* total, long long* available) = 0;
    virtual void updatecache(long long off) = 0;
protected:
    virtual ~IWimoVer1Reader();
};

class WIMO_VER1_FF_FILE
{
public:
	WIMO_VER1_FF_FILE(IWimoVer1Reader* pReader);
	~WIMO_VER1_FF_FILE();

    void setDemux(WimoVer1Demux* demux);
	int Seek(long long ofs);
	uint64 Tell();
	int Skip(int size);
	int Read(uint8 *buf, int size);
	int isEOF();
    int64_t getAvailableSize();

    bool checkWimoBufDataReady();

	uint8 ReadByte();
	uint16 Read2Byte();
	uint32 Read3Byte();
	uint32 Read4Byte();

    int skipBytesByRead(int size);
    int storeVideoPktFromWimoData(int size);
    void resetBuffer();
    void setBufferDataIsBroken();
    int getBrokenFlag();

    WimoUpdataErrorCode updateFileBuf();

    uint64 _fileSize;
    WimoReadVideoBuf m_VideoBuf;
private:
    WimoVer1Demux* m_demux;
	IWimoVer1Reader *m_input;
	long long m_offset;
    WimoReadFileBuffer m_FileBuf;
    WimoSkipBuffer m_SkipBuf;
};


typedef struct {
    uint16    FormatTag;
    uint16    Channels;
    uint32    SamplesPerSec;
    uint32    AvgBytesPerSec;
    uint16    BlockAlign;
    uint16    BitsPerSample;
    uint16    cbSize;
    uint16    SamplesPerBlock;
} WimoWAVEFORMATEX;

typedef struct  {
    WimoStreamType streamtype;
    int strmIdx;
    WimoCodecId codecid;
    char streamName[4];
    uint32 start_time;
    uint32 curTime;
	uint32 preTime;
    uint32 duration;
    uint32 rate;
    uint16 bits_per_sample;
    uint16 number_of_audio_channels;
    uint32 start_pos;
    int hdrsize;
    WIMO_VER1_FF_FILE *fp;
    WimoPacket pkt;
} WimoStreamProp;

typedef struct WIMO_Frame_Que_Info{
    Vector<MediaBuffer *> aFrmQue;
    Vector<MediaBuffer *> vFrmQue;
    uint32 vFrmCache;
    uint32 aFrmCache;

}WimoFrameQueInfo;

enum WimoThreadState{
    WIMO_IDLE,
    WIMO_START,
    WIMO_PAUSE,
    WIMO_INSUFFICIENT_DATA,
    WIMO_DATA_READY,
    WIMO_STOP,
};

class WimoVer1Demux{
public:
    WimoVer1Demux(WIMO_VER1_FF_FILE *fileHandle)
    {
        fileHandle->Seek(0);
        fileHandle->setDemux(this);

        m_fileHandle = fileHandle;

        m_curAudioStrNo = -1;
        m_curVideoStrNo = -1;
        m_startTime = 0xFFFFFFFF;
        m_duration = 0;

        m_width = 0;
        m_height = 0;
        m_filesize = 0LL;
        m_prevTime = 0;
        m_frameRate = 0;
        m_actFrmRate = 0;
        m_bitRate = -1;
        m_nbstreams = 0;
        m_frmCnt = 0;

        bSendWavConfig = false;

        m_ThreadState = WIMO_IDLE;

        m_frmQueInfo.aFrmCache = 0;
        m_frmQueInfo.vFrmCache = 0;

        memset(&m_wavehdr, 0, sizeof(WimoWAVEFORMATEX));
        memset(&m_readPkt, 0, sizeof(WimoPacket));
        m_readPkt.preVideoPktSeq = 0xFFFF;

    }

    ~WimoVer1Demux() {}
    int wimo_start();
    void wimo_pause();
    void wimo_stop();
    int wimo_read_header(WIMO_VER1_FF_FILE *fp);
    void wimo_read_close(void);
    int wimo_readFrm(int aTrkId, MediaBuffer **out);

    int wimo_outOneWimoPkt(int* aStrmIdx);
    int wimo_readPkt();
    int checkVideoFrameHeader(uint8_t* buf, uint32_t size, uint8_t** dst, uint32_t* dstSize);
    void wimo_storeVideoFrame(int aStrmIdx, uint32 aPktTime);
    void wimo_storeAudioPkt(int aStrmIdx, uint32 aPktSize, uint32 aPktTime);

    uint32 wimo_getVideoFrmCache()
    {
        Mutex::Autolock autoLock(mLock);
        return m_frmQueInfo.vFrmCache;
    }

    uint32 wimo_getAudioFrmCache()
    {
        Mutex::Autolock autoLock(mLock);
        return m_frmQueInfo.aFrmCache;
    }

    static void* readFrameThread(void* me);
    int frameProcess();
    void flushVideoFrmQue();
    void flushAudioFrmQue();

    inline uint32 get_duration()
    {
        return m_duration - m_startTime;
    }

    inline int32 get_videoHeight(void)
    {
        return m_height;
    }

    inline int32 get_videoWidth(void)
    {
        return m_width;
    }

    inline int32 get_nbStreams()
    {
        return m_nbstreams;
    }

    inline WimoThreadState getDemuxState()
    {
        return m_ThreadState;
    }

    inline void setDemuxState(WimoThreadState aState)
    {
        m_ThreadState = aState;
    }

    /* get stream id, and store in memory address 'ids' */
    int32 get_ID_list(uint32 *ids, int& size);

    WimoWAVEFORMATEX m_wavehdr;

    WimoFrameQueInfo m_frmQueInfo;

private:
    uint32      m_startTime;
    uint32      m_duration;    /* in ms */

    uint32      m_prevTime;

    uint32      m_width;
    uint32      m_height;

    uint64      m_filesize;

    double      m_frameRate;
    double      m_actFrmRate;
    double      m_bitRate;

    int         m_nbstreams;
    WimoStreamProp  m_strProp[WIMOVER1_MAX_STREAM_NUM];
    int         m_curVideoStrNo;
    int         m_curAudioStrNo;

    WIMO_VER1_FF_FILE *m_fileHandle;

    int         m_frmCnt;
    int         iWimoVideoSeekFlag;
    bool        bSendWavConfig;

    uint8       m_AudioType;

    WimoPacket   m_readPkt;

    /* read frame thread */
    pthread_t	mThread;
    WimoThreadState m_ThreadState;

    Mutex mLock;

    int wimoParseWimoPkt(WIMO_VER1_FF_FILE *fp, WimoPacket *pkt);

    inline int getStrmIdxByCodecId(WimoCodecId id);

    inline void wimo_swap16(uint16 *pcode) {*pcode = (*pcode << 8) | (*pcode >> 8);}

    void alternateCode(uint16 *buf, int bufSize);
    int wimo_GenWaveHeader(int streamindex);
};
}
#endif

