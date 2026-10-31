#ifndef IPMI_SESSION_IMPL_H
#define IPMI_SESSION_IMPL_H

#include <stddef.h>
#include <kernel.h>
#include "session.h"
#include "psp2/event_flag.h"
#include "psp2/msg_pipe.h"
#include "psp2/mutex.h"
#include "psp2/semaphore.h"

/* SessionImpl::m_magic while the session is initialized: its getters check it. */
#define IPMI_SESSION_MAGIC		0xFE36EB91

/*
 * SessionImpl::m_state: the client's process has exited (its process termination callback
 * ran); the server has dropped the session and told its event handler (onSessionKilled).
 */
#define IPMI_SESSION_STATE_PROCESS_EXITED	0x00000001
#define IPMI_SESSION_STATE_KILLED		0x00000002

/* SessionImpl::AsyncCall::state: what the call's client is waiting for. */
#define IPMI_ASYNC_CALL_IDLE		0
#define IPMI_ASYNC_CALL_PENDING		1
#define IPMI_ASYNC_CALL_RESPONDING	2
#define IPMI_ASYNC_CALL_MULTI_PENDING	3

/* The most inputs or outputs one multi-data call carries. */
#define IPMI_MULTI_DATA_MAX		32

namespace IPMI {
namespace impl {

class ServerImpl;

/* What the server hands a new session: the client's Config and the client's index. */
struct ClientConfig : Session::Config {
	SceUInt32 clientIndex;
};

/*
 * Sony's IPMI::impl::SessionImpl (session_impl.cpp): the server's end of one client, the
 * client's kernel objects opened by name (ipc_name.h). The server keeps its sessions in a
 * list (m_pNext); the arrays sized from the client's Config follow the object in the
 * caller's buffer (Session::getBufferSize).
 */
class SessionImpl : public Session {
public:
	SessionImpl();
	virtual ~SessionImpl();

	static SceSize getBufferSize(const Config *pConfig);
	int initialize(const ClientConfig *pConfig, ServerImpl *pServer, void *pUserData);
	int tryDispatch(void *pBuffer, SceSize bufferSize);

	virtual int destroy();
	virtual SceUID getProcessId();
	virtual int getAppId();
	virtual void *getUserData();
	virtual Server *getServer();
	virtual int respondToSyncMethodRequest(int result, const void *pOut, SceSize outSize);
	virtual int respondToSyncMethodRequest(int result, const BufferInfo *pOutList,
					       SceUInt32 outCount);
	virtual int respondToAsyncMethodRequest(int requestId, int result, const void *pOut,
						SceSize outSize);
	virtual int respondToAsyncMethodRequest(SceUInt32 methodId, int requestId, int result,
						const DataInfo *pOutList, SceUInt32 outCount);
	virtual int setEventFlag(SceUInt32 index, SceUInt32 bitPattern);
	virtual int sendMsg(SceUInt32 index, const void *pMsg, SceSize msgSize,
			    SceUInt32 *pTimeout);
	virtual int trySendMsg(SceUInt32 index, const void *pMsg, SceSize msgSize);
	virtual int emptyMsgQueue(SceUInt32 index);

	/*
	 * One of the client's asynchronous calls: the pipe and counter its result goes to, and
	 * the request it is running (state IPMI_ASYNC_CALL_*).
	 */
	struct AsyncCall {
		int requestId;
		psp2::MsgPipe responsePipe;
		psp2::Semaphore responseCounter;
		int eventFlagIndex;
		SceUInt32 eventFlagBitPattern;
		int state;

		AsyncCall()
			: requestId(-1), eventFlagIndex(-1), eventFlagBitPattern(0), state(0)
		{
		}
	};

	/* One of the client's message queues, opened by name. */
	struct MsgQueue {
		psp2::Mutex mutex;
		psp2::Semaphore counter;
		psp2::MsgPipe pipe;
		SceSize bufferSize;
	};

	static int process_termination_callback(int notifyId, int notifyCount, int notifyArg,
						void *pCommon);

	int finalize_msg_queue_stuff();
	void finalize_msg_queues(SceUInt32 count);
	int initialize_msg_queue_stuff(const ClientConfig *pConfig, const char *pServiceName);
	int initialize_event_flag(const ClientConfig *pConfig, const char *pServiceName);
	int initialize_async_call_stuff(const ClientConfig *pConfig, const char *pServiceName);
	int initialize_sync_call_stuff(const ClientConfig *pConfig, const char *pServiceName);
	int initialize_request_stuff(const ClientConfig *pConfig, const char *pServiceName);
	int initialize_process_term_callback(const char *pServiceName);
	void finalize_request_stuff();
	void finalize_sync_call_stuff();
	void finalize_async_call_stuff();
	void finalize_event_flag();
	int handle_session_close_request();
	int handle_async_multi_data_call_request(void *pBuffer, SceSize bufferSize);
	int handle_async_call_request(void *pBuffer, SceSize bufferSize);
	int handle_sync_multi_data_call_request(void *pBuffer, SceSize bufferSize);
	int handle_sync_call_request(void *pBuffer, SceSize bufferSize);

	SessionImpl *m_pNext;
	SceUInt32 m_magic;
	SceUInt32 m_clientIndex;
	SceUInt32 m_state;
	psp2::Mutex m_mutex;
	void *m_pUserData;
	SceUID m_processId;
	int m_appId;
	SceUInt32 m_sessionId;
	ServerImpl *m_pServer;
	SceUID m_processTerminationCallbackId;

	psp2::MsgPipe m_requestPipe;
	psp2::Semaphore m_requestCounter;
	bool m_syncCallPending;
	SceSize m_syncCallOutSize;

	psp2::MsgPipe m_syncResponsePipe;
	psp2::Semaphore m_syncResponseCounter;
	bool m_syncCallIsMulti;
	SceUInt32 m_syncCallOutCount;
	const SceSize *m_pSyncCallOutSizes;
	SceSize m_maxAsyncCallOutSize;
	SceUInt32 m_asyncCallCount;
	AsyncCall *m_pAsyncCalls;
	SceUInt32 m_eventFlagCount;
	psp2::EventFlag *m_pEventFlags;
	SceUInt32 m_msgQueueCount;
	MsgQueue *m_pMsgQueues;
};

}
}

#endif
