/*
 * mtp_service_dispatch.cpp: SceMtpService IPMI sync/async method dispatcher and
 * libmtp async request/completion bridges ("SERVICE", 0x812B51C6 - 0x812B67FF,
 * 0x812B81D0 - 0x812BC4EF, 0x81432738 - 0x814327DB).
 */
#include <appmgr/appmgr.h>
#include "mtp_server_internal.h"

/* ---------------------------------------------------------------------------
 * Service Worker & Cancel JobItems (0x81432738 - 0x814327DB)
 * --------------------------------------------------------------------------- */

class MtpCancelExecutionJob : public paf::job::JobItem {
public:
	explicit MtpCancelExecutionJob(SceUInt32 contextId)
		: paf::job::JobItem("SceMtpCancelExecutionJob", NULL),
		  m_contextId(contextId)
	{
	}

	virtual ~MtpCancelExecutionJob() {}

	/* 0x81432774 */
	virtual int Run()
	{
		return SceLibMtp_1021F75F(m_contextId);
	}

	virtual void Finish(int result) { (void)result; }

private:
	SceUInt32 m_contextId;
};

class MtpServiceWorkerJob : public paf::job::JobItem {
public:
	MtpServiceWorkerJob(MtpServiceWorker *worker, SceUInt32 contextId,
			    IPMI::Session *session, SceUInt32 requestId,
			    MtpServiceEndCallback endCallback)
		: paf::job::JobItem("SceMtpServiceWorkerJob", NULL),
		  m_worker(worker),
		  m_contextId(contextId),
		  m_session(session),
		  m_requestId(requestId),
		  m_endCallback(endCallback)
	{
		m_worker->reset();
	}

	virtual ~MtpServiceWorkerJob() {}

	/* 0x814327C0 */
	virtual int Run()
	{
		m_worker->run(m_contextId);
		return m_endCallback(m_session, m_requestId, m_contextId);
	}

	virtual void Finish(int result) { (void)result; }

private:
	MtpServiceWorker *m_worker;
	SceUInt32 m_contextId;
	IPMI::Session *m_session;
	SceUInt32 m_requestId;
	MtpServiceEndCallback m_endCallback;
};

/* 0x812BB1C0 */
static void onMtpContextFinished(void *userArg)
{
	(void)userArg;
	g_mtpServer.worker().setTerminate();
}

/* 0x812BC092 */
static void onNetMtpRegisterCallback(const SceUInt32 *eventCode)
{
	g_mtpServer.sessionMgr().broadcastNetMtpRegisterEvent(eventCode);
}

/* 0x812B81D0 */
int MtpServer::startServiceJob(IPMI::Session *session, SceUInt32 requestId,
			       SceUInt32 contextId, MtpServiceEndCallback endCallback)
{
	if (m_activationMgr.m_deactivatePending != 0) {
		int ret = sceKernelSetEventFlag(m_serviceEventFlag, MTP_EVF_CONNECTED);
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): sceKernelSetEventFlag failed: 0x%x\n",
				"SERVICE", "StartServiceJob", 100, ret);
	}

	paf::common::SharedPtr<paf::job::JobItem> job(
		new MtpServiceWorkerJob(&m_worker, contextId, session, requestId, endCallback));
	if (job.get() == NULL)
		return MTP_SVC_ERROR_INTERNAL;
	return enqueueServiceJob(job);
}

/* ---------------------------------------------------------------------------
 * IPMI Async Response Helpers (0x812B51C6 - 0x812B5975)
 * --------------------------------------------------------------------------- */

#define MTP_DEFINE_RESPOND_EMPTY(FnName, MethodId) \
static int FnName(IPMI::Session *session, SceUInt32 requestId, int result) \
{ \
	return session->respondToAsyncMethodRequest((MethodId), (int)requestId, result, NULL, 0); \
}

#define MTP_DEFINE_RESPOND_SINGLE(FnName, MethodId) \
static int FnName(IPMI::Session *session, SceUInt32 requestId, int result, \
		  const void *outBuf, SceSize outSize) \
{ \
	IPMI::DataInfo outInfo; \
	outInfo.pData = outBuf; \
	outInfo.dataSize = outSize; \
	return session->respondToAsyncMethodRequest((MethodId), (int)requestId, result, \
						    &outInfo, 1); \
}

#define MTP_DEFINE_RESPOND_RPC_NET(FnName, MethodId) \
static int FnName(IPMI::Session *session, SceUInt32 requestId, int result, \
		  const void *responseBuf, const SceSize *responseSize) \
{ \
	IPMI::DataInfo outInfo[2]; \
	outInfo[0].pData = responseBuf; \
	outInfo[0].dataSize = *responseSize; \
	outInfo[1].pData = responseSize; \
	outInfo[1].dataSize = sizeof(SceSize); \
	return session->respondToAsyncMethodRequest((MethodId), (int)requestId, result, \
						    outInfo, 2); \
}

MTP_DEFINE_RESPOND_SINGLE(respondEndGetNumOfObject, MTP_METHOD_END_GET_NUM_OF_OBJECT)
MTP_DEFINE_RESPOND_EMPTY(respondEndGetObjectMetadata, MTP_METHOD_END_GET_OBJECT_METADATA)
MTP_DEFINE_RESPOND_SINGLE(respondEndImportObject, MTP_METHOD_END_IMPORT_OBJECT)
MTP_DEFINE_RESPOND_EMPTY(respondEndExportObject, MTP_METHOD_END_EXPORT_OBJECT)
MTP_DEFINE_RESPOND_SINGLE(respondEndCreateHostDir, MTP_METHOD_END_CREATE_HOST_DIR)
MTP_DEFINE_RESPOND_EMPTY(respondEndRenameHostDir, MTP_METHOD_END_RENAME_HOST_DIR)
MTP_DEFINE_RESPOND_EMPTY(respondEndMoveHostDir, MTP_METHOD_END_MOVE_HOST_DIR)
MTP_DEFINE_RESPOND_SINGLE(respondEndCreateHostFile, MTP_METHOD_END_CREATE_HOST_FILE)
MTP_DEFINE_RESPOND_EMPTY(respondEndGetObjectThumbnail, MTP_METHOD_END_GET_OBJECT_THUMBNAIL)
MTP_DEFINE_RESPOND_EMPTY(respondEndGetObjectStatus, MTP_METHOD_END_GET_OBJECT_STATUS)
MTP_DEFINE_RESPOND_EMPTY(respondEndDeleteObject, MTP_METHOD_END_DELETE_OBJECT)
MTP_DEFINE_RESPOND_EMPTY(respondEndHttpGetDataWithUrl, MTP_METHOD_END_HTTP_GET_DATA_WITH_URL)
MTP_DEFINE_RESPOND_EMPTY(respondEndGetSystemSetting, MTP_METHOD_END_GET_SYSTEM_SETTING)
MTP_DEFINE_RESPOND_EMPTY(respondEndSetSystemSetting, MTP_METHOD_END_SET_SYSTEM_SETTING)
MTP_DEFINE_RESPOND_SINGLE(respondEndGetNpAccountInfo, MTP_METHOD_END_GET_NP_ACCOUNT_INFO)
MTP_DEFINE_RESPOND_EMPTY(respondEndReadObjectWithOffset, MTP_METHOD_END_READ_OBJECT_WITH_OFFSET)
MTP_DEFINE_RESPOND_EMPTY(respondEndWriteObjectWithOffset, MTP_METHOD_END_WRITE_OBJECT_WITH_OFFSET)
MTP_DEFINE_RESPOND_SINGLE(respondEndGetHostStorageSize, MTP_METHOD_END_GET_HOST_STORAGE_SIZE)
MTP_DEFINE_RESPOND_SINGLE(respondEndCheckSameObjectExist, MTP_METHOD_END_CHECK_SAME_OBJECT_EXIST)
MTP_DEFINE_RESPOND_EMPTY(respondEndGetSpecifiedMetadata, MTP_METHOD_END_GET_SPECIFIED_METADATA)
MTP_DEFINE_RESPOND_RPC_NET(respondEndRpcNetOperation, MTP_METHOD_END_RPC_NET_OPERATION)
MTP_DEFINE_RESPOND_RPC_NET(respondEndRpcNetOperationSend, MTP_METHOD_END_RPC_NET_OPERATION_SEND)
MTP_DEFINE_RESPOND_EMPTY(respondEndRpcNetOperationRecv, MTP_METHOD_END_RPC_NET_OPERATION_RECV)
MTP_DEFINE_RESPOND_EMPTY(respondEndConnectToHost, MTP_METHOD_END_CONNECT_TO_HOST)
MTP_DEFINE_RESPOND_EMPTY(respondEndHandover, MTP_METHOD_END_HANDOVER)
MTP_DEFINE_RESPOND_EMPTY(respondEndResumeExportObject, MTP_METHOD_END_RESUME_EXPORT_OBJECT)
MTP_DEFINE_RESPOND_SINGLE(respondEndResumeImportObject, MTP_METHOD_END_RESUME_IMPORT_OBJECT)

#undef MTP_DEFINE_RESPOND_EMPTY
#undef MTP_DEFINE_RESPOND_SINGLE
#undef MTP_DEFINE_RESPOND_RPC_NET

/* 0x812B5728 */
int mtpServiceRespondRecvData(IPMI::Session *session, SceUInt32 requestId, int result,
			      const void *buffer, SceSize *receivedSize,
			      SceUInt64 *totalSize)
{
	IPMI::DataInfo outInfo[3];
	outInfo[0].pData = buffer;
	outInfo[0].dataSize = *receivedSize;
	outInfo[1].pData = receivedSize;
	outInfo[1].dataSize = sizeof(SceSize);
	outInfo[2].pData = totalSize;
	outInfo[2].dataSize = sizeof(SceUInt64);
	return session->respondToAsyncMethodRequest(MTP_METHOD_END_RECV_DATA, (int)requestId,
						    result, outInfo, 3);
}

/* 0x812B577E */
int mtpServiceRespondSendData(IPMI::Session *session, SceUInt32 requestId, int result,
			      const SceSize *sentSize, const SceUInt64 *totalSize)
{
	IPMI::DataInfo outInfo[2];
	outInfo[0].pData = sentSize;
	outInfo[0].dataSize = sizeof(SceSize);
	outInfo[1].pData = totalSize;
	outInfo[1].dataSize = sizeof(SceUInt64);
	return session->respondToAsyncMethodRequest(MTP_METHOD_END_SEND_DATA, (int)requestId,
						    result, outInfo, 2);
}

static int respondAsyncHttpGetProperty(IPMI::Session *session, SceUInt32 requestId, int result,
				       const void *sizeBuf, const void *propBuf)
{
	IPMI::DataInfo outInfo[2];
	outInfo[0].pData = sizeBuf;
	outInfo[0].dataSize = 8;
	outInfo[1].pData = propBuf;
	outInfo[1].dataSize = 16;
	return session->respondToAsyncMethodRequest(MTP_METHOD_END_HTTP_GET_PROPERTY_WITH_URL,
						    (int)requestId, result, outInfo, 2);
}

static int respondAsyncTotalObjectSize(IPMI::Session *session, SceUInt32 requestId, int result,
				       const void *countOut, const void *sizeOut)
{
	IPMI::DataInfo outInfo[2];
	outInfo[0].pData = countOut;
	outInfo[0].dataSize = 4;
	outInfo[1].pData = sizeOut;
	outInfo[1].dataSize = 8;
	return session->respondToAsyncMethodRequest(MTP_METHOD_END_GET_TOTAL_OBJECT_SIZE,
						    (int)requestId, result, outInfo, 2);
}

static int respondAsyncNpDrmGetRtc(IPMI::Session *session, SceUInt32 requestId, int result,
				   const void *rtcOut, const int *httpStatus)
{
	IPMI::DataInfo outInfo[2];
	outInfo[0].pData = rtcOut;
	outInfo[0].dataSize = 304;
	outInfo[1].pData = httpStatus;
	outInfo[1].dataSize = sizeof(int);
	return session->respondToAsyncMethodRequest(MTP_METHOD_END_NPDRM_GET_RTC,
						    (int)requestId, result, outInfo, 2);
}

static int respondAsyncNpDrmActivate(IPMI::Session *session, SceUInt32 requestId, int result,
				     const void *actDatOut, const int *httpStatus,
				     const int *npError, const SceUInt32 *resultFlags)
{
	IPMI::DataInfo outInfo[4];
	outInfo[0].pData = actDatOut;
	outInfo[0].dataSize = 4240;
	outInfo[1].pData = httpStatus;
	outInfo[1].dataSize = sizeof(int);
	outInfo[2].pData = npError;
	outInfo[2].dataSize = sizeof(int);
	outInfo[3].pData = resultFlags;
	outInfo[3].dataSize = sizeof(SceUInt32);
	return session->respondToAsyncMethodRequest(MTP_METHOD_END_NPDRM_ACTIVATE,
						    (int)requestId, result, outInfo, 4);
}

