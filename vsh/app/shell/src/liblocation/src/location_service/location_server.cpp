/*
 * location_server.cpp: the "SceLocationLoc" and "SceLocationHead" IPMI servers and their
 * session tables (0x811868F4-0x811878D0). The file's and internal names here are ours
 * (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/new>
#include <kernel/threadmgr.h>
#include <ipmi/server.h>
#include <ipmi/session.h>
#include "location_service.h"

using namespace paf;

/* the two server singletons (0x81544CD0, 0x81544CD4) */
static LocationServer *s_locationServer;
static HeadingServer *s_headingServer;

/* (0x811868F4) */
__attribute__((noinline))
LocationServer::LocationServer()
{
	m_mutex.result = sceKernelCreateLwMutex(&m_mutex.work, "SceLocationSession", 2, 0, NULL);
	m_server = NULL;
	for (int i = 0; i < LOCATION_SERVER_MAX_SESSIONS; i++) {
		m_slots[i].session = NULL;
		m_slots[i].handle = 0;
		m_slots[i].requestId = 0;
		m_slots[i].methodId = 0;
	}
	m_sessionCount = 0;
	m_threadId = 0;
	m_stopped = 1;
	m_workBuffer = NULL;
	m_workBufferSize = 0;
}

/* (0x81186956) */
LocationServer::~LocationServer()
{
	if (m_mutex.result == 0) {
		sceKernelDeleteLwMutex(&m_mutex.work);
		m_mutex.result = -1;
	}
}

/* (0x811869BA) */
int LocationServer::Start()
{
	IPMI::Server::Config config;

	sce_paf_strncpy(config.serviceName, "SceLocationLoc", sizeof(config.serviceName));
	config.pEventHandler = this;
	config.budget = 2;

	m_workBufferSize = IPMI::Server::getWorkBufferSize(&config);

	void *serverMem = g_locationHeap->sce_paf_malloc(0x100);
	void *workMem = g_locationHeap->sce_paf_memalign(0x10, m_workBufferSize);

	m_workBuffer = workMem;
	if (serverMem == NULL || workMem == NULL) {
		g_locationHeap->sce_paf_free(serverMem);
		g_locationHeap->sce_paf_free(m_workBuffer);
		m_workBuffer = NULL;
		return SCE_LOCATION_ERROR_NO_SERVER_MEMORY;
	}

	int ret = IPMI::Server::create(&m_server, &config, this, serverMem);

	if (ret != 0) {
		g_locationHeap->sce_paf_free(serverMem);
		g_locationHeap->sce_paf_free(m_workBuffer);
		m_workBuffer = NULL;
		return ret;
	}

	m_stopped = 0;
	m_threadId = sceKernelCreateThread("SceLocationServer", ThreadEntry, 0x1000010A, 0x2000,
					   0, 0, NULL);
	if (m_threadId < 0) {
		m_server->destroy();
		g_locationHeap->sce_paf_free(serverMem);
		g_locationHeap->sce_paf_free(m_workBuffer);
		m_server = NULL;
		m_workBuffer = NULL;
		return m_threadId;
	}

	LocationServer *self = this;

	ret = sceKernelStartThread(m_threadId, sizeof(self), &self);
	if (ret < 0) {
		sceKernelDeleteThread(m_threadId);
		m_server->destroy();
		g_locationHeap->sce_paf_free(serverMem);
		g_locationHeap->sce_paf_free(m_workBuffer);
		m_server = NULL;
		m_workBuffer = NULL;
		return ret;
	}
	return 0;
}

