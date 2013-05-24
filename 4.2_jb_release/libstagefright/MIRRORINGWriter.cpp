/*
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

//#define LOG_NDEBUG 0
#define LOG_TAG "MIRRORINGWriter"
#include <media/stagefright/foundation/ADebug.h>

#include <media/stagefright/foundation/hexdump.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/MIRRORINGWriter.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/Utils.h>
#include "include/ESDS.h"
#include "include/AVCEncoder.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <pthread.h>
#define	LOGD_TEST    ALOGD
#undef LOGD_TEST
#define LOGD_TEST
extern "C"
{
	#include "crc.h"
};
namespace android {
	FILE *test_file;
	FILE *test_264;
	FILE *test_aac_file;
	FILE *test_aac;
	
	FILE *test_ts_file;
	FILE *test_ts;
	FILE *test_ts_txt_file;
	FILE *test_ts_txt;
	FILE *tx_status_file;
#define MIRRORING_IOCTL_MAGIC 0x60
#define MIRRORING_START						_IOW(MIRRORING_IOCTL_MAGIC, 0x1, unsigned int)
#define MIRRORING_STOP						_IOW(MIRRORING_IOCTL_MAGIC, 0x2, unsigned int)
#define MIRRORING_SET_ROTATION				_IOW(MIRRORING_IOCTL_MAGIC, 0x3, unsigned int)
#define MIRRORING_DEVICE_OKAY				_IOW(MIRRORING_IOCTL_MAGIC, 0x4, unsigned int)
#define MIRRORING_SET_TIME					_IOW(MIRRORING_IOCTL_MAGIC, 0x5, unsigned int)
#define MIRRORING_GET_TIME					_IOW(MIRRORING_IOCTL_MAGIC, 0x6, unsigned int)
#define MIRRORING_GET_ROTATION				_IOW(MIRRORING_IOCTL_MAGIC, 0x7, unsigned int)
#define MIRRORING_SET_ADDR 					_IOW(MIRRORING_IOCTL_MAGIC, 0x8, unsigned int)
#define MIRRORING_GET_ADDR 					_IOW(MIRRORING_IOCTL_MAGIC, 0x9, unsigned int)
#define MIRRORING_SET_SIGNAL					_IOW(MIRRORING_IOCTL_MAGIC, 0xa, unsigned int)
#define MIRRORING_GET_SIGNAL					_IOW(MIRRORING_IOCTL_MAGIC, 0xb, unsigned int)
#define MIRRORING_SET_WIRELESSDB				_IOW(MIRRORING_IOCTL_MAGIC, 0xc, unsigned int)
#define MIRRORING_GET_WIRELESSDB				_IOW(MIRRORING_IOCTL_MAGIC, 0xd, unsigned int)
#define MIRRORING_GET_WIRELESS_DATALOST		_IOW(MIRRORING_IOCTL_MAGIC, 0xe, unsigned int)
#define MIRRORING_SET_FRAMERATE				_IOW(MIRRORING_IOCTL_MAGIC, 0xf, unsigned int)
#define MIRRORING_GET_CHIPINFO				_IOR(MIRRORING_IOCTL_MAGIC, 0x31, unsigned int)
	
struct MIRRORINGWriter::SourceInfo : public AHandler {
    SourceInfo(const sp<MediaSource> &source,MIRRORINGWriter* writer);

    void start(const sp<AMessage> &notify);
    void stop();

    unsigned streamType() const;
    unsigned incrementContinuityCounter();

    void readMore();

    enum {
        kNotifyStartFailed,
        kNotifyBuffer,
        kNotifyReachedEOS,
    };

    sp<ABuffer> lastAccessUnit();
    int64_t lastAccessUnitTimeUs();
    void setLastAccessUnit(const sp<ABuffer> &accessUnit);

    void setEOSReceived();
    bool eosReceived() const;

protected:
    virtual void onMessageReceived(const sp<AMessage> &msg);

    virtual ~SourceInfo();

private:
    enum {
        kWhatStart = 'strt',
        kWhatRead  = 'read',
    };

    sp<MediaSource> mSource;
    sp<ALooper> mLooper;
    sp<AMessage> mNotify;
    int 				mStart;
    sp<ABuffer> mAACCodecSpecificData;

    sp<ABuffer> mAACBuffer;

    sp<ABuffer> mLastAccessUnit;
    bool mEOSReceived;
 	int		  			avc_es_packet;
    int		  			sourceIndex;
    unsigned mStreamType;
    unsigned mContinuityCounter;
    MIRRORINGWriter*		mWriter;
    unsigned char   	key_info[200];
    unsigned int   		key_info_len;
    void extractCodecSpecificData();

    bool appendAACFrames(MediaBuffer *buffer);
    bool flushAACFrames();

    void postAVCFrame(MediaBuffer *buffer);

    DISALLOW_EVIL_CONSTRUCTORS(SourceInfo);
};
static    int64_t start_time_us = 0;
static int64_t last_time = 0;
int mFd;
Mutex mLock;

AutoAdapterData AAData;

int64_t img_length;
int64_t old_img_length;
int64_t delt_img_length;

int64_t MaxRestSize = 2000000ll ;

static int postaacframe = 0;
MIRRORINGWriter::SourceInfo::SourceInfo(const sp<MediaSource> &source, MIRRORINGWriter *writer)
    : mSource(source),
      mLooper(new ALooper),
      mEOSReceived(false),
      mStreamType(0),
      mContinuityCounter(0), 
      mWriter(writer){
    mLooper->setName("MIRRORINGWriter source");

    sp<MetaData> meta = mSource->getFormat();
    const char *mime;
    CHECK(meta->findCString(kKeyMIMEType, &mime));
	key_info_len = 0;
    if (!strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_AAC)) {
        mStreamType = 0x0f;
	  last_time = 0;
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_VIDEO_AVC)) {
        mStreamType = 0x1b;
    }  else if (!strcasecmp(mime, MEDIA_MIMETYPE_VIDEO_MJPEG)) {
        mStreamType = 0x2b;
    } else {
        TRESPASS();
    }
}

MIRRORINGWriter::SourceInfo::~SourceInfo() {
	
}

unsigned MIRRORINGWriter::SourceInfo::streamType() const {
    return mStreamType;
}

unsigned MIRRORINGWriter::SourceInfo::incrementContinuityCounter() {
    if (++mContinuityCounter == 16) {
        mContinuityCounter = 0;
    }

    return mContinuityCounter;
}

void MIRRORINGWriter::SourceInfo::start(const sp<AMessage> &notify) {
    mLooper->registerHandler(this);
    mLooper->start();

    mStart = 1;
    mNotify = notify;

    (new AMessage(kWhatStart, id()))->post();
}

void MIRRORINGWriter::SourceInfo::stop() {
    mStart = 0;
    mLooper->unregisterHandler(id());
    mLooper->stop();

    ALOGD("before source stop");
    mSource->stop();
}

void MIRRORINGWriter::SourceInfo::extractCodecSpecificData() {
    sp<MetaData> meta = mSource->getFormat();

    const char *mime;
    CHECK(meta->findCString(kKeyMIMEType, &mime));

    if (!strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_AAC)) {
        uint32_t type;
        const void *data;
        size_t size;
        if (!meta->findData(kKeyESDS, &type, &data, &size)) {
            // Codec specific data better be in the first data buffer.
            return;
        }

        ESDS esds((const char *)data, size);
        CHECK_EQ(esds.InitCheck(), (status_t)OK);

        const uint8_t *codec_specific_data;
        size_t codec_specific_data_size;
        esds.getCodecSpecificInfo(
                (const void **)&codec_specific_data, &codec_specific_data_size);

        CHECK_GE(codec_specific_data_size, 2u);

        mAACCodecSpecificData = new ABuffer(codec_specific_data_size);

        memcpy(mAACCodecSpecificData->data(), codec_specific_data,
               codec_specific_data_size);

        return;
    }

    if (strcasecmp(mime, MEDIA_MIMETYPE_VIDEO_AVC)) {
        return;
    }

    uint32_t type;
    const void *data;
    size_t size;
    if (!meta->findData(kKeyAVCC, &type, &data, &size)) {
        // Codec specific data better be part of the data stream then.
        return;
    }

    sp<ABuffer> out = new ABuffer(1024);
    out->setRange(0, 0);

    const uint8_t *ptr = (const uint8_t *)data;

    size_t numSeqParameterSets = ptr[5] & 31;

    ptr += 6;
    size -= 6;

    for (size_t i = 0; i < numSeqParameterSets; ++i) {
        CHECK(size >= 2);
        size_t length = U16_AT(ptr);

        ptr += 2;
        size -= 2;

        CHECK(size >= length);

        CHECK_LE(out->size() + 4 + length, out->capacity());
        memcpy(out->data() + out->size(), "\x00\x00\x00\x01", 4);
        memcpy(out->data() + out->size() + 4, ptr, length);
        out->setRange(0, out->size() + length + 4);

        ptr += length;
        size -= length;
    }

    CHECK(size >= 1);
    size_t numPictureParameterSets = *ptr;
    ++ptr;
    --size;

    for (size_t i = 0; i < numPictureParameterSets; ++i) {
        CHECK(size >= 2);
        size_t length = U16_AT(ptr);

        ptr += 2;
        size -= 2;

        CHECK(size >= length);

        CHECK_LE(out->size() + 4 + length, out->capacity());
        memcpy(out->data() + out->size(), "\x00\x00\x00\x01", 4);
        memcpy(out->data() + out->size() + 4, ptr, length);
        out->setRange(0, out->size() + length + 4);

        ptr += length;
        size -= length;
    }

    out->meta()->setInt64("timeUs", 0ll);

    sp<AMessage> notify = mNotify->dup();
    notify->setInt32("what", kNotifyBuffer);
    notify->setObject("buffer", out);
    notify->setInt32("oob", true);
    notify->post();
}

void MIRRORINGWriter::SourceInfo::postAVCFrame(MediaBuffer *buffer) {
    sp<AMessage> notify = mNotify->dup();
    notify->setInt32("what", kNotifyBuffer);


    int64_t timeUs;
    sp<ABuffer> copy ;//= new ABuffer(buffer->range_length() + key_info_len);
	AAData.postAvcFrame.now++;
	ALOGV("postAVCFrame");
	if(key_info_len == 0)
	{
		copy = new ABuffer(buffer->range_length());
		uint8_t *data = ( uint8_t *)((const uint8_t *)buffer->data() + buffer->range_offset());
		if((data[4] & 0xf) == 0x7 && (data[0] == 0)&& (data[1] == 0)&& (data[2] == 0)&& (data[3] == 1))
		{
			memcpy(key_info,(const uint8_t *)buffer->data() + buffer->range_offset(), buffer->range_length());
			key_info_len = buffer->range_length();
		}
		memcpy(copy->data(),(const uint8_t *)buffer->data() + buffer->range_offset(),buffer->range_length());
		ALOGV("postAVCFrame %x %x %x %x %x  %x %x %x %x %x key_info_len %d ",data[0],data[1],data[2],data[3],data[4],copy->size());
	}
	else
	{
		copy = new ABuffer(buffer->range_length() + key_info_len + 4);
		uint8_t *data = ( uint8_t *)copy->data();
		uint8_t *data1 = ( uint8_t *)((const uint8_t *)buffer->data() + buffer->range_offset());
		memcpy(copy->data(), "\x00\x00\x00\x01", 4);
		memcpy(copy->data() + 4,(const uint8_t *)buffer->data() + buffer->range_offset(),buffer->range_length());
		memcpy(copy->data()+buffer->range_length() + 4,key_info,key_info_len);
		ALOGV("postAVCFrame %x %x %x %x %x    %x %x %x %x %x copy->size() %d",
			data[0],data[1],data[2],data[3],data[4],data1[0],data1[1],data1[2],data1[3],data1[4],copy->size());
	}
	{	
		int ret_avc;
		if((ret_avc = access("data/test/test_file",0)) == 0)//test_file!=NULL)
		{
			ALOGV("test_file!=NULL test_264 %x",test_264);
			if(test_264 !=NULL)
			{
				fwrite(copy->data(),copy->size() ,1,test_264);
			}
			else
				{
				test_264 = fopen("data/test/test_264_file","wb");
				if(test_264!=NULL)
					fwrite(copy->data(),copy->size(),1,test_264);
				}
		}
		else
		{
			if(test_264!=NULL)
			{
				fclose(test_264);
				test_264 = NULL;
			}
			ALOGV("test_264==NULL ret %d",ret_avc);
		}
	}
    CHECK(buffer->meta_data()->findInt64(kKeyTime, &timeUs));
    copy->meta()->setInt64("timeUs", timeUs);
	
    int32_t isSync;
    if (buffer->meta_data()->findInt32(kKeyIsSyncFrame, &isSync)
            && isSync != 0) {
        copy->meta()->setInt32("isSync", true);
    }
	if (1 == AAData.discardNeedIframe_flag && isSync == 0) {
		return ;
	} else {
		AAData.discardNeedIframe_flag = 0;
	}
	AVCEncParams param;
	mWriter->mAVCEncoder->get_encoder_param(&param);

	if (copy->size() > (param.bitRate / 8 * 0.3)) { //temp_values = 100K
		if (isSync == 0) {
//			if (0 == AAData.setIFrameFlag) {
				ALOGD("avcsize > 0.3*bitrate, postAvcFrame return ...");
				mWriter->mAVCEncoder->Set_IDR_Frame();		
				AAData.setIFrameFlag = 1;
				param.qp = param.qp + 12;
				param.bitRate = param.bitRate - 1; // for QP efficient
				mWriter->mAVCEncoder->set_encoder_param(&param);
				return;
/*			} else {
				param.qp = 31;
				param.bitRate = param.bitRate - 1; // for QP efficient
				AAData.setIFrameFlag = 0;
				mWriter->mAVCEncoder->set_encoder_param(&param);

			}
*/
		}
	}


	//setting QP  by cghs
	int maxCopySize = (param.bitRate / 8 * 0.12) > 50000 ? (param.bitRate / 8 * 0.12) : 50000;
	ALOGV("flag %d , old_delt %lld, new_delt %lld",mWriter->rx_flag, AAData.deltPostTimeus, 
		timeUs - AAData.curPostTimeus);
	if (1 == mWriter->rx_flag && AAData.deltPostTimeus > 500000ll && 
		timeUs - AAData.curPostTimeus < 50000ll) {
		ALOGD("Setting QP... ...");
		param.qp = 26;
		param.bitRate = param.bitRate + 1; // for QP efficient
		mWriter->mAVCEncoder->set_encoder_param(&param);
		if (isSync == 0) {
			mWriter->mAVCEncoder->Set_IDR_Frame();
		}
	} else if (copy->size() > 3*AAData.lastPostDataSize && copy->size() > maxCopySize) {
		if (isSync == 0) {
			if (0 == AAData.setIFrameFlag) {
				ALOGD("Setting I-Frame... ...");
				mWriter->mAVCEncoder->Set_IDR_Frame();
				AAData.setIFrameFlag = 1;
				param.qp = 31;
				param.bitRate = param.bitRate - 1; // for QP efficient
				mWriter->mAVCEncoder->set_encoder_param(&param);
			} else {
				AAData.setIFrameFlag = 0;
				param.qp = 31;
				param.bitRate = param.bitRate - 1; // for QP efficient
				mWriter->mAVCEncoder->set_encoder_param(&param);
				
			}
			
		}
	} 
	AAData.deltPostTimeus = (AAData.curPostTimeus == 0) ? 
							0 :(timeUs - AAData.curPostTimeus);
	AAData.curPostTimeus = timeUs;
	AAData.lastPostDataSize = copy->size();
    notify->setObject("buffer", copy);
    notify->post();
	AAData.post.now += copy->size();
	img_length +=copy->size();


	