static int respondAsyncNpDrmDeactivate(IPMI::Session *session, SceUInt32 requestId, int result,
				       const int *httpStatus, const int *npError,
				       const SceUInt32 *resultFlags)
{
	IPMI::DataInfo outInfo[3];
	outInfo[0].pData = httpStatus;
	outInfo[0].dataSize = sizeof(int);
	outInfo[1].pData = npError;
	outInfo[1].dataSize = sizeof(int);
	outInfo[2].pData = resultFlags;
	outInfo[2].dataSize = sizeof(SceUInt32);
	return session->respondToAsyncMethodRequest(MTP_METHOD_END_NPDRM_DEACTIVATE,
						    (int)requestId, result, outInfo, 3);
}

static int respondAsyncNpDrmGetLicense(IPMI::Session *session, SceUInt32 requestId, int result,
				       const void *licenseOut, const int *httpStatus)
{
	IPMI::DataInfo outInfo[2];
	outInfo[0].pData = licenseOut;
	outInfo[0].dataSize = 516;
	outInfo[1].pData = httpStatus;
	outInfo[1].dataSize = sizeof(int);
	return session->respondToAsyncMethodRequest(MTP_METHOD_END_NPDRM_GET_LICENSE,
						    (int)requestId, result, outInfo, 2);
}

static int respondAsyncSearchObject(IPMI::Session *session, SceUInt32 requestId, int result,
				    const SceUInt32 *numFound, const SceUInt32 *searchId)
{
	IPMI::DataInfo outInfo[2];
	outInfo[0].pData = numFound;
	outInfo[0].dataSize = sizeof(SceUInt32);
	outInfo[1].pData = searchId;
	outInfo[1].dataSize = sizeof(SceUInt32);
	return session->respondToAsyncMethodRequest(MTP_METHOD_END_SEARCH_OBJECT,
						    (int)requestId, result, outInfo, 2);
}

/* Helper shared by every Begin* handler after libmtp returns a contextId */
static int scheduleAsyncContext(IPMI::Session *session, SceUInt32 requestId,
				SceUInt32 contextId, MtpServiceEndCallback endCallback)
{
	g_mtpServer.sessionMgr().addContext(session, requestId, contextId);
	g_mtpServer.startServiceJob(session, requestId, contextId, endCallback);
	int cbRet = sceMtpSetFinishCallback(contextId, onMtpContextFinished, NULL);
	if (cbRet == MTP_SVC_ERROR_CONTEXT_FINISHED)
		g_mtpServer.worker().setTerminate();
	return cbRet;
}

/* ---------------------------------------------------------------------------
 * Async Completion Callbacks (0x812BB1D4 - 0x812BC4EF)
 * --------------------------------------------------------------------------- */

/* 0x812BB1D4 */
static int endGetNumOfObject(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	SceUInt32 numObjects = 0;
	int mtpRes = SceLibMtp_123CA37E(contextId, &numObjects);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndGetNumOfObject(session,
					 requestId, mtpRes, &numObjects, sizeof(numObjects));
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndGetNumOfObject failed: 0x%x\n",
				      "SERVICE", "endGetNumOfObject", 164, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BB272 */
static int endGetObjectMetadata(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	int mtpRes = SceLibMtp_4F285949(contextId);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndGetObjectMetadata(session,
					requestId, mtpRes);
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndGetObjectMetadata failed: 0x%x\n",
				"SERVICE", "endGetObjectMetadata", 199, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BB2E6 */
static int endImportObject(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	SceUInt64 objectId = 0;
	int mtpRes = sceMtpEndImportObject(contextId, &objectId);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	if (mtpRes < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): sceMtpEndImportObject failed: 0x%x\n",
			      "SERVICE", "endImportObject", 230, mtpRes);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		int sendRet = respondEndImportObject(session,
						 requestId, mtpRes, &objectId, sizeof(objectId));
		if (sendRet < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndImportObject failed: 0x%x\n",
				      "SERVICE", "endImportObject", 238, sendRet);
		g_mtpServer.sessionMgr().unlockSession();
		return 0;
	}
	return ret;
}