/* (0x81186B54) */
int locationServerStart(void)
{
	if (s_locationServer != NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	LocationServer *server = new (g_locationHeap) LocationServer();

	s_locationServer = server;
	if (server == NULL)
		return SCE_LOCATION_ERROR_NO_SERVER_MEMORY;
	return server->Start();
}

/* (0x81186BA4) */
int LocationServer::Stop()
{
	sceKernelLockLwMutex(&m_mutex.work, 1, NULL);
	if (m_server == NULL) {
		sceKernelUnlockLwMutex(&m_mutex.work, 1);
		return SCE_LOCATION_ERROR_FATAL_ERROR;
	}
	if (m_sessionCount != 0)
		sceKernelUnlockLwMutex(&m_mutex.work, 1);
	m_server->shutdownDispatcher();
	sceKernelWaitThreadEnd(m_threadId, NULL, NULL);
	sceKernelDeleteThread(m_threadId);
	m_threadId = -1;

	int ret = m_server->destroy();

	if (ret != 0) {
		sceKernelUnlockLwMutex(&m_mutex.work, 1);
		return ret;
	}
	g_locationHeap->sce_paf_free(m_server);
	m_server = NULL;
	g_locationHeap->sce_paf_free(m_workBuffer);
	m_workBuffer = NULL;
	sceKernelUnlockLwMutex(&m_mutex.work, 1);
	return 0;
}

/* (0x81186C4A) */
int locationServerStop(void)
{
	int ret = 0;

	if (s_locationServer != NULL) {
		ret = s_locationServer->Stop();
		if (ret == 0) {
			delete s_locationServer;
			s_locationServer = NULL;
		}
	}
	return ret;
}

/* (0x81186C84) */
LocationServer *locationServerGetInstance(void)
{
	return s_locationServer;
}

/* (0x81186C90) */
void LocationServer::NotifyEvent(SceLocationHandle handle, int result, int eventType)
{
	sceKernelLockLwMutex(&m_mutex.work, 1, NULL);
	for (unsigned int i = 0; i < LOCATION_SERVER_MAX_SESSIONS; i++) {
		if (m_slots[i].handle == handle) {
			SceUInt32 bits;

			switch (eventType) {
			case 0:
				bits = 2;
				break;
			case 1:
				bits = 1;
				break;
			case 2:
				bits = 4;
				break;
			case 3:
				bits = 8;
				break;
			case 4:
				bits = 16;
				break;
			case 5:
				bits = 32;
				break;
			case 6:
				bits = 64;
				break;
			case 7:
				bits = 256;
				break;
			case 8:
				bits = 128;
				break;
			case 9:
				bits = 512;
				break;
			default:
				sceKernelUnlockLwMutex(&m_mutex.work, 1);
				return;
			}
			m_slots[i].session->setEventFlag(0, bits);
			sceKernelUnlockLwMutex(&m_mutex.work, 1);
			return;
		}
	}
	sceKernelUnlockLwMutex(&m_mutex.work, 1);
}

/* (0x81186D28) */
void LocationServer::SetEventFlag(SceLocationHandle handle, SceUInt32 pattern)
{
	sceKernelLockLwMutex(&m_mutex.work, 1, NULL);
	for (unsigned int i = 0; i < LOCATION_SERVER_MAX_SESSIONS; i++) {
		if (m_slots[i].handle == handle) {
			m_slots[i].session->setEventFlag(0, pattern);
			sceKernelUnlockLwMutex(&m_mutex.work, 1);
			return;
		}
	}
	sceKernelUnlockLwMutex(&m_mutex.work, 1);
}

/* (0x81186D72) */
int LocationServer::BindHandle(IPMI::Session *session, SceLocationHandle handle)
{
	int index = -1;

	sceKernelLockLwMutex(&m_mutex.work, 1, NULL);
	for (unsigned int i = 0; i < LOCATION_SERVER_MAX_SESSIONS; i++) {
		if (m_slots[i].session == session) {
			m_slots[i].handle = handle;
			index = (signed char)i;
			break;
		}
	}
	sceKernelUnlockLwMutex(&m_mutex.work, 1);
	return index;
}

/* (0x81186DB2) */
void LocationServer::NotifyDenied(SceLocationHandle handle)
{
	sceKernelLockLwMutex(&m_mutex.work, 1, NULL);
	for (unsigned int i = 0; i < LOCATION_SERVER_MAX_SESSIONS; i++) {
		if (m_slots[i].handle == handle) {
			m_slots[i].session->setEventFlag(0, 3);
			sceKernelUnlockLwMutex(&m_mutex.work, 1);
			return;
		}
	}
	sceKernelUnlockLwMutex(&m_mutex.work, 1);
}

/* (0x81186DF6) */
void LocationServer::onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId,
					  const IPMI::DataInfo *pInList, SceUInt32 inCount,
					  IPMI::BufferInfo *pOutList, SceUInt32 outCount)
{
	locationServerDispatch(pSession, methodId, pInList, inCount, pOutList, outCount);
}

