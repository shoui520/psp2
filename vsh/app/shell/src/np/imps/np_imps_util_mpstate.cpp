/*
 * np_imps_util_mpstate.cpp: SceNpImpsUtil per-process HSM ("SceNpImpsUtilMPState"),
 *                           initial event snapshot replay, and IPC method handlers
 *                           (0x812F4C44..0x812F7FCF, 0x81438ECC..0x8143D91F).
 */
#include <stddef.h>
#include <std/new>
#include <std/string.h>
#include <scetypes.h>
#include <kernel.h>
#include <libkernel/libkernel.h>
#include <common/shared_ptr.h>
#include <job/job.h>
#include "np_imps_util.h"

/* External shell / NP / AppMgr bridges */
extern "C" {
extern int sceAppMgrGetStatusByAppId(int appId, void *appStatus);
extern int vshKernelGetCompiledSdkVersionByPid(SceUID processId, SceUInt32 *outSdkVersion);
extern int sceNpGetPlatformType(const SceNpId *npId);
extern int SceNpCommon_256670D6(SceNpId *npId, int platformType);
extern int SceNpCommon_E426FEE8(const SceNpCommunicationId *commId, char *outStr, SceSize maxLen);
extern int SceNpManager_9554802B(const char *tag, const char *detail, const char *extra);
}

/* SceNpImpsCore bridges (implemented in Batch G: np_imps_core) */
typedef void (*NpImpsCoreConnectCallback)(SceUID processId, int errorCode);
typedef void (*NpImpsCoreSendMsgCallback)(int result, void *userArg);

struct NpImpsCoreInGameMessage {
	SceUInt32 flags;			/* +0x00 */
	SceUInt32 reservedWords[3];		/* +0x04 */
	NpImpsCoreDataNode *dataNode;		/* +0x10 */
	SceUInt32 reservedTail[6];		/* +0x14 */
};

extern int npImpsCoreConnectProcess(SceUID processId, NpImpsCoreConnectCallback cb);	/* 0x8131B526 */
extern int npImpsCoreDisconnectProcess(SceUID processId);				/* 0x8131B5B4 */
extern int npImpsCoreSendInGameMessage(const SceNpId *targetNpId,
				       const NpImpsCoreInGameMessage *msg,
				       NpImpsCoreSendMsgCallback cb,
				       void *userArg);					/* 0x8131B706 */

/* SceNpManager / SceNpCommonPs4 / Shell job-queue bridges */
extern int npAccountGetLegacySignInStatus(SceUInt32 sdkVersion);			/* 0x81326D30 */
extern paf::job::JobQueue *shellGetNpBackgroundJobQueue(void);				/* 0x813C5BE0 */
extern int npPushNotifyAppOnline(int appId);						/* 0x813B9D0C */
extern void npPushNotifyAppExit(void);							/* 0x813B9D1A */
extern int npPushNotifySignedOut(void);							/* 0x813B9D22 */

/* NpFriendStorage play session log bridges (vsh/app/shell/np_friend_list.cpp) */
extern int npFriendSessionLogAdd(const paf::rtc::Tick *date, const SceNpId *npId,
				 const SceNpCommunicationId *commId, const char *description,
				 SceUID processId);
extern int npFriendSessionLogGetCount(int queryKind, const SceNpCommunicationId *commId,
				      unsigned int *outCount);
extern int npFriendSessionLogGetList(int queryKind, const SceNpCommunicationId *commId,
				     unsigned int startIndex, void *outEntries,
				     unsigned int maxCount, unsigned int *outCount);
extern int npFriendSessionLogDelete(SceUInt32 dateLow, SceUInt32 dateHigh);
extern int npFriendSessionLogDeleteAll(void);
extern int npFriendPlayerSessionLogGetCount(const SceNpId *npId, unsigned int *outCount);
extern int npFriendPlayerSessionLogGet(const SceNpId *npId, unsigned int index, void *outEntry);

/* AppMgr status struct layout used by ProcessHsm constructor (128 bytes) */
struct NpImpsUtilAppStatus {
	SceUInt32 headerWords[3];
	char titleId[10];
	char remainder[106];
};

/* ============================================================================
 * Asynchronous "SceNpImpsUtlSetGameTitle" job item (0x812F75F8, 0x8143CEE8)
 * ============================================================================ */

class NpImpsUtilSetGameTitleJob : public paf::job::JobItem {
public:
	NpImpsUtilSetGameTitleJob(int appId)
		: paf::job::JobItem("SceNpImpsUtlSetGameTitle", NULL),
		  m_appId(appId)
	{
	}

	virtual ~NpImpsUtilSetGameTitleJob() {}

	virtual int Run()
	{
		npImpsSvcSetGameTitle(m_appId);
		return 0;
	}

	virtual void Finish(int result)
	{
		(void)result;
	}

private:
	int m_appId;
};

/* ============================================================================
 * Per-process HSM state enum and class (0x812F6CE4..0x812F7E1B, 0x81438ECC..0x8143C62C)
 * ============================================================================ */

enum NpImpsHsmState {
	NP_IMPS_HSM_START = 0,
	NP_IMPS_HSM_SIGNED_OUT,
	NP_IMPS_HSM_SIGNED_IN,
	NP_IMPS_HSM_ENTERING_ONLINE,
	NP_IMPS_HSM_PSEUDO_SIGNED_IN,
	NP_IMPS_HSM_IPC_QUEUE_RECOVERING,
	NP_IMPS_HSM_ONLINE,
	NP_IMPS_HSM_NP_COMM_ID_SET,
	NP_IMPS_HSM_NOT_BIG_SIGNED_OUT,
	NP_IMPS_HSM_NOT_BIG_SIGNED_IN,
	NP_IMPS_HSM_NOT_BIG_ENTERING_ONLINE,
	NP_IMPS_HSM_NOT_BIG_PSEUDO_SIGNED_IN,
	NP_IMPS_HSM_NOT_BIG_IPC_RECOVERING,
	NP_IMPS_HSM_NOT_BIG_ONLINE,
	NP_IMPS_HSM_END
};

enum NpImpsPendingGamePresenceOp {
	NP_IMPS_PENDING_PRESENCE_NONE	= 0,
	NP_IMPS_PENDING_PRESENCE_SET	= 1,
	NP_IMPS_PENDING_PRESENCE_CLEAR	= 2
};

class NpImpsProcessHsm {
public:
	NpImpsProcessHsm(SceUID processId, int appId, NpImpsUtilMpState *owner);
	~NpImpsProcessHsm();

	SceUID GetProcessId() const { return m_processId; }
	int GetAppId() const { return m_appId; }
	bool IsEnded() const { return m_state == NP_IMPS_HSM_END; }

	void DispatchEvent(int eventId, int errorCode);
	int GetServiceState(SceUInt8 rawMode);
	void QueueSetGamePresence(const NpImpsGamePresenceInput *input);
	void QueueClearGamePresence();

	void LockState() { m_hsmMutex.Lock(); }
	void UnlockState() { m_hsmMutex.Unlock(); }

	/* Per-process SceNpImpsSvc listener callbacks (0x812F6CE4..0x812F758F) */
	static void OnSignedInMyOnlineStatus(int status, void *userArg);
	static void OnSignedInFriendList(int eventType, const SceNpId *npIds,
					 NpImpsCacheFriendPtr **handles, SceSize count,
					 void *userArg);
	static void OnSignedInBlockList(int eventType, const SceNpId *npIds, SceSize count,
					void *userArg);
	static void OnSignedInFriendRemoved(const SceNpId *npIds, SceSize count, void *userArg);
	static void OnSignedInFriendRequestRemoved(const SceNpId *npIds, SceSize count,
						   void *userArg);
	static void OnSignedInMyPresence(int eventType, const NpImpsSvcPresenceSnapshot *curSnap,
					 const NpImpsSvcPresenceSnapshot *prevSnap, void *userArg);

	static void OnOnlineFriendPresence(int eventType, NpImpsCacheFriendPtr *handle,
					   const SceNpId *npId,
					   const NpImpsSvcPresenceSnapshot *curSnap,
					   const NpImpsSvcPresenceSnapshot *prevSnap,
					   SceUInt32 diffMask, void *userArg);
	static void OnOnlinePlatformFriendPresence(int eventType, NpImpsCacheFriendPtr *handle,
						   const SceNpId *npId,
						   const NpImpsSvcPresenceSnapshot *curSnap,
						   const NpImpsSvcPresenceSnapshot *prevSnap,
						   SceUInt32 diffMask, int platform, void *userArg);
	static void OnOnlineFriendContext(int eventType, NpImpsCacheFriendPtr *handle,
					  const SceNpId *npId, int contextState,
					  void *extra, void *userArg);
	static void OnOnlineInGameDataMessage(int eventType, const SceNpId *senderNpId,
					      const NpImpsSvcInGameDataPayload *payload,
					      void *userArg);

private:
	void EnterInitialState();
	void TransitionTo(NpImpsHsmState target);
	void ExitStateHierarchy(NpImpsHsmState cur, NpImpsHsmState commonAncestor);
	void EnterStateHierarchy(NpImpsHsmState commonAncestor, NpImpsHsmState target);

	static NpImpsHsmState GetParentState(NpImpsHsmState state);
	static bool IsDescendantOrEqual(NpImpsHsmState state, NpImpsHsmState ancestor);

	void OnEnterState(NpImpsHsmState state);
	void OnExitState(NpImpsHsmState state);

	/* Leaf actions matching 0x812F7590..0x812F7BC9 */
	void NotifyServiceState(int serviceState);
	void SetLocalServiceState(int serviceState);
	void SetActiveProcess();
	void EnqueueSetGameTitleJob();
	void ClearGameTitle();
	bool IsFriendsApp() const;
	void RegisterOnlineListener();
	void UnregisterOnlineListener();
	void RegisterSignedInListener();
	void UnregisterSignedInListener();
	void ResetFriendPresenceToOffline();
	void ClearGameEventQueue();
	void PostQueueRecoveredEvent();
	void SendEmptyListSnapshots();
	bool IsBigAppGame() const;
	bool HasActiveCommId() const;
	bool IsGlobalStateOnline() const;
	void FlushPendingGamePresence();
	void CompletePendingEnterOnlineSuccess();
	void DisconnectCoreProcess();
	void CompletePendingEnterOnlineResult();
	void NotifyPushOnline();
	void NotifyPushAppExitIfOnline();
	void NotifyPushSignedOut();

	paf::thread::RMutex m_hsmMutex;
	NpImpsHsmState m_state;
	int m_lastErrorCode;
	SceUID m_processId;
	int m_appId;
	char m_titleId[10];
	paf::thread::RMutex m_serviceStateMutex;
	int m_serviceState;
	NpImpsUtilMpState *m_owner;
	NpImpsSvcListenerCallbacks m_onlineListenerCb;
	SceUInt32 m_onlineListenerId;
	NpImpsSvcListenerCallbacks m_signedInListenerCb;
	SceUInt32 m_signedInListenerId;
	int m_pendingPresenceOp;
	NpImpsGamePresenceInput m_pendingGamePresence;
};

/* ============================================================================
 * MPState worker thread ("SceNpImpsUtilMPState", 0x812F4C44..0x812F5833)
 * ============================================================================ */

class NpImpsUtilMpState : public paf::thread::Thread {
public:
	NpImpsUtilMpState();
	virtual ~NpImpsUtilMpState();
	virtual void EntryFunction();

	int OnProcessCreated(SceUID processId, int appId);
	int RemoveProcess(SceUID processId);
	int PostEvent(int eventId, SceUID processId, int errorCode);
	int GetServiceState(SceUID processId, int *outState, SceUInt8 rawMode);
	int LockProcess(SceUID processId);
	int UnlockProcess(SceUID processId);
	int SetPendingGamePresence(SceUID processId, const NpImpsGamePresenceInput *input);
	int ClearPendingGamePresence(SceUID processId);
	int SetConnectError(SceUID processId, int errorCode);
	int TakeConnectError(SceUID processId, int *outErrorCode);

private:
	struct ProcessNode {
		ProcessNode *next;
		SceUID processId;
		NpImpsProcessHsm *hsm;
	};

	struct ConnectErrorNode {
		ConnectErrorNode *next;
		SceUID processId;
		int errorCode;
	};

	struct EventItem {
		int eventId;
		SceUID processId;
		int errorCode;
	};

	struct EventNode {
		EventNode *next;
		EventItem item;
	};

	NpImpsProcessHsm *FindProcessLocked(SceUID processId);
	bool PopEvent(EventItem *outItem, SceUInt32 *outRemaining);
	int DispatchBroadcastEvent(int eventId, int errorCode);
	int DispatchProcessEvent(int eventId, SceUID processId, int errorCode);

	paf::thread::RMutex m_mapMutex;
	ProcessNode *m_processes;
	ConnectErrorNode *m_connectErrors;
	paf::thread::Mutex m_condMutex;
	paf::thread::Cond m_cond;
	paf::thread::RMutex m_queueMutex;
	EventNode *m_queueHead;
	EventNode *m_queueTail;
	SceUInt32 m_queueCount;
};

static NpImpsUtilMpState *s_mpStateThread = NULL;	/* 0x815460F8 */

/* Forward declarations for connect/async helpers */
static int completeEnterOnlineAsync(SceUID processId, int errorCode);
static void onCoreConnectComplete(SceUID processId, int errorCode);
static void onCoreConnectNoop(SceUID processId, int errorCode)
{
	(void)processId;
	(void)errorCode;
}
static void onCoreSendInGameMsgNoop(int result, void *userArg)
{
	(void)result;
	(void)userArg;
}

/* ============================================================================
 * NpImpsProcessHsm implementation
 * ============================================================================ */

/* 0x81438ECC */
NpImpsProcessHsm::NpImpsProcessHsm(SceUID processId, int appId, NpImpsUtilMpState *owner)
	: m_hsmMutex("SceNpImpsUtilityHsmRMtx", 0),
	  m_state(NP_IMPS_HSM_START),
	  m_lastErrorCode(0),
	  m_processId(processId),
	  m_appId(appId),
	  m_serviceStateMutex("SceNpServiceStateMutex", 0),
	  m_serviceState(0),
	  m_owner(owner),
	  m_onlineListenerId(0),
	  m_signedInListenerId(0),
	  m_pendingPresenceOp(NP_IMPS_PENDING_PRESENCE_NONE)
{
	sce_paf_memset(m_titleId, 0, sizeof(m_titleId));
	sce_paf_memset(&m_onlineListenerCb, 0, sizeof(m_onlineListenerCb));
	m_onlineListenerCb.onFriendPresence = OnOnlineFriendPresence;
	m_onlineListenerCb.onInGameDataMessage = OnOnlineInGameDataMessage;
	m_onlineListenerCb.onPlatformFriendPresence = OnOnlinePlatformFriendPresence;
	m_onlineListenerCb.onFriendContext = OnOnlineFriendContext;
	m_onlineListenerCb.userArg = this;

	sce_paf_memset(&m_signedInListenerCb, 0, sizeof(m_signedInListenerCb));
	m_signedInListenerCb.onMyOnlineStatus = OnSignedInMyOnlineStatus;
	m_signedInListenerCb.onFriendList = OnSignedInFriendList;
	m_signedInListenerCb.onBlockList = OnSignedInBlockList;
	m_signedInListenerCb.onFriendRemoved = OnSignedInFriendRemoved;
	m_signedInListenerCb.onMyPresence = OnSignedInMyPresence;
	m_signedInListenerCb.onFriendRequestRemoved = OnSignedInFriendRequestRemoved;
	m_signedInListenerCb.userArg = this;

	sce_paf_memset(&m_pendingGamePresence, 0, sizeof(m_pendingGamePresence));

	NpImpsUtilAppStatus appStatus;
	sce_paf_memset(&appStatus, 0, sizeof(appStatus));
	sceAppMgrGetStatusByAppId(m_appId, &appStatus);
	sce_paf_memcpy(m_titleId, appStatus.titleId, sizeof(m_titleId) - 1);
	m_titleId[sizeof(m_titleId) - 1] = '\0';

	EnterInitialState();
}

