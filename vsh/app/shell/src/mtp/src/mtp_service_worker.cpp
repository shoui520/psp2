/*
 * mtp_service_worker.cpp: MtpServiceWorker ("SRVWKR", 0x812B1D50 - 0x812B2727)
 * Serializes asynchronous MTP send/receive data phase transfers for active IPMI requests.
 */
#include "mtp_server_internal.h"

/* 0x812B1D50 */
MtpServiceWorker::MtpServiceWorker()
	: m_session(NULL),
	  m_requestId(0),
	  m_contextId(0),
	  m_transferSize(0),
	  m_transferBuffer(NULL),
	  m_isRecv(0),
	  m_jobPending(0),
	  m_waitingForFinish(0),
	  m_eventFlag(0),
	  m_state(0),
	  m_runningEventFlag(0),
	  m_completedCount(0),
	  m_queuedCount(0)
{
}

/* 0x812B1E2C */
MtpServiceWorker::~MtpServiceWorker()
{
	finalize();
}

void MtpServiceWorker::lock()
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): sccKernelLockLwMutext failed: 0x%x\n",
			      "SRVWKR", "lock", 350, ret);
}

void MtpServiceWorker::unlock()
{
	int ret = sceKernelUnlockLwMutex(&m_mutex, 1);
	if (ret < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): sceKernelUnlockLwMutex failed: 0x%x\n",
			      "SRVWKR", "unlock", 359, ret);
}

/* 0x812B1E38 */
int MtpServiceWorker::initialize()
{
	int ret = sceKernelCreateLwMutex(&m_mutex, "SceMtpServiceWorker", 0x2000u, 0, NULL);
	if (ret < 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): sceKernelCreateLwMutex failed: 0x%x\n",
			      "SRVWKR", "Initialize", 46, ret);
		return ret;
	}

	SceUID evf = sceKernelCreateEventFlag("SceMtpServiceWorker", 0, 0, NULL);
	if (evf < 0) {
		sceKernelDeleteLwMutex(&m_mutex);
		sceClibPrintf("[MTP %s] Error %s(%u): sceKernelCreateEventFlag failed: 0x%x\n",
			      "SRVWKR", "Initialize", 52, evf);
		return evf;
	}
	m_eventFlag = evf;

	SceUID runEvf = sceKernelCreateEventFlag("SceMtpServiceWorkerRunning", 0x1080u, 0, NULL);
	if (runEvf < 0) {
		sceKernelDeleteEventFlag(m_eventFlag);
		sceKernelDeleteLwMutex(&m_mutex);
		sceClibPrintf("[MTP %s] Error %s(%u): sceKernelCreateEventFlag failed: 0x%x\n",
			      "SRVWKR", "Initialize", 61, runEvf);
		return runEvf;
	}
	m_runningEventFlag = runEvf;

	ret = sceKernelCreateLwCond(&m_condJobFinish, "SceMtpServiceWorkerCondJobFinish",
				    0, &m_mutex, NULL);
	if (ret < 0) {
		sceKernelDeleteEventFlag(m_eventFlag);
		sceKernelDeleteEventFlag(m_runningEventFlag);
		sceKernelDeleteLwMutex(&m_mutex);
		sceClibPrintf("[MTP %s] Error %s(%u): sceKernelCreateEventFlag failed: 0x%x\n",
			      "SRVWKR", "Initialize", 71, ret);
		return ret;
	}

	m_completedCount = 0;
	m_queuedCount = 0;
	return 0;
}

/* 0x812B1D6C */
int MtpServiceWorker::finalize()
{
	if (m_eventFlag != 0) {
		int ret = sceKernelDeleteEventFlag(m_eventFlag);
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): sceKernelDeleteEventFlag failed: 0x%x\n",
				      "SRVWKR", "Finalize", 87, ret);
		ret = sceKernelDeleteEventFlag(m_runningEventFlag);
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): sceKernelDeleteEventFlag failed: 0x%x\n",
				      "SRVWKR", "Finalize", 91, ret);
		ret = sceKernelDeleteLwMutex(&m_mutex);
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): sceKernelDeleteLwMutex failed: 0x%x\n",
				      "SRVWKR", "Finalize", 95, ret);
		ret = sceKernelDeleteLwCond(&m_condJobFinish);
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): sceKernelDeleteLwCond failed: 0x%x\n",
				      "SRVWKR", "Finalize", 99, ret);
		m_eventFlag = 0;
		m_runningEventFlag = 0;
	}
	return 0;
}

