/*
 * Copyright (C) 2011 The Android Open Source Project
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
//#define LOG_NDEBUG 0
#define LOG_TAG "SurfaceMediaSource"

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/SurfaceMediaSource.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MetaData.h>
#include <OMX_IVCommon.h>
#include <MetadataBufferType.h>

#include <ui/GraphicBuffer.h>
#include <gui/ISurfaceComposer.h>
#include <gui/IGraphicBufferAlloc.h>
#include <OMX_Component.h>

#include <utils/Log.h>
#include <utils/String8.h>

#include <private/gui/ComposerService.h>

#include <sys/ioctl.h>
#include "rga.h"
#include <fcntl.h>
#include <poll.h>
#include <ui/Rect.h>
#include <ui/GraphicBufferMapper.h>
#include "vpu_mem.h"
#include <ui/DisplayInfo.h>

#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceComposerClient.h>

#define	SURFACE_ORIGINAL_SIZE 1
namespace android {
FILE *omx_txt = NULL;
FILE * omx_RGB ;
int64_t video_source_num;
int64_t last_video_source_num;
SurfaceMediaSource::SurfaceMediaSource(uint32_t bufferWidth, uint32_t bufferHeight) :
    mWidth(bufferWidth),
    mHeight(bufferHeight),
    mCurrentSlot(BufferQueue::INVALID_BUFFER_SLOT),
    mNumPendingBuffers(0),
    mCurrentTimestamp(0),
    mFrameRate(30),
    mStarted(false),
    mNumFramesReceived(0),
    mNumFramesEncoded(0),
    mFirstFrameTimestamp(0),
    mMaxAcquiredBufferCount(4),  // XXX double-check the default
    mUseAbsoluteTimestamps(false) {
    ALOGV("SurfaceMediaSource");
	#if ASYNC_MEDIA_BUFFER_USED
    omx_RGB = NULL;
	DisplayInfo info;
	last_video_source_num  = video_source_num = 0;
	sp<IBinder> display(SurfaceComposerClient::getBuiltInDisplay(
        ISurfaceComposer::eDisplayIdMain));	
    SurfaceComposerClient::getDisplayInfo(display, &info);

	if(info.w > info.h)
	{
         orig_width = (int32_t)info.w;
         orig_height = (int32_t)info.h;
	}
	else
	{
         orig_width = (int32_t)info.h;
         orig_height = (int32_t)info.w;
	}	
	#if SURFACE_ORIGINAL_SIZE
    orig_width = bufferWidth;
    orig_height = bufferHeight;//(int32_t)info.h;
    #endif
    ALOGD("SurfaceMediaSource mMaxAcquiredBufferCount %d bufferWidth %d bufferHeight %d orig_width %d orig_height %d",
		mMaxAcquiredBufferCount,bufferWidth
		,bufferHeight,orig_width,orig_height);
	
    if (bufferWidth == 0 || bufferHeight == 0 || orig_width == 0 || orig_height == 0) {
        ALOGE("Invalid dimensions %dx%d orig_width %d orig_height %d", bufferWidth, bufferHeight,orig_width,orig_height);
    }

    mBufferQueue = new BufferQueue(false);
    mBufferQueue->setDefaultBufferSize(orig_width,orig_height);//bufferWidth, bufferHeight);
   #else
    if (bufferWidth == 0 || bufferHeight == 0) {
        ALOGE("Invalid dimensions %dx%d", bufferWidth, bufferHeight);
    }

    mBufferQueue = new BufferQueue(false);
    mBufferQueue->setDefaultBufferSize(bufferWidth, bufferHeight);
 #endif
    mBufferQueue->setSynchronousMode(false);
    mBufferQueue->setConsumerUsageBits(GRALLOC_USAGE_HW_VIDEO_ENCODER |
            GRALLOC_USAGE_HW_TEXTURE);
	ALOGD("SurfaceMediaSource constructor false");
    sp<ISurfaceComposer> composer(ComposerService::getComposerService());

    // Note that we can't create an sp<...>(this) in a ctor that will not keep a
    // reference once the ctor ends, as that would cause the refcount of 'this'
    // dropping to 0 at the end of the ctor.  Since all we need is a wp<...>
    // that's what we create.
    wp<BufferQueue::ConsumerListener> listener;
    sp<BufferQueue::ConsumerListener> proxy;
    listener = static_cast<BufferQueue::ConsumerListener*>(this);
    proxy = new BufferQueue::ProxyConsumerListener(listener);

    status_t err = mBufferQueue->consumerConnect(proxy);
    if (err != NO_ERROR) {
        ALOGE("SurfaceMediaSource: error connecting to BufferQueue: %s (%d)",
                strerror(-err), err);
    }
	
}

SurfaceMediaSource::~SurfaceMediaSource() {
    ALOGV("~SurfaceMediaSource");
    CHECK(!mStarted);
	
}

nsecs_t SurfaceMediaSource::getTimestamp() {
    ALOGV("getTimestamp");
    Mutex::Autolock lock(mMutex);
    return mCurrentTimestamp;
}

void SurfaceMediaSource::setFrameAvailableListener(
        const sp<FrameAvailableListener>& listener) {
    ALOGV("setFrameAvailableListener");
    Mutex::Autolock lock(mMutex);
    mFrameAvailableListener = listener;
}

void SurfaceMediaSource::dump(String8& result) const
{
    char buffer[1024];
    dump(result, "", buffer, 1024);
}

void SurfaceMediaSource::dump(String8& result, const char* prefix,
        char* buffer, size_t SIZE) const
{
    Mutex::Autolock lock(mMutex);

    result.append(buffer);
    mBufferQueue->dump(result);
}

status_t SurfaceMediaSource::setFrameRate(int32_t fps)
{
    ALOGV("setFrameRate");
    Mutex::Autolock lock(mMutex);
    const int MAX_FRAME_RATE = 60;
    if (fps < 0 || fps > MAX_FRAME_RATE) {
        return BAD_VALUE;
    }
    mFrameRate = fps;
    return OK;
}

bool SurfaceMediaSource::isMetaDataStoredInVideoBuffers() const {
    ALOGV("isMetaDataStoredInVideoBuffers");
    return true;
}

int32_t SurfaceMediaSource::getFrameRate( ) const {
    ALOGV("getFrameRate");
    Mutex::Autolock lock(mMutex);
    return mFrameRate;
}

status_t SurfaceMediaSource::start(MetaData *params)
{
    ALOGV("start");

    Mutex::Autolock lock(mMutex);

    CHECK(!mStarted);

    mStartTimeNs = 0;
    int64_t startTimeUs;
    int32_t bufferCount = 0;
    if (params) {
        if (params->findInt64(kKeyTime, &startTimeUs)) {
            mStartTimeNs = startTimeUs * 1000;
        }

        if (!params->findInt32(kKeyNumBuffers, &bufferCount)) {
            ALOGE("Failed to find the advertised buffer count");
            return UNKNOWN_ERROR;
        }

        if (bufferCount <= 1) {
            ALOGE("bufferCount %d is too small", bufferCount);
            return BAD_VALUE;
        }

        mMaxAcquiredBufferCount = bufferCount;
    }

    CHECK_GT(mMaxAcquiredBufferCount, 1);

    status_t err =
        mBufferQueue->setMaxAcquiredBufferCount(mMaxAcquiredBufferCount);

    if (err != OK) {
        return err;
    }
    mBufferQueue->setSynchronousMode(false);
   ALOGD("SurfaceMediaSource start syncmode %d mMaxAcquiredBufferCount %d synchronousMode false",
		mBufferQueue.get(),mMaxAcquiredBufferCount);
    mNumPendingBuffers = 0;
    mStarted = true;

    return OK;
}

status_t SurfaceMediaSource::setMaxAcquiredBufferCount(size_t count) {
    ALOGD("setMaxAcquiredBufferCount(%d)", count);
    Mutex::Autolock lock(mMutex);

    CHECK_GT(count, 1);
    mMaxAcquiredBufferCount = count;

    return OK;
}

status_t SurfaceMediaSource::setUseAbsoluteTimestamps() {
    ALOGV("setUseAbsoluteTimestamps");
    Mutex::Autolock lock(mMutex);
    mUseAbsoluteTimestamps = true;

    return OK;
}

status_t SurfaceMediaSource::stop()
{
    ALOGD("stop");
    Mutex::Autolock lock(mMutex);

    if (!mStarted) {
        return OK;
    }

    while (mNumPendingBuffers > 0) {
        ALOGI("Still waiting for %d buffers to be returned.",
                mNumPendingBuffers);

#if DEBUG_PENDING_BUFFERS
        for (size_t i = 0; i < mPendingBuffers.size(); ++i) {
            ALOGI("%d: %p", i, mPendingBuffers.itemAt(i));
        }
#endif

        mMediaBuffersAvailableCondition.wait(mMutex);
    }

    mStarted = false;
    mFrameAvailableCondition.signal();
    mMediaBuffersAvailableCondition.signal();
    return mBufferQueue->consumerDisconnect();
}

sp<MetaData> SurfaceMediaSource::getFormat()
{
    ALOGV("getFormat");

    Mutex::Autolock lock(mMutex);
    sp<MetaData> meta = new MetaData;

    meta->setInt32(kKeyWidth, mWidth);
    meta->setInt32(kKeyHeight, mHeight);
    // The encoder format is set as an opaque colorformat
    // The encoder will later find out the actual colorformat
    // from the GL Frames itself.
    meta->setInt32(kKeyColorFormat, OMX_COLOR_FormatAndroidOpaque);
    meta->setInt32(kKeyStride, mWidth);
    meta->setInt32(kKeySliceHeight, mHeight);
    meta->setInt32(kKeyFrameRate, mFrameRate);
    meta->setCString(kKeyMIMEType, MEDIA_MIMETYPE_VIDEO_RAW);
    return meta;
}

// Pass the data to the MediaBuffer. Pass in only the metadata
// The metadata passed consists of two parts:
// 1. First, there is an integer indicating that it is a GRAlloc
// source (kMetadataBufferTypeGrallocSource)
// 2. This is followed by the buffer_handle_t that is a handle to the
// GRalloc buffer. The encoder needs to interpret this GRalloc handle
// and encode the frames.
// --------------------------------------------------------------
// |  kMetadataBufferTypeGrallocSource | sizeof(buffer_handle_t) |
// --------------------------------------------------------------
// Note: Call only when you have the lock
static void passMetadataBuffer(MediaBuffer **buffer,
        buffer_handle_t bufferHandle) {
    *buffer = new MediaBuffer(4 + sizeof(buffer_handle_t));
    char *data = (char *)(*buffer)->data();
    if (data == NULL) {
        ALOGE("Cannot allocate memory for metadata buffer!");
        return;
    }
    OMX_U32 type = kMetadataBufferTypeGrallocSource;
    memcpy(data, &type, 4);
    memcpy(data + 4, &bufferHandle, sizeof(buffer_handle_t));

    ALOGV("handle = %p, , offset = %d, length = %d",
            bufferHandle, (*buffer)->range_length(), (*buffer)->range_offset());
}

status_t SurfaceMediaSource::read( MediaBuffer **buffer,
                                    const ReadOptions *options)
{
    ALOGV("read mNumPendingBuffers %d mMaxAcquiredBufferCount %d",mNumPendingBuffers,mMaxAcquiredBufferCount);
    Mutex::Autolock lock(mMutex);
	static int sign = 0;

    *buffer = NULL;

    while (mStarted && mNumPendingBuffers == mMaxAcquiredBufferCount) {
        mMediaBuffersAvailableCondition.wait(mMutex);
    }
	
	int64_t systime = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;
	int64_t systime3;
    // Update the current buffer info
    // TODO: mCurrentSlot can be made a bufferstate since there
    // can be more than one "current" slots.

    BufferQueue::BufferItem item;
    // If the recording has started and the queue is empty, then just
    // wait here till the frames come in from the client side
    while (mStarted) {

        status_t err = mBufferQueue->acquireBuffer(&item);
		 systime3 = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;
        if (err == BufferQueue::NO_BUFFER_AVAILABLE) {
            // wait for a buffer to be queued
            mFrameAvailableCondition.wait(mMutex);
        } else if (err == OK) {

            // First time seeing the buffer?  Added it to the SMS slot
            if (item.mGraphicBuffer != NULL) {
		//	ALOGD("mBufferSlot 1 mCurrentSlot %d item.mGraphicBuffer %x",mCurrentSlot,item.mGraphicBuffer.get());
                mBufferSlot[item.mBuf] = item.mGraphicBuffer;
            }
		
            // check for the timing of this buffer
            if (mNumFramesReceived == 0 && !mUseAbsoluteTimestamps) {
                mFirstFrameTimestamp = item.mTimestamp;
                // Initial delay
                if (mStartTimeNs > 0) {
                    if (item.mTimestamp < mStartTimeNs) {
                        // This frame predates start of record, discard
                        mBufferQueue->releaseBuffer(item.mBuf, EGL_NO_DISPLAY,
                                EGL_NO_SYNC_KHR, Fence::NO_FENCE);
                        continue;
                    }
                    mStartTimeNs = item.mTimestamp - mStartTimeNs;
                }
            }
			if(1)
			{
			  int retrtptxt;
			  int64_t sys_time;
			  static int64_t last_time_us = 0;
			  static int64_t start_time_us = 0;
			  static int64_t last_sys_time = 0;
			  sys_time = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;		
			  if(start_time_us == 0)
			  	start_time_us=sys_time - (mStartTimeNs + (item.mTimestamp - mFirstFrameTimestamp))/1000;
			  if(start_time_us < sys_time -(mStartTimeNs + (item.mTimestamp - mFirstFrameTimestamp))/1000)
				  start_time_us=sys_time - (mStartTimeNs + (item.mTimestamp - mFirstFrameTimestamp))/1000;
			  if((retrtptxt = access("data/test/omx_txt_file",0)) == 0)//test_file!=NULL)
			  {
				  
				  if(omx_txt == NULL)
					  omx_txt = fopen("data/test/omx_txt.txt","ab");
				  if(omx_txt != NULL)
				  {
					fprintf(omx_txt,"SurfaceMediaSource::read Video sys_time %lld mStartTimeNs %lld %lld timeUs %lld %lld delta %lld %lld  %lld buffer_handle %x video_source_num %d %d %d mNumFramesReceived %d setMaxAcquiredBufferCount %d mNumPendingBuffers %d %x\n",
						sys_time,start_time_us ,mStartTimeNs,item.mTimestamp/1000,( mStartTimeNs + (item.mTimestamp - mFirstFrameTimestamp))/1000 ,
						(( mStartTimeNs + (item.mTimestamp - mFirstFrameTimestamp)) - last_time_us)/1000
						,sys_time - start_time_us -(mStartTimeNs + (item.mTimestamp - mFirstFrameTimestamp)) / 1000,
						sys_time - last_sys_time,mBufferSlot[item.mBuf]->handle,last_video_source_num,video_source_num,
						video_source_num-last_video_source_num,
						mNumFramesReceived,mMaxAcquiredBufferCount,mNumPendingBuffers,*buffer);
					ALOGV("SurfaceMediaSource::read Video sys_time %lld mStartTimeNs %lld %lld timeUs %lld delta %lld %lld  %lld buffer_handle %x video_source_num %d %d %d mNumFramesReceived %d setMaxAcquiredBufferCount %d mNumPendingBuffers %d %x\n",
						sys_time,start_time_us ,mStartTimeNs,( mStartTimeNs + (item.mTimestamp - mFirstFrameTimestamp))/1000 ,
						(( mStartTimeNs + (item.mTimestamp - mFirstFrameTimestamp)) - last_time_us)/1000
						,sys_time - start_time_us -(mStartTimeNs + (item.mTimestamp - mFirstFrameTimestamp)) / 1000,
						sys_time - last_sys_time,mBufferSlot[item.mBuf]->handle,last_video_source_num,video_source_num,
						video_source_num-last_video_source_num,
						mNumFramesReceived,mMaxAcquiredBufferCount,mNumPendingBuffers,*buffer);
					fflush(omx_txt);
				  }
			  }
			  last_sys_time = sys_time;
			  last_time_us = mStartTimeNs + (item.mTimestamp - mFirstFrameTimestamp);
			}
            item.mTimestamp = mStartTimeNs + (item.mTimestamp - mFirstFrameTimestamp);

            mNumFramesReceived++;

            break;
        } else {
            ALOGE("read: acquire failed with error code %d", err);
            return ERROR_END_OF_STREAM;
        }

    }

    // If the loop was exited as a result of stopping the recording,
    // it is OK
    if (!mStarted) {
        ALOGV("Read: SurfaceMediaSource is stopped. Returning ERROR_END_OF_STREAM.");
        return ERROR_END_OF_STREAM;
    }

    mCurrentSlot = item.mBuf;

    // First time seeing the buffer?  Added it to the SMS slot
    if (item.mGraphicBuffer != NULL) {
		
        mBufferSlot[mCurrentSlot] = item.mGraphicBuffer;
    }
	
    
    
    // Pass the data to the MediaBuffer. Pass in only the metadata
		
 //   passMetadataBuffer(buffer, mBufferSlot[mCurrentSlot]->handle);
 
	
	ALOGV("passMetadataBuffer");
	mCurrentBuffers.push_back(mBufferSlot[mCurrentSlot]);
	passMetadataBuffer(buffer, mBufferSlot[mCurrentSlot]->handle);
	int64_t prevTimeStamp = mCurrentTimestamp;
	mCurrentTimestamp = item.mTimestamp;

	mNumFramesEncoded++;

	ALOGV("passMetadataBuffer %d %x",(*buffer)->refcount(),*buffer);
    (*buffer)->setObserver(this);
    (*buffer)->add_ref();
    (*buffer)->meta_data()->setInt64(kKeyTime, mCurrentTimestamp / 1000);
    ALOGV("Frames encoded = %d, timestamp = %lld, time diff = %lld",
            mNumFramesEncoded, mCurrentTimestamp / 1000,
            mCurrentTimestamp / 1000 - prevTimeStamp / 1000);

    ++mNumPendingBuffers;
	
#if DEBUG_PENDING_BUFFERS
    mPendingBuffers.push_back(*buffer);
#endif

    ALOGV("returning mbuf %p", *buffer);

    return OK;
}

static buffer_handle_t getMediaBufferHandle(MediaBuffer *buffer) {
    // need to convert to char* for pointer arithmetic and then
    // copy the byte stream into our handle
    buffer_handle_t bufferHandle;
    memcpy(&bufferHandle, (char*)(buffer->data()) + 4, sizeof(buffer_handle_t));
    return bufferHandle;
}

void SurfaceMediaSource::signalBufferReturned(MediaBuffer *buffer) {

    bool foundBuffer = false;
	ALOGV("signalBufferReturned");
    Mutex::Autolock lock(mMutex);
	ALOGV("signalBufferReturned size8 %x",buffer);
	

		buffer_handle_t bufferHandle = getMediaBufferHandle(buffer);
		for (size_t i = 0; i < mCurrentBuffers.size(); i++) {
			if (mCurrentBuffers[i]->handle == bufferHandle) {
				mCurrentBuffers.removeAt(i);
				foundBuffer = true;
				break;
			}
		}
	{
	  int retrtptxt;
	  int64_t sys_time;
	  static int64_t last_time_us = 0;
	  static int64_t start_time_us = 0;
	  static int64_t last_sys_time = 0;
	  int64_t timeUs;
	  sys_time = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;		
	  (buffer)->meta_data()->findInt64(kKeyTime, &timeUs);
	  if(start_time_us == 0)
	  	start_time_us=sys_time - timeUs;
	  if(start_time_us < sys_time -timeUs)
		  start_time_us=sys_time - timeUs;  
	  if((retrtptxt = access("data/test/omx_txt_file",0)) == 0)//test_file!=NULL)
	  {
		  
		  if(omx_txt == NULL)
			  omx_txt = fopen("data/test/omx_txt.txt","ab");
		  if(omx_txt != NULL)
		  {
			fprintf(omx_txt,"SurfaceMediaSource::signalBufferReturned   Video sys_time %lld mStartTimeNs %lld %lld timeUs %lld delta %lld %lld  %lld buffer_handle %x mNumFramesReceived %d\n",
				sys_time,start_time_us ,mStartTimeNs,timeUs ,
				(timeUs - last_time_us)
				,sys_time - start_time_us -timeUs,
				sys_time - last_sys_time,bufferHandle, 
				mNumFramesReceived);
			ALOGV("BufferQueue signalBufferReturned SurfaceMediaSource::read Video sys_time %lld mStartTimeNs %lld %lld timeUs %lld delta %lld %lld  %lld buffer_handle %x mNumPendingBuffers %d mNumFramesReceived %d\n",
				sys_time,start_time_us ,mStartTimeNs,timeUs ,
				(timeUs - last_time_us)
				,sys_time - start_time_us -timeUs,
				sys_time - last_sys_time,bufferHandle,mNumPendingBuffers,
				mNumFramesReceived);
			fflush(omx_txt);
		  }
	  }
	  last_sys_time = sys_time;
	  last_time_us = timeUs;
	}
    if (!foundBuffer) {
        ALOGW("returned buffer was not found in the current buffer list");
    }
    for (int id = 0; id < BufferQueue::NUM_BUFFER_SLOTS; id++) {
        if (mBufferSlot[id] == NULL) {
            continue;
        }

        if (bufferHandle == mBufferSlot[id]->handle) {
            ALOGV("Slot %d returned, matches handle = %p", id,
                    mBufferSlot[id]->handle);

            mBufferQueue->releaseBuffer(id, EGL_NO_DISPLAY, EGL_NO_SYNC_KHR,
                    Fence::NO_FENCE);

            buffer->setObserver(0);
            buffer->release();

            foundBuffer = true;
            break;
        }
    }

    if (!foundBuffer) {
        CHECK(!"signalBufferReturned: bogus buffer");
    }
#if DEBUG_PENDING_BUFFERS
    for (size_t i = 0; i < mPendingBuffers.size(); ++i) {
        if (mPendingBuffers.itemAt(i) == buffer) {
            mPendingBuffers.removeAt(i);
            break;
        }
    }
#endif

    --mNumPendingBuffers;
    mMediaBuffersAvailableCondition.broadcast();
}

// Part of the BufferQueue::ConsumerListener
void SurfaceMediaSource::onFrameAvailable() {
	static int64_t last_time=0;
	int64_t systime = systemTime(SYSTEM_TIME_MONOTONIC) / 1000;
    ALOGV("onFrameAvailable time %lld %lld delta %lld",
		last_time,systime,systime-last_time);
	last_time = systime;
    sp<FrameAvailableListener> listener;
    { // scope for the lock
        Mutex::Autolock lock(mMutex);
        mFrameAvailableCondition.broadcast();
        listener = mFrameAvailableListener;
    }

    if (listener != NULL) {
        ALOGV("actually calling onFrameAvailable");
        listener->onFrameAvailable();
    }
}

// SurfaceMediaSource hijacks this event to assume
// the prodcuer is disconnecting from the BufferQueue
// and that it should stop the recording
void SurfaceMediaSource::onBuffersReleased() {
    ALOGV("onBuffersReleased");

    Mutex::Autolock lock(mMutex);

    mFrameAvailableCondition.signal();

    for (int i = 0; i < BufferQueue::NUM_BUFFER_SLOTS; i++) {
       mBufferSlot[i] = 0;
    }
}

} // end of namespace android
