/*
 * Copyright (C) 2009 The Android Open Source Project
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

#include "WMAPRODecoder.h"



#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MetaData.h>

#define OUTPUT_BUFFER_SIZE_WMAPRO (1024*1024*3)


#undef LOG_TAG
#define LOG_TAG "WMAProDecoder"

//#define WFO
//#define WFPCM

namespace android {

WMAPRODecoder::WMAPRODecoder(const sp<MediaSource> &source)
    : mSource(source),
    mNumChannels(0),
    mSampleRate(0),
    mStarted(false),
    mInitFlag(0),
    mBufferGroup(NULL),
    mAnchorTimeUs(0),
    mNumFramesOutput(0),
    mInputBuffer(NULL),
    mFirstOutputBuf(NULL),
    mReAsynTime(0LL),
    mReAsynThreshHold(0LL),
    lib(NULL),
    InitWmaPro(NULL),
    EndWmaPro(NULL),
    SetStreamNoPro(NULL),
    DecodeWmaPro(NULL),
    hasvideo(0),
    first(1),
    donot(1){
	count = 0;
    init();
}

int  WMAPRODecoder::libinit() {
    const char *libpath="librkwmapro.so";

    if(lib)
        return 0;

    lib=dlopen(libpath, RTLD_NOW | RTLD_GLOBAL);

    if(lib==NULL)
    {
        ALOGE("open lib error \n");
        return  -1;
    }

    InitWmaPro = (InitWmaProStr) dlsym(lib, "InitWmaPro");
    DecodeWmaPro = (DecodeWmaProStr) dlsym(lib, "DecodeWmaPro");
    EndWmaPro = (EndWmaProStr) dlsym(lib, "EndWmaPro");
    SetStreamNoPro = (SetStreamNoStr)dlsym(lib,"SetAudioStreamNo");

    if(!InitWmaPro || !DecodeWmaPro || !EndWmaPro || !SetStreamNoPro)
    {
        ALOGE("Find function in %s lib is error \n",libpath);
        return -2;
    }

    return 0;


}
void WMAPRODecoder::libuninit()
{
    if(lib)
        dlclose(lib);
}

void WMAPRODecoder::init() {
    int pk;
    int streamId = 0;

    libinit();


    sp<MetaData> srcFormat = mSource->getFormat();

    CHECK(srcFormat->findInt32(kKeyChannelCount, &mNumChannels));
    CHECK(srcFormat->findInt32(kKeySampleRate, &mSampleRate));

    mMeta = new MetaData;
    mMeta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_AUDIO_RAW);

    if(mNumChannels>2)
    {
        ALOGD("mNumChannels =%d  modify channels by hl \n",mNumChannels);
        mNumChannels = 2;
    }

    mMeta->setInt32(kKeyChannelCount, mNumChannels);
    mMeta->setInt32(kKeySampleRate, mSampleRate);

    int64_t durationUs;
    if (srcFormat->findInt64(kKeyDuration, &durationUs)) {
        mMeta->setInt64(kKeyDuration, durationUs);
    }

	//	LOGD("channels=%d ,SampleRate=%d \n",mNumChannels,mSampleRate);

    status_t err;
    unsigned char *p8;
    int  *p32;
	err = mSource->read(&mInputBuffer, NULL);
	p8 = (unsigned char *)(mInputBuffer->data());
	p32 = ( int *)p8;
	p8+=4;
	int len=0;
	int res=-1;

    int32_t wmvTrackCnt = 0;
    CHECK(srcFormat->findInt32(kKeyWmvTrackCnt, &wmvTrackCnt));
    CHECK(srcFormat->findInt32(kKeyWma3FirstPos, &first));

	hasvideo =(wmvTrackCnt==2 )?1:0;

	ALOGD("Start Call InitWmaPro,hasvideo is %d  ,first =%d \n",hasvideo,first);
    if(hasvideo)
    {
        pk = first;
    }
    else
    {
        pk = *p32;
    }
    ALOGE("FIRST = %d ,*p32= %d \N",first,*p32);

    if (srcFormat->findInt32(kKeyWma3AudioStreamId, &streamId)) {
    	res = (*InitWmaPro)(p8,pk,&len,&codecid);
        	  (*SetStreamNoPro)(streamId, codecid);
    } else {
    	res = (*InitWmaPro)(p8,pk,&len,&codecid);
        	  (*SetStreamNoPro)(1, codecid);
    }

/*
#ifdef WFO
    FILE *fp = NULL;
    fp = fopen("/data/data/test.bin","ab+");
    if(!fp)
    {
        LOGD("create file error \n");
        return ;
    }

    int  helunsize = fwrite(p8,1,pk,fp);
    fclose(fp);
    LOGE("Write header is %d/%d \n",helunsize,pk);
#endif
*/

	ALOGD("End Call InitWmaPro res=%d,len=%d\n",res,len);

	mMeta->setCString(kKeyDecoderComponent, "WMAPRODecoder");

	if(hasvideo)
	{

	if(first == *p32)
	{
	 mInputBuffer->release();
        mInputBuffer=NULL;
	ALOGE("FIRST mInputBuffer\n");
	}
	else {
    	*p32 = *p32-pk;
    	memmove(p8,p8+pk,*p32);
    	mInputBuffer->set_range(0,*p32+4);
		}

	}
	else
    {
        mInputBuffer->release();
        mInputBuffer=NULL;
    }

}

WMAPRODecoder::~WMAPRODecoder() {
    if (mStarted) {
        stop();
    }

	(*EndWmaPro)(codecid);
	libuninit();
}

