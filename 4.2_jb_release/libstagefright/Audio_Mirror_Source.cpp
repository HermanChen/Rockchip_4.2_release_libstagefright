#include <media/stagefright/Audio_Mirror_Source.h>

#define LOG_TAG "Audio_Mirror_Source"
#define MIRRORING_DEBUG

namespace android {

extern FILE *test_ts_txt;



Audio_Mirror_Source::Audio_Mirror_Source(int32_t sampleRate, int32_t numChannels)
{
	Audio_Source = new Audio_OutPut_Source(sampleRate,numChannels);
    
}

 Audio_Mirror_Source::~Audio_Mirror_Source() {
    
   

	if(Audio_Source)
		delete Audio_Source;
	
}

status_t Audio_Mirror_Source::start(MetaData *params) {
	
	
	return Audio_Source->start(params);
    

}

status_t Audio_Mirror_Source::stop() {
	return Audio_Source->stop();
}

sp<MetaData> Audio_Mirror_Source::getFormat() {
   

    return Audio_Source->getFormat();
}


status_t Audio_Mirror_Source::read(
        MediaBuffer **out, const ReadOptions *options) {
	
    return Audio_Source->read(out,options);
}


MediaSource* Audio_Mirror_Source::createInstanceEx(int32_t sampleRate, int32_t numChannels)
{
	MediaSource* mSource = new Audio_Mirror_Source(sampleRate,numChannels);
	return mSource;
}
extern "C" MediaSource* openAudio_Mirror_Source(int32_t sampleRate, int32_t numChannels)
{
	return Audio_Mirror_Source::createInstanceEx(sampleRate,numChannels);
}



}