/* 0x812BB3B0 */
static int endExportObject(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	int mtpRes = SceLibMtp_8F3DBF97(contextId);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndExportObject(session,
					requestId, mtpRes);
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndExportObject failed: 0x%x\n",
				      "SERVICE", "endExportObject", 274, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BB426 */
static int endCreateHostDir(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	SceUInt32 dirHandle = 0;
	int mtpRes = SceLibMtp_549279DC(contextId, &dirHandle);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndCreateHostDir(session,
					 requestId, mtpRes, &dirHandle, sizeof(dirHandle));
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndCreateHostDir failed: 0x%x\n",
				      "SERVICE", "endCreateHostDir", 309, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BB4C6 */
static int endRenameHostDir(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	int mtpRes = SceLibMtp_F2A1482E(contextId);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndRenameHostDir(session,
					requestId, mtpRes);
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndRenameHostDir failed: 0x%x\n",
				      "SERVICE", "endRenameHostDir", 343, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BB53C */
static int endMoveHostDir(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	int mtpRes = SceLibMtp_6079A84A(contextId);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndMoveHostDir(session,
					requestId, mtpRes);
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndMoveHostDir failed: 0x%x\n",
				      "SERVICE", "endMoveHostDir", 376, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BB5B2 */
static int endCreateHostFile(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	SceUInt32 fileHandle = 0;
	int mtpRes = SceLibMtp_F195BEC6(contextId, &fileHandle);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndCreateHostFile(session,
					 requestId, mtpRes, &fileHandle, sizeof(fileHandle));
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndCreateHostFile failed: 0x%x\n",
				      "SERVICE", "endCreateHostFile", 410, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BB652 */
static int endGetObjectThumbnail(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	int mtpRes = SceLibMtp_709C09B8(contextId);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndGetObjectThumbnail(session,
					requestId, mtpRes);
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndGetObjectThumbnail failed: 0x%x\n",
				"SERVICE", "endGetObjectThumbnail", 445, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BB6C8 */
static int endGetObjectStatus(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	int mtpRes = SceLibMtp_7A229158(contextId);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndGetObjectStatus(session,
					requestId, mtpRes);
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndGetObjectStatus failed: 0x%x\n",
				"SERVICE", "endGetObjectStatus", 479, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BB73E */
static int endDeleteObject(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	int mtpRes = SceLibMtp_F988D2EE(contextId);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndDeleteObject(session,
					requestId, mtpRes);
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndDeleteObject failed: 0x%x\n",
				      "SERVICE", "endDeleteObject", 512, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BB7B4 */
static int endHttpGetDataWithUrl(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	int mtpRes = SceLibMtp_520D2696(contextId);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndHttpGetDataWithUrl(session,
					requestId, mtpRes);
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndHttpGetDataWithUrl failed: 0x%x\n",
				"SERVICE", "endHttpGetDataWithUrl", 545, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BB82A */
static int endHttpGetPropertyWithUrl(IPMI::Session *session, SceUInt32 requestId,
				     SceUInt32 contextId)
{
	SceUInt8 contentLength[8];
	SceUInt8 propertyHeader[16];
	int mtpRes = SceLibMtp_54CDF039(contextId, (int)(uintptr_t)contentLength,
					(int)(uintptr_t)propertyHeader);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondAsyncHttpGetProperty(session, requestId, mtpRes,
						  contentLength, propertyHeader);
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndHttpGetPropertyWithUrl failed: 0x%x\n",
				"SERVICE", "endHttpGetPropertyWithUrl", 580, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BB8CE */
static int endGetSystemSetting(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	int mtpRes = SceLibMtp_D5FB089F(contextId);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndGetSystemSetting(session,
					requestId, mtpRes);
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndGetSystemSetting failed: 0x%x\n",
				"SERVICE", "endGetSystemSetting", 615, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BB944 */
static int endSetSystemSetting(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	int mtpRes = SceLibMtp_EAA968F4(contextId);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndSetSystemSetting(session,
					requestId, mtpRes);
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndSetSystemSetting failed: 0x%x\n",
				"SERVICE", "endSetSystemSetting", 648, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BB9BA */
static int endGetNpAccountInfo(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	SceUInt32 accountStatus = 0;
	int mtpRes = SceLibMtp_DD5917EF(contextId, &accountStatus);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndGetNpAccountInfo(session,
					 requestId, mtpRes, &accountStatus, sizeof(accountStatus));
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndGetNpAccountInfo failed: 0x%x\n",
				"SERVICE", "endGetNpAccountInfo", 682, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BBA56 */
static int endReadObjectWithOffset(IPMI::Session *session, SceUInt32 requestId,
				   SceUInt32 contextId)
{
	int mtpRes = SceLibMtp_7FDDA203(contextId);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndReadObjectWithOffset(session,
					requestId, mtpRes);
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndReadObjectWithOffset failed: 0x%x\n",
				"SERVICE", "endReadObjectWithOffset", 716, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BBACC */
static int endWriteObjectWithOffset(IPMI::Session *session, SceUInt32 requestId,
				    SceUInt32 contextId)
{
	int mtpRes = SceLibMtp_C5E9C54E(contextId);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndWriteObjectWithOffset(session,
					requestId, mtpRes);
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndWriteObjectWithOffset failed: 0x%x\n",
				"SERVICE", "endWriteObjectWithOffset", 749, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BBB42 */
static int endGetHostStorageSize(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	SceUInt8 storageInfo[28];
	int mtpRes = SceLibMtp_4CAFB048(contextId, storageInfo);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndGetHostStorageSize(session,
					 requestId, mtpRes, storageInfo, sizeof(storageInfo));
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndGetHostStorageSize failed: 0x%x\n",
				"SERVICE", "endGetHostStorageSize", 783, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BBBDE */
static int endCheckSameObjectExist(IPMI::Session *session, SceUInt32 requestId,
				   SceUInt32 contextId)
{
	SceUInt32 exists = 0;
	int mtpRes = SceLibMtp_905BB9E8(contextId, &exists);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndCheckSameObjectExist(session,
					 requestId, mtpRes, &exists, sizeof(exists));
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndCheckSameObjectExist failed: 0x%x\n",
				"SERVICE", "endCheckSameObjectExist", 819, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BBC7A */
static int endGetTotalObjectSize(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	SceUInt32 objectCount = 0;
	SceUInt64 totalBytes = 0;
	int mtpRes = SceLibMtp_4239328A(contextId, &objectCount, &totalBytes);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondAsyncTotalObjectSize(session, requestId, mtpRes,
						  &objectCount, &totalBytes);
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndGetTotalObjectSize failed: 0x%x\n",
				"SERVICE", "endGetTotalObjectSize", 855, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BBD1E */
static int endGetSpecifiedObjectMetadata(IPMI::Session *session, SceUInt32 requestId,
					 SceUInt32 contextId)
{
	int mtpRes = SceLibMtp_D4A132BD(contextId);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndGetSpecifiedMetadata(session,
					requestId, mtpRes);
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndGetSpecifiedObjectMetadata failed: 0x%x\n",
				"SERVICE", "endGetSpecifiedObjectMetadata", 891, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BBD94 */
static int endNpDrmGetRtc(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	int httpStatus = 0;
	SceUInt8 rtcResponse[304];
	sce_paf_memset(rtcResponse, 0, sizeof(rtcResponse));
	int mtpRes = SceLibMtp_2D3DD76D(contextId, rtcResponse, &httpStatus);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondAsyncNpDrmGetRtc(session, requestId, mtpRes,
					      rtcResponse, &httpStatus);
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndNpDrmGetRtc failed: 0x%x\n",
				      "SERVICE", "endNpDrmGetRtc", 928, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BBE4A */
static int endNpDrmActivate(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	int httpStatus = 0;
	int npError = 0;
	SceUInt32 resultFlags = 0;
	SceUInt8 actDatBuf[4240];
	sce_paf_memset(actDatBuf, 0, sizeof(actDatBuf));
	int mtpRes = SceLibMtp_48DE8672(contextId, actDatBuf, &httpStatus, &npError, &resultFlags);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondAsyncNpDrmActivate(session, requestId, mtpRes,
						actDatBuf, &httpStatus, &npError, &resultFlags);
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndNpDrmActivate failed: 0x%x\n",
				      "SERVICE", "endNpDrmActivate", 967, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BBF26 */
static int endNpDrmDeactivate(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	int httpStatus = 0;
	int npError = 0;
	SceUInt32 resultFlags = 0;
	int mtpRes = SceLibMtp_923FAB90(contextId, &httpStatus, &npError, &resultFlags);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondAsyncNpDrmDeactivate(session, requestId, mtpRes,
						  &httpStatus, &npError, &resultFlags);
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndNpDrmDeactivate failed: 0x%x\n",
				      "SERVICE", "endNpDrmDeactivate", 1005, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BBFD8 */
static int endNpDrmGetLicense(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	int httpStatus = 0;
	SceUInt8 licenseBuf[516];
	sce_paf_memset(licenseBuf, 0, sizeof(licenseBuf));
	int mtpRes = SceLibMtp_D9EEE248(contextId, licenseBuf, &httpStatus);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondAsyncNpDrmGetLicense(session, requestId, mtpRes,
						  licenseBuf, &httpStatus);
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndNpDrmGetLicense failed: 0x%x\n",
				      "SERVICE", "endNpDrmGetLicense", 1043, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BC0A6 */
static int endMtpRpcNetOperation(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	SceSize responseSize = 0;
	void *responseBuf = sce_paf_malloc(0x400u);
	if (responseBuf == NULL)
		return MTP_SVC_ERROR_NO_MEMORY;

	int mtpRes = SceLibMtp_6E2FE43A(contextId, responseBuf, &responseSize);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndRpcNetOperation(session,
					   requestId, mtpRes, responseBuf, &responseSize);
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndMtpRpcNetOperation failed: 0x%x\n",
				"SERVICE", "endMtpRpcNetOperation", 1665, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	sce_paf_free(responseBuf);
	return ret;
}

/* 0x812BC170 */
static int endMtpRpcNetOperationSend(IPMI::Session *session, SceUInt32 requestId,
				     SceUInt32 contextId)
{
	SceSize responseSize = 0;
	void *responseBuf = sce_paf_malloc(0x400u);
	if (responseBuf == NULL)
		return MTP_SVC_ERROR_NO_MEMORY;

	int mtpRes = SceLibMtp_D919C89C(contextId, responseBuf, &responseSize);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndRpcNetOperationSend(session,
					   requestId, mtpRes, responseBuf, &responseSize);
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndMtpRpcNetOperationSend failed: 0x%x\n",
				"SERVICE", "endMtpRpcNetOperationSend", 1719, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	sce_paf_free(responseBuf);
	return ret;
}

/* 0x812BC23A */
static int endMtpRpcNetOperationRecv(IPMI::Session *session, SceUInt32 requestId,
				     SceUInt32 contextId)
{
	int mtpRes = SceLibMtp_0311F66F(contextId);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndRpcNetOperationRecv(session,
					requestId, mtpRes);
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndMtpRpcNetOperationRecv failed: 0x%x\n",
				"SERVICE", "endMtpRpcNetOperationRecv", 1762, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BC2B0 */
static int endSearchObject(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	SceUInt32 numFound = 0;
	SceUInt32 searchId = 0xFFFFFFFFu;
	int mtpRes = SceLibMtp_BBCC3F5B(contextId, &numFound, &searchId);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondAsyncSearchObject(session, requestId, mtpRes, &numFound, &searchId);
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndSearchObject failed: 0x%x\n",
				      "SERVICE", "endSearchObject", 1803, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BC35E */
static int endHandover(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	int mtpRes = SceLibMtp_462A6667(contextId);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndHandover(session, requestId, mtpRes);
		if (ret < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndHandover failed: 0x%x\n",
				      "SERVICE", "endHandover", 1914, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BC3D4 */
static int endResumeExportObject(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	int mtpRes = SceLibMtp_33B08344(contextId);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndResumeExportObject(session,
					requestId, mtpRes);
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndResumeExportObject failed: 0x%x\n",
				"SERVICE", "endResumeExportObject", 1975, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* 0x812BC44A */
static int endResumeImportObject(IPMI::Session *session, SceUInt32 requestId, SceUInt32 contextId)
{
	SceUInt64 objectId = 0;
	int mtpRes = SceLibMtp_83242E03(contextId, &objectId);
	g_mtpServer.sessionMgr().deleteContext(session, requestId);
	int ret = g_mtpServer.sessionMgr().lockSession(session);
	if (ret >= 0) {
		ret = respondEndResumeImportObject(session,
					 requestId, mtpRes, &objectId, sizeof(objectId));
		if (ret < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndResumeImportObject failed: 0x%x\n",
				"SERVICE", "endResumeImportObject", 2020, ret);
		g_mtpServer.sessionMgr().unlockSession();
	}
	return ret;
}

/* ---------------------------------------------------------------------------
 * Async Request Handlers (0x812B834C - 0x812BAF59)
 * --------------------------------------------------------------------------- */

/* 0x812B834C */
static int beginGetNumOfObject(IPMI::Session *session, SceUInt32 requestId,
			       int storageId, int formatCode)
{
	SceUInt32 contextId = 0;
	SceUInt32 zeroCount = 0;
	int ret = sceMtpBeginGetNumOfObject(storageId, formatCode, &contextId);
	if (ret != 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): sceMtpBeginGetNumOfObject failed: 0x%x\n",
			      "SERVICE", "BeginGetNumOfObject", 179, ret);
		int respRet = respondEndGetNumOfObject(session,
						 requestId, ret, &zeroCount, sizeof(zeroCount));
		if (respRet < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndGetNumOfObject failed: 0x%x\n",
				      "SERVICE", "BeginGetNumOfObject", 183, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endGetNumOfObject);
}

/* 0x812B8440 */
static int beginGetObjectMetadata(IPMI::Session *session, SceUInt32 requestId,
				  int storageId, int formatCode, int offset, int count)
{
	SceUInt32 contextId = 0;
	int ret = sceMtpBeginGetObjectMetadata(storageId, formatCode, offset, count, &contextId);
	if (ret != 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): sceMtpBeginGetObjectMetadata failed: 0x%x\n",
			"SERVICE", "BeginGetObjectMetadata", 214, ret);
		int respRet = respondEndGetObjectMetadata(session,
						requestId, ret);
		if (respRet < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndGetObjectMetadata failed: 0x%x\n",
				"SERVICE", "BeginGetObjectMetadata", 218, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endGetObjectMetadata);
}

/* 0x812B8538 */
static int beginImportObject(IPMI::Session *session, SceUInt32 requestId,
			     int category, const void *metadata, SceSize metadataSize,
			     const void *extra, SceSize extraSize)
{
	SceUInt32 contextId = 0;
	SceUInt64 zeroObjId = 0;
	int ret = sceMtpBeginImportObject(category, metadata, metadataSize,
					  &contextId, extra, extraSize);
	if (ret != 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): sceMtpBeginImportObject failed: 0x%x\n",
			      "SERVICE", "BeginImportObject", 254, ret);
		int respRet = respondEndImportObject(session,
						 requestId, ret, &zeroObjId, sizeof(zeroObjId));
		if (respRet < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndImportObject failed: 0x%x\n",
				      "SERVICE", "BeginImportObject", 258, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endImportObject);
}

/* 0x812B8652 */
static int beginExportObjectWithCheck(IPMI::Session *session, SceUInt32 requestId,
				      SceUInt32 idLow, SceUInt32 idHigh,
				      SceUInt32 flagsLow, SceUInt32 flagsHigh, int checkFlag)
{
	SceUInt32 contextId = 0;
	int ret = sceMtpBeginExportObjectWithCheck(idLow, idHigh, flagsLow, flagsHigh,
						   &contextId, checkFlag);
	if (ret != 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): sceMtpBeginExportObjectWithCheck failed: 0x%x\n",
			"SERVICE", "BeginExportObjectWithCheck", 289, ret);
		int respRet = respondEndExportObject(session,
						requestId, ret);
		if (respRet < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndExportObject failed: 0x%x\n",
				      "SERVICE", "BeginExportObjectWithCheck", 293, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endExportObject);
}

/* 0x812B875A */
static int beginCreateHostDir(IPMI::Session *session, SceUInt32 requestId,
			      const void *dirInfo, int parentId, int flags)
{
	SceUInt32 contextId = 0;
	SceUInt32 zeroHandle = 0;
	int ret = sceMtpBeginCreateHostDir(dirInfo, parentId, flags, &contextId);
	if (ret != 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): sceMtpBeginCreateHostDir failed: 0x%x\n",
			      "SERVICE", "BeginCreateHostDir", 324, ret);
		int respRet = respondEndCreateHostDir(session,
						 requestId, ret, &zeroHandle, sizeof(zeroHandle));
		if (respRet < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndCreateHostDir failed: 0x%x\n",
				      "SERVICE", "BeginCreateHostDir", 328, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endCreateHostDir);
}

/* 0x812B8854 */
static int beginRenameHostDir(IPMI::Session *session, SceUInt32 requestId,
			      const void *dirInfo, int dirHandle, int flags)
{
	SceUInt32 contextId = 0;
	int ret = sceMtpBeginDeleteHostDir(dirInfo, dirHandle, flags, &contextId);
	if (ret != 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): sceMtpBeginDeleteHostDir failed: 0x%x\n",
			      "SERVICE", "BeginRenameHostDir", 357, ret);
		int respRet = respondEndRenameHostDir(session,
						requestId, ret);
		if (respRet < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndRenameHostDir failed: 0x%x\n",
				      "SERVICE", "BeginRenameHostDir", 361, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endRenameHostDir);
}

/* 0x812B894A */
static int beginMoveHostDir(IPMI::Session *session, SceUInt32 requestId,
			    int dirHandle, int destHandle)
{
	SceUInt32 contextId = 0;
	int ret = sceMtpBeginMoveHostDir(dirHandle, destHandle, &contextId);
	if (ret != 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): sceMtpBeginMoveHostDir failed: 0x%x\n",
			      "SERVICE", "BeginMoveHostDir", 390, ret);
		int respRet = respondEndMoveHostDir(session,
						requestId, ret);
		if (respRet < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndMoveHostDir failed: 0x%x\n",
				      "SERVICE", "BeginMoveHostDir", 394, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endMoveHostDir);
}

/* 0x812B8A3C */
static int beginCreateHostFile(IPMI::Session *session, SceUInt32 requestId,
			       const void *fileInfo, int parentId, int flags)
{
	SceUInt32 contextId = 0;
	SceUInt32 zeroHandle = 0;
	int ret = sceMtpBeginCreateHostFile(fileInfo, parentId, flags, &contextId);
	if (ret != 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): sceMtpBeginCreateHostFile failed: 0x%x\n",
			      "SERVICE", "BeginCreateHostFile", 425, ret);
		int respRet = respondEndCreateHostFile(session,
						 requestId, ret, &zeroHandle, sizeof(zeroHandle));
		if (respRet < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndCreateHostFile failed: 0x%x\n",
				      "SERVICE", "BeginCreateHostFile", 429, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endCreateHostFile);
}

/* 0x812B8B36 */
static int beginGetObjectThumbnail(IPMI::Session *session, SceUInt32 requestId,
				   int objectHandle, int flags)
{
	SceUInt32 contextId = 0;
	int ret = sceMtpBeginGetObjectThumbnail(objectHandle, flags, &contextId);
	if (ret != 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): sceMtpBeginGetObjectThumbnail failed: 0x%x\n",
			"SERVICE", "BeginGetObjectThumbnail", 460, ret);
		int respRet = respondEndGetObjectThumbnail(session,
						requestId, ret);
		if (respRet < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndGetObjectThumbnail failed: 0x%x\n",
				"SERVICE", "BeginGetObjectThumbnail", 464, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endGetObjectThumbnail);
}

/* 0x812B8C28 */
static int beginGetObjectStatus2(IPMI::Session *session, SceUInt32 requestId,
				 int objectHandle, const void *query, int querySize)
{
	SceUInt32 contextId = 0;
	int ret = sceMtpBeginGetObjectStatus2(objectHandle, query, querySize, &contextId);
	if (ret != 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): sceMtpBeginGetObjectStatus2 failed: 0x%x\n",
			      "SERVICE", "BeginGetObjectStatus2", 493, ret);
		int respRet = respondEndGetObjectStatus(session,
						requestId, ret);
		if (respRet < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndGetObjectStatus failed: 0x%x\n",
				      "SERVICE", "BeginGetObjectStatus2", 497, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endGetObjectStatus);
}

/* 0x812B8D1E */
static int beginDeleteObject(IPMI::Session *session, SceUInt32 requestId, int objectHandle)
{
	SceUInt32 contextId = 0;
	int ret = sceMtpBeginDeleteObject(objectHandle, &contextId);
	if (ret != 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): sceMtpBeginDeleteObject failed: 0x%x\n",
			      "SERVICE", "BeginDeleteObject", 526, ret);
		int respRet = respondEndDeleteObject(session,
						requestId, ret);
		if (respRet < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndDeleteObject failed: 0x%x\n",
				      "SERVICE", "BeginDeleteObject", 530, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endDeleteObject);
}

/* 0x812B8E0E */
static int beginHttpGetDataWithUrl(IPMI::Session *session, SceUInt32 requestId,
				   const char *url, int urlFlags, SceUInt64 offset)
{
	SceUInt32 contextId = 0;
	int ret = sceMtpBeginHttpGetDataWithUrl(url, urlFlags, offset, &contextId);
	if (ret != 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): sceMtpBeginHttpGetDataWithUrl failed: 0x%x\n",
			"SERVICE", "BeginHttpGetDataWithUrl", 559, ret);
		int respRet = respondEndHttpGetDataWithUrl(session,
						requestId, ret);
		if (respRet < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndHttpGetDataWithUrl failed: 0x%x\n",
				"SERVICE", "BeginHttpGetDataWithUrl", 563, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endHttpGetDataWithUrl);
}

/* 0x812B8F0C */
static int beginHttpGetPropertyWithUrl(IPMI::Session *session, SceUInt32 requestId,
				       const char *url, int urlFlags)
{
	SceUInt32 contextId = 0;
	SceUInt8 contentLength[8];
	SceUInt8 propertyHeader[16];
	int ret = sceMtpBeginHttpGetPropertyWithUrl(url, urlFlags, &contextId);
	if (ret != 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): sceMtpBeginHttpGetPropertyWithUrl failed: 0x%x\n",
			"SERVICE", "BeginHttpGetPropertyWithUrl", 596, ret);
		int respRet = respondAsyncHttpGetProperty(session, requestId, ret,
							  contentLength, propertyHeader);
		if (respRet < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndHttpGetPropertyWithUrl failed: 0x%x\n",
				"SERVICE", "BeginHttpGetPropertyWithUrl", 600, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endHttpGetPropertyWithUrl);
}

/* 0x812B9004 */
static int beginGetSystemSetting(IPMI::Session *session, SceUInt32 requestId, int settingId)
{
	SceUInt32 contextId = 0;
	int ret = sceMtpBeginSetSystemSetting(settingId, &contextId);
	if (ret != 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): sceMtpBeginGetSystemSetting failed: 0x%x\n",
			"SERVICE", "BeginGetSystemSetting", 629, ret);
		int respRet = respondEndGetSystemSetting(session,
						requestId, ret);
		if (respRet < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndGetSystemSetting failed: 0x%x\n",
				"SERVICE", "BeginGetSystemSetting", 633, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endGetSystemSetting);
}

/* 0x812B90F4 */
static int beginSetSystemSetting(IPMI::Session *session, SceUInt32 requestId,
				 int settingId, int settingValue)
{
	SceUInt32 contextId = 0;
	int ret = SceLibMtp_CFA3A9DA(settingId, settingValue, &contextId);
	if (ret != 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): sceMtpBeginSetSystemSetting failed: 0x%x\n",
			"SERVICE", "BeginSetSystemSetting", 662, ret);
		int respRet = respondEndSetSystemSetting(session,
						requestId, ret);
		if (respRet < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndSetSystemSetting failed: 0x%x\n",
				"SERVICE", "BeginSetSystemSetting", 666, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endSetSystemSetting);
}

/* 0x812B91E6 */
static int beginGetNpAccountInfo(IPMI::Session *session, SceUInt32 requestId, int accountFlags)
{
	SceUInt32 contextId = 0;
	SceUInt32 zeroResult = 0;
	int ret = sceMtpBeginGetNpAccountInfo(accountFlags, &contextId);
	if (ret != 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): sceMtpBeginGetNpAccountInfo failed: 0x%x\n",
			"SERVICE", "BeginGetNpAccountInfo", 696, ret);
		int respRet = respondEndGetNpAccountInfo(session,
						 requestId, ret, &zeroResult, sizeof(zeroResult));
		if (respRet < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndGetNpAccountInfo failed: 0x%x\n",
				"SERVICE", "BeginGetNpAccountInfo", 701, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endGetNpAccountInfo);
}

/* 0x812B92DC */
static int beginReadObjectWithOffset(IPMI::Session *session, SceUInt32 requestId,
				     int objectHandle, SceUInt64 offset, SceUInt64 readSize)
{
	SceUInt32 contextId = 0;
	int ret = sceMtpBeginReadObjectWithOffset(objectHandle, 0, offset, readSize, &contextId);
	if (ret != 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): sceMtpBeginReadObjectWithOffset failed: 0x%x\n",
			"SERVICE", "BeginReadObjectWithOffset", 730, ret);
		int respRet = respondEndReadObjectWithOffset(session,
						requestId, ret);
		if (respRet < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndReadObjectWithOffset failed: 0x%x\n",
				"SERVICE", "BeginReadObjectWithOffset", 734, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endReadObjectWithOffset);
}

/* 0x812B93E2 */
static int beginWriteObjectWithOffset(IPMI::Session *session, SceUInt32 requestId,
				      int objectHandle, SceUInt64 offset, SceUInt64 writeSize)
{
	SceUInt32 contextId = 0;
	int ret = sceMtpBeginWriteObjectWithOffset(objectHandle, 0, offset, writeSize, &contextId);
	if (ret != 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): sceMtpBeginWriteObjectWithOffset failed: 0x%x\n",
			"SERVICE", "BeginWriteObjectWithOffset", 763, ret);
		int respRet = respondEndWriteObjectWithOffset(session,
						requestId, ret);
		if (respRet < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndWriteObjectWithOffset failed: 0x%x\n",
				"SERVICE", "BeginWriteObjectWithOffset", 767, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endWriteObjectWithOffset);
}

/* 0x812B94E8 */
static int beginGetHostStorageSize(IPMI::Session *session, SceUInt32 requestId, int storageId)
{
	SceUInt32 contextId = 0;
	SceUInt8 zeroStorageInfo[28];
	int ret = sceMtpBeginGetHostStorageSize(storageId, &contextId);
	if (ret != 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): sceMtpBeginGetHostStorageSize failed: 0x%x\n",
			"SERVICE", "BeginGetHostStorageSize", 797, ret);
		sce_paf_memset(zeroStorageInfo, 0, sizeof(zeroStorageInfo));
		int respRet = respondEndGetHostStorageSize(session,
						 requestId, ret, zeroStorageInfo,
						 sizeof(zeroStorageInfo));
		if (respRet < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndGetHostStorageSize failed: 0x%x\n",
				"SERVICE", "BeginGetHostStorageSize", 803, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endGetHostStorageSize);
}

/* 0x812B95E6 */
static int beginCheckSameObjectExist(IPMI::Session *session, SceUInt32 requestId,
				     SceUInt32 idLow, SceUInt32 idHigh)
{
	SceUInt32 contextId = 0;
	SceUInt32 zeroExists = 0;
	int ret = sceMtpBeginCheckSameObjectExist(idLow, idHigh, &contextId);
	if (ret != 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): sceMtpBeginCheckSameObjectExist failed: 0x%x\n",
			"SERVICE", "BeginCheckSameObjectExist", 833, ret);
		int respRet = respondEndCheckSameObjectExist(session,
						 requestId, ret, &zeroExists, sizeof(zeroExists));
		if (respRet < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndCheckSameObjectExist failed: 0x%x\n",
				"SERVICE", "BeginCheckSameObjectExist", 838, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endCheckSameObjectExist);
}

/* 0x812B96DE */
static int beginGetTotalObjectSize(IPMI::Session *session, SceUInt32 requestId, int category)
{
	SceUInt32 contextId = 0;
	SceUInt32 zeroCount = 0;
	SceUInt64 zeroSize = 0;
	int ret = sceMtpBeginGetTotalObjectSize(category, &contextId);
	if (ret != 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): sceMtpBeginGetTotalObjectSize failed: 0x%x\n",
			"SERVICE", "BeginGetTotalObjectSize", 869, ret);
		int respRet = respondAsyncTotalObjectSize(session, requestId, ret,
							  &zeroCount, &zeroSize);
		if (respRet < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndGetTotalObjectSize failed: 0x%x\n",
				"SERVICE", "BeginGetTotalObjectSize", 875, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endGetTotalObjectSize);
}

/* 0x812B97D4 */
static int beginGetSpecifiedObjectMetadata(IPMI::Session *session, SceUInt32 requestId,
					   int category, const void *handles, int count)
{
	SceUInt32 contextId = 0;
	int ret = sceMtpBeginSpecifiedObjectMetadata(category, handles, count, &contextId);
	if (ret != 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): sceMtpBeginSpecifiedObjectMetadata failed: 0x%x\n",
			"SERVICE", "BeginGetSpecifiedObjectMetadata", 906, ret);
		int respRet = respondEndGetSpecifiedMetadata(session,
						requestId, ret);
		if (respRet < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndGetSpecifiedObjectMetadata failed: 0x%x\n",
				"SERVICE", "BeginGetSpecifiedObjectMetadata", 910, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endGetSpecifiedObjectMetadata);
}

/* 0x812B98CA */
static int beginNpDrmGetRtc(IPMI::Session *session, SceUInt32 requestId,
			    const void *request, SceSize size)
{
	SceUInt32 contextId = 0;
	int httpStatus = 0;
	SceUInt8 rtcResponse[304];
	int ret = sceMtpBeginNpDrmGetRtc(request, size, &contextId);
	if (ret != 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): sceMtpBeginNpDrmGetRtc failed: 0x%x\n",
			      "SERVICE", "BeginNpDrmGetRtc", 942, ret);
		int respRet = respondAsyncNpDrmGetRtc(session, requestId, ret,
						      rtcResponse, &httpStatus);
		if (respRet < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndNpDrmGetRtc failed: 0x%x\n",
				      "SERVICE", "BeginNpDrmGetRtc", 948, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endNpDrmGetRtc);
}

/* 0x812B99C2 */
static int beginNpDrmActivate(IPMI::Session *session, SceUInt32 requestId,
			      SceUInt32 flags, const void *request, SceSize size)
{
	SceUInt32 contextId = 0;
	int httpStatus = 0;
	SceUInt32 resultFlags = 0;
	SceUInt8 actDatBuf[4240];
	int ret = sceMtpBeginNpDrmActivate(flags, request, size, &contextId);
	if (ret != 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): sceMtpBeginNpDrmActivate failed: 0x%x\n",
			      "SERVICE", "BeginNpDrmActivate", 981, ret);
		int respRet = respondAsyncNpDrmActivate(session, requestId, ret,
							actDatBuf, &httpStatus, &httpStatus,
							&resultFlags);
		if (respRet < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndNpDrmActivate failed: 0x%x\n",
				      "SERVICE", "BeginNpDrmActivate", 988, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endNpDrmActivate);
}

/* 0x812B9ADE */
static int beginNpDrmDeactivate(IPMI::Session *session, SceUInt32 requestId,
				SceUInt32 flags, const void *request, SceSize size)
{
	SceUInt32 contextId = 0;
	int httpStatus = 0;
	SceUInt32 resultFlags = 0;
	int ret = sceMtpBeginNpDrmDeactivate(flags, request, size, &contextId);
	if (ret != 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): sceMtpBeginNpDrmDeactivate failed: 0x%x\n",
			      "SERVICE", "BeginNpDrmDeactivate", 1019, ret);
		int respRet = respondAsyncNpDrmDeactivate(session, requestId, ret,
							  &httpStatus, &httpStatus, &resultFlags);
		if (respRet < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndNpDrmDeactivate failed: 0x%x\n",
				      "SERVICE", "BeginNpDrmDeactivate", 1025, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endNpDrmDeactivate);
}

/* 0x812B9BDC */
static int beginNpDrmGetLicense(IPMI::Session *session, SceUInt32 requestId,
				const void *rtc, const void *licenseId, SceSize size)
{
	SceUInt32 contextId = 0;
	int httpStatus = 0;
	SceUInt8 licenseBuf[516];
	int ret = sceMtpBeginNpDrmGetLicense(rtc, licenseId, size, &contextId);
	if (ret != 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): sceMtpBeginNpDrmGetLicense failed: 0x%x\n",
			      "SERVICE", "BeginNpDrmGetLicense", 1057, ret);
		int respRet = respondAsyncNpDrmGetLicense(session, requestId, ret,
							  licenseBuf, &httpStatus);
		if (respRet < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndNpDrmGetLicense failed: 0x%x\n",
				      "SERVICE", "BeginNpDrmGetLicense", 1063, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endNpDrmGetLicense);
}

/* 0x812B9CDC */
static int beginRecvData(IPMI::Session *session, SceUInt32 requestId,
			 SceUInt32 ticket, SceSize size)
{
	SceUInt32 contextId = 0;
	int ret = g_mtpServer.sessionMgr().getContext(session, ticket, &contextId);
	if (ret == 0) {
		ret = g_mtpServer.worker().setRecvJob(session, requestId, contextId, size);
		if (ret == 0)
			return 0;
		sceClibPrintf("[MTP %s] Error %s(%u): SetRecvJob failed: 0x%x\n",
			      "SERVICE", "BeginRecvData", 1076, ret);
	}

	SceUInt8 emptyBuf[4];
	SceSize zeroReceived = 0;
	SceUInt64 zeroTotal = 0;
	int respRet = mtpServiceRespondRecvData(session, requestId, ret,
						emptyBuf, &zeroReceived, &zeroTotal);
	if (respRet < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): EndRecvData failed: 0x%x\n",
			      "SERVICE", "BeginRecvData", 1085, respRet);
	return respRet;
}

/* 0x812B9E16 */
static int beginSendData(IPMI::Session *session, SceUInt32 requestId,
			 SceUInt32 ticket, const void *srcData, SceSize size)
{
	SceUInt32 contextId = 0;
	int ret = g_mtpServer.sessionMgr().getContext(session, ticket, &contextId);
	if (ret == 0) {
		ret = g_mtpServer.worker().setSendJob(session, requestId, contextId, srcData, size);
		if (ret == 0)
			return 0;
		sceClibPrintf("[MTP %s] Error %s(%u): SetSendJob failed: 0x%x\n",
			      "SERVICE", "BeginSendData", 1109, ret);
	}

	SceSize zeroSent = 0;
	SceUInt64 zeroTotal = 0;
	int respRet = mtpServiceRespondSendData(session, requestId, ret, &zeroSent, &zeroTotal);
	if (respRet < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): EndSendData failed: 0x%x\n",
			      "SERVICE", "BeginSendData", 1117, respRet);
	return respRet;
}

