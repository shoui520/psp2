/*
 * location_perm_server.cpp: LocationPermissionServer ("SceLocationPerm",
 * "SceLocationPermissionServer"; 0x8119951C-0x81199B68).
 * The file and class names are ours (PLACEHOLDER).
 */
#define SceThreadmgrCoredumpTime_sceKernelExitThread np_utility_unused_exit_thread
#include "location_internal.h"
#undef SceThreadmgrCoredumpTime_sceKernelExitThread
#include <std/string.h>
#include <std/new>

extern "C" int SceThreadmgrCoredumpTime_sceKernelExitThread(int status);
extern int shellIncomingDialogServerAsyncDispatch(IPMI::Session *session, SceUInt32 requestId,
						  SceUInt32 methodId, const IPMI::DataInfo *inList,
						  SceUInt32 inCount); /* 0x8115B554 */

struct PermSessionSlot {
	IPMI::Session *session;
	SceUInt32 requestId;
	SceUInt32 methodId;
};

class LocationPermissionServer : public IPMI::Server::EventHandler {
public:
	LocationPermissionServer();
	virtual ~LocationPermissionServer();		/* 0x8119951C / 0x81199548 */

	virtual void onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId,
					  const IPMI::DataInfo *pInList, SceUInt32 inCount,
					  IPMI::BufferInfo *pOutList, SceUInt32 outCount); /* 0x81199830 */
	virtual void onAsyncMethodDispatch(IPMI::Session *pSession, SceUInt32 requestId,
					   SceUInt32 methodId, const IPMI::DataInfo *pInList,
					   SceUInt32 inCount); /* 0x8119984E */
	virtual int onNewClient(IPMI::Server *pServer, const IPMI::Session::Config *pConfig,
				const void *pParam, SceSize paramSize); /* 0x811998AC */
	virtual void onSessionClosedByClient(IPMI::Session *pSession); /* 0x81199984 */
	virtual void onSessionKilled(IPMI::Session *pSession); /* 0x811999E8 */

	int Start();					/* 0x81199578 */
	int Stop();					/* 0x81199768 */
	static int ThreadEntry(SceSize args, void *argp); /* 0x81199A58 */
	static int Dispatch(IPMI::Session *session, SceUInt32 methodId,
			    const IPMI::DataInfo *inList, SceUInt32 inCount,
			    IPMI::BufferInfo *outList, SceUInt32 outCount); /* 0x81199A70 */

	IPMI::Server *m_server;
	PermSessionSlot m_slots[2];
	int m_sessionCount;
	SceUID m_threadId;
	LocationLwMutex m_mutex;
	int m_stopped;
	void *m_workBuffer;
	SceSize m_workBufferSize;
};

static LocationPermissionServer *s_permServer;		/* 0x81544D8C */

LocationPermissionServer::LocationPermissionServer()
	: m_server(NULL)
	, m_sessionCount(0)
	, m_threadId(-1)
	, m_stopped(0)
	, m_workBuffer(NULL)
	, m_workBufferSize(0)
{
	m_mutex.result = -1;
	sce_paf_memset(m_slots, 0, sizeof(m_slots));
}

/* 0x8119951C / 0x81199548 */
LocationPermissionServer::~LocationPermissionServer()
{
	if (m_mutex.result == 0) {
		sceKernelDeleteLwMutex(&m_mutex.work);
		m_mutex.result = -1;
	}
}

