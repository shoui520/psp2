/*
 * mtp_server.cpp: MtpServer lifecycle, IPMI event handling, console-info transfer,
 * process activation/deactivation jobs, and public shell MTP API wrappers
 * ("SERVER", 0x812B1B64 - 0x812B1D4F, 0x812B69DC - 0x812B81CF, 0x81431E48 - 0x81432737).
 */
#include <appmgr/appmgr.h>
#include "mtp_server_internal.h"

/* Global MtpServer singleton (0x815C3118) */
MtpServer g_mtpServer;

/* Compile-time layout verification against 3.65-CEX shell.elf */
typedef char mtpStatusSizeCheck[sizeof(MtpStatus) == 104 ? 1 : -1];
typedef char mtpNetHostSizeCheck[sizeof(MtpNetHost) == 200 ? 1 : -1];
typedef char mtpBroadcastSizeCheck[sizeof(MtpBroadcastMessage) == 112 ? 1 : -1];
typedef char mtpCapabilitySizeCheck[sizeof(MtpCapabilityInfo) == 32 ? 1 : -1];
typedef char mtpSessionSlotSizeCheck[sizeof(MtpSessionSlot) == 148 ? 1 : -1];
typedef char mtpServiceWorkerSizeCheck[sizeof(MtpServiceWorker) == 96 ? 1 : -1];
typedef char mtpSessionMgrSizeCheck[sizeof(MtpServiceSessionMgr) == 1216 ? 1 : -1];
typedef char mtpActivationMgrSizeCheck[sizeof(MtpServiceActivationMgr) == 40 ? 1 : -1];
typedef char mtpServerSizeCheck[sizeof(MtpServer) == 1448 ? 1 : -1];

static bool isConnectedHostPs3()
{
	if (SceLibMtp_4C1B7DBB() == MTP_IF_TYPE_TCPIP) {
		MtpNetHost netHost;
		sce_paf_memset(&netHost, 0, sizeof(netHost));
		SceLibMtp_D63692E8(&netHost);
		return netHost.hostType == MTP_NET_HOST_TYPE_PS3;
	}
	MtpStatus hostStatus;
	sce_paf_memset(&hostStatus, 0, sizeof(hostStatus));
	sceMtpGetHostInfo(&hostStatus);
	return hostStatus.platformType == MTP_PLATFORM_TYPE_PS;
}

static bool isHostMultiAppCapable()
{
	MtpCapabilityInfo cap;
	cap.supported = 0;
	cap.capabilityStatus = 0;
	cap.timestamp = paf::datetime::DateTime(0, 0, 0, 0, 0, 0, 0);
	cap.timestamp.m_parse_failed = true;
	SceLibMtp_94040962(10, &cap);
	return cap.supported != 0;
}

/* ---------------------------------------------------------------------------
 * Process Lifecycle & LiveArea Jobs (0x81432414 - 0x81432737)
 * --------------------------------------------------------------------------- */

class MtpPsbtnLiveAreaOrientJob : public paf::job::JobItem {
public:
	MtpPsbtnLiveAreaOrientJob(MtpServer *server, MtpServiceSessionMgr *sessionMgr,
				  const char *titleId)
		: paf::job::JobItem("SceMtpPsbtnLiveAreaOrientJobJob", NULL),
		  m_server(server),
		  m_sessionMgr(sessionMgr)
	{
		sce_paf_memset(m_titleId, 0, sizeof(m_titleId));
		sce_paf_strncpy(m_titleId, titleId, 0x20u);
	}

	virtual ~MtpPsbtnLiveAreaOrientJob() {}

	/* 0x81432450 */
	virtual int Run()
	{
		if (m_server->processActivityState() != MTP_PROC_ACTIVITY_ACTIVATING) {
			SceAppMgrAppStatus appStatus;
			sceAppMgrGetStatusByName(m_titleId, &appStatus);
			m_sessionMgr->cancelByProcessId(appStatus.processId);
			m_sessionMgr->terminateByProcessId(appStatus.processId);
		}
		return 0;
	}

	virtual void Finish(int result) { (void)result; }

private:
	MtpServer *m_server;
	MtpServiceSessionMgr *m_sessionMgr;
	char m_titleId[33];
};

class MtpTerminationJob : public paf::job::JobItem {
public:
	MtpTerminationJob() : paf::job::JobItem("SceMtpTerminationJob", NULL) {}
	virtual ~MtpTerminationJob() {}

	/* 0x814324E0 */
	virtual int Run()
	{
		sceMtpTerminateExecution();
		return 0;
	}

	virtual void Finish(int result) { (void)result; }
};

class MtpProcessDeactivateJob : public paf::job::JobItem {
public:
	MtpProcessDeactivateJob(MtpServer *server, MtpServiceSessionMgr *sessionMgr, SceUID pid)
		: paf::job::JobItem("SceMtpProcessDeactivateJob", NULL),
		  m_server(server),
		  m_sessionMgr(sessionMgr),
		  m_pid(pid)
	{
	}

	virtual ~MtpProcessDeactivateJob() {}

