#include <libkernel/libkernel.h>
#include "ipmi_wrapper_server.h"
#include "../ipmi/ipmi_error.h"

void *operator new(size_t size, void *where);

namespace IpmiWrapper {

#define SERVER_MAX(a, b)	((a) > (b) ? (a) : (b))

SceSize Server::getSessionBufferSize(const Config *pConfig)
{
	IPMI::Session::Config sessionConfig;

	sessionConfig.syncCallCount = pConfig->syncCallCount;
	sessionConfig.maxSyncCallInSize = pConfig->maxSyncCallInSize;
	sessionConfig.maxSyncCallOutSize = pConfig->maxSyncCallOutSize;
	sessionConfig.asyncCallCount = pConfig->asyncCallCount;
	sessionConfig.maxAsyncCallInSize = pConfig->maxAsyncCallInSize;
	sessionConfig.maxAsyncCallOutSize = pConfig->maxAsyncCallOutSize;
	sessionConfig.eventFlagCount = (pConfig->callbackCount + 31) >> 5;
	sessionConfig.msgQueueCount = pConfig->msgQueueCount;
	return IPMI::Session::getBufferSize(&sessionConfig);
}

int Server::initialize(const Config *pConfig)
{
	char name[32];
	char *pSessionMemory;
	SceSize sessionSize;
	SceUInt32 i;
	int ret;

	m_maxSessions = pConfig->maxSessions;
	m_callbackCount = pConfig->callbackCount;
	m_msgQueueCount = pConfig->msgQueueCount;
	m_activeSessions = 0;
	m_running = 0;
	m_callbacks = pConfig->callbacks;
	m_pSessions = (SessionEntry *)((char *)(this + 1) + 256);

	IPMI::Session::Config sessionConfig;

	sessionConfig.syncCallCount = pConfig->syncCallCount;
	sessionConfig.maxSyncCallInSize = pConfig->maxSyncCallInSize;
	sessionConfig.maxSyncCallOutSize = pConfig->maxSyncCallOutSize;
	sessionConfig.asyncCallCount = pConfig->asyncCallCount;
	sessionConfig.maxAsyncCallInSize = pConfig->maxAsyncCallInSize;
	sessionConfig.maxAsyncCallOutSize = pConfig->maxAsyncCallOutSize;
	sessionConfig.eventFlagCount = (pConfig->callbackCount + 31) >> 5;
	sessionConfig.msgQueueCount = pConfig->msgQueueCount;
	sessionSize = IPMI::Session::getBufferSize(&sessionConfig);

	pSessionMemory = (char *)(m_pSessions + m_maxSessions) +
		(16 - sizeof(SessionEntry)) * m_maxSessions;
	for (i = 0; i < m_maxSessions; i++) {
		m_pSessions[i].inUse = 0;
		m_pSessions[i].pMemory = pSessionMemory + i * sessionSize;
		m_pSessions[i].pSession = NULL;
	}

	IPMI::Server::Config serverConfig;

	sceClibStrncpy(serverConfig.serviceName, pConfig->name, sizeof(serverConfig.serviceName));
	serverConfig.budget = pConfig->budget;
	serverConfig.pEventHandler = this;
	serverConfig.maxMessageSize = SERVER_MAX(pConfig->maxSyncCallInSize,
						 pConfig->maxAsyncCallInSize);
	ret = IPMI::Server::create(&m_pServer, &serverConfig, this, this + 1);
	if (ret != 0)
		return ret;
	m_pWorkBuffer = pSessionMemory + sessionSize * m_maxSessions;
	m_workBufferSize = IPMI::Server::getWorkBufferSize(&serverConfig);

	sceClibSnprintf(name, sizeof(name), "%sServer", pConfig->name);
	m_threadId = sceKernelCreateThread(name, threadEntry, pConfig->threadPriority,
					   pConfig->threadStackSize, 0, 0, NULL);
	if (m_threadId < 0) {
		if (m_pServer->destroy() != 0)
			__breakpoint(0);
		return m_threadId;
	}
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
		return IPMI_WRAPPER_ERROR_ALREADY_CONNECTED;
	if (sceKernelDeleteThread(m_threadId) != 0)
		__breakpoint(0);
	ret = m_pServer->destroy();
	if (ret != 0)
		return ret;
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
		return IPMI_WRAPPER_ERROR_NOT_CONNECTED;
	m_running = 1;
	pArg = this;
	if (sceKernelStartThread(m_threadId, sizeof(pArg), &pArg) != 0)
		__breakpoint(0);
	return 0;
}

int Server::stop()
{
	if (m_running != 1)
		return IPMI_WRAPPER_ERROR_NOT_CONNECTED;
	m_running = 0;
	if (m_pServer->shutdownDispatcher() != 0)
		__breakpoint(0);
	if (sceKernelWaitThreadEnd(m_threadId, NULL, NULL) != 0)
		__breakpoint(0);
	m_running = 0;
	return 0;
}

/* Sets the client's bit for callback index; the client's checkCallback runs it. */
int Server::signalCallback(IPMI::Session *pSession, SceUInt32 index)
{
	if (index >= m_callbackCount)
		return IPMI_WRAPPER_ERROR_INVALID_INDEX;
	return pSession->setEventFlag(index >> 5, 1 << (index & 31));
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

void Server::onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId,
				  const IPMI::DataInfo *pInList, SceUInt32 inCount,
				  IPMI::BufferInfo *pOutList, SceUInt32 outCount)
{
	int ret;

	if (m_callbacks.onSyncMethod != NULL) {
		ret = m_callbacks.onSyncMethod(pSession, methodId, pInList, inCount, pOutList,
					       outCount);
		if (ret != 0)
			sceClibPrintf("[SceIpmiWrapper] *ERROR* Server::onSyncMethodDispatch() failed. (MethodId %u, Error 0x%x)\n",
				      methodId, ret);
	} else {
		__breakpoint(0);
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
			sceClibPrintf("[SceIpmiWrapper] *ERROR* Server::onAsyncMethodDispatch() failed. (MethodId %u, Error 0x%x)\n",
				      methodId, ret);
	} else {
		__breakpoint(0);
	}
}

int Server::onNewClient(IPMI::Server *pServer, const IPMI::Session::Config *pConfig,
			 const void *pParam, SceSize paramSize)
{
	SessionEntry *pEntry;
	SceUInt32 i;
	int ret;

	if (m_activeSessions == m_maxSessions)
		return IPMI_WRAPPER_ERROR_TOO_MANY_SESSIONS;
	pEntry = NULL;
	for (i = 0; i < m_maxSessions; i++) {
		if (m_pSessions[i].inUse == 0) {
			pEntry = &m_pSessions[i];
			break;
		}
	}
	if (pEntry == NULL)
		__breakpoint(0);
	ret = pServer->createSession(&pEntry->pSession, NULL, pEntry->pMemory);
	if (ret != 0) {
		if (ret == IPMI_ERROR_NO_CLIENT)
			return IPMI_WRAPPER_ERROR_NO_CLIENT;
		__breakpoint(0);
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
		__breakpoint(0);
	if (pEntry->inUse != 1)
		__breakpoint(0);
	if (m_callbacks.onSessionClosedByClient != NULL)
		m_callbacks.onSessionClosedByClient(pSession);
	if (pSession->destroy() != 0)
		__breakpoint(0);
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
		__breakpoint(0);
	if (pEntry->inUse != 1)
		__breakpoint(0);
	if (m_callbacks.onSessionKilled != NULL)
		m_callbacks.onSessionKilled(pSession);
	if (pSession->destroy() != 0)
		__breakpoint(0);
	m_activeSessions--;
	pEntry->pSession = NULL;
	pEntry->inUse = 0;
}

SceSize Server::getBufferSize(const Config *pConfig)
{
	IPMI::Session::Config sessionConfig;
	IPMI::Server::Config serverConfig;

	serverConfig.maxMessageSize = SERVER_MAX(pConfig->maxSyncCallInSize,
						 pConfig->maxAsyncCallInSize);
	sessionConfig.syncCallCount = pConfig->syncCallCount;
	sessionConfig.maxSyncCallInSize = pConfig->maxSyncCallInSize;
	sessionConfig.maxSyncCallOutSize = pConfig->maxSyncCallOutSize;
	sessionConfig.asyncCallCount = pConfig->asyncCallCount;
	sessionConfig.maxAsyncCallInSize = pConfig->maxAsyncCallInSize;
	sessionConfig.maxAsyncCallOutSize = pConfig->maxAsyncCallOutSize;
	sessionConfig.eventFlagCount = (pConfig->callbackCount + 31) >> 5;
	sessionConfig.msgQueueCount = pConfig->msgQueueCount;
	return 16 * pConfig->maxSessions +
		((IPMI::Session::getBufferSize(&sessionConfig) + 15) & ~15) * pConfig->maxSessions +
		((IPMI::Server::getWorkBufferSize(&serverConfig) + 15) & ~15) +
		sizeof(Server) + 256;
}

}