/* 0x81199578 */
int LocationPermissionServer::Start()
{
	IPMI::Server::Config cfg;
	sce_paf_strncpy(cfg.serviceName, "SceLocationPerm", sizeof(cfg.serviceName));
	cfg.pEventHandler = this;
	cfg.budget = 2;

	m_workBufferSize = IPMI::Server::getWorkBufferSize(&cfg);
	void *serverMem = g_locationHeap->sce_paf_malloc(0x100);
	m_workBuffer = g_locationHeap->sce_paf_memalign(0x10, m_workBufferSize);
	if (serverMem == NULL || m_workBuffer == NULL) {
		g_locationHeap->sce_paf_free(serverMem);
		g_locationHeap->sce_paf_free(m_workBuffer);
		return (int)0x801012C4;
	}

	int ret = IPMI::Server::create(&m_server, &cfg, this, serverMem);
	if (ret != 0) {
		g_locationHeap->sce_paf_free(serverMem);
		g_locationHeap->sce_paf_free(m_workBuffer);
		return ret;
	}

	m_stopped = 0;
	m_threadId = sceKernelCreateThread("SceLocationPermissionServer", ThreadEntry,
					   0x1000010A, 0x2000, 0, 0, NULL);
	if (m_threadId < 0) {
		m_server->destroy();
		g_locationHeap->sce_paf_free(serverMem);
		g_locationHeap->sce_paf_free(m_workBuffer);
		return m_threadId;
	}

	LocationPermissionServer *arg = this;
	ret = sceKernelStartThread(m_threadId, sizeof(arg), &arg);
	if (ret < 0) {
		sceKernelDeleteThread(m_threadId);
		m_server->destroy();
		g_locationHeap->sce_paf_free(serverMem);
		g_locationHeap->sce_paf_free(m_workBuffer);
		return ret;
	}
	return 0;
}

/* 0x811996E0 */
int locationPermServerStart(void)
{
	if (s_permServer != NULL)
		return (int)0x801012CF;

	LocationPermissionServer *srv = new (g_locationHeap) LocationPermissionServer();
	s_permServer = srv;
	if (srv == NULL)
		return (int)0x801012C4;

	srv->m_mutex.result =
		sceKernelCreateLwMutex(&srv->m_mutex.work, "SceLocationPerm", 2, 0, NULL);
	int ret = srv->Start();
	if (ret != 0) {
		delete s_permServer;
		s_permServer = NULL;
	}
	return ret;
}

/* 0x81199768 */
int LocationPermissionServer::Stop()
{
	if (m_server == NULL)
		return 0;
	m_stopped = 1;
	m_server->shutdownDispatcher();
	sceKernelWaitThreadEnd(m_threadId, NULL, NULL);
	sceKernelDeleteThread(m_threadId);
	m_server->destroy();
	g_locationHeap->sce_paf_free(m_server);
	g_locationHeap->sce_paf_free(m_workBuffer);
	m_server = NULL;
	m_workBuffer = NULL;
	return 0;
}

/* 0x811997F2 */
int locationPermServerStop(void)
{
	int ret = 0;
	if (s_permServer != NULL) {
		ret = s_permServer->Stop();
		if (ret == 0) {
			delete s_permServer;
			s_permServer = NULL;
		}
	}
	return ret;
}

/* 0x8119982C (SNC ICF-folded LocationLogger::GetLogMask) */
SceUInt32 locationLoggerGetMask(const LocationLogger *logger)
{
	return logger->GetLogMask();
}

/* 0x81199830 */
void LocationPermissionServer::onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId,
						    const IPMI::DataInfo *pInList, SceUInt32 inCount,
						    IPMI::BufferInfo *pOutList, SceUInt32 outCount)
{
	Dispatch(pSession, methodId, pInList, inCount, pOutList, outCount);
}

/* 0x8119984E */
void LocationPermissionServer::onAsyncMethodDispatch(IPMI::Session *pSession, SceUInt32 requestId,
						     SceUInt32 methodId,
						     const IPMI::DataInfo *pInList,
						     SceUInt32 inCount)
{
	(void)pInList;
	(void)inCount;
	sceKernelLockLwMutex(&m_mutex.work, 1, NULL);
	for (unsigned int i = 0; i < 2; i++) {
		if (m_slots[i].session == pSession) {
			m_slots[i].requestId = requestId;
			m_slots[i].methodId = methodId;
			break;
		}
	}
	sceKernelUnlockLwMutex(&m_mutex.work, 1);
	shellIncomingDialogServerAsyncDispatch(pSession, requestId, methodId, pInList, inCount);
}