/* 0x812BA6E6 */
static int beginMtpRpcNetOperation(IPMI::Session *session, SceUInt32 requestId,
				   const void *request, SceSize size)
{
	SceUInt32 contextId = 0;
	SceSize zeroSize = 0;
	int ret = SceLibMtp_D6C90706(request, size, &contextId);
	if (ret != 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): sceMtpBeginRpcNetOperation failed: 0x%x\n",
			      "SERVICE", "BeginMtpRpcNetOperation", 1688, ret);
		int respRet = respondEndRpcNetOperation(session,
						   requestId, ret, NULL, &zeroSize);
		if (respRet < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndMtpRpcNetOperation failed: 0x%x\n",
				"SERVICE", "BeginMtpRpcNetOperation", 1692, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endMtpRpcNetOperation);
}

/* 0x812BA7E2 */
static int beginMtpRpcNetOperationSend(IPMI::Session *session, SceUInt32 requestId, SceSize size)
{
	SceUInt32 contextId = 0;
	SceSize zeroSize = 0;
	int ret = sceMtpBeginRpcNetOperationSend(size, &contextId);
	if (ret != 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): sceMtpBeginRpcNetOperationSend failed: 0x%x\n",
			"SERVICE", "BeginMtpRpcNetOperationSend", 1742, ret);
		int respRet = respondEndRpcNetOperationSend(session,
						   requestId, ret, NULL, &zeroSize);
		if (respRet < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndMtpRpcNetOperationSend failed: 0x%x\n",
				"SERVICE", "BeginMtpRpcNetOperationSend", 1746, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endMtpRpcNetOperationSend);
}

