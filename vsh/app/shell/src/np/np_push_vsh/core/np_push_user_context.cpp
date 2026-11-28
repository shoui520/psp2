/*
 * np_push_user_context.cpp: Per-user presence synchronization state machine
 * (login, logout, changeStatus, 120s grace period, and retry backoff).
 * Sony path: vsh/app/common/src/np/np_push_vsh/core/np_push_user_context.cpp
 * 3.65-CEX: 0x813B1A28..0x813B24BF
 */
#include "../np_push_internal.h"

namespace sce {
namespace Np {
namespace Push {

struct UserRetryTarget {
	const void *const *vtable;
	UserContext *owner;
};

static void UserRetryTargetInvoke(UserRetryTarget *target)
{
	if (target != NULL && target->owner != NULL)
		target->owner->OnRetryTimeout();
}

static const void *const kUserRetryTargetVtable[] = {
	NULL,
	NULL,
	reinterpret_cast<const void *>(UserRetryTargetInvoke)
};

/* 0x813B1A28 */
UserContext::UserContext(const NpPushOnlineId *onlineId, Handler *handler)
	: m_desiredStatus(PRESENCE_STATUS_OFFLINE),
	  m_serverStatus(PRESENCE_STATUS_OFFLINE),
	  m_pendingTargetStatus(PRESENCE_STATUS_OFFLINE),
	  m_activeRequestId(0),
	  m_lastSyncTick(0),
	  m_graceStartTick(0),
	  m_retryStartTick(0),
	  m_retryCount(0),
	  m_retryIntervalSec(NP_PUSH_DEFAULT_RETRY_INTERVAL_SEC),
	  m_handler(handler)
{
	if (onlineId != NULL)
		SceNpCommonPs4_AEC0DAC5(&m_onlineId, onlineId);
	else
		SceNpCommonPs4_B0BAB9BE(&m_onlineId);
	SceNpCommonPs4_1CC3A645(&m_retryJob, GetCalloutContext());
}

/* 0x813B1A80 */
UserContext::~UserContext()
{
	NP_PUSH_ASSERT(m_activeRequest.IsNull(), "~UserContext",
		       "core\\np_push_user_context.cpp", 64, "m_activeRequest == NULL");

	CancelRetry();
	SceNpCommonPs4_941B0290(&m_retryJob);
	SceNpCommonPs4_1543D7EE(&m_onlineId);
}

/* 0x813B1B60 */
void UserContext::ResetOnDisconnect(void)
{
	CancelPendingRequests();
	CancelRetry();
	m_serverStatus = PRESENCE_STATUS_OFFLINE;
	m_lastSyncTick = 0;
}

/* 0x813B1BE0: RequestStart */
int UserContext::RequestStart(Session *session, NpPushHandle *cancelHandle)
{
	NP_PUSH_ASSERT(session != NULL, "RequestStart",
		       "core\\np_push_user_context.cpp", 112, "session != NULL");

	if (!m_activeRequest.IsNull())
		return 0;

	if (m_desiredStatus == m_serverStatus)
		return 0;

	RefPtr<Packet> packet;
	int ret = 0;
	if (m_serverStatus == PRESENCE_STATUS_OFFLINE &&
	    m_desiredStatus != PRESENCE_STATUS_OFFLINE) {
		ret = Packet::CreateLoginRequest(&m_onlineId, m_desiredStatus, &packet);
	} else if (m_serverStatus != PRESENCE_STATUS_OFFLINE &&
		   m_desiredStatus == PRESENCE_STATUS_OFFLINE) {
		ret = Packet::CreateLogoutRequest(&m_onlineId, &packet);
	} else {
		ret = Packet::CreateChangeStatusRequest(&m_onlineId, m_desiredStatus, &packet);
	}
	if (ret < 0)
		return ret;

	Request *req = Request::Create(REQUEST_TYPE_TRANSACTION, &m_onlineId, packet, this, NULL);
	if (req == NULL)
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	m_activeRequest.Adopt(req);
	ret = m_activeRequest->Start(session, cancelHandle);
	if (ret < 0) {
		m_activeRequest.Reset();
		return ret;
	}

	return 0;
}

/* 0x813B1E40: StartPresenceOnlineRequest */
int UserContext::StartPresenceOnlineRequest(const RefPtr<UserRequest> &userRequest,
					    Session *session, NpPushHandle *cancelHandle)
{
	NP_PUSH_ASSERT(!userRequest.IsNull(), "StartPresenceOnlineRequest",
		       "core\\np_push_user_context.cpp", 198, "userRequest != NULL");

	m_desiredStatus = PRESENCE_STATUS_ACTIVE;
	if (m_serverStatus == PRESENCE_STATUS_ACTIVE && m_activeRequest.IsNull()) {
		SceRtcTick nowTick;
		sceClibMemset(&nowTick, 0, sizeof(nowTick));
		SceNpCommonPs4_477BD360(&nowTick);
		SceUInt64 remaining = 0;
		SceInt64 nowVal = static_cast<SceInt64>(nowTick.tick);
		if (m_lastSyncTick != 0 &&
		    SceNpCommonPs4_7DABC125(&nowVal, &m_lastSyncTick,
					    static_cast<SceUInt64>(NP_PUSH_GRACE_PERIOD_SEC) * 1000000ULL,
					    &remaining) == 0) {
			return 0;
		}
	}

	m_pendingUserRequest = userRequest;
	int ret = RequestStart(session, cancelHandle);
	if (ret < 0) {
		m_pendingUserRequest.Reset();
		return ret;
	}
	return 0;
}

/* 0x813B2040: SetUserRequestResult */
void UserContext::SetUserRequestResult(int result)
{
	NP_PUSH_ASSERT(!m_pendingUserRequest.IsNull(), "SetUserRequestResult",
		       "core\\np_push_user_context.cpp", 252, "m_pendingUserRequest != NULL");

	m_pendingUserRequest->SetResult(result);
	m_pendingUserRequest->InvokeCallback();
	m_pendingUserRequest.Reset();
}

/* 0x813B2100 */
void UserContext::CancelPendingRequests(void)
{
	if (!m_activeRequest.IsNull()) {
		m_activeRequest->Cancel();
		m_activeRequest.Reset();
	}
	if (!m_pendingUserRequest.IsNull())
		SetUserRequestResult(SCE_NP_PUSH_ERROR_ABORTED);
}

/* 0x813B2190 */
void UserContext::ScheduleRetry(SceUInt32 delaySec)
{
	CancelRetry();
	SceUInt32 effectiveDelay = (delaySec > 0) ? delaySec : m_retryIntervalSec;
	UserRetryTarget *target = static_cast<UserRetryTarget *>(
		SceNpCommonPs4_AA980113(GetAllocator(), sizeof(UserRetryTarget)));
	if (target == NULL)
		return;

	target->vtable = kUserRetryTargetVtable;
	target->owner = this;
	SceNpCommonPs4_ED3D9A81(&m_retryJob, effectiveDelay * 1000U, target);
}

/* 0x813B2240 */
void UserContext::CancelRetry(void)
{
	SceNpCommonPs4_7A30E24E(&m_retryJob);
}

/* 0x813B2290 */
void UserContext::OnRetryTimeout(void)
{
	if (m_handler != NULL)
		m_handler->OnUserContextRetryTimer(this);
}

/* 0x813B22D8: Request::Handler::OnRequestFinished */
void UserContext::OnRequestFinished(Request *request)
{
	if (request == NULL)
		return;

	if (request->GetResult() == 0) {
		int reqMethod = PACKET_METHOD_NONE;
		if (!request->GetSendPacket().IsNull())
			request->GetSendPacket()->GetMethod(&reqMethod);

		if (reqMethod == PACKET_METHOD_LOGOUT) {
			m_serverStatus = PRESENCE_STATUS_OFFLINE;
		} else {
			int sentStatus = m_desiredStatus;
			if (!request->GetSendPacket().IsNull())
				request->GetSendPacket()->GetStatus(&sentStatus);
			m_serverStatus = sentStatus;
		}

		SceRtcTick nowTick;
		sceClibMemset(&nowTick, 0, sizeof(nowTick));
		SceNpCommonPs4_477BD360(&nowTick);
		m_lastSyncTick = static_cast<SceInt64>(nowTick.tick);
	} else {
		SceUInt32 retrySec = request->GetRetryInterval();
		if (retrySec > 0)
			m_retryIntervalSec = retrySec;
	}

	if (m_activeRequest.Get() == request)
		m_activeRequest.Reset();

	if (!m_pendingUserRequest.IsNull())
		SetUserRequestResult(request->GetResult());

	if (m_handler != NULL)
		m_handler->OnUserContextRequestFinished(this, request);
}

/* 0x813B1BF2 */
int UserContext::UpdateAccountStatus(const PushAccountSnapshot *snapshot)
{
	if (snapshot->signedIn == 0) {
		if (m_desiredStatus != 4) {
			if (m_desiredStatus == 2)
				m_lastSyncTick = 0;
			if (!m_pendingUserRequest.IsNull())
				SetUserRequestResult(SCE_NP_PUSH_ERROR_SEND_FAILED);
			m_desiredStatus = 4;
		}
		return 0;
	}

	if (m_desiredStatus == 2)
		return 0;

	SceRtcTick monoTick;
	SceRtcTick utcTick;
	sceClibMemset(&monoTick, 0, sizeof(monoTick));
	sceClibMemset(&utcTick, 0, sizeof(utcTick));
	int ret = SceNpCommonPs4_5B904477(&monoTick);
	if (ret < 0)
		return ret;
	ret = SceNpCommonPs4_477BD360(&utcTick);
	if (ret < 0)
		return ret;

	m_lastSyncTick = static_cast<SceInt64>(monoTick.tick);
	m_desiredStatus = 2;
	return 0;
}

/* 0x813B1C82 */
int UserContext::MarkSignedOut(void)
{
	if (m_desiredStatus != 4) {
		if (m_desiredStatus == 2)
			m_lastSyncTick = 0;
		if (!m_pendingUserRequest.IsNull())
			SetUserRequestResult(SCE_NP_PUSH_ERROR_SEND_FAILED);
		m_desiredStatus = 4;
	}
	return 0;
}

/* 0x813B1CAE */
void UserContext::SetServerPresenceState(int nextState)
{
	if (m_serverStatus == nextState)
		return;

	int targetState = nextState;
	if (nextState == 2) {
		if (!m_pendingUserRequest.IsNull())
			SetUserRequestResult(0);
	} else if (m_serverStatus == 2 && nextState == 0) {
		targetState = 1;
		SceRtcTick nowTick;
		sceClibMemset(&nowTick, 0, sizeof(nowTick));
		SceNpCommonPs4_5B904477(&nowTick);
		m_graceStartTick = static_cast<SceInt64>(nowTick.tick);
	}
	m_serverStatus = targetState;
}

/* 0x813B1D0C */
void UserContext::SetInitialServerPresenceState(int nextState)
{
	SetServerPresenceState(nextState);
}

/* 0x813B1D16 */
SceUInt32 UserContext::GetNextTimerDelayUsec(const SceInt64 *nowTick) const
{
	SceUInt32 retryDelay = 0xFFFFFFFFU;
	if (m_retryStartTick != 0) {
		SceUInt64 limitUsec = (m_retryIntervalSec != 0)
					      ? (static_cast<SceUInt64>(m_retryIntervalSec) * 1000000ULL)
					      : 60000000ULL;
		SceUInt64 remaining = 0;
		if (SceNpCommonPs4_7DABC125(nowTick, &m_retryStartTick, limitUsec, &remaining) == 0)
			retryDelay = static_cast<SceUInt32>(remaining);
	}

	SceUInt32 graceDelay = 0xFFFFFFFFU;
	if (m_serverStatus == 1) {
		SceUInt64 remaining = 0;
		if (SceNpCommonPs4_7DABC125(nowTick, &m_graceStartTick, 120000000ULL, &remaining) == 0)
			graceDelay = static_cast<SceUInt32>(remaining);
	}

	return (retryDelay < graceDelay) ? retryDelay : graceDelay;
}

/* 0x813B2244 */
void UserContext::ExpireGracePeriod(const SceInt64 *nowTick, bool *outRemovable)
{
	if (outRemovable != NULL)
		*outRemovable = false;

	SceUInt64 remaining = 0;
	if (m_serverStatus == 1 &&
	    SceNpCommonPs4_7DABC125(nowTick, &m_graceStartTick, 120000000ULL, &remaining) == 0) {
		return;
	}

	if (m_serverStatus == 1)
		SetServerPresenceState(0);

	if (outRemovable != NULL && (m_serverStatus == 0 || m_serverStatus == 4) &&
	    (m_desiredStatus == 0 || m_desiredStatus == 4)) {
		*outRemovable = true;
	}
}

/* 0x813B22B8 */
int UserContext::SyncStateWithSession(Session *session, const SceInt64 *nowTick,
				      int activeIntervalSec, bool *outRemovable)
{
	(void)activeIntervalSec;
	*outRemovable = false;
	if (m_activeRequestId != 0)
		return 0;

	ExpireGracePeriod(nowTick, NULL);
	if ((m_serverStatus == 0 || m_serverStatus == 4) &&
	    (m_desiredStatus == 0 || m_desiredStatus == 4)) {
		*outRemovable = true;
		return 0;
	}

	return RequestStart(session, NULL);
}

/* 0x813B2352 */
int UserContext::HandleRequestResult(const PushRequestCompletion *completion)
{
	if (m_activeRequestId == 0 || m_activeRequestId != completion->requestId)
		return 0;

	if (completion->errorCode != 0) {
		m_activeRequestId = 0;
		m_pendingTargetStatus = 0;
		m_retryCount++;
		m_retryIntervalSec = completion->retryIntervalSec;
		if (!m_pendingUserRequest.IsNull())
			SetUserRequestResult(completion->errorCode);
		return 0;
	}

	int resolvedStatus = m_pendingTargetStatus;
	if (m_pendingTargetStatus == 5) {
		if (completion->serverStatus == 1)
			resolvedStatus = 2;
		else if (completion->serverStatus == 0 || completion->serverStatus == 2)
			resolvedStatus = 3;
		else
			return SCE_NP_PUSH_ERROR_SERVER_SHUTDOWN;
	}

	m_pendingTargetStatus = 0;
	m_activeRequestId = 0;
	m_retryCount = 0;
	m_retryIntervalSec = 0;
	m_retryStartTick = 0;
	SetServerPresenceState(resolvedStatus);
	return 0;
}

/* 0x813B23D4 */
void UserContext::ResetWithError(int errorCode)
{
	if (m_activeRequestId != 0) {
		m_pendingTargetStatus = 0;
		m_activeRequestId = 0;
		m_retryCount = 0;
		m_retryIntervalSec = 0;
		m_retryStartTick = 0;
	}
	SetServerPresenceState(0);
	if (!m_pendingUserRequest.IsNull())
		SetUserRequestResult(errorCode);
}

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */
