/*
 * The app events: sceAppUtilReceiveAppEvent reads the text SceAppMgr last sent the application
 * ("psla:...", "type=INVITATION_MESSAGE&npcommid=...&uid=...", ...), and a parser per event type
 * takes its values apart into the event's parameters.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <apputil.h>
#include <np/np_common.h>
#include <appmgr/appmgr.h>
#include "apputil_private.h"

/* the values of an event's text, name=value pairs joined by & */
enum {
	APPEVENT_KEY_TYPE = 1,
	APPEVENT_KEY_NPCOMMID,
	APPEVENT_KEY_UID,
	APPEVENT_KEY_JID,
	APPEVENT_KEY_GIFTID,
	APPEVENT_KEY_VERSION,
	APPEVENT_KEY_PARAM,
	APPEVENT_KEY_EVENT_TYPE,
	APPEVENT_KEY_TRIGGER_TYPE,
	APPEVENT_KEY_PATH,
	APPEVENT_KEY_FACTOR,
	APPEVENT_KEY_SESSIONID,
	APPEVENT_KEY_INVITATIONID,
	APPEVENT_KEY_ITEMID,
};

/* a jid ("user@domain/resource") taken apart, each part a string in one buffer */
typedef struct JidText {
	const char *user;
	char *domain;
	char *resource;
	SceUInt32 reserved;
} JidText;

#define APPEVENT_ISALNUM(c) \
	(sceClibLookCtypeTable(c) & (SCE_CLIB_CTYPE_UPPER | SCE_CLIB_CTYPE_LOWER | SCE_CLIB_CTYPE_DIGIT))

static int urlDecode(char *dst, const char *src);
static int urlDecodeChecked(char *dst, const char *src);
static int appEventGetValue(const char *str, char *value, int key);
static int parseNpCommunicationId(const char *str, SceNpCommunicationId *commId);
static int parseJid(const char *str, SceNpId *npId);