#ifdef AUTO_NETWORK_ADAPTER
	mLock.lock();
	if(AAData.discard_flag) {//isSyncFrame(&copy)
		ALOGI("postAVCFrame -- discard  and find isSyncFrame, clear buffer data......");
   	    mWriter->mLooper.get()->clearMessage();
		mWriter->mAVCEncoder->Set_IDR_Frame();
		AAData.discard_flag = 0;
		AAData.discardNeedIframe_flag = 1;
		AAData.send.now = AAData.post.now;
		AAData.send.old = AAData.post.old;
	}
	mLock.unlock();
#endif
 //   mWriter->writeTS();
}

bool MIRRORINGWriter::SourceInfo::appendAACFrames(MediaBuffer *buffer) {
	if(mAACCodecSpecificData == NULL)
	{
		CHECK_GE(buffer->range_length(), 2u);

        mAACCodecSpecificData = new ABuffer(buffer->range_length());

        memcpy(mAACCodecSpecificData->data(),
               (const uint8_t *)buffer->data()
                + buffer->range_offset(),
               buffer->range_length());
		return true;
	}
    int64_t timeUs;
	sp<ABuffer> 		mAACBuffer;


    size_t alloc = 188; 
    if (buffer->range_length() + 7 > alloc) {
        alloc = 7 + buffer->range_length();
    }

    mAACBuffer = new ABuffer(alloc);

    CHECK(buffer->meta_data()->findInt64(kKeyTime, &timeUs));

    mAACBuffer->meta()->setInt64("timeUs", timeUs);
    mAACBuffer->meta()->setInt32("isSync", true);

    mAACBuffer->setRange(0, 0);
	

    const uint8_t *codec_specific_data = mAACCodecSpecificData->data();

    unsigned profile = (codec_specific_data[0] >> 3) - 1;

    unsigned sampling_freq_index =
        ((codec_specific_data[0] & 7) << 1)
        | (codec_specific_data[1] >> 7);

    unsigned channel_configuration =
        (codec_specific_data[1] >> 3) & 0x0f;

    uint8_t *ptr = mAACBuffer->data() + mAACBuffer->size();

    const uint32_t aac_frame_length = buffer->range_length() + 7;

    *ptr++ = 0xff;
    *ptr++ = 0xf1;  // b11110001, ID=0, layer=0, protection_absent=1

    *ptr++ =
        profile << 6
        | sampling_freq_index << 2
        | ((channel_configuration >> 2) & 1);  // private_bit=0

    // original_copy=0, home=0, copyright_id_bit=0, copyright_id_start=0
    *ptr++ =
        (channel_configuration & 3) << 6
        | aac_frame_length >> 11;
    *ptr++ = (aac_frame_length >> 3) & 0xff;
    *ptr++ = (aac_frame_length & 7) << 5;

    // adts_buffer_fullness=0, number_of_raw_data_blocks_in_frame=0
    *ptr++ = 0;

	memcpy(ptr, (const uint8_t *)buffer->data() + buffer->range_offset(), buffer->range_length());

    ptr += buffer->range_length();

    mAACBuffer->setRange(0, ptr - mAACBuffer->data());

    sp<AMessage> notify = mNotify->dup();
    notify->setInt32("what", kNotifyBuffer);
    notify->setObject("buffer", mAACBuffer);
    notify->post();
	mLock.lock();
	AAData.post.now += mAACBuffer->size();
	mLock.unlock();
	return true;
}

bool MIRRORINGWriter::SourceInfo::flushAACFrames() {
	#if 0
    if (mAACBuffer == NULL) {
        return;
    }

    sp<AMessage> notify = mNotify->dup();
    notify->setInt32("what", kNotifyBuffer);
    notify->setObject("buffer", mAACBuffer);
    notify->post();
    post_length += mAACBuffer->size();

    mAACBuffer.clear();
	#endif
    return true;
}

void MIRRORINGWriter::SourceInfo::readMore() {
    (new AMessage(kWhatRead, id()))->post();
}

