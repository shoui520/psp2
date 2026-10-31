#include <libkernel/libkernel.h>
#include "blocking_ipc_server.h"
#include "../ipmi/ipmi_error.h"

void *operator new(size_t size, void *where);

namespace BlockingIpc {

#define SERVER_MAX(a, b)	((a) > (b) ? (a) : (b))

int Server::initialize(const Config *pConfig)
{
	char name[32];
	char *pSessionMemory;
	SceSize sessionSize;
	SceUInt32 i;
	SceUID threadId;
	int ret;

	m_maxSessions = pConfig->maxSessions;
	m_eventFlagCount = pConfig->eventFlagCount;
	m_msgQueueCount = pConfig->msgQueueCount;
	m_activeSessions = 0;
	m_running = 0;
	m_slotCount = pConfig->slotCount != 0 ? pConfig->slotCount : pConfig->blockingIpcCount;
	m_callbacks = pConfig->callbacks;
	m_pSessions = (SessionEntry *)((char *)(this + 1) + 256);

	IPMI::Session::Config sessionConfig;

	sessionConfig.syncCallCount = pConfig->syncCallCount;
	sessionConfig.maxSyncCallInSize = pConfig->maxSyncCallInSize;
	sessionConfig.maxSyncCallOutSize = pConfig->maxSyncCallOutSize;
	sessionConfig.asyncCallCount = pConfig->asyncCallCount + pConfig->blockingIpcCount;
	sessionConfig.maxAsyncCallInSize =
		SERVER_MAX(pConfig->maxAsyncCallInSize, pConfig->maxBlockingInSize);
	sessionConfig.maxAsyncCallOutSize =
		SERVER_MAX(pConfig->maxAsyncCallOutSize, pConfig->maxBlockingOutSize);
	sessionConfig.eventFlagCount = pConfig->eventFlagCount + pConfig->blockingIpcCount;
	sessionConfig.msgQueueCount = pConfig->msgQueueCount;
	sessionSize = IPMI::Session::getBufferSize(&sessionConfig);

	pSessionMemory = (char *)(m_pSessions + m_maxSessions) +
		(16 - sizeof(SessionEntry)) * m_maxSessions;
	for (i = 0; i < m_maxSessions; i++) {
		m_pSessions[i].inUse = 0;
		m_pSessions[i].serial = 0;
		m_pSessions[i].pMemory = pSessionMemory + i * sessionSize;
		m_pSessions[i].pSession = NULL;
	}

	IPMI::Server::Config serverConfig;

	serverConfig.budget = pConfig->budget;
	sceClibStrncpy(serverConfig.serviceName, pConfig->name, sizeof(serverConfig.serviceName));
	serverConfig.pUserData = this;
	serverConfig.maxMessageSize = SERVER_MAX(SERVER_MAX(pConfig->maxSyncCallInSize,
		pConfig->maxAsyncCallInSize), pConfig->maxBlockingInSize);
	ret = IPMI::Server::create(&m_pServer, &serverConfig, this, this + 1);
	if (ret != 0)
		return ret;
	m_pWorkBuffer = pSessionMemory + sessionSize * m_maxSessions;
	m_workBufferSize = IPMI::Server::getWorkBufferSize(&serverConfig);

	sceClibSnprintf(name, sizeof(name), "%sServer", pConfig->name);
	threadId = sceKernelCreateThread(name, threadEntry, pConfig->threadPriority,
					 pConfig->threadStackSize, 0, 0, NULL);
	if (threadId < 0) {
		if (m_pServer->destroy() != 0)
			__builtin_trap();
		return threadId;
	}
	m_threadId = threadId;
	return 0;
}

int Server::create(Server **ppServer, const Config *pConfig, void *pMemory)
{
	Server *pServer = new (pMemory) Server;
	int ret;

	ret = pServer->initialize(pConfig);
	if (ret != 0)
		pServer->~Server();
	else
		*ppServer = pServer;
	return ret;
}

int Server::finalize()
{
	int ret;

	if (m_activeSessions != 0 || m_running != 0)
		return BLOCKING_IPC_ERROR_ALREADY_CONNECTED;
	if (m_threadId > 0) {
		if (sceKernelDeleteThread(m_threadId) != 0)
			__builtin_trap();
		m_threadId = -1;
	}
	if (m_pServer != NULL) {
		ret = m_pServer->destroy();
		if (ret != 0)
			return ret;
		m_pServer = NULL;
	}
	return 0;
}

int Server::destroy()
{
	int ret;

	ret = finalize();
	if (ret != 0)
		return ret;
	this->~Server();
	return 0;
}

int Server::start()
{
	Server *pArg;

	if (m_running != 0)
		return BLOCKING_IPC_ERROR_NOT_CONNECTED;
	m_running = 1;
	pArg = this;
	if (sceKernelStartThread(m_threadId, sizeof(pArg), &pArg) != 0)
		__builtin_trap();
	return 0;
}

int Server::stop()
{
	if (m_running != 1)
		return BLOCKING_IPC_ERROR_NOT_CONNECTED;
	m_running = 0;
	if (m_pServer->shutdownDispatcher() != 0)
		__builtin_trap();
	if (sceKernelWaitThreadEnd(m_threadId, NULL, NULL) != 0)
		__builtin_trap();
	m_running = 0;
	return 0;
}

int Server::runDispatcher()
{
	return m_pServer->runDispatcher(m_pWorkBuffer, m_workBufferSize);
}

int Server::threadEntry(SceSize argSize, void *pArgBlock)
{
	Server *pServer = *(Server **)pArgBlock;

	return pServer->runDispatcher();
}

void Server::onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId, void *pIn,
				  SceSize inSize, SceUInt32 outCount, void *pOut, SceSize outSize)
{
	int result;

	switch (methodId) {
	case METHOD_CREATE_REQUEST:
		result = m_callbacks.createRequest(pSession, pIn, inSize);
		if (pSession->respondToSyncMethodRequest(result, (const void *)NULL, 0) != 0)
			__builtin_trap();
		break;
	case METHOD_DESTROY_REQUEST:
		result = m_callbacks.destroyRequest(pSession, *(int *)pIn);
		if (pSession->respondToSyncMethodRequest(result, (const void *)NULL, 0) != 0)
			__builtin_trap();
		break;
	case METHOD_ABORT_REQUEST:
		result = m_callbacks.abortRequest(pSession, *(int *)pIn);
		if (pSession->respondToSyncMethodRequest(result, (const void *)NULL, 0) != 0)
			__builtin_trap();
		break;
	}
}