	/* 0x8143252C */
	virtual int Run()
	{
		if (m_server->processActivityState() == MTP_PROC_ACTIVITY_ACTIVATING)
			return 0;

		m_sessionMgr->onProcessDeactivate(m_pid);
		int ifType = SceLibMtp_4C1B7DBB();
		bool multiAppCapable = isHostMultiAppCapable();
		if (ifType != MTP_IF_TYPE_TCPIP || !multiAppCapable)
			m_sessionMgr->cancelByProcessId(m_pid);

		if (m_server->processActivityState() == MTP_PROC_ACTIVITY_ACTIVATING)
			return 0;

		if (ifType == MTP_IF_TYPE_TCPIP && !isConnectedHostPs3() && !multiAppCapable)
			m_sessionMgr->terminateByProcessId(m_pid);

		SceLibMtp_4E2CF833(0);
		m_server->setProcessActivityState(MTP_PROC_ACTIVITY_DEACTIVATED);
		return 0;
	}

	virtual void Finish(int result) { (void)result; }

private:
	MtpServer *m_server;
	MtpServiceSessionMgr *m_sessionMgr;
	SceUID m_pid;
};

class MtpProcessActivateJob : public paf::job::JobItem {
public:
	MtpProcessActivateJob(MtpServer *server, MtpServiceSessionMgr *sessionMgr,
			      SceUID pid, const char *titleId)
		: paf::job::JobItem("SceMtpProcessActivateJob", NULL),
		  m_server(server),
		  m_sessionMgr(sessionMgr),
		  m_pid(pid)
	{
		sce_paf_memset(m_titleId, 0, sizeof(m_titleId));
		sce_paf_strncpy(m_titleId, titleId, 0x20u);
	}

	virtual ~MtpProcessActivateJob() {}

	/* 0x81432670 */
	virtual int Run()
	{
		if (m_server->processActivityState() == MTP_PROC_ACTIVITY_DEACTIVATING)
			return 0;

		MtpStatus hostStatus;
		if (sceMtpGetHostInfo(&hostStatus) == 0)
			m_sessionMgr->broadcastMtpStatusEventDuringSuspend(m_pid, &hostStatus);

		m_sessionMgr->broadcastNetMtpRegisterEventDuringSuspend(m_pid);
		if (sce_paf_strcmp("NPXS10026", m_titleId) == 0 &&
		    SceLibMtp_4C1B7DBB() == MTP_IF_TYPE_TCPIP)
			SceLibMtp_965F1CD0();

		m_sessionMgr->onProcessActivate(m_pid);
		SceLibMtp_4E2CF833(0);
		m_server->setProcessActivityState(MTP_PROC_ACTIVITY_ACTIVATED);
		return 0;
	}

	virtual void Finish(int result) { (void)result; }

private:
	MtpServer *m_server;
	MtpServiceSessionMgr *m_sessionMgr;
	SceUID m_pid;
	char m_titleId[33];
};

/* ---------------------------------------------------------------------------
 * MtpServer Implementation (0x812B69DC - 0x812B81CF)
 * --------------------------------------------------------------------------- */

/* 0x81431E48 */
MtpServer::MtpServer()
	: m_server(NULL),
	  m_cancelServer(NULL),
	  m_serviceJobQueue(NULL),
	  m_cancelJobQueue(NULL),
	  m_state(0),
	  m_serverThreadId(-1),
	  m_cancelThreadId(-1),
	  m_serverWorkBufSize(0),
	  m_serverWorkBuf(NULL),
	  m_cancelWorkBufSize(0),
	  m_cancelWorkBuf(NULL),
	  m_eventListener(NULL),
	  m_serviceEventFlag(-1),
	  m_cmaActive(1),
	  m_consoleInfoCallback(NULL),
	  m_consoleInfoArg(NULL),
	  m_pendingConsoleInfoReq(NULL),
	  m_consoleInfoAborted(0),
	  m_consoleInfoEventFlag(-1),
	  m_consoleInfoHttpStatus(0),
	  m_consoleInfoReservedFlag(0),
	  m_processActivityState(MTP_PROC_ACTIVITY_NONE)
{
}

/* 0x81431EC8 */
MtpServer::~MtpServer()
{
}

/* 0x812B818E */
void MtpServer::hostEventTrampoline(int eventCode, void *userArg)
{
	if (userArg != NULL)
		((MtpServer *)userArg)->onHostEvent(eventCode);
}

/* 0x812B819E */
void MtpServer::bgCopyEventTrampoline(int eventCode, void *userArg)
{
	if (userArg != NULL) {
		SceUInt32 code = (SceUInt32)eventCode;
		((MtpServer *)userArg)->sessionMgr().broadcastMtpBgCopyEvent(&code);
	}
}

/* 0x812B69DC */
void MtpServer::onHostEvent(int eventCode)
{
	MtpStatusEventMsg eventMsg;
	sce_paf_memset(&eventMsg, 0, sizeof(eventMsg));
	sceMtpGetHostInfo(&eventMsg.hostStatus);
	eventMsg.eventCode = eventCode;

	switch (eventCode) {
	case MTP_HOST_EVENT_AVAILABLE:
		notifyMtpAvailable();
		enqueueStorageActivationJob(true);
		if (eventMsg.hostStatus.state == 4)
			notifyHostReady();
		break;

	case MTP_HOST_EVENT_USB_DETECTED:
		notifyUsbDetection();
		m_sessionMgr.broadcastMtpStatusEvent(&eventMsg);
		enqueueWatchConnectionJob();
		break;

	case MTP_HOST_EVENT_DISCONNECTED:
		if (!isUsbCdromMode()) {
			notifyMtpDisconnection();
			if (SceLibMtp_4C1B7DBB() == MTP_IF_TYPE_USB)
				m_sessionMgr.markHostResetAll();
			enqueueStorageActivationJob(false);
			m_sessionMgr.broadcastMtpStatusEvent(&eventMsg);
		}
		break;

	case MTP_HOST_EVENT_CONNECTED:
		notifyMtpConnection();
		m_sessionMgr.broadcastMtpStatusEvent(&eventMsg);
		break;

	case MTP_HOST_EVENT_BUSY:
		notifyMtpBusy();
		m_sessionMgr.broadcastMtpStatusEvent(&eventMsg);
		break;

	case MTP_HOST_EVENT_READY:
		notifyMtpAvailable();
		m_sessionMgr.broadcastMtpStatusEvent(&eventMsg);
		break;

	case MTP_HOST_EVENT_STORAGE_RESET:
		enqueueStorageResetJob();
		break;

	case MTP_HOST_EVENT_HOST_RESET:
		m_sessionMgr.markHostResetAll();
		m_sessionMgr.broadcastMtpStatusEvent(&eventMsg);
		break;

	default:
		break;
	}
}