void MIRRORINGWriter::SourceInfo::onMessageReceived(const sp<AMessage> &msg) {
	if(mWriter->end_flag==1)
	{
		int err = UNKNOWN_ERROR;
		ALOGD("error happen in MIRRORING write");
        sp<AMessage> notify = mNotify->dup();
        notify->setInt32("what", kNotifyReachedEOS);
        notify->setInt32("status", err);
        notify->post();
		return;
	}
    switch (msg->what()) {
        case kWhatStart:
        {
			int temp;
			while(mWriter->connect_flag == 0)
			{
				if(mWriter->end_flag == 1)
				{
					int err = UNKNOWN_ERROR;
					ALOGD("error happen in MIRRORINGWriter kWhatStart");
			        sp<AMessage> notify = mNotify->dup();
			        notify->setInt32("what", kNotifyReachedEOS);
			        notify->setInt32("status", err);
			        notify->post();
					break;
				}	
				usleep(30000);
			}
			status_t err = mSource->start();

			if (err != OK) {
	                sp<AMessage> notify = mNotify->dup();
	                notify->setInt32("what", kNotifyStartFailed);
                notify->post();
                break;
            }

		//extractCodecSpecificData();
			ALOGD("mStreamType %d start",mStreamType);
            readMore();
            break;
        }

        case kWhatRead:
        {
		int64_t timeFirst,timesec,timeThird,timeFourth;
			if(start_time_us == 0 &&(!mWriter->HasVideo()|| (mStreamType == 0x1b && mWriter->HasVideo())))
		{
			int ret;
			start_time_us = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;		
			ret =ioctl(mFd,MIRRORING_SET_TIME,&start_time_us);
			if(ret < 0){
				ALOGD("set start time error");
				close(mFd);
				mFd = -1;
                sp<AMessage> notify = mNotify->dup();
                notify->setInt32("what", kNotifyStartFailed);
                notify->post();
                break;
			}
			ALOGD("start_time_us %lld mStreamType %d",start_time_us,mStreamType);
				
		}
		if(mStreamType == 0xf)
		{
            MediaBuffer *buffer;
			int loop_time = 1;
			if(start_time_us == 0)
			{
				msg->post();
			}
			timeFirst = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;		 	
			
			loop_time = (timeFirst - last_time  - start_time_us) / 23000 ;
			ALOGV(" loop_time %d cur time %lld %lld %lld",
					loop_time,last_time,timeFirst,start_time_us);
			if(start_time_us == 0)
				loop_time = 0;
			if(loop_time > 0 && ( (last_time + 24000ll  <= (timeFirst - start_time_us))))
			{
				ALOGV(" MIRRORINGWriter read curtime %lld start_time_us  %lld  last_time %lld  loop_time %d ",
					timeFirst,start_time_us,last_time,loop_time);
				

			}
			if(loop_time > 0 && ( (last_time + 24000ll  <= (timeFirst - start_time_us))))
			{
				while( loop_time > 0)
				{
					status_t err;
					
						err = mSource->read(&buffer,NULL);
					
	            	if (err != OK && err != INFO_FORMAT_CHANGED && err != -8 && err != -2111) {
						ALOGD("error happen in audio thread");
		                sp<AMessage> notify = mNotify->dup();
		                notify->setInt32("what", kNotifyReachedEOS);
		                notify->setInt32("status", err);
		                notify->post();
		                break;
	            	}

		            if (err == OK) {
		                if (mStreamType == 0x0f && mAACCodecSpecificData == NULL) {
		                    #if 0
		                    CHECK_GE(buffer->range_length(), 2u);

		                    mAACCodecSpecificData = new ABuffer(buffer->range_length());

		                    memcpy(mAACCodecSpecificData->data(),
		                           (const uint8_t *)buffer->data()
		                            + buffer->range_offset(),
		                           buffer->range_length());
							#endif
							appendAACFrames(buffer);
		                } else if (buffer->range_length() > 0) {
		                   if (mStreamType == 0x0f) {
						
		                        appendAACFrames(buffer);
		                    } 
		                }
						buffer->meta_data()->findInt64(kKeyTime, &last_time);
		                buffer->release();
		                buffer = NULL;
				   
					}
					loop_time--;	
			  	}
			}
			else
			{
				usleep(10000);
			}
		
			if(mStart)
				msg->post();
			else
				ALOGD("source audio stoped already");
		}

		else
		{

		//	msg->post();

			//break;
			MediaBuffer *buffer; 
			static int video_time = 0;
			
			status_t err = mSource->read(&buffer);//(MediaSource::ReadOptions*)(&start_time_us));
        	if (err != OK && err != INFO_FORMAT_CHANGED && err != -2111 && err != 9 && err != -8) {//jmj
	            ALOGD("(err != OK && err != INFO_FORMAT_CHANGED err  %d )",err );
			
	            sp<AMessage> notify = mNotify->dup();
	            notify->setInt32("what", kNotifyReachedEOS);
	            notify->setInt32("status", err);
	            notify->post();
	            break;
        	}
			if((video_time %100)==0)
				ALOGD("video_time %d err %d",video_time++,err);
            if (err == OK) {
				if (buffer->range_length() > 0) {
					postAVCFrame(buffer);
                }
                buffer->release();
                buffer = NULL;
            }
		
			usleep(10000);
			if(mStart)
				msg->post();
			else
				ALOGD("source video stoped already");
		}
		
		break;
       }

        default:
	      ALOGD("messagereceived default %d", msg->what());
            TRESPASS();
    }
}

sp<ABuffer> MIRRORINGWriter::SourceInfo::lastAccessUnit() {
    if(mLastAccessUnit == NULL)
    	ALOGD("MPEG2Writer::SourceInfo::lastAccessUnit NULL %d",mStreamType);
    else
	ALOGD("lastAccessUnit not NULL %d",mStreamType);
    return mLastAccessUnit;
}

void MIRRORINGWriter::SourceInfo::setLastAccessUnit(
        const sp<ABuffer> &accessUnit) {
    mLastAccessUnit = accessUnit;
    if(mLastAccessUnit == NULL)
    ALOGD("setLastAccessUnit NULL %d",mStreamType);
    else
	ALOGD("setLastAccessUnit not NULL %d",mStreamType);
}

int64_t MIRRORINGWriter::SourceInfo::lastAccessUnitTimeUs() {
    if (mLastAccessUnit == NULL) {
        return -1;
    }

    int64_t timeUs;
    CHECK(mLastAccessUnit->meta()->findInt64("timeUs", &timeUs));

    return timeUs;
}

void MIRRORINGWriter::SourceInfo::setEOSReceived() {
	ALOGD("setEOSReceived %d",mEOSReceived);
  //  CHECK(!mEOSReceived);
    
    mEOSReceived = true;
}

bool MIRRORINGWriter::SourceInfo::eosReceived() const {
    return mEOSReceived;
}

////////////////////////////////////////////////////////////////////////////////

int	last_sign;



#define sendersource

MIRRORINGWriter::MIRRORINGWriter(unsigned long addr, unsigned short local_port,unsigned short remort_port)
	  :mSenderSource(NULL),
      mStarted(false),
	  end_flag(0),
	  connect_flag(0),
      mNumSourcesDone(0),
      mNumTSPacketsWritten(0),
      mNumTSPacketsBeforeMeta(0),
      Snd_drpo_flag(0){
    int i;
    int temp[4];
    int ret;
    init();
    udp_buffer = NULL;	
    cmpFile = NULL;
    mFd = NULL;
	start_time_us = 0;
    last_time = 0;
    access_unit_num = 0;
	test_file = test_264 = NULL;
	mWriter_Type = 3;
   // mFile = fopen("/data/output.ts","wb");
	//cmpFile = fopen("/data/output_cmp.ts","wb"); 
    udp_buffer = (uint8_t*)malloc(64 * 1024);
    if((mFd = open("/dev/mirroring", 00000002, 0)) < 0)
    {
    	ALOGD("mirroring device can't be open fd %d\n",mFd);
    	close(mFd);
      	mFd = -1;
    	return;
    }
    if(ioctl(mFd,MIRRORING_GET_ADDR,temp))
    {
    	ALOGD("MIRRORING_GET_ADDR error");
		close(mFd);
      	mFd = -1;
    	return;
    }
	last_sign = 1;
	if(ioctl(mFd,MIRRORING_SET_SIGNAL,&last_sign))
	{
    	ALOGD("MIRRORING_SET_SIGNAL error");
		close(mFd);
      	mFd = -1;
    	return;
    }
	rx_addr = temp[0];
	rx_port	= temp[1];
	tx_port = temp[2];
	rx_flag = temp[3];
	//if(rx_flag == 1)
		ts_packet_num = 7;
	//else
		//ts_packet_num = 329;
	ALOGD("Before SenderSource get addr %d local_port %d remort_port %d flag %d",addr,local_port,remort_port,rx_flag);	
    mSenderSource = new SenderSource(rx_addr,rx_port,tx_port,rx_flag);
	mAVCEncoder = NULL;
#ifdef AUTO_NETWORK_ADAPTER
    if(rx_flag = 1)
	{
	memset(&AAData, 0 ,sizeof(AutoAdapterData));
	AAData.delt_length = -1;
	img_length = 0;
	old_img_length = 0;
	delt_img_length = 0;
	}
#endif
    
}

pthread_t  thread;
void* retval1;
int cal_flag = 1;
void MIRRORINGWriter::init() {

    mLooper = new ALooper;
    mLooper->setName("MIRRORINGWriter");

    mReflector = new AHandlerReflector<MIRRORINGWriter>(this);

    mLooper->registerHandler(mReflector);
    mLooper->start();
#if 0
	mLooper_Audio = new ALooper;
    mLooper_Audio->setName("MIRRORINGWriter_Audio");
    mReflector[1] = new AHandlerReflector<MIRRORINGWriter>(this);
    mLooper_Audio->registerHandler(mReflector[1]);
    mLooper_Audio->start();
	LOGD(" MIRRORINGWriter::init end");
#endif
}

