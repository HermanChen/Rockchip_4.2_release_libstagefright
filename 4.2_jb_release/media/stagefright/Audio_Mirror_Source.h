#ifndef _AUDIO_MIRROR_SOURCE_H_
#define _AUDIO_MIRROR_SOURCE_H_


#include <media/stagefright/Audio_OutPut_Source.h>
namespace android {

class Audio_Mirror_Source : public MediaSource {

public:
Audio_Mirror_Source(int32_t sampleRate, int32_t numChannels);

virtual ~Audio_Mirror_Source() ;
virtual status_t start(MetaData *params) ;

virtual status_t stop() ;

virtual sp<MetaData> getFormat() ;

virtual status_t read(
        MediaBuffer **out, const ReadOptions *options) ;

static MediaSource* createInstanceEx(int32_t sampleRate, int32_t numChannels);


private:
    Audio_OutPut_Source *Audio_Source; 
	int reserved[32];
};
}
#endif