/* 0x812B6B3C */
int MtpServer::finalizeServer()
{
	if (m_state != 1) {
		sceClibPrintf("[MTP %s] Error %s(%u): invalid state: %d\n",
			      "SERVER", "finalizeServer", 369, m_state);
		return MTP_SVC_ERROR_INVALID_STATE;
	}

	int ret = sceKernelDeleteThread(m_serverThreadId);
	if (ret != 0)
		sceClibPrintf("[MTP %s] Error %s(%u): delete thread failed: 0x%x\n",
			      "SERVER", "finalizeServer", 375, ret);

	ret = sceKernelDeleteThread(m_cancelThreadId);
	if (ret != 0)
		sceClibPrintf("[MTP %s] Error %s(%u): delete thread failed: 0x%x\n",
			      "SERVER", "finalizeServer", 380, ret);

	ret = m_server->destroy();
	if (ret != 0)
		sceClibPrintf("[MTP %s] Error %s(%u): destroy server failed: 0x%x\n",
			      "SERVER", "finalizeServer", 385, ret);

	int cancelRet = m_cancelServer->destroy();
	if (cancelRet != 0)
		sceClibPrintf("[MTP %s] Error %s(%u): destroy server failed: 0x%x\n",
			      "SERVER", "finalizeServer", 390, cancelRet);

	sce_paf_free(m_server);
	m_server = NULL;
	sce_paf_free(m_cancelServer);
	m_cancelServer = NULL;
	m_state = 0;
	sce_paf_free(m_serverWorkBuf);
	sce_paf_free(m_cancelWorkBuf);
	return cancelRet;
}

/* 0x812B6C60 */
int MtpServer::initializeServer(int priority, IPMI::Server **outServer,
				const IPMI::Server::Config *config,
				void **outWorkBuf, SceSize *outWorkBufSize,
				SceUID *outThreadId, SceKernelThreadEntry threadEntry)
{
	if (outServer == NULL || outWorkBuf == NULL ||
	    outWorkBufSize == NULL || outThreadId == NULL)
		return MTP_SVC_ERROR_INVALID_ARGUMENT;

	if (*outServer != NULL) {
		sceClibPrintf("[MTP %s] Error %s(%u): already initialized\n",
			      "SERVER", "initializeServer", 309);
		return MTP_SVC_ERROR_INVALID_STATE;
	}

	*outWorkBufSize = IPMI::Server::getWorkBufferSize(config);
	void *serverMem = sce_paf_malloc(0x100u);
	*outWorkBuf = sce_paf_memalign(0x10u, *outWorkBufSize);
	if (serverMem == NULL) {
		sceClibPrintf("[MTP %s] Error %s(%u): no memory\n",
			      "SERVER", "initializeServer", 320);
		sce_paf_free(NULL);
		sce_paf_free(*outWorkBuf);
		return MTP_SVC_ERROR_NO_MEMORY;
	}

	IPMI::Server *createdServer = NULL;
	int ret = IPMI::Server::create(&createdServer, config, this, serverMem);
	if (ret != 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): create server failed: 0x%x\n",
			      "SERVER", "initializeServer", 328, ret);
		sce_paf_free(serverMem);
		sce_paf_free(*outWorkBuf);
		return ret;
	}

	*outServer = createdServer;
	SceUID threadId = sceKernelCreateThread("SceMtpService", threadEntry, priority,
						0x4000u, 0, 0, NULL);
	if (threadId < 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): create thread failed: 0x%x\n",
			      "SERVER", "initializeServer", 344, threadId);
		createdServer->destroy();
		sce_paf_free(serverMem);
		sce_paf_free(*outWorkBuf);
		return threadId;
	}

	*outThreadId = threadId;
	return 0;
}

