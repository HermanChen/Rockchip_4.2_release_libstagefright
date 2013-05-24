#ifndef FRAMEQUEUEMANAGE_H_
#define FRAMEQUEUEMANAGE_H_

#include "vpu_global.h"
#include <media/MediaPlayerInterface.h>
#include <utils/threads.h>
#include <utils/List.h>
#include "ffmpg/include/dvdsubdec.h"
#include "include/rk29-ipp.h"
#include <fcntl.h>
#include <poll.h>

#define CACHE_NUM 6

namespace android {

struct MediaBuffer;

/*
 ** for support Live WallPaper, embeded subtitle and so on,
 ** must sync with VideoDisplayView.java, also note that
 ** do not conflict with MediaPlayer.h.
*/
enum awesome_player_media_para_keys {
    KEY_PARAMETER_SET_LIVE_WALL_PAPER = 1998,               // set only
	KEY_PARAMETER_TIMED_TEXT_FRAME_DURATION_MS = 1999,      // get only

	KEY_PARAMETER_SET_VIDEO_CODEC_NOT_SUPPORT  = 2100,      // set only
	KEY_PARAMETER_SET_AUDIO_CODEC_NOT_SUPPORT  = 2101,      // set only
	KEY_PARAMETER_SET_BROWSER_REQUEST = 3188,               // set only
};

enum awesome_player_media_config_map {
    LIVE_WALL_PAPER_ENABLE             = 0x01,
    BROWSER_REQUEST_ENABLE             = 0x02,
    VIDEO_CODEC_DISABLE                = 0x04,
    AUDIO_CODEC_DISABLE                = 0x08,
};

typedef struct AwesomePlayerExtion{
    uint32_t flag;
    bool isBuffering;

}AwesomePlayerExt_t;


enum subtitle_msg_type {
    // 0xx
    SUBTITLE_MSG_UNKNOWN = -1,
    // 1xx
    SUBTITLE_MSG_VOBSUB_FLAG = 1,
    // 2xx
    SUBTITLE_MSG_VOBSUB_GET = 2,
};

typedef enum {
    FRM_QUE_MAN_INIT_FAIL = -1,
    FRM_QUE_MAN_OK = 0,
} FrmQueManStatus;
typedef struct FrmQueManContext {
    void* obj;
    int (*display_proc)(void* aContextObj);
}FrmQueManContext;
struct FrameQueue
{
	FrameQueue	*next;
	MediaBuffer		*info;
	int64_t		time;
	FrameQueue(MediaBuffer *data, int64_t tr = 0);
	~FrameQueue();
};

#define USE_DEINTERLACE_DEV         1

struct deinterlace_dev
{
    #define USING_IPP       (0)
    #define USING_PP        (1)
    #define USING_NULL      (-1)
    deinterlace_dev();
    ~deinterlace_dev();

    status_t dev_status;
    int dev_fd;
    void *priv_data;
    status_t perform(VPU_FRAME *frm, uint32_t bypass);
    status_t sync();
    status_t status();
    status_t test();
};

struct FrameQueueManage : public RefBase
{
	FrameQueue	*pstart;
	FrameQueue	*pend;
	FrameQueue	*pdisplay;
	FrameQueue	*plastdisplay;
	MediaBuffer *pdeInterlaceBuffer;
	pthread_t	mThread;
	bool		mThread_sistarted;
	bool		stopFlag;
	bool		run;
	bool        deintpollFlag;
	int32_t     deintFlag;
	size_t		num;
	size_t		numdeintlace;
	Mutex		mLock;
#if USE_DEINTERLACE_DEV
    deinterlace_dev *deint;
#else
	int ipp_fd;
	struct pollfd fd;
#endif
	MediaBuffer *OrignMediaBuffer;
    uint32_t mCurrentSubIsVobSub;
    bool        mHaveReadOneFrm;

    AwesomePlayerExt_t mPlayerExtCfg;

    FrmQueManContext sFrmQueContext;
	FrameQueueManage();
	~FrameQueueManage();
    FrmQueManStatus initFrmQueManContext(FrmQueManContext* aContext);
	int pushframe(FrameQueue *frame);
	Vector<MediaBuffer *> DeInterlaceFrame;
    Vector<void *> mVobSubPicQue;
	int pushDeInterlaceframe(MediaBuffer* frame);
	int deleteframe(int64_t curtime);
    void pushVobSubPic(SubPicture* pic);
    void flushVobSubQueue();
    void blendVobSubtitle(MediaBuffer* videoBuf, int64_t timeUs);
	void DeinterlacePoll();
	int cache_full()
	{
    	Mutex::Autolock autoLock(mLock);
	    if(deintFlag)
	    {
	         return (numdeintlace + num) >= CACHE_NUM;
	    }
		return num >= CACHE_NUM;
	}
	void updateDisplayframe();
	FrameQueue *getNextDiplayframe();
	MediaBuffer *getNextFrameDinterlace();
	FrameQueue *popframe();
	void start();
	void stop(void);
	void play();
	void pause();
	bool runstate();
	void flushframes(bool end);
	int Displayframe(void *ptr);
	void DeinterlaceProc();
	static void* Threadproc(void *me);

    void subtitleNotify(int msg, void* obj);
};
}
#endif  // FRAMEQUEUEMANAGE_H_