NpImpsProcessHsm::~NpImpsProcessHsm()
{
	if (m_onlineListenerId != 0)
		UnregisterOnlineListener();
	if (m_signedInListenerId != 0)
		UnregisterSignedInListener();
}

/* 0x812F7BCA */
void NpImpsProcessHsm::EnterInitialState()
{
	npImpsSvcLockFriendStore();
	npImpsSvcLockBlockStore();
	npImpsSvcLockServiceState();

	int globalState = NP_IMPS_SVC_STATE_SIGNED_OUT;
	npImpsSvcGetServiceState(&globalState);

	SceUInt8 bigAppGame = 0;
	npImpsSvcIsBigAppGame(m_appId, &bigAppGame);

	if (bigAppGame != 0) {
		if (globalState == NP_IMPS_SVC_STATE_SIGNED_OUT)
			TransitionTo(NP_IMPS_HSM_SIGNED_OUT);
		else
			TransitionTo(NP_IMPS_HSM_SIGNED_IN);
	} else {
		if (globalState == NP_IMPS_SVC_STATE_SIGNED_OUT)
			TransitionTo(NP_IMPS_HSM_NOT_BIG_SIGNED_OUT);
		else
			TransitionTo(NP_IMPS_HSM_NOT_BIG_SIGNED_IN);
	}

	npImpsSvcUnlockServiceState();
	npImpsSvcUnlockBlockStore();
	npImpsSvcUnlockFriendStore();
}

NpImpsHsmState NpImpsProcessHsm::GetParentState(NpImpsHsmState state)
{
	switch (state) {
	case NP_IMPS_HSM_SIGNED_OUT:
	case NP_IMPS_HSM_SIGNED_IN:
	case NP_IMPS_HSM_NOT_BIG_SIGNED_OUT:
	case NP_IMPS_HSM_NOT_BIG_SIGNED_IN:
		return NP_IMPS_HSM_START;
	case NP_IMPS_HSM_ENTERING_ONLINE:
	case NP_IMPS_HSM_PSEUDO_SIGNED_IN:
	case NP_IMPS_HSM_IPC_QUEUE_RECOVERING:
	case NP_IMPS_HSM_ONLINE:
		return NP_IMPS_HSM_SIGNED_IN;
	case NP_IMPS_HSM_NP_COMM_ID_SET:
		return NP_IMPS_HSM_ONLINE;
	case NP_IMPS_HSM_NOT_BIG_ENTERING_ONLINE:
	case NP_IMPS_HSM_NOT_BIG_PSEUDO_SIGNED_IN:
	case NP_IMPS_HSM_NOT_BIG_IPC_RECOVERING:
	case NP_IMPS_HSM_NOT_BIG_ONLINE:
		return NP_IMPS_HSM_NOT_BIG_SIGNED_IN;
	default:
		return NP_IMPS_HSM_START;
	}
}

bool NpImpsProcessHsm::IsDescendantOrEqual(NpImpsHsmState state, NpImpsHsmState ancestor)
{
	NpImpsHsmState cur = state;
	while (true) {
		if (cur == ancestor)
			return true;
		if (cur == NP_IMPS_HSM_START || cur == NP_IMPS_HSM_END)
			return false;
		cur = GetParentState(cur);
	}
}

static bool isBigAppState(NpImpsHsmState state)
{
	return state >= NP_IMPS_HSM_SIGNED_OUT && state <= NP_IMPS_HSM_NP_COMM_ID_SET;
}

static bool isNotBigAppState(NpImpsHsmState state)
{
	return state >= NP_IMPS_HSM_NOT_BIG_SIGNED_OUT && state <= NP_IMPS_HSM_NOT_BIG_ONLINE;
}

static bool isRunningState(NpImpsHsmState state)
{
	return isBigAppState(state) || isNotBigAppState(state);
}

void NpImpsProcessHsm::OnEnterState(NpImpsHsmState state)
{
	switch (state) {
	case NP_IMPS_HSM_SIGNED_OUT:
	case NP_IMPS_HSM_NOT_BIG_SIGNED_OUT:
		SetLocalServiceState(NP_IMPS_SVC_STATE_SIGNED_OUT);
		NotifyServiceState(NP_IMPS_SVC_STATE_SIGNED_OUT);
		break;

	case NP_IMPS_HSM_SIGNED_IN:
	case NP_IMPS_HSM_NOT_BIG_SIGNED_IN:
		SetLocalServiceState(NP_IMPS_SVC_STATE_SIGNED_IN);
		NotifyServiceState(NP_IMPS_SVC_STATE_SIGNED_IN);
		RegisterSignedInListener();
		ResetFriendPresenceToOffline();
		break;

	case NP_IMPS_HSM_IPC_QUEUE_RECOVERING:
	case NP_IMPS_HSM_NOT_BIG_IPC_RECOVERING:
		UnregisterSignedInListener();
		ClearGameEventQueue();
		PostQueueRecoveredEvent();
		break;

	case NP_IMPS_HSM_ONLINE:
		SetLocalServiceState(NP_IMPS_SVC_STATE_ONLINE);
		NotifyServiceState(NP_IMPS_SVC_STATE_ONLINE);
		RegisterOnlineListener();
		FlushPendingGamePresence();
		CompletePendingEnterOnlineResult();
		break;

	case NP_IMPS_HSM_NOT_BIG_ONLINE:
		SetLocalServiceState(NP_IMPS_SVC_STATE_ONLINE);
		NotifyServiceState(NP_IMPS_SVC_STATE_ONLINE);
		RegisterOnlineListener();
		CompletePendingEnterOnlineResult();
		break;

	case NP_IMPS_HSM_NP_COMM_ID_SET:
		npImpsSvcPublishMyPresence();
		break;

	default:
		break;
	}
}

void NpImpsProcessHsm::OnExitState(NpImpsHsmState state)
{
	switch (state) {
	case NP_IMPS_HSM_SIGNED_IN:
		UnregisterSignedInListener();
		SendEmptyListSnapshots();
		NotifyPushSignedOut();
		break;

	case NP_IMPS_HSM_NOT_BIG_SIGNED_IN:
		UnregisterSignedInListener();
		SendEmptyListSnapshots();
		break;

	case NP_IMPS_HSM_IPC_QUEUE_RECOVERING:
		SendEmptyListSnapshots();
		RegisterSignedInListener();
		ResetFriendPresenceToOffline();
		break;

	case NP_IMPS_HSM_NOT_BIG_IPC_RECOVERING:
		RegisterSignedInListener();
		ResetFriendPresenceToOffline();
		break;

	case NP_IMPS_HSM_ONLINE:
	case NP_IMPS_HSM_NOT_BIG_ONLINE:
		DisconnectCoreProcess();
		SetLocalServiceState(NP_IMPS_SVC_STATE_SIGNED_IN);
		NotifyServiceState(NP_IMPS_SVC_STATE_SIGNED_IN);
		UnregisterOnlineListener();
		ResetFriendPresenceToOffline();
		break;

	case NP_IMPS_HSM_NP_COMM_ID_SET:
		npImpsSvcRepublishMyPresenceWithoutCommId();
		break;

	default:
		break;
	}
}

void NpImpsProcessHsm::ExitStateHierarchy(NpImpsHsmState cur, NpImpsHsmState commonAncestor)
{
	while (cur != commonAncestor && cur != NP_IMPS_HSM_START && cur != NP_IMPS_HSM_END) {
		OnExitState(cur);
		cur = GetParentState(cur);
	}
}

void NpImpsProcessHsm::EnterStateHierarchy(NpImpsHsmState commonAncestor, NpImpsHsmState target)
{
	NpImpsHsmState path[6];
	int depth = 0;
	NpImpsHsmState cur = target;
	while (cur != commonAncestor && cur != NP_IMPS_HSM_START && depth < 6) {
		path[depth++] = cur;
		cur = GetParentState(cur);
	}
	for (int idx = depth - 1; idx >= 0; --idx)
		OnEnterState(path[idx]);
}

/* 0x8143A4A8 */
void NpImpsProcessHsm::TransitionTo(NpImpsHsmState target)
{
	NpImpsHsmState prev = m_state;
	if (prev == target) {
		OnExitState(prev);
		OnEnterState(target);
		return;
	}

	bool wasBigApp = isBigAppState(prev);
	bool willBeBigApp = isBigAppState(target);
	bool wasRunning = isRunningState(prev);
	bool willBeRunning = isRunningState(target);

	/* Find lowest common ancestor within the same branch */
	NpImpsHsmState common = NP_IMPS_HSM_START;
	if ((wasBigApp && willBeBigApp) || (isNotBigAppState(prev) && isNotBigAppState(target))) {
		NpImpsHsmState anc = prev;
		while (anc != NP_IMPS_HSM_START) {
			if (IsDescendantOrEqual(target, anc)) {
				common = anc;
				break;
			}
			anc = GetParentState(anc);
		}
	}

	ExitStateHierarchy(prev, common);

	/* Superstate exits: BigAppGame and ProcessRunning */
	if (wasBigApp && !willBeBigApp)
		ClearGameTitle();
	if (wasRunning && !willBeRunning) {
		if (IsFriendsApp())
			npImpsSvcClearPartyPresence();
	}

	/* Superstate entries: BigAppGame */
	if (!wasBigApp && willBeBigApp) {
		SetActiveProcess();
		EnqueueSetGameTitleJob();
	}

	EnterStateHierarchy(common, target);

	/* Automatic initial-substate completion transitions */
	NpImpsHsmState settled = target;
	if (settled == NP_IMPS_HSM_SIGNED_IN && IsGlobalStateOnline()) {
		NotifyPushOnline();
		settled = NP_IMPS_HSM_PSEUDO_SIGNED_IN;
		OnEnterState(settled);
	} else if (settled == NP_IMPS_HSM_NOT_BIG_SIGNED_IN && IsGlobalStateOnline()) {
		settled = NP_IMPS_HSM_NOT_BIG_PSEUDO_SIGNED_IN;
		OnEnterState(settled);
	} else if (settled == NP_IMPS_HSM_ONLINE && IsBigAppGame() && HasActiveCommId()) {
		settled = NP_IMPS_HSM_NP_COMM_ID_SET;
		OnEnterState(settled);
	}

	m_state = settled;
}

/* 0x8143A6AC */
void NpImpsProcessHsm::DispatchEvent(int eventId, int errorCode)
{
	m_hsmMutex.Lock();
	m_lastErrorCode = errorCode;

	switch (m_state) {
	case NP_IMPS_HSM_SIGNED_OUT:
		if (eventId == NP_IMPS_MP_EVT_SIGN_IN)
			TransitionTo(NP_IMPS_HSM_SIGNED_IN);
		else if (eventId == NP_IMPS_MP_EVT_PROCESS_TERMINATE)
			TransitionTo(NP_IMPS_HSM_END);
		break;

	case NP_IMPS_HSM_SIGNED_IN:
		if (eventId == NP_IMPS_MP_EVT_SIGN_OUT) {
			TransitionTo(NP_IMPS_HSM_SIGNED_OUT);
		} else if (eventId == NP_IMPS_MP_EVT_IPC_QUEUE_OVERFLOW) {
			TransitionTo(NP_IMPS_HSM_IPC_QUEUE_RECOVERING);
		} else if (eventId == NP_IMPS_MP_EVT_ENTER_ONLINE_REQ) {
			TransitionTo(NP_IMPS_HSM_ENTERING_ONLINE);
		} else if (eventId == NP_IMPS_MP_EVT_CORE_ONLINE) {
			NotifyPushOnline();
			TransitionTo(NP_IMPS_HSM_PSEUDO_SIGNED_IN);
		} else if (eventId == NP_IMPS_MP_EVT_PROCESS_TERMINATE) {
			TransitionTo(NP_IMPS_HSM_END);
		}
		break;

	case NP_IMPS_HSM_ENTERING_ONLINE:
		if (eventId == NP_IMPS_MP_EVT_ENTER_ONLINE_FAIL ||
		    eventId == NP_IMPS_MP_EVT_LEAVE_ONLINE_REQ) {
			CompletePendingEnterOnlineResult();
			TransitionTo(NP_IMPS_HSM_SIGNED_IN);
		} else if (eventId == NP_IMPS_MP_EVT_CORE_ONLINE) {
			CompletePendingEnterOnlineSuccess();
			NotifyPushOnline();
			TransitionTo(NP_IMPS_HSM_ONLINE);
		} else if (eventId == NP_IMPS_MP_EVT_SIGN_OUT) {
			TransitionTo(NP_IMPS_HSM_SIGNED_OUT);
		} else if (eventId == NP_IMPS_MP_EVT_IPC_QUEUE_OVERFLOW) {
			TransitionTo(NP_IMPS_HSM_IPC_QUEUE_RECOVERING);
		} else if (eventId == NP_IMPS_MP_EVT_PROCESS_TERMINATE) {
			TransitionTo(NP_IMPS_HSM_END);
		}
		break;

	case NP_IMPS_HSM_PSEUDO_SIGNED_IN:
		if (eventId == NP_IMPS_MP_EVT_SIGN_IN) {
			NotifyPushSignedOut();
			TransitionTo(NP_IMPS_HSM_SIGNED_IN);
		} else if (eventId == NP_IMPS_MP_EVT_ENTER_ONLINE_REQ) {
			TransitionTo(NP_IMPS_HSM_ONLINE);
		} else if (eventId == NP_IMPS_MP_EVT_PROCESS_TERMINATE) {
			NotifyPushAppExitIfOnline();
			TransitionTo(NP_IMPS_HSM_END);
		} else if (eventId == NP_IMPS_MP_EVT_SIGN_OUT) {
			TransitionTo(NP_IMPS_HSM_SIGNED_OUT);
		} else if (eventId == NP_IMPS_MP_EVT_IPC_QUEUE_OVERFLOW) {
			TransitionTo(NP_IMPS_HSM_IPC_QUEUE_RECOVERING);
		}
		break;

	case NP_IMPS_HSM_IPC_QUEUE_RECOVERING:
		if (eventId == NP_IMPS_MP_EVT_IPC_QUEUE_OVERFLOW) {
			/* Already recovering; ignore duplicate overflow */
		} else if (eventId == NP_IMPS_MP_EVT_IPC_QUEUE_RECOVERED) {
			if (IsGlobalStateOnline())
				TransitionTo(NP_IMPS_HSM_PSEUDO_SIGNED_IN);
			else
				TransitionTo(NP_IMPS_HSM_SIGNED_IN);
		} else if (eventId == NP_IMPS_MP_EVT_SIGN_OUT) {
			TransitionTo(NP_IMPS_HSM_SIGNED_OUT);
		} else if (eventId == NP_IMPS_MP_EVT_PROCESS_TERMINATE) {
			TransitionTo(NP_IMPS_HSM_END);
		}
		break;

	case NP_IMPS_HSM_ONLINE:
		if (eventId == NP_IMPS_MP_EVT_SIGN_IN) {
			NotifyPushSignedOut();
			TransitionTo(NP_IMPS_HSM_SIGNED_IN);
		} else if (eventId == NP_IMPS_MP_EVT_IPC_QUEUE_OVERFLOW) {
			TransitionTo(NP_IMPS_HSM_IPC_QUEUE_RECOVERING);
		} else if (eventId == NP_IMPS_MP_EVT_SET_COMM_ID) {
			TransitionTo(NP_IMPS_HSM_NP_COMM_ID_SET);
		} else if (eventId == NP_IMPS_MP_EVT_LEAVE_ONLINE_REQ) {
			TransitionTo(NP_IMPS_HSM_PSEUDO_SIGNED_IN);
		} else if (eventId == NP_IMPS_MP_EVT_PROCESS_TERMINATE) {
			NotifyPushAppExitIfOnline();
			TransitionTo(NP_IMPS_HSM_END);
		} else if (eventId == NP_IMPS_MP_EVT_SIGN_OUT) {
			TransitionTo(NP_IMPS_HSM_SIGNED_OUT);
		}
		break;

	case NP_IMPS_HSM_NP_COMM_ID_SET:
		if (eventId == NP_IMPS_MP_EVT_CLEAR_COMM_ID) {
			TransitionTo(NP_IMPS_HSM_ONLINE);
		} else if (eventId == NP_IMPS_MP_EVT_SIGN_IN) {
			NotifyPushSignedOut();
			TransitionTo(NP_IMPS_HSM_SIGNED_IN);
		} else if (eventId == NP_IMPS_MP_EVT_IPC_QUEUE_OVERFLOW) {
			TransitionTo(NP_IMPS_HSM_IPC_QUEUE_RECOVERING);
		} else if (eventId == NP_IMPS_MP_EVT_LEAVE_ONLINE_REQ) {
			TransitionTo(NP_IMPS_HSM_PSEUDO_SIGNED_IN);
		} else if (eventId == NP_IMPS_MP_EVT_PROCESS_TERMINATE) {
			NotifyPushAppExitIfOnline();
			TransitionTo(NP_IMPS_HSM_END);
		} else if (eventId == NP_IMPS_MP_EVT_SIGN_OUT) {
			TransitionTo(NP_IMPS_HSM_SIGNED_OUT);
		}
		break;

	case NP_IMPS_HSM_NOT_BIG_SIGNED_OUT:
		if (eventId == NP_IMPS_MP_EVT_SIGN_IN)
			TransitionTo(NP_IMPS_HSM_NOT_BIG_SIGNED_IN);
		else if (eventId == NP_IMPS_MP_EVT_PROCESS_TERMINATE)
			TransitionTo(NP_IMPS_HSM_END);
		break;

	case NP_IMPS_HSM_NOT_BIG_SIGNED_IN:
		if (eventId == NP_IMPS_MP_EVT_SIGN_OUT) {
			TransitionTo(NP_IMPS_HSM_NOT_BIG_SIGNED_OUT);
		} else if (eventId == NP_IMPS_MP_EVT_IPC_QUEUE_OVERFLOW) {
			TransitionTo(NP_IMPS_HSM_NOT_BIG_IPC_RECOVERING);
		} else if (eventId == NP_IMPS_MP_EVT_ENTER_ONLINE_REQ) {
			TransitionTo(NP_IMPS_HSM_NOT_BIG_ENTERING_ONLINE);
		} else if (eventId == NP_IMPS_MP_EVT_CORE_ONLINE) {
			TransitionTo(NP_IMPS_HSM_NOT_BIG_PSEUDO_SIGNED_IN);
		} else if (eventId == NP_IMPS_MP_EVT_PROCESS_TERMINATE) {
			TransitionTo(NP_IMPS_HSM_END);
		}
		break;

	case NP_IMPS_HSM_NOT_BIG_ENTERING_ONLINE:
		if (eventId == NP_IMPS_MP_EVT_ENTER_ONLINE_FAIL ||
		    eventId == NP_IMPS_MP_EVT_LEAVE_ONLINE_REQ) {
			CompletePendingEnterOnlineResult();
			TransitionTo(NP_IMPS_HSM_NOT_BIG_SIGNED_IN);
		} else if (eventId == NP_IMPS_MP_EVT_CORE_ONLINE) {
			CompletePendingEnterOnlineSuccess();
			TransitionTo(NP_IMPS_HSM_NOT_BIG_ONLINE);
		} else if (eventId == NP_IMPS_MP_EVT_SIGN_OUT) {
			TransitionTo(NP_IMPS_HSM_NOT_BIG_SIGNED_OUT);
		} else if (eventId == NP_IMPS_MP_EVT_IPC_QUEUE_OVERFLOW) {
			TransitionTo(NP_IMPS_HSM_NOT_BIG_IPC_RECOVERING);
		} else if (eventId == NP_IMPS_MP_EVT_PROCESS_TERMINATE) {
			TransitionTo(NP_IMPS_HSM_END);
		}
		break;

	case NP_IMPS_HSM_NOT_BIG_PSEUDO_SIGNED_IN:
		if (eventId == NP_IMPS_MP_EVT_SIGN_IN) {
			TransitionTo(NP_IMPS_HSM_NOT_BIG_SIGNED_IN);
		} else if (eventId == NP_IMPS_MP_EVT_ENTER_ONLINE_REQ) {
			TransitionTo(NP_IMPS_HSM_NOT_BIG_ONLINE);
		} else if (eventId == NP_IMPS_MP_EVT_SIGN_OUT) {
			TransitionTo(NP_IMPS_HSM_NOT_BIG_SIGNED_OUT);
		} else if (eventId == NP_IMPS_MP_EVT_IPC_QUEUE_OVERFLOW) {
			TransitionTo(NP_IMPS_HSM_NOT_BIG_IPC_RECOVERING);
		} else if (eventId == NP_IMPS_MP_EVT_PROCESS_TERMINATE) {
			TransitionTo(NP_IMPS_HSM_END);
		}
		break;

	case NP_IMPS_HSM_NOT_BIG_IPC_RECOVERING:
		if (eventId == NP_IMPS_MP_EVT_IPC_QUEUE_OVERFLOW) {
			/* Already recovering */
		} else if (eventId == NP_IMPS_MP_EVT_IPC_QUEUE_RECOVERED) {
			if (IsGlobalStateOnline())
				TransitionTo(NP_IMPS_HSM_NOT_BIG_PSEUDO_SIGNED_IN);
			else
				TransitionTo(NP_IMPS_HSM_NOT_BIG_SIGNED_IN);
		} else if (eventId == NP_IMPS_MP_EVT_SIGN_OUT) {
			TransitionTo(NP_IMPS_HSM_NOT_BIG_SIGNED_OUT);
		} else if (eventId == NP_IMPS_MP_EVT_PROCESS_TERMINATE) {
			TransitionTo(NP_IMPS_HSM_END);
		}
		break;

	case NP_IMPS_HSM_NOT_BIG_ONLINE:
		if (eventId == NP_IMPS_MP_EVT_SIGN_IN) {
			TransitionTo(NP_IMPS_HSM_NOT_BIG_SIGNED_IN);
		} else if (eventId == NP_IMPS_MP_EVT_IPC_QUEUE_OVERFLOW) {
			TransitionTo(NP_IMPS_HSM_NOT_BIG_IPC_RECOVERING);
		} else if (eventId == NP_IMPS_MP_EVT_LEAVE_ONLINE_REQ) {
			TransitionTo(NP_IMPS_HSM_NOT_BIG_PSEUDO_SIGNED_IN);
		} else if (eventId == NP_IMPS_MP_EVT_SIGN_OUT) {
			TransitionTo(NP_IMPS_HSM_NOT_BIG_SIGNED_OUT);
		} else if (eventId == NP_IMPS_MP_EVT_PROCESS_TERMINATE) {
			TransitionTo(NP_IMPS_HSM_END);
		}
		break;

	default:
		break;
	}

	m_lastErrorCode = 0;
	m_hsmMutex.Unlock();
}

/* 0x812F7590, 0x812F75AE, 0x812F75CC */
void NpImpsProcessHsm::NotifyServiceState(int serviceState)
{
	NpImpsUtilSessionHolder *gameHolder = npImpsUtilGetGameSessionHolder();
	if (gameHolder != NULL)
		gameHolder->SendServiceStateToProcess(m_processId, serviceState, m_lastErrorCode);
}

/* 0x812F7A2E */
void NpImpsProcessHsm::SetLocalServiceState(int serviceState)
{
	m_serviceStateMutex.Lock();
	m_serviceState = serviceState;
	m_serviceStateMutex.Unlock();
}

/* 0x812F7A48 */
int NpImpsProcessHsm::GetServiceState(SceUInt8 rawMode)
{
	m_serviceStateMutex.Lock();
	int state = m_serviceState;
	m_serviceStateMutex.Unlock();

	SceUInt32 sdkVersion = 0;
	vshKernelGetCompiledSdkVersionByPid(m_processId, &sdkVersion);
	if (rawMode == 0 && state <= NP_IMPS_SVC_STATE_SIGNED_OUT &&
	    npAccountGetLegacySignInStatus(sdkVersion) == 1) {
		return NP_IMPS_SVC_STATE_SIGNED_IN;
	}
	return state;
}

/* 0x812F75EA */
void NpImpsProcessHsm::SetActiveProcess()
{
	npImpsSvcSetActiveProcess(m_processId, m_appId);
}

/* 0x812F75F8 */
void NpImpsProcessHsm::EnqueueSetGameTitleJob()
{
	void *mem = npImpsUtilMalloc(sizeof(NpImpsUtilSetGameTitleJob));
	if (mem == NULL)
		return;

	NpImpsUtilSetGameTitleJob *job = new (mem) NpImpsUtilSetGameTitleJob(m_appId);
	paf::common::SharedPtr<paf::job::JobItem> jobPtr(job);
	paf::job::JobQueue *queue = shellGetNpBackgroundJobQueue();
	if (queue != NULL)
		queue->Enqueue(jobPtr);
}

/* 0x812F76CA */
void NpImpsProcessHsm::ClearGameTitle()
{
	npImpsSvcClearGameTitle();
}

/* 0x812F76D2 */
bool NpImpsProcessHsm::IsFriendsApp() const
{
	SceSize len = sce_paf_strlen("NPXS10001");
	return sce_paf_strncmp(m_titleId, "NPXS10001", len) == 0;
}

/* 0x812F76FE */
void NpImpsProcessHsm::RegisterOnlineListener()
{
	npImpsSvcRegisterListener(&m_onlineListenerCb, &m_onlineListenerId);
}

/* 0x812F7710 */
void NpImpsProcessHsm::UnregisterOnlineListener()
{
	npImpsSvcUnregisterListener(m_onlineListenerId);
	m_onlineListenerId = 0;
}

/* 0x812F787C */
void NpImpsProcessHsm::RegisterSignedInListener()
{
	npImpsSvcRegisterListener(&m_signedInListenerCb, &m_signedInListenerId);
}

/* 0x812F788E */
void NpImpsProcessHsm::UnregisterSignedInListener()
{
	npImpsSvcUnregisterListener(m_signedInListenerId);
	m_signedInListenerId = 0;
}

/* 0x812F7724 */
void NpImpsProcessHsm::ResetFriendPresenceToOffline()
{
	SceNpCommunicationId emptyCommId;
	NpImpsGamePresenceDetail emptyDetail;
	NpImpsGamePresenceWithTitle emptyPresence;
	sce_paf_memset(&emptyCommId, 0, sizeof(emptyCommId));
	sce_paf_memset(&emptyDetail, 0, sizeof(emptyDetail));
	sce_paf_memset(&emptyPresence, 0, sizeof(emptyPresence));

	NpImpsUtilSessionHolder *gameHolder = npImpsUtilGetGameSessionHolder();
	if (npImpsSvcLockFriendStore() >= 0) {
		SceUInt32 totalCount = 0;
		if (npImpsSvcGetAcceptedFriendCount(&totalCount, 0) >= 0 && totalCount > 0) {
			SceNpId *npIds = static_cast<SceNpId *>(
				npImpsUtilMalloc(sizeof(SceNpId) * totalCount));
			if (npIds != NULL) {
				sce_paf_memset(npIds, 0, sizeof(SceNpId) * totalCount);
				SceUInt32 actualCount = 0;
				if (npImpsSvcGetAcceptedFriendEntries(0, npIds, totalCount,
								      &actualCount, 0) >= 0) {
					for (SceUInt32 idx = 0; idx < actualCount; ++idx) {
						gameHolder->SendFriendContextToProcess(
							m_processId, 0, &npIds[idx]);
						gameHolder->SendFriendCommIdToProcess(
							m_processId, &npIds[idx], &emptyCommId);
						gameHolder->SendFriendPresenceDetailToProcess(
							m_processId, &npIds[idx], &emptyDetail);
						gameHolder->SendFriendGamePresenceToProcess(
							m_processId, &npIds[idx],
							&emptyCommId, &emptyCommId, 1,
							&emptyPresence);
						for (int platform = 1; platform < 3; ++platform) {
							gameHolder->SendPlatformFriendPresenceToProcess(
								m_processId, &npIds[idx],
								&emptyDetail, platform);
						}
					}
				}
				npImpsUtilFree(npIds);
			}
		}
		npImpsSvcUnlockFriendStore();
	}
}

/* 0x812F784E */
void NpImpsProcessHsm::ClearGameEventQueue()
{
	NpImpsUtilSessionHolder *gameHolder = npImpsUtilGetGameSessionHolder();
	if (gameHolder != NULL)
		gameHolder->ClearQueuesForProcess(m_processId);
}

/* 0x812F7866 */
void NpImpsProcessHsm::PostQueueRecoveredEvent()
{
	if (m_owner != NULL)
		m_owner->PostEvent(NP_IMPS_MP_EVT_IPC_QUEUE_RECOVERED, m_processId, 0);
}

/* 0x812F78A2 */
void NpImpsProcessHsm::SendEmptyListSnapshots()
{
	NpImpsUtilSessionHolder *gameHolder = npImpsUtilGetGameSessionHolder();
	if (gameHolder == NULL)
		return;

	SceNpId emptyNpId;
	sce_paf_memset(&emptyNpId, 0, sizeof(emptyNpId));
	gameHolder->SendFriendListToProcess(m_processId, 0, &emptyNpId);
	gameHolder->SendFriendListToProcess(m_processId, 2, &emptyNpId);
	gameHolder->SendBlockListToProcess(m_processId, 0, &emptyNpId);
	gameHolder->SendBlockListToProcess(m_processId, 2, &emptyNpId);
	gameHolder->SendFriendRequestToProcess(m_processId, 0, &emptyNpId);
	gameHolder->SendFriendRequestToProcess(m_processId, 3, &emptyNpId);
}

/* 0x812F7988 */
bool NpImpsProcessHsm::IsBigAppGame() const
{
	SceUInt8 isBigApp = 0;
	npImpsSvcIsBigAppGame(m_appId, &isBigApp);
	return isBigApp != 0;
}

/* 0x812F79BC */
bool NpImpsProcessHsm::HasActiveCommId() const
{
	bool hasCommId = false;
	npImpsSvcHasCommId(&hasCommId);
	return hasCommId;
}

/* 0x812F79F4 */
bool NpImpsProcessHsm::IsGlobalStateOnline() const
{
	int globalState = 0;
	return npImpsSvcGetServiceState(&globalState) >= 0 &&
	       globalState == NP_IMPS_SVC_STATE_ONLINE;
}

/* 0x812F7ABA */
void NpImpsProcessHsm::QueueSetGamePresence(const NpImpsGamePresenceInput *input)
{
	m_pendingGamePresence = *input;
	m_pendingPresenceOp = NP_IMPS_PENDING_PRESENCE_SET;
}

/* 0x812F7AE6 */
void NpImpsProcessHsm::QueueClearGamePresence()
{
	m_pendingPresenceOp = NP_IMPS_PENDING_PRESENCE_CLEAR;
}

/* 0x812F7AEE */
void NpImpsProcessHsm::FlushPendingGamePresence()
{
	if (m_pendingPresenceOp == NP_IMPS_PENDING_PRESENCE_SET)
		npImpsSvcSetGamePresence(m_processId, &m_pendingGamePresence);
	else if (m_pendingPresenceOp == NP_IMPS_PENDING_PRESENCE_CLEAR)
		npImpsSvcClearGamePresence();
	m_pendingPresenceOp = NP_IMPS_PENDING_PRESENCE_NONE;
}

/* 0x812F7B1E */
void NpImpsProcessHsm::CompletePendingEnterOnlineSuccess()
{
	completeEnterOnlineAsync(m_processId, 0);
}

/* 0x812F7B2C */
void NpImpsProcessHsm::DisconnectCoreProcess()
{
	npImpsCoreDisconnectProcess(m_processId);
}

/* 0x812F7B36 */
void NpImpsProcessHsm::CompletePendingEnterOnlineResult()
{
	int errCode = 0;
	if (m_owner != NULL && m_owner->TakeConnectError(m_processId, &errCode) >= 0)
		completeEnterOnlineAsync(m_processId, errCode);
}

/* 0x812F7B7E */
void NpImpsProcessHsm::NotifyPushOnline()
{
	npPushNotifyAppOnline(m_appId);
}

/* 0x812F7B88 */
void NpImpsProcessHsm::NotifyPushAppExitIfOnline()
{
	int globalState = 0;
	if (npImpsSvcGetServiceState(&globalState) >= 0 &&
	    globalState == NP_IMPS_SVC_STATE_ONLINE) {
		npPushNotifyAppExit();
	}
}

/* 0x812F7BC2 */
void NpImpsProcessHsm::NotifyPushSignedOut()
{
	npPushNotifySignedOut();
}

/* Per-process listener callbacks (0x812F6CE4..0x812F758F) */

/* 0x812F6CE4 */
void NpImpsProcessHsm::OnSignedInMyOnlineStatus(int status, void *userArg)
{
	NpImpsProcessHsm *self = static_cast<NpImpsProcessHsm *>(userArg);
	NpImpsUtilSessionHolder *gameHolder = npImpsUtilGetGameSessionHolder();
	if (gameHolder != NULL)
		gameHolder->SendMyOnlineStatusToProcess(self->m_processId, status);
}

/* 0x812F6CFC */
void NpImpsProcessHsm::OnSignedInMyPresence(int eventType, const NpImpsSvcPresenceSnapshot *curSnap,
					    const NpImpsSvcPresenceSnapshot *prevSnap, void *userArg)
{
	NpImpsProcessHsm *self = static_cast<NpImpsProcessHsm *>(userArg);
	NpImpsUtilSessionHolder *gameHolder = npImpsUtilGetGameSessionHolder();
	if (gameHolder == NULL)
		return;

	SceUInt8 gameChanged = 0;
	SceUInt8 partyChanged = 0;
	npImpsUtilDiffMyPresence(eventType, curSnap, prevSnap, &gameChanged, &partyChanged);

	if (gameChanged != 0) {
		NpImpsGamePresenceDetail detail;
		npImpsUtilFillGamePresenceDetail(&detail, curSnap);
		gameHolder->SendMyPresenceDetailToProcess(self->m_processId, &detail);
	}
	if (partyChanged != 0) {
		NpImpsPartyPresence party;
		npImpsUtilFillPartyPresence(&party, curSnap);
		gameHolder->SendMyPartyPresenceToProcess(self->m_processId, &party);
	}
}

/* 0x812F6DBA */
void NpImpsProcessHsm::OnSignedInFriendList(int eventType, const SceNpId *npIds,
					    NpImpsCacheFriendPtr **handles, SceSize count,
					    void *userArg)
{
	(void)npIds;
	NpImpsProcessHsm *self = static_cast<NpImpsProcessHsm *>(userArg);
	NpImpsUtilSessionHolder *gameHolder = npImpsUtilGetGameSessionHolder();
	if (gameHolder == NULL || handles == NULL)
		return;

	if (eventType == 1 || eventType == 2 || eventType == 4) {
		for (SceSize idx = 0; idx < count; ++idx) {
			int relation = 0;
			SceUInt8 isNew = 0;
			SceNpId friendNpId;
			if (npImpsSvcGetFriendEntryRelation(handles[idx], &relation) >= 0 &&
			    npImpsSvcGetFriendEntryIsNew(handles[idx], &isNew) >= 0 &&
			    npImpsSvcGetFriendEntryNpId(handles[idx], &friendNpId) >= 0 &&
			    isNew != 0 && relation == 3) {
				gameHolder->SendFriendListToProcess(self->m_processId, 3,
								    &friendNpId);
			}
		}
	}

	for (SceSize idx = 0; idx < count; ++idx) {
		int relation = 0;
		SceNpId friendNpId;
		SceUInt8 isNew = 0;
		if (npImpsSvcGetFriendEntryRelation(handles[idx], &relation) >= 0 &&
		    npImpsSvcGetFriendEntryNpId(handles[idx], &friendNpId) >= 0 &&
		    npImpsSvcGetFriendEntryIsNew(handles[idx], &isNew) >= 0) {
			if (eventType == 4) {
				if (relation == 3)
					gameHolder->SendFriendRequestToProcess(self->m_processId, 6,
									       &friendNpId);
			} else if ((eventType == 1 || eventType == 2) && isNew != 0) {
				if (relation == 1)
					gameHolder->SendFriendRequestToProcess(self->m_processId, 4,
									       &friendNpId);
				else if (relation == 2)
					gameHolder->SendFriendRequestToProcess(self->m_processId, 5,
									       &friendNpId);
			}
		}
	}
}

/* 0x812F6F32 */
void NpImpsProcessHsm::OnSignedInFriendRemoved(const SceNpId *npIds, SceSize count, void *userArg)
{
	NpImpsProcessHsm *self = static_cast<NpImpsProcessHsm *>(userArg);
	NpImpsUtilSessionHolder *gameHolder = npImpsUtilGetGameSessionHolder();
	if (gameHolder == NULL)
		return;
	for (SceSize idx = 0; idx < count; ++idx)
		gameHolder->SendFriendListToProcess(self->m_processId, 4, &npIds[idx]);
}

/* 0x812F6F5C */
void NpImpsProcessHsm::OnSignedInFriendRequestRemoved(const SceNpId *npIds, SceSize count,
						      void *userArg)
{
	NpImpsProcessHsm *self = static_cast<NpImpsProcessHsm *>(userArg);
	NpImpsUtilSessionHolder *gameHolder = npImpsUtilGetGameSessionHolder();
	if (gameHolder == NULL)
		return;
	for (SceSize idx = 0; idx < count; ++idx)
		gameHolder->SendFriendRequestToProcess(self->m_processId, 7, &npIds[idx]);
}

/* 0x812F6F86 */
void NpImpsProcessHsm::OnSignedInBlockList(int eventType, const SceNpId *npIds, SceSize count,
					   void *userArg)
{
	NpImpsProcessHsm *self = static_cast<NpImpsProcessHsm *>(userArg);
	NpImpsUtilSessionHolder *gameHolder = npImpsUtilGetGameSessionHolder();
	if (gameHolder == NULL)
		return;

	int action = 0;
	if (eventType == 1 || eventType == 2)
		action = 3;
	else if (eventType == 3)
		action = 4;
	else
		return;

	for (SceSize idx = 0; idx < count; ++idx)
		gameHolder->SendBlockListToProcess(self->m_processId, action, &npIds[idx]);
}

/* 0x812F7056 */
void NpImpsProcessHsm::OnOnlineFriendContext(int eventType, NpImpsCacheFriendPtr *handle,
					     const SceNpId *npId, int contextState,
					     void *extra, void *userArg)
{
	(void)eventType;
	(void)handle;
	(void)extra;
	NpImpsProcessHsm *self = static_cast<NpImpsProcessHsm *>(userArg);
	NpImpsUtilSessionHolder *gameHolder = npImpsUtilGetGameSessionHolder();
	if (gameHolder != NULL)
		gameHolder->SendFriendContextToProcess(self->m_processId, contextState, npId);
}

static void buildBasicPresenceDetail(NpImpsGamePresenceDetail *outDetail,
				     const NpImpsSvcPresenceSnapshot *snapshot)
{
	sce_paf_memset(outDetail, 0, sizeof(*outDetail));
	outDetail->size = sizeof(*outDetail);
	outDetail->titleId = snapshot->titleId;
	outDetail->commId = snapshot->commId;
	outDetail->gamePresence.size = sizeof(outDetail->gamePresence);
	sce_paf_strncpy(outDetail->gamePresence.title, snapshot->titleName,
			sizeof(outDetail->gamePresence.title));
	outDetail->gamePresence.body = snapshot->gamePresence;
}

/* 0x812F7122 */
void NpImpsProcessHsm::OnOnlineFriendPresence(int eventType, NpImpsCacheFriendPtr *handle,
					      const SceNpId *npId,
					      const NpImpsSvcPresenceSnapshot *curSnap,
					      const NpImpsSvcPresenceSnapshot *prevSnap,
					      SceUInt32 diffMask, void *userArg)
{
	(void)handle;
	NpImpsProcessHsm *self = static_cast<NpImpsProcessHsm *>(userArg);
	NpImpsUtilSessionHolder *gameHolder = npImpsUtilGetGameSessionHolder();
	if (gameHolder == NULL)
		return;

	if (eventType == 1) {
		gameHolder->SendFriendCommIdToProcess(self->m_processId, npId, &curSnap->commId);

		NpImpsGamePresenceDetail detail;
		buildBasicPresenceDetail(&detail, curSnap);
		gameHolder->SendFriendPresenceDetailToProcess(self->m_processId, npId, &detail);

		NpImpsGamePresenceWithTitle presence;
		sce_paf_memset(&presence, 0, sizeof(presence));
		presence.size = sizeof(presence);
		sce_paf_strncpy(presence.title, curSnap->titleName, sizeof(presence.title));
		presence.body = curSnap->gamePresence;
		gameHolder->SendFriendGamePresenceToProcess(self->m_processId, npId,
							    &curSnap->commId, &prevSnap->commId,
							    1, &presence);

		gameHolder->SendFriendPartyPresenceToProcess(self->m_processId, npId,
							     &curSnap->partyPresence);
		return;
	}

	if (eventType == 2) {
		if ((diffMask & NP_IMPS_DIFF_COMM_ID) != 0)
			gameHolder->SendFriendCommIdToProcess(self->m_processId, npId,
							      &curSnap->commId);

		if ((diffMask & (NP_IMPS_DIFF_TITLE | NP_IMPS_DIFF_GAME_PRESENCE |
				 NP_IMPS_DIFF_CONTENT_INFO)) != 0) {
			NpImpsGamePresenceDetail detail;
			buildBasicPresenceDetail(&detail, curSnap);
			gameHolder->SendFriendPresenceDetailToProcess(self->m_processId, npId,
								     &detail);
		}

		if ((diffMask & (NP_IMPS_DIFF_TITLE | NP_IMPS_DIFF_COMM_ID |
				 NP_IMPS_DIFF_GAME_PRESENCE)) != 0) {
			NpImpsGamePresenceWithTitle presence;
			sce_paf_memset(&presence, 0, sizeof(presence));
			presence.size = sizeof(presence);
			sce_paf_strncpy(presence.title, curSnap->titleName, sizeof(presence.title));
			presence.body = curSnap->gamePresence;
			gameHolder->SendFriendGamePresenceToProcess(self->m_processId, npId,
								    &curSnap->commId,
								    &prevSnap->commId, 0,
								    &presence);
		}

		if ((diffMask & NP_IMPS_DIFF_PARTY_PRESENCE) != 0) {
			gameHolder->SendFriendPartyPresenceToProcess(self->m_processId, npId,
								     &curSnap->partyPresence);
		}
	}
}

/* 0x812F7302 */
void NpImpsProcessHsm::OnOnlineInGameDataMessage(int eventType, const SceNpId *senderNpId,
						 const NpImpsSvcInGameDataPayload *payload,
						 void *userArg)
{
	(void)eventType;
	NpImpsProcessHsm *self = static_cast<NpImpsProcessHsm *>(userArg);
	NpImpsUtilSessionHolder *gameHolder = npImpsUtilGetGameSessionHolder();
	if (gameHolder == NULL)
		return;

	SceUInt32 sdkVersion = 0;
	if (vshKernelGetCompiledSdkVersionByPid(self->m_processId, &sdkVersion) < 0)
		return;

	if (sdkVersion < 0x01600000u && sceNpGetPlatformType(senderNpId) != 2)
		return;

	NpImpsSvcInGameDataPayload adjusted;
	sce_paf_memset(&adjusted, 0, sizeof(adjusted));
	adjusted.commId = payload->commId;

	if (sdkVersion >= 0x02000000u) {
		sce_paf_memcpy(adjusted.data, payload->data, sizeof(adjusted.data));
		adjusted.dataSize = payload->dataSize;
	} else {
		/* Pre-2.00 SDK games expect a 128-byte data buffer followed immediately by size */
		struct LegacyInGameDataBody {
			SceUInt8 data[128];
			SceSize size;
		};
		LegacyInGameDataBody legacy;
		sce_paf_memset(&legacy, 0, sizeof(legacy));
		sce_paf_memcpy(legacy.data, payload->data, sizeof(legacy.data));
		legacy.size = payload->dataSize;
		sce_paf_memcpy(adjusted.data, &legacy, sizeof(legacy));
	}

	if (sceNpGetPlatformType(senderNpId) >= 0)
		gameHolder->SendInGameDataToProcess(self->m_processId, senderNpId, &adjusted);
}

/* 0x812F74D2 */
void NpImpsProcessHsm::OnOnlinePlatformFriendPresence(int eventType, NpImpsCacheFriendPtr *handle,
						      const SceNpId *npId,
						      const NpImpsSvcPresenceSnapshot *curSnap,
						      const NpImpsSvcPresenceSnapshot *prevSnap,
						      SceUInt32 diffMask, int platform,
						      void *userArg)
{
	(void)handle;
	(void)prevSnap;
	NpImpsProcessHsm *self = static_cast<NpImpsProcessHsm *>(userArg);
	NpImpsUtilSessionHolder *gameHolder = npImpsUtilGetGameSessionHolder();
	if (gameHolder == NULL)
		return;

	bool shouldSend = false;
	if (eventType == 1) {
		shouldSend = true;
	} else if (eventType == 2 &&
		   ((diffMask & (NP_IMPS_DIFF_TITLE | NP_IMPS_DIFF_GAME_PRESENCE |
				 NP_IMPS_DIFF_CONTENT_INFO)) != 0)) {
		shouldSend = true;
	}

	if (shouldSend) {
		NpImpsGamePresenceDetail detail;
		buildBasicPresenceDetail(&detail, curSnap);
		gameHolder->SendPlatformFriendPresenceToProcess(self->m_processId, npId,
								&detail, platform);
	}
}

/* ============================================================================
 * NpImpsUtilMpState thread implementation (0x812F4C44..0x812F5833)
 * ============================================================================ */

NpImpsUtilMpState::NpImpsUtilMpState()
	: paf::thread::Thread(120, 12288, "SceNpImpsUtilMPState", NULL),
	  m_mapMutex("SceNpImpsUtilMPStateMtx", 0),
	  m_processes(NULL),
	  m_connectErrors(NULL),
	  m_condMutex("SceNpImpsUtilMPStateCMtx", 0),
	  m_cond(m_condMutex, "SceNpImpsUtilMPStateCMtx", 0),
	  m_queueMutex("SceNpImpsUtilMPStateQMtx", 0),
	  m_queueHead(NULL),
	  m_queueTail(NULL),
	  m_queueCount(0)
{
}

NpImpsUtilMpState::~NpImpsUtilMpState()
{
	m_mapMutex.Lock();
	while (m_processes != NULL) {
		ProcessNode *next = m_processes->next;
		m_processes->hsm->~NpImpsProcessHsm();
		npImpsUtilFree(m_processes->hsm);
		npImpsUtilFree(m_processes);
		m_processes = next;
	}
	while (m_connectErrors != NULL) {
		ConnectErrorNode *next = m_connectErrors->next;
		npImpsUtilFree(m_connectErrors);
		m_connectErrors = next;
	}
	m_mapMutex.Unlock();

	m_queueMutex.Lock();
	while (m_queueHead != NULL) {
		EventNode *next = m_queueHead->next;
		npImpsUtilFree(m_queueHead);
		m_queueHead = next;
	}
	m_queueTail = NULL;
	m_queueCount = 0;
	m_queueMutex.Unlock();
}

NpImpsProcessHsm *NpImpsUtilMpState::FindProcessLocked(SceUID processId)
{
	for (ProcessNode *cur = m_processes; cur != NULL; cur = cur->next) {
		if (cur->processId == processId)
			return cur->hsm;
	}
	return NULL;
}

/* 0x812F4D94 */
int NpImpsUtilMpState::OnProcessCreated(SceUID processId, int appId)
{
	m_mapMutex.Lock();

	void *hsmMem = npImpsUtilMalloc(sizeof(NpImpsProcessHsm));
	if (hsmMem == NULL) {
		m_mapMutex.Unlock();
		return NP_IMPS_UTIL_ERROR_OUT_OF_MEMORY;
	}
	NpImpsProcessHsm *hsm = new (hsmMem) NpImpsProcessHsm(processId, appId, this);

	ProcessNode *node = static_cast<ProcessNode *>(npImpsUtilMalloc(sizeof(ProcessNode)));
	if (node == NULL) {
		hsm->~NpImpsProcessHsm();
		npImpsUtilFree(hsm);
		m_mapMutex.Unlock();
		return NP_IMPS_UTIL_ERROR_OUT_OF_MEMORY;
	}
	node->processId = processId;
	node->hsm = hsm;
	node->next = m_processes;
	m_processes = node;

	m_mapMutex.Unlock();
	return 0;
}

/* 0x812F4E3C */
int NpImpsUtilMpState::RemoveProcess(SceUID processId)
{
	m_mapMutex.Lock();
	ProcessNode **link = &m_processes;
	while (*link != NULL) {
		if ((*link)->processId == processId) {
			ProcessNode *victim = *link;
			*link = victim->next;
			victim->hsm->~NpImpsProcessHsm();
			npImpsUtilFree(victim->hsm);
			npImpsUtilFree(victim);
			m_mapMutex.Unlock();
			return 0;
		}
		link = &(*link)->next;
	}
	m_mapMutex.Unlock();
	return NP_IMPS_UTIL_ERROR_INTERNAL;
}

/* 0x812F4EE8 */
int NpImpsUtilMpState::PostEvent(int eventId, SceUID processId, int errorCode)
{
	m_queueMutex.Lock();
	EventNode *node = static_cast<EventNode *>(npImpsUtilMalloc(sizeof(EventNode)));
	if (node == NULL) {
		m_queueMutex.Unlock();
		return NP_IMPS_UTIL_ERROR_OUT_OF_MEMORY;
	}
	node->item.eventId = eventId;
	node->item.processId = processId;
	node->item.errorCode = errorCode;
	node->next = NULL;

	if (m_queueTail != NULL)
		m_queueTail->next = node;
	else
		m_queueHead = node;
	m_queueTail = node;
	++m_queueCount;

	m_cond.Notify();
	m_queueMutex.Unlock();
	return 0;
}

/* 0x812F5460 */
bool NpImpsUtilMpState::PopEvent(EventItem *outItem, SceUInt32 *outRemaining)
{
	m_queueMutex.Lock();
	if (m_queueHead == NULL) {
		*outRemaining = 0;
		m_queueMutex.Unlock();
		return false;
	}

	EventNode *node = m_queueHead;
	m_queueHead = node->next;
	if (m_queueHead == NULL)
		m_queueTail = NULL;
	--m_queueCount;
	*outItem = node->item;
	*outRemaining = m_queueCount;
	npImpsUtilFree(node);
	m_queueMutex.Unlock();
	return true;
}

/* 0x812F5012 */
int NpImpsUtilMpState::GetServiceState(SceUID processId, int *outState, SceUInt8 rawMode)
{
	m_mapMutex.Lock();
	NpImpsProcessHsm *hsm = FindProcessLocked(processId);
	if (hsm == NULL) {
		m_mapMutex.Unlock();
		return NP_IMPS_UTIL_ERROR_INTERNAL;
	}
	*outState = hsm->GetServiceState(rawMode);
	m_mapMutex.Unlock();
	return 0;
}

/* 0x812F508E */
int NpImpsUtilMpState::LockProcess(SceUID processId)
{
	m_mapMutex.Lock();
	NpImpsProcessHsm *hsm = FindProcessLocked(processId);
	if (hsm == NULL) {
		m_mapMutex.Unlock();
		return NP_IMPS_UTIL_ERROR_INTERNAL;
	}
	hsm->LockState();
	m_mapMutex.Unlock();
	return 0;
}

/* 0x812F50F6 */
int NpImpsUtilMpState::UnlockProcess(SceUID processId)
{
	m_mapMutex.Lock();
	NpImpsProcessHsm *hsm = FindProcessLocked(processId);
	if (hsm == NULL) {
		m_mapMutex.Unlock();
		return NP_IMPS_UTIL_ERROR_INTERNAL;
	}
	hsm->UnlockState();
	m_mapMutex.Unlock();
	return 0;
}

/* 0x812F5152 */
int NpImpsUtilMpState::SetPendingGamePresence(SceUID processId,
					      const NpImpsGamePresenceInput *input)
{
	m_mapMutex.Lock();
	NpImpsProcessHsm *hsm = FindProcessLocked(processId);
	if (hsm == NULL) {
		m_mapMutex.Unlock();
		return NP_IMPS_UTIL_ERROR_INTERNAL;
	}
	int state = hsm->GetServiceState(0);
	int ret = 0;
	if (state == NP_IMPS_SVC_STATE_ONLINE)
		ret = npImpsSvcSetGamePresence(processId, input);
	else
		hsm->QueueSetGamePresence(input);
	m_mapMutex.Unlock();
	return ret;
}

/* 0x812F522E */
int NpImpsUtilMpState::ClearPendingGamePresence(SceUID processId)
{
	m_mapMutex.Lock();
	NpImpsProcessHsm *hsm = FindProcessLocked(processId);
	if (hsm == NULL) {
		m_mapMutex.Unlock();
		return NP_IMPS_UTIL_ERROR_INTERNAL;
	}
	int state = hsm->GetServiceState(0);
	int ret = 0;
	if (state == NP_IMPS_SVC_STATE_ONLINE)
		ret = npImpsSvcClearGamePresence();
	else
		hsm->QueueClearGamePresence();
	m_mapMutex.Unlock();
	return ret;
}

/* 0x812F52FA */
int NpImpsUtilMpState::SetConnectError(SceUID processId, int errorCode)
{
	m_mapMutex.Lock();
	for (ConnectErrorNode *cur = m_connectErrors; cur != NULL; cur = cur->next) {
		if (cur->processId == processId) {
			cur->errorCode = errorCode;
			m_mapMutex.Unlock();
			return 0;
		}
	}

	ConnectErrorNode *node =
		static_cast<ConnectErrorNode *>(npImpsUtilMalloc(sizeof(ConnectErrorNode)));
	if (node == NULL) {
		m_mapMutex.Unlock();
		return NP_IMPS_UTIL_ERROR_OUT_OF_MEMORY;
	}
	node->processId = processId;
	node->errorCode = errorCode;
	node->next = m_connectErrors;
	m_connectErrors = node;
	m_mapMutex.Unlock();
	return 0;
}

/* 0x812F53BE */
int NpImpsUtilMpState::TakeConnectError(SceUID processId, int *outErrorCode)
{
	m_mapMutex.Lock();
	ConnectErrorNode **link = &m_connectErrors;
	while (*link != NULL) {
		if ((*link)->processId == processId) {
			ConnectErrorNode *victim = *link;
			*link = victim->next;
			*outErrorCode = victim->errorCode;
			npImpsUtilFree(victim);
			m_mapMutex.Unlock();
			return 0;
		}
		link = &(*link)->next;
	}
	m_mapMutex.Unlock();
	return NP_IMPS_UTIL_ERROR_INTERNAL;
}

/* 0x812F54BC */
int NpImpsUtilMpState::DispatchBroadcastEvent(int eventId, int errorCode)
{
	m_mapMutex.Lock();
	ProcessNode *cur = m_processes;
	while (cur != NULL) {
		ProcessNode *next = cur->next;
		cur->hsm->DispatchEvent(eventId, errorCode);
		if (cur->hsm->IsEnded())
			RemoveProcess(cur->processId);
		cur = next;
	}
	m_mapMutex.Unlock();
	return 0;
}

/* 0x812F561C */
int NpImpsUtilMpState::DispatchProcessEvent(int eventId, SceUID processId, int errorCode)
{
	m_mapMutex.Lock();
	NpImpsProcessHsm *hsm = FindProcessLocked(processId);
	if (hsm == NULL) {
		m_mapMutex.Unlock();
		return NP_IMPS_UTIL_ERROR_INTERNAL;
	}
	hsm->DispatchEvent(eventId, errorCode);
	if (hsm->IsEnded())
		RemoveProcess(processId);
	m_mapMutex.Unlock();
	return 0;
}

/* 0x812F5754 */
void NpImpsUtilMpState::EntryFunction()
{
	m_condMutex.Lock();
	while (!IsCanceled()) {
		m_cond.Wait(0);
		m_condMutex.Unlock();

		while (true) {
			EventItem ev;
			SceUInt32 remaining = 0;
			if (!PopEvent(&ev, &remaining))
				break;

			int ret = 0;
			if (ev.processId != 0)
				ret = DispatchProcessEvent(ev.eventId, ev.processId, ev.errorCode);
			else
				ret = DispatchBroadcastEvent(ev.eventId, ev.errorCode);
			if (ret < 0)
				break;
			if (remaining == 0)
				break;
		}

		m_condMutex.Lock();
	}

	EventItem drainEv;
	SceUInt32 drainRem = 0;
	while (PopEvent(&drainEv, &drainRem)) {
		if (drainRem == 0)
			break;
	}
	m_condMutex.Unlock();
}

/* 0x812F4C44 */
int npImpsUtilMpStateStart(void)
{
	void *mem = npImpsUtilMalloc(sizeof(NpImpsUtilMpState));
	if (mem == NULL)
		return NP_IMPS_UTIL_ERROR_OUT_OF_MEMORY;

	s_mpStateThread = new (mem) NpImpsUtilMpState();
	int ret = s_mpStateThread->Start();
	if (ret < 0) {
		s_mpStateThread->~NpImpsUtilMpState();
		npImpsUtilFree(s_mpStateThread);
		s_mpStateThread = NULL;
	}
	return ret;
}

/* 0x812F4D5E */
int npImpsUtilMpStateStop(void)
{
	if (s_mpStateThread == NULL)
		return 0;

	s_mpStateThread->Cancel();
	s_mpStateThread->Join();
	s_mpStateThread->~NpImpsUtilMpState();
	npImpsUtilFree(s_mpStateThread);
	s_mpStateThread = NULL;
	return 0;
}

int npImpsUtilMpStateOnProcessCreated(NpImpsUtilMpState *mpState, SceUID processId, int appId)
{
	if (mpState == NULL)
		return NP_IMPS_UTIL_ERROR_NOT_INITIALIZED;
	return mpState->OnProcessCreated(processId, appId);
}

int npImpsUtilMpStatePostEvent(NpImpsUtilMpState *mpState, int eventId,
			       SceUID processId, int errorCode)
{
	if (mpState == NULL)
		return NP_IMPS_UTIL_ERROR_NOT_INITIALIZED;
	return mpState->PostEvent(eventId, processId, errorCode);
}

int npImpsUtilMpStateGetServiceState(NpImpsUtilMpState *mpState, SceUID processId,
				     int *outState, SceUInt8 rawMode)
{
	if (mpState == NULL)
		return NP_IMPS_UTIL_ERROR_NOT_INITIALIZED;
	return mpState->GetServiceState(processId, outState, rawMode);
}

/* ============================================================================
 * Initial event snapshot replay for newly registered sessions (0x812F7E1C..0x812F7FCF,
 * 0x8143CF18..0x8143D91F)
 * ============================================================================ */

/* 0x8143D5A8 */
static int replayFriendAndRequestLists(NpImpsUtilSessionHolder *holder, IPMI::Session *session,
				       NpImpsCacheFriendPtr **handles, SceUInt32 count)
{
	SceNpId emptyNpId;
	sce_paf_memset(&emptyNpId, 0, sizeof(emptyNpId));
	holder->SendFriendListToSession(session, 0, &emptyNpId);

	int ret = 0;
	for (SceUInt32 idx = 0; idx < count; ++idx) {
		int relation = 0;
		ret = npImpsSvcGetFriendEntryRelation(handles[idx], &relation);
		if (ret >= 0 && relation == 3) {
			SceNpId friendNpId;
			ret = npImpsSvcGetFriendEntryNpId(handles[idx], &friendNpId);
			if (ret >= 0)
				holder->SendFriendListToSession(session, 1, &friendNpId);
		}
	}
	holder->SendFriendListToSession(session, 2, &emptyNpId);

	holder->SendFriendRequestToSession(session, 0, &emptyNpId);
	for (SceUInt32 idx = 0; idx < count; ++idx) {
		int relation = 0;
		ret = npImpsSvcGetFriendEntryRelation(handles[idx], &relation);
		if (ret >= 0) {
			SceNpId friendNpId;
			ret = npImpsSvcGetFriendEntryNpId(handles[idx], &friendNpId);
			if (ret >= 0) {
				if (relation == 1)
					holder->SendFriendRequestToSession(session, 1, &friendNpId);
				else if (relation == 2)
					holder->SendFriendRequestToSession(session, 2, &friendNpId);
			}
		}
	}
	holder->SendFriendRequestToSession(session, 3, &emptyNpId);
	return ret;
}

/* 0x8143CF18 */
static int replayFriendPresencesOffline(NpImpsUtilSessionHolder *holder, IPMI::Session *session,
					NpImpsCacheFriendPtr **handles, SceUInt32 count)
{
	int ret = 0;
	for (SceUInt32 idx = 0; idx < count; ++idx) {
		int relation = 0;
		ret = npImpsSvcGetFriendEntryRelation(handles[idx], &relation);
		if (ret < 0 || relation != 3)
			continue;

		SceNpId friendNpId;
		ret = npImpsSvcGetFriendEntryNpId(handles[idx], &friendNpId);
		if (ret < 0)
			continue;

		NpImpsSvcPresenceSnapshot emptySnap;
		sce_paf_memset(&emptySnap, 0, sizeof(emptySnap));

		NpImpsGamePresenceDetail detail;
		buildBasicPresenceDetail(&detail, &emptySnap);
		holder->SendFriendPresenceDetailToSession(session, &friendNpId, &detail);
		holder->SendFriendCommIdToSession(session, &friendNpId, &emptySnap.commId);

		NpImpsGamePresenceWithTitle presence;
		sce_paf_memset(&presence, 0, sizeof(presence));
		presence.size = sizeof(presence);
		holder->SendFriendGamePresenceToSession(session, &friendNpId,
							&emptySnap.commId, &emptySnap.commId,
							1, &presence);
		holder->SendFriendPartyPresenceToSession(session, &friendNpId,
							 &emptySnap.partyPresence);

		for (int platform = 2; platform > 0; --platform)
			holder->SendPlatformFriendPresenceToSession(session, &friendNpId,
								    &detail, platform);
	}
	return ret;
}

/* 0x8143D184 */
static int replayFriendPresencesOnline(NpImpsUtilSessionHolder *holder, IPMI::Session *session,
				       NpImpsCacheFriendPtr **handles, SceUInt32 count)
{
	int ret = 0;
	for (SceUInt32 idx = 0; idx < count; ++idx) {
		int relation = 0;
		ret = npImpsSvcGetFriendEntryRelation(handles[idx], &relation);
		if (ret < 0 || relation != 3)
			continue;

		SceNpId friendNpId;
		ret = npImpsSvcGetFriendEntryNpId(handles[idx], &friendNpId);
		if (ret < 0)
			continue;

		NpImpsSvcPresenceSnapshot snap;
		sce_paf_memset(&snap, 0, sizeof(snap));
		ret = npImpsSvcGetFriendPresenceByHandle(handles[idx], &snap);
		if (ret < 0)
			continue;

		NpImpsGamePresenceDetail detail;
		npImpsUtilFillGamePresenceDetail(&detail, &snap);
		holder->SendFriendPresenceDetailToSession(session, &friendNpId, &detail);
		holder->SendFriendCommIdToSession(session, &friendNpId, &snap.commId);

		NpImpsGamePresenceWithTitle presence;
		sce_paf_memset(&presence, 0, sizeof(presence));
		presence.size = sizeof(presence);
		sce_paf_strncpy(presence.title, snap.titleName, sizeof(presence.title));
		presence.body = snap.gamePresence;
		holder->SendFriendGamePresenceToSession(session, &friendNpId,
							&snap.commId, &snap.commId, 1, &presence);
		holder->SendFriendPartyPresenceToSession(session, &friendNpId, &snap.partyPresence);

		for (int platform = 2; platform > 0; --platform) {
			NpImpsSvcPresenceSnapshot platSnap;
			sce_paf_memset(&platSnap, 0, sizeof(platSnap));
			ret = npImpsSvcGetFriendPresenceByPlatform(handles[idx], &platSnap,
								   platform);
			if (ret >= 0) {
				NpImpsGamePresenceDetail platDetail;
				npImpsUtilFillGamePresenceDetail(&platDetail, &platSnap);
				holder->SendPlatformFriendPresenceToSession(session, &friendNpId,
									    &platDetail, platform);
			}
		}
	}
	return ret;
}

/* 0x812F7E1C / 0x8143D6E4 */
static int replayInitialFriendSnapshot(IPMI::Session *session, NpImpsUtilSessionHolder *holder,
				       SceUInt8 checkProcessState)
{
	int globalState = 0;
	int ret = npImpsSvcGetServiceState(&globalState);
	if (ret < 0)
		return ret;

	NpImpsCacheFriendPtr **handles = NULL;
	SceUInt32 count = 0;
	if (globalState == NP_IMPS_SVC_STATE_SIGNED_IN || globalState == NP_IMPS_SVC_STATE_ONLINE) {
		ret = npImpsSvcSnapshotFriendHandles(&handles, &count, npImpsUtilMalloc);
		if (ret < 0)
			return ret;
	}

	replayFriendAndRequestLists(holder, session, handles, count);

	int effectiveState = globalState;
	if (checkProcessState != 0) {
		effectiveState = npImpsUtilGetProcessServiceState(npImpsUtilGetServerContext(),
								  session->getProcessId());
	}

	if (globalState == NP_IMPS_SVC_STATE_ONLINE && effectiveState == NP_IMPS_SVC_STATE_ONLINE) {
		for (SceUInt32 idx = 0; idx < count; ++idx) {
			int relation = 0;
			SceNpId friendNpId;
			if (npImpsSvcGetFriendEntryRelation(handles[idx], &relation) >= 0 &&
			    relation == 3 &&
			    npImpsSvcGetFriendEntryNpId(handles[idx], &friendNpId) >= 0) {
				int ctxState = 0;
				if (npImpsSvcGetFriendContextByNpId(&friendNpId, &ctxState) >= 0)
					holder->SendFriendContextToSession(session, ctxState,
									   &friendNpId);
			}
		}
		ret = replayFriendPresencesOnline(holder, session, handles, count);
	} else {
		for (SceUInt32 idx = 0; idx < count; ++idx) {
			int relation = 0;
			SceNpId friendNpId;
			if (npImpsSvcGetFriendEntryRelation(handles[idx], &relation) >= 0 &&
			    relation == 3 &&
			    npImpsSvcGetFriendEntryNpId(handles[idx], &friendNpId) >= 0) {
				holder->SendFriendContextToSession(session, 0, &friendNpId);
			}
		}
		ret = replayFriendPresencesOffline(holder, session, handles, count);
	}

	if (globalState == NP_IMPS_SVC_STATE_SIGNED_IN || globalState == NP_IMPS_SVC_STATE_ONLINE)
		ret = npImpsSvcReleaseFriendHandles(handles, count, npImpsUtilFree);

	return ret;
}

/* 0x812F7E28 / 0x8143D850 */
static int replayInitialBlockList(IPMI::Session *session, NpImpsUtilSessionHolder *holder)
{
	SceNpId emptyNpId;
	sce_paf_memset(&emptyNpId, 0, sizeof(emptyNpId));
	holder->SendBlockListToSession(session, 0, &emptyNpId);

	int globalState = 0;
	int ret = npImpsSvcGetServiceState(&globalState);
	if (ret < 0)
		return ret;

	if (globalState == NP_IMPS_SVC_STATE_SIGNED_IN || globalState == NP_IMPS_SVC_STATE_ONLINE) {
		SceUInt32 blockCount = 0;
		ret = npImpsSvcGetBlockListCount(&blockCount, 0);
		if (ret < 0)
			return ret;

		for (SceUInt32 idx = 0; idx < blockCount; ++idx) {
			SceNpId blockedNpId;
			SceUInt32 fetched = 0;
			ret = npImpsSvcGetBlockListEntries(idx, &blockedNpId, 1, &fetched, 0);
			if (ret < 0)
				return ret;
			if (fetched != 1)
				return NP_IMPS_UTIL_ERROR_INTERNAL;
			holder->SendBlockListToSession(session, 1, &blockedNpId);
		}
	}

	holder->SendBlockListToSession(session, 2, &emptyNpId);
	return ret;
}

/* 0x812F7E30 */
static int replayInitialServiceState(IPMI::Session *session, NpImpsUtilSessionHolder *holder)
{
	int procState = 0;
	int ret = npImpsUtilMpStateGetServiceState(s_mpStateThread, session->getProcessId(),
						   &procState, 0);
	if (ret >= 0) {
		holder->SendServiceStateToSession(session, procState, 0);
		return 0;
	}
	return ret;
}

/* 0x812F7E8A */
static int replayInitialMyOnlineStatus(IPMI::Session *session, NpImpsUtilSessionHolder *holder)
{
	int globalState = 0;
	int status = NP_IMPS_SVC_ONLINE_OFFLINE;
	int ret = npImpsSvcGetServiceState(&globalState);
	if (ret < 0)
		return ret;

	if (globalState == NP_IMPS_SVC_STATE_SIGNED_IN || globalState == NP_IMPS_SVC_STATE_ONLINE) {
		ret = npImpsSvcGetMyOnlineStatus(&status);
		if (ret < 0)
			return ret;
	}
	holder->SendMyOnlineStatusToSession(session, status);
	return 0;
}

/* 0x812F7EDE */
static int replayInitialMyPresenceDetail(IPMI::Session *session, NpImpsUtilSessionHolder *holder)
{
	NpImpsSvcPresenceSnapshot snap;
	sce_paf_memset(&snap, 0, sizeof(snap));

	int globalState = 0;
	int ret = npImpsSvcGetServiceState(&globalState);
	if (ret < 0)
		return ret;

	if (globalState == NP_IMPS_SVC_STATE_SIGNED_IN || globalState == NP_IMPS_SVC_STATE_ONLINE) {
		ret = npImpsSvcGetMyPresence(&snap);
		if (ret < 0)
			return ret;
	}

	NpImpsGamePresenceDetail detail;
	npImpsUtilFillGamePresenceDetail(&detail, &snap);
	holder->SendMyPresenceDetailToSession(session, &detail);
	return 0;
}

/* 0x812F7F56 */
static int replayInitialMyPartyPresence(IPMI::Session *session, NpImpsUtilSessionHolder *holder)
{
	NpImpsSvcPresenceSnapshot snap;
	sce_paf_memset(&snap, 0, sizeof(snap));

	int globalState = 0;
	int ret = npImpsSvcGetServiceState(&globalState);
	if (ret < 0)
		return ret;

	if (globalState == NP_IMPS_SVC_STATE_SIGNED_IN || globalState == NP_IMPS_SVC_STATE_ONLINE) {
		ret = npImpsSvcGetMyPresence(&snap);
		if (ret < 0)
			return ret;
	}

	NpImpsPartyPresence party;
	npImpsUtilFillPartyPresence(&party, &snap);
	holder->SendMyPartyPresenceToSession(session, &party);
	return 0;
}

/* 0x812F5834 */
static int lockSnapshotStores(IPMI::Session *session, int *outGlobalState)
{
	SceUID processId = session->getProcessId();
	int ret = s_mpStateThread->LockProcess(processId);
	if (ret < 0)
		return ret;

	ret = npImpsSvcLockServiceState();
	if (ret < 0) {
		s_mpStateThread->UnlockProcess(processId);
		return ret;
	}

	ret = npImpsSvcGetServiceState(outGlobalState);
	if (ret < 0) {
		npImpsSvcUnlockServiceState();
		s_mpStateThread->UnlockProcess(processId);
		return ret;
	}

	if (*outGlobalState != NP_IMPS_SVC_STATE_SIGNED_IN &&
	    *outGlobalState != NP_IMPS_SVC_STATE_ONLINE) {
		return 0;
	}

	ret = npImpsSvcLockOnlineStatus();
	if (ret >= 0) {
		ret = npImpsSvcLockMyPresence();
		if (ret >= 0) {
			ret = npImpsSvcLockFriendStore();
			if (ret >= 0) {
				ret = npImpsSvcLockBlockStore();
				if (ret >= 0)
					return 0;
				npImpsSvcUnlockFriendStore();
			}
			npImpsSvcUnlockMyPresence();
		}
		npImpsSvcUnlockOnlineStatus();
	}

	npImpsSvcUnlockServiceState();
	s_mpStateThread->UnlockProcess(processId);
	return ret;
}

static void unlockSnapshotStores(IPMI::Session *session, int globalState)
{
	if (globalState == NP_IMPS_SVC_STATE_SIGNED_IN || globalState == NP_IMPS_SVC_STATE_ONLINE) {
		npImpsSvcUnlockBlockStore();
		npImpsSvcUnlockFriendStore();
		npImpsSvcUnlockMyPresence();
		npImpsSvcUnlockOnlineStatus();
	}
	npImpsSvcUnlockServiceState();
	s_mpStateThread->UnlockProcess(session->getProcessId());
}

static int replayAllInitialSnapshots(IPMI::Session *session, NpImpsUtilSessionHolder *holder,
				     SceUInt8 checkProcessState)
{
	int ret = replayInitialServiceState(session, holder);
	if (ret >= 0)
		ret = replayInitialMyOnlineStatus(session, holder);
	if (ret >= 0)
		ret = replayInitialMyPresenceDetail(session, holder);
	if (ret >= 0)
		ret = replayInitialMyPartyPresence(session, holder);
	if (ret >= 0)
		ret = replayInitialFriendSnapshot(session, holder, checkProcessState);
	if (ret >= 0)
		ret = replayInitialBlockList(session, holder);
	return ret;
}

/* ============================================================================
 * IPC method handlers (0x812F58EE..0x812F6CE3)
 * ============================================================================ */

/* 0x812F58EE */
int npImpsUtilHandleRegisterGameEvents(IPMI::Session *session,
				       const SceUInt8 mask[NP_IMPS_UTIL_EVENT_COUNT])
{
	int globalState = 0;
	int ret = lockSnapshotStores(session, &globalState);
	if (ret < 0)
		return ret;

	NpImpsUtilSessionHolder *gameHolder = npImpsUtilGetGameSessionHolder();
	ret = gameHolder->RegisterSession(session, mask);
	if (ret >= 0) {
		ret = replayAllInitialSnapshots(session, gameHolder, 1);
		if (ret < 0)
			gameHolder->UnregisterSession(session);
	}

	unlockSnapshotStores(session, globalState);
	return ret;
}

/* 0x812F5A14 */
int npImpsUtilHandleUnregisterGameEvents(IPMI::Session *session)
{
	return npImpsUtilGetGameSessionHolder()->UnregisterSession(session);
}

/* 0x812F5A2A */
int npImpsUtilHandleRegisterSystemEvents(IPMI::Session *session,
					 const SceUInt8 mask[NP_IMPS_UTIL_EVENT_COUNT])
{
	int globalState = 0;
	int ret = lockSnapshotStores(session, &globalState);
	if (ret < 0)
		return ret;

	NpImpsUtilSessionHolder *sysHolder = npImpsUtilGetSystemSessionHolder();
	ret = sysHolder->RegisterSession(session, mask);
	if (ret >= 0) {
		ret = replayAllInitialSnapshots(session, sysHolder, 0);
		if (ret < 0)
			sysHolder->UnregisterSession(session);
	}

	unlockSnapshotStores(session, globalState);
	return ret;
}

/* 0x812F5B50 */
int npImpsUtilHandleUnregisterSystemEvents(IPMI::Session *session)
{
	return npImpsUtilGetSystemSessionHolder()->UnregisterSession(session);
}