/* 0x812B6E00 */
int MtpServer::initializeMtp(const ShellMtpServerParam *param)
{
	SceUID evf = sceKernelCreateEventFlag("SceMtpService", 0, 0, NULL);
	if (evf < 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): CreateEventFlag failed: 0x%x\n",
			      "SERVER", "initializeMtp", 892, evf);
		return evf;
	}
	m_serviceEventFlag = evf;

	SceUID consoleEvf = sceKernelCreateEventFlag("SceMtpConsoleInfo", 0,
						     MTP_CONSOLE_EVF_IDLE, NULL);
	if (consoleEvf < 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): CreateEventFlag failed: 0x%x\n",
			      "SERVER", "initializeMtp", 898, consoleEvf);
		return consoleEvf;
	}
	m_consoleInfoEventFlag = consoleEvf;

	if (m_activationMgr.initialize() < 0) {
		sceKernelDeleteEventFlag(m_serviceEventFlag);
		sceKernelDeleteEventFlag(m_consoleInfoEventFlag);
		m_serviceEventFlag = -1;
		m_consoleInfoEventFlag = -1;
		return MTP_SVC_ERROR_INTERNAL;
	}

	int ret;
	if (param != NULL) {
		SceMtpInitParam initParam;
		sce_paf_memset(&initParam, 0, sizeof(initParam));
		initParam.devThreadPriority = param->devThreadPriority;
		initParam.devThreadCpuAffinityMask = param->devThreadCpuAffinityMask;
		initParam.xferThreadPriority = param->xferThreadPriority;
		initParam.xferThreadCpuAffinityMask = param->xferThreadCpuAffinityMask;
		initParam.largeBufferFuncs = param->largeBufferFuncs;
		ret = mtpWrapperInitialize(&initParam);
	} else {
		ret = mtpWrapperInitialize(NULL);
	}

	if (ret != 0) {
		m_activationMgr.finalize();
		sceKernelDeleteEventFlag(m_serviceEventFlag);
		sceKernelDeleteEventFlag(m_consoleInfoEventFlag);
		m_serviceEventFlag = -1;
		m_consoleInfoEventFlag = -1;
		return ret;
	}

	SceLibMtp_5120B22A(MTP_STATUS_EVENT_MASK, hostEventTrampoline, this);
	SceLibMtp_934240B4(bgCopyEventTrampoline, this);
	return 0;
}

/* 0x812B6F62 */
int MtpServer::initializeJobQueue(int priority)
{
	paf::job::JobQueue::Option opt;
	opt.priority = priority;
	opt.stackSize = 0x4000u;
	opt.numThreads = 1;

	m_cancelJobQueue = new paf::job::JobQueue("SceMtpServiceCancelJob", &opt);
	if (m_cancelJobQueue == NULL) {
		sceClibPrintf("[MTP %s] Error %s(%u): create JobQueue failed\n",
			      "SERVER", "initializeJobQueue", 850);
		return MTP_SVC_ERROR_NO_MEMORY;
	}

	m_serviceJobQueue = new paf::job::JobQueue("SceMtpServiceJob", &opt);
	if (m_serviceJobQueue == NULL) {
		sceClibPrintf("[MTP %s] Error %s(%u): create JobQueue failed\n",
			      "SERVER", "initializeJobQueue", 856);
		return MTP_SVC_ERROR_NO_MEMORY;
	}

	return m_worker.initialize();
}

/* 0x812B7036 */
int MtpServer::initialize(const ShellMtpServerParam *param)
{
	int priority = (param != NULL) ? param->serverThreadPriority : 80;
	int ret = initializeJobQueue(priority);
	if (ret == 0) {
		ret = initializeMtp(param);
		if (ret == 0) {
			IPMI::Server::Config cfg;
			cfg.maxRequestSize = 0xF00u;
			cfg.maxMessageSize = 0x10F00u;
			cfg.pEventHandler = this;
			sce_paf_strncpy(cfg.serviceName, "SceMtpService", sizeof(cfg.serviceName));
			cfg.budget = 2;
			cfg.openLimit = -1;
			m_state = 0;

			ret = initializeServer(priority, &m_server, &cfg,
					       &m_serverWorkBuf, &m_serverWorkBufSize,
					       &m_serverThreadId, serverThreadEntry);
			if (ret == 0) {
				IPMI::Server::Config cancelCfg;
				cancelCfg.maxRequestSize = 0xF00u;
				cancelCfg.maxMessageSize = 0x10F00u;
				cancelCfg.pEventHandler = this;
				sce_paf_strncpy(cancelCfg.serviceName, "SceMtpServiceCa",
						sizeof(cancelCfg.serviceName));
				cancelCfg.budget = 2;
				cancelCfg.openLimit = -1;

				ret = initializeServer(priority, &m_cancelServer, &cancelCfg,
						       &m_cancelWorkBuf, &m_cancelWorkBufSize,
						       &m_cancelThreadId, serverCancelThreadEntry);
				if (ret == 0) {
					m_state = 1;
					ret = m_sessionMgr.initialize();
				}
			}
		}
	}

	if (ret < 0) {
		if (m_cancelJobQueue != NULL) {
			delete m_cancelJobQueue;
			m_cancelJobQueue = NULL;
		}
		m_worker.finalize();
		if (m_serviceJobQueue != NULL) {
			delete m_serviceJobQueue;
			m_serviceJobQueue = NULL;
		}
		finalizeServer();
		SceLibMtp_39CEBDD1(MTP_STATUS_EVENT_MASK, hostEventTrampoline);
		SceLibMtp_E7CB0238();
		mtpWrapperFinalize();
		m_activationMgr.finalize();
		sceKernelDeleteEventFlag(m_serviceEventFlag);
		sceKernelDeleteEventFlag(m_consoleInfoEventFlag);
		m_serviceEventFlag = -1;
		m_consoleInfoEventFlag = -1;
	}
	return ret;
}

