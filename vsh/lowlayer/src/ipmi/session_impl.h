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

	int initialize(const ClientConfig *pConfig, ServerImpl *pServer, void *pUserData);
	int tryDispatch(void *pBuffer, SceSize bufferSize);

	virtual int destroy();
	virtual SceUID getProcessId();
	virtual SceUID getThreadId();
	virtual void *getUserData();
	virtual void *getClientUserData();
	virtual int respondToSyncMethodRequest(int result, const void *pOut, SceSize outSize);
	virtual int sendAsyncResult(SceUInt32 requestId, int result, const void *pOut,
				    SceSize outSize);
	virtual int sendMsg(SceUInt32 index, const void *pMsg, SceSize msgSize);
	virtual int setEventFlag(SceUInt32 index, SceUInt32 bitPattern);
	virtual int respondToSyncMethodRequest(int result, const BufferInfo *pOutList,
					       SceUInt32 outCount);
	virtual int sendAsyncResult(SceUInt32 requestId, int result, const BufferInfo *pOutList,
				    SceUInt32 outCount);

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
	bool m_requestPending;
	SceSize m_requestSize;

	psp2::MsgPipe m_syncResponsePipe;
	psp2::Semaphore m_syncResponseCounter;
	bool m_syncResponsePending;
	SceSize m_syncResponseSize;

	SceUInt32 m_pendingSyncCallCount;
	SceSize m_maxAsyncCallOutSize;
	SceUInt32 m_asyncCallCount;
	void *m_pAsyncCalls;
	SceUInt32 m_eventFlagCount;
	psp2::EventFlag *m_pEventFlags;
	SceUInt32 m_msgQueueCount;
	void *m_pMsgQueues;
};

}
}

#endif
