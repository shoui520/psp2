/*
 * The web browser opened for an application that is to be called back
 * (SceAppUtilWebBrowserCBLimited_3ED8F841): the URI names the application and the string to call it
 * back with, and the user agent the browser is to use is kept in useragent.dat.
 */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <kernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <appmgr.h>
#include <apputil.h>
#include <appmgr/appmgr.h>
#include "../apputil_private.h"

/* the longest URI the browser and callback strings may make together */
#define WEBBROWSER_CB_STR_MAX		896
#define WEBBROWSER_USER_AGENT_SIZE	256
/* useragent.dat keeps one record */
#define WEBBROWSER_USER_AGENT_COUNT	1

/* the URI's callback part: "*cbapp^len=<n>*str=<the application>*cb^len=<n>*str=<cbstr>" */
static inline void appendCallback(char *cursor, SceSize *length,
				  const SceAppUtilWebBrowserCBLimitedParam *param)
{
	char part[WEBBROWSER_URI_PART_SIZE];
	SceAppMgrAppStatus status;
	SceSize n;
	int ret;

	sceClibMemsetChk(part, 0, sizeof(part), sizeof(part));
	if (sceAppMgrGetStatusByName(NULL, &status) != 0) {
		paramError(150);
		return;
	}
	n = webBrowserUriAppend(&cursor, "*", WEBBROWSER_CB_STR_MAX);
	webBrowserUriAdvance(&cursor, length, n);
	n = webBrowserUriAppend(&cursor, "cbapp", WEBBROWSER_CB_STR_MAX);
	webBrowserUriAdvance(&cursor, length, n);
	n = webBrowserUriAppend(&cursor, "^", WEBBROWSER_CB_STR_MAX);
	webBrowserUriAdvance(&cursor, length, n);
	ret = sceClibSnprintf(part, sizeof(part), "%s=%d%s%s=", "len",
			      sceClibStrnlen(status.appName, SCE_APPUTIL_TITLE_ID_DATA_SIZE - 1), "*", "str");
	if (ret < 0) {
		paramError(154);
		return;
	}
	sceClibStrncpy(cursor, part, ret);
	webBrowserUriAdvance(&cursor, length, ret);
	sceClibStrncpy(cursor, status.appName, sceClibStrnlen(status.appName, SCE_APPUTIL_TITLE_ID_DATA_SIZE - 1));
	webBrowserUriAdvance(&cursor, length, sceClibStrnlen(status.appName, SCE_APPUTIL_TITLE_ID_DATA_SIZE - 1));
	n = webBrowserUriAppend(&cursor, "*", WEBBROWSER_CB_STR_MAX);
	webBrowserUriAdvance(&cursor, length, n);
	n = webBrowserUriAppend(&cursor, "cb", WEBBROWSER_CB_STR_MAX);
	webBrowserUriAdvance(&cursor, length, n);
	n = webBrowserUriAppend(&cursor, "^", WEBBROWSER_CB_STR_MAX);
	webBrowserUriAdvance(&cursor, length, n);
	ret = sceClibSnprintf(part, sizeof(part), "%s=%d%s%s=", "len", param->cbstrLength, "*", "str");
	if (ret < 0) {
		paramError(154);
		return;
	}
	sceClibStrncpy(cursor, part, ret);
	webBrowserUriAdvance(&cursor, length, ret);
	sceClibStrncpy(cursor, (const char *)param->cbstr, param->cbstrLength);
	webBrowserUriAdvance(&cursor, length, param->cbstrLength);
}

