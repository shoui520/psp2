/*
 * mtp_marlin_service.cpp: SceMtpMlnSvc IPMI server (0x812BC4F0 - 0x812BCA2F)
 * Provides Marlin DRM metadata read/write over IPMI backed by SceLibMtp.
 */
#include "mtp_server_internal.h"

class MtpMarlinService : public IPMI::Server::EventHandler {
public:
	/* 0x814327DC */
	MtpMarlinService()
		: m_server(NULL),
		  m_sessionCount(0),
		  m_state(0),
		  m_threadId(-1),
		  m_workBufSize(0),
		  m_workBuf(NULL)
	{
		sce_paf_memset(m_sessions, 0, sizeof(m_sessions));
	}

	/* 0x814327F8 */
	virtual ~MtpMarlinService() {}

	int initialize();
	int start();

	/* 0x812BC728 */
	virtual void onSyncMethodDispatch(IPMI::Session *session, SceUInt32 methodId,
					  const IPMI::DataInfo *inList, SceUInt32 inCount,
					  IPMI::BufferInfo *outList, SceUInt32 outCount)
	{
		int ret = dispatchSyncMethod(session, methodId, inList, inCount, outList, outCount);
		if (ret != 0)
			sceClibPrintf("Error %s(%u): dispatchSyncMethod failed: 0x%x\n",
				      "onSyncMethodDispatch", 167, ret);
	}

	/* 0x812BC762 */
	virtual void onAsyncMethodDispatch(IPMI::Session *session, SceUInt32 requestId,
					   SceUInt32 methodId, const IPMI::DataInfo *inList,
					   SceUInt32 inCount)
	{
		(void)session;
		(void)requestId;
		(void)methodId;
		(void)inList;
		(void)inCount;
		sceClibPrintf("Error %s(%u): dispatchAsyncMethod failed: 0x%x\n",
			      "onAsyncMethodDispatch", 176, IPMI_ERROR_UNKNOWN_METHOD);
	}

	/* 0x812BC798 */
	virtual int onNewClient(IPMI::Server *server, const IPMI::Session::Config *config,
				const void *param, SceSize paramSize)
	{
		(void)param;
		(void)paramSize;
		if (server != m_server) {
			sceClibPrintf("Error %s(%u): wrong server\n", "onNewClient", 183);
			return MTP_SVC_ERROR_INVALID_ARGUMENT;
		}
		if (m_sessionCount == MTP_MAX_CLIENTS)
			return MTP_SVC_ERROR_MAX_CLIENTS;

		IPMI::Session **slot = NULL;
		for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
			if (m_sessions[i] == NULL) {
				slot = &m_sessions[i];
				break;
			}
		}
		if (slot == NULL) {
			sceClibPrintf("Error %s(%u): not found context\n", "onNewClient", 199);
			return MTP_SVC_ERROR_MAX_CLIENTS;
		}

		void *sessionMem = sce_paf_malloc(config->bufferSize);
		if (sessionMem == NULL) {
			sceClibPrintf("Error %s(%u): no memory\n", "onNewClient", 205);
			return MTP_SVC_ERROR_NO_MEMORY;
		}

		int ret = server->createSession(slot, slot, sessionMem);
		if (ret != 0)
			sce_paf_free(sessionMem);
		else
			m_sessionCount++;
		return ret;
	}

	/* 0x812BC85E */
	virtual void onSessionClosedByClient(IPMI::Session *session)
	{
		IPMI::Session **slot = NULL;
		for (unsigned int i = 0; i < MTP_MAX_CLIENTS; i++) {
			if (m_sessions[i] == session) {
				slot = &m_sessions[i];
				break;
			}
		}
		if (slot == NULL) {
			sceClibPrintf("Error %s(%u): not found context\n",
				      "onSessionClosedByClient", 230);
			return;
		}

		int ret = session->destroy();
		if (ret != 0)
			sceClibPrintf("Error %s(%u): Destroy session failed: 0x%x\n",
				      "onSessionClosedByClient", 236, ret);
		sce_paf_free(session);
		*slot = NULL;
		m_sessionCount--;
	}

	/* 0x812BC8CC */
	virtual void onFatalError(int error)
	{
		sceClibPrintf("Error %s(%u): 0x%x\n", "onFatalError", 250, error);
	}

private:
	/* 0x812BC8E8 */
	static int serverThreadEntry(SceSize args, void *argp)
	{
		(void)args;
		MtpMarlinService *self = *(MtpMarlinService **)argp;
		int ret = self->m_server->runDispatcher(self->m_workBuf, self->m_workBufSize);
		if (ret != 0)
			sceClibPrintf("Error %s(%u): runDispatcher failed: 0x%x\n",
				      "serverThreadEntry", 264, ret);
		return ret;
	}

	static int dispatchSyncMethod(IPMI::Session *session, SceUInt32 methodId,
				      const IPMI::DataInfo *inList, SceUInt32 inCount,
				      IPMI::BufferInfo *outList, SceUInt32 outCount);

	IPMI::Server *m_server;
	IPMI::Session *m_sessions[MTP_MAX_CLIENTS];
	SceUInt32 m_sessionCount;
	int m_state;
	SceUID m_threadId;
	SceSize m_workBufSize;
	void *m_workBuf;
};