MIRRORINGWriter::~MIRRORINGWriter() {
	int ret;
	if (mStarted) {
	    stop();
	}
	if(mFd >= 0)
	{
		close(mFd);
		mFd = -1;
	}
	if(mSenderSource!=NULL)
	{
		delete mSenderSource;
		mSenderSource = NULL;
	}
	
	if(cmpFile != NULL)
	{
	    fclose(cmpFile);
	    cmpFile = NULL;
	}	
	if(udp_buffer)
    		free(udp_buffer);
   	end_flag = 1;
   	pthread_join(thread, &retval1);
    	mLooper->unregisterHandler(mReflector->id());
    	mLooper->stop();
    
#ifdef	VIDEO_WRITER
  	mLooper_Audio->unregisterHandler(mReflector[1]->id());
	mLooper_Audio->stop();
#endif
    	ALOGD("MIRRORINGWriter::~MIRRORINGWriter exit");
}

status_t MIRRORINGWriter::addSource(const sp<MediaSource> &source) {
    CHECK(!mStarted);

    sp<MetaData> meta = source->getFormat();
    const char *mime;
    CHECK(meta->findCString(kKeyMIMEType, &mime));

    if (strcasecmp(mime, MEDIA_MIMETYPE_VIDEO_AVC)
            && strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_AAC)) {
        return ERROR_UNSUPPORTED;
    }else if (!strcasecmp(mime, MEDIA_MIMETYPE_VIDEO_AVC)) {
		MediaSource* ptr = source.get();
		mAVCEncoder = static_cast <AVCEncoder *>(ptr);
		mAVCEncoder->Set_Flag((rx_flag==1)?2:1,(rx_flag==1)?1:0);
    }

    sp<SourceInfo> info = new SourceInfo(source, this);

    mSources.push(info);

    return OK;
}

status_t MIRRORINGWriter::start(MetaData *param) {
    CHECK(!mStarted);
	int ret;
	start_time = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;		
	
	if(mStarted == true)
	{
		ALOGD("MIRRORINGWriter::start error. mStart state is already true");
		return UNKNOWN_ERROR;
	}
	else	if(!((mSenderSource != NULL ) && (mFd != NULL)  ))
	{
	ALOGD("MIRRORINGWriter::start error. mFd %d "
			,mFd);
		return UNKNOWN_ERROR;
	}

	
	int len;
		
    mStarted = true;
    mNumSourcesDone = 0;
    mNumTSPacketsWritten = 0;
    mNumTSPacketsBeforeMeta = 0;

    for (size_t i = 0; i < mSources.size(); ++i) {
        sp<AMessage> notify =
            new AMessage(kWhatSourceNotify, mReflector->id());

        notify->setInt32("source-index", i);

        mSources.editItemAt(i)->start(notify);
    }
    pthread_attr_t attr;
    cal_flag = 1;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
#ifdef AUTO_NETWORK_ADAPTER
	if(rx_flag == 1)
	{
    pthread_create(&thread, &attr, ThreadWrapper, this);
    pthread_attr_destroy(&attr);
    memset(AAData.sendRates, 0, 30*sizeof(int64_t));
    AAData.sendRatePos = 0;
	}
#endif
    return OK;
}

status_t MIRRORINGWriter::stop() {

    ALOGD("MIRRORINGWriter::stop  mStarted %d",mStarted);
    for (size_t i = 0; i < mSources.size(); ++i) {
        mSources.editItemAt(i)->stop();
    }
	if(mSenderSource!=NULL)
		mSenderSource->stop();
	end_flag = 1;
	
    mStarted = false;
    return OK;
}

status_t MIRRORINGWriter::pause() {
    CHECK(mStarted);

    return OK;
}

bool MIRRORINGWriter::reachedEOS() {
    return !mStarted || (mNumSourcesDone == mSources.size() ? true : false);
}

status_t MIRRORINGWriter::dump(int fd, const Vector<String16> &args) {
    return OK;
}

void MIRRORINGWriter::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
        case kWhatSourceNotify:
        {
            int32_t sourceIndex;
            CHECK(msg->findInt32("source-index", &sourceIndex));

            int32_t what;
            CHECK(msg->findInt32("what", &what));

            if (what == SourceInfo::kNotifyReachedEOS
                    || what == SourceInfo::kNotifyStartFailed) {
                sp<SourceInfo> source = mSources.editItemAt(sourceIndex);
				//结束时设置信号好，以免弹框
				if(mFd >= 0)
				{
					int sign = 1;
					int err;
					if(err = ioctl(mFd,MIRRORING_SET_SIGNAL,&sign))
					{
						close(mFd);
						mFd = -1;
						ALOGD("MIRRORING_SET_SIGNAL err %d",err);
					}
				}
                source->setEOSReceived();
				#if 0
                sp<ABuffer> buffer = source->lastAccessUnit();
                source->setLastAccessUnit(NULL);

                if (buffer != NULL) {
                    writeTS();
                    writeAccessUnit(sourceIndex, buffer);
                }
				#endif
                ++mNumSourcesDone;
            } else if (what == SourceInfo::kNotifyBuffer) {
                sp<RefBase> obj;
                CHECK(msg->findObject("buffer", &obj));
#if 0
                sp<ABuffer> buffer = static_cast<ABuffer *>(obj.get());

                int32_t oob;
                if (msg->findInt32("oob", &oob) && oob) {
                    // This is codec specific data delivered out of band.
                    // It can be written out immediately.
                    writeTS();
                    writeAccessUnit(sourceIndex, buffer);
                    break;
                }

                // We don't just write out data as we receive it from
                // the various sources. That would essentially write them
                // out in random order (as the thread scheduler determines
                // how the messages are dispatched).
                // Instead we gather an access unit for all tracks and
                // write out the one with the smallest timestamp, then
                // request more data for the written out track.
                // Rinse, repeat.
                // If we don't have data on any track we don't write
                // anything just yet.

                sp<SourceInfo> source = mSources.editItemAt(sourceIndex);

                CHECK(source->lastAccessUnit() == NULL);
                source->setLastAccessUnit(buffer);

                ALOGD("lastAccessUnitTimeUs[%d] = %.2f secs",
                     sourceIndex, source->lastAccessUnitTimeUs() / 1E6);

                int64_t minTimeUs = -1;
                size_t minIndex = 0;

                for (size_t i = 0; i < mSources.size(); ++i) {
                    const sp<SourceInfo> &source = mSources.editItemAt(i);

                    if (source->eosReceived()) {
                        continue;
                    }

                    int64_t timeUs = source->lastAccessUnitTimeUs();
                    if (timeUs < 0) {
                        minTimeUs = -1;
                        break;
                    } else if (minTimeUs < 0 || timeUs < minTimeUs) {
                        minTimeUs = timeUs;
                        minIndex = i;
                    }
                }

                if (minTimeUs < 0) {
                    ALOGD("not a all tracks have valid data. %d",source->streamType());
                    break;
                }

                ALOGD("writing access unit at time %.2f secs (index %d)",
                     minTimeUs / 1E6, minIndex);

                source = mSources.editItemAt(minIndex);

                buffer = source->lastAccessUnit();
                source->setLastAccessUnit(NULL);

                writeTS();
                writeAccessUnit(minIndex, buffer);

                source->readMore();
#else
				{
					int ret_ts;
					if((ret_ts = access("data/test/test_ts_file",0)) == 0)//test_file!=NULL)
					{
						ALOGV("test_ts_file!=NULL test_ts %x",test_ts);
						if(test_ts ==NULL)
							test_ts = fopen("data/test/test_ts","wb");
					}
					else
					{
						if(test_ts!=NULL)
						{
							fclose(test_ts);
							test_ts = NULL;
						}
						ALOGV("test_264==NULL ret %d",ret_ts);
					}
				}
				writeTS();

				sp<ABuffer> buffer = static_cast<ABuffer *>(obj.get());
				writeAccessUnit(sourceIndex, buffer);
				ALOGV("kWhatSourceNotify");
#endif
            }
            break;
        }

        default:
            TRESPASS();
    }
}

void MIRRORINGWriter::writeProgramAssociationTable() {
    // 0x47
    // transport_error_indicator = b0
    // payload_unit_start_indicator = b1
    // transport_priority = b0
    // PID = b0000000000000 (13 bits)
    // transport_scrambling_control = b00
    // adaptation_field_control = b01 (no adaptation field, payload only)
    // continuity_counter = b????
    // skip = 0x00
    // --- payload follows
    // table_id = 0x00
    // section_syntax_indicator = b1
    // must_be_zero = b0
    // reserved = b11
    // section_length = 0x00d
    // transport_stream_id = 0x0000
    // reserved = b11
    // version_number = b00001
    // current_next_indicator = b1
    // section_number = 0x00
    // last_section_number = 0x00
    //   one program follows:
    //   program_number = 0x0001
    //   reserved = b111
    //   program_map_PID = 0x01e0 (13 bits!)
    // CRC = 0x????????
    unsigned int crc;

    static const uint8_t kData[] = {
        0x47,
        0x40, 0x00, 0x10, 0x00,  // b0100 0000 0000 0000 0001 ???? 0000 0000
        0x00, 0xb0, 0x0d, 0x00,  // b0000 0000 1011 0000 0000 1101 0000 0000
        0x00, 0xc3, 0x00, 0x00,  // b0000 0000 1100 0011 0000 0000 0000 0000
        0x00, 0x01, 0xe1, 0xe0,  // b0000 0000 0000 0001 1110 0001 1110 0000
        0x00, 0x00, 0x00, 0x00   // b???? ???? ???? ???? ???? ???? ???? ????
    };

    sp<ABuffer> buffer = new ABuffer(188);
    memset(buffer->data(), 0, buffer->size());
    memcpy(buffer->data(), kData, sizeof(kData));

    static const unsigned kContinuityCounter = 5;
    buffer->data()[3] |= kContinuityCounter;
    crc = __swap32(av_crc(av_crc_get_table(AV_CRC_32_IEEE), -1, buffer->data() + 5, 0x10-4));	
    ALOGV("pmt crc = %x",crc);
    buffer->data()[17] = (crc >> 24) & 0xff;
    buffer->data()[18] = (crc >> 16) & 0xff;
    buffer->data()[19] = (crc >> 8) & 0xff;
    buffer->data()[20] = (crc) & 0xff;
    memcpy(programmap_asso,buffer->data(),188);

   
    {
		
		ssize_t n;
		if(test_ts!=NULL)// && sourceIndex == 1)
			fwrite(buffer->data(), 1, buffer->size(), test_ts);
			n = internalWrite(buffer->data(), buffer->size(),0);            
			ALOGV(" programassociation okay n %d",n);
		if(n!=0)
		{
			ALOGD("writeProgramAssociationTable n!= (ssize_t)buffer->size() %d %d",n,(ssize_t)buffer->size() );
			if(n == 0)
				end_flag = 1;
			return;
		}
    }
}

