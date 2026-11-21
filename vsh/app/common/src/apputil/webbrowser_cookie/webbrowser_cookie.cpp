/*
 * The web browser's cookies for an application (sceAppUtilAddCookieWebBrowser,
 * sceAppUtilResetCookieWebBrowser): each one kept as its Set-Cookie string in cookie.dat in the
 * application's work directory, the file removed to reset them.
 */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <kernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <apputil.h>
#include "../apputil_private.h"

/* a cookie's strings, and the Set-Cookie string they make */
#define WEBBROWSER_COOKIE_STR_SIZE	4096
#define WEBBROWSER_COOKIE_URI_MAX	1024
/* cookie.dat keeps ten records */
#define WEBBROWSER_COOKIE_COUNT		10
/* the stack sceAppUtilAddCookieWebBrowser needs */
#define WEBBROWSER_COOKIE_STACK_SIZE	8192

/* name=value; expires=...; path=...; domain=...; secure */
int CreateCookieString(const SceAppUtilWebBrowserAddCookieParam *param, char *str)
{
	const char *s;

	s = (const char *)param->wbstrName;
	sceClibStrncat(str, s, sceClibStrnlen(s, WEBBROWSER_COOKIE_STR_SIZE));
	sceClibStrncat(str, "=", sizeof("="));
	s = (const char *)param->wbstrValue;
	sceClibStrncat(str, s, sceClibStrnlen(s, WEBBROWSER_COOKIE_STR_SIZE));
	if (param->wbstrExpires != NULL) {
		sceClibStrncat(str, "; ", sizeof("; "));
		sceClibStrncat(str, "expires=", sizeof("expires="));
		s = (const char *)param->wbstrExpires;
		sceClibStrncat(str, s, sceClibStrnlen(s, WEBBROWSER_COOKIE_STR_SIZE));
	}
	if (param->wbstrPath != NULL) {
		sceClibStrncat(str, "; ", sizeof("; "));
		sceClibStrncat(str, "path=", sizeof("path="));
		s = (const char *)param->wbstrPath;
		sceClibStrncat(str, s, sceClibStrnlen(s, WEBBROWSER_COOKIE_STR_SIZE));
	}
	if (param->wbstrDomain != NULL) {
		sceClibStrncat(str, "; ", sizeof("; "));
		sceClibStrncat(str, "domain=", sizeof("domain="));
		s = (const char *)param->wbstrDomain;
		sceClibStrncat(str, s, sceClibStrnlen(s, WEBBROWSER_COOKIE_STR_SIZE));
	}
	if (param->isSecure) {
		sceClibStrncat(str, "; ", sizeof("; "));
		sceClibStrncat(str, "secure", sizeof("secure"));
	}
	return 0;
}

/* the cookie appended to the work directory's file of that name */
int saveCookie(const char *name, SceSize nameLength, const SceAppUtilWebBrowserAddCookieParam *param)
{
	WebBrowserFileHeader header;
	char mountPoint[16];
	char path[256];
	WebBrowserFileRecord record;
	const char *uri;
	int ret;

	sceClibMemset(&header, 0, sizeof(header));
	sceClibMemset(&record, 0, sizeof(record));
	ret = CreateCookieString(param, record.value);
	if (ret != 0)
		return ret;
	uri = (const char *)param->wbstr;
	sceClibStrncpy(record.uri, uri, sceClibStrnlen(uri, sceClibStrnlen(uri, WEBBROWSER_COOKIE_STR_SIZE)));
	sceClibMemset(path, 0, sizeof(path));
	sceClibMemset(mountPoint, 0, sizeof(mountPoint));
	if (webBrowserFileMount(WEBBROWSER_FILE_MOUNT_WORKDIR, mountPoint) < 0)
		return SCE_APPUTIL_ERROR_NOT_MOUNTED;
	sceClibStrncpy(path, mountPoint, sizeof(path));
	sceClibStrncat(path, name, nameLength);
	WebBrowserFile file;
	ret = file.Append(path, &header, &record, WEBBROWSER_COOKIE_COUNT);
	if (ret < 0)
		ret = SCE_APPUTIL_ERROR_BUSY;
	webBrowserFileUmount(mountPoint);
	return ret;
}

