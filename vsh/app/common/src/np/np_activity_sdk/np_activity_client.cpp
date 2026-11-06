/*
 * NP Activity's status posts: the message's UTF-8 characters and linefeeds counted against the
 * SDK's limits, then the shell's method. The file's name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <np/np_activity.h>
#include "../../../../../lowlayer/src/shell_svc/shell_svc_client.h"
#include "np_activity_client.h"

/* the status kind of a post with a message and an action URI */
#define NP_ACTIVITY_STATUS_KIND_MESSAGE	1

static int checkStatusMessage(const char *message)
{
	if (countUtf8Characters(message, SCE_NP_ACTIVITY_STATUS_MESSAGE_MAX_SIZE) >
	    SCE_NP_ACTIVITY_STATUS_MESSAGE_MAX_NUM_CHARACTERS)
		return NP_ACTIVITY_CLIENT_ERROR_INVALID_STATUS_MESSAGE;
	if (countLinefeeds(message, SCE_NP_ACTIVITY_STATUS_MESSAGE_MAX_SIZE) >
	    SCE_NP_ACTIVITY_STATUS_MESSAGE_MAX_NUM_LINEFEEDS)
		return NP_ACTIVITY_CLIENT_ERROR_INVALID_STATUS_MESSAGE;
	return 0;
}

int npActivityPostStatus(int context, const char *message, const char *actionUri,
			 const char *extraText)
{
	int ret;

	if (message == NULL)
		return NP_ACTIVITY_CLIENT_ERROR_INVALID_ARGUMENT;
	if (message[0] == '\0')
		return NP_ACTIVITY_CLIENT_ERROR_INVALID_ARGUMENT;
	ret = checkStatusMessage(message);
	if (ret != 0)
		return ret;
	if (actionUri == NULL)
		actionUri = "";
	if (extraText == NULL)
		extraText = "";
	return npActivityIpcPostStatus(SceShellSvc_B31E7F1C(), context,
				       NP_ACTIVITY_STATUS_KIND_MESSAGE, "", "", message, extraText,
				       actionUri);
}

int npActivityPostAppStartupStatus(int context, const char *message, SceUInt32 appVersion,
				   const char *escapedParam)
{
	int ret;

	if (message == NULL)
		return NP_ACTIVITY_CLIENT_ERROR_INVALID_ARGUMENT;
	if (message[0] == '\0')
		return NP_ACTIVITY_CLIENT_ERROR_INVALID_ARGUMENT;
	ret = checkStatusMessage(message);
	if (ret != 0)
		return ret;
	if (escapedParam == NULL)
		return NP_ACTIVITY_CLIENT_ERROR_INVALID_ARGUMENT;
	return npActivityIpcPostAppStartupStatus(SceShellSvc_B31E7F1C(), context, message,
						 appVersion, escapedParam);
}