void MIRRORINGWriter::writeProgramMap() {
    // 0x47
    // transport_error_indicator = b0
    // payload_unit_start_indicator = b1
    // transport_priority = b0
    // PID = b0 0001 1110 0000 (13 bits) [0x1e0]
    // transport_scrambling_control = b00
    // adaptation_field_control = b01 (no adaptation field, payload only)
    // continuity_counter = b????
    // skip = 0x00
    // -- payload follows
    // table_id = 0x02
    // section_syntax_indicator = b1
    // must_be_zero = b0
    // reserved = b11
    // section_length = 0x???
    // program_number = 0x0001
    // reserved = b11
    // version_number = b00001
    // current_next_indicator = b1
    // section_number = 0x00
    // last_section_number = 0x00
    // reserved = b111
    // PCR_PID = b? ???? ???? ???? (13 bits)
    // reserved = b1111
    // program_info_length = 0x000
    //   one or more elementary stream descriptions follow:
    //   stream_type = 0x??
    //   reserved = b111
    //   elementary_PID = b? ???? ???? ???? (13 bits)
    //   reserved = b1111
    //   ES_info_length = 0x000
    // CRC = 0x????????

    static const uint8_t kData[] = {
        0x47,
        0x41, 0xe0, 0x10, 0x00,  // b0100 0001 1110 0000 0001 ???? 0000 0000
        0x02, 0xb0, 0x00, 0x00,  // b0000 0010 1011 ???? ???? ???? 0000 0000
        0x01, 0xc3, 0x00, 0x00,  // b0000 0001 1100 0011 0000 0000 0000 0000
        0xe0, 0x00, 0xf0, 0x00   // b111? ???? ???? ???? 1111 0000 0000 0000
    };
    uint32_t crc;
    sp<ABuffer> buffer = new ABuffer(188);
    memset(buffer->data(), 0, buffer->size());
    memcpy(buffer->data(), kData, sizeof(kData));

    static const unsigned kContinuityCounter = 5;
    buffer->data()[3] |= kContinuityCounter;

    size_t section_length = 5 * mSources.size() + 4 + 9;
    buffer->data()[6] |= section_length >> 8;
    buffer->data()[7] = section_length & 0xff;

    static const unsigned kPCR_PID = 0x1e1;
    buffer->data()[13] |= (kPCR_PID >> 8) & 0x1f;
    buffer->data()[14] = kPCR_PID & 0xff;

    uint8_t *ptr = &buffer->data()[sizeof(kData)];
    for (size_t i = 0; i < mSources.size(); ++i) {
        *ptr++ = mSources.editItemAt(i)->streamType();

        const unsigned ES_PID = 0x1e0 + i + 1;
        *ptr++ = 0xe0 | (ES_PID >> 8);
        *ptr++ = ES_PID & 0xff;
        *ptr++ = 0xf0;
        *ptr++ = 0x00;
    }
     crc = __swap32(av_crc(av_crc_get_table(AV_CRC_32_IEEE), -1, buffer->data() + 5, 0x1a - 4));	
	 ALOGV("pat crc = %x",crc);
    *ptr++ = (crc >> 24) & 0xff;
    *ptr++ = (crc >> 16) & 0xff;
    *ptr++ = (crc >> 8) & 0xff;
    *ptr++ = (crc) & 0xff;
    memcpy(programmap_asso + 188,buffer->data(),188);

    
    {
    	
		ssize_t n;
		if(test_ts!=NULL)// && sourceIndex == 1)
			fwrite(buffer->data(), 1, buffer->size(), test_ts);
		n = internalWrite(buffer->data(), buffer->size(),0);            
		if(n!=0)
		{
			ALOGD("writeProgramMap n!= (ssize_t)buffer->size() %d %d",n,(ssize_t)buffer->size() );
			if(n == 0)
				end_flag = 1;
			return;
		}
    }	
}

