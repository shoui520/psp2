#ifndef IPMI_CLIENT_IMPL_H
#define IPMI_CLIENT_IMPL_H

#include <stddef.h>
#include <stdint.h>
#include <kernel.h>
#include "client.h"
#include "psp2/event_flag.h"
#include "psp2/msg_pipe.h"
#include "psp2/mutex.h"
#include "psp2/semaphore.h"

/* ClientImpl::m_state: connect moves it from disconnected to connected, terminateConnection ends it. */
#define IPMI_CLIENT_STATE_DISCONNECTED	1
#define IPMI_CLIENT_STATE_CONNECTED	2
#define IPMI_CLIENT_STATE_TERMINATED	3

namespace IPMI {
namespace impl {

/*
 * Sony's IPMI::impl::ClientImpl (client_impl.cpp): the client end, a set of kernel objects
 * named after the service, the process and the client's index (ipc_name.h). The arrays it
 * sizes from its Config follow the object in the caller's buffer (getBufferSize).
 */
class ClientImpl : public Client {
public:
	ClientImpl();
	virtual ~ClientImpl();

	static SceSize getBufferSize(const Config *pConfig);
	int initialize(const Config *pConfig, void *pUserData);

	virtual int destroy();
	virtual int connect(const void *pParam, SceSize paramSize, int *pResult);
	virtual int disconnect();
	virtual int terminateConnection();
	virtual int invokeSyncMethod(SceUInt32 methodId, const void *pIn, SceSize inSize,
				     int *pResult, void *pOut, SceSize *pOutSize,
				     SceSize outSize);
	virtual int invokeSyncMethod(SceUInt32 methodId, const DataInfo *pInList,
				     SceUInt32 inCount, int *pResult, BufferInfo *pOutList,
				     SceUInt32 outCount);
	virtual int invokeAsyncMethod(SceUInt32 methodId, const void *pIn, SceSize inSize,
				      int *pRequestId);
	virtual int invokeAsyncMethod(SceUInt32 methodId, const DataInfo *pInList,
				      SceUInt32 inCount, int *pRequestId);
	virtual int tryGetResult(SceUInt32 methodId, int requestId, int *pResult,
				 void *pOut, SceSize outSize, SceSize *pOutSize);
	virtual int tryGetResult(SceUInt32 methodId, int requestId, int *pResult,
				 BufferInfo *pOutList, SceUInt32 outCount);
	virtual int pollEventFlag(SceUInt32 index, SceUInt32 bitPattern, SceUInt32 waitMode,
				  SceUInt32 *pResultPattern);
	virtual int waitEventFlag(SceUInt32 index, SceUInt32 bitPattern, SceUInt32 waitMode,
				  SceUInt32 *pResultPattern, SceUInt32 *pTimeout);
	virtual void *getUserData();
	virtual int getMsg(SceUInt32 index, void *pBuffer, SceSize *pSize, SceSize bufferSize,
			   SceUInt32 *pTimeout);
	virtual int tryGetMsg(SceUInt32 index, void *pBuffer, SceSize *pSize,
			      SceSize bufferSize);

private:
	/* One outstanding asynchronous call: where its result comes back. */
	struct AsyncCall {
		int requestId;
		psp2::MsgPipe responsePipe;
		psp2::Semaphore responseCounter;
		SceUInt32 state;
		SceUInt32 methodId;
		SceUInt32 magic;
		int result;
		SceSize resultSize;

		AsyncCall()
		{
			requestId = -1;
			state = 0;
			methodId = 0;
			magic = 0xDEADBEEF;
			result = -1;
			resultSize = 0;
		}
	};

	/* One message queue the server sends into. */
	struct MsgQueue {
		psp2::Mutex mutex;
		psp2::Semaphore counter;
		psp2::MsgPipe pipe;
		SceUInt32 msgCount;
		SceUInt32 readIndex;
		SceUInt32 writeIndex;
		SceUInt32 waiterCount;
		SceSize bufferSize;

		MsgQueue()
		{
			msgCount = 0;
			readIndex = 0;
			writeIndex = 0;
			waiterCount = 0;
		}
	};

	int finalize_msg_queue_stuff();
	void finalize_msg_queues(SceUInt32 count);
	int finalize_asyncMI_stuff();
	int initialize_msg_queue_stuff(const Config *pConfig, SceUID pid, int openLimit);
	int initialize_eventflag_stuff(const Config *pConfig, SceUID pid);
	int initialize_asyncMI_stuff(const Config *pConfig, SceUID pid, int openLimit);
	int initialize_syncMI_stuff(const Config *pConfig, SceUID pid, int openLimit);
	int initialize_request_stuff(const Config *pConfig, SceUID pid, int openLimit);
	void finalize_request_stuff();
	int finalize_syncMI_stuff();
	void finalize_eventflag_stuff();
	int receive_connect_response(int *pResult);
	int is_server_alive();
	int wake_up_server_dispatcher();
	int send_connect_request(const void *pParam, SceSize paramSize);
	int send_disconnect_request();
	int receive_sync_response(int *pResult, void *pOut, SceSize *pOutSize, SceSize outSize);
	int send_sync_call_request(SceUInt32 methodId, const void *pIn, SceSize inSize,
				   SceSize outSize);
	int send_sync_multi_call_request(SceUInt32 methodId, const DataInfo *pInList,
					 SceUInt32 inCount, const SceSize *pInSizes, SceSize inTotal,
					 const BufferInfo *pOutList, SceUInt32 outCount,
					 const SceSize *pOutSizes);

	char m_serviceName[16];
	SceUInt32 m_reserved;
	void *m_pUserData;
	SceUInt32 m_clientIndex;
	SceUInt32 m_sessionId;
	volatile int32_t m_state;

	psp2::Mutex m_requestMutex;
	psp2::MsgPipe m_requestPipe;
	psp2::Semaphore m_requestCounter;

	psp2::Mutex m_syncResponseMutex;
	psp2::MsgPipe m_syncResponsePipe;
	psp2::Semaphore m_syncResponseCounter;
	SceUInt32 m_pendingSyncCallCount;
	SceUInt32 m_syncCallCount;
	SceSize m_maxSyncCallInSize;
	SceSize m_maxSyncCallOutSize;

	psp2::Mutex m_asyncCallMutex;
	SceUInt32 m_pendingAsyncCallCount;
	SceUInt32 m_asyncCallCount;
	AsyncCall *m_pAsyncCalls;
	SceSize m_maxAsyncCallInSize;
	SceSize m_maxAsyncCallOutSize;
	SceUInt32 m_nextRequestId;

	SceUInt32 m_eventFlagCount;
	psp2::EventFlag *m_pEventFlags;

	MsgQueue *m_pMsgQueues;
	SceUInt32 m_msgQueueCount;

	SceUID m_serverPid;
	SceUInt32 m_terminated;
};

}
}

#endif
