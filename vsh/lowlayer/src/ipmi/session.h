#ifndef IPMI_SESSION_H
#define IPMI_SESSION_H

#include <stddef.h>
#include <kernel.h>
#include "psp2/msg_pipe.h"

namespace IPMI {

/* The server's end of one client's connection (Sony's IPMI::Session, session_impl.cpp). */
class Session {
public:
	/*
	 * What the server allocates for one client: the client's Config, as its ConnectRequest
	 * gives it, and the session's buffer size (getBufferSize).
	 */
	struct Config {
		SceUID processId;
		int appId;
		SceUInt32 syncCallCount;
		SceSize maxSyncCallInSize;
		SceSize maxSyncCallOutSize;
		SceUInt32 asyncCallCount;
		SceSize maxAsyncCallInSize;
		SceSize maxAsyncCallOutSize;
		SceUInt32 eventFlagCount;
		SceUInt32 msgQueueCount;
		SceSize msgQueueBufferSize[32];
		SceSize bufferSize;
	};

	static SceSize getBufferSize(const Config *pConfig);

	/*
	 * Slots 2 and 7 are the ones BlockingIpc calls; the getters' and the later slots'
	 * names are provisional until session_impl.cpp is converted.
	 */
	virtual ~Session();
	virtual int destroy() = 0;
	virtual SceUID getProcessId() = 0;
	virtual SceUID getThreadId() = 0;
	virtual void *getUserData() = 0;
	virtual void *getClientUserData() = 0;
	virtual int respondToSyncMethodRequest(int result, const void *pOut, SceSize outSize) = 0;
	virtual int sendAsyncResult(SceUInt32 requestId, int result, const void *pOut,
				    SceSize outSize) = 0;
	virtual int sendMsg(SceUInt32 index, const void *pMsg, SceSize msgSize) = 0;
	virtual int setEventFlag(SceUInt32 index, SceUInt32 bitPattern) = 0;
	virtual int respondToSyncMethodRequest(int result, const BufferInfo *pOutList,
					       SceUInt32 outCount) = 0;
	virtual int sendAsyncResult(SceUInt32 requestId, int result, const BufferInfo *pOutList,
				    SceUInt32 outCount) = 0;
};

}

#endif
