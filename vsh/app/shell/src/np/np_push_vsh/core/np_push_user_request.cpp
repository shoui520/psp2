/*
 * np_push_user_request.cpp: Asynchronous user presence request tracking and queues.
 * Sony path: vsh/app/common/src/np/np_push_vsh/core/np_push_user_request.cpp
 * 3.65-CEX: 0x813AEACC..0x813AEF8F
 */
#include "../np_push_internal.h"

namespace sce {
namespace Np {
namespace Push {

static void UserRequestRefDestroy(NpPushRefCounted *base)
{
	UserRequest *req = static_cast<UserRequest *>(base);
	req->~UserRequest();
	SceNpCommonPs4_6B428829(GetAllocator(), req);
}

static const void *const kUserRequestVtable[] = {
	NULL,
	NULL,
	reinterpret_cast<const void *>(UserRequestRefDestroy),
	reinterpret_cast<const void *>(UserRequestRefDestroy)
};

/* 0x813AEACC */
UserRequest *UserRequest::CreateSetPresenceOnline(const NpPushOnlineId *onlineId,
						  UserRequestCallback callback,
						  void *userContext)
{
	void *rawMem = SceNpCommonPs4_0C922423(sizeof(UserRequest), GetAllocator());
	if (rawMem == NULL)
		return NULL;
	return new (rawMem) UserRequest(USER_REQUEST_SET_PRESENCE_ONLINE, onlineId,
					callback, userContext);
}

/* 0x813AEB14 */
void UserRequest::Destroy(UserRequest *request)
{
	if (request != NULL)
		SceNpCommonPs4_93C97239(request);
}

/* 0x813AEB28 */
UserRequest::UserRequest(UserRequestType requestType, const NpPushOnlineId *onlineId,
			 UserRequestCallback callback, void *userContext)
	: m_type(requestType),
	  m_state(USER_REQUEST_STATE_IDLE),
	  m_result(0),
	  m_callback(callback),
	  m_userContext(userContext)
{
	SceNpCommonPs4_108738F2(this);
	vtable = &kUserRequestVtable[2];
	if (onlineId != NULL)
		SceNpCommonPs4_AEC0DAC5(&m_onlineId, onlineId);
	else
		SceNpCommonPs4_B0BAB9BE(&m_onlineId);
}

/* 0x813AEB8C */
UserRequest::~UserRequest()
{
	SceNpCommonPs4_1543D7EE(&m_onlineId);
	SceNpCommonPs4_BA23AD76(this);
}

/* 0x813AEBC4 */
void UserRequest::InvokeCallback(void)
{
	if (m_callback != NULL)
		m_callback(m_result, m_userContext);
}

/* 0x813AEBE0 */
UserRequestList::UserRequestList() : m_initialized(false)
{
	SceNpCommonPs4_4266D870(&m_mutex);
}

/* 0x813AEC58 */
UserRequestList::~UserRequestList()
{
	NP_PUSH_ASSERT(!m_initialized, "~UserRequestList",
		       "core\\np_push_user_request.cpp", 118, "!m_initialized");
	SceNpCommonPs4_294F0B1A(&m_mutex);
}

/* 0x813AECE0 */
int UserRequestList::Init(void)
{
	if (m_initialized)
		return SCE_NP_PUSH_ERROR_ALREADY_INITIALIZED;

	int ret = SceNpCommonPs4_8AA7E995(&m_mutex, "SceNpPushUserRequestList", 1);
	if (ret < 0)
		return ret;

	m_registered.SetAllocator(GetAllocator());
	m_running.SetAllocator(GetAllocator());
	m_finished.SetAllocator(GetAllocator());
	m_initialized = true;
	return 0;
}

/* 0x813AED44 */
void UserRequestList::Finalize(void)
{
	if (!m_initialized)
		return;

	{
		ScopedLock lock(&m_mutex);
		m_registered.Clear();
		m_running.Clear();
		m_finished.Clear();
	}
	SceNpCommonPs4_63EED478(&m_mutex);
	m_initialized = false;
}

/* 0x813AEDAC */
int UserRequestList::Register(const RefPtr<UserRequest> &request)
{
	if (!m_initialized)
		return SCE_NP_PUSH_ERROR_NOT_INITIALIZED;
	if (request.IsNull())
		return SCE_NP_PUSH_ERROR_INVALID_ARGUMENT;

	ScopedLock lock(&m_mutex);
	if (!m_registered.PushBack(request))
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;
	return 0;
}

/* 0x813AEE00 */
int UserRequestList::SetRunning(const RefPtr<UserRequest> &request)
{
	if (!m_initialized)
		return SCE_NP_PUSH_ERROR_NOT_INITIALIZED;

	NP_PUSH_ASSERT(!request.IsNull(), "SetRunning",
		       "core\\np_push_user_request.cpp", 183, "request != NULL");

	ScopedLock lock(&m_mutex);
	for (ListNode<RefPtr<UserRequest> > *iter = m_registered.Begin();
	     iter != m_registered.End(); iter = iter->next) {
		if (iter->value.Get() == request.Get()) {
			m_registered.Erase(iter);
			break;
		}
	}

	request->SetState(USER_REQUEST_STATE_RUNNING);
	if (!m_running.PushBack(request))
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;
	return 0;
}

/* 0x813AEE74 */
int UserRequestList::SetFinishedImpl(const RefPtr<UserRequest> &request, int result)
{
	NP_PUSH_ASSERT(!request.IsNull(), "SetFinishedImpl",
		       "core\\np_push_user_request.cpp", 215, "request != NULL");

	for (ListNode<RefPtr<UserRequest> > *iter = m_registered.Begin();
	     iter != m_registered.End(); iter = iter->next) {
		if (iter->value.Get() == request.Get()) {
			m_registered.Erase(iter);
			break;
		}
	}

	for (ListNode<RefPtr<UserRequest> > *iter = m_running.Begin();
	     iter != m_running.End(); iter = iter->next) {
		if (iter->value.Get() == request.Get()) {
			m_running.Erase(iter);
			break;
		}
	}

	request->SetResult(result);
	request->SetState(USER_REQUEST_STATE_FINISHED);
	if (!m_finished.PushBack(request))
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;
	return 0;
}

/* 0x813AEEF0 */
int UserRequestList::SetFinished(const RefPtr<UserRequest> &request, int result)
{
	if (!m_initialized)
		return SCE_NP_PUSH_ERROR_NOT_INITIALIZED;

	ScopedLock lock(&m_mutex);
	return SetFinishedImpl(request, result);
}

/* 0x813AEF1C */
int UserRequestList::SetFinishedAll(int result)
{
	if (!m_initialized)
		return SCE_NP_PUSH_ERROR_NOT_INITIALIZED;

	ScopedLock lock(&m_mutex);
	while (!m_registered.Empty()) {
		RefPtr<UserRequest> req = m_registered.Begin()->value;
		SetFinishedImpl(req, result);
	}
	while (!m_running.Empty()) {
		RefPtr<UserRequest> req = m_running.Begin()->value;
		SetFinishedImpl(req, result);
	}
	return 0;
}

bool UserRequestList::PopRegistered(RefPtr<UserRequest> *outRequest)
{
	if (!m_initialized || outRequest == NULL)
		return false;

	ScopedLock lock(&m_mutex);
	if (m_registered.Empty())
		return false;
	*outRequest = m_registered.Begin()->value;
	m_registered.Erase(m_registered.Begin());
	return true;
}

bool UserRequestList::PopFinished(RefPtr<UserRequest> *outRequest)
{
	if (!m_initialized || outRequest == NULL)
		return false;

	ScopedLock lock(&m_mutex);
	if (m_finished.Empty())
		return false;
	*outRequest = m_finished.Begin()->value;
	m_finished.Erase(m_finished.Begin());
	return true;
}

bool UserRequestList::HasPending(void) const
{
	return !m_registered.Empty() || !m_running.Empty();
}

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */
