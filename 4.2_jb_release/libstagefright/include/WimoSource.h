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

#ifndef MIRRORING_SOURCE_H_

#define MIRRORING_SOURCE_H_

#include <stdio.h>

#include <media/stagefright/DataSource.h>
#include <media/stagefright/MediaErrors.h>
#include <utils/threads.h>
#include <media/stagefright/Utils.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
namespace android {

class WimoSource : public DataSource {
public:
    WimoSource(const char *filename);
    WimoSource(unsigned long addr,unsigned short rx_port,unsigned short tx_port);

    virtual status_t initCheck() const;

    virtual ssize_t readAt(off64_t offset, void *data, size_t size);
	void queueEOS(status_t finalResult);

    virtual status_t getSize(off64_t *size);
    virtual ssize_t seek( off64_t offset, int whence);

    void* ThreadWrapper(void *);
    void* ThreadWrapper_send(void *);
	typedef void *(*receive_pro)(void*);
	static void* rec_data(void* me);
	static void* rec_data_send(void* me);
    void startThread();
protected:
    virtual ~WimoSource();

private:
    FILE 	*mFile;
    FILE 	*out_mFile;
    FILE 	*in_mFile;
    int 	mFd;
    long 	mOffset;
    long 	mLength;
    long 	head_offset;
    long 	tail_offset;
    long 	data_len;
    long 	file_offset;
    long 	connect_flag;
	int		udp_packet_size;
	int		cache_size;
    Mutex mLock;
    Condition DataRecCondition;
	
    int mSocket_Video;
    int mSocket_Video_Header;
    int mSocket_Audio;
	
	int	audio_port_remote ;
	int	video_port_remote;
	int	video_header_port_remote;
	
	int 		audio_port_local;
	int				video_port_local;
	int					video_header_port_local;
    struct sockaddr_in 	mRTPAddr;
    struct sockaddr_in 	mRTP_REC_Addr;
    uint8_t 			*rec_buf;

	status_t			mFinalResult;
    int 				pack_num;
	pthread_t 			mWimoThread;
	pthread_t 			mWimorecThread;
    int 				end_flag ;
    int 				do_end ;
	int					reserved[16];
    bool mStartThreadFlag;
    WimoSource(const WimoSource &);
    WimoSource &operator=(const WimoSource &);
};

}  // namespace android

#endif  // RTP_SOURCE_H_

