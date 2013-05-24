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

#ifndef RTP_SOURCE_H_

#define RTP_SOURCE_H_

#include <stdio.h>

#include <media/stagefright/DataSource.h>
#include <media/stagefright/MediaErrors.h>
#include <utils/threads.h>
#include <media/stagefright/Utils.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <linux/tcp.h>
namespace android {

class RtpSource : public DataSource {
public:
    RtpSource(const char *filename,int type = 0);
    RtpSource(unsigned long addr,unsigned short rx_port,unsigned short tx_port);

    virtual status_t initCheck() const;

    virtual ssize_t readAt(off64_t offset, void *data, size_t size);
	void queueEOS(status_t finalResult);

    virtual status_t getSize(off64_t *size);
    virtual ssize_t seek( off64_t offset, int whence);

    void* ThreadWrapper(void *);
	static void* rec_data(void* me);
	void* ThreadWrapper_Rtp(void *);
	static void* rec_data_Rtp(void* me);
protected:
    virtual ~RtpSource();

private:
    long mOffset;
    long mLength;
    long head_offset;
    long tail_offset;
    long unalign_len[2];
    long data_len;
    long file_offset;
    long connect_flag;

//    int64_t file_offset;
    Mutex mLock;
    int mSocket_Rec_Rtp;
    uint32_t   local_port;
    uint32_t   remote_port;
	int			rx_port;
	int			tx_port;
	int			tx_addr;	
	int mSocket_Rec_Client;
	struct sockaddr_in mRTPAddr_Local;
	struct sockaddr_in mRTPAddr_Client;
    struct sockaddr_in mRTPAddr;
    struct sockaddr_in mRTP_REC_Addr;
    uint8_t *rec_buf;
    pthread_t mThread;
	pthread_t mThread_Rtp;
	status_t	mFinalResult;
    int 		pack_num;
    unsigned char *temp_buf;
	unsigned char *align_buf[2];
	int		connect_Type;
	int		player_type;
    int end_flag ;
    int do_end ;
	struct pollfd fds[2];
	char* data_url;
	int		resync_flag;
	int	reserved[16];
    RtpSource(const RtpSource &);
    RtpSource &operator=(const RtpSource &);
};

}  // namespace android

#endif  // RTP_SOURCE_H_