/* (0x81186E14) */
void LocationServer::onAsyncMethodDispatch(IPMI::Session *pSession, SceUInt32 requestId,
					   SceUInt32 methodId, const IPMI::DataInfo *pInList,
					   SceUInt32 inCount)
{
	sceKernelLockLwMutex(&m_mutex.work, 1, NULL);
	for (unsigned int i = 0; i < LOCATION_SERVER_MAX_SESSIONS; i++) {
		if (m_slots[i].session == pSession) {
			m_slots[i].requestId = requestId;
			m_slots[i].methodId = methodId;
			break;
		}
	}
	sceKernelUnlockLwMutex(&m_mutex.work, 1);
}

/* (0x81186E72) */
int LocationServer::onNewClient(IPMI::Server *pServer, const IPMI::Session::Config *pConfig,
				const void *pParam, SceSize paramSize)
{
	sceKernelLockLwMutex(&m_mutex.work, 1, NULL);
	if (m_sessionCount == LOCATION_SERVER_MAX_SESSIONS) {
		sceKernelUnlockLwMutex(&m_mutex.work, 1);
		return SCE_LOCATION_ERROR_TOO_MANY_HANDLES;
	}
	m_sessionCount++;

	LocationSessionSlot *slot = NULL;

	for (unsigned int i = 0; i < LOCATION_SERVER_MAX_SESSIONS; i++) {
		if (m_slots[i].session == NULL) {
			slot = &m_slots[i];
			break;
		}
	}

	void *mem = g_locationHeap->sce_paf_malloc(pConfig->bufferSize);

	if (mem == NULL) {
		m_sessionCount--;
		sceKernelUnlockLwMutex(&m_mutex.work, 1);
		return SCE_LOCATION_ERROR_NO_SERVER_MEMORY;
	}
	if (pServer->createSession(&slot->session, this, mem) != 0) {
		m_sessionCount--;
		g_locationHeap->sce_paf_free(mem);
		sceKernelUnlockLwMutex(&m_mutex.work, 1);
		return SCE_LOCATION_ERROR_FATAL_ERROR;
	}
	sceKernelUnlockLwMutex(&m_mutex.work, 1);
	return 0;
}

/* (0x81186F54) */
void LocationServer::onSessionClosedByClient(IPMI::Session *pSession)
{
	sceKernelLockLwMutex(&m_mutex.work, 1, NULL);

	LocationSessionSlot *slot = NULL;

	for (unsigned int i = 0; i < LOCATION_SERVER_MAX_SESSIONS; i++) {
		if (m_slots[i].session == pSession) {
			slot = &m_slots[i];
			break;
		}
	}
	pSession->destroy();
	g_locationHeap->sce_paf_free(pSession);
	slot->session = NULL;
	slot->handle = 0;
	slot->requestId = -1;
	slot->methodId = 0;
	m_sessionCount--;
	sceKernelUnlockLwMutex(&m_mutex.work, 1);
}

/* (0x81186FBE) */
void LocationServer::onSessionKilled(IPMI::Session *pSession)
{
	sceKernelLockLwMutex(&m_mutex.work, 1, NULL);

	LocationSessionSlot *slot = NULL;

	for (unsigned int i = 0; i < LOCATION_SERVER_MAX_SESSIONS; i++) {
		if (m_slots[i].session == pSession) {
			slot = &m_slots[i];
			break;
		}
	}
	if (slot != NULL) {
		locationSvcClose(pSession->getAppId(), slot->handle);
		pSession->destroy();
		g_locationHeap->sce_paf_free(pSession);
		slot->session = NULL;
		slot->handle = 0;
		slot->requestId = -1;
		slot->methodId = 0;
		m_sessionCount--;
	}
	sceKernelUnlockLwMutex(&m_mutex.work, 1);
}

