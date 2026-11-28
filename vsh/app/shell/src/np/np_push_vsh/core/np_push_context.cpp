/*
 * np_push_context.cpp: Push connection context owning the Session and up to 32 UserContexts.
 * Sony path: vsh/app/common/src/np/np_push_vsh/core/np_push_context.cpp
 * 3.65-CEX: 0x813AF20A..0x813AFDC1
 */
#include "../np_push_internal.h"

namespace sce {
namespace Np {
namespace Push {

/* 0x813AF20A */
Context::Context()
	: m_ownerCtl(NULL),
	  m_session(NULL),
	  m_initialStatusUpdated(false),
	  m_hasSessionOnlineId(false),
	  m_activeIntervalSec(0),
	  m_sleepIntervalSec(0),
	  m_initialized(false)
{
	SceNpCommonPs4_4266D870(&m_mutex);
	SceNpCommonPs4_1CC3A645(&m_updateTimerJob, GetCalloutContext());
	sceClibMemset(m_users, 0, sizeof(m_users));
	SceNpCommonPs4_B0BAB9BE(&m_sessionOnlineId);
	SceNpCommonPs4_2E2B2655(&m_cancelHandle);
}

/* 0x813AF2D8 */
Context::~Context()
{
	Finalize();
	SceNpCommonPs4_62214062(&m_cancelHandle);
	SceNpCommonPs4_1543D7EE(&m_sessionOnlineId);
	SceNpCommonPs4_941B0290(&m_updateTimerJob);
	SceNpCommonPs4_294F0B1A(&m_mutex);
}

/* 0x813AF2E8 */
int Context::InitMutex(void)
{
	int ret = SceNpCommonPs4_8AA7E995(&m_mutex, "SceNpPushContext", 1);
	return (ret >= 0) ? 0 : ret;
}

/* 0x813AF380: Init */
int Context::Init(Session::Handler *sessionHandler, UserContext::Handler *userHandler)
{
	(void)userHandler;
	if (m_initialized)
		return SCE_NP_PUSH_ERROR_ALREADY_INITIALIZED;

	int ret = SceNpCommonPs4_D86527FB(&m_cancelHandle);
	if (ret < 0)
		return ret;

	void *rawSession = SceNpCommonPs4_0C922423(sizeof(Session), GetAllocator());
	if (rawSession == NULL) {
		SceNpCommonPs4_FC2480E5(&m_cancelHandle);
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;
	}

	m_session = new (rawSession) Session();
	ret = m_session->Init();
	if (ret >= 0)
		ret = m_session->RegisterHandler(sessionHandler);
	if (ret >= 0)
		ret = m_session->Start();

	if (ret < 0) {
		m_session->Finalize();
		m_session->~Session();
		SceNpCommonPs4_6B428829(GetAllocator(), m_session);
		m_session = NULL;
		SceNpCommonPs4_FC2480E5(&m_cancelHandle);
		return ret;
	}

	m_initialized = true;
	return 0;
}

/* 0x813AF520: Finalize */
void Context::Finalize(void)
{
	if (!m_initialized)
		return;

	SceNpCommonPs4_6B8D9291(&m_cancelHandle);
	ClearAllUserContexts();

	if (m_session != NULL) {
		m_session->Finalize();
		m_session->~Session();
		SceNpCommonPs4_6B428829(GetAllocator(), m_session);
		m_session = NULL;
	}

	SceNpCommonPs4_FC2480E5(&m_cancelHandle);
	m_hasSessionOnlineId = false;
	m_initialized = false;
}

/* 0x813AF640: FindUserContext */
UserContext *Context::FindUserContext(const NpPushOnlineId *onlineId) const
{
	if (onlineId == NULL)
		return NULL;

	for (int index = 0; index < NP_PUSH_MAX_USERS; ++index) {
		if (m_users[index] != NULL &&
		    SceNpCommonPs4_21B2C0B7(&m_users[index]->GetOnlineId(), onlineId) != 0) {
			return m_users[index];
		}
	}
	return NULL;
}

/* 0x813AF6D0: GetOrCreateUserContext */
UserContext *Context::GetOrCreateUserContext(const NpPushOnlineId *onlineId,
					     UserContext::Handler *userHandler)
{
	if (onlineId == NULL)
		return NULL;

	UserContext *existing = FindUserContext(onlineId);
	if (existing != NULL)
		return existing;

	int freeIndex = -1;
	for (int index = 0; index < NP_PUSH_MAX_USERS; ++index) {
		if (m_users[index] == NULL) {
			freeIndex = index;
			break;
		}
	}
	if (freeIndex < 0)
		return NULL;

	void *rawUser = SceNpCommonPs4_0C922423(sizeof(UserContext), GetAllocator());
	if (rawUser == NULL)
		return NULL;

	UserContext *user = new (rawUser) UserContext(onlineId, userHandler);
	m_users[freeIndex] = user;
	return user;
}

/* 0x813AF7D0: RemoveUserContext */
void Context::RemoveUserContext(const NpPushOnlineId *onlineId)
{
	if (onlineId == NULL)
		return;

	for (int index = 0; index < NP_PUSH_MAX_USERS; ++index) {
		if (m_users[index] != NULL &&
		    SceNpCommonPs4_21B2C0B7(&m_users[index]->GetOnlineId(), onlineId) != 0) {
			m_users[index]->CancelPendingRequests();
			m_users[index]->~UserContext();
			SceNpCommonPs4_6B428829(GetAllocator(), m_users[index]);
			m_users[index] = NULL;
			return;
		}
	}
}

/* 0x813AF890: ClearAllUserContexts */
void Context::ClearAllUserContexts(void)
{
	for (int index = 0; index < NP_PUSH_MAX_USERS; ++index) {
		if (m_users[index] != NULL) {
			m_users[index]->CancelPendingRequests();
			m_users[index]->~UserContext();
			SceNpCommonPs4_6B428829(GetAllocator(), m_users[index]);
			m_users[index] = NULL;
		}
	}
}

/* 0x813AF940: SetSessionOnlineId */
void Context::SetSessionOnlineId(const NpPushOnlineId *onlineId)
{
	if (onlineId != NULL) {
		SceNpCommonPs4_AEC0DAC5(&m_sessionOnlineId, onlineId);
		m_hasSessionOnlineId = true;
	} else {
		m_hasSessionOnlineId = false;
	}
}

/* 0x813AF9B0: UpdateIntervals */
void Context::UpdateIntervals(const ConnectionResponseInfo *respInfo)
{
	if (respInfo == NULL)
		return;
	if (respInfo->hasActiveInterval)
		m_activeIntervalSec = respInfo->activeIntervalSec;
	if (respInfo->hasSleepInterval)
		m_sleepIntervalSec = respInfo->sleepIntervalSec;
}

/* 0x813AFA10: ConnectSession */
int Context::ConnectSession(bool isReconnect)
{
	NP_PUSH_ASSERT(m_session != NULL, "ConnectSession",
		       "core\\np_push_context.cpp", 268, "m_session != NULL");

	if (!m_hasSessionOnlineId)
		return SCE_NP_PUSH_ERROR_USER_NOT_FOUND;

	return m_session->Connect(&m_sessionOnlineId, isReconnect);
}

/* 0x813AFAD0: UpdateInitialSessionStatus */
int Context::UpdateInitialSessionStatus(void)
{
	NP_PUSH_ASSERT(m_session != NULL, "UpdateInitialSessionStatus",
		       "core\\np_push_context.cpp", 294, "m_session != NULL");

	for (int index = 0; index < NP_PUSH_MAX_USERS; ++index) {
		UserContext *user = m_users[index];
		if (user == NULL)
			continue;
		if (SceNpCommonPs4_21B2C0B7(&user->GetOnlineId(), &m_sessionOnlineId) != 0)
			user->SetServerStatus(PRESENCE_STATUS_ACTIVE);
		else
			user->SetServerStatus(PRESENCE_STATUS_OFFLINE);
	}
	return 0;
}

/* 0x813AFBA0: UpdateUserContexts */
int Context::UpdateUserContexts(void)
{
	if (m_session == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_STATE;

	for (int index = 0; index < NP_PUSH_MAX_USERS; ++index) {
		UserContext *user = m_users[index];
		if (user != NULL && user->NeedsSync() && !user->HasActiveRequest())
			user->RequestStart(m_session, &m_cancelHandle);
	}
	return 0;
}

/* 0x813AFC30: HandleRequestResult */
int Context::HandleRequestResult(UserContext *userContext, Request *request)
{
	if (userContext == NULL || request == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	if (request->GetResult() < 0) {
		userContext->ScheduleRetry(request->GetRetryInterval());
		return request->GetResult();
	}

	if (userContext->NeedsSync() && m_session != NULL)
		return userContext->RequestStart(m_session, &m_cancelHandle);
	return 0;
}

/* 0x813AFCE0: StartPresenceOnlineRequest */
int Context::StartPresenceOnlineRequest(const RefPtr<UserRequest> &userRequest)
{
	if (userRequest.IsNull() || m_session == NULL)
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	UserContext *user = FindUserContext(&userRequest->GetOnlineId());
	if (user == NULL)
		return SCE_NP_PUSH_ERROR_USER_NOT_FOUND;

	return user->StartPresenceOnlineRequest(userRequest, m_session, &m_cancelHandle);
}

/* 0x813AFD50: ResetUsersOnDisconnect */
void Context::ResetUsersOnDisconnect(void)
{
	for (int index = 0; index < NP_PUSH_MAX_USERS; ++index) {
		if (m_users[index] != NULL)
			m_users[index]->ResetOnDisconnect();
	}
}

/* 0x813AFD90: HasAnyActiveUsers */
bool Context::HasAnyActiveUsers(void) const
{
	for (int index = 0; index < NP_PUSH_MAX_USERS; ++index) {
		if (m_users[index] != NULL &&
		    m_users[index]->GetDesiredStatus() != PRESENCE_STATUS_OFFLINE) {
			return true;
		}
	}
	return false;
}

/* 0x813AF304 */
int Context::GetSessionId(void)
{
	ScopedLock lock(&m_mutex);
	return (m_session != NULL) ? m_session->GetSessionId() : 0;
}

/* 0x813AF32C */
void Context::ResetAllUsersWithError(int errorCode)
{
	ScopedLock lock(&m_mutex);
	SceNpCommonPs4_7A30E24E(&m_updateTimerJob);
	for (int index = 0; index < NP_PUSH_MAX_USERS; ++index) {
		if (m_users[index] != NULL)
			m_users[index]->ResetWithError(errorCode);
	}
}

/* 0x813AF454 */
SceUInt32 Context::GetSessionRetryInterval(void)
{
	ScopedLock lock(&m_mutex);
	return (m_session != NULL) ? m_session->GetRetryIntervalSec() : 0;
}

/* 0x813AF47E */
void Context::TeardownSession(SceUInt32 *outRetryIntervalSec)
{
	ScopedLock lock(&m_mutex);
	if (outRetryIntervalSec != NULL)
		*outRetryIntervalSec = GetSessionRetryInterval();

	if (m_session != NULL) {
		m_session->StopSessionThread();
		m_session->Finalize();
		m_session->~Session();
		SceNpCommonPs4_6B428829(GetAllocator(), m_session);
		m_session = NULL;
		m_initialStatusUpdated = false;
	}

	ResetAllUsersWithError(SCE_NP_PUSH_ERROR_NOT_CONNECTED);
}

/* 0x813AF50A */
int Context::AllocateUserContext(const NpPushOnlineId *onlineId,
				 UserContext::Handler *userHandler)
{
	ScopedLock lock(&m_mutex);
	int freeIndex = -1;
	for (int index = 0; index < NP_PUSH_MAX_USERS; ++index) {
		if (m_users[index] == NULL) {
			freeIndex = index;
			break;
		}
	}
	if (freeIndex < 0)
		return SCE_NP_PUSH_ERROR_INVALID_PACKET;

	void *rawUser = SceNpCommonPs4_0C922423(sizeof(UserContext), GetAllocator());
	if (rawUser == NULL)
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	m_users[freeIndex] = new (rawUser) UserContext(onlineId, userHandler);
	return freeIndex;
}

/* 0x813AF58E */
UserContext *Context::FindUserContextWithIndex(const NpPushOnlineId *onlineId,
					       SceUInt32 *outIndex)
{
	ScopedLock lock(&m_mutex);
	for (SceUInt32 index = 0; index < NP_PUSH_MAX_USERS; ++index) {
		if (m_users[index] != NULL &&
		    SceNpCommonPs4_21B2C0B7(&m_users[index]->GetOnlineId(), onlineId) != 0) {
			if (outIndex != NULL)
				*outIndex = index;
			return m_users[index];
		}
	}
	if (outIndex != NULL)
		*outIndex = 0xFFFFFFFFU;
	return NULL;
}

/* 0x813AF6AC */
int Context::SyncSignedInUsers(UserContext::Handler *userHandler)
{
	ScopedLock lock(&m_mutex);
	SceUInt32 seenMask[NP_PUSH_MAX_USERS];
	sceClibMemset(seenMask, 0, sizeof(seenMask));

	Ctl *ctl = Ctl::GetInstance();
	if (ctl != NULL && ctl->IsImpsOnline()) {
		char loginId[68];
		char password[36];
		SceUInt64 accountId = 0;
		int ret = npManagerGetCachedCredentials(loginId, password, &accountId);
		if (ret < 0)
			return ret;

		PushAccountSnapshot snapshot;
		sceClibMemset(&snapshot, 0, sizeof(snapshot));
		ret = npAccountGetCachedNpId(&snapshot.npId);
		if (ret < 0)
			return ret;

		snapshot.accountId = accountId;
		snapshot.signedIn = (npAccountIsSignedIn() != 0) ? 1U : 0U;

		NpPushOnlineId onlineId;
		SceNpCommonPs4_D0787194(&onlineId, &snapshot.npId);
		SceUInt32 foundIdx = 0xFFFFFFFFU;
		UserContext *user = FindUserContextWithIndex(&onlineId, &foundIdx);
		SceNpCommonPs4_1543D7EE(&onlineId);

		if (user != NULL) {
			ret = user->UpdateAccountStatus(&snapshot);
			if (ret < 0)
				return ret;
			seenMask[foundIdx] = 1;
		} else if (snapshot.signedIn != 0) {
			NpPushOnlineId newOnlineId;
			SceNpCommonPs4_D0787194(&newOnlineId, &snapshot.npId);
			int slot = AllocateUserContext(&newOnlineId, userHandler);
			SceNpCommonPs4_1543D7EE(&newOnlineId);
			if (slot < 0)
				return slot;
			seenMask[slot] = 1;
			ret = m_users[slot]->UpdateAccountStatus(&snapshot);
			if (ret < 0)
				return ret;
		}
	}

	for (int index = 0; index < NP_PUSH_MAX_USERS; ++index) {
		if (m_users[index] != NULL && seenMask[index] == 0) {
			int ret = m_users[index]->MarkSignedOut();
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}

/* 0x813AF8C8 */
void Context::DestroyUserContextAt(int index)
{
	ScopedLock lock(&m_mutex);
	if (index >= 0 && index < NP_PUSH_MAX_USERS && m_users[index] != NULL) {
		m_users[index]->~UserContext();
		SceNpCommonPs4_6B428829(GetAllocator(), m_users[index]);
		m_users[index] = NULL;
	}
}

/* 0x813AFB28 */
int Context::UpdateAllUserContexts(int activeIntervalSec, UserContext::Handler *userHandler)
{
	(void)userHandler;
	ScopedLock lock(&m_mutex);
	SceRtcTick nowTick;
	sceClibMemset(&nowTick, 0, sizeof(nowTick));
	int ret = SceNpCommonPs4_5B904477(&nowTick);
	if (ret < 0)
		return ret;

	SceInt64 nowVal = static_cast<SceInt64>(nowTick.tick);
	if (m_session != NULL) {
		if (m_session->GetState() == SESSION_STATE_ESTABLISHED) {
			if (!m_initialStatusUpdated) {
				ret = UpdateInitialSessionStatus();
				if (ret < 0)
					return ret;
				m_initialStatusUpdated = true;
			}
			for (int index = 0; index < NP_PUSH_MAX_USERS; ++index) {
				if (m_users[index] != NULL) {
					bool removable = false;
					ret = m_users[index]->SyncStateWithSession(
						m_session, &nowVal, activeIntervalSec, &removable);
					if (ret < 0)
						return ret;
					if (removable)
						DestroyUserContextAt(index);
				}
			}
		}
	} else {
		for (int index = 0; index < NP_PUSH_MAX_USERS; ++index) {
			if (m_users[index] != NULL) {
				bool removable = false;
				m_users[index]->ExpireGracePeriod(&nowVal, &removable);
				if (removable)
					DestroyUserContextAt(index);
			}
		}
	}

	SceUInt32 minDelay = 0xFFFFFFFFU;
	for (int index = 0; index < NP_PUSH_MAX_USERS; ++index) {
		if (m_users[index] != NULL) {
			SceUInt32 delay = m_users[index]->GetNextTimerDelayUsec(&nowVal);
			if (delay != 0 && delay < minDelay)
				minDelay = delay;
		}
	}

	if (minDelay == 0xFFFFFFFFU) {
		SceNpCommonPs4_7A30E24E(&m_updateTimerJob);
	} else {
		ret = SceNpCommonPs4_ED3D9A81(&m_updateTimerJob, minDelay, this);
		if (ret < 0)
			return ret;
	}
	return 0;
}

/* 0x813AFCAC */
int Context::HandleRequestResultById(const PushRequestCompletion *completion)
{
	ScopedLock lock(&m_mutex);
	for (int index = 0; index < NP_PUSH_MAX_USERS; ++index) {
		if (m_users[index] != NULL) {
			int ret = m_users[index]->HandleRequestResult(completion);
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}

/* 0x813AFCF6 */
int Context::DispatchUserRequest(const RefPtr<UserRequest> &userRequest, bool *outQueued)
{
	ScopedLock lock(&m_mutex);
	SceUInt32 foundIdx = 0xFFFFFFFFU;
	UserContext *user = FindUserContextWithIndex(&userRequest->GetOnlineId(), &foundIdx);
	if (user != NULL) {
		bool attached = false;
		int ret = userRequest->AttachToUserContext(user, &attached);
		if (outQueued != NULL)
			*outQueued = attached;
		if (ret < 0)
			return ret;
	} else {
		userRequest->SetResult(SCE_NP_PUSH_ERROR_CONNECT_FAILED);
		userRequest->InvokeCallback();
		if (outQueued != NULL)
			*outQueued = false;
	}
	return 0;
}

/* 0x813AFD8A */
void Context::OnCalloutTimer(void)
{
	if (m_ownerCtl != NULL)
		m_ownerCtl->PostWakeupCommand();
}

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */
