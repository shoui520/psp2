/* Application-state queries registered for debugger clients. */

#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysroot_appmgr.h>
#include <appmgr/appmgr_debugger.h>
#include "app_state_internal.h"
#include "appmgr_device.h"

#if !FW_DEBUG
static const SceKernelDebugInfo assertPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0xc879dadc,
	.funcHash = 0x11a05ee1,
};
#endif

/* a launch mode or process attribute bit and the bit the debugger sees for it */
typedef struct AppmgrFlagMap {
	SceUInt32 flag;
	SceUInt32 debuggerFlag;
} AppmgrFlagMap;

int appmgrGetAppList(void *out)
{
	AppMgrAppList *list = out;
	unsigned int count = 0;
	int slot;

	if (list == NULL)
		return (int)0x80020005;
	for (slot = 0; slot != 20; ++slot) {
		int appId = g_appmgrAppStates[slot].appId;

		if (appId == -1)
			continue;
		list->appIds[count++] = appId;
		if (count >= 16)
			break;
	}
	if (count < 16)
		memset(&list->appIds[count], -1,
		       sizeof(list->appIds) - count * sizeof(list->appIds[0]));
	appmgrCaptureDeviceInfo(&list->device);
	return 0;
}

void convertAppList(AppMgrAppInfo *info, const struct AppState *app)
{
	const AppmgrFlagMap launchModes[] = {
		{ 0x10000, 0x10000 },
		{ 0x20000, 0x20000 },
		{ 0x40000, 0x40000 },
		{ 0x100000, 0x100000 },
		{ 0, 0 },
	};
	const AppmgrFlagMap processAttrs[] = {
		{ 1, 1 },
		{ 2, 2 },
		{ 4, 4 },
		{ 0, 0 },
	};
	const AppmgrFlagMap *map;
	SceUInt32 flags;

#if FW_DEBUG
	if (info == NULL) {
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 66, "convertAppList");
	}
	if (app == NULL) {
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 67, "convertAppList");
	}
#else
	if (info == NULL)
		SceDebugForDriver_391B5B74(&assertPanicInfo, __builtin_return_address(0));
	if (app == NULL)
		SceDebugForDriver_391B5B74(&assertPanicInfo, __builtin_return_address(0));
#endif
	memset(info, 0, sizeof(*info));
	info->appId = app->appId;
	info->launchSourceAppId = app->launchSourceAppId;
	info->processId = app->process.processId;
	info->launchSourceProcessId = app->launchSourceProcessId;
	info->budgetId = app->process.budgetId;
	info->fdLimit = app->process.maxFD;
	info->dirLimit = app->process.maxDIR;
	__builtin_memcpy(info->appName, app->process.appName, sizeof(info->appName) - 1);
	memcpy(info->appPath, app->launchInfo.appPath, sizeof(info->appPath) - 1);
	flags = 0;
	for (map = launchModes; map->flag != 0; ++map)
		if (map->flag & app->launchMode)
			flags |= map->debuggerFlag;
	info->launchMode = flags;
	flags = 0;
	for (map = processAttrs; map->flag != 0; ++map)
		if (map->flag & app->launchInfo.processAttr)
			flags |= map->debuggerFlag;
	info->processAttr = flags;
}

int appmgrGetAppInfoByAppId(int appId, void *out)
{
	AppMgrAppInfo *info = out;
	int slot;

	if (info == NULL)
		return (int)0x80020005;
	for (slot = 0; slot != 20; ++slot) {
		if (g_appmgrAppStates[slot].appId != -1 && g_appmgrAppStates[slot].appId == appId) {
			convertAppList(info, &g_appmgrAppStates[slot]);
			return 0;
		}
	}
	return (int)0x80802012;
}

int appmgrGetAppInfoByPid(int pid, void *out)
{
	AppMgrAppInfo *info = out;
	int slot;

	if (info == NULL)
		return (int)0x80020005;
	for (slot = 0; slot != 20; ++slot) {
		if (g_appmgrAppStates[slot].appId != -1 &&
		    g_appmgrAppStates[slot].process.processId == pid) {
			convertAppList(info, &g_appmgrAppStates[slot]);
			return 0;
		}
	}
	return (int)0x80802012;
}

void appmgrRegisterDebuggerCallbacks(void)
{
	SceSysrootForDriver_2BE874EF(appmgrGetAppInfoByPid);
	SceSysrootForDriver_3B19B06B(appmgrGetAppInfoByAppId);
	SceSysrootForDriver_70AD47A9(appmgrGetAppList);
}
