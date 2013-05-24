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

#ifndef AVC_DECODER_FLASH_H_

#define AVC_DECODER_FLASH_H_

#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaSource.h>
#include <utils/Vector.h>
#include <vpu_api.h>

#define ON2_AVC_FLASH_DEC	1


namespace android {
template <typename Type>
class Queue{
public:
	Queue(){};
	~Queue(){
		queue.clear();
			};
inline	Type &front()
			{
				return queue.editItemAt(0);
			};	
inline    const Type &end()
			{
				return queue.top();
			};
inline	void push(const Type &x)
			{
				queue.push(x);
			};
inline	void pop(Type &x)
			{
				if(!isEmpty())
				{
					x = queue.editItemAt(0);
					queue.removeItemsAt(0);
				}
			};
inline	int32_t size()
			{	
				return queue.size();
			}
inline	bool isEmpty() const
			{
				return queue.isEmpty();
			}
private:
	Vector<Type> queue;
};

struct AVCDecoder_FLASH : public MediaSource,
                    public MediaBufferObserver {
    AVCDecoder_FLASH(const sp<MediaSource> &source);

    virtual status_t start(MetaData *params);
    virtual status_t stop();

    virtual sp<MetaData> getFormat();

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options);

    virtual void signalBufferReturned(MediaBuffer *buffer);

protected:
    virtual ~AVCDecoder_FLASH();

private:
    sp<MediaSource> mSource;
    bool mStarted;

    sp<MetaData> mFormat;

    Vector<MediaBuffer *> mCodecSpecificData;

    MediaBuffer *mInputBuffer;

    int64_t mPendingSeekTimeUs;
    MediaSource::ReadOptions::SeekMode mPendingSeekMode;

    int64_t mTargetTimeUs;

    bool mSPSSeen;
    bool mPPSSeen;

    void addCodecSpecificData(const uint8_t *data, size_t size);







	int32_t mts_en;
	int32_t mWidth;
	int32_t mHeight;
	int32_t mYuvLen;
	VPU_API sDecApi;
    void *pOn2Dec;
  int32_t _success;
	Queue<int64_t>  mInputTime;
	bool mUseInputTime;
    AVCDecoder_FLASH(const AVCDecoder_FLASH &);
    AVCDecoder_FLASH &operator=(const AVCDecoder_FLASH &);
};

}  // namespace android

#endif  // AVC_DECODER_H_
