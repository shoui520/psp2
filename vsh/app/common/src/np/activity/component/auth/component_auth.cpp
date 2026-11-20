/* The authentication component (Sony's file: its checks name component/auth/component_auth.cpp) */
#include <stddef.h>
#include <libkernel/libkernel.h>
#include <np/np_error.h>
#include "../../../np_manager/np_manager.h"
#include <np_common/np_common.h>
#include "../../common/activity_debug.h"
#include "../../common/activity_memory.h"
#include "../../common/activity_simple_lock.h"
#include "../../np_activity.h"
#include "../utility/component_utility.h"
#include "component_auth.h"

#define ACTIVITY_FILE	ACTIVITY_SRC_DIR "component\\auth\\component_auth.cpp"

/* the request's error, with SceNpManager's abort as the library's */
static inline int convertError(int ret)
{
	if (ret == SCE_NP_MANAGER_ERROR_ABORTED)
		ret = NP_ACTIVITY_ERROR_ABORTED;
	return ret;
}

ComponentAuth::ComponentAuth() : m_requestId(0)
{
}

ComponentAuth::~ComponentAuth()
{
}

inline void ComponentAuth::SetRequestId(int requestId)
{
	ActivitySimpleLock lock(GetMutex());

	m_requestId = requestId;
}

int ComponentAuth::GetFullTicketBase64Str(char **ticket, SceSize *size)
{
	void *data;
	SceSize dataSize;
	SceSize encodedSize;
	int requestId;
	int ret;

	*ticket = NULL;
#line 69
	ACTIVITY_ASSERT(m_utility != NULL, "GetFullTicketBase64Str");
	ret = CheckAbort(0);
	if (ret == NP_ACTIVITY_ERROR_ABORTED)
		return ret;

	requestId = SceNpManagerTicket_37F7F258();
	if (requestId < 0)
		return convertError(requestId);
	SetRequestId(requestId);

	dataSize = 0;
	ret = SceNpManager_E2FAC2CA(requestId, NULL, &dataSize);
	if (ret < 0) {
		ret = convertError(ret);
		SceNpManagerTicket_2B813EC7(requestId);
		SetRequestId(0);
		return ret;
	}
	data = activityMalloc(dataSize);
	if (data == NULL) {
		SceNpManagerTicket_2B813EC7(requestId);
		SetRequestId(0);
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	}
	ret = SceNpManager_E2FAC2CA(requestId, data, &dataSize);
	if (ret < 0) {
		SceNpManagerTicket_2B813EC7(requestId);
		SetRequestId(0);
		activityFree(data);
		return convertError(ret);
	}
	ret = SceNpManagerTicket_2B813EC7(requestId);
	SetRequestId(0);
	if (ret < 0) {
		ret = convertError(ret);
		activityFree(data);
		return ret;
	}

	encodedSize = m_utility->GetBase64EncodedSize(dataSize);
	*ticket = (char *)activityMalloc(encodedSize);
	if (*ticket == NULL) {
		activityFree(data);
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	}
	sceClibMemset(*ticket, 0, encodedSize);
	*size = m_utility->Base64Encode(data, dataSize, *ticket, encodedSize);
	activityFree(data);
	return ret;
}