/* 0x812BA8DA */
static int beginMtpRpcNetOperationRecv(IPMI::Session *session, SceUInt32 requestId,
				       const void *request, SceSize size)
{
	SceUInt32 contextId = 0;
	int ret = sceMtpBeginRpcNetOperationRecv(request, size, &contextId);
	if (ret != 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): sceMtpBeginRpcNetOperationRecv failed: 0x%x\n",
			"SERVICE", "BeginMtpRpcNetOperationRecv", 1777, ret);
		int respRet = respondEndRpcNetOperationRecv(session,
						requestId, ret);
		if (respRet < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndMtpRpcNetOperationRecv failed: 0x%x\n",
				"SERVICE", "BeginMtpRpcNetOperationRecv", 1781, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endMtpRpcNetOperationRecv);
}

/* 0x812BA9DE */
static int beginSearchObject(IPMI::Session *session, SceUInt32 requestId,
			     int category, const void *query, int querySize)
{
	SceUInt32 contextId = 0;
	SceUInt32 zeroFound = 0;
	SceUInt32 invalidSearchId = 0xFFFFFFFFu;
	int ret = sceMtpBeginSearchObject(category, query, querySize, &contextId);
	if (ret != 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): sceMtpBeginSearchObject failed: 0x%x\n",
			      "SERVICE", "BeginSearchObject", 1819, ret);
		int respRet = respondAsyncSearchObject(session, requestId, ret,
						       &zeroFound, &invalidSearchId);
		if (respRet < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndSearchObject failed: 0x%x\n",
				      "SERVICE", "BeginSearchObject", 1823, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endSearchObject);
}

/* 0x812BAAE4 */
static int beginConnectToHost(IPMI::Session *session, SceUInt32 requestId,
			      const MtpNetHost *targetHost, int timeoutSeconds)
{
	int err = 0;
	int started = 0;
	int ret = SceLibMtp_B7984D4C(&started);
	if (ret < 0) {
		err = ret;
	} else {
		SceLibMtp_768EBC03(timeoutSeconds);
		bool alreadySameHost = false;
		if (started != 0 && SceLibMtp_4C1B7DBB() == MTP_IF_TYPE_TCPIP) {
			MtpNetHost currentHost;
			if (SceLibMtp_D63692E8(&currentHost) >= 0 &&
			    sce_paf_memcmp(targetHost->id, currentHost.id, sizeof(targetHost->id)) == 0) {
				SceLibMtp_965F1CD0();
				alreadySameHost = true;
			}
		}
		if (!alreadySameHost) {
			if (started != 0) {
				int stopRet = sceMtpStop();
				if (stopRet < 0)
					err = stopRet;
			}
			if (err == 0) {
				int startRet = sceMtpStart(MTP_IF_TYPE_TCPIP, targetHost, 0, 1);
				if (startRet < 0)
					err = startRet;
			}
		}
	}

	int respRet = respondEndConnectToHost(session, requestId, err);
	if (respRet < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): EndConnectToHost failed: 0x%x\n",
			      "SERVICE", "BeginConnectToHost", 1892, respRet);
	return respRet;
}

/* 0x812BABAE */
static int beginHandover(IPMI::Session *session, SceUInt32 requestId, int handoverParam)
{
	SceUInt32 contextId = 0;
	int ret = sceMtpBeginHandover(handoverParam, &contextId);
	if (ret != 0) {
		sceClibPrintf("[MTP %s] Error %s(%u): sceMtpBeginHandover failed: 0x%x\n",
			      "SERVICE", "BeginHandover", 1929, ret);
		int respRet = respondEndHandover(session, requestId, ret);
		if (respRet < 0)
			sceClibPrintf("[MTP %s] Error %s(%u): EndHandover failed: 0x%x\n",
				      "SERVICE", "BeginHandover", 1933, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endHandover);
}

/* 0x812BAD32 */
static int beginResumeExportObject(IPMI::Session *session, SceUInt32 requestId)
{
	SceUInt32 contextId = 0;
	int ret = sceMtpBeginResumeExportObject(&contextId);
	if (ret != 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): sceMtpBeginResumeExportObject failed: 0x%x\n",
			"SERVICE", "BeginResumeExportObject", 1989, ret);
		int respRet = respondEndResumeExportObject(session,
						requestId, ret);
		if (respRet < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndResumeExportObject failed: 0x%x\n",
				"SERVICE", "BeginResumeExportObject", 1993, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endResumeExportObject);
}

/* 0x812BAE32 */
static int beginResumeImportObject(IPMI::Session *session, SceUInt32 requestId,
				   int category, SceUInt64 offset, SceUInt64 resumeSize,
				   const void *extra)
{
	SceUInt32 contextId = 0;
	SceUInt64 zeroObjId = 0;
	int ret = sceMtpBeginResumeImportObject(category, 0, offset, resumeSize,
						extra, &contextId);
	if (ret != 0) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): sceMtpBeginResumeImportObject failed: 0x%x\n",
			"SERVICE", "BeginResumeImportObject", 2035, ret);
		int respRet = respondEndResumeImportObject(session,
						 requestId, ret, &zeroObjId, sizeof(zeroObjId));
		if (respRet < 0)
			sceClibPrintf(
				"[MTP %s] Error %s(%u): EndResumeImportObject failed: 0x%x\n",
				"SERVICE", "BeginResumeImportObject", 2039, respRet);
		return respRet;
	}
	return scheduleAsyncContext(session, requestId, contextId, endResumeImportObject);
}

/* ---------------------------------------------------------------------------
 * Sync Method Implementations (0x812B831E - 0x812BB1BF)
 * --------------------------------------------------------------------------- */

/* 0x812B9FE4 */
static int syncGetHostStatus(IPMI::Session *session, MtpStatus *outStatus)
{
	int ret = SceLibMtp_F82B75F5(outStatus);
	if (ret >= 0 && SceLibMtp_4C1B7DBB() == MTP_IF_TYPE_TCPIP) {
		SceUInt8 wasReset = 0;
		int resetRet = g_mtpServer.sessionMgr().checkHostReset(session, &wasReset);
		if (resetRet < 0)
			return resetRet;
		if (wasReset != 0) {
			SceAppMgrAppStatus appStatus;
			int appRet = sceAppMgrGetStatusByAppId(-4, &appStatus);
			bool isCmaFront = false;
			if (appRet >= 0) {
				isCmaFront = (sce_paf_strcmp(appStatus.appName, "NPXS10026") == 0);
			} else if (appRet != MTP_APPMGR_ERROR_NO_ACTIVE_APP) {
				sceClibPrintf(
					"[MTP %s] Error %s(%u): sceAppMgrGetStatusByAppId failed: 0x%x\n",
					"SERVICE", "isCurrentActiveApp", 1191, appRet);
			}
			if (!isCmaFront) {
				outStatus->state = 0;
				outStatus->reason = 8;
			}
		}
	}
	return ret;
}

/* 0x812BA0A6 */
static int syncCancelExecution(SceUInt32 ticket, SceUInt32 sessionIndex)
{
	IPMI::Session *targetSession = NULL;
	int ret = g_mtpServer.sessionMgr().getSessionByIndex(sessionIndex, &targetSession);
	if (ret < 0)
		return ret;

	SceUInt32 contextId = 0;
	if (g_mtpServer.sessionMgr().getContext(targetSession, ticket, &contextId) < 0)
		contextId = 0;

	paf::common::SharedPtr<paf::job::JobItem> job(new MtpCancelExecutionJob(contextId));
	g_mtpServer.enqueueCancelJob(job);
	return 0;
}

/* 0x812BA298 */
static int syncSendSessionInfo(IPMI::Session *session)
{
	SceUInt32 timeout = MTP_MSG_SEND_TIMEOUT_US;
	char *msgBuf = new char[256];
	if (msgBuf == NULL) {
		sceClibPrintf(
			"[MTP %s] Error %s(%u): Creation of BgParam buffer failed due to lack of memory\n",
			"SERVICE", "SendSessionInfo", 1334);
		return MTP_SVC_ERROR_NO_MEMORY;
	}

	SceUInt32 taskId = 0;
	int taskType = 0;
	paf::string taskPath;
	sceMtpGetBgTaskInfo(&taskId, &taskType, &taskPath);

	struct MtpSessionInfoTlv {
		SceUInt32 tag;
		SceUInt32 length;
		SceUInt32 value;
	};
	MtpSessionInfoTlv entries[2];
	entries[0].tag = 18;
	entries[0].length = sizeof(SceUInt32);
	entries[0].value = taskId;
	entries[1].tag = 1;
	entries[1].length = sizeof(SceUInt32);
	entries[1].value = (SceUInt32)taskType;
	sce_paf_memcpy(msgBuf, entries, sizeof(entries));

	int ret = session->sendMsg(1, msgBuf, sizeof(entries), &timeout);
	if (ret != 0)
		sceClibPrintf(
			"[MTP %s] Error %s(%u): Session::sendMsg() for BgCopyImportContext failed [ret: 0x%x]\n",
			"SERVICE", "SendSessionInfo", 1384, ret);
	delete[] msgBuf;
	return ret;
}

/* 0x812BA514 */
static int syncStartInterface(int ifType)
{
	MtpNetHost assignedHost;
	const MtpNetHost *hostPtr = NULL;
	if (ifType == MTP_IF_TYPE_TCPIP && SceLibMtp_ED13170C(&assignedHost) == 0)
		hostPtr = &assignedHost;
	int ret = sceMtpStart(ifType, hostPtr, 0, 1);
	if (ret == 0)
		SceLibMtp_A78FEA82(ifType);
	return ret;
}

