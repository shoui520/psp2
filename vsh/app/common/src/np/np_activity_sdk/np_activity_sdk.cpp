/*
 * SceNpActivity (vs0:sys/external/np_activity_sdk.suprx): the SDK's NP Activity library
 * (np/np_activity.h). It checks the application's status and passes it to the shell. No Sony
 * record names its files or internal functions; the file's name is the module's.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/libkernel.h>
#include <libkernel/libkernel.h>
#include <np/np_activity.h>
#include <np/np_error.h>
#include "np_activity_client.h"

static int s_isInitialized;

/* the status reaches the client's errors as the library's */
static int convertClientError(int ret)
{
	if (ret < 0) {
		if (ret == NP_ACTIVITY_CLIENT_ERROR_INVALID_STATUS_MESSAGE)
			ret = SCE_NP_ACTIVITY_ERROR_INVALID_STATUS_MESSAGE;
		if ((ret & 0xffffff00) == NP_ACTIVITY_CLIENT_ERROR_BASE)
			ret = SCE_NP_ACTIVITY_ERROR_INTERNAL;
	}
	return ret;
}

/* the action URI's scheme, after leading blanks: http, https, psns or psts */
static int checkActionUriScheme(const char *uri)
{
	const char *p = uri;
	char scheme[10];
	int found = 0;
	int length;
	char c;

	if (uri == NULL)
		return SCE_NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	do {
		if (p - uri >= SCE_NP_ACTIVITY_STATUS_ACTION_URI_MAX_SIZE)
			return SCE_NP_ACTIVITY_ERROR_INVALID_URI_SCHEME;
		if (*p == ' ' || *p == '\t')
			p++;
		else if (*p == '\0')
			return SCE_NP_ACTIVITY_ERROR_INVALID_URI_SCHEME;
		else
			found = 1;
	} while (!found);

	sceClibMemset(scheme, 0, sizeof(scheme));
	for (length = 0;;) {
		if (p - uri >= SCE_NP_ACTIVITY_STATUS_ACTION_URI_MAX_SIZE)
			return SCE_NP_ACTIVITY_ERROR_INVALID_URI_SCHEME;
		c = *p;
		if (c == ':')
			break;
		if (c == '\0')
			return SCE_NP_ACTIVITY_ERROR_INVALID_URI_SCHEME;
		scheme[length++] = c;
		p++;
		if (length >= (int)sizeof(scheme) - 1)
			return SCE_NP_ACTIVITY_ERROR_INVALID_URI_SCHEME;
	}
	scheme[sizeof(scheme) - 1] = '\0';

	if (sceClibStrncasecmp(scheme, "http", sizeof(scheme)) == 0 ||
	    sceClibStrncasecmp(scheme, "https", sizeof(scheme)) == 0 ||
	    sceClibStrncasecmp(scheme, "psns", sizeof(scheme)) == 0 ||
	    sceClibStrncasecmp(scheme, "psts", sizeof(scheme)) == 0)
		return 0;
	return SCE_NP_ACTIVITY_ERROR_INVALID_URI_SCHEME;
}

/* what an escaped parameter may hold: letters, digits, the URI's unreserved marks and '%', '*' */
static int isAppParamCharacter(int c)
{
	if (c >= '0' && c <= '9')
		return 1;
	if (c >= 'a' && c <= 'z')
		return 1;
	if (c >= 'A' && c <= 'Z')
		return 1;
	switch (c) {
	case '%':
	case '*':
	case '-':
	case '.':
	case '_':
	case '~':
		return 1;
	}
	return 0;
}

static int checkAppParam(const char *param)
{
	const char *p;

	if (param == NULL)
		return SCE_NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	for (p = param; *p != '\0'; p++) {
		if (p - param >= SCE_NP_ACTIVITY_APP_PARAM_MAX_SIZE)
			return SCE_NP_ACTIVITY_ERROR_INVALID_APP_PARAM;
		if (!isAppParamCharacter(*p))
			return SCE_NP_ACTIVITY_ERROR_INVALID_APP_PARAM;
	}
	return 0;
}

extern "C" int sceNpActivityInit(void *option)
{
	if (s_isInitialized)
		return SCE_NP_ACTIVITY_ERROR_ALREADY_INITIALIZED;
	if (option != NULL)
		return SCE_NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	s_isInitialized = 1;
	return 0;
}

extern "C" int sceNpActivityTerm(void)
{
	if (!s_isInitialized)
		return SCE_NP_ACTIVITY_ERROR_NOT_INITIALIZED;
	s_isInitialized = 0;
	return 0;
}

extern "C" int sceNpActivityPostStatus(const SceNpActivityStatusMessage *message,
				       const SceNpActivityStatusActionUri *actionUri, void *option)
{
	const char *uri;
	int ret;

	if (!s_isInitialized)
		return SCE_NP_ACTIVITY_ERROR_NOT_INITIALIZED;
	if (option != NULL)
		return SCE_NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (message == NULL)
		return SCE_NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (message->data[0] == '\0')
		return SCE_NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (sceClibStrnlen(message->data, SCE_NP_ACTIVITY_STATUS_MESSAGE_MAX_SIZE) >=
	    SCE_NP_ACTIVITY_STATUS_MESSAGE_MAX_SIZE)
		return SCE_NP_ACTIVITY_ERROR_INVALID_ARGUMENT;

	uri = NULL;
	ret = 0;
	if (actionUri != NULL) {
		if (sceClibStrnlen(actionUri->data, SCE_NP_ACTIVITY_STATUS_ACTION_URI_MAX_SIZE) >=
		    SCE_NP_ACTIVITY_STATUS_ACTION_URI_MAX_SIZE)
			return SCE_NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
		if (actionUri->data[0] != '\0') {
			uri = actionUri->data;
			ret = checkActionUriScheme(uri);
		}
	}
	if (ret < 0)
		return ret;
	return convertClientError(npActivityPostStatus(0, message->data, uri, NULL));
}

extern "C" int sceNpActivityPostAppStartupStatus(const SceNpActivityStatusMessage *message,
						 const SceNpActivityStatusAppStartupParam *param,
						 void *option)
{
	int ret;

	if (!s_isInitialized)
		return SCE_NP_ACTIVITY_ERROR_NOT_INITIALIZED;
	if (option != NULL)
		return SCE_NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (message == NULL)
		return SCE_NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (message->data[0] == '\0')
		return SCE_NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (sceClibStrnlen(message->data, SCE_NP_ACTIVITY_STATUS_MESSAGE_MAX_SIZE) >=
	    SCE_NP_ACTIVITY_STATUS_MESSAGE_MAX_SIZE)
		return SCE_NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (param == NULL)
		return SCE_NP_ACTIVITY_ERROR_INVALID_ARGUMENT;
	if (param->appVersion != SCE_NP_ACTIVITY_APP_VER_NOT_REQUIRED &&
	    (param->appVersion < 100 || param->appVersion > 9999))
		return SCE_NP_ACTIVITY_ERROR_INVALID_APP_VER;
	if (sceClibStrnlen(param->escapedParam, SCE_NP_ACTIVITY_APP_PARAM_MAX_SIZE) >=
	    SCE_NP_ACTIVITY_APP_PARAM_MAX_SIZE)
		return SCE_NP_ACTIVITY_ERROR_INVALID_APP_PARAM;
	ret = checkAppParam(param->escapedParam);
	if (ret < 0)
		return ret;
	return convertClientError(npActivityPostAppStartupStatus(0, message->data,
								 param->appVersion,
								 param->escapedParam));
}