/* the user agent for the page the browser opens, appended to the work directory's file name */
static int saveUserAgent(const char *name, SceSize nameLength, const SceAppUtilWebBrowserCBLimitedParam *param)
{
	WebBrowserFileHeader header;
	char mountPoint[16];
	char path[256];
	WebBrowserFileRecord record;
	const char *str;
	int ret;

	sceClibMemset(&header, 0, sizeof(header));
	sceClibMemset(&record, 0, sizeof(record));
	str = (const char *)param->userAgent;
	if (str == NULL)
		return -1;
	sceClibStrncat(record.value, str, sceClibStrnlen(str, WEBBROWSER_USER_AGENT_SIZE));
	str = (const char *)param->wbstr;
	sceClibStrncpy(record.uri, str, sceClibStrnlen(str, sceClibStrnlen(str, WEBBROWSER_USER_AGENT_SIZE)));
	sceClibMemset(path, 0, sizeof(path));
	sceClibMemset(mountPoint, 0, sizeof(mountPoint));
	if (webBrowserFileMount(WEBBROWSER_FILE_MOUNT_WORKDIR, mountPoint) < 0)
		return SCE_APPUTIL_ERROR_NOT_MOUNTED;
	sceClibStrncpy(path, mountPoint, sizeof(path));
	sceClibStrncat(path, name, nameLength);
	WebBrowserFile file;
	ret = file.Append(path, &header, &record, WEBBROWSER_USER_AGENT_COUNT);
	if (ret < 0)
		ret = SCE_APPUTIL_ERROR_BUSY;
	webBrowserFileUmount(mountPoint);
	return ret;
}

int _sceAppUtilLaunchWebBrowserCBLimited(const SceAppUtilWebBrowserCBLimitedParam *param)
{
	char *cursor;
	SceSize length;
	char part[WEBBROWSER_URI_PART_SIZE];
	char uri[WEBBROWSER_URI_SIZE];
	const char *prefix;
	SceSize wbLength, cbLength, n;
	int launchMode, ret;

	cursor = uri;
	length = 0;
	sceClibMemsetChk(uri, 0, sizeof(uri), sizeof(uri));
	sceClibMemsetChk(part, 0, sizeof(part), sizeof(part));
	if (param == NULL) {
		paramError(150);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param->wbstr == NULL) {
		paramError(151);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	wbLength = sceClibStrnlen((const char *)param->wbstr, WEBBROWSER_CB_STR_MAX);
	if (wbLength != param->wbstrLength) {
		paramError(152);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param->cbstr == NULL) {
		paramError(151);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	cbLength = sceClibStrnlen((const char *)param->cbstr, WEBBROWSER_CB_STR_MAX);
	if (cbLength != param->cbstrLength) {
		paramError(152);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if ((int)(cbLength + wbLength) > WEBBROWSER_CB_STR_MAX) {
		paramError(152);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param->userAgent != NULL &&
	    sceClibStrnlen((const char *)param->userAgent, WEBBROWSER_USER_AGENT_SIZE) >= WEBBROWSER_USER_AGENT_SIZE)
		return SCE_APPUTIL_ERROR_PARAMETER;
	ret = getFreeStackSize();
	if (ret < APPUTIL_LAUNCH_STACK_SIZE)
		return SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE;
	switch (param->launchMode & 3) {
	case 0:
		prefix = "wbapp0:";
		launchMode = 0x20000;
		break;
	case 1:
		prefix = "wbapp1:";
		launchMode = 0x60000;
		break;
	default:
		paramError(153);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibStrncpy(cursor, prefix, 7);
	webBrowserUriAdvance(&cursor, &length, 7);
	switch (param->launchMode & 0x3c) {
	case 0:
		prefix = "openurl?";
		n = 8;
		break;
	case 4:
		prefix = "search?";
		n = 7;
		break;
	default:
		paramError(153);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibStrncpy(cursor, prefix, n);
	webBrowserUriAdvance(&cursor, &length, n);
	ret = sceClibSnprintf(part, sizeof(part), "%s%d&%s", "len=", param->wbstrLength, "str=");
	if (ret < 0) {
		paramError(154);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	sceClibStrncpy(cursor, part, ret);
	webBrowserUriAdvance(&cursor, &length, ret);
	sceClibStrncpy(cursor, (const char *)param->wbstr, param->wbstrLength);
	webBrowserUriAdvance(&cursor, &length, param->wbstrLength);
	appendCallback(cursor, &length, param);
	saveUserAgent("useragent.dat", sizeof("useragent.dat"), param);
	return sceAppMgrLaunchAppByUri(launchMode, uri);
}