/* 0x812B723C */
int MtpServer::finalize()
{
	m_sessionMgr.finalize();
	if (m_cancelJobQueue != NULL) {
		delete m_cancelJobQueue;
		m_cancelJobQueue = NULL;
	}
	m_worker.finalize();
	if (m_serviceJobQueue != NULL) {
		delete m_serviceJobQueue;
		m_serviceJobQueue = NULL;
	}
	finalizeServer();
	SceLibMtp_39CEBDD1(MTP_STATUS_EVENT_MASK, hostEventTrampoline);
	SceLibMtp_E7CB0238();
	int ret = mtpWrapperFinalize();
	m_activationMgr.finalize();
	sceKernelDeleteEventFlag(m_serviceEventFlag);
	sceKernelDeleteEventFlag(m_consoleInfoEventFlag);
	m_serviceEventFlag = -1;
	m_consoleInfoEventFlag = -1;
	return ret;
}

/* 0x812B72B8 */
int MtpServer::start()
{
	if (m_state != 1) {
		sceClibPrintf("[MTP %s] Error %s(%u): invalid state: %d\n",
			      "SERVER", "Start", 410, m_state);
		return MTP_SVC_ERROR_INVALID_ARGUMENT;
	}

	MtpServer *selfArg = this;
	int ret = sceKernelStartThread(m_serverThreadId, sizeof(selfArg), &selfArg);
	if (ret != 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): start thread failed: 0x%x\n",
			      "SERVER", "Start", 417, ret);
		return ret;
	}

	ret = sceKernelStartThread(m_cancelThreadId, sizeof(selfArg), &selfArg);
	if (ret != 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): start thread failed: 0x%x\n",
			      "SERVER", "Start", 423, ret);
		return ret;
	}

	m_state = 2;
	return 0;
}

/* 0x812B738A */
void MtpServer::notifyProcessDeactivated(SceUID pid)
{
	if (!m_sessionMgr.hasSessionForProcess(pid))
		return;

	SceLibMtp_4E2CF833(0);
	m_processActivityState = MTP_PROC_ACTIVITY_DEACTIVATING;
	SceLibMtp_4C1B7DBB();
	(void)isHostMultiAppCapable();

	paf::common::SharedPtr<paf::job::JobItem> job(
		new MtpProcessDeactivateJob(this, &m_sessionMgr, pid));
	shellGetMtpJobQueue()->Enqueue(job);
}

/* 0x812B74CC */
void MtpServer::notifyProcessActivated(SceUID pid, int appId, const char *titleId)
{
	(void)appId;
	if (!m_sessionMgr.hasSessionForProcess(pid) &&
	    sce_paf_strcmp("NPXS10026", titleId) != 0 &&
	    sce_paf_strcmp("NPXS10004", titleId) != 0 &&
	    sce_paf_strcmp("NPXS10009", titleId) != 0 &&
	    sce_paf_strcmp("NPXS10010", titleId) != 0)
		return;

	if (m_processActivityState == MTP_PROC_ACTIVITY_DEACTIVATING)
		SceLibMtp_4E2CF833(1);
	else
		SceLibMtp_4E2CF833(0);

	m_processActivityState = MTP_PROC_ACTIVITY_ACTIVATING;
	paf::common::SharedPtr<paf::job::JobItem> job(
		new MtpProcessActivateJob(this, &m_sessionMgr, pid, titleId));
	shellGetMtpJobQueue()->Enqueue(job);
}

/* 0x812B764A */
void MtpServer::notifyProcessEnded(SceUID pid, int appId, const char *titleId)
{
	(void)pid;
	(void)appId;
	if (titleId != NULL && sce_paf_strcmp("NPXS10026", titleId) == 0) {
		setCmaActive(1);
		SceLibMtp_55B66FA0();
	}
}

/* 0x812B7670 */
int MtpServer::enqueueServiceJob(paf::common::SharedPtr<paf::job::JobItem> &job)
{
	return m_serviceJobQueue->Enqueue(job);
}

/* 0x812B767A */
void MtpServer::notifyLiveAreaOrient(const char *titleId)
{
	SceLibMtp_BB32CD0A();
	bool multiAppCapable = isHostMultiAppCapable();
	if (SceLibMtp_4C1B7DBB() == MTP_IF_TYPE_TCPIP &&
	    isConnectedHostPs3() && !multiAppCapable) {
		paf::common::SharedPtr<paf::job::JobItem> job(
			new MtpPsbtnLiveAreaOrientJob(this, &m_sessionMgr, titleId));
		m_serviceJobQueue->Enqueue(job);
	}
}

/* 0x812B7810 */
void MtpServer::notifyWifiTerminate()
{
	SceLibMtp_BB32CD0A();
	if (SceLibMtp_4C1B7DBB() == MTP_IF_TYPE_TCPIP && isConnectedHostPs3()) {
		paf::common::SharedPtr<paf::job::JobItem> job(new MtpTerminationJob());
		m_serviceJobQueue->Enqueue(job);
	}
}

/* 0x812B7944 */
int MtpServer::enqueueCancelJob(paf::common::SharedPtr<paf::job::JobItem> &job)
{
	return m_cancelJobQueue->Enqueue(job);
}

/* 0x812B79E2 */
void MtpServer::setEventListener(MtpEventListener *listener)
{
	m_eventListener = listener;
}

/* 0x812B79F2 */
int MtpServer::setConsoleInfoCallback(MtpConsoleInfoStartCallback callback, void *userArg)
{
	if (callback == NULL)
		return MTP_SVC_ERROR_INVALID_ARGUMENT;
	m_consoleInfoCallback = callback;
	m_consoleInfoArg = userArg;
	return 0;
}