/* 0x812B1F70 */
int MtpServiceWorker::execSendRecvJob()
{
	lock();
	if (m_jobPending == 0) {
		unlock();
		return MTP_SVC_ERROR_INVALID_ARGUMENT;
	}
	unlock();

	int ret;
	if (m_isRecv == 1) {
		SceSize received = 0;
		SceUInt64 totalSize = 0;
		int mtpRes = SceLibMtp_CA704934(m_contextId, m_transferBuffer, m_transferSize,
						&received, &totalSize);
		ret = g_mtpServer.sessionMgr().lockSession(m_session);
		if (ret >= 0) {
			m_completedCount++;
			ret = mtpServiceRespondRecvData(m_session, m_requestId, mtpRes,
							m_transferBuffer, &received, &totalSize);
			if (ret < 0)
				sceClibPrintf("[MTP %s] Error %s(%u): EndRecvData failed: 0x%x %u\n",
					      "SRVWKR", "doRecvJob", 320, ret, received);
			g_mtpServer.sessionMgr().unlockSession();
		}
	} else {
		SceSize sent = 0;
		SceUInt64 totalSize = 0;
		int mtpRes = sceMtpSendData(m_contextId, m_transferBuffer, m_transferSize,
					    &sent, &totalSize);
		ret = g_mtpServer.sessionMgr().lockSession(m_session);
		if (ret >= 0) {
			m_completedCount++;
			ret = mtpServiceRespondSendData(m_session, m_requestId, mtpRes,
							&sent, &totalSize);
			if (ret < 0)
				sceClibPrintf("[MTP %s] Error %s(%u): EndSendData failed: 0x%x %u\n",
					      "SRVWKR", "doSendJob", 340, ret, sent);
			g_mtpServer.sessionMgr().unlockSession();
		}
	}

	SceLibMtp_492421F7(m_transferBuffer);

	lock();
	m_jobPending = 0;
	if (m_waitingForFinish != 0) {
		ret = sceKernelSignalLwCond(&m_condJobFinish);
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): sceKernelSetEventFlag failed: 0x%x\n",
				      "SRVWKR", "execSendRecvJob", 298, ret);
		m_waitingForFinish = 0;
	}
	unlock();
	return ret;
}

/* 0x812B2204 */
int MtpServiceWorker::run(SceUInt32 contextId)
{
	SceUInt timeout = 10000000u;
	unsigned int outBits = 0;

	m_state = 1;
	int setRet = sceKernelSetEventFlag(m_runningEventFlag, MTP_WORKER_RUN_EVF_STARTED);
	if (setRet < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): sceKernelSetEventFlag failed: 0x%x\n",
			      "SRVWKR", "Run", 118, setRet);

	int waitRet = 0;
	for (;;) {
		waitRet = sceKernelWaitEventFlag(m_eventFlag,
						 MTP_WORKER_EVF_TERMINATE | MTP_WORKER_EVF_JOB_READY,
						 SCE_KERNEL_EVF_WAITMODE_OR |
						 SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
						 &outBits, &timeout);
		if (waitRet == MTP_KERNEL_ERROR_WAIT_TIMEOUT) {
			if (sceMtpCheckContextValid(contextId) == MTP_SVC_ERROR_NOT_INITIALIZED)
				break;
			timeout = 10000000u;
			continue;
		}
		if (waitRet < 0) {
			sceClibPrintf("[MTP %s] Error %s(%u): sceKernelWaitEventFlag failed: 0x%x\n",
				      "SRVWKR", "Run", 133, waitRet);
			break;
		}
		if ((outBits & MTP_WORKER_EVF_JOB_READY) != 0)
			execSendRecvJob();
		if ((outBits & MTP_WORKER_EVF_TERMINATE) != 0)
			break;
		timeout = 10000000u;
	}

	execSendRecvJob();
	while (m_queuedCount != m_completedCount) {
		timeout = 10000000u;
		if (sceKernelWaitEventFlag(m_eventFlag,
					   MTP_WORKER_EVF_TERMINATE | MTP_WORKER_EVF_JOB_READY,
					   SCE_KERNEL_EVF_WAITMODE_OR |
					   SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
					   &outBits, &timeout) != 0)
			break;
		execSendRecvJob();
	}

	m_completedCount = 0;
	m_queuedCount = 0;
	return waitRet;
}