int receiveAppEvent(SceAppUtilAppEventParam *eventParam)
{
	char text[SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE];
	char value[SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE];
	int ret;

	if (eventParam == NULL) {
		paramError(130);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibMemset(text, 0, sizeof(text));
	sceClibMemset(value, 0, sizeof(value));
	sceClibMemset(eventParam, 0, sizeof(*eventParam));
	ret = sceAppMgrGetAppParam((char *)eventParam->data);
	if (ret != SCE_OK)
		return ret;
	ret = urlDecode(text, (const char *)eventParam->data);
	if (ret != SCE_OK)
		return ret;

	if (sceClibStrncmp(text, "psla:", 5) == 0) {
		eventParam->type = SCE_APPUTIL_APPEVENT_TYPE_LIVEAREA;
		return SCE_OK;
	}
	if (sceClibStrncmp(text, "trigger_util:", 13) == 0) {
		eventParam->type = SCE_APPUTIL_APPEVENT_TYPE_TRIGGER_UTIL;
		return SCE_OK;
	}
	if (sceClibStrncmp(text, "incoming_dialog:", 16) == 0) {
		eventParam->type = SCE_APPUTIL_APPEVENT_TYPE_INCOMING_DIALOG;
		return SCE_OK;
	}
	if (sceClibStrncmp(text, "webbrowser:", 11) == 0) {
		eventParam->type = SCE_APPUTIL_APPEVENT_TYPE_WEBBROWSER;
		return SCE_OK;
	}
	if (sceClibStrncmp(text, "screenshot:", 11) == 0) {
		ret = appEventGetValue(text, value, APPEVENT_KEY_EVENT_TYPE);
		if (ret != SCE_OK)
			return ret;
		if (sceClibStrncmp(value, "SCREENSHOT_NOTIFICATION", 24) == 0) {
			eventParam->type = SCE_APPUTIL_APPEVENT_TYPE_SCREENSHOT_NOTIFICATION;
			return SCE_OK;
		}
	}

	ret = appEventGetValue(text, value, APPEVENT_KEY_TYPE);
	if (ret != SCE_OK)
		return ret;
	if (sceClibStrncmp(value, "INVITATION_MESSAGE", 19) == 0)
		eventParam->type = SCE_APPUTIL_APPEVENT_TYPE_NP_INVITE_MESSAGE;
	else if (sceClibStrncmp(value, "APP_DATA_MESSAGE", 17) == 0)
		eventParam->type = SCE_APPUTIL_APPEVENT_TYPE_NP_APP_DATA_MESSAGE;
	else if (sceClibStrncmp(value, "BASIC_JOINABLE_PRESENCE", 24) == 0)
		eventParam->type = SCE_APPUTIL_APPEVENT_TYPE_NP_BASIC_JOINABLE_PRESENCE;
	else if (sceClibStrncmp(value, "NEAR_GIFT", 10) == 0)
		eventParam->type = SCE_APPUTIL_APPEVENT_TYPE_NEAR_GIFT;
	else if (sceClibStrncmp(value, "USER_DEFINED", 13) == 0)
		eventParam->type = g_sdkVersion < 0x1800000 ? SCE_APPUTIL_APPEVENT_TYPE_TRIGGER_UTIL :
							      SCE_APPUTIL_APPEVENT_TYPE_USER_DEFINED;
	else if (sceClibStrncmp(value, "LAUNCH_APP_BY_URI", 18) == 0)
		eventParam->type = SCE_APPUTIL_APPEVENT_TYPE_LAUNCH_APP;
	else if (sceClibStrncmp(value, "ACTIVITY", 9) == 0)
		eventParam->type = SCE_APPUTIL_APPEVENT_TYPE_NP_ACTIVITY;
	else if (sceClibStrncmp(value, "SESSION_INVITATION", 19) == 0)
		eventParam->type = SCE_APPUTIL_APPEVENT_TYPE_SESSION_INVITATION;
	else if (sceClibStrncmp(value, "GAME_CUSTOM_DATA", 17) == 0)
		eventParam->type = SCE_APPUTIL_APPEVENT_TYPE_GAME_CUSTOM_DATA;
	else
		eventParam->type = SCE_APPUTIL_APPEVENT_TYPE_INVALID;

	sceClibMemset(value, 0, sizeof(value));
	if (eventParam->type == SCE_APPUTIL_APPEVENT_TYPE_LAUNCH_APP) {
		if (appEventGetValue(text, value, APPEVENT_KEY_FACTOR) != SCE_OK)
			return SCE_OK;
		if (sceClibStrncmp(value, "TELEPORT", 9) == 0) {
			eventParam->type = SCE_APPUTIL_APPEVENT_TYPE_TELEPORT;
			return SCE_OK;
		}
	}
	if (eventParam->type < SCE_APPUTIL_APPEVENT_TYPE_WEBBROWSER ||
	    eventParam->type == SCE_APPUTIL_APPEVENT_TYPE_WEBBROWSER)
		return SCE_OK;
	return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;
}

int appEventParseNpInviteMessage(const SceAppUtilAppEventParam *eventParam,
				 SceAppUtilNpInviteMessageParam *param)
{
	char text[SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE];
	char value[SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE];
	int ret;

	if (eventParam == NULL) {
		paramError(130);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param == NULL) {
		paramError(132);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (eventParam->type != SCE_APPUTIL_APPEVENT_TYPE_NP_INVITE_MESSAGE) {
		paramError(131);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibMemset(text, 0, sizeof(text));
	sceClibMemset(value, 0, sizeof(value));
	sceClibMemset(param, 0, sizeof(*param));
	ret = urlDecode(text, (const char *)eventParam->data);
	if (ret != SCE_OK)
		return ret;
	ret = appEventGetValue(text, value, APPEVENT_KEY_NPCOMMID);
	if (ret != SCE_OK)
		return ret;
	ret = parseNpCommunicationId(value, &param->commId);
	if (ret != SCE_OK)
		return ret;
	sceClibMemset(value, 0, sizeof(value));
	ret = appEventGetValue(text, value, APPEVENT_KEY_UID);
	if (ret != SCE_OK)
		return ret;
	sceClibMemcpy_safe(param->uid, value, sizeof(param->uid) - 1);
	return SCE_OK;
}

int appEventParseNpAppDataMessage(const SceAppUtilAppEventParam *eventParam,
				  SceAppUtilNpAppDataMessageParam *param)
{
	char text[SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE];
	char value[SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE];
	int ret;

	if (eventParam == NULL) {
		paramError(130);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param == NULL) {
		paramError(133);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (eventParam->type != SCE_APPUTIL_APPEVENT_TYPE_NP_APP_DATA_MESSAGE) {
		paramError(131);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibMemset(text, 0, sizeof(text));
	sceClibMemset(value, 0, sizeof(value));
	sceClibMemset(param, 0, sizeof(*param));
	ret = urlDecode(text, (const char *)eventParam->data);
	if (ret != SCE_OK)
		return ret;
	ret = appEventGetValue(text, value, APPEVENT_KEY_NPCOMMID);
	if (ret != SCE_OK)
		return ret;
	ret = parseNpCommunicationId(value, &param->commId);
	if (ret != SCE_OK)
		return ret;
	sceClibMemset(value, 0, sizeof(value));
	ret = appEventGetValue(text, value, APPEVENT_KEY_UID);
	if (ret != SCE_OK)
		return ret;
	sceClibMemcpy_safe(param->uid, value, sizeof(param->uid) - 1);
	return SCE_OK;
}

int appEventParseNpBasicJoinablePresence(const SceAppUtilAppEventParam *eventParam,
					 SceAppUtilNpBasicJoinablePresenceParam *param)
{
	char text[SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE];
	char value[SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE];
	int ret;

	if (eventParam == NULL) {
		paramError(130);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param == NULL) {
		paramError(134);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (eventParam->type != SCE_APPUTIL_APPEVENT_TYPE_NP_BASIC_JOINABLE_PRESENCE) {
		paramError(131);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibMemset(text, 0, sizeof(text));
	sceClibMemset(value, 0, sizeof(value));
	sceClibMemset(param, 0, sizeof(*param));
	ret = urlDecode(text, (const char *)eventParam->data);
	if (ret != SCE_OK)
		return ret;
	ret = appEventGetValue(text, value, APPEVENT_KEY_NPCOMMID);
	if (ret != SCE_OK)
		return ret;
	ret = parseNpCommunicationId(value, &param->commId);
	if (ret != SCE_OK)
		return ret;
	sceClibMemset(value, 0, sizeof(value));
	ret = appEventGetValue(text, value, APPEVENT_KEY_JID);
	if (ret != SCE_OK)
		return ret;
	ret = parseJid(value, &param->npId);
	if (ret != SCE_OK)
		return ret;
	return SCE_OK;
}

int appEventParseNearGift(const SceAppUtilAppEventParam *eventParam, SceAppUtilNearGiftParam *param)
{
	char text[SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE];
	char value[SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE];
	int ret;

	if (eventParam == NULL) {
		paramError(130);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param == NULL) {
		paramError(135);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (eventParam->type != SCE_APPUTIL_APPEVENT_TYPE_NEAR_GIFT) {
		paramError(131);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibMemset(text, 0, sizeof(text));
	sceClibMemset(value, 0, sizeof(value));
	sceClibMemset(param, 0, sizeof(*param));
	ret = urlDecode(text, (const char *)eventParam->data);
	if (ret != SCE_OK)
		return ret;
	ret = appEventGetValue(text, value, APPEVENT_KEY_NPCOMMID);
	if (ret != SCE_OK)
		return ret;
	ret = parseNpCommunicationId(value, &param->commId);
	if (ret != SCE_OK)
		return ret;
	sceClibMemset(value, 0, sizeof(value));
	ret = appEventGetValue(text, value, APPEVENT_KEY_JID);
	if (ret != SCE_OK)
		return ret;
	ret = parseJid(value, &param->npId);
	if (ret != SCE_OK)
		return ret;
	sceClibMemset(value, 0, sizeof(value));
	ret = appEventGetValue(text, value, APPEVENT_KEY_GIFTID);
	if (ret != SCE_OK)
		return ret;
	param->giftId = sceClibStrtoll(value, NULL, 0);
	sceClibMemset(value, 0, sizeof(value));
	ret = appEventGetValue(text, value, APPEVENT_KEY_VERSION);
	if (ret != SCE_OK)
		return ret;
	param->version = sceClibStrtoll(value, NULL, 0);
	sceClibMemset(value, 0, sizeof(value));
	ret = appEventGetValue(text, value, APPEVENT_KEY_PARAM);
	if (ret != SCE_OK)
		return ret;
	sceClibMemcpy_safe(param->param, value, sizeof(param->param));
	return SCE_OK;
}

int appEventParseLiveArea(const SceAppUtilAppEventParam *eventParam, SceAppUtilLiveAreaParam *param)
{
	char text[SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE];
	int ret;

	if (eventParam == NULL) {
		paramError(130);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param == NULL) {
		paramError(136);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (eventParam->type != SCE_APPUTIL_APPEVENT_TYPE_LIVEAREA) {
		paramError(131);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibMemset(text, 0, sizeof(text));
	sceClibMemset(param, 0, sizeof(*param));
	ret = urlDecodeChecked(text, (const char *)eventParam->data);
	if (ret != SCE_OK)
		return ret;
	if (sceClibStrncmp(text, "psla:", 5) != 0)
		return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;
	sceClibStrncpy((char *)param->param, &text[5], sizeof(param->param) - 1);
	return SCE_OK;
}

int appEventParseNpActivity(const SceAppUtilAppEventParam *eventParam, SceAppUtilNpActivityParam *param)
{
	char value[SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE];
	char text[SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE];
	const char *data;
	int ret;

	if (eventParam == NULL) {
		paramError(130);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param == NULL) {
		paramError(400);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	data = (const char *)eventParam->data;
	if (eventParam->type != SCE_APPUTIL_APPEVENT_TYPE_NP_ACTIVITY) {
		paramError(131);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (sceClibStrnlen(data, SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE) >= SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE) {
		paramError(131);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibMemset(value, 0, sizeof(value));
	sceClibMemset(text, 0, sizeof(text));
	sceClibMemset(param, 0, sizeof(*param));
	appEventGetValue(data, value, APPEVENT_KEY_PARAM);
	if ((SceUInt8)value[0] == '\0')
		return SCE_OK;
	ret = urlDecodeChecked(text, value);
	if (ret != SCE_OK)
		return ret;
	sceClibStrncpy((char *)param->param, text, sizeof(param->param) - 1);
	return SCE_OK;
}

#define TELEPORT_PREFIX "type=LAUNCH_APP_BY_URI&factor=TELEPORT&uri="

int appEventParseTeleport(const SceAppUtilAppEventParam *eventParam, SceAppUtilTeleportParam *param)
{
	char text[SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE];
	const char *data;
	SceSize len, n;
	char c;

	if (eventParam == NULL) {
		paramError(130);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param == NULL) {
		paramError(401);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	data = (const char *)eventParam->data;
	if (eventParam->type != SCE_APPUTIL_APPEVENT_TYPE_TELEPORT) {
		paramError(131);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (sceClibStrnlen(data, SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE) >= SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE) {
		paramError(131);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibMemset(text, 0, sizeof(text));
	sceClibMemset(param, 0, sizeof(*param));
	len = sceClibStrnlen(TELEPORT_PREFIX, SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE);
	if (sceClibStrncmp(data, TELEPORT_PREFIX, len) != 0)
		return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;
	/* the uri's scheme is left out */
	for (n = 1, c = data[len]; c != ':' && c != '\0'; n++)
		c = data[len + n];
	sceClibStrncpy((char *)param->param, &data[len + n], sizeof(param->param) - 1);
	return SCE_OK;
}

int appEventParseUserDefined(const SceAppUtilAppEventParam *eventParam, SceAppUtilUserDefinedParam *param)
{
	if (eventParam == NULL) {
		paramError(130);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param == NULL) {
		paramError(137);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if ((g_sdkVersion < 0x1800000 && eventParam->type != SCE_APPUTIL_APPEVENT_TYPE_TRIGGER_UTIL) ||
	    (g_sdkVersion >= 0x1800000 && eventParam->type != SCE_APPUTIL_APPEVENT_TYPE_USER_DEFINED)) {
		paramError(131);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibMemset(param, 0, sizeof(*param));
	if (sceClibStrncmp((const char *)eventParam->data, "type=USER_DEFINED&arg=", 22) != 0)
		return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;
	sceClibStrncpy((char *)param->arg, (const char *)&eventParam->data[22], sizeof(param->arg) - 1);
	return SCE_OK;
}

extern "C" SceInt32 sceAppUtilAppEventParseIncomingDialog(const SceAppUtilAppEventParam *eventParam,
							 SceAppUtilIncomingDialogParam *param)
{
	const char *text;
	const char *next;

	if (eventParam == NULL) {
		paramError(130);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param == NULL) {
		paramError(180);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (eventParam->type != SCE_APPUTIL_APPEVENT_TYPE_INCOMING_DIALOG) {
		paramError(131);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	param->isFromStartScreen = 0;
	/* after "incoming_dialog:" */
	text = (const char *)&eventParam->data[16];
	if (sceClibStrncmp(text, "from_start_screen=", 18) == 0) {
		if (sceClibStrncmp(&text[18], "true", 4) == 0)
			param->isFromStartScreen = 1;
		next = sceClibStrstr(&text[18], "&");
		if (next == NULL)
			return SCE_OK;
		text = next + 1;
	}
	if (g_sdkVersion >= 0x3000000) {
		sceClibMemset(param->param, 0, sizeof(param->param));
		if (sceClibStrncmp(text, "param=", 6) == 0)
			sceClibStrncpy((char *)param->param, &text[6], sizeof(param->param) - 1);
	}
	return SCE_OK;
}

extern "C" SceInt32 sceAppUtilAppEventParseTriggerUtil(const SceAppUtilAppEventParam *eventParam,
						      SceAppUtilTriggerUtilParam *param)
{
	const char *text;
	const char *slot;

	if (eventParam == NULL) {
		paramError(130);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param == NULL) {
		paramError(181);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (eventParam->type != SCE_APPUTIL_APPEVENT_TYPE_TRIGGER_UTIL) {
		paramError(131);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibMemset(param, 0, sizeof(*param));
	text = (const char *)eventParam->data;
	if (sceClibStrstr(text, "type=PeriodicTimer") != NULL)
		param->type = SCE_APPUTIL_TRIGGER_UTIL_TYPE_PERIODIC_TIMER;
	else if (sceClibStrstr(text, "type=TimeOfDayTimer") != NULL)
		param->type = SCE_APPUTIL_TRIGGER_UTIL_TYPE_TIME_OF_DAY_TIMER;
	else if (sceClibStrstr(text, "type=DailyTimer") != NULL)
		param->type = SCE_APPUTIL_TRIGGER_UTIL_TYPE_DAILY_TIMER;
	else
		return SCE_APPUTIL_ERROR_PARAMETER;
	slot = sceClibStrstr(text, "slot=");
	if (slot == NULL)
		return SCE_APPUTIL_ERROR_PARAMETER;
	param->slot = slot[5] - '0';
	return SCE_OK;
}

int appEventParseWebBrowser(const SceAppUtilAppEventParam *eventParam, SceAppUtilWebBrowserEventParam *param)
{
	char text[SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE];
	char value[SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE];
	int ret;

	sceClibMemset(text, 0, sizeof(text));
	sceClibMemset(value, 0, sizeof(value));
	if (eventParam == NULL) {
		paramError(130);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param == NULL) {
		paramError(138);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (eventParam->type != SCE_APPUTIL_APPEVENT_TYPE_WEBBROWSER) {
		paramError(131);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibMemset(param, 0, sizeof(*param));
	sceClibStrncpy(text, (const char *)eventParam->data, sizeof(text));
	ret = appEventGetValue(text, value, APPEVENT_KEY_TYPE);
	if (ret != SCE_OK)
		return ret;
	if (sceClibStrncmp(value, "CALLBACK", 9) == 0)
		param->type = SCE_APPUTIL_WEBBROWSER_EVENT_TYPE_CALLBACK;
	sceClibMemset(value, 0, sizeof(value));
	if (param->type != SCE_APPUTIL_WEBBROWSER_EVENT_TYPE_CALLBACK)
		return SCE_APPUTIL_ERROR_PARAMETER;
	if (sceClibStrncmp((const char *)eventParam->data, "webbrowser:type=CALLBACK&param=", 31) != 0)
		return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;
	sceClibStrncpy((char *)param->param, (const char *)&eventParam->data[31], sizeof(param->param) - 1);
	return SCE_OK;
}

int appEventParseScreenShotNotification(const SceAppUtilAppEventParam *eventParam,
					SceAppUtilScreenShotNotification *param)
{
	char triggerType[16];
	const char *data;
	int ret;

	if (eventParam == NULL) {
		paramError(130);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param == NULL) {
		paramError(270);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (eventParam->type != SCE_APPUTIL_APPEVENT_TYPE_SCREENSHOT_NOTIFICATION) {
		paramError(131);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibMemset(param, 0, sizeof(*param));
	sceClibMemset(triggerType, 0, sizeof(triggerType));
	data = (const char *)eventParam->data;
	ret = appEventGetValue(data, triggerType, APPEVENT_KEY_TRIGGER_TYPE);
	if (ret != SCE_OK)
		return ret;
	if (triggerType[0] == '0')
		param->triggerType = 0;
	else if (triggerType[0] == '1')
		param->triggerType = 1;
	else
		return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;
	ret = appEventGetValue(data, (char *)param->contentPath, APPEVENT_KEY_PATH);
	if (ret != SCE_OK)
		return ret;
	return SCE_OK;
}

int appEventParseLaunchApp(const SceAppUtilAppEventParam *eventParam, SceAppUtilLaunchAppParam *param)
{
	const char *uri;
	int isTeleport;

	if (eventParam == NULL) {
		paramError(130);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param == NULL) {
		paramError(139);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (eventParam->type != SCE_APPUTIL_APPEVENT_TYPE_LAUNCH_APP) {
		paramError(131);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	/* the size of the parameter before SDK 2.50 */
	sceClibMemset(param, 0, sizeof(param->uri));
	if (sceClibStrncmp((const char *)eventParam->data, "type=LAUNCH_APP_BY_URI&uri=", 27) == 0) {
		uri = (const char *)&eventParam->data[27];
		isTeleport = 0;
	} else if (sceClibStrncmp((const char *)eventParam->data,
				  "type=LAUNCH_APP_BY_URI&factor=TELEPORT&uri=", 43) == 0) {
		uri = (const char *)&eventParam->data[43];
		isTeleport = 1;
	} else {
		return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;
	}
	sceClibStrncpy((char *)param->uri, uri, sizeof(param->uri) - 1);
	if (g_sdkVersion >= 0x2500000)
		param->isTeleport = isTeleport;
	return SCE_OK;
}

int appEventParseSessionInvitation(const SceAppUtilAppEventParam *eventParam,
				   SceAppUtilSessionInvitationParam *param)
{
	char text[SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE];
	char value[SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE];
	int ret;

	if (eventParam == NULL) {
		paramError(130);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param == NULL) {
		paramError(271);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (eventParam->type != SCE_APPUTIL_APPEVENT_TYPE_SESSION_INVITATION) {
		paramError(131);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibMemset(text, 0, sizeof(text));
	sceClibMemset(value, 0, sizeof(value));
	sceClibMemset(param, 0, sizeof(*param));
	ret = urlDecode(text, (const char *)eventParam->data);
	if (ret != SCE_OK)
		return ret;
	sceClibMemset(value, 0, sizeof(value));
	ret = appEventGetValue(text, value, APPEVENT_KEY_SESSIONID);
	if (ret != SCE_OK)
		return ret;
	sceClibMemcpy_safe(&param->sessionId, value, SCE_NP_SESSION_ID_MAX_SIZE);
	sceClibMemset(value, 0, sizeof(value));
	if (appEventGetValue(text, value, APPEVENT_KEY_INVITATIONID) == SCE_OK &&
	    sceClibStrnlen(value, sizeof(value)) != 0)
		sceClibMemcpy_safe(&param->invitationId, value, SCE_NP_INVITATION_ID_MAX_SIZE);
	return SCE_OK;
}

int appEventParseGameCustomData(const SceAppUtilAppEventParam *eventParam,
				SceAppUtilGameCustomDataParam *param)
{
	char text[SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE];
	char value[SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE];
	int ret;

	if (eventParam == NULL) {
		paramError(130);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param == NULL) {
		paramError(272);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (eventParam->type != SCE_APPUTIL_APPEVENT_TYPE_GAME_CUSTOM_DATA) {
		paramError(131);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibMemset(text, 0, sizeof(text));
	sceClibMemset(value, 0, sizeof(value));
	sceClibMemset(param, 0, sizeof(*param));
	ret = urlDecode(text, (const char *)eventParam->data);
	if (ret != SCE_OK)
		return ret;
	sceClibMemset(value, 0, sizeof(value));
	ret = appEventGetValue(text, value, APPEVENT_KEY_ITEMID);
	if (ret != SCE_OK)
		return ret;
	param->gameCustomDataId = sceClibStrtoll(value, NULL, 0);
	return SCE_OK;
}

/* a hexadecimal digit's value, -1 for any other character */
static inline int hexDigit(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

/* the event's text with every %XX replaced by its byte */
static int urlDecode(char *dst, const char *src)
{
	int c;

	for (c = *src; c != '\0'; dst++) {
		if (c == '%') {
			*dst = hexDigit((SceUInt8)src[1]) * 16 + hexDigit((SceUInt8)src[2]);
			c = src[3];
			src += 3;
		} else {
			*dst = c;
			c = src[1];
			src++;
		}
	}
	*dst = '\0';
	return SCE_OK;
}

/* the same, leaving a % that two hexadecimal digits do not follow as it is */
static int urlDecodeChecked(char *dst, const char *src)
{
	int c;

	for (c = *src; c != '\0'; dst++) {
		if (c == '%' && src[1] != '\0' && src[2] != '\0' &&
		    hexDigit((SceUInt8)src[1]) != -1 && hexDigit((SceUInt8)src[2]) != -1) {
			*dst = hexDigit((SceUInt8)src[1]) * 16 + hexDigit((SceUInt8)src[2]);
			c = src[3];
			src += 3;
			continue;
		}
		*dst = c;
		c = src[1];
		src++;
	}
	*dst = '\0';
	return SCE_OK;
}

/*
 * the value after name in the first SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE characters of str, up to
 * the next & (not terminated); notFound when str has no such name
 */
static inline int findValue(const char *str, char *value, const char *name, SceSize nameLen,
			    int notFound)
{
	const SceUChar8 *p;
	int i;
	SceUChar8 c;

	for (i = 0, p = (const SceUChar8 *)str; *p != '\0'; ) {
		if (sceClibStrncmp((const char *)p, name, nameLen) == 0) {
			for (p = (const SceUChar8 *)&str[i]; (c = p[nameLen]) != '&' && c != '\0'; p++)
				*value++ = c;
			return SCE_OK;
		}
		i++;
		p++;
		if (i >= SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE)
			break;
	}
	return notFound;
}

static int appEventGetValue(const char *str, char *value, int key)
{
	switch (key) {
	case APPEVENT_KEY_TYPE:
		return findValue(str, value, "type=", 5, SCE_OK);
	case APPEVENT_KEY_NPCOMMID:
		return findValue(str, value, "npcommid=", 9, SCE_OK);
	case APPEVENT_KEY_UID:
		return findValue(str, value, "uid=", 4, SCE_OK);
	case APPEVENT_KEY_JID:
		return findValue(str, value, "jid=", 4, SCE_OK);
	case APPEVENT_KEY_GIFTID:
		return findValue(str, value, "giftid=", 7, SCE_OK);
	case APPEVENT_KEY_VERSION:
		return findValue(str, value, "version=", 8, SCE_OK);
	case APPEVENT_KEY_PARAM:
		return findValue(str, value, "param=", 6, SCE_OK);
	case APPEVENT_KEY_EVENT_TYPE:
		return findValue(str, value, "event_type=", 11, SCE_OK);
	case APPEVENT_KEY_TRIGGER_TYPE:
		return findValue(str, value, "trigger_type=", 13, SCE_OK);
	case APPEVENT_KEY_PATH:
		return findValue(str, value, "path=", 5, SCE_OK);
	case APPEVENT_KEY_FACTOR:
		return findValue(str, value, "factor=", 7, SCE_OK);
	case APPEVENT_KEY_SESSIONID:
		return findValue(str, value, "sessionid=", 10, SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA);
	case APPEVENT_KEY_INVITATIONID:
		return findValue(str, value, "invitationid=", 13,
				 SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA);
	case APPEVENT_KEY_ITEMID:
		return findValue(str, value, "itemid=", 7, SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA);
	}
	return SCE_OK;
}

/* "NPWR00000_00": the nine characters, then the number after the _ */
static int parseNpCommunicationId(const char *str, SceNpCommunicationId *commId)
{
	if (str == NULL)
		return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;
	if (sceClibStrnlen(str, SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE - 1) > sizeof(*commId) || str[9] != '_')
		return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;
	sceClibMemcpy_safe(commId->data, str, sizeof(commId->data));
	commId->num = sceClibStrtoll(&str[10], NULL, 10);
	return SCE_OK;
}

/* len characters of src as a string at dst; used: the bytes taken, copied: the characters */
static inline int copyToken(char *dst, SceSize dstSize, const char *src, SceSize len, SceSize *used,
			    SceSize *copied)
{
	if (len == 0) {
		if (used != NULL)
			*used = 1;
		if (dstSize == 0)
			return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;
		dst[0] = '\0';
		if (copied != NULL)
			*copied = 0;
		return SCE_OK;
	}
	if (len + 1 > dstSize)
		return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;
	sceClibMemcpy_safe(dst, src, len);
	dst[len] = '\0';
	if (used != NULL)
		*used = len + 1;
	if (copied != NULL)
		*copied = len;
	return SCE_OK;
}

/* src, up to maxLen characters, as a string at dst */
static inline int copyString(char *dst, SceSize dstSize, const char *src, SceSize maxLen)
{
	SceSize len;

	if (src != NULL && maxLen != 0) {
		for (len = 0; len < maxLen && src[len] != '\0'; len++)
			;
		if (len != 0) {
			if (len + 1 > dstSize)
				return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;
			sceClibMemcpy_safe(dst, src, len);
			dst[len] = '\0';
			return SCE_OK;
		}
	}
	if (dstSize == 0)
		return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;
	dst[0] = '\0';
	return SCE_OK;
}

/*
 * an NP ID from its jid, "<online ID>@<a1>.<us>.np.playstation.net/<resource>": the online ID,
 * the two pairs of the domain's first two labels and the resource as its options
 */
static int parseJid(const char *str, SceNpId *npId)
{
	JidText jid;
	char buf[SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE];
	SceSize used = 0, copied = 0;
	SceSize len, i, n, pos, userUsed, domainUsed, userLen, resourceLen;
	const char *domain;
	const char *p;
	char c;

	sceClibMemset(&jid, 0, sizeof(jid));
	sceClibMemset(buf, 0, sizeof(buf));
	if (str == NULL)
		return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;
	len = sceClibStrnlen(str, SCE_APPUTIL_APPEVENT_PARAM_MAXSIZE - 1);
	for (i = 0; i < len && str[i] == ' '; i++)
		;
	if (str[i] == '\0')
		return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;

	/* the user, when an @ follows it */
	for (n = 0; n < len - i; n++) {
		c = str[i + n];
		if (!APPEVENT_ISALNUM(c) && c != '+' && c != '-' && c != '_' && c != '.')
			break;
	}
	if (n == 0 || str[i + n] != '@')
		n = 0;
	if (copyToken(buf, sizeof(buf), &str[i], n, &used, &copied) != SCE_OK)
		return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;
	jid.user = buf;
	pos = i + copied;
	userUsed = used;
	if (str[pos] == '@' && pos < len)
		pos++;

	/* the domain, without trailing dots */
	for (n = 0; n < len - pos; n++) {
		c = str[pos + n];
		if (!APPEVENT_ISALNUM(c) && c != '.' && c != '_' && c != '-')
			break;
	}
	while (n != 0 && str[pos + n - 1] == '.')
		n--;
	if (copyToken(&buf[userUsed], sizeof(buf) - userUsed, &str[pos], n, &used, &copied) != SCE_OK)
		return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;
	jid.domain = &buf[userUsed];
	pos += copied;
	domainUsed = used;

	/* the resource, after the / */
	if (pos < len)
		pos++;
	if (copyString(&buf[userUsed + domainUsed], sizeof(buf) - (userUsed + domainUsed), &str[pos],
		       len - pos) != SCE_OK)
		return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;
	jid.resource = &buf[userUsed + domainUsed];

	userLen = sceClibStrnlen(jid.user, SCE_NP_ONLINEID_MAX_LENGTH);
	if (userLen == 0 || userLen > SCE_NP_ONLINEID_MAX_LENGTH)
		return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;
	domain = jid.domain;
	if (!APPEVENT_ISALNUM(domain[0]) || !APPEVENT_ISALNUM(domain[1]) || domain[2] != '.' ||
	    !APPEVENT_ISALNUM(domain[3]) || !APPEVENT_ISALNUM(domain[4]) || domain[5] != '.')
		return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;
	for (p = &domain[6]; *p != '.'; p++) {
		if (*p == '\0')
			return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;
	}
	if (sceClibStrncmp(p + 1, "playstation.net", 16) != 0)
		return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;
	resourceLen = sceClibStrnlen(jid.resource, 4);
	if (resourceLen > 4)
		return SCE_APPUTIL_ERROR_APPEVENT_PARSE_INVALID_DATA;

	sceClibMemcpy_safe(npId->handle.data, jid.user, userLen);
	npId->opt[0] = domain[0];
	npId->opt[1] = domain[1];
	npId->opt[2] = domain[3];
	npId->opt[3] = domain[4];
	if (resourceLen != 0)
		sceClibMemcpy_safe(&npId->opt[4], jid.resource, resourceLen);
	npId->reserved[0] = 1;
	return SCE_OK;
}