/* 0x812B7A0C */
int MtpServer::startConsoleInfo(int *outStarted)
{
	m_consoleInfoAborted = 0;
	m_pendingConsoleInfoReq = NULL;
	if (m_consoleInfoCallback == NULL)
		return MTP_SVC_ERROR_INVALID_ARGUMENT;

	int started = m_consoleInfoCallback(m_consoleInfoArg);
	*outStarted = started;
	int ret = 0;
	if (started != 0) {
		ret = sceKernelClearEventFlag(m_consoleInfoEventFlag, 0);
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): sceKernelClearEventFlag failed: 0x%x\n",
				"SERVER", "StartConsoleInfo", 972, ret);
	}
	return ret;
}

/* 0x812B7A76 */
int MtpServer::beginSendConsoleInfoData(char *outUrl, SceSize outUrlSize, int *outFinished)
{
	if (m_pendingConsoleInfoReq == NULL)
		return MTP_SVC_ERROR_INVALID_STATE;
	if (outUrl == NULL)
		return MTP_SVC_ERROR_INVALID_ARGUMENT;

	int ret = 0;
	*outFinished = 0;
	if (m_consoleInfoAborted != 0) {
		SceUInt32 numWaitThreads = 0;
		ret = sceKernelCancelEventFlag(m_consoleInfoEventFlag, 0, &numWaitThreads);
		if (ret >= 0)
			ret = MTP_SVC_ERROR_CONSOLE_INFO_CANCELED;
		else
			sceClibPrintf(
				"[MTP %s] Error %s(%u): sceKernelCancelEventFlag failed: 0x%x\n",
				"SERVER", "BeginSendConsoleInfoData", 995, ret);
	} else if (m_pendingConsoleInfoReq->uri != NULL ||
		   m_pendingConsoleInfoReq->uriLength != 0) {
		SceSize hostLen = sce_paf_strlen(m_pendingConsoleInfoReq->host);
		SceSize schemeLen = sce_paf_strlen(m_pendingConsoleInfoReq->scheme);
		SceSize sepLen = sce_paf_strlen("://");
		if (schemeLen + sepLen + hostLen + m_pendingConsoleInfoReq->uriLength > outUrlSize)
			return MTP_SVC_ERROR_INVALID_ARGUMENT;

		sce_paf_strncpy(outUrl, m_pendingConsoleInfoReq->scheme, outUrlSize);
		sce_paf_strncat(outUrl, "://", outUrlSize - sce_paf_strlen(outUrl));
		sce_paf_strncat(outUrl, m_pendingConsoleInfoReq->host,
				outUrlSize - sce_paf_strlen(outUrl));
		sce_paf_strncat(outUrl, m_pendingConsoleInfoReq->uri,
				outUrlSize - sce_paf_strlen(outUrl));

		ret = sceKernelSetEventFlag(m_consoleInfoEventFlag, MTP_CONSOLE_EVF_REQUEST_READY);
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): sceKernelSetEventFlag failed: 0x%x\n",
				"SERVER", "BeginSendConsoleInfoData", 1015, ret);
	} else {
		*outFinished = 1;
	}

	m_pendingConsoleInfoReq = NULL;
	return ret;
}

/* 0x812B7BF0 */
int MtpServer::endSendConsoleInfoData(int httpStatus)
{
	int ret;
	if (m_consoleInfoAborted != 0) {
		SceUInt32 numWaitThreads = 0;
		ret = sceKernelCancelEventFlag(m_consoleInfoEventFlag, 0, &numWaitThreads);
		if (ret >= 0)
			return MTP_SVC_ERROR_CONSOLE_INFO_CANCELED;
		sceClibPrintf(
			"[MTP %s] Error %s(%u): sceKernelCancelEventFlag failed: 0x%x\n",
			"SERVER", "EndSendConsoleInfoData", 1029, ret);
		return ret;
	}

	ret = sceKernelSetEventFlag(m_consoleInfoEventFlag, MTP_CONSOLE_EVF_DONE);
	if (ret >= 0)
		m_consoleInfoHttpStatus = httpStatus;
	else
		sceClibPrintf("[MTP %s] Error %s(%u): sceKernelSetEventFlag failed: 0x%x\n",
			      "SERVER", "EndSendConsoleInfoData", 1038, ret);
	return ret;
}

