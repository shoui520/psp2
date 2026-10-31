#include <libkernel/libkernel.h>
#include <appmgr.h>
#define _Static_assert(expression, message)
#include <appmgr/appmgr.h>
#undef _Static_assert
#include "server_impl.h"
#include "assert.h"
#include "ipc_name.h"
#include "ipmi_error.h"
#include "message.h"

void *operator new(size_t size, void *where);

namespace IPMI {
namespace impl {

ServerImpl::ServerImpl()
{
}

int ServerImpl::initialize(const char *pServiceName, EventHandler *pEventHandler,
			   void *pUserData, SceSize maxRequestSize, SceSize maxMessageSize,
			   SceSize workBufferSize, int budget, int openLimit)
{
	SceAppMgrAppStatus status;
	char name[IPC_NAME_SIZE];
	int ret;

	if (pServiceName[0] == '\0')
		return IPMI_ERROR_INVALID_NAME;
	if (budget == PHYCONT_BUDGET)
		return IPMI_ERROR_BUDGET_UNAVAILABLE;
	IPMI_ASSERT_SUCCESS(sceAppMgrGetStatusByName(NULL, &status));
	if ((status.processAttr & 1) == 0 && status.budgetType != 2)
		return IPMI_ERROR_NOT_PERMITTED;

	ret = m_mutex.create();
	if (ret != 0)
		return ret;
	sceClibSnprintf(name, sizeof(name), "%sServerMP", pServiceName);
	ret = m_pipe.create(name, 0x1000, budget, openLimit);
	if (ret != 0) {
		IPMI_ASSERT_SUCCESS(m_mutex.finalize());
		return ret;
	}
	sceClibSnprintf(name, sizeof(name), "%sServerMC", pServiceName);
	ret = m_messageCounter.create(name, 0xFFFFFF, 0);
	if (ret != 0) {
		IPMI_ASSERT_SUCCESS(m_mutex.finalize());
		IPMI_ASSERT_SUCCESS(m_pipe.close());
		return ret;
	}
	sceClibSnprintf(name, sizeof(name), "%sServerDC", pServiceName);
	ret = m_dispatchCounter.create(name, 0xFFFFFF, 0);
	if (ret != 0) {
		IPMI_ASSERT_SUCCESS(m_mutex.finalize());
		IPMI_ASSERT_SUCCESS(m_pipe.close());
		IPMI_ASSERT_SUCCESS(m_messageCounter.close());
		return ret;
	}
	sceClibStrncpy(m_serviceName, pServiceName, sizeof(m_serviceName));
	m_pEventHandler = pEventHandler;
	m_pUserData = pUserData;
	m_maxRequestSize = maxRequestSize;
	m_maxMessageSize = maxMessageSize;
	m_workBufferSize = workBufferSize;
	m_pNewSessionConfig = NULL;
	m_pSessionHead = NULL;
	m_pSessionTail = NULL;
	m_dispatcher = IPMI_SERVER_DISPATCHER_NONE;
	m_flags = 0;
	return 0;
}

ServerImpl::~ServerImpl()
{
}

int ServerImpl::destroy()
{
	if (m_pSessionHead != NULL)
		return IPMI_ERROR_BUSY;
	if (m_dispatcher != IPMI_SERVER_DISPATCHER_NONE)
		return IPMI_ERROR_DISPATCHER_BUSY;
	IPMI_ASSERT_SUCCESS(m_pipe.close());
	IPMI_ASSERT_SUCCESS(m_messageCounter.close());
	IPMI_ASSERT_SUCCESS(m_dispatchCounter.close());
	IPMI_ASSERT_SUCCESS(m_mutex.finalize());
	return 0;
}

/*
 * Lets the first session serve one request; a session that has nothing to do, or did its
 * work, goes to the end of the list, and a closed one leaves it.
 */
int ServerImpl::try_dispatch_call_request(void *pBuffer, SceSize bufferSize)
{
	SessionImpl *pSession;
	SessionImpl *pNext;
	SessionImpl *pTail;
	int ret;

	pSession = m_pSessionHead;
	if (pSession == NULL)
		return 0;
	pNext = pSession->m_pNext;
	ret = pSession->tryDispatch(pBuffer, bufferSize);
	switch (ret) {
	case 0:
	case IPMI_ERROR_WOULD_BLOCK:
		pTail = m_pSessionTail;
		if (pSession != pTail) {
			m_pSessionHead = pNext;
			m_pSessionTail = pSession;
			pTail->m_pNext = pSession;
			pSession->m_pNext = NULL;
		}
		return ret;
	case IPMI_ERROR_SESSION_CLOSED:
		if (pSession == m_pSessionTail) {
			pNext = NULL;
			m_pSessionTail = NULL;
		}
		m_pSessionHead = pNext;
		return 0;
	case IPMI_ERROR_INVALID_ARGUMENT:
		return 0;
	default:
		IPMI_ASSERT_SUCCESS(ret);
		return 0;
	}
}

/* Drops every session whose client's process has exited, and tells the event handler. */
int ServerImpl::handle_process_exit(message::Header *pHeader, SceSize bufferSize)
{
	SessionImpl *pSession;
	SessionImpl *pPrev;
	SessionImpl *pNext;
	SceSize size;

	IPMI_ASSERT_SUCCESS(m_pipe.tryReceive(pHeader + 1, &size,
					      sizeof(message::ProcessExitNotification) -
					      sizeof(message::Header)));
	IPMI_ASSERT(pHeader->type == IPMI_MESSAGE_TYPE_PROCESS_EXIT);
	pPrev = NULL;
	for (pSession = m_pSessionHead; pSession != NULL; pSession = pNext) {
		pNext = pSession->m_pNext;
		if ((pSession->m_state & IPMI_SESSION_STATE_PROCESS_EXITED) == 0 ||
		    (pSession->m_state & IPMI_SESSION_STATE_KILLED) != 0) {
			pPrev = pSession;
			continue;
		}
		if (pPrev != NULL)
			pPrev->m_pNext = pNext;
		else
			m_pSessionHead = pNext;
		if (pSession == m_pSessionTail)
			m_pSessionTail = pPrev;
		pSession->m_state |= IPMI_SESSION_STATE_KILLED;
		m_pEventHandler->onSessionKilled(pSession);
	}
	return 0;
}

/*
 * A client asks to connect: its event handler may create a session for it (createSession),
 * and the client gets the answer in its request pipe.
 */
int ServerImpl::handle_new_client(message::Header *pHeader, SceSize bufferSize)
{
	message::ConnectRequest *pRequest;
	message::ConnectResponse response;
	ClientConfig config;
	SessionImpl *pSession;
	void *pParam;
	SceSize size;
	SceUInt32 sessionId;
	char name[IPC_NAME_SIZE];
	int error;
	int result;
	int pipeRet;
	int counterRet;

	pRequest = static_cast<message::ConnectRequest *>(pHeader);
	IPMI_ASSERT_SUCCESS(m_pipe.tryReceive(&pRequest->pid,
					      &size,
					      sizeof(message::ConnectRequest) -
					      sizeof(message::Header)));
	IPMI_ASSERT(pRequest->type == IPMI_MESSAGE_TYPE_CONNECT_REQUEST);
	if (pRequest->size > bufferSize || pRequest->paramSize > bufferSize ||
	    pRequest->size + pRequest->paramSize > bufferSize || pRequest->msgQueueCount > 32)
		return IPMI_ERROR_WOULD_BLOCK;
	if (pRequest->msgQueueCount != 0)
		IPMI_ASSERT_SUCCESS(m_pipe.tryReceive(config.msgQueueBufferSize, &size,
						      sizeof(SceSize) * pRequest->msgQueueCount));
	pParam = NULL;
	if (pRequest->paramSize != 0) {
		pParam = reinterpret_cast<char *>(pRequest) + pRequest->size;
		IPMI_ASSERT(pRequest->paramSize <= bufferSize - pRequest->size);
		IPMI_ASSERT_SUCCESS(m_pipe.tryReceive(pParam, &size, pRequest->paramSize));
	}

	config.processId = pRequest->pid;
	config.appId = pRequest->appId;
	config.syncCallCount = pRequest->syncCallCount;
	config.maxSyncCallInSize = pRequest->maxSyncCallInSize;
	config.maxSyncCallOutSize = pRequest->maxSyncCallOutSize;
	config.asyncCallCount = pRequest->asyncCallCount;
	config.maxAsyncCallInSize = pRequest->maxAsyncCallInSize;
	config.maxAsyncCallOutSize = pRequest->maxAsyncCallOutSize;
	config.eventFlagCount = pRequest->eventFlagCount;
	config.msgQueueCount = pRequest->msgQueueCount;
	config.bufferSize = SessionImpl::getBufferSize(&config);
	config.clientIndex = pRequest->clientIndex;

	for (pSession = m_pSessionHead; pSession != NULL; pSession = pSession->m_pNext) {
		if (pSession->getProcessId() != config.processId ||
		    pSession->m_clientIndex != config.clientIndex)
			continue;
		if (sceClibStrncmp(m_serviceName, "SceDownload", 12) != 0) {
			sceClibPrintf("[SceIpmi] Workaround bug#63938 %s %x %u\n", m_serviceName,
				      pSession->getProcessId(), pSession->m_clientIndex);
			return IPMI_ERROR_WOULD_BLOCK;
		}
		sceClibPrintf("[SceIpmi] Workaround bug#63938, bug#69656 %s %x %u\n",
			      m_serviceName, pSession->getProcessId(), pSession->m_clientIndex);
		break;
	}

	m_pNewSession = NULL;
	m_pNewSessionConfig = &config;
	result = m_pEventHandler->onNewClient(this, &config, pParam, pRequest->paramSize);
	pSession = m_pNewSession;
	error = pSession != NULL ? 0 : IPMI_ERROR_CONNECTION_REFUSED;
	sessionId = pSession != NULL ? pSession->m_sessionId : 0;

	psp2::MsgPipe pipe;
	psp2::Semaphore counter;

	get_name_of_client_request_msg_pipe(name, m_serviceName, pRequest->pid,
					    pRequest->clientIndex);
	pipeRet = pipe.open(name);
	get_name_of_client_sync_response_counter_semaphore(name, m_serviceName, pRequest->pid,
							   pRequest->clientIndex);
	counterRet = counter.open(name);
	if ((pipeRet | counterRet) == 0) {
		response.type = IPMI_MESSAGE_TYPE_CONNECT_RESPONSE;
		response.size = sizeof(response);
		response.error = error;
		response.result = result;
		response.sessionId = sessionId;
		IPMI_ASSERT_SUCCESS(pipe.trySend(&response, sizeof(response)));
		IPMI_ASSERT_SUCCESS(counter.signal());
	}
	if (pipeRet == 0)
		IPMI_ASSERT_SUCCESS(pipe.close());
	if (counterRet == 0)
		IPMI_ASSERT_SUCCESS(counter.close());
	return 0;
}

/* Takes the next message from the server's pipe, if its message counter has one. */
int ServerImpl::try_accept_new_client(void *pBuffer, SceSize bufferSize)
{
	message::Header *pHeader;
	SceSize size;
	int ret;

	ret = IPMI_ERROR_WOULD_BLOCK;
	if (m_messageCounter.tryWait() != 0)
		return ret;
	pHeader = static_cast<message::Header *>(pBuffer);
	IPMI_ASSERT_SUCCESS(m_pipe.tryReceive(pHeader, &size, sizeof(message::Header)));
	IPMI_ASSERT(size == sizeof(message::Header));
	switch (pHeader->type) {
	case IPMI_MESSAGE_TYPE_CONNECT_REQUEST:
		ret = handle_new_client(pHeader, bufferSize);
		break;
	case IPMI_MESSAGE_TYPE_PROCESS_EXIT:
		IPMI_ASSERT_SUCCESS(handle_process_exit(pHeader, bufferSize));
		ret = 0;
		break;
	default:
		IPMI_ASSERT(0);
		break;
	}
	return ret;
}

inline SceUInt32 ServerImpl::get_session_count()
{
	SessionImpl *pSession;
	SceUInt32 count;

	count = 0;
	for (pSession = m_pSessionHead; pSession != NULL; pSession = pSession->m_pNext)
		count++;
	return count;
}

int ServerImpl::tryDispatch(void *pBuffer, SceSize bufferSize)
{
	SceUInt32 i;
	int ret;

	if (((SceUIntPtr)pBuffer & 15) != 0)
		return IPMI_ERROR_MISALIGNED_BUFFER;
	if (bufferSize < m_workBufferSize)
		return IPMI_ERROR_BUFFER_TOO_SMALL;
	IPMI_ASSERT_SUCCESS(m_mutex.lock());
	if (m_dispatcher != IPMI_SERVER_DISPATCHER_NONE) {
		IPMI_ASSERT_SUCCESS(m_mutex.unlock());
		return IPMI_ERROR_DISPATCHER_BUSY;
	}
	m_dispatcher = IPMI_SERVER_DISPATCHER_TRY;
	IPMI_ASSERT_SUCCESS(m_mutex.unlock());

	sceKernelCheckCallback();
	while (m_dispatchCounter.tryWait() == 0) {
		ret = try_accept_new_client(pBuffer, bufferSize);
		if (ret == IPMI_ERROR_WOULD_BLOCK) {
			for (i = 0; i < get_session_count(); i++) {
				ret = try_dispatch_call_request(pBuffer, bufferSize);
				if (ret == 0)
					break;
				IPMI_ASSERT(ret == IPMI_ERROR_WOULD_BLOCK);
			}
		} else {
			IPMI_ASSERT_SUCCESS(ret);
		}
	}

	IPMI_ASSERT_SUCCESS(m_mutex.lock());
	m_dispatcher = IPMI_SERVER_DISPATCHER_NONE;
	IPMI_ASSERT_SUCCESS(m_mutex.unlock());
	return 0;
}

int ServerImpl::runDispatcher(void *pBuffer, SceSize bufferSize)
{
	SceUInt32 i;
	int ret;

	if (((SceUIntPtr)pBuffer & 15) != 0)
		return IPMI_ERROR_MISALIGNED_BUFFER;
	if (bufferSize < m_workBufferSize)
		return IPMI_ERROR_BUFFER_TOO_SMALL;
	IPMI_ASSERT_SUCCESS(m_mutex.lock());
	if (m_dispatcher != IPMI_SERVER_DISPATCHER_NONE) {
		IPMI_ASSERT_SUCCESS(m_mutex.unlock());
		return IPMI_ERROR_DISPATCHER_BUSY;
	}
	m_dispatcher = IPMI_SERVER_DISPATCHER_RUN;
	IPMI_ASSERT_SUCCESS(m_mutex.unlock());

	for (;;) {
		IPMI_ASSERT_SUCCESS(m_dispatchCounter.waitCB(NULL));
		IPMI_ASSERT_SUCCESS(m_mutex.lock());
		if ((m_flags & IPMI_SERVER_FLAG_SHUTDOWN) != 0)
			break;
		IPMI_ASSERT_SUCCESS(m_mutex.unlock());
		ret = try_accept_new_client(pBuffer, bufferSize);
		if (ret == IPMI_ERROR_WOULD_BLOCK) {
			for (i = 0; i < get_session_count(); i++) {
				ret = try_dispatch_call_request(pBuffer, bufferSize);
				if (ret == 0)
					break;
				IPMI_ASSERT(ret == IPMI_ERROR_WOULD_BLOCK);
			}
		} else {
			IPMI_ASSERT_SUCCESS(ret);
		}
	}
	m_flags &= ~IPMI_SERVER_FLAG_SHUTDOWN;
	IPMI_ASSERT_SUCCESS(m_mutex.unlock());

	IPMI_ASSERT_SUCCESS(m_mutex.lock());
	m_dispatcher = IPMI_SERVER_DISPATCHER_NONE;
	IPMI_ASSERT_SUCCESS(m_mutex.unlock());
	return 0;
}

int ServerImpl::shutdownDispatcher()
{
	IPMI_ASSERT_SUCCESS(m_mutex.lock());
	if ((m_flags & IPMI_SERVER_FLAG_SHUTDOWN) != 0) {
		IPMI_ASSERT_SUCCESS(m_mutex.unlock());
		return IPMI_ERROR_NOT_INITIALIZED;
	}
	m_flags |= IPMI_SERVER_FLAG_SHUTDOWN;
	IPMI_ASSERT_SUCCESS(m_mutex.unlock());
	IPMI_ASSERT_SUCCESS(m_dispatchCounter.signal());
	return 0;
}

/* Called by the event handler's onNewClient: the session for the client being connected. */
int ServerImpl::createSession(Session **ppSession, void *pUserData, void *pMemory)
{
	SessionImpl *pSession;
	int ret;

	if (ppSession == NULL || pMemory == NULL)
		return IPMI_ERROR_INVALID_ARGUMENT;
	if (((SceUIntPtr)pMemory & 3) != 0)
		return IPMI_ERROR_MISALIGNED;
	if (m_pNewSessionConfig == NULL)
		return IPMI_ERROR_NOT_INITIALIZED;
	pSession = new (pMemory) SessionImpl;
	ret = pSession->initialize(m_pNewSessionConfig, this, pUserData);
	if (ret != 0)
		return ret;
	IPMI_ASSERT(pSession->m_sessionId == 0);
	pSession->m_sessionId = reinterpret_cast<SceUInt32>(pSession);
	m_pNewSessionConfig = NULL;
	m_pNewSession = pSession;
	pSession->m_pNext = m_pSessionHead;
	m_pSessionHead = pSession;
	if (m_pSessionTail == NULL)
		m_pSessionTail = pSession;
	*ppSession = pSession;
	return 0;
}

void *ServerImpl::getUserData()
{
	return m_pUserData;
}

/* Replaced by the sessions' own process termination callbacks (notify_process_exit). */
int ServerImpl::notifyProcessExit(SceUID processId)
{
	return sceClibPrintf("[SceIpmi] *WARNING* Obsolete API call bug#48730, %s\n",
			     m_serviceName);
}

/*
 * A session's process termination callback: wakes the dispatcher with a message, so that
 * handle_process_exit drops the session in the dispatcher's thread.
 */
void ServerImpl::notify_process_exit()
{
	message::ProcessExitNotification notification;

	notification.type = IPMI_MESSAGE_TYPE_PROCESS_EXIT;
	notification.size = sizeof(notification);
	IPMI_ASSERT_SUCCESS(m_pipe.send(&notification, sizeof(notification), NULL));
	IPMI_ASSERT_SUCCESS(m_messageCounter.signal());
	IPMI_ASSERT_SUCCESS(m_dispatchCounter.signal());
}

}
}

/* An empty function the original ends this file with; nothing refers to it. The name is ours. */
void serverImplEnd()
{
}