/* 0x811998AC */
int LocationPermissionServer::onNewClient(IPMI::Server *pServer,
					  const IPMI::Session::Config *pConfig,
					  const void *pParam, SceSize paramSize)
{
	(void)pParam;
	(void)paramSize;
	sceKernelLockLwMutex(&m_mutex.work, 1, NULL);
	if (m_sessionCount == 2) {
		sceKernelUnlockLwMutex(&m_mutex.work, 1);
		return (int)0x801012C5;
	}

	m_sessionCount++;
	IPMI::Session **slot = NULL;
	for (unsigned int i = 0; i < 2; i++) {
		if (m_slots[i].session == NULL) {
			slot = &m_slots[i].session;
			break;
		}
	}

	void *mem = g_locationHeap->sce_paf_malloc(pConfig->bufferSize);
	if (mem == NULL) {
		m_sessionCount--;
		sceKernelUnlockLwMutex(&m_mutex.work, 1);
		return (int)0x801012C4;
	}

	if (pServer->createSession(slot, this, mem) != 0) {
		m_sessionCount--;
		g_locationHeap->sce_paf_free(mem);
		sceKernelUnlockLwMutex(&m_mutex.work, 1);
		return (int)0x801012CF;
	}

	sceKernelUnlockLwMutex(&m_mutex.work, 1);
	return 0;
}

/* 0x81199984 */
void LocationPermissionServer::onSessionClosedByClient(IPMI::Session *pSession)
{
	sceKernelLockLwMutex(&m_mutex.work, 1, NULL);
	PermSessionSlot *slot = NULL;
	for (unsigned int i = 0; i < 2; i++) {
		if (m_slots[i].session == pSession) {
			slot = &m_slots[i];
			break;
		}
	}
	pSession->destroy();
	g_locationHeap->sce_paf_free(pSession);
	slot->methodId = 0;
	slot->session = NULL;
	slot->requestId = (SceUInt32)-1;
	m_sessionCount--;
	sceKernelUnlockLwMutex(&m_mutex.work, 1);
}

/* 0x811999E8 */
void LocationPermissionServer::onSessionKilled(IPMI::Session *pSession)
{
	sceKernelLockLwMutex(&m_mutex.work, 1, NULL);
	for (unsigned int i = 0; i < 2; i++) {
		if (m_slots[i].session == pSession) {
			pSession->destroy();
			g_locationHeap->sce_paf_free(pSession);
			m_slots[i].methodId = 0;
			m_slots[i].session = NULL;
			m_slots[i].requestId = (SceUInt32)-1;
			m_sessionCount--;
			break;
		}
	}
	sceKernelUnlockLwMutex(&m_mutex.work, 1);
}

/* 0x81199A58 */
int LocationPermissionServer::ThreadEntry(SceSize args, void *argp)
{
	(void)args;
	LocationPermissionServer *self = *(LocationPermissionServer **)argp;
	int ret = self->m_server->runDispatcher(self->m_workBuffer, self->m_workBufferSize);
	SceThreadmgrCoredumpTime_sceKernelExitThread(ret);
	return 0;
}

/* 0x81199A70 */
int LocationPermissionServer::Dispatch(IPMI::Session *session, SceUInt32 methodId,
				       const IPMI::DataInfo *inList, SceUInt32 inCount,
				       IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	(void)inCount;
	switch (methodId) {
	case 0x12340000: {
		int ret = locationPermSvcSetAppPerm(0, (const char *)inList[1].pData,
						    *(const int *)inList[0].pData);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case 0x12340001: {
		int ret = locationPermSvcGetMainPerm(0, (int *)outList[0].pBuffer);
		outList[0].outSize = 4;
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case 0x12340002: {
		int ret = locationPermSvcSetMainPerm(0, *(const int *)inList[0].pData);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case 0x12340003: {
		int ret = locationPermSvcGetParentalPerm(0, (int *)outList[0].pBuffer);
		outList[0].outSize = 4;
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case 0x12340004: {
		int ret = locationPermSvcSetParentalPerm(0, *(const int *)inList[0].pData);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case 0x12340005: {
		int ret = locationPermSvcGetGlobalPerm(0, (int *)outList[0].pBuffer);
		outList[0].outSize = 4;
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case 0x12340006: {
		int ret = locationPermSvcGetAppPermSetting(0, (int *)outList[0].pBuffer);
		outList[0].outSize = 4;
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	default:
		return (int)0x80020641;
	}
}
