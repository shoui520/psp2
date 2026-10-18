/* Application and device ownership captured for debugger clients. */
#pragma once

#include <stddef.h>
#include <scetypes.h>
#include <sysmem/sysroot.h>

typedef struct AppMgrDeviceInfo {
	SceUID soundExclusivePid;
	SceUID mainOutPortOwner;
	SceUID bgmOutPortOwner;
	SceUID voiceOutPortOwner;
	SceUID inputPortOwner;
	SceUID backRenderPid;
	SceUID inputOwnerPid;
	int bgmProxyAppId;
	SceUID shellPid;
	int activeAppId;
	SceUID renderingPids[16];
	SceUID touchPids[16];
	SceUID displayOwner0;
	SceUID displayOwner1;
	SceUID systemChatPid;
	int backRenderAppId;
} AppMgrDeviceInfo;

typedef struct AppMgrAppList {
	int appIds[16];
	AppMgrDeviceInfo device;
} AppMgrAppList;

typedef struct AppMgrAppInfo {
	int appId;
	int launchSourceAppId;
	SceUID processId;
	SceUID launchSourceProcessId;
	SceUID budgetId;
	char appName[32];
	SceUInt32 launchMode;
	SceUInt32 processAttr;
	SceUInt32 fdLimit;
	SceUInt32 dirLimit;
	char appPath[256];
} AppMgrAppInfo;

_Static_assert(sizeof(AppMgrAppInfo) == 324, "AppMgrAppInfo size");
_Static_assert(offsetof(AppMgrAppInfo, appId) == 0 &&
	       offsetof(AppMgrAppInfo, launchSourceAppId) == 4 &&
	       offsetof(AppMgrAppInfo, processId) == 8 &&
	       offsetof(AppMgrAppInfo, launchSourceProcessId) == 12 &&
	       offsetof(AppMgrAppInfo, budgetId) == 16,
	       "AppMgrAppInfo identities");
_Static_assert(offsetof(AppMgrAppInfo, appName) == 20 &&
	       offsetof(AppMgrAppInfo, launchMode) == 52 &&
	       offsetof(AppMgrAppInfo, processAttr) == 56 &&
	       offsetof(AppMgrAppInfo, fdLimit) == 60 &&
	       offsetof(AppMgrAppInfo, dirLimit) == 64 &&
	       offsetof(AppMgrAppInfo, appPath) == 68,
	       "AppMgrAppInfo fields");

_Static_assert(sizeof(AppMgrDeviceInfo) == 184, "AppMgrDeviceInfo size");
_Static_assert(offsetof(AppMgrDeviceInfo, renderingPids) == 40 &&
	       offsetof(AppMgrDeviceInfo, touchPids) == 104 &&
	       offsetof(AppMgrDeviceInfo, displayOwner0) == 168,
	       "AppMgrDeviceInfo lists and owners");
_Static_assert(sizeof(AppMgrAppList) == 248, "AppMgrAppList size");
_Static_assert(offsetof(AppMgrAppList, device) == 64, "AppMgrAppList device");
