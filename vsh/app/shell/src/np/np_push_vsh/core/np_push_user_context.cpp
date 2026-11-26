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
	  m_lastSyncTick(0),
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

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */
