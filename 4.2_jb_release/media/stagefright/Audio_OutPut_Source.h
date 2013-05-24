#ifndef _AUDIO_OUTPUT_SOURCE_H_
#define _AUDIO_OUTPUT_SOURCE_H_
#include <binder/IPCThreadState.h>
#include <media/stagefright/AMRWriter.h>
#include <media/stagefright/CameraSource.h>
#include <media/stagefright/MPEG2TSWriter.h>
#include <media/stagefright/MPEG4Writer.h>
//#include <media/stagefright/MediaDebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/OMXClient.h>
#include <media/stagefright/OMXCodec.h>
#include <media/MediaProfiles.h>
#include <camera/ICamera.h>
#include <camera/Camera.h>
#include <camera/CameraParameters.h>
#include <utils/Errors.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <media/stagefright/AudioPlayer.h>
#include <media/stagefright/CameraSource.h>
#include <media/stagefright/FileSource.h>
#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/MediaExtractor.h>
#include <media/stagefright/MPEG4Writer.h>
#include <media/stagefright/OMXClient.h>
#include <media/stagefright/OMXCodec.h>
#include <media/MediaPlayerInterface.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <../../../../bionic/libc/include/fcntl.h>
#include <../../../../bionic/libc/kernel/common/linux/fb.h>
#include <sys/un.h>
namespace android {
#define MIRRORING_IOCTL_MAGIC 0x60
#define MIRRORING_START				_IOW(MIRRORING_IOCTL_MAGIC, 0x1, unsigned int)
#define MIRRORING_STOP				_IOW(MIRRORING_IOCTL_MAGIC, 0x2, unsigned int)
#define MIRRORING_SET_ROTATION			_IOW(MIRRORING_IOCTL_MAGIC, 0x3, unsigned int)
#define MIRRORING_DEVICE_OKAY			_IOW(MIRRORING_IOCTL_MAGIC, 0x4, unsigned int)
#define MIRRORING_SET_TIME			_IOW(MIRRORING_IOCTL_MAGIC, 0x5, unsigned int)
#define MIRRORING_GET_TIME			_IOW(MIRRORING_IOCTL_MAGIC, 0x6, unsigned int)

#define MIRRORING_VIDEO_OPEN				_IOW(MIRRORING_IOCTL_MAGIC, 0x11, unsigned int)
#define MIRRORING_VIDEO_CLOSE			_IOW(MIRRORING_IOCTL_MAGIC, 0x12, unsigned int)
#define MIRRORING_VIDEO_GET_BUF			_IOW(MIRRORING_IOCTL_MAGIC, 0x13, unsigned int)


#define MIRRORING_AUDIO_OPEN           	      	_IOW(MIRRORING_IOCTL_MAGIC, 0x21, unsigned int)
#define MIRRORING_AUDIO_CLOSE                	_IOW(MIRRORING_IOCTL_MAGIC, 0x22, unsigned int)
#define MIRRORING_AUDIO_GET_BUF            		_IOW(MIRRORING_IOCTL_MAGIC, 0x23, unsigned int)
#define MIRRORING_AUDIO_SET_PARA                     _IOW(MIRRORING_IOCTL_MAGIC, 0x24, unsigned int)
#define	MIRRORING_AUDIO_SET_VOL			_IOW(MIRRORING_IOCTL_MAGIC, 0x25, unsigned int)


#define	MIRRORING_AUDIO_SET_BUFFER_SIZE		0xa1
#define	MIRRORING_AUDIO_SET_BYTEPERFRAME		0xa2


#define MIRRORING_COUNT_ZERO				-111
#define VIDEO_ENCODER_CLOSED			-222
#define AUDIO_ENCODER_CLOSED			-222
#define ENCODER_BUFFER_FULL			-333


#define MIRRORING_IOCTL_ERROR			-1111
struct wimo_audio_param{
	unsigned long			buffer_size;
	unsigned long			nSamplePerFrame;
	unsigned long			nChannel;
	unsigned long			nBytePerSample;
	unsigned long			nBytePerFrame;
	unsigned long			Reserverd[7];
};
class Audio_OutPut_Source : public MediaSource {

public:
Audio_OutPut_Source(int32_t sampleRate, int32_t numChannels);

virtual ~Audio_OutPut_Source() ;

status_t start(MetaData *params) ;

status_t stop() ;

sp<MetaData> getFormat() ;

status_t read(
        MediaBuffer **out, const ReadOptions *options) ;


    void* ThreadWrapper(void *);
	static void* rec_data(void* me);



private:
    int	kBufferSize;
    static const double kFrequency = 500.0;
     long mOffset;
    long mLength;
    long file_offset;
    Mutex mLock;
    int64_t start_data_time_us ;
    bool mStarted;
    int32_t mSampleRate;
    int32_t mNumChannels;
    size_t mPhase;
	
	int			buffer_size;
	int			nBytePerFrame_src;
	int			nSamplePerFrame_src;
	size_t 		frameSize;
	size_t 		numFramesPerBuffer;
	FILE* 		audio_input;
	FILE* 		audio_input_test;
	MediaBufferGroup mGroup;
	int				is_last_has_data;
	unsigned char		*Input_Buf;
	unsigned char		*audio_enc_buf;
	int			fd;
	int64_t last_pcm_timestamp;
	int reserved[32];
   
	};
}
#endif