/* 0x812F5B66 */
int npImpsUtilHandleResyncSystemEvents(IPMI::Session *session)
{
	int globalState = 0;
	int ret = lockSnapshotStores(session, &globalState);
	if (ret < 0)
		return ret;

	ret = replayAllInitialSnapshots(session, npImpsUtilGetSystemSessionHolder(), 0);
	unlockSnapshotStores(session, globalState);
	return ret;
}

/* 0x812F5C22 */
int npImpsUtilHandleCreateRosterCtx(IPMI::Session *session, SceUInt32 *outCtxId)
{
	NpImpsUtilAsyncRelayMgr *relayMgr = NpImpsUtilAsyncRelayMgr::GetInstance();
	int ret = npImpsSvcCreateContext(outCtxId);
	if (ret < 0)
		return ret;

	ret = relayMgr->AddProcessContext(session->getProcessId(), *outCtxId);
	if (ret < 0) {
		npImpsSvcDestroyContext(*outCtxId);
		*outCtxId = 0;
		return ret;
	}
	return 0;
}

/* 0x812F5C68 */
int npImpsUtilHandleDestroyRosterCtx(IPMI::Session *session, const SceUInt32 *ctxId)
{
	NpImpsUtilAsyncRelayMgr *relayMgr = NpImpsUtilAsyncRelayMgr::GetInstance();
	relayMgr->RemoveProcessContext(session->getProcessId(), *ctxId);
	return npImpsSvcDestroyContext(*ctxId);
}

/* 0x812F5C8E */
int npImpsUtilHandleAbortRosterCtx(IPMI::Session *session, const SceUInt32 *ctxId)
{
	(void)session;
	return npImpsSvcAbortContext(*ctxId);
}

/* 0x812F5C98 */
int npImpsUtilHandleWaitRosterCtx(IPMI::Session *session, const SceUInt32 *ctxId)
{
	(void)session;
	return npImpsSvcWaitContext(*ctxId);
}

/* Roster async completion callbacks (0x812F5FEA..0x812F634F) */
static void finishRosterAsyncReply(SceUInt32 replyMethodId, SceUInt32 callbackIndex,
				   int result, SceUInt32 contextId, const SceNpId *npId)
{
	SceNpId npIdCopy = *npId;
	NpImpsUtilAsyncRelayMgr *relayMgr = NpImpsUtilAsyncRelayMgr::GetInstance();
	NpImpsAsyncRelayRecord rec;
	if (relayMgr->LookupContextRelay(contextId, &rec) >= 0) {
		if (npImpsUtilSendRosterReply(rec.session, replyMethodId, rec.requestId, 0,
					      &contextId, &npIdCopy, &result) >= 0) {
			npImpsUtilGetIpmiServer()->signalCallback(rec.session, callbackIndex);
		}
		SceShell_794BF5BE(rec.netLockToken);
		relayMgr->UnregisterContextRelay(contextId);
	}
}

/* 0x812F5FEA */
static void onRequestFriendComplete(int result, SceUInt32 contextId, const SceNpId *npId)
{
	finishRosterAsyncReply(NP_IMPS_METHOD_REQUEST_FRIEND_RESULT, 0, result, contextId, npId);
}

/* 0x812F6146 */
static void onAcceptFriendComplete(int result, SceUInt32 contextId, const SceNpId *npId)
{
	finishRosterAsyncReply(NP_IMPS_METHOD_ACCEPT_FRIEND_RESULT, 1, result, contextId, npId);
}

/* 0x812F6098 */
static void onDeleteFriendComplete(int result, SceUInt32 contextId, const SceNpId *npId)
{
	finishRosterAsyncReply(NP_IMPS_METHOD_DELETE_FRIEND_RESULT, 2, result, contextId, npId);
}

/* 0x812F61F4 */
static void onAddBlockComplete(int result, SceUInt32 contextId, const SceNpId *npId)
{
	finishRosterAsyncReply(NP_IMPS_METHOD_ADD_BLOCK_RESULT, 3, result, contextId, npId);
}

/* 0x812F62A2 */
static void onDeleteBlockComplete(int result, SceUInt32 contextId, const SceNpId *npId)
{
	finishRosterAsyncReply(NP_IMPS_METHOD_DELETE_BLOCK_RESULT, 4, result, contextId, npId);
}

/* 0x812F5CA2 */
int npImpsUtilHandleRequestFriendAsync(IPMI::Session *session, SceUInt32 requestId,
				       const SceUInt32 *ctxId, const SceNpId *npId,
				       const char *comment)
{
	NpImpsUtilAsyncRelayMgr *relayMgr = NpImpsUtilAsyncRelayMgr::GetInstance();
	int lockToken = SceShell_6B8772A2(session->getProcessId());
	if (lockToken < 0)
		return lockToken;

	NpImpsAsyncRelayRecord rec;
	rec.session = session;
	rec.requestId = requestId;
	rec.netLockToken = lockToken;

	int ret = relayMgr->RegisterContextRelay(*ctxId, &rec);
	if (ret < 0) {
		SceShell_794BF5BE(lockToken);
		return ret;
	}

	ret = npImpsSvcRequestFriend(*ctxId, npId, comment, onRequestFriendComplete);
	if (ret < 0) {
		ret = relayMgr->UnregisterContextRelay(*ctxId);
		SceShell_794BF5BE(lockToken);
	}
	return ret;
}

/* 0x812F5D36 */
int npImpsUtilHandleAcceptFriendAsync(IPMI::Session *session, SceUInt32 requestId,
				      const SceUInt32 *ctxId, const SceNpId *npId)
{
	NpImpsUtilAsyncRelayMgr *relayMgr = NpImpsUtilAsyncRelayMgr::GetInstance();
	int lockToken = SceShell_6B8772A2(session->getProcessId());
	if (lockToken < 0)
		return lockToken;

	NpImpsAsyncRelayRecord rec;
	rec.session = session;
	rec.requestId = requestId;
	rec.netLockToken = lockToken;

	int ret = relayMgr->RegisterContextRelay(*ctxId, &rec);
	if (ret < 0) {
		SceShell_794BF5BE(lockToken);
		return ret;
	}

	ret = npImpsSvcAcceptFriend(*ctxId, npId, onAcceptFriendComplete);
	if (ret < 0) {
		ret = relayMgr->UnregisterContextRelay(*ctxId);
		SceShell_794BF5BE(lockToken);
	}
	return ret;
}

/* 0x812F5DC0 */
int npImpsUtilHandleDeleteFriendAsync(IPMI::Session *session, SceUInt32 requestId,
				      const SceUInt32 *ctxId, const SceNpId *npId)
{
	NpImpsUtilAsyncRelayMgr *relayMgr = NpImpsUtilAsyncRelayMgr::GetInstance();
	int lockToken = SceShell_6B8772A2(session->getProcessId());
	if (lockToken < 0)
		return lockToken;

	NpImpsAsyncRelayRecord rec;
	rec.session = session;
	rec.requestId = requestId;
	rec.netLockToken = lockToken;

	int ret = relayMgr->RegisterContextRelay(*ctxId, &rec);
	if (ret < 0) {
		SceShell_794BF5BE(lockToken);
		return ret;
	}

	ret = npImpsSvcDeleteFriend(*ctxId, npId, onDeleteFriendComplete);
	if (ret < 0) {
		ret = relayMgr->UnregisterContextRelay(*ctxId);
		SceShell_794BF5BE(lockToken);
	}
	return ret;
}

/* 0x812F5E4A */
int npImpsUtilHandleAddBlockAsync(IPMI::Session *session, SceUInt32 requestId,
				  const SceUInt32 *ctxId, const SceNpId *npId)
{
	NpImpsUtilAsyncRelayMgr *relayMgr = NpImpsUtilAsyncRelayMgr::GetInstance();
	int lockToken = SceShell_6B8772A2(session->getProcessId());
	if (lockToken < 0)
		return lockToken;

	NpImpsAsyncRelayRecord rec;
	rec.session = session;
	rec.requestId = requestId;
	rec.netLockToken = lockToken;

	int ret = relayMgr->RegisterContextRelay(*ctxId, &rec);
	if (ret < 0) {
		SceShell_794BF5BE(lockToken);
		return ret;
	}

	if (npImpsSvcAddBlock(*ctxId, npId, onAddBlockComplete) >= 0)
		return 0;

	ret = relayMgr->UnregisterContextRelay(*ctxId);
	SceShell_794BF5BE(lockToken);
	return ret;
}

/* 0x812F5ED6 */
int npImpsUtilHandleDeleteBlockAsync(IPMI::Session *session, SceUInt32 requestId,
				     const SceUInt32 *ctxId, const SceNpId *npId)
{
	NpImpsUtilAsyncRelayMgr *relayMgr = NpImpsUtilAsyncRelayMgr::GetInstance();
	int lockToken = SceShell_6B8772A2(session->getProcessId());
	if (lockToken < 0)
		return lockToken;

	NpImpsAsyncRelayRecord rec;
	rec.session = session;
	rec.requestId = requestId;
	rec.netLockToken = lockToken;

	int ret = relayMgr->RegisterContextRelay(*ctxId, &rec);
	if (ret < 0) {
		SceShell_794BF5BE(lockToken);
		return ret;
	}

	ret = npImpsSvcDeleteBlock(*ctxId, npId, onDeleteBlockComplete);
	if (ret < 0) {
		ret = relayMgr->UnregisterContextRelay(*ctxId);
		SceShell_794BF5BE(lockToken);
	}
	return ret;
}

/* 0x812F6350 */
void npImpsUtilHandleSessionClosedCleanup(SceUID processId)
{
	NpImpsUtilAsyncRelayMgr *relayMgr = NpImpsUtilAsyncRelayMgr::GetInstance();
	if (relayMgr == NULL)
		return;

	SceUIntPtr opKey = reinterpret_cast<SceUIntPtr>(&npImpsCoreConnectProcess);
	relayMgr->Lock();
	NpImpsAsyncRelayRecord rec;
	if (relayMgr->LookupProcessOpRelay(processId, opKey, &rec) >= 0) {
		SceShell_794BF5BE(rec.netLockToken);
		relayMgr->UnregisterProcessOpRelay(processId, opKey);
	}
	relayMgr->Unlock();
}

/* 0x812F63B6 */
static int completeEnterOnlineAsync(SceUID processId, int errorCode)
{
	NpImpsUtilAsyncRelayMgr *relayMgr = NpImpsUtilAsyncRelayMgr::GetInstance();
	if (relayMgr == NULL)
		return 0;

	SceUIntPtr opKey = reinterpret_cast<SceUIntPtr>(&npImpsCoreConnectProcess);
	relayMgr->Lock();

	NpImpsAsyncRelayRecord rec;
	int resultCode = errorCode;
	if (relayMgr->LookupProcessOpRelay(processId, opKey, &rec) >= 0) {
		npImpsUtilSendEnterOnlineReply(rec.session, rec.requestId, 0, &resultCode);
		npImpsUtilGetIpmiServer()->signalCallback(rec.session, 0);
	}

	if (relayMgr->LookupProcessOpRelay(processId, opKey, &rec) >= 0) {
		SceShell_794BF5BE(rec.netLockToken);
		relayMgr->UnregisterProcessOpRelay(processId, opKey);
	}

	relayMgr->Unlock();
	return 0;
}

/* 0x812F66F4 */
static void onCoreConnectComplete(SceUID processId, int errorCode)
{
	if (s_mpStateThread != NULL) {
		s_mpStateThread->SetConnectError(processId, errorCode);
		if (errorCode != 0)
			s_mpStateThread->PostEvent(NP_IMPS_MP_EVT_ENTER_ONLINE_FAIL, processId, 0);
	}
}

/* 0x812F64D2 */
void npImpsUtilHandleEnterOnlineAsync(IPMI::Session *session, SceUInt32 requestId,
				      SceUInt8 requireAlreadyOnline)
{
	SceUID processId = session->getProcessId();
	NpImpsUtilAsyncRelayMgr *relayMgr = NpImpsUtilAsyncRelayMgr::GetInstance();
	SceUIntPtr opKey = reinterpret_cast<SceUIntPtr>(&npImpsCoreConnectProcess);
	int zeroResult = 0;

	int lockToken = SceShell_6B8772A2(processId);
	if (lockToken < 0) {
		npImpsUtilSendEnterOnlineReply(session, requestId, lockToken, &zeroResult);
		npImpsUtilGetIpmiServer()->signalCallback(session, 0);
		return;
	}

	NpImpsAsyncRelayRecord rec;
	rec.session = session;
	rec.requestId = requestId;
	rec.netLockToken = lockToken;

	int ret = relayMgr->RegisterProcessOpRelay(processId, opKey, &rec);
	if (ret < 0) {
		SceShell_794BF5BE(lockToken);
		npImpsUtilSendEnterOnlineReply(session, requestId, ret, &zeroResult);
		npImpsUtilGetIpmiServer()->signalCallback(session, 0);
		return;
	}

	int globalState = 0;
	ret = npImpsSvcGetServiceState(&globalState);
	if (ret >= 0) {
		NpImpsCoreConnectCallback connectCb = onCoreConnectComplete;
		if (requireAlreadyOnline != 0) {
			if (globalState != NP_IMPS_SVC_STATE_ONLINE) {
				ret = NP_IMPS_UTIL_ERROR_NOT_ONLINE;
			} else {
				s_mpStateThread->SetConnectError(processId, 0);
				connectCb = onCoreConnectNoop;
			}
		}

		if (ret >= 0) {
			ret = s_mpStateThread->PostEvent(NP_IMPS_MP_EVT_ENTER_ONLINE_REQ,
							 processId, 0);
			if (ret >= 0) {
				ret = npImpsCoreConnectProcess(processId, connectCb);
				if (ret >= 0)
					return;
				s_mpStateThread->PostEvent(NP_IMPS_MP_EVT_ENTER_ONLINE_FAIL,
							   processId, 0);
			}
		}
	}

	relayMgr->UnregisterProcessOpRelay(processId, opKey);
	SceShell_794BF5BE(lockToken);
	npImpsUtilSendEnterOnlineReply(session, requestId, ret, &zeroResult);
	npImpsUtilGetIpmiServer()->signalCallback(session, 0);
}

/* 0x812F646C & 0x812F649C */
static int leaveOnlineForProcess(SceUID processId)
{
	int ret = 0;
	int postRet = s_mpStateThread->PostEvent(NP_IMPS_MP_EVT_LEAVE_ONLINE_REQ, processId, 0);
	if (postRet < 0)
		ret = postRet;
	int discRet = npImpsCoreDisconnectProcess(processId);
	if (discRet < 0 && ret == 0)
		ret = discRet;
	return ret;
}

int npImpsUtilHandleLeaveOnline(IPMI::Session *session)
{
	return leaveOnlineForProcess(session->getProcessId());
}

/* 0x812F6628 */
int npImpsUtilHandleConnectCore(IPMI::Session *session)
{
	SceUID processId = session->getProcessId();
	int lockToken = SceShell_6B8772A2(processId);
	if (lockToken < 0)
		return lockToken;

	NpImpsUtilAsyncRelayMgr *relayMgr = NpImpsUtilAsyncRelayMgr::GetInstance();
	SceUIntPtr opKey = reinterpret_cast<SceUIntPtr>(&npImpsCoreConnectProcess);

	NpImpsAsyncRelayRecord rec;
	rec.session = session;
	rec.requestId = 0;
	rec.netLockToken = lockToken;

	int ret = relayMgr->RegisterProcessOpRelay(processId, opKey, &rec);
	if (ret < 0) {
		SceShell_794BF5BE(lockToken);
		return ret;
	}

	ret = npImpsCoreConnectProcess(processId, onCoreConnectNoop);
	if (ret < 0) {
		relayMgr->UnregisterProcessOpRelay(processId, opKey);
		SceShell_794BF5BE(lockToken);
	}
	return ret;
}