void Server::onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId,
				  const IPMI::DataInfo *pInList, SceUInt32 inCount,
				  IPMI::BufferInfo *pOutList, SceUInt32 outCount)
{
	int ret;

	if (m_callbacks.onSyncMethod != NULL) {
		ret = m_callbacks.onSyncMethod(pSession, methodId, pInList, inCount, pOutList,
					       outCount);
		if (ret != 0)
			sceClibPrintf("[SceBlockingIpc] *ERROR* Server::onSyncMethodDispatch() failed. (MethodId %u, Error 0x%x)\n",
				      methodId, ret);
	} else {
		__builtin_trap();
	}
}

void Server::onAsyncMethodDispatch(IPMI::Session *pSession, SceUInt32 requestId,
				   SceUInt32 methodId, const IPMI::DataInfo *pInList,
				   SceUInt32 inCount)
{
	int ret;

	if (m_callbacks.onAsyncMethod != NULL) {
		ret = m_callbacks.onAsyncMethod(pSession, requestId, methodId, pInList, inCount);
		if (ret != 0)
			sceClibPrintf("[SceBlockingIpc] *ERROR* Server::onAsyncMethodDispatch() failed. (MethodId %u, Error 0x%x)\n",
				      methodId, ret);
	} else {
		__builtin_trap();
	}
}

