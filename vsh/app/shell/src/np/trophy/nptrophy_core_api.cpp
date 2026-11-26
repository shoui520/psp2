/*
 * nptrophy_core_api.cpp: SceNpTrophy core API marshaling layer implementing all
 * npTrophyCore* entrypoints called by SceNpTrophyServer IPC handlers and jobs
 * (0x812C16B4..0x812C7DFC).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include "../ipc/nptrophy/nptrophy_internal.h"
#include "nptrophy_service.h"

namespace {

/* RAII guard releasing a pooled object reference on scope exit */
template <typename T>
class PoolObjGuard {
public:
	PoolObjGuard() : m_obj(NULL) {}
	~PoolObjGuard() { Reset(); }

	T **Out() { return &m_obj; }
	T *Get() const { return m_obj; }
	T *operator->() const { return m_obj; }

	T *ReleaseOwnership()
	{
		T *tmp = m_obj;
		m_obj = NULL;
		return tmp;
	}

	void Reset()
	{
		if (m_obj != NULL) {
			m_obj->ReleaseRef();
			m_obj = NULL;
		}
	}

private:
	T *m_obj;
};

static uint32_t buildSessionAccessFlags(int sessionFlags, int extraFlags, SceUInt64 options)
{
	uint32_t flags = static_cast<uint32_t>(sessionFlags) |
			 static_cast<uint32_t>(extraFlags);
	if ((options & 1ULL) != 0)
		flags |= 0x00000100u;
	return flags;
}

static int openTitleStorageIfNeeded(TrophyHandle *handle, TrophyTitleStorage *titleStorage,
				    uint32_t openFlags)
{
	if (titleStorage->IsOpen())
		return 0;
	return titleStorage->Open(handle, openFlags);
}

static int openTrpConfSourceIfNeeded(TrophyClientState *client, TrophyHandle *handle,
				     TrophyTrpConfSource *trpSource, const TrophyCommId *commId,
				     bool verifySha1)
{
	if (trpSource->IsOpen() != 0)
		return 0;

	int ret = client->EnsureMounted();
	if (ret < 0)
		return ret;

	return trpSource->Open(handle, client->GetMount(), commId, verifySha1);
}

static int checkTitleStatusForGameInternal(TrophyClientState *client, TrophyHandle *handle,
					   TrophyTitleSessionPoolObj *sessionObj,
					   TrophyContextPoolObj *ctxObj, int *outStatus)
{
	if (outStatus == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	*outStatus = 0;

	TrophyContext *context = ctxObj->GetContext();
	const TrophyCommId &commId = context->GetCommId();
	TrophyUserSession *userSession = sessionObj->GetUserSession();

	int ret = userSession->EnsureReady(handle, 1);
	if (ret < 0)
		return ret;

	bool exists = false;
	TrophyTitleRecord record;
	ret = userSession->GetIndexStorage()->HasTitle(handle, &commId, &exists, &record);
	if (ret < 0)
		return ret;

	if (!exists) {
		*outStatus = 1;
		return 0;
	}

	if (record.IsBroken()) {
		*outStatus = 3;
		return 0;
	}

	PoolObjGuard<TrophyConfSourcePoolObj> confSource;
	ret = client->AcquireOrCreateConfSource(&commId, confSource.Out());
	if (ret < 0)
		return ret;

	ret = openTrpConfSourceIfNeeded(client, handle, confSource->GetConfSource(), &commId,
					false);
	if (ret < 0)
		return ret;

	TrophyConfData *trpConf = NULL;
	ret = confSource->GetConfSource()->ReadDefaultConf(handle, &trpConf);
	if (ret < 0)
		return ret;

	uint32_t trpSetVer = trpConf->GetTrophySetVersion();
	delete trpConf;

	if (trpSetVer > static_cast<uint32_t>(record.titleSnapshot.summary.trophySetVersion))
		*outStatus = 2;
	else
		*outStatus = 0;

	return 0;
}

static int registerStreamObject(TrophyClientState *client, TrophyFileBase *ownedFile,
				bool stripSfmHeader, SceSize headerSize,
				PoolObjGuard<TrophyTitleStoragePoolObj> &heldStorage,
				PoolObjGuard<TrophyConfSourcePoolObj> &heldConfSource,
				int *outStreamId, SceUInt64 *outSize)
{
	TrophyStreamWrapper *wrapper = new TrophyStreamWrapper();
	if (wrapper == NULL) {
		ownedFile->Close();
		delete ownedFile;
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	}

	int ret = wrapper->BindFile(ownedFile, stripSfmHeader, headerSize);
	if (ret < 0) {
		delete wrapper;
		return ret;
	}

	if (outSize != NULL) {
		ret = wrapper->GetReadableSize(outSize);
		if (ret < 0) {
			delete wrapper;
			return ret;
		}
	}

	TrophyStreamPoolObj *streamObj = new TrophyStreamPoolObj();
	if (streamObj == NULL) {
		delete wrapper;
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	}

	ret = streamObj->Init(wrapper, heldStorage.Get(), heldConfSource.Get());
	if (ret < 0) {
		delete streamObj;
		delete wrapper;
		return ret;
	}

	/* Stream pool object takes ownership of the held references */
	heldStorage.ReleaseOwnership();
	heldConfSource.ReleaseOwnership();

	ret = client->GetStreamPool().Add(streamObj, outStreamId);
	if (ret < 0) {
		delete streamObj;
		return ret;
	}
	return 0;
}

} /* namespace */

/* =========================================================================
 * Service lifecycle & client session hooks (0x812C2094..0x812C259F)
 * ========================================================================= */

/* 0x812C2094 */
int npTrophyCoreInit(void)
{
	return g_npTrophyService.Init();
}

/* 0x812C2154 */
int npTrophyCoreTerm(void)
{
	return g_npTrophyService.Term();
}

/* 0x812C2504 */
int npTrophyCoreOnClientConnect(SceUID pid, IPMI::Session *session)
{
	return g_npTrophyService.OnClientConnect(pid, session);
}

/* 0x812C252C */
int npTrophyCoreOnClientDisconnect(SceUID pid, IPMI::Session *session, int reason)
{
	return g_npTrophyService.OnClientDisconnect(pid, session, reason);
}

/* 0x812C2558 */
int npTrophyCoreSuspend(SceUID pid)
{
	return g_npTrophyService.SuspendPid(pid);
}

/* 0x812C257C */
int npTrophyCoreResume(SceUID pid)
{
	return g_npTrophyService.ResumePid(pid);
}

/* =========================================================================
 * Context, debug config, and handle management (0x812C16B4..0x812C2093)
 * ========================================================================= */