/* 0x812B7C96 */
int MtpServer::sendConsoleInfoData(const ShellMtpHttpRequest *request, int timeoutMs)
{
	m_pendingConsoleInfoReq = request;
	unsigned int outBits = 0;
	SceUInt timeoutUs = (SceUInt)(1000 * timeoutMs);

	int ret = sceKernelWaitEventFlag(
		m_consoleInfoEventFlag, MTP_CONSOLE_EVF_DONE,
		SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
		&outBits, (timeoutUs != 0) ? &timeoutUs : NULL);

	if (ret == MTP_KERNEL_ERROR_WAIT_TIMEOUT)
		return MTP_SVC_ERROR_CONSOLE_INFO_TIMEOUT;

	if (ret == MTP_KERNEL_ERROR_WAIT_CANCEL) {
		if (sceKernelClearEventFlag(m_consoleInfoEventFlag, MTP_CONSOLE_EVF_IDLE) < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): sceKernelSetEventFlag failed: 0x%x\n",
				"SERVER", "SendConsoleInfoData", 1068,
				MTP_SVC_ERROR_CONSOLE_INFO_CANCELED);
		return MTP_SVC_ERROR_CONSOLE_INFO_CANCELED;
	}

	if (ret < 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): sceKernelWaitEventFlag failed: 0x%x\n",
			      "SERVER", "SendConsoleInfoData", 1072, ret);
		return ret;
	}

	if (outBits == MTP_CONSOLE_EVF_IDLE) {
		sceClibPrintf("[MTP %s] Error %s(%u): console info dose not started.\n",
			      "SERVER", "SendConsoleInfoData", 1076);
		return MTP_SVC_ERROR_INVALID_ARGUMENT;
	}
	if (outBits == MTP_CONSOLE_EVF_REQUEST_READY) {
		int clrRet = sceKernelClearEventFlag(m_consoleInfoEventFlag, 0);
		if (clrRet < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): sceKernelClearEventFlag failed: 0x%x\n",
				"SERVER", "SendConsoleInfoData", 1082, clrRet);
	} else if (outBits == MTP_CONSOLE_EVF_DONE) {
		int clrRet = sceKernelClearEventFlag(m_consoleInfoEventFlag, MTP_CONSOLE_EVF_IDLE);
		if (clrRet < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): sceKernelSetEventFlag failed: 0x%x\n",
				"SERVER", "SendConsoleInfoData", 1088, clrRet);
		if (m_consoleInfoHttpStatus != 200 && m_consoleInfoHttpStatus != 304) {
			sceClibPrintf(
				"[MTP %s] Error %s(%u): console info send failed: %d (0x%x)\n",
				"SERVER", "SendConsoleInfoData", 1091,
				m_consoleInfoHttpStatus, m_consoleInfoHttpStatus);
			return MTP_SVC_ERROR_INVALID_ARGUMENT;
		}
	}
	return ret;
}

/* 0x812B7E4E */
int MtpServer::cancelConsoleInfo()
{
	m_consoleInfoAborted = 1;
	return 0;
}

/* 0x812B7E64 */
void MtpServer::onSyncMethodDispatch(IPMI::Session *session, SceUInt32 methodId,
				     const IPMI::DataInfo *inList, SceUInt32 inCount,
				     IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	SceAppMgrAppStatus appStatus;
	sceAppMgrGetStatusByAppId(session->getAppId(), &appStatus);
	SceLibMtp_DE41AE27(appStatus.appName);
	int ret = mtpServiceDispatchSync(session, methodId, inList, inCount, outList, outCount);
	if (ret != 0)
		sceClibPrintf("[MTP %s] Error %s(%u): dispatchSyncMethod failed: 0x%x\n",
			      "SERVER", "onSyncMethodDispatch", 715, ret);
}

/* 0x812B7EEA */
void MtpServer::onAsyncMethodDispatch(IPMI::Session *session, SceUInt32 requestId,
				      SceUInt32 methodId, const IPMI::DataInfo *inList,
				      SceUInt32 inCount)
{
	(void)inCount;
	SceAppMgrAppStatus appStatus;
	sceAppMgrGetStatusByAppId(session->getAppId(), &appStatus);
	SceLibMtp_DE41AE27(appStatus.appName);
	int ret = mtpServiceDispatchAsync(session, requestId, methodId, inList);
	if (ret != 0)
		sceClibPrintf("[MTP %s] Error %s(%u): dispatchAsyncMethod failed: 0x%x\n",
			      "SERVER", "onAsyncMethodDispatch", 728, ret);
}

/* 0x812B7F6C */
int MtpServer::onNewClient(IPMI::Server *server, const IPMI::Session::Config *config,
			   const void *param, SceSize paramSize)
{
	(void)param;
	(void)paramSize;
	if (server != m_server && server != m_cancelServer) {
		sceClibPrintf("[MTP %s] Error %s(%u): wrong server\n",
			      "SERVER", "onNewClient", 735);
		return MTP_SVC_ERROR_INVALID_ARGUMENT;
	}

	int slotIndex = -1;
	if (server == m_server) {
		int prepRet = m_sessionMgr.prepare(&slotIndex);
		if (prepRet < 0)
			return prepRet;
	}

	void *sessionMem = sce_paf_malloc(config->bufferSize);
	if (sessionMem == NULL) {
		sceClibPrintf("[MTP %s] Error %s(%u): no memory\n", "SERVER", "onNewClient", 751);
		return MTP_SVC_ERROR_NO_MEMORY;
	}

	IPMI::Session *session = NULL;
	int ret = server->createSession(&session, (void *)(intptr_t)slotIndex, sessionMem);
	if (ret != 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): createSession failed: 0x%x\n",
			      "SERVER", "onNewClient", 758, ret);
		sce_paf_free(sessionMem);
	}
	if (server == m_server)
		m_sessionMgr.set((SceUInt32)slotIndex, session);
	return ret;
}

/* 0x812B8076 */
void MtpServer::destroySession(IPMI::Session *session)
{
	SceLibMtp_BB32CD0A();
	if (session->getServer() == m_cancelServer || m_sessionMgr.clear(session) >= 0) {
		int ret = session->destroy();
		if (ret != 0)
			sceClibPrintf("[MTP %s] Error %s(%u): Destroy session failed: 0x%x\n",
				      "SERVER", "destroySession", 796, ret);
		sce_paf_free(session);
	}
}

void MtpServer::onSessionClosedByClient(IPMI::Session *session)
{
	destroySession(session);
}

void MtpServer::onSessionKilled(IPMI::Session *session)
{
	destroySession(session);
}

