#ifndef _VIDEO_MIRROR_SOURCE_H_
#define _VIDEO_MIRROR_SOURCE_H_



#include <media/stagefright/UiSource.h>
namespace android {
class Video_Mirror_Source : public MediaSource {

public:
    Video_Mirror_Source(int width, int height, int colorFormat);
    virtual ~Video_Mirror_Source()   ;
    virtual sp<MetaData> getFormat() ;

    virtual status_t start(MetaData *params) ;

    virtual status_t stop() ;

    virtual status_t read( MediaBuffer **buffer, const MediaSource::ReadOptions *options) ;
	 static MediaSource* createInstanceEx(int width, int height, int colorFormat);

    

private:
    
    UiSource *Video_Source;
	int	reserved[32];
};
}
#endif