/* 0x812C16B4 */
int npTrophyCoreCreateContext(SceUID pid, IPMI::Session *session, int *outCtxId, int titleIdMode,
			      const void *commId, const void *commSign, SceUInt64 options)
{
	if (outCtxId == NULL || commId == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	TrophyCommId parsedCommId;
	sceClibMemcpy(parsedCommId.data, commId, sizeof(parsedCommId.data));

	TrophyContextPoolObj *ctxObj = new TrophyContextPoolObj();
	if (ctxObj == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	ret = ctxObj->Init(titleIdMode, pid, &parsedCommId, commSign, options);
	if (ret < 0) {
		delete ctxObj;
		return ret;
	}

	ret = client->GetContextPool().Add(ctxObj, outCtxId);
	if (ret < 0) {
		delete ctxObj;
		if (ret == NP_TROPHY_ERR_HANDLE_LIMIT)
			return NP_TROPHY_ERR_CONTEXT_LIMIT;
		return ret;
	}
	return 0;
}

/* 0x812C1832 */
int npTrophyCoreDestroyContext(SceUID pid, IPMI::Session *session, int ctxId)
{
	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	ret = client->GetContextPool().RemoveById(ctxId);
	if (ret == NP_TROPHY_ERR_INVALID_HANDLE)
		return NP_TROPHY_ERR_INVALID_CONTEXT;
	return ret;
}

/* 0x812C19B4 */
int npTrophyCoreRegisterContextData(SceUID pid, IPMI::Session *session, const void *commId,
				    const void *buf, SceSize bufSize)
{
	(void)pid;
	(void)session;
	if (!g_npTrophyService.IsInitialized())
		return NP_TROPHY_ERR_NOT_INITIALIZED;
	return g_npTrophyService.SetDebugParam(static_cast<const char *>(commId), buf, bufSize);
}

/* 0x812C19DA */
int npTrophyCoreReadContextData(SceUID pid, IPMI::Session *session, const void *commId,
				void *outBuf, SceSize bufSize)
{
	(void)pid;
	(void)session;
	if (!g_npTrophyService.IsInitialized())
		return NP_TROPHY_ERR_NOT_INITIALIZED;
	return g_npTrophyService.GetDebugParam(static_cast<const char *>(commId), outBuf, bufSize);
}

/* 0x812C1A00 */
int npTrophyCoreUpdateContextData(SceUID pid, IPMI::Session *session, int ctxId, const void *buf,
				  SceSize bufSize)
{
	(void)pid;
	(void)session;
	if (!g_npTrophyService.IsInitialized())
		return NP_TROPHY_ERR_NOT_INITIALIZED;
	return g_npTrophyService.ExecDebugOp(ctxId, buf, bufSize);
}

/* 0x812C1A28 */
int npTrophyCoreCreateHandle(SceUID pid, IPMI::Session *session, int handleType, int *outHandleId)
{
	if (outHandleId == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	TrophyHandlePoolObj *handleObj = new TrophyHandlePoolObj();
	if (handleObj == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	ret = handleObj->Init(handleType);
	if (ret < 0) {
		delete handleObj;
		return ret;
	}

	ret = client->GetHandlePool().Add(handleObj, outHandleId);
	if (ret < 0) {
		delete handleObj;
		return ret;
	}
	return 0;
}

/* 0x812C1B76 */
int npTrophyCoreDestroyHandle(SceUID pid, IPMI::Session *session, int handleId)
{
	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret >= 0) {
		handleObj->GetHandle()->Cancel(NP_TROPHY_ERR_ABORTED);
		handleObj.Reset();
	}

	return client->GetHandlePool().RemoveById(handleId);
}

/* 0x812C1DC8 */
int npTrophyCoreAbortHandle(SceUID pid, IPMI::Session *session, int handleId)
{
	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;

	handleObj->GetHandle()->Cancel(NP_TROPHY_ERR_ABORTED);
	return 0;
}

/* 0x812C1F22 */
int npTrophyCoreGetHandleProgress(SceUID pid, IPMI::Session *session, int handleId,
				  int *outProgress)
{
	if (outProgress == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;

	*outProgress = handleObj->GetHandle()->GetProgressPercent();
	return 0;
}

/* =========================================================================
 * Title session lifecycle (0x812C2DF8..0x812C319F)
 * ========================================================================= */

/* 0x812C2DF8 */
int npTrophyCoreCreateTitleSession(SceUID pid, IPMI::Session *session, int sessionFlags, int limit,
				   int reserved, SceUInt64 options, int *outTitleSession)
{
	if (outTitleSession == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	TrophyHandle tempHandle;
	ret = tempHandle.Init();
	if (ret < 0)
		return ret;

	TrophyTitleSessionPoolObj *sessionObj = new TrophyTitleSessionPoolObj();
	if (sessionObj == NULL) {
		tempHandle.Term();
		return NP_TROPHY_ERR_OUT_OF_MEMORY;
	}

	uint32_t accessFlags = buildSessionAccessFlags(sessionFlags, reserved, options);
	ret = sessionObj->Init(&tempHandle, &g_trophyStorageArbiter, limit, accessFlags);
	tempHandle.Term();
	if (ret < 0) {
		delete sessionObj;
		return ret;
	}

	ret = client->GetTitleSessionPool().Add(sessionObj, outTitleSession);
	if (ret < 0) {
		delete sessionObj;
		return ret;
	}
	return 0;
}

/* 0x812C307A */
int npTrophyCoreDestroyTitleSession(SceUID pid, IPMI::Session *session, int titleSession)
{
	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	client->EvictAllTitleStorage();
	ret = client->GetTitleSessionPool().RemoveById(titleSession);
	if (ret == NP_TROPHY_ERR_INVALID_HANDLE)
		return NP_TROPHY_ERR_INVALID_TITLE_SESSION;
	return ret;
}

/* =========================================================================
 * Sync, recovery, and title removal (0x812C21AC..0x812C2DF7)
 * ========================================================================= */

/* 0x812C21AC */
int npTrophyCoreCheckNetSync(SceUID pid, IPMI::Session *session, int handleId, int ctxId,
			     SceUInt64 options, int *outNeedSync)
{
	if (outNeedSync == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(ctxId, sessionObj.Out());
	if (ret < 0)
		return ret;

	TrophySyncController controller(sessionObj->GetUserSession());
	return controller.CheckNetSync(handleObj->GetHandle(), options, outNeedSync);
}

/* 0x812C236C */
int npTrophyCoreNetSyncTitles(SceUID pid, IPMI::Session *session, int handleId, int ctxId,
			      SceUInt64 options)
{
	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(ctxId, sessionObj.Out());
	if (ret < 0)
		return ret;

	TrophySyncController controller(sessionObj->GetUserSession());
	return controller.SyncTitles(handleObj->GetHandle(), options);
}

/* 0x812C25A0 */
int npTrophyCoreCheckRecovery(SceUID pid, IPMI::Session *session, int handleId, int ctxId,
			      int *outRequired)
{
	if (outRequired == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(ctxId, sessionObj.Out());
	if (ret < 0)
		return ret;

	bool required = false;
	TrophyRecoveryController controller(sessionObj->GetUserSession());
	ret = controller.CheckRecoveryRequired(handleObj->GetHandle(), &required);
	if (ret >= 0)
		*outRequired = required ? 1 : 0;
	return ret;
}

/* 0x812C2738 */
int npTrophyCorePerformRecovery(SceUID pid, IPMI::Session *session, int handleId, int ctxId)
{
	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(ctxId, sessionObj.Out());
	if (ret < 0)
		return ret;

	TrophyRecoveryController controller(sessionObj->GetUserSession());
	return controller.RecoverAllTitles(handleObj->GetHandle());
}

/* 0x812C28C8 */
int npTrophyCoreGetTitleForRemoval(SceUID pid, IPMI::Session *session, int handleId, int ctxId,
				   void *outInfo, void *outExtra, NpTrophyGameDetails *outDetails)
{
	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(ctxId, sessionObj.Out());
	if (ret < 0)
		return ret;

	TrophyUserSession *userSession = sessionObj->GetUserSession();
	ret = userSession->EnsureReady(handleObj->GetHandle(), 1);
	if (ret < 0)
		return ret;

	TrophyTitleRecord oldest;
	ret = userSession->GetIndexStorage()->FindOldestOrBrokenTitle(handleObj->GetHandle(),
								      &oldest);
	if (ret < 0)
		return ret;

	if (outInfo != NULL)
		oldest.ExportTitleInfo(outInfo);
	if (outExtra != NULL)
		oldest.ExportSyncInfo(outExtra);

	if (outDetails != NULL) {
		sceClibMemset(outDetails, 0, sizeof(*outDetails));
		outDetails->size = sizeof(*outDetails);

		TrophyTitleStorage titleStorage(userSession, &oldest.commId);
		if (titleStorage.Open(handleObj->GetHandle(), 0x03u) >= 0) {
			TrophyConfHandle *confHandle = NULL;
			if (titleStorage.ReadConfHandle(handleObj->GetHandle(), &confHandle) >= 0 &&
			    confHandle != NULL) {
				titleStorage.ExtractTitleText(handleObj->GetHandle(), confHandle,
							      outDetails);
				delete confHandle;
			}
			titleStorage.Close();
		}
	}
	return 0;
}

/* 0x812C2A60 */
int npTrophyCoreRemoveTitle(SceUID pid, IPMI::Session *session, int handleId, int ctxId,
			    int titleHandle)
{
	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	PoolObjGuard<TrophyContextPoolObj> ctxObj;
	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(ctxId, sessionObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireContext(titleHandle, ctxObj.Out());
	if (ret < 0)
		return ret;

	const TrophyCommId &commId = ctxObj->GetContext()->GetCommId();
	client->EvictTitleStorageForCommId(&commId);

	TrophyTitleStorage titleStorage(sessionObj->GetUserSession(), &commId);
	ret = titleStorage.Uninstall(handleObj->GetHandle());
	titleStorage.Close();
	return ret;
}

/* 0x812C2C80 */
int npTrophyCoreRemoveAllTitles(SceUID pid, IPMI::Session *session, int handleId, int ctxId)
{
	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(ctxId, sessionObj.Out());
	if (ret < 0)
		return ret;

	client->EvictAllTitleStorage();
	return sessionObj->GetUserSession()->RemoveAllTitles(handleObj->GetHandle());
}

/* =========================================================================
 * Asset & SFM stream operations (0x812C31A0..0x812C4DDF)
 * ========================================================================= */

/* 0x812C31A0 */
int npTrophyCoreOpenConfStream(SceUID pid, IPMI::Session *session, int handleId, int ctxId,
			       int mode, SceUInt64 options, int *outStreamId, SceUInt64 *outSize)
{
	if (outStreamId == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyContextPoolObj> ctxObj;
	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireContext(mode, ctxObj.Out());
	if (ret < 0)
		return ret;

	TrophyContext *context = ctxObj->GetContext();
	const TrophyCommId &commId = context->GetCommId();
	bool fromTrp = (ctxId == 0) || ((options & 1ULL) != 0);

	PoolObjGuard<TrophyTitleStoragePoolObj> storageObj;
	PoolObjGuard<TrophyConfSourcePoolObj> confSourceObj;

	if (fromTrp) {
		ret = client->AcquireOrCreateConfSource(&commId, confSourceObj.Out());
		if (ret < 0)
			return ret;
		ret = openTrpConfSourceIfNeeded(client, handleObj->GetHandle(),
						confSourceObj->GetConfSource(), &commId, false);
		if (ret < 0)
			return ret;

		TrophyTrpFileStream *trpStream = new TrophyTrpFileStream();
		if (trpStream == NULL)
			return NP_TROPHY_ERR_OUT_OF_MEMORY;

		SceSize headerSize = 0;
		ret = confSourceObj->GetConfSource()->OpenSfmStream(
			handleObj->GetHandle(), context->GetDisplayLang(), true, trpStream,
			&headerSize);
		if (ret < 0) {
			delete trpStream;
			return ret;
		}

		return registerStreamObject(client, trpStream, true, headerSize, storageObj,
					    confSourceObj, outStreamId, outSize);
	}

	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	ret = client->AcquireTitleSession(ctxId, sessionObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireOrCreateTitleStorage(sessionObj.Get(), &commId, storageObj.Out());
	if (ret < 0)
		return ret;
	ret = openTitleStorageIfNeeded(handleObj->GetHandle(), storageObj->GetTitleStorage(),
				       0x03u);
	if (ret < 0)
		return ret;

	TrophyConfStream *confStream = new TrophyConfStream();
	if (confStream == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	ret = storageObj->GetTitleStorage()->OpenSfmStream(handleObj->GetHandle(), confStream);
	if (ret < 0) {
		delete confStream;
		return ret;
	}

	return registerStreamObject(client, confStream, false, 0, storageObj, confSourceObj,
				    outStreamId, outSize);
}

/* 0x812C362E */
int npTrophyCoreOpenTitleIconStream(SceUID pid, IPMI::Session *session, int handleId, int ctxId,
				    int mode, SceUInt64 options, int *outStreamId,
				    SceUInt64 *outSize)
{
	if (outStreamId == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyContextPoolObj> ctxObj;
	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireContext(mode, ctxObj.Out());
	if (ret < 0)
		return ret;

	TrophyContext *context = ctxObj->GetContext();
	const TrophyCommId &commId = context->GetCommId();
	bool fromTrp = (ctxId == 0) || ((options & 1ULL) != 0);

	PoolObjGuard<TrophyTitleStoragePoolObj> storageObj;
	PoolObjGuard<TrophyConfSourcePoolObj> confSourceObj;

	if (fromTrp) {
		ret = client->AcquireOrCreateConfSource(&commId, confSourceObj.Out());
		if (ret < 0)
			return ret;
		ret = openTrpConfSourceIfNeeded(client, handleObj->GetHandle(),
						confSourceObj->GetConfSource(), &commId, false);
		if (ret < 0)
			return ret;

		TrophyTrpFileStream *trpStream = new TrophyTrpFileStream();
		if (trpStream == NULL)
			return NP_TROPHY_ERR_OUT_OF_MEMORY;

		ret = confSourceObj->GetConfSource()->OpenTitleIconStream(
			handleObj->GetHandle(), context->GetDisplayLang(), true, trpStream);
		if (ret < 0) {
			delete trpStream;
			return ret;
		}

		return registerStreamObject(client, trpStream, false, 0, storageObj, confSourceObj,
					    outStreamId, outSize);
	}

	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	ret = client->AcquireTitleSession(ctxId, sessionObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireOrCreateTitleStorage(sessionObj.Get(), &commId, storageObj.Out());
	if (ret < 0)
		return ret;
	ret = openTitleStorageIfNeeded(handleObj->GetHandle(), storageObj->GetTitleStorage(),
				       0x03u);
	if (ret < 0)
		return ret;

	TrophyConfIconFile *iconFile = new TrophyConfIconFile();
	if (iconFile == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	ret = storageObj->GetTitleStorage()->OpenTitleIcon(handleObj->GetHandle(), iconFile);
	if (ret < 0) {
		delete iconFile;
		return ret;
	}

	return registerStreamObject(client, iconFile, false, 0, storageObj, confSourceObj,
				    outStreamId, outSize);
}

/* 0x812C3BB8 */
int npTrophyCoreOpenGroupIconStream(SceUID pid, IPMI::Session *session, int handleId, int ctxId,
				    int mode, int groupId, SceUInt64 options, int *outStreamId,
				    SceUInt64 *outSize)
{
	if (outStreamId == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyContextPoolObj> ctxObj;
	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireContext(mode, ctxObj.Out());
	if (ret < 0)
		return ret;

	TrophyContext *context = ctxObj->GetContext();
	const TrophyCommId &commId = context->GetCommId();
	bool fromTrp = (ctxId == 0) || ((options & 1ULL) != 0);

	PoolObjGuard<TrophyTitleStoragePoolObj> storageObj;
	PoolObjGuard<TrophyConfSourcePoolObj> confSourceObj;

	if (fromTrp) {
		ret = client->AcquireOrCreateConfSource(&commId, confSourceObj.Out());
		if (ret < 0)
			return ret;
		ret = openTrpConfSourceIfNeeded(client, handleObj->GetHandle(),
						confSourceObj->GetConfSource(), &commId, false);
		if (ret < 0)
			return ret;

		TrophyTrpFileStream *trpStream = new TrophyTrpFileStream();
		if (trpStream == NULL)
			return NP_TROPHY_ERR_OUT_OF_MEMORY;

		ret = confSourceObj->GetConfSource()->OpenGroupIconStream(
			handleObj->GetHandle(), groupId, context->GetDisplayLang(), true,
			trpStream);
		if (ret < 0) {
			delete trpStream;
			return ret;
		}

		return registerStreamObject(client, trpStream, false, 0, storageObj, confSourceObj,
					    outStreamId, outSize);
	}

	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	ret = client->AcquireTitleSession(ctxId, sessionObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireOrCreateTitleStorage(sessionObj.Get(), &commId, storageObj.Out());
	if (ret < 0)
		return ret;
	ret = openTitleStorageIfNeeded(handleObj->GetHandle(), storageObj->GetTitleStorage(),
				       0x03u);
	if (ret < 0)
		return ret;

	TrophyConfIconFile *iconFile = new TrophyConfIconFile();
	if (iconFile == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	ret = storageObj->GetTitleStorage()->OpenGroupIcon(handleObj->GetHandle(), groupId,
							   iconFile);
	if (ret < 0) {
		delete iconFile;
		return ret;
	}

	return registerStreamObject(client, iconFile, false, 0, storageObj, confSourceObj,
				    outStreamId, outSize);
}

/* 0x812C41A2 */
int npTrophyCoreOpenTrophyIconStream(SceUID pid, IPMI::Session *session, int handleId, int ctxId,
				     int mode, int trophyId, SceUInt64 options, int *outStreamId,
				     SceUInt64 *outSize)
{
	if (outStreamId == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyContextPoolObj> ctxObj;
	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireContext(mode, ctxObj.Out());
	if (ret < 0)
		return ret;

	TrophyContext *context = ctxObj->GetContext();
	const TrophyCommId &commId = context->GetCommId();
	bool fromTrp = (ctxId == 0) || ((options & 1ULL) != 0);

	PoolObjGuard<TrophyTitleStoragePoolObj> storageObj;
	PoolObjGuard<TrophyConfSourcePoolObj> confSourceObj;

	if (fromTrp) {
		ret = client->AcquireOrCreateConfSource(&commId, confSourceObj.Out());
		if (ret < 0)
			return ret;
		ret = openTrpConfSourceIfNeeded(client, handleObj->GetHandle(),
						confSourceObj->GetConfSource(), &commId, false);
		if (ret < 0)
			return ret;

		TrophyTrpFileStream *trpStream = new TrophyTrpFileStream();
		if (trpStream == NULL)
			return NP_TROPHY_ERR_OUT_OF_MEMORY;

		ret = confSourceObj->GetConfSource()->OpenTrophyIconStream(
			handleObj->GetHandle(), trophyId, trpStream);
		if (ret < 0) {
			delete trpStream;
			return ret;
		}

		return registerStreamObject(client, trpStream, false, 0, storageObj, confSourceObj,
					    outStreamId, outSize);
	}

	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	ret = client->AcquireTitleSession(ctxId, sessionObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireOrCreateTitleStorage(sessionObj.Get(), &commId, storageObj.Out());
	if (ret < 0)
		return ret;
	ret = openTitleStorageIfNeeded(handleObj->GetHandle(), storageObj->GetTitleStorage(),
				       0x03u);
	if (ret < 0)
		return ret;

	TrophyConfIconFile *iconFile = new TrophyConfIconFile();
	if (iconFile == NULL)
		return NP_TROPHY_ERR_OUT_OF_MEMORY;

	ret = storageObj->GetTitleStorage()->OpenTrophyIcon(handleObj->GetHandle(), trophyId,
							    iconFile);
	if (ret < 0) {
		delete iconFile;
		return ret;
	}

	return registerStreamObject(client, iconFile, false, 0, storageObj, confSourceObj,
				    outStreamId, outSize);
}

/* 0x812C4776 */
int npTrophyCoreReadStream(SceUID pid, IPMI::Session *session, int handleId, int streamId,
			   void *buf, SceSize bufSize, SceSize *outReadSize)
{
	if (buf == NULL || outReadSize == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyStreamPoolObj> streamObj;
	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireStream(streamId, streamObj.Out());
	if (ret < 0)
		return ret;

	return streamObj->GetStream()->Read(handleObj->GetHandle(), buf, bufSize, outReadSize);
}

/* 0x812C4C10 */
int npTrophyCoreCloseStream(SceUID pid, IPMI::Session *session, int streamId)
{
	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	ret = client->GetStreamPool().RemoveById(streamId);
	if (ret == NP_TROPHY_ERR_INVALID_HANDLE)
		return NP_TROPHY_ERR_INVALID_STREAM;
	return ret;
}

/* 0x812C4D36 */
int npTrophyCoreGetTrophyIconPath(const NpTrophyCommId *commId, int trophyId, char *outPath,
				  SceSize outPathSize)
{
	if (commId == NULL || outPath == NULL || outPathSize == 0)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyCommId localComm;
	sceClibMemcpy(localComm.data, commId->commId, sizeof(localComm.data));

	char iconName[32];
	int ret = npTrophyFormatTrophyIconFilename(iconName, trophyId, 0);
	if (ret < 0)
		return ret;

	char fullPath[256];
	ret = npTrophyUx0ConfFilePath(fullPath, 0, &localComm, iconName, "");
	if (ret < 0)
		return ret;

	SceSize len = npTrophyStrlcpy(outPath, fullPath, outPathSize);
	if (len >= outPathSize)
		return NP_TROPHY_ERR_INSUFFICIENT_BUFFER;
	return 0;
}

/* =========================================================================
 * Title/Group/Trophy data & enumeration queries (0x812C4DE0..0x812C617F)
 * ========================================================================= */

/* 0x812C4DE0 */
int npTrophyCoreGetTrophyTitleData(SceUID pid, IPMI::Session *session, int handleId, int ctxId,
				   int titleHandle, void *outInfo, void *outStats)
{
	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	PoolObjGuard<TrophyContextPoolObj> ctxObj;
	PoolObjGuard<TrophyTitleStoragePoolObj> storageObj;

	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(ctxId, sessionObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireContext(titleHandle, ctxObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireOrCreateTitleStorage(sessionObj.Get(),
						  &ctxObj->GetContext()->GetCommId(),
						  storageObj.Out());
	if (ret < 0)
		return ret;

	ret = openTitleStorageIfNeeded(handleObj->GetHandle(), storageObj->GetTitleStorage(),
				       0x03u);
	if (ret < 0)
		return ret;

	return storageObj->GetTitleStorage()->GetTitleProgressView(
		handleObj->GetHandle(), outInfo,
		static_cast<TrophyTitleProgressView *>(outStats));
}

/* 0x812C502E */
int npTrophyCoreGetTrophyGroupData(SceUID pid, IPMI::Session *session, int handleId, int ctxId,
				   int titleHandle, int groupId, void *outInfo, void *outStats)
{
	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	PoolObjGuard<TrophyContextPoolObj> ctxObj;
	PoolObjGuard<TrophyTitleStoragePoolObj> storageObj;

	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(ctxId, sessionObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireContext(titleHandle, ctxObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireOrCreateTitleStorage(sessionObj.Get(),
						  &ctxObj->GetContext()->GetCommId(),
						  storageObj.Out());
	if (ret < 0)
		return ret;

	ret = openTitleStorageIfNeeded(handleObj->GetHandle(), storageObj->GetTitleStorage(),
				       0x03u);
	if (ret < 0)
		return ret;

	return storageObj->GetTitleStorage()->GetGroupProgressView(
		handleObj->GetHandle(), groupId, outInfo,
		static_cast<TrophyGroupProgressView *>(outStats));
}

/* 0x812C5286 */
int npTrophyCoreGetTrophyData(SceUID pid, IPMI::Session *session, int handleId, int ctxId,
			      int titleHandle, int trophyId, void *outInfo, void *outStats)
{
	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	PoolObjGuard<TrophyContextPoolObj> ctxObj;
	PoolObjGuard<TrophyTitleStoragePoolObj> storageObj;

	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(ctxId, sessionObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireContext(titleHandle, ctxObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireOrCreateTitleStorage(sessionObj.Get(),
						  &ctxObj->GetContext()->GetCommId(),
						  storageObj.Out());
	if (ret < 0)
		return ret;

	ret = openTitleStorageIfNeeded(handleObj->GetHandle(), storageObj->GetTitleStorage(),
				       0x03u);
	if (ret < 0)
		return ret;

	return storageObj->GetTitleStorage()->GetTrophyStateView(
		handleObj->GetHandle(), trophyId, outInfo,
		static_cast<TrophyEntryStateView *>(outStats));
}

/* 0x812C54E0 */
int npTrophyCoreGetNextTitleEntry(SceUID pid, IPMI::Session *session, int handleId, int ctxId,
				  int cursor, int *outNextCursor, void *outEntry)
{
	if (outNextCursor == NULL || outEntry == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(ctxId, sessionObj.Out());
	if (ret < 0)
		return ret;

	TrophyUserSession *userSession = sessionObj->GetUserSession();
	ret = userSession->EnsureReady(handleObj->GetHandle(), 1);
	if (ret < 0)
		return ret;

	uint32_t foundIndex = 0;
	ret = userSession->GetIndexStorage()->EnumerateTitle(
		handleObj->GetHandle(), static_cast<uint32_t>(cursor), &foundIndex, outEntry, NULL);
	if (ret >= 0)
		*outNextCursor = static_cast<int>(foundIndex + 1);
	return ret;
}

/* 0x812C5678 */
int npTrophyCoreGetGameInfo(SceUID pid, IPMI::Session *session, int handleId, int ctxId, int mode,
			    SceUInt64 options, NpTrophyGameDetails *outDetails,
			    NpTrophyGameData *outData)
{
	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	PoolObjGuard<TrophyContextPoolObj> ctxObj;
	PoolObjGuard<TrophyTitleStoragePoolObj> storageObj;

	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(ctxId, sessionObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireContext(mode, ctxObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireOrCreateTitleStorage(sessionObj.Get(),
						  &ctxObj->GetContext()->GetCommId(),
						  storageObj.Out());
	if (ret < 0)
		return ret;

	ret = openTitleStorageIfNeeded(handleObj->GetHandle(), storageObj->GetTitleStorage(),
				       0x03u);
	if (ret < 0)
		return ret;

	if (outDetails != NULL) {
		if ((options & 1ULL) != 0) {
			PoolObjGuard<TrophyConfSourcePoolObj> confSourceObj;
			ret = client->AcquireOrCreateConfSource(&ctxObj->GetContext()->GetCommId(),
								confSourceObj.Out());
			if (ret < 0)
				return ret;
			ret = openTrpConfSourceIfNeeded(client, handleObj->GetHandle(),
							confSourceObj->GetConfSource(),
							&ctxObj->GetContext()->GetCommId(), false);
			if (ret < 0)
				return ret;
			ret = ctxObj->EnsureTrpConfLoaded(handleObj->GetHandle(),
							  confSourceObj->GetConfSource());
			if (ret < 0)
				return ret;
			ret = confSourceObj->GetConfSource()->ExtractTitleText(
				handleObj->GetHandle(), ctxObj->GetContext()->GetTrpConfHandle(),
				outDetails);
		} else {
			ret = ctxObj->EnsureStorageConfLoaded(handleObj->GetHandle(),
							      storageObj->GetTitleStorage());
			if (ret < 0)
				return ret;
			ret = storageObj->GetTitleStorage()->ExtractTitleText(
				handleObj->GetHandle(),
				ctxObj->GetContext()->GetStorageConfHandle(), outDetails);
		}
		if (ret < 0)
			return ret;
	}

	if (outData != NULL) {
		ret = storageObj->GetTitleStorage()->GetGameProgressInfo(handleObj->GetHandle(),
									 outData);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/* 0x812C5A2A */
int npTrophyCoreGetGroupInfo(SceUID pid, IPMI::Session *session, int handleId, int ctxId, int mode,
			     int groupId, SceUInt64 options, NpTrophyGroupDetails *outDetails,
			     NpTrophyGroupData *outData)
{
	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	PoolObjGuard<TrophyContextPoolObj> ctxObj;
	PoolObjGuard<TrophyTitleStoragePoolObj> storageObj;

	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(ctxId, sessionObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireContext(mode, ctxObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireOrCreateTitleStorage(sessionObj.Get(),
						  &ctxObj->GetContext()->GetCommId(),
						  storageObj.Out());
	if (ret < 0)
		return ret;

	ret = openTitleStorageIfNeeded(handleObj->GetHandle(), storageObj->GetTitleStorage(),
				       0x03u);
	if (ret < 0)
		return ret;

	if (outDetails != NULL) {
		if ((options & 1ULL) != 0) {
			PoolObjGuard<TrophyConfSourcePoolObj> confSourceObj;
			ret = client->AcquireOrCreateConfSource(&ctxObj->GetContext()->GetCommId(),
								confSourceObj.Out());
			if (ret < 0)
				return ret;
			ret = openTrpConfSourceIfNeeded(client, handleObj->GetHandle(),
							confSourceObj->GetConfSource(),
							&ctxObj->GetContext()->GetCommId(), false);
			if (ret < 0)
				return ret;
			ret = ctxObj->EnsureTrpConfLoaded(handleObj->GetHandle(),
							  confSourceObj->GetConfSource());
			if (ret < 0)
				return ret;
			ret = confSourceObj->GetConfSource()->ExtractGroupText(
				handleObj->GetHandle(), ctxObj->GetContext()->GetTrpConfHandle(),
				groupId, outDetails);
		} else {
			ret = ctxObj->EnsureStorageConfLoaded(handleObj->GetHandle(),
							      storageObj->GetTitleStorage());
			if (ret < 0)
				return ret;
			TrophyConfStream sfmStream;
			ret = storageObj->GetTitleStorage()->OpenSfmStream(handleObj->GetHandle(),
									   &sfmStream);
			if (ret >= 0) {
				ret = npTrophyExtractGroupTextInfo(
					handleObj->GetHandle(), &sfmStream,
					ctxObj->GetContext()->GetStorageConfHandle(), groupId,
					outDetails);
				sfmStream.Close();
			}
		}
		if (ret < 0)
			return ret;
	}

	if (outData != NULL) {
		ret = storageObj->GetTitleStorage()->GetGroupProgressInfo(handleObj->GetHandle(),
									  groupId, outData);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/* 0x812C5DB4 */
int npTrophyCoreGetTrophyInfo(SceUID pid, IPMI::Session *session, int handleId, int ctxId,
			      int mode, int trophyId, SceUInt64 options,
			      NpTrophyDetails *outDetails, NpTrophyData *outData)
{
	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	PoolObjGuard<TrophyContextPoolObj> ctxObj;
	PoolObjGuard<TrophyTitleStoragePoolObj> storageObj;

	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(ctxId, sessionObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireContext(mode, ctxObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireOrCreateTitleStorage(sessionObj.Get(),
						  &ctxObj->GetContext()->GetCommId(),
						  storageObj.Out());
	if (ret < 0)
		return ret;

	ret = openTitleStorageIfNeeded(handleObj->GetHandle(), storageObj->GetTitleStorage(),
				       0x03u);
	if (ret < 0)
		return ret;

	bool revealDetails = true;
	NpTrophyData localData;
	sceClibMemset(&localData, 0, sizeof(localData));
	ret = storageObj->GetTitleStorage()->GetTrophyUnlockInfo(
		handleObj->GetHandle(), trophyId, &revealDetails, &localData);
	if (ret < 0)
		return ret;

	if (outData != NULL)
		*outData = localData;

	if (outDetails != NULL) {
		if ((options & 1ULL) != 0) {
			PoolObjGuard<TrophyConfSourcePoolObj> confSourceObj;
			ret = client->AcquireOrCreateConfSource(&ctxObj->GetContext()->GetCommId(),
								confSourceObj.Out());
			if (ret < 0)
				return ret;
			ret = openTrpConfSourceIfNeeded(client, handleObj->GetHandle(),
							confSourceObj->GetConfSource(),
							&ctxObj->GetContext()->GetCommId(), false);
			if (ret < 0)
				return ret;
			ret = ctxObj->EnsureTrpConfLoaded(handleObj->GetHandle(),
							  confSourceObj->GetConfSource());
			if (ret < 0)
				return ret;
			ret = confSourceObj->GetConfSource()->ExtractTrophyText(
				handleObj->GetHandle(), ctxObj->GetContext()->GetTrpConfHandle(),
				trophyId, revealDetails, outDetails);
		} else {
			ret = ctxObj->EnsureStorageConfLoaded(handleObj->GetHandle(),
							      storageObj->GetTitleStorage());
			if (ret < 0)
				return ret;
			ret = storageObj->GetTitleStorage()->ExtractTrophyText(
				handleObj->GetHandle(),
				ctxObj->GetContext()->GetStorageConfHandle(), trophyId,
				revealDetails, outDetails);
		}
		if (ret < 0)
			return ret;
	}
	return 0;
}

/* =========================================================================
 * Title install, update, setup, and unlock operations (0x812C6180..0x812C7DFC)
 * ========================================================================= */

/* 0x812C6180 */
int npTrophyCoreGetTitleStatusForGame(SceUID pid, IPMI::Session *session, int handleId,
				      int titleSession, int ctxId, int *outStatus)
{
	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	PoolObjGuard<TrophyContextPoolObj> ctxObj;
	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(titleSession, sessionObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireContext(ctxId, ctxObj.Out());
	if (ret < 0)
		return ret;

	return checkTitleStatusForGameInternal(client, handleObj->GetHandle(), sessionObj.Get(),
					       ctxObj.Get(), outStatus);
}

/* 0x812C63D0 */
int npTrophyCoreInstallTitle(SceUID pid, IPMI::Session *session, int handleId, int titleSession,
			     int ctxId, int *outResult)
{
	if (outResult == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	PoolObjGuard<TrophyContextPoolObj> ctxObj;
	PoolObjGuard<TrophyTitleStoragePoolObj> storageObj;
	PoolObjGuard<TrophyConfSourcePoolObj> confSourceObj;

	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(titleSession, sessionObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireContext(ctxId, ctxObj.Out());
	if (ret < 0)
		return ret;

	TrophyContext *context = ctxObj->GetContext();
	const TrophyCommId &commId = context->GetCommId();

	ret = client->AcquireOrCreateConfSource(&commId, confSourceObj.Out());
	if (ret < 0)
		return ret;
	ret = openTrpConfSourceIfNeeded(client, handleObj->GetHandle(),
					confSourceObj->GetConfSource(), &commId, true);
	if (ret < 0)
		return ret;
	ret = client->AcquireOrCreateTitleStorage(sessionObj.Get(), &commId, storageObj.Out());
	if (ret < 0)
		return ret;

	int platinumId = -1;
	ret = storageObj->GetTitleStorage()->InstallFromContext(
		handleObj->GetHandle(), context, confSourceObj->GetConfSource(),
		context->GetInstallLang(), &platinumId);
	if (ret >= 0) {
		ctxObj->MarkRegistered();
		ctxObj->MarkTrpLoaded();
		*outResult = platinumId;
	}
	return ret;
}

/* 0x812C6686 */
int npTrophyCoreUpdateTitle(SceUID pid, IPMI::Session *session, int handleId, int titleSession,
			    int ctxId, int *outResult)
{
	if (outResult == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	PoolObjGuard<TrophyContextPoolObj> ctxObj;
	PoolObjGuard<TrophyTitleStoragePoolObj> storageObj;
	PoolObjGuard<TrophyConfSourcePoolObj> confSourceObj;

	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(titleSession, sessionObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireContext(ctxId, ctxObj.Out());
	if (ret < 0)
		return ret;

	TrophyContext *context = ctxObj->GetContext();
	const TrophyCommId &commId = context->GetCommId();

	ret = client->AcquireOrCreateConfSource(&commId, confSourceObj.Out());
	if (ret < 0)
		return ret;
	ret = openTrpConfSourceIfNeeded(client, handleObj->GetHandle(),
					confSourceObj->GetConfSource(), &commId, true);
	if (ret < 0)
		return ret;
	ret = client->AcquireOrCreateTitleStorage(sessionObj.Get(), &commId, storageObj.Out());
	if (ret < 0)
		return ret;

	int platinumId = -1;
	ret = storageObj->GetTitleStorage()->UpgradeFromContext(
		handleObj->GetHandle(), context, confSourceObj->GetConfSource(),
		context->GetInstallLang(), &platinumId);
	if (ret >= 0) {
		ctxObj->MarkRegistered();
		ctxObj->MarkTrpLoaded();
		*outResult = platinumId;
	}
	return ret;
}

/* 0x812C693C */
int npTrophyCoreSetupTitle(SceUID pid, IPMI::Session *session, int handleId, int titleSession,
			   int ctxId, int *outResult)
{
	if (outResult == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	PoolObjGuard<TrophyContextPoolObj> ctxObj;
	PoolObjGuard<TrophyTitleStoragePoolObj> storageObj;
	PoolObjGuard<TrophyConfSourcePoolObj> confSourceObj;

	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(titleSession, sessionObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireContext(ctxId, ctxObj.Out());
	if (ret < 0)
		return ret;

	TrophyContext *context = ctxObj->GetContext();
	const TrophyCommId &commId = context->GetCommId();

	ret = client->AcquireOrCreateConfSource(&commId, confSourceObj.Out());
	if (ret < 0)
		return ret;
	ret = openTrpConfSourceIfNeeded(client, handleObj->GetHandle(),
					confSourceObj->GetConfSource(), &commId, false);
	if (ret < 0)
		return ret;
	ret = client->AcquireOrCreateTitleStorage(sessionObj.Get(), &commId, storageObj.Out());
	if (ret < 0)
		return ret;

	int platinumId = -1;
	ret = storageObj->GetTitleStorage()->RegisterExistingContext(
		handleObj->GetHandle(), context, confSourceObj->GetConfSource(),
		context->GetInstallLang(), &platinumId);
	if (ret >= 0) {
		ctxObj->MarkRegistered();
		ctxObj->MarkTrpLoaded();
		*outResult = platinumId;
	}
	return ret;
}

/* 0x812C6BF0 */
int npTrophyCoreGetTitleFileStatus(SceUID pid, IPMI::Session *session, int handleId,
				   int titleSession, int ctxId, void *outStatus)
{
	if (outStatus == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	PoolObjGuard<TrophyContextPoolObj> ctxObj;
	PoolObjGuard<TrophyTitleStoragePoolObj> storageObj;

	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(titleSession, sessionObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireContext(ctxId, ctxObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireOrCreateTitleStorage(sessionObj.Get(),
						  &ctxObj->GetContext()->GetCommId(),
						  storageObj.Out());
	if (ret < 0)
		return ret;

	ret = openTitleStorageIfNeeded(handleObj->GetHandle(), storageObj->GetTitleStorage(),
				       0x03u);
	if (ret < 0)
		return ret;

	return storageObj->GetTitleStorage()->GetTitleInfo(handleObj->GetHandle(), outStatus);
}

/* 0x812C6F38 */
int npTrophyCoreGetTitleSyncStatus(SceUID pid, IPMI::Session *session, int handleId,
				   int titleSession, int ctxId, void *outStatus)
{
	if (outStatus == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	PoolObjGuard<TrophyContextPoolObj> ctxObj;
	PoolObjGuard<TrophyTitleStoragePoolObj> storageObj;

	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(titleSession, sessionObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireContext(ctxId, ctxObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireOrCreateTitleStorage(sessionObj.Get(),
						  &ctxObj->GetContext()->GetCommId(),
						  storageObj.Out());
	if (ret < 0)
		return ret;

	ret = openTitleStorageIfNeeded(handleObj->GetHandle(), storageObj->GetTitleStorage(),
				       0x03u);
	if (ret < 0)
		return ret;

	return storageObj->GetTitleStorage()->GetSyncInfo(handleObj->GetHandle(), outStatus);
}

/* 0x812C7178 */
int npTrophyCoreUnlockTrophy(SceUID pid, IPMI::Session *session, int handleId, int titleSession,
			     int ctxId, int trophyId, int *outPlatinumId,
			     NpTrophyGameDetails *outGameDetails,
			     NpTrophyDetails *outTrophyDetails,
			     NpTrophyDetails *outPlatinumDetails, NpTrophyCommId *outCommId,
			     int *ioUnlockedFlag)
{
	if (outPlatinumId == NULL || ioUnlockedFlag == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	PoolObjGuard<TrophyContextPoolObj> ctxObj;
	PoolObjGuard<TrophyTitleStoragePoolObj> storageObj;

	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireContext(ctxId, ctxObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(titleSession, sessionObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireOrCreateTitleStorage(sessionObj.Get(),
						  &ctxObj->GetContext()->GetCommId(),
						  storageObj.Out());
	if (ret < 0)
		return ret;

	if (!ctxObj->CanWriteStorage() || !ctxObj->GetContext()->HasCommSign())
		return NP_TROPHY_ERR_INVALID_HANDLE;
	if (!ctxObj->IsRegisteredAndLoaded())
		return NP_TROPHY_ERR_INVALID_CONTEXT;

	ret = openTitleStorageIfNeeded(handleObj->GetHandle(), storageObj->GetTitleStorage(),
				       0x03u);
	if (ret < 0)
		return ret;

	TrophyContext *context = ctxObj->GetContext();
	int platinumId = -1;
	if ((*ioUnlockedFlag & 1) == 0) {
		ret = storageObj->GetTitleStorage()->UnlockTrophy(
			handleObj->GetHandle(), context, trophyId, &platinumId);
		if (ret < 0)
			return ret;
	}

	*outPlatinumId = platinumId;
	*ioUnlockedFlag |= 1;

	if (outCommId != NULL)
		context->GetCommId().CopyTo(outCommId->commId);

	if (outGameDetails != NULL || outTrophyDetails != NULL ||
	    (platinumId >= 0 && outPlatinumDetails != NULL)) {
		if (ctxObj->EnsureStorageConfLoaded(handleObj->GetHandle(),
						    storageObj->GetTitleStorage()) >= 0) {
			const TrophyConfHandle *confHandle = context->GetStorageConfHandle();
			if (outGameDetails != NULL) {
				storageObj->GetTitleStorage()->ExtractTitleText(
					handleObj->GetHandle(), confHandle, outGameDetails);
			}
			if (outTrophyDetails != NULL) {
				storageObj->GetTitleStorage()->ExtractTrophyText(
					handleObj->GetHandle(), confHandle, trophyId, true,
					outTrophyDetails);
			}
			if (platinumId >= 0 && outPlatinumDetails != NULL) {
				storageObj->GetTitleStorage()->ExtractTrophyText(
					handleObj->GetHandle(), confHandle, platinumId, true,
					outPlatinumDetails);
			}
		}
	}

	return 0;
}

/* 0x812C74FC */
int npTrophyCoreDebugUnlockTrophy(SceUID pid, IPMI::Session *session, int handleId,
				  int titleSession, int ctxId, const SceUInt64 *debugTarget,
				  int *outPlatinumId)
{
	const TrophyDebugTrophyTarget *target =
		reinterpret_cast<const TrophyDebugTrophyTarget *>(debugTarget);

	if (target == NULL || outPlatinumId == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	PoolObjGuard<TrophyContextPoolObj> ctxObj;
	PoolObjGuard<TrophyTitleStoragePoolObj> storageObj;

	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireContext(ctxId, ctxObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(titleSession, sessionObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireOrCreateTitleStorage(sessionObj.Get(),
						  &ctxObj->GetContext()->GetCommId(),
						  storageObj.Out());
	if (ret < 0)
		return ret;

	ret = openTitleStorageIfNeeded(handleObj->GetHandle(), storageObj->GetTitleStorage(),
				       0x03u);
	if (ret < 0)
		return ret;

	return storageObj->GetTitleStorage()->DebugUnlock(handleObj->GetHandle(), target,
							  outPlatinumId);
}

/* 0x812C775E */
int npTrophyCoreDebugLockTrophy(SceUID pid, IPMI::Session *session, int handleId, int titleSession,
				int ctxId, const SceUInt64 *debugTarget, int *outPlatinumId)
{
	const TrophyDebugTrophyTarget *target =
		reinterpret_cast<const TrophyDebugTrophyTarget *>(debugTarget);

	if (target == NULL || outPlatinumId == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	PoolObjGuard<TrophyContextPoolObj> ctxObj;
	PoolObjGuard<TrophyTitleStoragePoolObj> storageObj;

	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireContext(ctxId, ctxObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(titleSession, sessionObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireOrCreateTitleStorage(sessionObj.Get(),
						  &ctxObj->GetContext()->GetCommId(),
						  storageObj.Out());
	if (ret < 0)
		return ret;

	ret = openTitleStorageIfNeeded(handleObj->GetHandle(), storageObj->GetTitleStorage(),
				       0x03u);
	if (ret < 0)
		return ret;

	return storageObj->GetTitleStorage()->DebugLock(handleObj->GetHandle(), target,
							outPlatinumId);
}

/* 0x812C79C0 */
int npTrophyCoreGetUnlockedFlagArray(SceUID pid, IPMI::Session *session, int handleId, int ctxId,
				     int mode, NpTrophyFlagArray *outFlags, SceUInt32 *outCount)
{
	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	PoolObjGuard<TrophyContextPoolObj> ctxObj;
	PoolObjGuard<TrophyTitleStoragePoolObj> storageObj;

	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(ctxId, sessionObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireContext(mode, ctxObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireOrCreateTitleStorage(sessionObj.Get(),
						  &ctxObj->GetContext()->GetCommId(),
						  storageObj.Out());
	if (ret < 0)
		return ret;

	ret = openTitleStorageIfNeeded(handleObj->GetHandle(), storageObj->GetTitleStorage(),
				       0x03u);
	if (ret < 0)
		return ret;

	NpTrophyFlagArray unlockedMask;
	NpTrophyFlagArray allMask;
	ret = storageObj->GetTitleStorage()->GetUnlockedAndAllMasks(handleObj->GetHandle(),
								    &unlockedMask, &allMask);
	if (ret < 0)
		return ret;

	if (outFlags != NULL)
		*outFlags = unlockedMask;
	if (outCount != NULL)
		*outCount = static_cast<SceUInt32>(npTrophyBitsetCount(&unlockedMask));
	return 0;
}

/* 0x812C7C88 */
int npTrophyCoreGetUserFileStatus(SceUID pid, IPMI::Session *session, int handleId, int ctxId,
				  void *outStatus)
{
	if (outStatus == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(ctxId, sessionObj.Out());
	if (ret < 0)
		return ret;

	TrophyUserSession *userSession = sessionObj->GetUserSession();
	ret = userSession->EnsureReady(handleObj->GetHandle(), 1);
	if (ret < 0)
		return ret;

	return userSession->GetIndexStorage()->GetJournalStatus(handleObj->GetHandle(), outStatus);
}

/* 0x812C7DFC */
int npTrophyCoreGetUserTitleInfo(SceUID pid, IPMI::Session *session, int handleId, int ctxId,
				 void *outInfo)
{
	if (outInfo == NULL)
		return NP_TROPHY_ERR_INVALID_ARGUMENT;

	TrophyClientState *client = NULL;
	int ret = g_npTrophyService.FindClient(pid, session, &client);
	if (ret < 0)
		return ret;

	PoolObjGuard<TrophyHandlePoolObj> handleObj;
	PoolObjGuard<TrophyTitleSessionPoolObj> sessionObj;
	ret = client->AcquireHandle(handleId, handleObj.Out());
	if (ret < 0)
		return ret;
	ret = client->AcquireTitleSession(ctxId, sessionObj.Out());
	if (ret < 0)
		return ret;

	TrophyUserSession *userSession = sessionObj->GetUserSession();
	ret = userSession->EnsureReady(handleObj->GetHandle(), 1);
	if (ret < 0)
		return ret;

	return userSession->GetIndexStorage()->GetCounts(handleObj->GetHandle(), outInfo);
}
