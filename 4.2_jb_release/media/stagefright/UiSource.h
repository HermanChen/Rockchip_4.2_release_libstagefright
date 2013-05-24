#ifndef _UISOURCE_H_
#define _UISOURCE_H_
#include <binder/IPCThreadState.h>
#include <media/stagefright/AudioSource.h>
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
#include <../../gui/ISurface.h>
#include <utils/Errors.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <media/stagefright/AudioPlayer.h>
#include <media/stagefright/CameraSource.h>
#include <media/stagefright/FileSource.h>
#include <media/stagefright/MediaBufferGroup.h>
//#include <media/stagefright/MediaDebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/MediaExtractor.h>
#include <media/stagefright/MPEG4Writer.h>
#include <media/stagefright/OMXClient.h>
#include <media/stagefright/OMXCodec.h>
#include <media/MediaPlayerInterface.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <../../../../bionic/libc/include/fcntl.h>
#include <../../../../bionic/libc/kernel/common/linux/fb.h>
namespace android {
class UiSource : public MediaSource {

public:
    UiSource(int width, int height, int colorFormat);
    virtual ~UiSource()   ;
    virtual sp<MetaData> getFormat() ;

    virtual status_t start(MetaData *params) ;

    virtual status_t stop() ;

    virtual status_t read( MediaBuffer **buffer, const MediaSource::ReadOptions *options) ;

protected:
    

private:
	UiSource(const UiSource &);
    UiSource &operator=(const UiSource &);
    MediaBufferGroup mGroup;
    int mWidth, mHeight;
    int mColorFormat;
    size_t mSize;
    int64_t mNumFramesOutput;;
    int fd ;
  //  unsigned long temp; 
    unsigned long temp[10];
    FILE* output;
    FILE* input;
    int64_t last_time;
    int last_value;
    void *ui_buf_void[2];	
    int last_imgtype;
    int64_t ui_source_start_time_us;
	int	mStarted;
	int	chip_type;
	int reserved[32];
    
};
}
#endif 