/* (0x81187042) */
int LocationServer::ThreadEntry(SceSize args, void *argp)
{
	LocationServer *self = *(LocationServer **)argp;

	self->m_server->runDispatcher(self->m_workBuffer, self->m_workBufferSize);
	return 0;
}

/* (0x8118705A) */
HeadingServer::HeadingServer()
{
	m_mutexResult = sceKernelCreateLwMutex(&m_mutex, "SceHeadingSession", 2, 0, NULL);
	m_server = NULL;
	for (int i = 0; i < HEADING_SERVER_MAX_SESSIONS; i++) {
		m_slots[i].handleCount = 0;
		m_slots[i].session = NULL;
		m_slots[i].appId = -1;
		for (int j = 0; j < 8; j++)
			m_slots[i].handles[j] = 0;
		m_slots[i].requestId = 0;
		m_slots[i].methodId = 0;
	}
	m_sessionCount = 0;
	m_threadId = 0;
	m_stopped = 1;
	m_workBuffer = NULL;
	m_workBufferSize = 0;
}

/* (0x811870D2) */
HeadingServer::~HeadingServer()
{
	if (m_mutexResult == 0) {
		sceKernelDeleteLwMutex(&m_mutex);
		m_mutexResult = -1;
	}
}

/* (0x81187136) */
int HeadingServer::Start()
{
	if (m_server != NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;
	sce_paf_memset(m_slots, 0, sizeof(m_slots));
	m_sessionCount = 0;

	IPMI::Server::Config config;

	sce_paf_strncpy(config.serviceName, "SceLocationHead", sizeof(config.serviceName));
	config.pEventHandler = this;
	config.budget = 2;

	m_workBufferSize = IPMI::Server::getWorkBufferSize(&config);

	void *serverMem = g_locationHeap->sce_paf_malloc(0x100);
	void *workMem = g_locationHeap->sce_paf_memalign(0x10, m_workBufferSize);

	m_workBuffer = workMem;
	if (serverMem == NULL || workMem == NULL) {
		g_locationHeap->sce_paf_free(serverMem);
		g_locationHeap->sce_paf_free(m_workBuffer);
		m_workBuffer = NULL;
		return SCE_LOCATION_ERROR_NO_SERVER_MEMORY;
	}

	int ret = IPMI::Server::create(&m_server, &config, this, serverMem);

	if (ret != 0) {
		g_locationHeap->sce_paf_free(serverMem);
		g_locationHeap->sce_paf_free(m_workBuffer);
		m_workBuffer = NULL;
		return ret;
	}

	m_stopped = 0;
	m_threadId = sceKernelCreateThread("SceHeadingServer", ThreadEntry, 0x1000010A, 0x2000,
					   0, 0, NULL);
	if (m_threadId < 0) {
		m_server->destroy();
		g_locationHeap->sce_paf_free(serverMem);
		g_locationHeap->sce_paf_free(m_workBuffer);
		m_server = NULL;
		m_workBuffer = NULL;
		return m_threadId;
	}

	HeadingServer *self = this;

	ret = sceKernelStartThread(m_threadId, sizeof(self), &self);
	if (ret < 0) {
		sceKernelDeleteThread(m_threadId);
		m_server->destroy();
		g_locationHeap->sce_paf_free(serverMem);
		g_locationHeap->sce_paf_free(m_workBuffer);
		m_server = NULL;
		m_workBuffer = NULL;
		return ret;
	}
	return 0;
}

/* (0x811872F0) */
int headingServerStart(void)
{
	if (s_headingServer != NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	HeadingServer *server = new (g_locationHeap) HeadingServer();

	s_headingServer = server;
	if (server == NULL)
		return SCE_LOCATION_ERROR_NO_SERVER_MEMORY;
	return server->Start();
}

/* (0x81187342) */
int HeadingServer::Stop()
{
	sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (m_server == NULL) {
		sceKernelUnlockLwMutex(&m_mutex, 1);
		return SCE_LOCATION_ERROR_FATAL_ERROR;
	}
	if (m_sessionCount != 0) {
		sceKernelUnlockLwMutex(&m_mutex, 1);
		return SCE_LOCATION_ERROR_FATAL_ERROR;
	}
	m_server->shutdownDispatcher();
	sceKernelWaitThreadEnd(m_threadId, NULL, NULL);
	sceKernelDeleteThread(m_threadId);
	m_threadId = -1;

	int ret = m_server->destroy();

	if (ret != 0) {
		sceKernelUnlockLwMutex(&m_mutex, 1);
		return ret;
	}
	g_locationHeap->sce_paf_free(m_server);
	g_locationHeap->sce_paf_free(m_workBuffer);
	m_server = NULL;
	m_workBuffer = NULL;
	sceKernelUnlockLwMutex(&m_mutex, 1);
	return 0;
}

/* (0x811873EC) */
int headingServerStop(void)
{
	if (s_headingServer == NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	int ret = s_headingServer->Stop();

	delete s_headingServer;
	s_headingServer = NULL;
	return ret;
}

/* (0x8118742C) */
HeadingServer *headingServerGetInstance(void)
{
	if (s_headingServer == NULL)
		s_headingServer = new (g_locationHeap) HeadingServer();
	return s_headingServer;
}

/* (0x81187466) */
void HeadingServer::NotifyHeading(SceLocationHandle handle, int unused)
{
	sceKernelLockLwMutex(&m_mutex, 1, NULL);
	for (unsigned int i = 0; i < HEADING_SERVER_MAX_SESSIONS; i++) {
		for (unsigned int j = 0; j < 8; j++) {
			if (m_slots[i].handles[j] == handle) {
				m_slots[i].session->setEventFlag(0, handle | 0x10000000);
				sceKernelUnlockLwMutex(&m_mutex, 1);
				return;
			}
		}
	}
	sceKernelUnlockLwMutex(&m_mutex, 1);
}

/* (0x811874C2) */
void HeadingServer::BindHandle(int appId, signed char slotIdx, SceLocationHandle handle)
{
	if (slotIdx < 0)
		return;
	sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (m_slots[slotIdx].appId == appId) {
		m_slots[slotIdx].handles[0] = handle;
		m_slots[slotIdx].handleCount++;
	} else {
		for (unsigned int i = 0; i < HEADING_SERVER_MAX_SESSIONS; i++) {
			if (m_slots[i].appId == appId) {
				for (unsigned int j = 0; j < 8; j++) {
					if (m_slots[i].handles[j] == 0) {
						m_slots[i].handles[j] = handle;
						m_slots[i].handleCount++;
						if (m_slots[i].handleCount > 1)
							m_sessionCount++;
						sceKernelUnlockLwMutex(&m_mutex, 1);
						return;
					}
				}
			}
		}
	}
	sceKernelUnlockLwMutex(&m_mutex, 1);
}

/* (0x8118755E) */
void HeadingServer::UnbindHandle(int appId, SceLocationHandle handle)
{
	sceKernelLockLwMutex(&m_mutex, 1, NULL);
	for (unsigned int i = 0; i < HEADING_SERVER_MAX_SESSIONS; i++) {
		if (m_slots[i].appId == appId && m_slots[i].handleCount > 1) {
			for (unsigned int j = 0; j < 8; j++) {
				if (m_slots[i].handles[j] == handle) {
					m_slots[i].handles[j] = 0;
					m_slots[i].handleCount--;
					m_sessionCount--;
					sceKernelUnlockLwMutex(&m_mutex, 1);
					return;
				}
			}
		}
	}
	sceKernelUnlockLwMutex(&m_mutex, 1);
}

/* (0x811875DE) */
void HeadingServer::onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId,
					 const IPMI::DataInfo *pInList, SceUInt32 inCount,
					 IPMI::BufferInfo *pOutList, SceUInt32 outCount)
{
	headingServerDispatch(pSession, methodId, pInList, inCount, pOutList, outCount);
}

/* (0x811875FC) */
void HeadingServer::onAsyncMethodDispatch(IPMI::Session *pSession, SceUInt32 requestId,
					  SceUInt32 methodId, const IPMI::DataInfo *pInList,
					  SceUInt32 inCount)
{
	sceKernelLockLwMutex(&m_mutex, 1, NULL);
	for (unsigned int i = 0; i < HEADING_SERVER_MAX_SESSIONS; i++) {
		if (m_slots[i].session == pSession) {
			m_slots[i].requestId = requestId;
			m_slots[i].methodId = methodId;
			break;
		}
	}
	sceKernelUnlockLwMutex(&m_mutex, 1);
}

/* (0x8118765A) */
int HeadingServer::onNewClient(IPMI::Server *pServer, const IPMI::Session::Config *pConfig,
			       const void *pParam, SceSize paramSize)
{
	sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (m_sessionCount == HEADING_SERVER_MAX_SESSIONS) {
		sceKernelUnlockLwMutex(&m_mutex, 1);
		return SCE_LOCATION_ERROR_TOO_MANY_HANDLES;
	}
	m_sessionCount++;

	HeadingSessionSlot *slot = NULL;

	for (unsigned int i = 0; i < HEADING_SERVER_MAX_SESSIONS; i++) {
		if (m_slots[i].session == NULL) {
			slot = &m_slots[i];
			break;
		}
	}

	void *mem = g_locationHeap->sce_paf_malloc(pConfig->bufferSize);

	if (mem == NULL) {
		m_sessionCount--;
		sceKernelUnlockLwMutex(&m_mutex, 1);
		return SCE_LOCATION_ERROR_NO_SERVER_MEMORY;
	}
	if (pServer->createSession(&slot->session, this, mem) != 0) {
		m_sessionCount--;
		g_locationHeap->sce_paf_free(mem);
		sceKernelUnlockLwMutex(&m_mutex, 1);
		return SCE_LOCATION_ERROR_FATAL_ERROR;
	}
	slot->appId = slot->session->getAppId();
	sceKernelUnlockLwMutex(&m_mutex, 1);
	return 0;
}

/* (0x81187746) */
void HeadingServer::onSessionClosedByClient(IPMI::Session *pSession)
{
	sceKernelLockLwMutex(&m_mutex, 1, NULL);

	HeadingSessionSlot *slot = NULL;

	for (unsigned int i = 0; i < HEADING_SERVER_MAX_SESSIONS; i++) {
		if (m_slots[i].session == pSession) {
			slot = &m_slots[i];
			break;
		}
	}
	pSession->destroy();
	g_locationHeap->sce_paf_free(pSession);
	slot->appId = -1;
	slot->handleCount = 0;
	slot->session = NULL;
	for (int j = 0; j < 8; j++)
		slot->handles[j] = 0;
	m_sessionCount--;
	sceKernelUnlockLwMutex(&m_mutex, 1);
}

/* (0x811877BE) */
void HeadingServer::onSessionKilled(IPMI::Session *pSession)
{
	sceKernelLockLwMutex(&m_mutex, 1, NULL);

	HeadingSessionSlot *slot = NULL;

	for (unsigned int i = 0; i < HEADING_SERVER_MAX_SESSIONS; i++) {
		if (m_slots[i].session == pSession) {
			slot = &m_slots[i];
			break;
		}
	}
	if (slot != NULL) {
		pSession->destroy();
		g_locationHeap->sce_paf_free(pSession);
		slot->session = NULL;
		for (int j = 0; j < 8; j++)
			slot->handles[j] = 0;
		slot->appId = -1;
		slot->requestId = -1;
		slot->methodId = 0;
		m_sessionCount -= slot->handleCount;
		slot->handleCount = 0;
	}
	sceKernelUnlockLwMutex(&m_mutex, 1);
}

/* (0x81187848) */
int HeadingServer::ThreadEntry(SceSize args, void *argp)
{
	HeadingServer *self = *(HeadingServer **)argp;

	self->m_server->runDispatcher(self->m_workBuffer, self->m_workBufferSize);
	return 0;
}
