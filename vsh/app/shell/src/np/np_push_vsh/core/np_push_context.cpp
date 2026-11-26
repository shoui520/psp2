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
	: m_session(NULL),
	  m_hasSessionOnlineId(false),
	  m_activeIntervalSec(0),
	  m_sleepIntervalSec(0),
	  m_initialized(false)
{
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

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */
