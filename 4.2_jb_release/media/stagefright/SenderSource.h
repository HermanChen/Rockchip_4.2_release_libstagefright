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

class SenderSource  {
public:
    SenderSource(unsigned long addr, unsigned short remort_port,unsigned short local_port,int		type);
    virtual ~SenderSource();
    virtual status_t initCheck() const;
    virtual ssize_t SendData(const void *data, size_t size,int data_type);
	void queueEOS(status_t finalResult);
    void* ThreadWrapper(void *);
	static void* rec_data(void* me);
	status_t start();
	status_t stop();

private:
	SenderSource(const SenderSource &);
    SenderSource &operator=(const SenderSource &);
   	struct pollfd fds[2];
	struct sockaddr_in  mRTPAddr_RTP[2];
	unsigned long rx_addr;
	unsigned short tx_port;
	unsigned short rx_port;
	pthread_t  mThread;
	int mType;
    int end_flag;
	int do_end;
    int mSocket;
	int	mWrite_Type;
	int connect_flag;
    struct sockaddr_in mRTPAddr;
    struct sockaddr_in mRTCPAddr;
    bool mStarted;

	int	reserved[32];
    
};

}  // namespace android

#endif  // RTP_SOURCE_H_

