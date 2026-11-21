/*
 * sceAppUtilStoreBrowse: the PlayStation Store opened at a product, a category or a code to
 * redeem. The "2" kinds open it inside a content install period and wait for the store to close.
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
#include "apputil_private.h"

/* the store's URI: "psts:redeem?code1=XXXX&code2=XXXX&code3=XXXX" and the longest id */
#define STORE_BROWSE_URI_SIZE		78
/* a code to redeem: three groups of four letters or digits, "XXXX-XXXX-XXXX" */
#define STORE_REDEEM_CODE_GROUPS	3
#define STORE_REDEEM_GROUP_SIZE		4
#define STORE_REDEEM_CODE_LENGTH	14

/* set by the store's callback when the store is closed */
static bool g_storeBrowseFinished;

/* the store application's notice that it closed */
static SceInt32 storeBrowseCallback(SceUID notifyId, SceInt32 notifyCount, SceInt32 notifyArg,
				    void *pCommon)
{
	(void)notifyId;
	(void)notifyCount;
	(void)notifyArg;
	(void)pCommon;
	g_storeBrowseFinished = true;
	return 1;
}

int storeBrowse(SceAppUtilStoreBrowseParam *param)
{
	char here;
	char code[STORE_REDEEM_CODE_GROUPS][STORE_REDEEM_GROUP_SIZE];
	AppmgrLaunchAppByNameOption option;
	SceKernelThreadInfo info;
	const SceChar8 *id;
	SceUInt32 group, n, i;
	SceUID callbackId;
	int ret, c;

	if (param == NULL) {
		paramError(140);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	info.size = sizeof(info);
	ret = sceKernelGetThreadInfo(0, &info);
	if (ret >= 0)
		ret = &here - (char *)info.pStack;
	if (ret < APPUTIL_LAUNCH_STACK_SIZE)
		return SCE_KERNEL_ERROR_ILLEGAL_STACK_SIZE;
	switch (param->type) {
	case SCE_APPUTIL_STORE_BROWSE_TYPE_PRODUCT2:
	case SCE_APPUTIL_STORE_BROWSE_TYPE_CATEGORY2:
	case SCE_APPUTIL_STORE_BROWSE_TYPE_PRODUCT_CODE2:
		ret = sceAppMgrContentInstallPeriodStart();
		if (ret != 0) {
			if (ret == APPMGR_ERROR_CONTENT_INSTALL_BUSY)
				ret = SCE_APPUTIL_ERROR_BUSY;
			return ret;
		}
		break;
	}
	SceSize uriSize = STORE_BROWSE_URI_SIZE;
	char uri[uriSize];
	id = param->id;
	switch (param->type) {
	case SCE_APPUTIL_STORE_BROWSE_TYPE_PRODUCT:
	case SCE_APPUTIL_STORE_BROWSE_TYPE_PRODUCT2:
		sceClibSnprintf(uri, STORE_BROWSE_URI_SIZE, "psts:browse?product=%s", id);
		break;
	case SCE_APPUTIL_STORE_BROWSE_TYPE_CATEGORY:
	case SCE_APPUTIL_STORE_BROWSE_TYPE_CATEGORY2:
		sceClibSnprintf(uri, STORE_BROWSE_URI_SIZE, "psts:browse?category=%s", id);
		break;
	case SCE_APPUTIL_STORE_BROWSE_TYPE_PRODUCT_CODE:
	case SCE_APPUTIL_STORE_BROWSE_TYPE_PRODUCT_CODE2:
		if (id == NULL || id[0] == '\0') {
			sceClibSnprintf(uri, STORE_BROWSE_URI_SIZE, "psts:redeem");
			break;
		}
		group = 0;
		n = 0;
		i = 0;
		do {
			c = (SceChar8)id[i];
			if (c == '\0')
				break;
			if (i == STORE_REDEEM_GROUP_SIZE || i == STORE_REDEEM_GROUP_SIZE * 2 + 1) {
				if (c != '-')
					break;
			} else {
				if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
					break;
				code[group][n++] = c;
				if (n >= STORE_REDEEM_GROUP_SIZE) {
					group++;
					if (group >= STORE_REDEEM_CODE_GROUPS)
						break;
					n = 0;
				}
			}
			i++;
		} while (i < STORE_REDEEM_CODE_LENGTH);
		if (group != STORE_REDEEM_CODE_GROUPS || n != STORE_REDEEM_GROUP_SIZE) {
			paramError(142);
			return SCE_APPUTIL_ERROR_PARAMETER;
		}
		sceClibSnprintf(uri, STORE_BROWSE_URI_SIZE, "psts:redeem?code1=%.4s&code2=%.4s&code3=%.4s", code[0],
				code[1], code[2]);
		break;
	default:
		paramError(141);
		return SCE_APPUTIL_ERROR_PARAMETER;
	}
	switch (param->type) {
	case SCE_APPUTIL_STORE_BROWSE_TYPE_PRODUCT2:
	case SCE_APPUTIL_STORE_BROWSE_TYPE_CATEGORY2:
	case SCE_APPUTIL_STORE_BROWSE_TYPE_PRODUCT_CODE2:
		callbackId = sceKernelCreateCallback("SceAppUtilStoreBrowse", 0, storeBrowseCallback, NULL);
		ret = callbackId;
		if (callbackId >= 0) {
			g_storeBrowseFinished = false;
			sceClibMemsetChk(&option, 0, sizeof(option), sizeof(option));
			option.size = sizeof(option);
			option.launchMode = APPUTIL_STORE_LAUNCH_MODE;
			option.reserved = -1;
			option.launchCallbackId = callbackId;
			option.paramCallbackId = -1;
			option.creatorAppId = -1;
			ret = sceAppMgrLaunchAppByUri2(uri, &option);
			if (ret != 0) {
				sceKernelDeleteCallback(callbackId);
			} else {
				while (1) {
					sceKernelCheckCallback();
					if (g_storeBrowseFinished)
						break;
					sceKernelDelayThread(20000);
				}
				ret = 0;
			}
		}
		c = sceAppMgrContentInstallPeriodStop();
		if (c != 0)
			ret = c;
		return ret;
	}
	return sceAppMgrLaunchAppByUri(APPUTIL_STORE_LAUNCH_MODE, uri);
}