/* 0x812B2326 */
int MtpServiceWorker::reset()
{
	lock();
	sceKernelClearEventFlag(m_eventFlag, 0);
	sceKernelClearEventFlag(m_runningEventFlag, 0);
	m_state = 0;
	unlock();
	return 0;
}

/* 0x812B23A0 */
int MtpServiceWorker::setTerminate()
{
	lock();
	m_state = 2;
	unlock();

	int ret = sceKernelSetEventFlag(m_runningEventFlag, MTP_WORKER_RUN_EVF_TERMINATED);
	if (ret < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): sceKernelSetEventFlag failed: 0x%x\n",
			      "SRVWKR", "SetTerminate", 182, ret);

	ret = sceKernelSetEventFlag(m_eventFlag, MTP_WORKER_EVF_TERMINATE);
	if (ret < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): sceKernelSetEventFlag failed: 0x%x\n",
			      "SRVWKR", "SetTerminate", 187, ret);
	return ret;
}

/* 0x812B2466 */
int MtpServiceWorker::setSendRecvJob(IPMI::Session *session, SceUInt32 requestId,
				     SceUInt32 contextId, void *buffer,
				     SceSize size, int isRecv)
{
	if (m_state == 2) {
		sceClibPrintf("[MTP %s] Error %s(%u): already finished (%d) context %u\n",
			      "SRVWKR", "setSendRecvJob", 238, m_state, contextId);
		return MTP_SVC_ERROR_INVALID_ARGUMENT;
	}

	while (m_jobPending != 0) {
		m_waitingForFinish = 1;
		SceUInt timeout = 40000000u;
		int waitRet = sceKernelWaitLwCond(&m_condJobFinish, &timeout);
		if (waitRet < 0) {
			sceClibPrintf("[MTP %s] Error %s(%u): sceKernelWaitLwCond failed: 0x%x\n",
				      "SRVWKR", "setSendRecvJob", 247, waitRet);
			return MTP_SVC_ERROR_INTERNAL;
		}
	}

	m_session = session;
	m_requestId = requestId;
	m_contextId = contextId;
	m_transferSize = size;
	m_transferBuffer = buffer;
	m_isRecv = isRecv;
	m_jobPending = 1;

	int ret = sceKernelSetEventFlag(m_eventFlag, MTP_WORKER_EVF_JOB_READY);
	if (ret < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): sceKernelSetEventFlag failed: 0x%x\n",
			      "SRVWKR", "setSendRecvJob", 268, ret);
	return ret;
}

/* 0x812B2580 */
int MtpServiceWorker::setRecvJob(IPMI::Session *session, SceUInt32 requestId,
				 SceUInt32 contextId, SceSize size)
{
	m_queuedCount++;
	void *buf = SceLibMtp_DA13726B(size);
	if (buf == NULL) {
		sceClibPrintf("[MTP %s] Error %s(%u): no memory\n", "SRVWKR", "SetRecvJob", 198);
		return MTP_SVC_ERROR_NO_MEM_POOL;
	}

	lock();
	int ret = setSendRecvJob(session, requestId, contextId, buf, size, 1);
	unlock();
	if (ret < 0)
		SceLibMtp_492421F7(buf);
	return ret;
}

/* 0x812B264C */
int MtpServiceWorker::setSendJob(IPMI::Session *session, SceUInt32 requestId,
				 SceUInt32 contextId, const void *srcData, SceSize size)
{
	m_queuedCount++;
	void *buf = SceLibMtp_DA13726B(size);
	if (buf == NULL) {
		sceClibPrintf("[MTP %s] Error %s(%u): no memory\n", "SRVWKR", "SetSendJob", 218);
		return MTP_SVC_ERROR_NO_MEM_POOL;
	}

	sce_paf_memcpy(buf, srcData, size);
	lock();
	int ret = setSendRecvJob(session, requestId, contextId, buf, size, 0);
	unlock();
	if (ret < 0)
		SceLibMtp_492421F7(buf);
	return ret;
}