int ComponentAuth::GetMiniTicketBase64Str(char **ticket, SceSize *size)
{
	void *data;
	void *mini;
	SceSize dataSize;
	SceSize miniSize;
	SceSize encodedSize;
	int requestId;
	int ret;

	*ticket = NULL;
#line 164
	ACTIVITY_ASSERT(m_utility != NULL, "GetMiniTicketBase64Str");
	ret = CheckAbort(0);
	if (ret == NP_ACTIVITY_ERROR_ABORTED)
		return ret;

	requestId = SceNpManagerTicket_37F7F258();
	if (requestId < 0)
		return convertError(requestId);
	SetRequestId(requestId);

	dataSize = 0;
	ret = SceNpManager_E2FAC2CA(requestId, NULL, &dataSize);
	if (ret < 0) {
		ret = convertError(ret);
		SceNpManagerTicket_2B813EC7(requestId);
		SetRequestId(0);
		return ret;
	}
	data = activityMalloc(dataSize);
	if (data == NULL) {
		SceNpManagerTicket_2B813EC7(requestId);
		SetRequestId(0);
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	}
	ret = SceNpManager_E2FAC2CA(requestId, data, &dataSize);
	if (ret < 0) {
		SceNpManagerTicket_2B813EC7(requestId);
		SetRequestId(0);
		activityFree(data);
		return convertError(ret);
	}
	ret = SceNpManagerTicket_2B813EC7(requestId);
	SetRequestId(0);
	if (ret < 0) {
		ret = convertError(ret);
		activityFree(data);
		return ret;
	}

	miniSize = 0;
	ret = SceNpCommon_85503FA6(data, dataSize, NULL, &miniSize);
	if (ret < 0) {
		activityFree(data);
		return convertError(ret);
	}
	mini = activityMalloc(miniSize);
	if (mini == NULL) {
		activityFree(data);
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	}
	ret = SceNpCommon_85503FA6(data, dataSize, mini, &miniSize);
	if (ret < 0) {
		activityFree(data);
		activityFree(mini);
		return convertError(ret);
	}
	activityFree(data);

	encodedSize = m_utility->GetBase64EncodedSize(miniSize);
	*ticket = (char *)activityMalloc(encodedSize);
	if (*ticket == NULL) {
		activityFree(mini);
		return NP_ACTIVITY_ERROR_OUT_OF_MEMORY;
	}
	sceClibMemset(*ticket, 0, encodedSize);
	*size = m_utility->Base64Encode(mini, miniSize, *ticket, encodedSize);
	activityFree(mini);
	return ret;
}

int ComponentAuth::GetMyNpId(SceNpId *npId)
{
	SceNpOnlineId onlineId;
	int requestId;
	int ret;

#line 291
	ACTIVITY_ASSERT(m_utility != NULL, "GetMyNpId");
	if (npId == NULL)
		return NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	ret = CheckAbort(0);
	if (ret == NP_ACTIVITY_ERROR_ABORTED)
		return ret;

	requestId = SceNpManagerTicket_37F7F258();
	if (requestId < 0)
		return convertError(requestId);
	SetRequestId(requestId);

	ret = SceNpManager_2841C20F(requestId, npId);
	if (ret < 0) {
		ret = convertError(ret);
		SceNpManagerTicket_2B813EC7(requestId);
		SetRequestId(0);
		return ret;
	}
	sceClibMemcpy_safe(onlineId.data, npId, SCE_NP_ONLINEID_MAX_LENGTH);
	onlineId.term = '\0';
	ret = SceNpManagerTicket_2B813EC7(requestId);
	SetRequestId(0);
	return convertError(ret);
}

int ComponentAuth::CheckChatRestriction()
{
	SceUInt8 accountData[AUTH_ACCOUNT_DATA_SIZE];
	SceUInt8 restriction[AUTH_RESTRICTION_SIZE];
	int requestId;
	int ret;

#line 347
	ACTIVITY_ASSERT(m_utility != NULL, "CheckChatRestriction");
	ret = CheckAbort(0);
	if (ret == NP_ACTIVITY_ERROR_ABORTED)
		return ret;

	requestId = SceNpManagerTicket_37F7F258();
	if (requestId < 0)
		return convertError(requestId);
	SetRequestId(requestId);

	sceClibMemset(accountData, 0, sizeof(accountData));
	sceClibMemset(restriction, 0, sizeof(restriction));
	ret = SceNpManager_2FE3625C(requestId, accountData, restriction);
	if (ret < 0) {
		ret = convertError(ret);
		SceNpManagerTicket_2B813EC7(requestId);
		SetRequestId(0);
		return ret;
	}
	ret = SceNpManagerTicket_2B813EC7(requestId);
	SetRequestId(0);
	if (ret < 0)
		return convertError(ret);
	if (restriction[AUTH_RESTRICTION_CHAT_BYTE] & AUTH_RESTRICTION_CHAT)
		return NP_ACTIVITY_ERROR_CHAT_RESTRICTED;
	return ret;
}

void ComponentAuth::SetAbort()
{
	ActivitySimpleLock lock(GetMutex());

	m_aborted = 1;
	if (m_requestId > 0)
		SceNpManagerTicket_A30BF29A(m_requestId);
}
