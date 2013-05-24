#ifndef TVPAD_SOURCE_H
#define TVPAD_SOURCE_H

/*
#include <stdio.h>
#include <utils/threads.h>
#include <sys/socket.h>
#include <arpa/inet.h> 
*/

#include <media/stagefright/DataSource.h>
#include <media/stagefright/MediaErrors.h>
#include <utils/threads.h>
#include <media/stagefright/Utils.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

namespace android {

class TvpadSource : public DataSource {

public:
	TvpadSource(char *uri);
//	void getData();
	void startThread();

	/* extends from DataSource */
	virtual status_t initCheck() const;
	virtual ssize_t readAt(off64_t offset, void *data, size_t size);
	virtual status_t getSize(off64_t *size);
	void queueEOS(status_t finalResult);
protected:
	~TvpadSource();

private:

	struct LoopBuffer{
		int sum_frame;
		int begin_frame;
		int end_frame;
		uint64_t begin_offset;
		bool over_flow;
	};

	/* buffer for size == 1 */
	struct LoopBuffer mLoopBuffer;
	int feedBuffer(off64_t offset);

	void getIp(char *dst , char *src, int n);
	static void* RecBufferThread(void *me);
	static void* SendBufferThread(void *me);
	
	int testFd;

	char *mUri;

	void readThread();
	void writeThread();
	pthread_t ReadThread;
	bool inWrite;
	bool temp_flag;
	char *rec_buf;
	char *temp_buf;
	char *copy_buf;
	char *read_buf;
	int mSocket_service;
//	sockaddr* serviceAddr;
	struct sockaddr_in serviceAddr;
	struct sockaddr_in clientAddr;
	int mSocket_client;

	uint64_t total_buf_size;
	uint64_t total_buf_offset;
	status_t	mFinalResult;
	/* file debug */
	bool file_debug;
	bool file_end;
	FILE *ts_file;
	FILE *bug_file;
	pthread_t WriteThread;

	// so
	void* libhandle;

}; // TvpadSource

} // android

#endif // MEM_SOURCE_H