int Server::onNewClient(IPMI::Server *pServer)
{
	SessionEntry *pEntry;
	SceUInt32 i;
	int ret;

	if (m_activeSessions == m_maxSessions)
		return BLOCKING_IPC_ERROR_TOO_MANY_SESSIONS;
	pEntry = NULL;
	for (i = 0; i < m_maxSessions; i++) {
		if (m_pSessions[i].inUse == 0 &&
		    (pEntry == NULL || pEntry->serial > m_pSessions[i].serial))
			pEntry = &m_pSessions[i];
	}
	if (pEntry == NULL)
		__builtin_trap();
	pEntry->serial++;
	ret = pServer->createSession(&pEntry->pSession, NULL, pEntry->pMemory);
	if (ret != 0) {
		if (ret == IPMI_ERROR_NO_CLIENT)
			return BLOCKING_IPC_ERROR_NO_CLIENT;
		__builtin_trap();
	}
	pEntry->inUse = 1;
	m_activeSessions++;
	if (m_callbacks.onSessionCreated != NULL)
		m_callbacks.onSessionCreated(pEntry->pSession);
	return 0;
}

void Server::onSessionClosedByClient(IPMI::Session *pSession)
{
	SessionEntry *pEntry;
	SceUInt32 i;

	pEntry = NULL;
	for (i = 0; i < m_maxSessions; i++) {
		if (m_pSessions[i].pSession == pSession) {
			pEntry = &m_pSessions[i];
			break;
		}
	}
	if (pEntry == NULL)
		__builtin_trap();
	if (pEntry->inUse != 1)
		__builtin_trap();
	if (m_callbacks.onSessionClosedByClient != NULL)
		m_callbacks.onSessionClosedByClient(pSession);
	if (pSession->destroy() != 0)
		__builtin_trap();
	m_activeSessions--;
	pEntry->pSession = NULL;
	pEntry->inUse = 0;
}

void Server::onSessionKilled(IPMI::Session *pSession)
{
	SessionEntry *pEntry;
	SceUInt32 i;

	pEntry = NULL;
	for (i = 0; i < m_maxSessions; i++) {
		if (m_pSessions[i].pSession == pSession) {
			pEntry = &m_pSessions[i];
			break;
		}
	}
	if (pEntry == NULL)
		__builtin_trap();
	if (pEntry->inUse != 1)
		__builtin_trap();
	if (m_callbacks.onSessionKilled != NULL)
		m_callbacks.onSessionKilled(pSession);
	if (pSession->destroy() != 0)
		__builtin_trap();
	m_activeSessions--;
	pEntry->pSession = NULL;
	pEntry->inUse = 0;
}

SceSize Server::getBufferSize(const Config *pConfig)
{
	IPMI::Session::Config sessionConfig;
	IPMI::Server::Config serverConfig;

	serverConfig.maxMessageSize = SERVER_MAX(SERVER_MAX(pConfig->maxSyncCallInSize,
		pConfig->maxAsyncCallInSize), pConfig->maxBlockingInSize);
	sessionConfig.syncCallCount = pConfig->syncCallCount;
	sessionConfig.maxSyncCallInSize = pConfig->maxSyncCallInSize;
	sessionConfig.maxSyncCallOutSize = pConfig->maxSyncCallOutSize;
	sessionConfig.asyncCallCount = pConfig->asyncCallCount + pConfig->blockingIpcCount;
	sessionConfig.maxAsyncCallInSize =
		SERVER_MAX(pConfig->maxAsyncCallInSize, pConfig->maxBlockingInSize);
	sessionConfig.maxAsyncCallOutSize =
		SERVER_MAX(pConfig->maxAsyncCallOutSize, pConfig->maxBlockingOutSize);
	sessionConfig.eventFlagCount = pConfig->eventFlagCount + pConfig->blockingIpcCount;
	sessionConfig.msgQueueCount = pConfig->msgQueueCount;
	return 16 * pConfig->maxSessions +
		((IPMI::Session::getBufferSize(&sessionConfig) + 15) & ~15) * pConfig->maxSessions +
		((IPMI::Server::getWorkBufferSize(&serverConfig) + 15) & ~15) +
		sizeof(Server) + 256;
}

}