void MIRRORINGWriter::writeAccessUnit(
        int32_t sourceIndex, const sp<ABuffer> &accessUnit) {
    // 0x47
    // transport_error_indicator = b0
    // payload_unit_start_indicator = b1
    // transport_priority = b0
    // PID = b0 0001 1110 ???? (13 bits) [0x1e0 + 1 + sourceIndex]
    // transport_scrambling_control = b00
    // adaptation_field_control = b01 (no adaptation field, payload only)
    // continuity_counter = b????
    // -- payload follows
    // packet_startcode_prefix = 0x000001
    // stream_id = 0x?? (0xe0 for avc video, 0xc0 for aac audio)
    // PES_packet_length = 0x????
    // reserved = b10
    // PES_scrambling_control = b00
    // PES_priority = b0
    // data_alignment_indicator = b1
    // copyright = b0
    // original_or_copy = b0
    // PTS_DTS_flags = b10  (PTS only)
    // ESCR_flag = b0
    // ES_rate_flag = b0
    // DSM_trick_mode_flag = b0
    // additional_copy_info_flag = b0
    // PES_CRC_flag = b0
    // PES_extension_flag = b0
    // PES_header_data_length = 0x05
    // reserved = b0010 (PTS)
    // PTS[32..30] = b???
    // reserved = b1
    // PTS[29..15] = b??? ???? ???? ???? (15 bits)
    // reserved = b1
    // PTS[14..0] = b??? ???? ???? ???? (15 bits)
    // reserved = b1
    // the first fragment of "buffer" follows

    int packet_num = 0;
	int	es_packet_num = 0;
    sp<ABuffer> buffer = new ABuffer(188);
    memset(buffer->data(), 0, buffer->size());

    const unsigned PID = 0x1e0 + sourceIndex + 1;


    // XXX if there are multiple streams of a kind (more than 1 audio or
    // more than 1 video) they need distinct stream_ids.
    const unsigned stream_id = mSources.editItemAt(sourceIndex)->streamType() == 0x0f ? 0xc0 : 0xe0;
	
    int64_t timeUs;
    CHECK(accessUnit->meta()->findInt64("timeUs", &timeUs));
	if (mSources.editItemAt(sourceIndex)->streamType() == 0x1b) {
		AAData.curSystemTimeUs = systemTime(SYSTEM_TIME_MONOTONIC) / 1000 - start_time_us;		
		AAData.curSendTimeus = timeUs;
		//ALOGD("curSendTimeus : %lld , systemTimeUs: %lld ",curSendTimeus, tempTimeUs);	
	}
	
    timeUs %= 42949672950ll;
	accessUnit->meta()->setInt64("timeUs", timeUs);
	
   
    uint64_t PTS = (timeUs * 9ll) / 100ll;
	int	payload_size =  accessUnit->size();
    size_t PES_packet_length;
    size_t offset = 0;
	{
		int ret_ts;
		
		if((ret_ts = access("data/test/test_ts_txt_file",0)) == 0)//test_file!=NULL)
		{
			ALOGV("test_ts_txt_file!=NULL test_ts_txt %x",test_ts_txt_file);
			if(test_ts_txt == NULL)
				test_ts_txt = fopen("data/test/test_ts_txt.txt","wb");
			else
			{
				int64_t cur_time;
				static  int64_t last_audio_time_us,last_video_time_us;
				static  int64_t last_audio_systime,last_video_systime;
				cur_time = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;		
				if(mSources.editItemAt(sourceIndex)->streamType() == 0xf)
				{
					fprintf(test_ts_txt,"Audio aac sys start_time_us %lld time %lld timeUs %lld delta %lld sys time va %lld aa %lld delta time va %lld aa %lld PTS %d %2x%2x%2x%2x%2x \n",
						start_time_us,cur_time,timeUs,cur_time - timeUs -start_time_us ,
						cur_time - last_video_systime,cur_time - last_audio_systime,timeUs - last_video_time_us
						,timeUs - last_audio_time_us,PTS, 0x20 | (((PTS >> 30) & 7) << 1) | 1,(PTS >> 22) & 0xff,
			    		(((PTS >> 15) & 0x7f) << 1) | 1, (PTS >> 7) & 0xff, ((PTS & 0x7f) << 1) | 1);
					fflush(test_ts_txt);
					last_audio_time_us = timeUs;
					last_audio_systime = cur_time;
				}
				else
				{
					fprintf(test_ts_txt,"Video aac sys start_time_us %lld time %lld timeUs %lld delta %lld sys time av %lld vv %lld delta time av %lld vv %lld PTS %d %2x%2x%2x%2x%2x\n",
						start_time_us,cur_time,timeUs,cur_time - timeUs -start_time_us - 200000ll
						,cur_time - last_audio_systime,cur_time - last_video_systime,timeUs - last_audio_time_us
						,timeUs - last_video_time_us,PTS, 0x20 | (((PTS >> 30) & 7) << 1) | 1,(PTS >> 22) & 0xff,
			    		(((PTS >> 15) & 0x7f) << 1) | 1, (PTS >> 7) & 0xff, ((PTS & 0x7f) << 1) | 1);
					fflush(test_ts_txt);
					last_video_time_us = timeUs;
					last_video_systime = cur_time;
				}
					
			}
		}
		else
		{
			
			if(test_ts_txt!=NULL)
			{
				fclose(test_ts_txt);
				test_ts_txt = NULL;
			}
			ALOGV("test_ts_txt==NULL ret %d",ret_ts);
		}
	}
	AAData.send.now += accessUnit->size();
    while (offset < accessUnit->size()) {
        // for subsequent fragments of "buffer":
        // 0x47
        const unsigned continuity_counter = mSources.editItemAt(sourceIndex)->incrementContinuityCounter();
		uint8_t *ptr = udp_buffer + 188 * packet_num;

		if(es_packet_num== 0)
		{
			PES_packet_length = payload_size + 8;
			if (PES_packet_length > 60522) {//325*184 + 170
        // This really should only happen for video.
        	CHECK_EQ(stream_id, 0xe0u);

        // It's valid to set this to 0 for video according to the specs.
		        PES_packet_length = 60522+8;
				payload_size -= 60522;
		    }

		    *ptr++ = 0x47;
		    *ptr++ = 0x40 | (PID >> 8);
		    *ptr++ = PID & 0xff;
		    *ptr++ = 0x10 | continuity_counter;
		    *ptr++ = 0x00;
		    *ptr++ = 0x00;
		    *ptr++ = 0x01;
		    *ptr++ = stream_id;
		    *ptr++ = PES_packet_length >> 8;
		    *ptr++ = PES_packet_length & 0xff;
		    *ptr++ = 0x84;
		    *ptr++ = 0x80;
		    *ptr++ = 0x05;
		    *ptr++ = 0x20 | (((PTS >> 30) & 7) << 1) | 1;
		    *ptr++ = (PTS >> 22) & 0xff;
		    *ptr++ = (((PTS >> 15) & 0x7f) << 1) | 1;
		    *ptr++ = (PTS >> 7) & 0xff;
		    *ptr++ = ((PTS & 0x7f) << 1) | 1;

		    size_t sizeLeft = udp_buffer + 188 - ptr;
		    size_t copy = accessUnit->size();
		    if (copy > sizeLeft) {
		        copy = sizeLeft;
		    }
		    else
				memset(ptr,0,sizeLeft);
		    memcpy(ptr, accessUnit->data(), copy);

		}
		else
		{

	        *ptr++ 	= 0x47;
	        *ptr++ 	= 0x00 | (PID >> 8);
	        *ptr++ 	= PID & 0xff;
	        *ptr++ 	= 0x10 | continuity_counter;
		}

        size_t sizeLeft = udp_buffer + 188 * (packet_num + 1) - ptr;
        size_t copy = accessUnit->size() - offset;
        if (copy > sizeLeft) {
            copy = sizeLeft;
        }
        else
	    memset(ptr,0,sizeLeft);
        memcpy(ptr, accessUnit->data() + offset, copy);
	    packet_num++;
		es_packet_num++;
        offset += copy;
		if(packet_num == ts_packet_num)
		{

		
			ssize_t n;
			if((test_ts!=NULL))// && sourceIndex == 1)
				fwrite(udp_buffer, 1,  packet_num * 188, test_ts);
			n = internalWrite(udp_buffer, packet_num * 188,(mSources.editItemAt(sourceIndex)->streamType()) == 0xf ? 1 :0);
			if(n!=0)
			{
				ALOGD("writeAccessUnit  n!= (ssize_t)buffer->size() %d packet_num %d * 188",n,packet_num * 188);
				if(n == -2111)
	  			{
	  				Snd_drpo_flag = 1;
					mAVCEncoder->Set_IDR_Frame();
	  			}
				else	if(n == -1111)
					end_flag = 1;
				return;
			}

			packet_num = 0;
		}
		if(es_packet_num == 329)
			es_packet_num = 0;
    }
	if(test_ts!=NULL )//&& sourceIndex == 1)
		fwrite(udp_buffer, 1,  packet_num * 188, test_ts);
	if((((access_unit_num < 50)||((access_unit_num % 50) == 0 ) ) ) && packet_num < 5)
	{
		memcpy(&udp_buffer[packet_num * 188], programmap_asso, 376);
		packet_num+=2;
	}
	access_unit_num++;
	if(mSources.editItemAt(sourceIndex)->streamType() == 0x1b )
	{
		const unsigned continuity_counter = mSources.editItemAt(sourceIndex)->incrementContinuityCounter();
		size_t PES_packet_length = 16;
		uint8_t *ptr = &udp_buffer[packet_num * 188];
		 unsigned char nal_data[8]={0,0,0,1,0x6e,0,0,0};
		*ptr++ = 0x47;
		*ptr++ = 0x40 | (PID >> 8);
		*ptr++ = PID & 0xff;
		*ptr++ = 0x10 | continuity_counter;
		*ptr++ = 0x00;
		*ptr++ = 0x00;
		*ptr++ = 0x01;
		*ptr++ = stream_id;
		*ptr++ = PES_packet_length >> 8; 
		*ptr++ = PES_packet_length & 0xff;
		*ptr++ = 0x84;
		*ptr++ = 0x80;
		*ptr++ = 0x05;
		*ptr++ = 0x20 | (((PTS >> 30) & 7) << 1) | 1;
		*ptr++ = (PTS >> 22) & 0xff;
		*ptr++ = (((PTS >> 15) & 0x7f) << 1) | 1;
		*ptr++ = (PTS >> 7) & 0xff;
		*ptr++ = ((PTS & 0x7f) << 1) | 1;
		size_t sizeLeft = udp_buffer + 188 * (packet_num + 1) - ptr;
		memset(ptr,0,sizeLeft);
		memcpy(ptr,nal_data,8);
		packet_num++;
	}
	ALOGV("writeAccessUnit end n!= (ssize_t)buffer->size() %d packet_num %d * 188",packet_num,ts_packet_num);
	if(packet_num > 0)
	{
		
		ssize_t n;
		n = internalWrite(udp_buffer, packet_num * 188,(mSources.editItemAt(sourceIndex)->streamType()) == 0xf ? 1 :0);
		if(n!=0)
		{
			ALOGD("writeAccessUnit end n!= (ssize_t)buffer->size() %d packet_num %d * 188",n,ts_packet_num);
			if(n == -2111)
  			{
  				Snd_drpo_flag = 1;
				mAVCEncoder->Set_IDR_Frame();
  			}
			else	if(n == -1111)
				end_flag = 1;
			return;
		}
			
	}  
		
}

void MIRRORINGWriter::writeTS() {
    if (mNumTSPacketsWritten >= mNumTSPacketsBeforeMeta) {
        writeProgramAssociationTable();
        writeProgramMap();

        mNumTSPacketsBeforeMeta = mNumTSPacketsWritten + 2500;
    }
}

ssize_t MIRRORINGWriter::internalWrite(const void *data, size_t size,int AudioStream) {
	int ret = 0;
	
		
	
		if(mSenderSource!= NULL)
			ret = mSenderSource->SendData(data,size, AudioStream);
		
		return ret;

}
#ifdef AUTO_NETWORK_ADAPTER

void* MIRRORINGWriter::ThreadWrapper(void *me ) {
	return (void *) static_cast<MIRRORINGWriter *>(me)->decideByUnsendData();
	//return (void *) static_cast<MIRRORINGWriter *>(me)->calcDelayTime();
}