int _sceAppUtilResetCookieWebBrowser(const SceAppUtilWebBrowserResetCookieParam *param)
{
	char path[256];
	SceUInt32 i;

	if (param == NULL) {
		paramError(150);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	for (i = 0; i < sizeof(param->reserved); i++) {
		if (param->reserved[i] != 0) {
			paramError(2);
			return SCE_APPUTIL_ERROR_PARAMETER;
		}
	}
	sceClibMemset(path, 0, sizeof(path));
	sceClibStrncpy(path, "cookie.dat", sizeof(path));
	return webBrowserFileRemove(path);
}

int _sceAppUtilAddCookieWebBrowser(const SceAppUtilWebBrowserAddCookieParam *param)
{
	char path[256];
	SceUInt32 i, size;
	int ret;

	if (param == NULL) {
		paramError(250);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	for (i = 0; i < sizeof(param->reserved); i++) {
		if (param->reserved[i] != 0) {
			paramError(2);
			return SCE_APPUTIL_ERROR_PARAMETER;
		}
	}
	if (param->wbstr == NULL) {
		paramError(252);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (sceClibStrnlen((const char *)param->wbstr, WEBBROWSER_COOKIE_URI_MAX) == WEBBROWSER_COOKIE_STR_SIZE) {
		paramError(253);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param->wbstrName == NULL) {
		paramError(254);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (sceClibStrnlen((const char *)param->wbstrName, WEBBROWSER_COOKIE_STR_SIZE) == WEBBROWSER_COOKIE_STR_SIZE) {
		paramError(255);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param->wbstrValue == NULL) {
		paramError(256);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (sceClibStrnlen((const char *)param->wbstrValue, WEBBROWSER_COOKIE_STR_SIZE) == WEBBROWSER_COOKIE_STR_SIZE) {
		paramError(257);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param->wbstrExpires != NULL &&
	    sceClibStrnlen((const char *)param->wbstrExpires, WEBBROWSER_COOKIE_STR_SIZE) == WEBBROWSER_COOKIE_STR_SIZE) {
		paramError(259);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param->wbstrPath != NULL &&
	    sceClibStrnlen((const char *)param->wbstrPath, WEBBROWSER_COOKIE_STR_SIZE) == WEBBROWSER_COOKIE_STR_SIZE) {
		paramError(261);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	if (param->wbstrDomain != NULL &&
	    sceClibStrnlen((const char *)param->wbstrDomain, WEBBROWSER_COOKIE_STR_SIZE) == WEBBROWSER_COOKIE_STR_SIZE) {
		paramError(263);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	size = sceClibStrnlen((const char *)param->wbstrName, WEBBROWSER_COOKIE_STR_SIZE) + sceClibStrnlen("=", sizeof("=")) +
	       sceClibStrnlen((const char *)param->wbstrValue, WEBBROWSER_COOKIE_STR_SIZE) + 1;
	if (param->wbstrExpires != NULL)
		size += sceClibStrnlen("; ", sizeof("; ")) + sceClibStrnlen("expires=", sizeof("expires=")) +
			sceClibStrnlen("=", sizeof("=")) +
			sceClibStrnlen((const char *)param->wbstrExpires, WEBBROWSER_COOKIE_STR_SIZE);
	if (param->wbstrPath != NULL)
		size += sceClibStrnlen("; ", sizeof("; ")) + sceClibStrnlen("path=", sizeof("path=")) +
			sceClibStrnlen("=", sizeof("=")) +
			sceClibStrnlen((const char *)param->wbstrPath, WEBBROWSER_COOKIE_STR_SIZE);
	if (param->wbstrDomain != NULL)
		size += sceClibStrnlen("; ", sizeof("; ")) + sceClibStrnlen("domain=", sizeof("domain=")) +
			sceClibStrnlen("=", sizeof("=")) +
			sceClibStrnlen((const char *)param->wbstrDomain, WEBBROWSER_COOKIE_STR_SIZE);
	if (param->isSecure)
		size += sceClibStrnlen("; ", sizeof("; ")) + sceClibStrnlen("secure", sizeof("secure"));
	if (size >= WEBBROWSER_COOKIE_STR_SIZE) {
		paramError(251);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	ret = getFreeStackSize();
	if (ret < WEBBROWSER_COOKIE_STACK_SIZE)
		return SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE;
	sceClibMemset(path, 0, sizeof(path));
	sceClibStrncpy(path, "cookie.dat", sizeof(path));
	return saveCookie(path, sceClibStrnlen(path, sizeof(path)), param);
}