/* 0x812B80DE */
void MtpServer::onFatalError(int error)
{
	sceClibPrintf("[MTP %s] Error %s(%u): 0x%x\n", "SERVER", "onFatalError", 804, error);
}

/* 0x812B8108 */
int MtpServer::serverThreadEntry(SceSize args, void *argp)
{
	(void)args;
	MtpServer *self = *(MtpServer **)argp;
	int ret = self->m_server->runDispatcher(self->m_serverWorkBuf, self->m_serverWorkBufSize);
	if (ret != 0)
		sceClibPrintf("[MTP %s] Error %s(%u): runDispatcher failed: 0x%x\n",
			      "SERVER", "serverThreadEntry", 818, ret);
	return ret;
}

/* 0x812B814A */
int MtpServer::serverCancelThreadEntry(SceSize args, void *argp)
{
	(void)args;
	MtpServer *self = *(MtpServer **)argp;
	int ret = self->m_cancelServer->runDispatcher(self->m_cancelWorkBuf,
						      self->m_cancelWorkBufSize);
	if (ret != 0)
		sceClibPrintf("[MTP %s] Error %s(%u): runDispatcher failed: 0x%x\n",
			      "SERVER", "serverCancelThreadEntry", 835, ret);
	return ret;
}

/* ---------------------------------------------------------------------------
 * Public Shell MTP Server Entry Points (0x812B1B64 - 0x812B1D27)
 * --------------------------------------------------------------------------- */

/* 0x812B1B64 */
int shellMtpServerSetConsoleInfoCallback(int (*callback)(void), int userArg)
{
	return g_mtpServer.setConsoleInfoCallback((MtpConsoleInfoStartCallback)callback,
						  (void *)(intptr_t)userArg);
}

/* 0x812B1B7A */
int shellMtpServerHttpRequest(const ShellMtpHttpRequest *request, int timeout)
{
	return g_mtpServer.sendConsoleInfoData(request, timeout);
}

/* 0x812B1B90 */
int shellMtpServerStart(ShellMtpServerParam *param)
{
	int ret = g_mtpServer.initialize(param);
	if (ret >= 0) {
		ret = g_mtpServer.start();
		if (ret < 0)
			g_mtpServer.finalize();
	}
	return ret;
}

/* 0x812B1BCC */
int shellMtpServerNotifyApp6(int processId, int appId, int titlePtr)
{
	(void)appId;
	(void)titlePtr;
	g_mtpServer.notifyProcessDeactivated((SceUID)processId);
	return 0;
}

/* 0x812B1BE2 */
int shellMtpServerNotifyAppActivated(int processId, int appId, int titlePtr)
{
	g_mtpServer.notifyProcessActivated((SceUID)processId, appId,
					   (const char *)(uintptr_t)titlePtr);
	return 0;
}

/* 0x812B1BF8 */
int shellMtpServerNotifyAppEnded(int processId, int appId, const char *titleId)
{
	g_mtpServer.notifyProcessEnded((SceUID)processId, appId, titleId);
	return 0;
}

/* 0x812B1C0E */
int shellMtpServerNotifyShellEvent(const void *eventData)
{
	g_mtpServer.notifyLiveAreaOrient((const char *)eventData);
	return 0;
}

/* 0x812B1C22 */
int mtpRegisterHandler(void *handler)
{
	g_mtpServer.setEventListener((MtpEventListener *)handler);
	return 0;
}

/* 0x812B1C36 */
int mtpSwitchUsbMode(int usbMode)
{
	return g_mtpServer.switchUsbMode(usbMode);
}

/* 0x812B1C4A */
int mtpIsDialogSuppressed(void)
{
	return MtpServer::isDialogSuppressed() ? 1 : 0;
}

/* 0x812B1C5A */
void mtpResetStorage(void)
{
	g_mtpServer.enqueueStorageResetJob();
}

/* 0x812B1C6A */
int mtpIsCmaActive(void)
{
	return g_mtpServer.isCmaActive();
}

/* 0x812B1C7A */
int shellMtpWiFiTerminate(void)
{
	g_mtpServer.notifyWifiTerminate();
	return 0;
}

paf::job::JobQueue *shellGetMtpJobQueue(void)
{
	return paf::job::JobQueue::default_queue;
}

/* 0x812B1C8A */
void mtpNotifyAccountLinkResult(const MtpAccountLinkInfo *linkInfo, int broadcastFlag)
{
	int statusCode = 0;
	if (linkInfo->errorCode == -1 && linkInfo->hasErrorFlag != 0) {
		statusCode = 2;
	} else if (linkInfo->accountType == 1) {
		if (linkInfo->linked != 0 && linkInfo->hostLinked != 0 && linkInfo->hostValid != 0)
			statusCode = (linkInfo->hostAccountId == linkInfo->localAccountId) ? 1 : 4;
		else
			statusCode = 3;
	} else if (linkInfo->accountType == 2) {
		statusCode = (linkInfo->linked != 0) ? 1 : 3;
	}

	SceLibMtp_3AF2F5D8(statusCode);
	MtpStatusEventMsg eventMsg;
	sce_paf_memset(&eventMsg, 0, sizeof(eventMsg));
	sceMtpGetHostInfo(&eventMsg.hostStatus);
	if (broadcastFlag != 0) {
		eventMsg.eventCode = MTP_HOST_EVENT_AVAILABLE;
		g_mtpServer.sessionMgr().broadcastMtpStatusEvent(&eventMsg);
	}
}