void* MIRRORINGWriter::decideByUnsendData() {
	int time1 = 0; // T = 30
	int time2 = 0; // T = 10
	int wifiDropdataTime = 0;
	int	wifiDropdataStartTime = 0;
	int networkWorseTime = 0;
	int networkWorseStartTime = 0;
	int delt_length_SameDataTime = 0;
	int deltLengthTime = 0;
	//flag
	int time2_flag = 0;
	int wifiDropdataFlag = 0;
	int networkWorseFlag = 0; 
	int networkBetterSetEnc_flag = 0;
	int networkWorseSud_flag = 0; 		//network become worse suddently;
	int networkWorseSetEnc_flag = 0;
	int dropdataSetEnc_flag = 0;
	//values
	int MaxSendrateDes = 0;
	int MaxBitrate = 0;
	int MinBitrate = 400 * 1024;
	int wirelessSignal = 0;
	
	int totalWifiData = 0;
	int old_totalWifiData = 0;
	int delt_totalWifiData = 0;

	int dropWifiData = 0;
	int old_dropWifiData = 0;
	int delt_dropWifiData = 0;

	float dropDataRate = 0.0;
	if (mWriter_Type == 3) {
		int ret;
		int64_t cur_time;
		while(mSenderSource->initCheck() != OK) {
			usleep(50000);
			cur_time = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;		
			if(cur_time - start_time > 10000000) {
				ALOGD("connection time out curtime %lld start_time %lld",cur_time, start_time);
				end_flag = 1;
				break;
			}
		}
		if(end_flag==0) {
			ret = mSenderSource->start();
			
			if(ret!=OK)
				end_flag = 1;
			else
				connect_flag = 1;
		}
	}
	while (end_flag == 0 && mAVCEncoder != NULL) {
//		usleep(1000000ll);
		usleep(985000ll);

#if 0
		//read tx_status_file
		tx_status_file = fopen("/sys/class/rkwifi_wimo/tx_status", "r");
		if(tx_status_file != NULL) {
			fscanf(tx_status_file ,"TOTAL=%d DROPPED=%d",&totalWifiData, &dropWifiData);
			ALOGE("TOTAL=%d DROPPED=%d",totalWifiData, dropWifiData);
			fclose(tx_status_file);
			tx_status_file = NULL;

			delt_totalWifiData = totalWifiData - old_totalWifiData;
			old_totalWifiData = totalWifiData;
			
			delt_dropWifiData = dropWifiData - old_dropWifiData;
			old_dropWifiData = dropWifiData;		
			ALOGE("delt_totalWifiData : %d, delt_dropWifiData :%d ",delt_totalWifiData,delt_dropWifiData);
		}
		
		if (delt_dropWifiData > 0 ) {
			//in this state, uninterrupted in 5times.
			if(!wifiDropdataFlag) {
				wifiDropdataFlag = 1;
				wifiDropdataTime = 0;
				wifiDropdataStartTime = time1;
			}

			if (time1 -wifiDropdataStartTime != wifiDropdataTime) {
				wifiDropdataFlag = 0;
				wifiDropdataStartTime = 0;
				wifiDropdataTime = 0;
			}
			
			wifiDropdataTime++;
			
			if (3 == wifiDropdataTime)	{				
				wifiDropdataFlag = 0;
				dropdataSetEnc_flag = 1;		
			}

		}
#endif
		
		//get wireless_signal
		int err;
		if(err = ioctl(mFd,MIRRORING_GET_WIRELESSDB,&wirelessSignal)){
			close(mFd);
			mFd = -1;
			ALOGD("MIRRORING_GET_WIRELESSDB err %d",err);
		}

/*
		//set framerate 
		int framerate = 20;
		if(err = ioctl(mFd,MIRRORING_SET_FRAMERATE,&framerate)){
			close(mFd);
			mFd = -1;
			ALOGD("MIRRORING_SET_FRAMERATE err %d",err);
		}
*/	
		MaxBitrate = calcMaxBitrate(wirelessSignal);

		//LOGD_TEST("wirelessSignal : %d , MaxBitrate : %d", wirelessSignal, MaxBitrate);

		//set MaxRestSize
		mAVCEncoder->get_encoder_param(&mAVCEncParams);
		MaxRestSize = (3 * mAVCEncParams.bitRate) / 8;

		MaxSendrateDes = (mAVCEncParams.bitRate * 0.4) / 8;
		MaxSendrateDes = MaxSendrateDes > 100000ll ? MaxSendrateDes : 100000ll;
		
		int64_t temp_length = calcDeltData();
		if ( /*AAData.post.delt < 35000 ||*/ AAData.postAvcFrame.delt < 10){//Desktop state
			AAData.desktopFlag = 1;
			AAData.lastBitrate = mAVCEncParams.bitRate;
			AAData.lastQP = mAVCEncParams.qp;
			if(mAVCEncParams.bitRate != 2*1024*1024)
				setAvcencBitRate(2*1024*1024);
			continue;
		}

		if (AAData.desktopFlag) {
			//back from desktop ,resume the previous state;
			mAVCEncoder->get_encoder_param(&mAVCEncParams);
			mAVCEncParams.bitRate = AAData.lastBitrate;
			mAVCEncParams.qp = AAData.lastQP;
			mAVCEncoder->set_encoder_param(&mAVCEncParams);
			AAData.desktopFlag = 0;
		}

		//
		time1 ++;
		if (time2_flag) {
			time2 ++;
		} else {
			time2 = 0;
		}
		//LOGD_TEST("time1 = %d , time2 = %d", time1, time2);
		
		AAData.sendRates[AAData.sendRatePos %30] = AAData.send.delt;
		if(AAData.sendRatePos > 1 && 
			AAData.sendRates[(AAData.sendRatePos-1)%30] - AAData.sendRates[AAData.sendRatePos%30] > MaxSendrateDes
			&& temp_length > 10000) {
			networkWorseSud_flag = 1;
		}
		AAData.sendRatePos ++;

		if(AAData.sendRatePos >= 30) {
		    AAData.avgSendRate = calculateAvgSendRate();

		    if(0 == AAData.sendRatePos%30) {
				AAData.sendRatePos = 30; //range from 30 ~ 60
		    }
		}


		if(1) {
			int sign;
			int err;
			mAVCEncoder->get_encoder_param(&mAVCEncParams);
			if(temp_length > 0) 
				deltLengthTime ++;
			if(deltLengthTime > 10 ) {//|| AAData.timeDelay > 500000ll ) {	
				deltLengthTime = 0;
				sign = 0;
			}else{
				sign = 1;
			}
			
			if(sign == 0)
				ALOGD("net work is not okay now size %d  bitrate %d timeDelay %lld",temp_length,mAVCEncParams.bitRate, AAData.timeDelay);
			if(last_sign != sign){
				if(err = ioctl(mFd,MIRRORING_SET_SIGNAL,&sign))
				{
					close(mFd);
					mFd = -1;
					ALOGD("MIRRORING_SET_SIGNAL err %d",err);
				}
				last_sign = sign;
			}
		}
		if (-1 == AAData.delt_length) {
			//first init
			AAData.delt_length = temp_length;
		} 

		if ( 0 != temp_length && temp_length == AAData.delt_length) {
			// fix the error delt_length
			delt_length_SameDataTime ++;
			if (delt_length_SameDataTime == 10) {
				delt_length_SameDataTime = 0;
				mLock.lock();
				AAData.post.now = AAData.send.now;
				mLock.unlock();
			}
		}

		if (networkWorseSud_flag && temp_length == 0)
			networkWorseSud_flag = 0;


		if (0 == temp_length) { // actually temp_length < 1000 ,network may be better.
			//network become better		
			deltLengthTime = 0;
			if(0 != AAData.avgSendRate){ //&& (calculateSendIncrease(sendRatePos) > 1) ) {//Vx >= Vs 
				time2_flag = 1;
				if(mAVCEncParams.bitRate < 3000000ll) {
					if (20 == time2) {
						networkBetterSetEnc_flag = 1;
					}
				} else if (mAVCEncParams.bitRate >= 3000000ll) {
					if (30 == time2) {
						networkBetterSetEnc_flag = 1;
					}
				}		
			}
		} 

		if (temp_length > MaxRestSize){ 
		//MaxSize, if above , discard
			AAData.discard_flag = 1;
 			time2_flag = 0;
		} 

		if (temp_length > 0 && AAData.delt_length < temp_length) {
			ALOGD("delt_length < temp_length ...  networkWorseTime: %d", networkWorseTime);
				//in this state, uninterrupted in 5times.
				if(!networkWorseFlag) {
					networkWorseFlag = 1;
					networkWorseTime = 0;
					networkWorseStartTime = time1;
				}

				if (time1 -networkWorseStartTime != networkWorseTime) {
					ALOGD("networkWorseFlag : 0 , time1:%d, networkWorseStartTime : %d networkWorseTime:%d ", 
						time1, networkWorseStartTime, networkWorseTime);
					networkWorseFlag = 0;
					networkWorseStartTime = 0;
					networkWorseTime = 0;
				}
				
				networkWorseTime++;
				
				if (3 == networkWorseTime)	{				
					networkWorseSetEnc_flag = 1;
					networkWorseFlag = 0;
				}

//			}
			time2_flag = 0;
		} 

		if ( 30 == time1) { 			
			time1 = 0;
		}
		
		AAData.delt_length	 = temp_length;		

		//LOGD_TEST("worse_flag : %d , better_flag : %d , sud_worse_flag : %d", networkWorseSetEnc_flag, networkBetterSetEnc_flag, networkWorseSud_flag);

		//set bitrate
		int bitrate = 0;
		mAVCEncoder->get_encoder_param(&mAVCEncParams);
		//LOGD_TEST("w %d ,h %d, bitrate %d, rc %d, fps %d, qp %d",
//		mAVCEncParams.width,mAVCEncParams.height,mAVCEncParams.bitRate,mAVCEncParams.rc_mode,mAVCEncParams.framerate, mAVCEncParams.qp);

		if (networkWorseSud_flag || networkWorseSetEnc_flag || 
			networkBetterSetEnc_flag || dropdataSetEnc_flag)	{

			if (dropdataSetEnc_flag) {
				ALOGD("wifi dropdata: setting bitrate ... ...");
				bitrate = mAVCEncParams.bitRate * 0.8;
				
			} if (networkWorseSud_flag) {
				ALOGD("NetworkWorse Suddently: setting bitrate ... ...");
				AAData.discard_flag = 1;
				bitrate = mAVCEncParams.bitRate * 0.5;
				bitrate = bitrate < MinBitrate ? (MinBitrate) : bitrate;
		
			} else if (networkWorseSetEnc_flag) {
				ALOGD("NetworkWorse: setting bitrate ... ...");
				int find = 0;
				for (int p = 0; p < 3; p++) {
					int64_t tempBR = AAData.historyBitrate[(AAData.HBPos-p+10)%10];
					if (tempBR < mAVCEncParams.bitRate 
						&& (tempBR > MinBitrate && tempBR < MaxBitrate)) {
						bitrate = tempBR;
						find = 1;
						break;
					}
				}

				if (0 == find) {
					bitrate = mAVCEncParams.bitRate - 16 *	AAData.delt_length;//(mAVCEncParams.bitRate - 16 * delt_length) * 0.9;
					bitrate = bitrate < MinBitrate ? (MinBitrate) : bitrate;
				}
			
			} else if (networkBetterSetEnc_flag) {
				ALOGD("NetworkBetter: setting bitrate ... ...");
				if(mAVCEncParams.bitRate == MaxBitrate) {
					time2_flag = 0;
					time2 = 0;
				} else {
//					float para = calculateEncRatePara(mAVCEncParams.bitRate);
//					bitrate = mAVCEncParams.bitRate * para;
					bitrate = mAVCEncParams.bitRate + 500 * 1024;
					bitrate = bitrate > MaxBitrate ? MaxBitrate : bitrate;
				    time2 = 0;
				    time1 = 0; 
				}			

			}

	//		if(delt_postavcframe > 25) {
	//			bitrate = bitrate * 25 / delt_postavcframe;
	//		}

			if (bitrate >= 400000ll && bitrate <= MaxBitrate) {
				AAData.HBPos = AAData.HBPos%10;
				AAData.HBPos++;
				AAData.historyBitrate[AAData.HBPos] = bitrate;
				status_t err = setAvcencBitRate(bitrate);
			}
		}	
		
		if (time2 > 60) time2  = 0;
		dropdataSetEnc_flag = 0;
		networkBetterSetEnc_flag = 0;
		networkWorseSetEnc_flag = 0;
		networkWorseSud_flag = 0;
		
	}
	return NULL;
}

