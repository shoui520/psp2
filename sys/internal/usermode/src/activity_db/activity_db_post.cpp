/*
 * Posts a status of a title to the shell's NP activity service: the message's UTF-8
 * characters and linefeeds counted against the SDK's limits, then the shell's method, as
 * np_activity_sdk's client does (np_activity_utf8.cpp, np_activity_ipc_client.cpp).
 */
#include <np/np_activity.h>
#include "../../../../../vsh/lowlayer/src/shell_svc/shell_svc_client.h"
#include "../../../../../vsh/app/common/src/np/np_activity_sdk/np_activity_client.h"
#include "activity_db_internal.h"

static inline int ActivityDbCheckStatusMessage(const char *message)
{
	if (countUtf8Characters(message, SCE_NP_ACTIVITY_STATUS_MESSAGE_MAX_SIZE) >
	    SCE_NP_ACTIVITY_STATUS_MESSAGE_MAX_NUM_CHARACTERS)
		return NP_ACTIVITY_CLIENT_ERROR_INVALID_STATUS_MESSAGE;
	if (countLinefeeds(message, SCE_NP_ACTIVITY_STATUS_MESSAGE_MAX_SIZE) >
	    SCE_NP_ACTIVITY_STATUS_MESSAGE_MAX_NUM_LINEFEEDS)
		return NP_ACTIVITY_CLIENT_ERROR_INVALID_STATUS_MESSAGE;
	return 0;
}

int ActivityDbPostStatus(int type, const char *titleId, const char *contentId,
	const char *message, const char *actionUrl, const char *iconUrl)
{
	int result;

	if (message == NULL || titleId == NULL)
		return NP_ACTIVITY_CLIENT_ERROR_INVALID_ARGUMENT;
	if (message[0] == '\0')
		return NP_ACTIVITY_CLIENT_ERROR_INVALID_ARGUMENT;
	if (titleId[0] == '\0')
		return NP_ACTIVITY_CLIENT_ERROR_INVALID_ARGUMENT;
	result = ActivityDbCheckStatusMessage(message);
	if (result != 0)
		return result;
	if (contentId == NULL)
		contentId = "";
	if (actionUrl == NULL)
		actionUrl = "";
	if (iconUrl == NULL)
		iconUrl = "";
	return npActivityIpcPostStatus(SceShellSvc_B31E7F1C(), type, 0, titleId, contentId,
		message, iconUrl, actionUrl);
}
