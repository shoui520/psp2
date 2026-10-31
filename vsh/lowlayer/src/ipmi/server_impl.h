#ifndef IPMI_SERVER_IMPL_H
#define IPMI_SERVER_IMPL_H

#include <stddef.h>
#include <kernel.h>
#include "server.h"
#include "message.h"
#include "session_impl.h"
#include "psp2/msg_pipe.h"
#include "psp2/mutex.h"
#include "psp2/semaphore.h"

/* ServerImpl::m_dispatcher: which dispatcher runs (tryDispatch or runDispatcher), if any. */
#define IPMI_SERVER_DISPATCHER_NONE	0
#define IPMI_SERVER_DISPATCHER_TRY	1
#define IPMI_SERVER_DISPATCHER_RUN	2

/* ServerImpl::m_flags: shutdownDispatcher asked runDispatcher to return. */
#define IPMI_SERVER_FLAG_SHUTDOWN	0x00000001

namespace IPMI {
namespace impl {

/*
 * Sony's IPMI::impl::ServerImpl (server_impl.cpp): the server end. Clients send their
 * ConnectRequest into the server's pipe ("<service>ServerMP") and count it in its message
 * counter ("ServerMC"); the dispatch counter ("ServerDC") wakes the dispatcher. 0.990 names
 * initialize's first six arguments; 3.6x adds the budget and the open limit, as ClientImpl
 * does.
 */
class ServerImpl : public Server {
public:
	ServerImpl();
	virtual ~ServerImpl();

	int initialize(const char *pServiceName, EventHandler *pEventHandler, void *pUserData,
		       SceSize maxRequestSize, SceSize maxMessageSize,
		       SceSize workBufferSize, int budget, int openLimit);

	virtual int destroy();
	virtual int tryDispatch(void *pBuffer, SceSize bufferSize);
	virtual int runDispatcher(void *pBuffer, SceSize bufferSize);
	virtual int shutdownDispatcher();
	virtual int createSession(Session **ppSession, void *pUserData, void *pMemory);
	virtual void *getUserData();
	virtual int notifyProcessExit(SceUID processId);
	virtual void notify_process_exit();

private:
	int try_dispatch_call_request(void *pBuffer, SceSize bufferSize);
	int handle_process_exit(message::Header *pHeader, SceSize bufferSize);
	int handle_new_client(message::Header *pHeader, SceSize bufferSize);
	int try_accept_new_client(void *pBuffer, SceSize bufferSize);
	SceUInt32 get_session_count();

	psp2::Mutex m_mutex;
	SceUInt32 m_dispatcher;
	SceUInt32 m_flags;
	char m_serviceName[16];
	SceUInt32 m_reserved;
	EventHandler *m_pEventHandler;
	void *m_pUserData;
	psp2::MsgPipe m_pipe;
	psp2::Semaphore m_messageCounter;
	psp2::Semaphore m_dispatchCounter;
	SceSize m_maxRequestSize;
	SceSize m_maxMessageSize;
	SceSize m_workBufferSize;
	const ClientConfig *m_pNewSessionConfig;
	SessionImpl *m_pNewSession;
	SessionImpl *m_pSessionHead;
	SessionImpl *m_pSessionTail;
};

}
}

#endif