int64_t MIRRORINGWriter::calculateAvgSendRate(/*int64_t *sendrates*/) {
	//Mutex::Autolock autolock(mLock);
	
	int64_t max = AAData.sendRates[0];
	int64_t min = AAData.sendRates[0];
	int64_t sum = 0;
	for (int i =0 ; i < 30; i++) {
	    if(max < AAData.sendRates[i]) {
		max = AAData.sendRates[i];
	    } else if(min > AAData.sendRates[i]) {
		min = AAData.sendRates[i];
	    } 
            sum +=AAData.sendRates[i];
	}
	sum = sum - max - min;
	return sum/28;
}

float MIRRORINGWriter::calculateEncRatePara(int bitrate){
	if(bitrate >0 && bitrate < 1000000ll) {
		return 1.5;
	}else if(bitrate >= 1000000ll && bitrate < 2000000ll) {
		return 1.3;
	} else if (bitrate >= 2000000ll && bitrate < 3000000ll) {
		return 1.2;
	} else if (bitrate >= 3000000ll && bitrate < 4000000ll) {
		return 1.1;
	} else if (bitrate >= 4000000ll && bitrate < 6000000ll) {
		return 1.05;
	} else {
		return 1.0;
	}
}


float MIRRORINGWriter::calculateSendIncrease(int pos) {
	int64_t rate1 = AAData.sendRates[pos--%30];
	int64_t rate2 = AAData.sendRates[pos--%30];
	int64_t rate3 = AAData.sendRates[pos--%30];

	int flag1 = rate1 - rate2 > 0 ? 1 : 0;
	int flag2 = rate2 - rate3 > 0 ? 1 : 0;
	if (1 == flag1 && 1 == flag2) {
		return 1.2;
	} else if (0 == flag1 && 0 == flag2) {
		return 0.8;
	} else if (1== flag1 && 0 == flag2 ) {
		return 1.1;
	} else if (0 == flag1 && 1 == flag2) {
		return 0.9;
	}

	return 1;
//	return 0;
}

int MIRRORINGWriter::isSyncFrame(sp<ABuffer> *buffer){
	 //cghs
       uint8_t head[5];
       memcpy(head, (const uint8_t *)(*buffer)->data(), 5*sizeof(uint8_t));
	if(head[3] ==0x01 && head[0] == 0 && head[1] == 0 && head[2] == 0 && (head[4] == 0x25 ||head[4] == 0x65)) {
	    ALOGD("postavcframe: head: %x, %x ------find SyncFrame !!!!!! ",head[3], head[4]);
    	    return 1;
	} else {
		return 0;
	}   
}

int64_t MIRRORINGWriter::calcDeltData() {
		AAData.post.delt = AAData.post.now - AAData.post.old;
		AAData.post.old = AAData.post.now;
		
		AAData.send.delt = AAData.send.now - AAData.send.old;
		AAData.send.old = AAData.send.now;

		AAData.postAvcFrame.delt = AAData.postAvcFrame.now - AAData.postAvcFrame.old;
		AAData.postAvcFrame.old = AAData.postAvcFrame.now;

		delt_img_length = img_length - old_img_length;
		old_img_length = img_length;

		int64_t temp_length = AAData.post.now - AAData.send.now;

		AAData.timeDelay = AAData.curSystemTimeUs - AAData.curSendTimeus;
		
		//LOGD_TEST("delt_img_length = %lld --delt_post_length = %lld  -- delt_postavcframe =%d --delt_send_length=%lld - avgSendRate= %lld -- temp_length=%lld timeDelay %lld", 
				//	delt_img_length,AAData.post.delt, AAData.postAvcFrame.delt, AAData.send.delt, AAData.avgSendRate , temp_length, AAData.timeDelay);
		
		return temp_length;
		
}

status_t MIRRORINGWriter::setAvcencBitRate(int64_t bitrate) {
	status_t err;
	if (bitrate <= 0) {
		ALOGE("setAvcencBitRate --->bitrate value invalid!");
		return UNKNOWN_ERROR;
	}
	
	err = mAVCEncoder->get_encoder_param(&mAVCEncParams);
	if (err != OK) {
		ALOGE("------->get AVC bitrate failed!!!!");
		return UNKNOWN_ERROR;
	}
	//ALOGE("old bitrate : %d", mAVCEncParams.bitRate);
	mAVCEncParams.bitRate = bitrate;
	//ALOGE("new bitrate : %d", mAVCEncParams.bitRate);	
	err = mAVCEncoder->set_encoder_param(&mAVCEncParams);
	if(err != OK){
		ALOGE("-------->set AVC bitrate failed!!!!");
		return UNKNOWN_ERROR;
	}
	return OK;
	
}


int MIRRORINGWriter::calcMaxBitrate (int db) {
	if (db >= 0) {
		return 6*1024*1024;
	} else if (db <= -100) {
		return 400*1024;
	} else if (db >=-50 && db < 0) {
		return 6*1024*1024;
	} else if (db >=-70 && db < -50) {
		return 4*1024*1024;
	} else if (db >=-100 && db < -70) {
		return 2*1024*1024;
	}

	return 2*1024*1024;
}

//ms

void* MIRRORINGWriter::decideByDelayTime() {

	int sign;	
	int delayPos = 0;
	int bitrate = 0;
	int delayLevel = 0;
	int betterTime = 0;
	int worseTime = 0;
	
	int64_t delays[30];
	memset(delays, 0, 30*sizeof(int64_t));

	int64_t unsendDataLength = 0;

	
	while (cal_flag) {
		usleep(985000ll);
		betterTime++;
		worseTime++;
		
		mAVCEncoder->get_encoder_param(&mAVCEncParams);
		MaxRestSize = (3 * mAVCEncParams.bitRate) / 8;

		if(mAVCEncParams.bitRate == 2*1024*1024) {
			// for test
			status_t err = setAvcencBitRate(400 * 1024);
		}
		AAData.timeDelay= AAData.curSystemTimeUs - AAData.curSendTimeus;
		delays[delayPos++ % 30] = AAData.timeDelay;
		if(delayPos > 30) 
			delayPos = 30 + delayPos%30;
	
		ALOGD("curSystemTimeUs: %lld, curSendTimeus:%lld, curPostTimeus:%lld,temp_delay: %lld", 
			 AAData.curSystemTimeUs, AAData.curSendTimeus, AAData.curPostTimeus, AAData.timeDelay);

		unsendDataLength = calcDeltData();
		if (unsendDataLength > MaxRestSize) {
			AAData.discard_flag = 1;
			mAVCEncoder->get_encoder_param(&mAVCEncParams);
			status_t err = setAvcencBitRate(mAVCEncParams.bitRate);
			continue;
		}

		{
			//network worse tip
			if(AAData.timeDelay > 800000ll) {
				sign = 0;
				ALOGD("net work is not okay now timeDelay %lld ms ", AAData.timeDelay);
			} else {
				sign = 1;
			}
			if(last_sign != sign){
				int err;
				if(err = ioctl(mFd,MIRRORING_SET_SIGNAL,&sign)) {
					close(mFd);
					mFd = -1;
					ALOGD("MIRRORING_SET_SIGNAL err %d",err);
				}
				last_sign = sign;
			}
		}
		
		if (AAData.timeDelay <= 220000ll) {
			if(isDelayBetter(delays) && betterTime > 30 && unsendDataLength == 0) {
				//better
				delayLevel = 1;
				betterTime = 0;
			}
			
		} else if (AAData.timeDelay > 220000ll && AAData.timeDelay <= 400000ll ) {
			if (isDelayWorse(delays, delayPos) && worseTime > 10){
				//worse
				delayLevel = 2;		
				worseTime = 0;
			}

		} else if (AAData.timeDelay > 400000ll) {
			//worst
			delayLevel = 3;
			worseTime = 0;
		}	

		mAVCEncoder->get_encoder_param(&mAVCEncParams);

		switch (delayLevel) {
			case 1:{
				bitrate = mAVCEncParams.bitRate + 0.5 * 1024 * 1024;
			}	break;
			case 2:{
				bitrate = mAVCEncParams.bitRate - 0.5 * 1024 * 1024;
			}	break;
			case 3:{
				bitrate = mAVCEncParams.bitRate * 0.5;
				AAData.discard_flag = 1;
				mAVCEncoder->Set_IDR_Frame();
			}	break;
			default :
				//bitrate = 2*1024*1024;
				break;
		}
		if (delayLevel && bitrate > 400000ll && bitrate < 6000000ll)
			status_t err = setAvcencBitRate(bitrate);

//		status_t err = setAvcencBitRate(300*1024);
		delayLevel = 0;
		
	}
	return NULL;
}

int MIRRORINGWriter::isDelayBetter(int64_t *delays) {
	int time = 0;
	
	for (int i = 0; i < 30 ; i++) {
		if (delays[i] > 220000ll) 
			time ++;

		if (time == 4) {
			return 0;
		}
	}
	ALOGI("isDelayMin : return 1");
	return 1;
}

int MIRRORINGWriter::isDelayWorse(int64_t *delays, int pos) {
	int time = 0;

	for (int i = pos; pos-i<4 ;i--) {
		if(delays[i%30] > 220000ll) {
			time++;
		}

		if (time == 4) {
			ALOGI("isDelayWorse : return 1;");
			return 1;
		}
	}

	return 0;
}


#endif
MediaWriter* MIRRORINGWriter::createInstanceEx(unsigned long addr, unsigned short local_port,unsigned short remort_port)
{
	MediaWriter* mWriter = new MIRRORINGWriter(addr,local_port,remort_port);
	return mWriter;
}
extern "C" MediaWriter* openMIRRORINGWriter(unsigned long addr, unsigned short local_port,unsigned short remort_port)
{
	return MIRRORINGWriter::createInstanceEx(addr,local_port,remort_port);
}
}  // namespace android