/* 0x812BA5D4 */
static int syncGetCapability(int featureId, MtpCapabilityInfo *outCapability)
{
	if (outCapability == NULL)
		return MTP_SVC_ERROR_INVALID_ARGUMENT;

	MtpCapabilityInfo cap;
	cap.supported = 0;
	cap.capabilityStatus = 0;
	cap.timestamp = paf::datetime::DateTime(0, 0, 0, 0, 0, 0, 0);
	cap.timestamp.m_parse_failed = true;
	int ret = SceLibMtp_94040962(featureId, &cap);
	*outCapability = cap;
	return ret;
}

/* 0x812BACA8 */
static int syncPauseExportObject(SceUInt32 ticket, SceUInt32 sessionIndex)
{
	IPMI::Session *targetSession = NULL;
	int ret = g_mtpServer.sessionMgr().getSessionByIndex(sessionIndex, &targetSession);
	if (ret < 0)
		return ret;

	SceUInt32 contextId = 0;
	ret = g_mtpServer.sessionMgr().getContext(targetSession, ticket, &contextId);
	if (ret < 0)
		return ret;

	ret = sceMtpPauseExportObject(contextId);
	if (ret < 0)
		sceClibPrintf("[MTP %s] Error %s(%u): sceMtpPauseExportObject failed: 0x%x\n",
			      "SERVICE", "PauseExportObject", 1960, ret);
	return ret;
}

/* 0x812BAF5A */
static int syncBgCopyRequestTask(int contentType, SceUInt32 sizeLow, SceUInt32 sizeHigh,
				 const char *taskName, char *outPathBuf)
{
	MtpBgCopyTaskWire task;
	task.contentType = contentType;
	task.reserved = 0;
	task.sizeLow = sizeLow;
	task.sizeHigh = sizeHigh;
	task.status.result = 0;
	if (taskName != NULL)
		task.status.name = taskName;
	task.status.progress = 0;
	task.status.index = 0;
	task.status.total = 0;
	task.status.remaining = 0;

	paf::string pathStr;
	int ret = SceLibMtp_E89AF226(&task, &pathStr);
	sce_paf_strncpy(outPathBuf, pathStr.c_str(), 0x3FFu);
	return ret;
}

/* 0x812BB010 */
static int syncBgCopySetTaskInfo(int result, const char *taskName, SceUInt32 progress,
				 int index, SceUInt32 total, int remaining)
{
	MtpBgCopyStatusWire status;
	status.result = result;
	if (taskName != NULL)
		status.name = taskName;
	status.progress = progress;
	status.index = index;
	status.total = total;
	status.remaining = remaining;
	return SceLibMtp_B15DA25F(&status);
}

/* 0x812BB0BE */
static int syncBgCopyGetTaskInfo(SceUInt32 *outTaskId, char *outPathBuf)
{
	int taskType = 0;
	paf::string pathStr;
	int ret = sceMtpGetBgTaskInfo(outTaskId, &taskType, &pathStr);
	sce_paf_strncpy(outPathBuf, pathStr.c_str(), 0x3FFu);
	return ret;
}

/* 0x812BB118 */
static int syncBgCopyNotifyTask(int taskId, int contentType, SceUInt32 sizeLow, SceUInt32 sizeHigh,
				const char *taskName, int result, SceUInt32 progress,
				int index, SceUInt32 total, int remaining)
{
	MtpBgCopyTaskWire task;
	task.contentType = contentType;
	task.reserved = 0;
	task.sizeLow = sizeLow;
	task.sizeHigh = sizeHigh;
	task.status.result = result;
	if (taskName != NULL)
		task.status.name = taskName;
	task.status.progress = progress;
	task.status.index = index;
	task.status.total = total;
	task.status.remaining = remaining;
	return SceLibMtp_87D21619(taskId, &task);
}

/* ---------------------------------------------------------------------------
 * Dedicated Sync Method Handlers (0x812B831E..0x812BB0B0)
 * --------------------------------------------------------------------------- */

/* 0x812B831E */
static int syncGetSessionId(IPMI::Session *session, SceUInt32 *outIndex)
{
	g_mtpServer.sessionMgr().getSessionIndex(session, outIndex);
	return 0;
}

/* 0x812B8336 */
static int syncDisableNotify(IPMI::Session *session)
{
	g_mtpServer.sessionMgr().disableNotify(session);
	return 0;
}

/* 0x812B9DAC */
static int syncRecvData(IPMI::Session *session, SceUInt32 ticket, void *outBuf,
			SceSize maxLen, SceSize *outReceived, SceUInt64 *outTotal)
{
	*outReceived = 0;
	*outTotal = 0;
	SceUInt32 contextId = 0;
	int res = g_mtpServer.sessionMgr().getContext(session, ticket, &contextId);
	if (res >= 0)
		res = SceLibMtp_CA704934(contextId, outBuf, maxLen, outReceived, outTotal);
	return res;
}

/* 0x812B9EE8 */
static int syncSendData(IPMI::Session *session, SceUInt32 ticket, const void *inBuf,
			SceSize sendLen, SceSize *outSent, SceUInt64 *outTotal)
{
	*outSent = 0;
	*outTotal = 0;
	SceUInt32 contextId = 0;
	int res = g_mtpServer.sessionMgr().getContext(session, ticket, &contextId);
	if (res >= 0)
		res = sceMtpSendData(contextId, inBuf, sendLen, outSent, outTotal);
	return res;
}

/* 0x812B9F52 */
static int syncSetDataPhaseResult(IPMI::Session *session, SceUInt32 ticket, int phaseResult)
{
	SceUInt32 contextId = 0;
	int res = g_mtpServer.sessionMgr().getContext(session, ticket, &contextId);
	if (res >= 0)
		res = SceLibMtp_77793BF2(contextId, phaseResult);
	return res;
}

/* 0x812B9F96 */
static int syncGetContextProgress(IPMI::Session *session, SceUInt32 ticket, void *outProgress)
{
	SceUInt32 contextId = 0;
	int res = g_mtpServer.sessionMgr().getContext(session, ticket, &contextId);
	if (res >= 0) {
		sce_paf_memset(outProgress, 0, 24u);
		res = SceLibMtp_0497FA87(contextId, outProgress);
	}
	return res;
}

/* 0x812BA09C */
static int syncGetHostVersion(void *outVersion)
{
	return SceLibMtp_F6C15FE8(outVersion);
}

/* 0x812BA1C0 */
static int syncCheckHostReset(IPMI::Session *session, SceUInt32 *outWasReset)
{
	SceUInt8 wasReset = 0;
	int res = g_mtpServer.sessionMgr().checkHostReset(session, &wasReset);
	if (res >= 0)
		*outWasReset = wasReset;
	return res;
}

/* 0x812BA206 */
static int syncMarlinActivate(SceUInt32 accountFlag, SceUInt32 *outStatus, SceUInt32 *outErr)
{
	return SceLibMtp_C12DE479(accountFlag, outStatus, outErr);
}

/* 0x812BA214 */
static int syncMarlinDeactivate(SceUInt32 accountFlag, SceUInt32 *outStatus, SceUInt32 *outErr)
{
	return SceLibMtp_24369599(accountFlag, outStatus, outErr);
}

/* 0x812BA222 */
static int syncMarlinLicenseMove(const char *path, SceUInt32 contentId, SceUInt32 licenseFlags,
				 SceUInt32 *outStatus)
{
	return SceLibMtp_A55E6AB6(path, contentId, licenseFlags, outStatus);
}

/* 0x812BA234 */
static int syncMarlinLicenseCopy(const char *path, SceUInt32 contentId, SceUInt32 licenseFlags,
				 SceUInt32 *outStatus)
{
	return SceLibMtp_0BC34F1F(path, contentId, licenseFlags, outStatus);
}

/* 0x812BA246 */
static int syncMarlinLicenseCheck(const char *path, SceUInt32 contentId, SceUInt32 licenseFlags,
				  SceUInt32 *outStatus)
{
	return SceLibMtp_542F3064(path, contentId, licenseFlags, outStatus);
}

/* 0x812BA258 */
static int syncMarlinSharedInfoAcq(const char *path, SceUInt32 requestFlags, SceUInt32 *outStatus)
{
	return SceLibMtp_E35C1C83(path, requestFlags, outStatus);
}

/* 0x812BA266 */
static int syncAddObjectData(const void *objectRecord, SceUInt64 *outObjectId)
{
	return SceLibMtp_81BCF297(objectRecord, outObjectId);
}

/* 0x812BA272 */
static int syncRemoveObjectData(SceUInt32 objectIdLow, SceUInt32 objectIdHigh)
{
	return SceLibMtp_E2A9E59C(objectIdLow, objectIdHigh);
}

/* 0x812BA27E */
static int syncGetObjectData(SceUInt32 objectIdLow, SceUInt32 objectIdHigh, void *outRecord)
{
	return SceLibMtp_9310EEEF(objectIdLow, objectIdHigh, outRecord);
}

/* 0x812BA28E */
static int syncSetBgCopyFlag(int enabled)
{
	return SceLibMtp_6BB1C852(enabled);
}

/* 0x812BA3DC */
static int syncFindObjectByPath(const char *path, SceUInt64 *outObjectId)
{
	return SceLibMtp_E107F90D(path, outObjectId);
}

/* 0x812BA3E8 */
static int syncUpdateObjectData(SceUInt32 objectIdLow, SceUInt32 objectIdHigh,
				const void *objectRecord)
{
	return SceLibMtp_EA4E8C39(objectIdLow, objectIdHigh, objectRecord);
}

/* 0x812BA3F8 */
static int syncResetStorage(void)
{
	g_mtpServer.enqueueStorageResetJob();
	return 0;
}

/* 0x812BA40A */
static int syncStartHostDiscovery(IPMI::Session *session)
{
	int res = SceLibMtp_79F3EE1B();
	if (res == 0) {
		g_mtpServer.sessionMgr().onStartHostDiscovery(session);
		g_mtpServer.sessionMgr().enableNetworkImpose(session);
	}
	return res;
}

/* 0x812BA428 */
static int syncStopHostDiscovery(IPMI::Session *session)
{
	g_mtpServer.sessionMgr().disableNetworkImpose(session);
	g_mtpServer.sessionMgr().onStopHostDiscovery(session);
	return SceLibMtp_71197C62();
}

/* 0x812BA440 */
static int syncGetHostDiscoveryState(int *outState)
{
	return SceLibMtp_06123E14(outState);
}

/* 0x812BA44A */
static int syncGetNumDiscoveredHosts(SceUInt32 *outCount)
{
	return SceLibMtp_06714DA7(outCount);
}

/* 0x812BA454 */
static int syncGetDiscoveredHost(SceUInt32 index, MtpNetHost *outHost)
{
	return SceLibMtp_2CA2CD16(index, outHost);
}

/* 0x812BA460 */
static int syncStartNetRegister(IPMI::Session *session, MtpNetHost *host)
{
	int res = SceLibMtp_6DBD4FF7(host, onNetMtpRegisterCallback);
	if (res == 0)
		g_mtpServer.sessionMgr().onStartRegister(session);
	return res;
}

/* 0x812BA48A */
static int syncSendNetRegisterPin(const char *pin)
{
	return SceLibMtp_66B08C7D(pin);
}

/* 0x812BA494 */
static int syncStopNetRegister(IPMI::Session *session)
{
	g_mtpServer.sessionMgr().onStopRegister(session);
	return SceLibMtp_55B66FA0();
}

/* 0x812BA4AC */
static int syncGetNumRegisteredHosts(SceUInt32 *outCount)
{
	return SceLibMtp_8E76FE4D(outCount);
}

/* 0x812BA4B6 */
static int syncGetRegisteredHost(SceUInt32 index, MtpNetHost *outHost)
{
	return SceLibMtp_0898F989(index, outHost);
}

/* 0x812BA4C2 */
static int syncUnregisterHost(const MtpNetHost *host)
{
	return SceLibMtp_9144D0B4(host);
}

/* 0x812BA4CC */
static int syncIsHostOnline(const MtpNetHost *host, int *outOnline)
{
	return SceLibMtp_56FA46BB(host, outOnline);
}

/* 0x812BA4D8 */
static int syncIsHostRegistered(const MtpNetHost *host, int *outRegistered)
{
	return SceLibMtp_389F2FE2(host, outRegistered);
}

/* 0x812BA4E4 */
static int syncAssignHost(const MtpNetHost *host)
{
	return SceLibMtp_5B4AF4B8(host);
}

/* 0x812BA4EE */
static int syncUnassignHost(void)
{
	return SceLibMtp_27F8701C();
}

/* 0x812BA4F6 */
static int syncIsHostAssigned(int *outAssigned)
{
	return SceLibMtp_8CD95681(outAssigned);
}

/* 0x812BA500 */
static int syncGetAssignedHost(MtpNetHost *outHost)
{
	return SceLibMtp_ED13170C(outHost);
}

