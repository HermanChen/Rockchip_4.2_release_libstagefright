#define LOG_TAG "Video_Mirror_Source"


#include "Video_Mirror_Source.h"
 namespace android {


Video_Mirror_Source::Video_Mirror_Source(int width, int height, int colorFormat)
{
	Video_Source = new UiSource( width,  height,  colorFormat);
}
Video_Mirror_Source::~Video_Mirror_Source() 

{
	
	if(Video_Source)
		delete Video_Source;
}
sp<MetaData> Video_Mirror_Source::getFormat() {
    return Video_Source->getFormat();
}

status_t Video_Mirror_Source::start(MetaData *params) {
	
    return Video_Source->start(params);
	
}

status_t Video_Mirror_Source::stop() {
	
    return Video_Source->stop();
	
}

 status_t Video_Mirror_Source::read(
        MediaBuffer **buffer, const MediaSource::ReadOptions *options) {
	 return Video_Source->read(buffer,options);
 	}
MediaSource* Video_Mirror_Source::createInstanceEx(int width, int height, int colorFormat)
{
	MediaSource* mSource = new Video_Mirror_Source(width,height,colorFormat);
	return mSource;
}
extern "C" MediaSource* openVideo_Mirror_Source(int width, int height, int colorFormat)
{
	return Video_Mirror_Source::createInstanceEx(width,height,colorFormat);
}
}