status_t WMAPRODecoder::start(MetaData *params) {
    CHECK(!mStarted);

    if(mSampleRate>48000)
    {
        ALOGE(" Samplerate is %d ,Not Support \n",mSampleRate);
        return UNKNOWN_ERROR;
    }

    mBufferGroup = new MediaBufferGroup;
    mBufferGroup->add_buffer(new MediaBuffer(OUTPUT_BUFFER_SIZE_WMAPRO));
	mSource->start();
    mAnchorTimeUs = 0;
    mNumFramesOutput = 0;
    mStarted = true;

    return OK;
}

status_t WMAPRODecoder::stop() {
    CHECK(mStarted);

    if (mInputBuffer) {
        mInputBuffer->release();
        mInputBuffer = NULL;
    }

    delete mBufferGroup;
    mBufferGroup = NULL;

    mSource->stop();

    mStarted = false;

    return OK;
}

sp<MetaData> WMAPRODecoder::getFormat() {
    return mMeta;
}

status_t WMAPRODecoder::read(
        MediaBuffer **out, const ReadOptions *options) {
    status_t err;
    static int j=0;
    j++;
	count++;
	*out = NULL;
	if(mFirstOutputBuf)
	{
		*out = mFirstOutputBuf;
		mFirstOutputBuf = NULL;
		return OK;
	}
    int64_t seekTimeUs;
    ReadOptions::SeekMode mode;

    if (options && options->getSeekTo(&seekTimeUs, &mode)) {
        CHECK(seekTimeUs >= 0);
        //when the user seek the video,then will entry into this case,so you must do some reset fun in this case
        //by Charles Chen at Feb,11th ,2011
        mNumFramesOutput = 0;

        //	LOGD("j=%d,us = %ld,ms= %ld ,s=%ld,mode=%ld \n",j,seekTimeUs,seekTimeUs/1000,seekTimeUs/1000000,mode);
        ALOGD("seek time is %lld in %s  \n",seekTimeUs,__FUNCTION__);

        if (mInputBuffer) {
            mInputBuffer->release();
            mInputBuffer = NULL;
        }

        mAnchorTimeUs = -1LL;//-1 mean after seek ,need Assignment first frame time after seek

    } else {
        seekTimeUs = -1;
    }

    if (mInputBuffer == NULL) {
    	status_t err1;

    	err = mSource->read(&mInputBuffer, options);

        if (err != OK ) {
            //LOGE("j=%d,mSource ->read error :%d \n",j,err);
            return err;
        }

#ifdef WFO
        unsigned char *p81;
        int  *p321;
        p81 = (unsigned char *)(mInputBuffer->data());
        p321 = ( int *)p81;
        p81 += 4;

        FILE *fp = NULL;
        fp = fopen("/data/data/test.bin","ab+");
        if(!fp)
        {
        ALOGE("create file error \n");
        return -1 ;
        }
        int helunsize = fwrite(p81,1,*p321,fp);
        fclose(fp);
        ALOGE("Write data is  %d/%d  count=%d\n",helunsize,*p321,count);
#endif

        int64_t timeUs;
        if (mInputBuffer->meta_data()->findInt64(kKeyTime, &timeUs)) {
            //if the audio codec can't make sure that one in one out,will not change the mAnchorTimeUs/mNumFramesOutput value
            //you could add the tmp time para Charles Chen Feb,11th ,2011
            //	LOGD("j=%d,us =%ld ,ms=%ld ,s=%ld\n",j,timeUs,timeUs/1000,timeUs/1000000);
            if(mAnchorTimeUs == -1LL)
            {
                mAnchorTimeUs = timeUs;
                mNumFramesOutput = 0;
                mReAsynThreshHold = timeUs;
            }
            mReAsynTime = timeUs;
        } else {
            // We must have a new timestamp after seeking.
            CHECK(seekTimeUs < 0);
        }
    }

    MediaBuffer *buffer;
    mBufferGroup->acquire_buffer(&buffer);

	int32_t inputUsedLen = mInputBuffer->range_offset();
	unsigned char *outbuf=(unsigned char *)(buffer->data());
	unsigned char *inbuf=(unsigned char *)(mInputBuffer->data());
	int *p32=(int *)inbuf;
	int inlen=*p32;
	int outlen=0;
	int res=-1;
	int writelen=0;
	ALOGV("count = %d ,inlen = %d \n",count,inlen);
	res = (*DecodeWmaPro)(inbuf+4,inlen,outbuf,&outlen,&writelen,codecid);

#ifdef WFPCM
    FILE *fp = NULL;
    fp = fopen("/data/data/pcm.pcm","ab+");
    if(!fp)
    {
        ALOGD("create file error \n");
        return  0;
    }
    fwrite(outbuf,1,outlen,fp);
   ALOGE("Write pcm is %d ,count = %d \n",outlen,count);
    fclose(fp);
#endif

	//LOGD("j=%d,res=%d,outlen=%d,inlen=%d,writelen=%d,codecid=%d \n",j,res,outlen,inlen,writelen,codecid);


	int32_t aOutputLength = 0;
	int32_t retValue = 0;
	mInputBuffer->release();
	mInputBuffer = NULL;

	buffer->set_range(0, outlen);//the aOutPutLen is the R/L totle sampleNum

   	buffer->meta_data()->setInt64(
            kKeyTime,
            mAnchorTimeUs
                + (mNumFramesOutput * 1000000) / mSampleRate);
	//LOGE("j=%d,numFrmaeOutput=%lld,outlen=%d,audio output time is: %lld us",j, mNumFramesOutput,outlen,mAnchorTimeUs + (mNumFramesOutput * 1000000) / mSampleRate);
    mNumFramesOutput +=( outlen /mNumChannels/2);

    *out = buffer;


    return OK;
}

}  // namespace android