/* 0x812BA50A */
static int syncGetCurrentInterface(int *outIfType)
{
	return sceMtpGetCurrentInterface(outIfType);
}

/* 0x812BA564 */
static int syncStopInterface(void)
{
	return sceMtpStop();
}

/* 0x812BA56C */
static int syncIsStarted(int *outStarted)
{
	return SceLibMtp_B7984D4C(outStarted);
}

/* 0x812BA576 */
static int syncSwitchUsbMode(int usbMode)
{
	return g_mtpServer.switchUsbMode(usbMode);
}

/* 0x812BA586 */
static int syncGetUsbModeState(int *outState)
{
	return g_mtpServer.getUsbModeState(outState);
}

/* 0x812BA596 */
static int syncGetPublicStatus(int *outStatus)
{
	if (outStatus == NULL)
		return MTP_SVC_ERROR_INVALID_ARGUMENT;
	int progress = 0;
	*outStatus = SceLibMtp_08C00AFC(&progress);
	return 0;
}

/* 0x812BA666 */
static int syncSetCmaActive(int active)
{
	g_mtpServer.setCmaActive(active);
	return 0;
}

/* 0x812BA678 */
static int syncGetCmaActive(int *outActive)
{
	if (outActive == NULL)
		return MTP_SVC_ERROR_INVALID_ARGUMENT;
	*outActive = g_mtpServer.isCmaActive();
	return 0;
}

/* 0x812BA69A */
static int syncStartConsoleInfo(int *outTransferId)
{
	if (outTransferId == NULL)
		return MTP_SVC_ERROR_INVALID_ARGUMENT;
	return g_mtpServer.startConsoleInfo(outTransferId);
}

/* 0x812BA6B6 */
static int syncBeginSendConsoleInfoData(char *outUrlBuf, SceSize urlBufSize, int *outSendId)
{
	return g_mtpServer.beginSendConsoleInfoData(outUrlBuf, urlBufSize, outSendId);
}

/* 0x812BA6C6 */
static int syncEndSendConsoleInfoData(int transferResult)
{
	return g_mtpServer.endSendConsoleInfoData(transferResult);
}

/* 0x812BA6D6 */
static int syncCancelConsoleInfo(void)
{
	return g_mtpServer.cancelConsoleInfo();
}

/* 0x812BA9CC */
static int syncCheckAborted(int contentCategory, int objectHandle, int queryMode, int *outAborted)
{
	return SceLibMtp_FB181427(contentCategory, objectHandle, queryMode, outAborted);
}

/* 0x812BAC9E */
static int syncGetDeviceId(SceUInt8 *outDeviceId)
{
	return SceLibMtp_29FAC63B(outDeviceId);
}

/* 0x812BAE20 */
static int syncBeginLargeBuffer(void)
{
	SceLibMtp_AF9A2787();
	return 0;
}

/* 0x812BAE2A */
static int syncEndLargeBuffer(void)
{
	return SceLibMtp_BB32CD0A();
}

/* 0x812BB008 */
static int syncBgCopyEndTask(void)
{
	return SceLibMtp_E7542628();
}

/* 0x812BB096 */
static int syncBgCopyWriteData(const void *buffer, SceSize size, int streamFlags)
{
	return SceLibMtp_972A378F(buffer, size, streamFlags);
}

/* 0x812BB0A4 */
static int syncBgCopyReadData(void *outBuffer, SceSize offset, SceSize readSize,
			      int streamFlags, SceSize *outReadBytes)
{
	return SceLibMtp_ADE08924(outBuffer, offset, readSize, streamFlags, outReadBytes);
}

/* ---------------------------------------------------------------------------
 * Top-Level Sync & Async Dispatchers (0x812B5976 & 0x812B64B8)
 * --------------------------------------------------------------------------- */