/* 0x812BC53C */
int MtpMarlinService::initialize()
{
	if (m_server != NULL) {
		sceClibPrintf("Error %s(%u): already initialized\n", "initialize", 28);
		return MTP_SVC_ERROR_INVALID_STATE;
	}

	sce_paf_memset(m_sessions, 0, sizeof(m_sessions));
	m_sessionCount = 0;
	m_state = 0;

	IPMI::Server::Config cfg;
	cfg.maxRequestSize = 0xF00u;
	cfg.maxMessageSize = 0x2300u;
	cfg.pEventHandler = this;
	sce_paf_strncpy(cfg.serviceName, "SceMtpMlnSvc", sizeof(cfg.serviceName));
	cfg.budget = 0;
	cfg.openLimit = -1;

	m_workBufSize = IPMI::Server::getWorkBufferSize(&cfg);
	void *serverMem = sce_paf_malloc(0x100u);
	m_workBuf = sce_paf_memalign(0x10u, m_workBufSize);
	if (serverMem == NULL || m_workBuf == NULL) {
		sceClibPrintf("Error %s(%u): no memory\n", "initialize", 47);
		sce_paf_free(serverMem);
		sce_paf_free(m_workBuf);
		return MTP_SVC_ERROR_NO_MEMORY;
	}

	int ret = IPMI::Server::create(&m_server, &cfg, this, serverMem);
	if (ret != 0) {
		sceClibPrintf("Error %s(%u): create server failed: 0x%x\n",
			      "initialize", 55, ret);
		sce_paf_free(serverMem);
		sce_paf_free(m_workBuf);
		return ret;
	}

	SceUID threadId = sceKernelCreateThread("SceMtpMarlinServiceThread",
						serverThreadEntry, 80, 0x4000u,
						0, 0, NULL);
	if (threadId < 0) {
		sceClibPrintf("Error %s(%u): create thread failed: 0x%x\n",
			      "initialize", 70, threadId);
		m_server->destroy();
		sce_paf_free(serverMem);
		sce_paf_free(m_workBuf);
		return threadId;
	}

	m_threadId = threadId;
	m_state = 1;
	return 0;
}

/* 0x812BC6AC */
int MtpMarlinService::start()
{
	if (m_state != 1) {
		sceClibPrintf("Error %s(%u): invalid state: %d\n", "start", 115, m_state);
		return MTP_SVC_ERROR_INVALID_ARGUMENT;
	}

	MtpMarlinService *selfArg = this;
	int ret = sceKernelStartThread(m_threadId, sizeof(selfArg), &selfArg);
	if (ret != 0)
		sceClibPrintf("Error %s(%u): start thread failed: 0x%x\n",
			      "start", 122, ret);
	m_state = 2;
	return ret;
}

/* 0x812BC91E */
int MtpMarlinService::dispatchSyncMethod(IPMI::Session *session, SceUInt32 methodId,
					 const IPMI::DataInfo *inList, SceUInt32 inCount,
					 IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	(void)inCount;
	const SceUInt32 *inWords = (inList != NULL && inList[0].pData != NULL)
					   ? (const SceUInt32 *)inList[0].pData
					   : NULL;
	int res;
	switch (methodId) {
	case MTP_MARLIN_METHOD_INITIALIZE:
		res = mtpWrapperInitialize(NULL);
		break;

	case MTP_MARLIN_METHOD_FINALIZE:
		res = mtpWrapperFinalize();
		break;

	case MTP_MARLIN_METHOD_GET_INFO: {
		SceSize *outSize = (SceSize *)outList[1].pBuffer;
		res = SceLibMtp_C10B3D7C((int)inWords[0], inWords[1], inWords[2],
					 outList[0].pBuffer, outSize);
		outList[0].outSize = *outSize;
		outList[1].outSize = sizeof(SceSize);
		break;
	}

	case MTP_MARLIN_METHOD_SET_INFO:
		res = SceLibMtp_6211F119((int)inWords[0], inWords[1],
					 inList[1].pData, inWords[2]);
		break;

	default:
		return IPMI_ERROR_UNKNOWN_METHOD;
	}
	return session->respondToSyncMethodRequest(res, outList, outCount);
}

static MtpMarlinService s_mtpMarlinService;	/* 0x815C36C0 */

typedef char mtpMarlinServiceSizeCheck[sizeof(MtpMarlinService) == 60 ? 1 : -1];

/* 0x812BC4F0 */
int shellMtpMarlinServiceStart(void)
{
	int ret = s_mtpMarlinService.initialize();
	if (ret >= 0)
		return s_mtpMarlinService.start();
	return ret;
}