/* 0x812F6CB2 */
int npImpsUtilHandleDisconnectCore(IPMI::Session *session)
{
	return leaveOnlineForProcess(session->getProcessId());
}

/* 0x812F66B0 */
int npImpsUtilHandleIsGlobalOnline(IPMI::Session *session, SceUInt8 *outOnline)
{
	(void)session;
	int globalState = 0;
	int ret = npImpsSvcGetServiceState(&globalState);
	if (ret >= 0)
		*outOnline = (globalState == NP_IMPS_SVC_STATE_ONLINE) ? 1 : 0;
	return ret;
}

/* 0x812F6C90 */
int npImpsUtilHandleGetServiceState(IPMI::Session *session, int *outState)
{
	return s_mpStateThread->GetServiceState(session->getProcessId(), outState, 0);
}

/* 0x812F6CC0 */
int npImpsUtilHandleGetRawServiceState(IPMI::Session *session, int *outState)
{
	return s_mpStateThread->GetServiceState(session->getProcessId(), outState, 1);
}

/* 0x812F6724..0x812F689D & 0x812F5F60..0x812F5FE9 */
int npImpsUtilHandleGetAcceptedFriendCount(IPMI::Session *session, SceUInt32 *outCount,
					   SceUInt8 *outLoaded)
{
	(void)session;
	return npImpsSvcGetAcceptedFriendCount(outCount, outLoaded);
}

int npImpsUtilHandleGetAcceptedFriendList(IPMI::Session *session, SceUInt32 startIndex,
					  SceNpId *outEntries, SceUInt32 maxCount,
					  SceUInt32 *outCount, SceUInt8 *outLoaded)
{
	(void)session;
	return npImpsSvcGetAcceptedFriendEntries(startIndex, outEntries, maxCount,
						 outCount, outLoaded);
}

int npImpsUtilHandleGetPendingFriendCount(IPMI::Session *session, SceUInt32 *outCount,
					  SceUInt8 *outLoaded)
{
	(void)session;
	return npImpsSvcGetPendingFriendCount(outCount, outLoaded);
}

int npImpsUtilHandleGetPendingFriendList(IPMI::Session *session, SceUInt32 startIndex,
					 SceNpId *outEntries, SceUInt32 maxCount,
					 SceUInt32 *outCount, SceUInt8 *outLoaded)
{
	(void)session;
	return npImpsSvcGetPendingFriendEntries(startIndex, outEntries, maxCount,
						outCount, outLoaded);
}

int npImpsUtilHandleGetRequestedFriendCount(IPMI::Session *session, SceUInt32 *outCount,
					    SceUInt8 *outLoaded)
{
	(void)session;
	return npImpsSvcGetRequestedFriendCount(outCount, outLoaded);
}

int npImpsUtilHandleGetRequestedFriendList(IPMI::Session *session, SceUInt32 startIndex,
					   SceNpId *outEntries, SceUInt32 maxCount,
					   SceUInt32 *outCount, SceUInt8 *outLoaded)
{
	(void)session;
	return npImpsSvcGetRequestedFriendEntries(startIndex, outEntries, maxCount,
						  outCount, outLoaded);
}

int npImpsUtilHandleGetAllFriendCount(IPMI::Session *session, SceUInt32 *outCount)
{
	(void)session;
	return npImpsSvcGetFriendCount(outCount);
}

int npImpsUtilHandleGetAllFriendList(IPMI::Session *session, SceUInt32 startIndex,
				     SceNpId *outEntries, SceUInt32 maxCount, SceUInt32 *outCount)
{
	(void)session;
	return npImpsSvcGetFriendEntries(startIndex, outEntries, maxCount, outCount);
}

int npImpsUtilHandleIsOnline(IPMI::Session *session, SceUInt8 *outOnline)
{
	(void)session;
	int online = 0;
	int ret = npImpsSvcIsOnline(&online);
	if (ret >= 0) {
		*outOnline = (online != 0) ? 1 : 0;
		return 0;
	}
	return ret;
}

/* 0x812F6796 */
int npImpsUtilHandleGetFriendContextGame(IPMI::Session *session, const SceNpId *npId,
					 int *outContext)
{
	int procState = 0;
	s_mpStateThread->GetServiceState(session->getProcessId(), &procState, 0);
	if (procState != NP_IMPS_SVC_STATE_ONLINE) {
		*outContext = 0;
		return NP_IMPS_UTIL_ERROR_OFFLINE;
	}
	return npImpsSvcGetFriendContextByNpId(npId, outContext);
}

/* 0x812F5FBC */
int npImpsUtilHandleGetFriendContextSys(IPMI::Session *session, const SceNpId *npId,
					int defaultVal, int *outContext)
{
	(void)session;
	*outContext = defaultVal;
	return npImpsSvcGetFriendContextByNpId(npId, outContext);
}

/* 0x812F67F8 */
int npImpsUtilHandleGetFriendPresenceGame(IPMI::Session *session, const SceNpId *npId,
					  NpImpsSvcPresenceSnapshot *outSnap)
{
	int ret = npImpsSvcGetFriendPresenceByNpId(npId, outSnap);
	if (ret < 0)
		return ret;

	int procState = 0;
	ret = s_mpStateThread->GetServiceState(session->getProcessId(), &procState, 0);
	if (procState != NP_IMPS_SVC_STATE_ONLINE) {
		sce_paf_memset(outSnap, 0, sizeof(*outSnap));
		outSnap->onlineStatus = 0;
		return NP_IMPS_UTIL_ERROR_OFFLINE;
	}
	return ret;
}

/* 0x812F5FCA */
int npImpsUtilHandleGetFriendPresenceSys(IPMI::Session *session, const SceNpId *npId,
					 NpImpsSvcPresenceSnapshot *outSnap)
{
	(void)session;
	return npImpsSvcGetFriendPresenceByNpId(npId, outSnap);
}

/* 0x812F6BCE */
int npImpsUtilHandleGetFriendGamePresence(IPMI::Session *session, const SceNpId *npId,
					  NpImpsGamePresenceWithTitle *outPresence)
{
	int procState = 0;
	s_mpStateThread->GetServiceState(session->getProcessId(), &procState, 0);
	if (procState != NP_IMPS_SVC_STATE_ONLINE)
		return NP_IMPS_UTIL_ERROR_OFFLINE;
	return npImpsSvcGetFriendGamePresence(npId, outPresence);
}

int npImpsUtilHandleGetBlockListCount(IPMI::Session *session, SceUInt32 *outCount,
				      SceUInt8 *outLoaded)
{
	(void)session;
	return npImpsSvcGetBlockListCount(outCount, outLoaded);
}

int npImpsUtilHandleGetBlockListEntries(IPMI::Session *session, SceUInt32 startIndex,
					SceNpId *outEntries, SceUInt32 maxCount,
					SceUInt32 *outCount, SceUInt8 *outLoaded)
{
	(void)session;
	return npImpsSvcGetBlockListEntries(startIndex, outEntries, maxCount, outCount, outLoaded);
}

int npImpsUtilHandleIsBlocked(IPMI::Session *session, const SceNpId *npId,
			      bool *outBlocked, SceUInt8 *outLoaded)
{
	(void)session;
	return npImpsSvcIsBlocked(npId, outBlocked, outLoaded);
}

/* 0x812F689E */
int npImpsUtilHandleSetPartyPresence(IPMI::Session *session, const NpImpsPartyPresence *party)
{
	(void)session;
	if (party->partyDataSize > 128u)
		return NP_IMPS_UTIL_ERROR_INVALID_SIZE;
	return npImpsSvcSetPartyPresence(party);
}

/* 0x812F68BA */
int npImpsUtilHandleClearPartyPresence(IPMI::Session *session)
{
	(void)session;
	return npImpsSvcClearPartyPresence();
}

/* 0x812F68C2 */
int npImpsUtilHandleSetCommId(IPMI::Session *session, const SceNpCommunicationId *commId)
{
	SceUID processId = session->getProcessId();
	int ret = npImpsSvcSetCommId(processId, commId);
	if (ret < 0)
		return ret;

	ret = s_mpStateThread->PostEvent(NP_IMPS_MP_EVT_SET_COMM_ID, processId, 0);
	if (ret >= 0 && commId != NULL) {
		char commIdStr[13];
		sce_paf_memset(commIdStr, 0, sizeof(commIdStr));
		if (SceNpCommon_E426FEE8(commId, commIdStr, sizeof(commIdStr)) >= 0) {
			char tagBuf[32];
			sce_paf_memset(tagBuf, 0, sizeof(tagBuf));
			sceClibStrlcpy(tagBuf, "NpBasic", sizeof(tagBuf));
			SceNpManager_9554802B("NpBasic", commIdStr, tagBuf);
		}
	}
	return ret;
}

/* 0x812F6964 */
int npImpsUtilHandleClearCommId(IPMI::Session *session)
{
	int ret = npImpsSvcClearCommId();
	if (ret >= 0)
		return s_mpStateThread->PostEvent(NP_IMPS_MP_EVT_CLEAR_COMM_ID,
						  session->getProcessId(), 0);
	return ret;
}

/* 0x812F698E */
int npImpsUtilHandleSetGamePresence(IPMI::Session *session, const NpImpsGamePresenceInput *input)
{
	if (input->gameDataSize > 128u)
		return NP_IMPS_UTIL_ERROR_INVALID_SIZE;

	int ret = s_mpStateThread->SetPendingGamePresence(session->getProcessId(), input);
	if (ret >= 0)
		SceNpManager_9554802B("NpBasic", "Set presence", NULL);
	return ret;
}

/* 0x812F69DE */
int npImpsUtilHandleClearGamePresence(IPMI::Session *session)
{
	return s_mpStateThread->ClearPendingGamePresence(session->getProcessId());
}

/* 0x812F69FA */
int npImpsUtilHandleSendInGameData(IPMI::Session *session, const SceNpId *targetNpId,
				   const void *msgData, const SceNpCommunicationId *commId)
{
	SceUID processId = session->getProcessId();
	int procState = 0;
	s_mpStateThread->GetServiceState(processId, &procState, 0);

	if (procState == NP_IMPS_SVC_STATE_SIGNED_OUT)
		return NP_IMPS_UTIL_ERROR_SIGNED_OUT;
	if (procState == NP_IMPS_SVC_STATE_SIGNED_IN)
		return NP_IMPS_UTIL_ERROR_OFFLINE;
	if (procState != NP_IMPS_SVC_STATE_ONLINE)
		return NP_IMPS_UTIL_ERROR_STATE_INVALID;

	SceUInt32 sdkVersion = 0;
	int ret = vshKernelGetCompiledSdkVersionByPid(processId, &sdkVersion);
	if (ret < 0)
		return ret;

	struct ModernInGameData {
		SceUInt8 bytes[512];
		SceSize size;
	};
	struct LegacyInGameData {
		SceUInt8 bytes[128];
		SceSize size;
	};

	SceSize payloadSize = (sdkVersion >= 0x02000000u)
		? static_cast<const ModernInGameData *>(msgData)->size
		: static_cast<const LegacyInGameData *>(msgData)->size;

	NpImpsCoreDataNode dataNode;
	sce_paf_memset(&dataNode, 0, sizeof(dataNode));
	dataNode.valid = 1;
	dataNode.dataPtr = const_cast<void *>(msgData);
	dataNode.dataSize = payloadSize;
	dataNode.commId = *commId;
	dataNode.targetCommId = *commId;

	NpImpsCoreInGameMessage coreMsg;
	sce_paf_memset(&coreMsg, 0, sizeof(coreMsg));
	coreMsg.dataNode = &dataNode;

	SceNpId resolvedNpId = *targetNpId;
	if (sdkVersion >= 0x01600000u) {
		int platRet = sceNpGetPlatformType(&resolvedNpId);
		if (platRet < 0)
			return platRet;
		if (platRet == 0) {
			ret = SceNpCommon_256670D6(&resolvedNpId, 1);
			if (ret < 0)
				return ret;
		}
	} else {
		ret = SceNpCommon_256670D6(&resolvedNpId, 1);
		if (ret < 0)
			return ret;
	}

	int sendResultToken = 0;
	return npImpsCoreSendInGameMessage(&resolvedNpId, &coreMsg,
					   onCoreSendInGameMsgNoop, &sendResultToken);
}

/* 0x812F6B7E */
int npImpsUtilHandleAddPlayLog(IPMI::Session *session, const paf::rtc::Tick *date,
			       const SceNpId *npId, const SceNpCommunicationId *commId,
			       const char *description)
{
	return npFriendSessionLogAdd(date, npId, commId, description, session->getAppId());
}

/* 0x812F6BA2 */
int npImpsUtilHandleGetPlayLogCount(IPMI::Session *session, int queryKind,
				    const SceNpCommunicationId *commId, SceUInt32 *outCount)
{
	(void)session;
	return npFriendSessionLogGetCount(queryKind, commId, outCount);
}

/* 0x812F6BB0 */
int npImpsUtilHandleGetPlayLogList(IPMI::Session *session, int queryKind,
				   const SceNpCommunicationId *commId, SceUInt32 startIndex,
				   void *outEntries, SceUInt32 maxCount, SceUInt32 *outCount)
{
	(void)session;
	return npFriendSessionLogGetList(queryKind, commId, startIndex, outEntries,
					 maxCount, outCount);
}

/* 0x812F5FD6 */
int npImpsUtilHandleDeletePlayLog(IPMI::Session *session, SceUInt32 dateLow, SceUInt32 dateHigh)
{
	(void)session;
	return npFriendSessionLogDelete(dateLow, dateHigh);
}

/* 0x812F5FE2 */
int npImpsUtilHandleDeleteAllPlayLog(IPMI::Session *session)
{
	(void)session;
	return npFriendSessionLogDeleteAll();
}

/* 0x812F6C2A */
int npImpsUtilHandleForceRosterSync(IPMI::Session *session, int syncFlags)
{
	(void)session;
	return npImpsSvcForceRosterSync(syncFlags);
}

/* 0x812F6C34 */
int npImpsUtilHandleGetMyStatusAndPresence(IPMI::Session *session, int *outStatus,
					   NpImpsSvcPresenceSnapshot *outSnap)
{
	(void)session;
	*outStatus = 0;
	sce_paf_memset(outSnap, 0, sizeof(*outSnap));
	int ret = npImpsSvcGetMyOnlineStatus(outStatus);
	if (ret >= 0)
		ret = npImpsSvcGetMyPresence(outSnap);
	return ret;
}

/* 0x812F6C5C */
int npImpsUtilHandleGetPlayerPlayLogCount(IPMI::Session *session, const SceNpId *npId,
					  SceUInt32 *outCount)
{
	(void)session;
	*outCount = 0;
	return npFriendPlayerSessionLogGetCount(npId, outCount);
}

/* 0x812F6C6C */
int npImpsUtilHandleGetPlayerPlayLog(IPMI::Session *session, const SceNpId *npId,
				     SceUInt32 index, void *outEntry)
{
	(void)session;
	sce_paf_memset(outEntry, 0, 0x2B8u);
	return npFriendPlayerSessionLogGet(npId, index, outEntry);
}