/* 0x812B5976 */
int mtpServiceDispatchSync(IPMI::Session *session, SceUInt32 methodId,
			   const IPMI::DataInfo *inList, SceUInt32 inCount,
			   IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	(void)inCount;
	const SceUInt32 *inWords = (inList != NULL && inList[0].pData != NULL)
					   ? (const SceUInt32 *)inList[0].pData
					   : NULL;
	int res = 0;

	switch (methodId) {
	case MTP_METHOD_GET_SESSION_ID:
		res = syncGetSessionId(session, (SceUInt32 *)outList[0].pBuffer);
		outList[0].outSize = sizeof(SceUInt32);
		break;

	case MTP_METHOD_DISABLE_NOTIFY:
		res = syncDisableNotify(session);
		break;

	case MTP_METHOD_RECV_DATA_SYNC: {
		SceSize *receivedSize = (SceSize *)outList[1].pBuffer;
		SceUInt64 *totalSize = (SceUInt64 *)outList[2].pBuffer;
		res = syncRecvData(session, inWords[0], outList[0].pBuffer, inWords[1],
				   receivedSize, totalSize);
		outList[0].outSize = *receivedSize;
		outList[1].outSize = sizeof(SceSize);
		outList[2].outSize = sizeof(SceUInt64);
		break;
	}

	case MTP_METHOD_SEND_DATA_SYNC: {
		SceSize *sentSize = (SceSize *)outList[0].pBuffer;
		SceUInt64 *totalSize = (SceUInt64 *)outList[1].pBuffer;
		res = syncSendData(session, inWords[0], inList[1].pData, inWords[1],
				   sentSize, totalSize);
		outList[0].outSize = sizeof(SceSize);
		outList[1].outSize = sizeof(SceUInt64);
		break;
	}

	case MTP_METHOD_SET_DATA_PHASE_RESULT:
		res = syncSetDataPhaseResult(session, inWords[0], (int)inWords[1]);
		break;

	case MTP_METHOD_GET_CONTEXT_PROGRESS:
		res = syncGetContextProgress(session, inWords[0], outList[0].pBuffer);
		outList[0].outSize = 24u;
		break;

	case MTP_METHOD_GET_HOST_STATUS:
		res = syncGetHostStatus(session, (MtpStatus *)outList[0].pBuffer);
		outList[0].outSize = sizeof(MtpStatus);
		break;

	case MTP_METHOD_GET_HOST_VERSION:
		res = syncGetHostVersion(outList[0].pBuffer);
		outList[0].outSize = 36u;
		break;

	case MTP_METHOD_CANCEL_EXECUTION:
		res = syncCancelExecution(inWords[0], inWords[1]);
		break;

	case MTP_METHOD_CHECK_HOST_RESET:
		res = syncCheckHostReset(session, (SceUInt32 *)outList[0].pBuffer);
		outList[0].outSize = sizeof(SceUInt32);
		break;

	case MTP_METHOD_MARLIN_ACTIVATE:
		res = syncMarlinActivate(inWords[0], (SceUInt32 *)outList[0].pBuffer,
					 (SceUInt32 *)outList[1].pBuffer);
		outList[0].outSize = sizeof(SceUInt32);
		outList[1].outSize = sizeof(SceUInt32);
		break;

	case MTP_METHOD_MARLIN_DEACTIVATE:
		res = syncMarlinDeactivate(inWords[0], (SceUInt32 *)outList[0].pBuffer,
					   (SceUInt32 *)outList[1].pBuffer);
		outList[0].outSize = sizeof(SceUInt32);
		outList[1].outSize = sizeof(SceUInt32);
		break;

	case MTP_METHOD_MARLIN_LICENSE_MOVE:
		res = syncMarlinLicenseMove((const char *)inList[1].pData, inWords[0], inWords[1],
					    (SceUInt32 *)outList[0].pBuffer);
		outList[0].outSize = sizeof(SceUInt32);
		break;

	case MTP_METHOD_MARLIN_LICENSE_COPY:
		res = syncMarlinLicenseCopy((const char *)inList[1].pData, inWords[0], inWords[1],
					    (SceUInt32 *)outList[0].pBuffer);
		outList[0].outSize = sizeof(SceUInt32);
		break;

	case MTP_METHOD_MARLIN_LICENSE_CHECK:
		res = syncMarlinLicenseCheck((const char *)inList[1].pData, inWords[0], inWords[1],
					     (SceUInt32 *)outList[0].pBuffer);
		outList[0].outSize = sizeof(SceUInt32);
		break;

	case MTP_METHOD_MARLIN_SHARED_INFO_ACQ:
		res = syncMarlinSharedInfoAcq((const char *)inList[1].pData, inWords[0],
					      (SceUInt32 *)outList[0].pBuffer);
		outList[0].outSize = sizeof(SceUInt32);
		break;

	case MTP_METHOD_ADD_OBJECT_DATA:
		res = syncAddObjectData(inList[0].pData, (SceUInt64 *)outList[0].pBuffer);
		outList[0].outSize = sizeof(SceUInt64);
		break;

	case MTP_METHOD_REMOVE_OBJECT_DATA:
		res = syncRemoveObjectData(inWords[0], inWords[1]);
		break;

	case MTP_METHOD_GET_OBJECT_DATA:
		res = syncGetObjectData(inWords[0], inWords[1], outList[0].pBuffer);
		outList[0].outSize = 1329u;
		break;

	case MTP_METHOD_FIND_OBJECT_BY_PATH:
		res = syncFindObjectByPath((const char *)inList[0].pData,
					   (SceUInt64 *)outList[0].pBuffer);
		outList[0].outSize = sizeof(SceUInt64);
		break;

	case MTP_METHOD_UPDATE_OBJECT_DATA:
		res = syncUpdateObjectData(inWords[0], inWords[1], inList[1].pData);
		break;

	case MTP_METHOD_RESET_STORAGE:
		res = syncResetStorage();
		break;

	case MTP_METHOD_START_HOST_DISCOVERY:
		res = syncStartHostDiscovery(session);
		break;

	case MTP_METHOD_STOP_HOST_DISCOVERY:
		res = syncStopHostDiscovery(session);
		break;

	case MTP_METHOD_GET_HOST_DISCOVERY_STATE:
		res = syncGetHostDiscoveryState((int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		break;

	case MTP_METHOD_GET_NUM_DISCOVERED_HOSTS:
		res = syncGetNumDiscoveredHosts((SceUInt32 *)outList[0].pBuffer);
		outList[0].outSize = sizeof(SceUInt32);
		break;

	case MTP_METHOD_GET_DISCOVERED_HOST:
		res = syncGetDiscoveredHost(inWords[0], (MtpNetHost *)outList[0].pBuffer);
		outList[0].outSize = sizeof(MtpNetHost);
		break;

	case MTP_METHOD_START_NET_REGISTER:
		res = syncStartNetRegister(session, (MtpNetHost *)inList[0].pData);
		break;

	case MTP_METHOD_SEND_NET_REGISTER_PIN:
		res = syncSendNetRegisterPin((const char *)inList[0].pData);
		break;

	case MTP_METHOD_STOP_NET_REGISTER:
		res = syncStopNetRegister(session);
		break;

	case MTP_METHOD_GET_NUM_REGISTERED_HOSTS:
		res = syncGetNumRegisteredHosts((SceUInt32 *)outList[0].pBuffer);
		outList[0].outSize = sizeof(SceUInt32);
		break;

	case MTP_METHOD_GET_REGISTERED_HOST:
		res = syncGetRegisteredHost(inWords[0], (MtpNetHost *)outList[0].pBuffer);
		outList[0].outSize = sizeof(MtpNetHost);
		break;

	case MTP_METHOD_UNREGISTER_HOST:
		res = syncUnregisterHost((const MtpNetHost *)inList[0].pData);
		break;

	case MTP_METHOD_IS_HOST_ONLINE:
		res = syncIsHostOnline((const MtpNetHost *)inList[0].pData,
				       (int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		break;

	case MTP_METHOD_IS_HOST_REGISTERED:
		res = syncIsHostRegistered((const MtpNetHost *)inList[0].pData,
					   (int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		break;

	case MTP_METHOD_ASSIGN_HOST:
		res = syncAssignHost((const MtpNetHost *)inList[0].pData);
		break;

	case MTP_METHOD_UNASSIGN_HOST:
		res = syncUnassignHost();
		break;

	case MTP_METHOD_IS_HOST_ASSIGNED:
		res = syncIsHostAssigned((int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		break;

	case MTP_METHOD_GET_ASSIGNED_HOST:
		res = syncGetAssignedHost((MtpNetHost *)outList[0].pBuffer);
		outList[0].outSize = sizeof(MtpNetHost);
		break;

	case MTP_METHOD_GET_CURRENT_INTERFACE:
		res = syncGetCurrentInterface((int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		break;

	case MTP_METHOD_START_INTERFACE:
		res = syncStartInterface((int)inWords[0]);
		break;

	case MTP_METHOD_STOP_INTERFACE:
	case MTP_METHOD_STOP_INTERFACE_ALT:
		res = syncStopInterface();
		break;

	case MTP_METHOD_IS_STARTED:
		res = syncIsStarted((int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		break;

	case MTP_METHOD_SWITCH_USB_MODE:
		res = syncSwitchUsbMode((int)inWords[0]);
		break;

	case MTP_METHOD_GET_USB_MODE_STATE:
		res = syncGetUsbModeState((int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		break;

	case MTP_METHOD_GET_PUBLIC_STATUS:
		res = syncGetPublicStatus((int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		break;

	case MTP_METHOD_GET_CAPABILITY:
		res = syncGetCapability((int)inWords[0], (MtpCapabilityInfo *)outList[0].pBuffer);
		outList[0].outSize = sizeof(MtpCapabilityInfo);
		break;

	case MTP_METHOD_SET_CMA_ACTIVE:
		res = syncSetCmaActive((int)inWords[0]);
		break;

	case MTP_METHOD_GET_CMA_ACTIVE:
		res = syncGetCmaActive((int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		break;

	case MTP_METHOD_START_CONSOLE_INFO:
		res = syncStartConsoleInfo((int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		break;

	case MTP_METHOD_BEGIN_SEND_CONSOLE_INFO_DATA: {
		SceSize urlBufSize = inWords[0];
		res = syncBeginSendConsoleInfoData((char *)outList[0].pBuffer, urlBufSize,
						   (int *)outList[1].pBuffer);
		outList[0].outSize = urlBufSize;
		outList[1].outSize = sizeof(int);
		break;
	}

	case MTP_METHOD_END_SEND_CONSOLE_INFO_DATA:
		res = syncEndSendConsoleInfoData((int)inWords[0]);
		break;

	case MTP_METHOD_CANCEL_CONSOLE_INFO:
		res = syncCancelConsoleInfo();
		break;

	case MTP_METHOD_CHECK_ABORTED:
		res = syncCheckAborted((int)inWords[0], (int)inWords[1], (int)inWords[2],
				       (int *)outList[0].pBuffer);
		outList[0].outSize = sizeof(int);
		break;

	case MTP_METHOD_GET_DEVICE_ID:
		res = syncGetDeviceId((SceUInt8 *)outList[0].pBuffer);
		outList[0].outSize = 16u;
		break;

	case MTP_METHOD_PAUSE_EXPORT_OBJECT:
		res = syncPauseExportObject(inWords[0], inWords[1]);
		break;

	case MTP_METHOD_NOOP:
		res = 0;
		break;

	case MTP_METHOD_BEGIN_LARGE_BUFFER:
		res = syncBeginLargeBuffer();
		break;

	case MTP_METHOD_END_LARGE_BUFFER:
		res = syncEndLargeBuffer();
		break;

	case MTP_METHOD_SET_BG_COPY_FLAG:
		res = syncSetBgCopyFlag((int)inWords[0]);
		break;

	case MTP_METHOD_SEND_SESSION_INFO:
		res = syncSendSessionInfo(session);
		break;

	case MTP_METHOD_BGCOPY_REQUEST_TASK:
		res = syncBgCopyRequestTask((int)inWords[0], inWords[2], inWords[3],
					    (const char *)inList[1].pData,
					    (char *)outList[0].pBuffer);
		outList[0].outSize = 1024u;
		break;

	case MTP_METHOD_BGCOPY_END_TASK:
		res = syncBgCopyEndTask();
		break;

	case MTP_METHOD_BGCOPY_SET_TASK_INFO:
		res = syncBgCopySetTaskInfo((int)inWords[0], (const char *)inList[1].pData,
					    inWords[1], (int)inWords[2], inWords[3],
					    (int)inWords[4]);
		break;

	case MTP_METHOD_BGCOPY_WRITE_DATA:
		res = syncBgCopyWriteData(inList[1].pData, inWords[0], (int)inWords[1]);
		break;

	case MTP_METHOD_BGCOPY_READ_DATA: {
		SceSize readSize = inWords[1];
		res = syncBgCopyReadData(outList[0].pBuffer, inWords[0], readSize,
					 (int)inWords[2], (SceSize *)outList[1].pBuffer);
		outList[0].outSize = readSize;
		outList[1].outSize = sizeof(SceSize);
		break;
	}

	case MTP_METHOD_BGCOPY_GET_TASK_INFO:
		res = syncBgCopyGetTaskInfo((SceUInt32 *)outList[0].pBuffer, (char *)outList[1].pBuffer);
		outList[0].outSize = sizeof(SceUInt32);
		outList[1].outSize = 1024u;
		break;

	case MTP_METHOD_BGCOPY_NOTIFY_TASK:
		res = syncBgCopyNotifyTask((int)inWords[0], (int)inWords[1],
					   inWords[2], inWords[3],
					   (const char *)inList[1].pData,
					   (int)inWords[4], inWords[5],
					   (int)inWords[6], inWords[7], (int)inWords[8]);
		break;

	default:
		return IPMI_ERROR_UNKNOWN_METHOD;
	}

	return session->respondToSyncMethodRequest(res, outList, outCount);
}

/* 0x812B64B8 */
int mtpServiceDispatchAsync(IPMI::Session *session, SceUInt32 requestId,
			    SceUInt32 methodId, const IPMI::DataInfo *inList)
{
	const SceUInt32 *inWords = (inList != NULL && inList[0].pData != NULL)
					   ? (const SceUInt32 *)inList[0].pData
					   : NULL;

	switch (methodId) {
	case MTP_METHOD_BEGIN_GET_NUM_OF_OBJECT:
		return beginGetNumOfObject(session, requestId, (int)inWords[0], (int)inWords[1]);

	case MTP_METHOD_BEGIN_GET_OBJECT_METADATA:
		return beginGetObjectMetadata(session, requestId, (int)inWords[0], (int)inWords[1],
					      (int)inWords[2], (int)inWords[3]);

	case MTP_METHOD_BEGIN_IMPORT_OBJECT:
		return beginImportObject(session, requestId, (int)inWords[0], inList[1].pData,
					 inWords[1], inList[2].pData, inWords[2]);

	case MTP_METHOD_BEGIN_EXPORT_OBJECT_WITH_CHECK:
		return beginExportObjectWithCheck(session, requestId, inWords[0], inWords[1],
						  inWords[2], inWords[3], (int)inWords[4]);

	case MTP_METHOD_BEGIN_CREATE_HOST_DIR:
		return beginCreateHostDir(session, requestId, inList[1].pData,
					  (int)inWords[0], (int)inWords[1]);

	case MTP_METHOD_BEGIN_RENAME_HOST_DIR:
		return beginRenameHostDir(session, requestId, inList[1].pData,
					  (int)inWords[0], (int)inWords[1]);

	case MTP_METHOD_BEGIN_MOVE_HOST_DIR:
		return beginMoveHostDir(session, requestId, (int)inWords[0], (int)inWords[1]);

	case MTP_METHOD_BEGIN_CREATE_HOST_FILE:
		return beginCreateHostFile(session, requestId, inList[1].pData,
					   (int)inWords[0], (int)inWords[1]);

	case MTP_METHOD_BEGIN_GET_OBJECT_THUMBNAIL:
		return beginGetObjectThumbnail(session, requestId,
					       (int)inWords[0], (int)inWords[1]);

	case MTP_METHOD_BEGIN_GET_OBJECT_STATUS2:
		return beginGetObjectStatus2(session, requestId, (int)inWords[0],
					     inList[1].pData, (int)inWords[1]);

	case MTP_METHOD_BEGIN_DELETE_OBJECT:
		return beginDeleteObject(session, requestId, (int)inWords[0]);

	case MTP_METHOD_BEGIN_HTTP_GET_DATA_WITH_URL: {
		SceUInt64 offset = ((SceUInt64)inWords[3] << 32) | inWords[2];
		return beginHttpGetDataWithUrl(session, requestId, (const char *)inList[1].pData,
					       (int)inWords[0], offset);
	}

	case MTP_METHOD_BEGIN_HTTP_GET_PROPERTY_WITH_URL:
		return beginHttpGetPropertyWithUrl(session, requestId,
						   (const char *)inList[1].pData, (int)inWords[0]);

	case MTP_METHOD_BEGIN_GET_SYSTEM_SETTING:
		return beginGetSystemSetting(session, requestId, (int)inWords[0]);

	case MTP_METHOD_BEGIN_SET_SYSTEM_SETTING:
		return beginSetSystemSetting(session, requestId, (int)inWords[0], (int)inWords[1]);

	case MTP_METHOD_BEGIN_GET_NP_ACCOUNT_INFO:
		return beginGetNpAccountInfo(session, requestId, (int)inWords[0]);

	case MTP_METHOD_BEGIN_READ_OBJECT_WITH_OFFSET: {
		SceUInt64 offset = ((SceUInt64)inWords[3] << 32) | inWords[2];
		SceUInt64 readSize = ((SceUInt64)inWords[5] << 32) | inWords[4];
		return beginReadObjectWithOffset(session, requestId, (int)inWords[0],
						 offset, readSize);
	}

	case MTP_METHOD_BEGIN_WRITE_OBJECT_WITH_OFFSET: {
		SceUInt64 offset = ((SceUInt64)inWords[3] << 32) | inWords[2];
		SceUInt64 writeSize = ((SceUInt64)inWords[5] << 32) | inWords[4];
		return beginWriteObjectWithOffset(session, requestId, (int)inWords[0],
						  offset, writeSize);
	}

	case MTP_METHOD_BEGIN_GET_HOST_STORAGE_SIZE:
		return beginGetHostStorageSize(session, requestId, (int)inWords[0]);

	case MTP_METHOD_BEGIN_CHECK_SAME_OBJECT_EXIST:
		return beginCheckSameObjectExist(session, requestId, inWords[0], inWords[1]);

	case MTP_METHOD_BEGIN_GET_TOTAL_OBJECT_SIZE:
		return beginGetTotalObjectSize(session, requestId, (int)inWords[0]);

	case MTP_METHOD_BEGIN_GET_SPECIFIED_METADATA:
		return beginGetSpecifiedObjectMetadata(session, requestId, (int)inWords[0],
						       inList[1].pData, (int)inWords[1]);

	case MTP_METHOD_BEGIN_NPDRM_GET_RTC:
		return beginNpDrmGetRtc(session, requestId, inList[1].pData, inWords[0]);

	case MTP_METHOD_BEGIN_NPDRM_ACTIVATE:
		return beginNpDrmActivate(session, requestId, inWords[0],
					  inList[1].pData, inWords[1]);

	case MTP_METHOD_BEGIN_NPDRM_DEACTIVATE:
		return beginNpDrmDeactivate(session, requestId, inWords[0],
					    inList[1].pData, inWords[1]);

	case MTP_METHOD_BEGIN_NPDRM_GET_LICENSE:
		return beginNpDrmGetLicense(session, requestId, inList[1].pData,
					    inList[2].pData, inWords[0]);

	case MTP_METHOD_BEGIN_RECV_DATA:
		return beginRecvData(session, requestId, inWords[0], inWords[1]);

	case MTP_METHOD_BEGIN_SEND_DATA:
		return beginSendData(session, requestId, inWords[0], inList[1].pData, inWords[1]);

	case MTP_METHOD_BEGIN_RPC_NET_OPERATION:
		return beginMtpRpcNetOperation(session, requestId, inList[1].pData, inWords[0]);

	case MTP_METHOD_BEGIN_RPC_NET_OPERATION_SEND:
		return beginMtpRpcNetOperationSend(session, requestId, inWords[0]);

	case MTP_METHOD_BEGIN_RPC_NET_OPERATION_RECV:
		return beginMtpRpcNetOperationRecv(session, requestId, inList[1].pData, inWords[0]);

	case MTP_METHOD_BEGIN_SEARCH_OBJECT:
		return beginSearchObject(session, requestId, (int)inWords[0],
					 inList[1].pData, (int)inWords[1]);

	case MTP_METHOD_BEGIN_CONNECT_TO_HOST:
		return beginConnectToHost(session, requestId,
					  (const MtpNetHost *)inList[1].pData, (int)inWords[0]);

	case MTP_METHOD_BEGIN_HANDOVER:
		return beginHandover(session, requestId, (int)inWords[0]);

	case MTP_METHOD_BEGIN_RESUME_EXPORT_OBJECT:
		return beginResumeExportObject(session, requestId);

	case MTP_METHOD_BEGIN_RESUME_IMPORT_OBJECT: {
		SceUInt64 offset = ((SceUInt64)inWords[3] << 32) | inWords[2];
		SceUInt64 resumeSize = ((SceUInt64)inWords[5] << 32) | inWords[4];
		return beginResumeImportObject(session, requestId, (int)inWords[0],
					       offset, resumeSize, inList[1].pData);
	}

	default:
		return IPMI_ERROR_UNKNOWN_METHOD;
	}
}
