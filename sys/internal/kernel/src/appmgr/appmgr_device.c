/* Audio and device arbitration. */

#include <sysmem/debug.h>
#include <intrmgr/syscall.h>
#include <threadmgr/threadmgr.h>
#include <touch/touch.h>
#include <appmgr/appmgr_debugger.h>
#include <acmgr/acmgr.h>
#include <psp2kern/kernel/sysclib.h>
#include "appmgr_device.h"
#include <display/display.h>
#include <appmgr/appmgr.h>

extern SceUID g_appmgrMainMutex;
extern SceUID g_appmgrMainMutexOwner;
extern int g_appmgrPowerSaveMode;
extern int dumpBgmState(void);
extern int _sceAppMgrReleaseSoundOutExclusive(void);
extern void updateAudioArbitration2NoLock(void);
extern int _updateDeviceArbitration2(void);
extern int sceAudioOutSetAdoptMode(int mode);
extern int SceAudioForDriver_629261CD(SceUID pid, int mask, unsigned int volume,
		int duration);
extern int SceAudioForDriver_3440C1CA(SceUID pid, int type, int adopt,
		int duration, int wait);
extern int SceAudioInForDriver_E4042F6F(int pid, int unusedArgument1,
		int unusedArgument2, int delay);

SceUInt32 g_appmgrDisplayMergeAlpha = 256;
SceUInt32 g_appmgrDisplayMergeSetting = 128;
SceUID g_appmgrShellPid;
int g_appmgrActiveAppId;
SceUID g_appmgrRenderingPids[8];
TouchProcessPriorities g_appmgrTouchProcessPriorities;
int g_appmgrPrioritizeSystemChat;
int g_appmgrInputImposed;
int g_appmgrBtrmOwner;
int g_appmgrRecommendedScreenOrientation;
int g_appmgrCtrlpMode;
SceUID g_appmgrSoundExclusivePid;
int g_appmgrSoundExclusivePriority;
int g_appmgrShellMainPortEnabled;
SceUID g_appmgrMainOutPortOwner;
SceUID g_appmgrBgmOutPortOwner;
int g_appmgrBgmOutPortOwnerPriority;
int g_appmgrBgmOutPortOwnerSubPriority;
SceUID g_appmgrVoiceOutPortOwner;
SceUID g_appmgrInputPortOwner;
SceUInt32 g_appmgrOtherPortVolume;
SceUID g_appmgrDisplayOwner[2];
SceUID g_appmgrSystemChatPid;
SceUInt32 g_appmgrSystemChatDuckingVolume;
int g_appmgrBackRenderAppId;
AppmgrDeviceOwners g_appmgrDeviceOwners;
AppmgrAudioApp g_appmgrAudioApps[20];
SceUID g_appmgrAudioUpdateMutex;

#if !FW_DEBUG
static const SceKernelDebugInfo deviceLockPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0xc879dadc,
	.funcHash = 0x11a05ee1,
};

static const SceKernelDebugInfo mainMutexPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0x79d3a1f0,
	.funcHash = 0x9d536bce,
};

static const SceKernelDebugInfo enableInPortPanicInfo = {
	.fileHash = 0xbbec1541,
	.lineHash = 0x217b1653,
	.funcHash = 0x0b787d98,
};

static const SceKernelDebugInfo mainVolumePanicInfo = {
	.fileHash = 0xbbec1541,
	.lineHash = 0x2f7a7d06,
	.funcHash = 0xa5ad555c,
};

static const SceKernelDebugInfo bgmVolumePanicInfo = {
	.fileHash = 0xbbec1541,
	.lineHash = 0xdd1ad517,
	.funcHash = 0xa5ad555c,
};

static const SceKernelDebugInfo enableOutPortPanicInfo = {
	.fileHash = 0xbbec1541,
	.lineHash = 0x32358b91,
	.funcHash = 0xfdc55a7d,
};

void appmgrDeviceLockPanic(void)
{
	SceDebugForDriver_391B5B74(&deviceLockPanicInfo, __builtin_return_address(0));
}

void appmgrMainMutexPanic(void)
{
	SceDebugForDriver_391B5B74(&mainMutexPanicInfo, __builtin_return_address(0));
}
#endif

AppmgrAudioApp *appmgrResolveBgmProxy(AppmgrAudioApp *app)
{
	AppmgrAudioApp *current = app;
	int remaining = 20;
	int index;

	if (app == NULL)
		return app;
	while (current->proxyAppName[0] != '\0') {
		for (index = 0; index != 20; ++index) {
			AppmgrAudioApp *candidate = &g_appmgrAudioApps[index];

			if (candidate->appId > 0 &&
			    strncmp(candidate->appName, current->proxyAppName, 32) == 0)
				break;
		}
		if (index == 20)
			return current;
		current = &g_appmgrAudioApps[index];
		if (--remaining == 0)
			return app;
	}
	return current;
}

#if FW_DEBUG
int dumpBgmState(void)
{
	int index;

	SCE_DBG_PRINTF_FUNC(0, 503, "dumpBgmState", "------------------------------------------\n");
	SCE_DBG_PRINTF_FUNC(0, 504, "dumpBgmState", "soundExclusivePid=0x%08x\n",
		g_appmgrSoundExclusivePid);
	SCE_DBG_PRINTF_FUNC(0, 505, "dumpBgmState", "soundExclusivePriority=0x%08x\n",
		g_appmgrSoundExclusivePriority);
	SCE_DBG_PRINTF_FUNC(0, 506, "dumpBgmState", "shellMainPortEnabled=0x%08x\n",
		g_appmgrShellMainPortEnabled);
	SCE_DBG_PRINTF_FUNC(0, 507, "dumpBgmState", "mainOutPortOwner=0x%08x\n",
		g_appmgrMainOutPortOwner);
	SCE_DBG_PRINTF_FUNC(0, 508, "dumpBgmState", "bgmOutPortOwner=0x%08x\n",
		g_appmgrBgmOutPortOwner);
	SCE_DBG_PRINTF_FUNC(0, 509, "dumpBgmState", "bgmOutPortOwnerPriority=%d\n",
		g_appmgrBgmOutPortOwnerPriority);
	SCE_DBG_PRINTF_FUNC(0, 510, "dumpBgmState", "bgmOutPortOwnerSubPriority=%d\n",
		g_appmgrBgmOutPortOwnerSubPriority);
	SCE_DBG_PRINTF_FUNC(0, 511, "dumpBgmState", "voiceOutPortOwner=0x%08x\n",
		g_appmgrVoiceOutPortOwner);
	SCE_DBG_PRINTF_FUNC(0, 512, "dumpBgmState", "inputPortOwner=0x%08x\n",
		g_appmgrInputPortOwner);
	SCE_DBG_PRINTF_FUNC(0, 514, "dumpBgmState", "displayOwner0=0x%08x\n",
		g_appmgrDisplayOwner[0]);
	SCE_DBG_PRINTF_FUNC(0, 515, "dumpBgmState", "displayOwner1=0x%08x\n",
		g_appmgrDisplayOwner[1]);
	SCE_DBG_PRINTF_FUNC(0, 516, "dumpBgmState", "BTRM owner=0x%08x\n",
		g_appmgrBtrmOwner);
	for (index = 0; index != 20; ++index) {
		AppmgrAudioApp *app = &g_appmgrAudioApps[index];

		if (app->appId > 0)
			SCE_DBG_PRINTF_FUNC(0, 523, "dumpBgmState",
				"[%d] %s(0x%08x, 0x%08x) : bgm_prio = 0x%08x, enable_bgm_sub_prio = %d, sub_bgm_prio = 0x%08x, proxy=%s isRunning=%d\n",
				index, app->appName, app->processId, app->appId,
				app->bgmPriority, app->enableBgmSubPriority,
				app->bgmSubPriority, app->proxyAppName, app->isRunning);
	}
	return SCE_DBG_PRINTF_FUNC(0, 526, "dumpBgmState", "------------------------------------------\n");
}

int lockAudioUpdate(void)
{
	SceUID mutexOwner = g_appmgrMainMutexOwner;
	int result;

	if (mutexOwner == sceKernelGetThreadId()) {
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 147, "lockAudioUpdate");
	}
	result = sceKernelLockMutex(g_appmgrAudioUpdateMutex, 1, NULL);
	if (result != 0 && (SceUInt32)result + 0x7ffd7fd4U > 1) {
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 152, "lockAudioUpdate",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
	}
	return result;
}
#endif

#if !FW_DEBUG
void appmgrEnableOutPortPanic(void)
{
	SceDebugForDriver_391B5B74(&enableOutPortPanicInfo, __builtin_return_address(0));
}
#endif

void setVolume(const SceUID *exclusivePid, const SceUInt32 *otherVolume)
{
	int index;

	for (index = 0; index != 20; ++index) {
		AppmgrAudioApp *app = &g_appmgrAudioApps[index];
		SceUID processId;
		SceUInt32 volume;
		int result;

		if (app->appId <= 0)
			continue;
		processId = app->processId;
		if (processId <= 0)
			continue;
		if (*exclusivePid == -1)
			volume = processId == g_appmgrSystemChatPid ? 256 : *otherVolume;
		else
			volume = processId == *exclusivePid ? 256 : 0;
		result = SceAudioForDriver_629261CD(processId, 6, volume, 0);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 968, "setVolume",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			SceDebugForDriver_00CCE39C(&mainVolumePanicInfo,
				__builtin_return_address(0), "0x%08x,0x%08x\n", result, 0);
#endif
		}
		result = SceAudioForDriver_629261CD(app->processId, 8, volume, 0);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 971, "setVolume",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			SceDebugForDriver_00CCE39C(&bgmVolumePanicInfo,
				__builtin_return_address(0), "0x%08x,0x%08x\n", result, 0);
#endif
		}
	}
}

void _deviceAddApp(int appId)
{
	SceUID mutexOwner;
	int index;
	AppmgrAudioApp *app;

	if (appId <= 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 307, "_deviceAddApp");
#else
		appmgrDeviceLockPanic();
#endif
	}
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 308, "_deviceAddApp");
#else
		appmgrDeviceLockPanic();
#endif
	}
	for (index = 1; index != 20; ++index) {
		app = &g_appmgrAudioApps[index];
		if (app->appId > 0 && app->appId == appId) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_CHECK_FUNC(0, 312, "_deviceAddApp");
#else
			appmgrDeviceLockPanic();
#endif
		}
	}
	for (index = 1; index != 20; ++index) {
		app = &g_appmgrAudioApps[index];
		if (app->appId == -1) {
			app->appId = appId;
			__builtin_memset(app->appName, 0, sizeof(app->appName));
			app->processId = -1;
			app->isRunning = 0;
			app->bgmPriority = 0;
			app->enableBgmSubPriority = 0;
			app->bgmSubPriority = 0;
			__builtin_memset(app->proxyAppName, 0, sizeof(app->proxyAppName));
			app->soundExclusivePriority = -1;
			app->displayRequestEnabled = 0;
			app->displayRequestMode = 0;
			app->displayRequestBlanked = 0;
			app->soundExclusiveDisplayRequest = 0;
			app->audioPortOpenFlags = 0;
			app->btrmAcquired = 0;
			return;
		}
	}
#if FW_DEBUG
	sceAppMgrDump();
	dumpBgmState();
	SCE_DBG_ASSERT_CHECK_FUNC(0, 337, "_deviceAddApp");
#else
	appmgrDeviceLockPanic();
#endif
}

void _deviceSetApp(int appId, const char *name, const char *proxyName)
{
	SceUID mutexOwner;
	AppmgrAudioApp *app;
	int index;

	if (appId <= 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 344, "_deviceSetApp");
#else
		appmgrDeviceLockPanic();
#endif
	}
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 345, "_deviceSetApp");
#else
		appmgrDeviceLockPanic();
#endif
	}
	for (index = 1; index != 20; ++index) {
		app = &g_appmgrAudioApps[index];
		if (app->appId > 0 && app->appId != appId &&
		    strncmp(app->appName, name, 32) == 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_CHECK_FUNC(0, 350, "_deviceSetApp");
#else
			appmgrDeviceLockPanic();
#endif
		}
	}
	for (index = 1; index != 20; ++index) {
		app = &g_appmgrAudioApps[index];
		if (app->appId == appId) {
			__builtin_memcpy(app->appName, name, sizeof(app->appName));
			__builtin_memcpy(app->proxyAppName, proxyName, sizeof(app->proxyAppName));
			return;
		}
	}
#if FW_DEBUG
	sceAppMgrDump();
	dumpBgmState();
	SCE_DBG_ASSERT_CHECK_FUNC(0, 362, "_deviceSetApp");
#else
	appmgrDeviceLockPanic();
#endif
}

void _deviceSetActiveAppId(int appId)
{
	SceUID mutexOwner = g_appmgrMainMutexOwner;

	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 368, "_deviceSetActiveAppId");
#else
		appmgrDeviceLockPanic();
#endif
	}
	g_appmgrActiveAppId = appId;
	g_appmgrBackRenderAppId = appId;
}

void _deviceDeactivate(void)
{
	SceUID mutexOwner = g_appmgrMainMutexOwner;

	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 375, "_deviceDeactivate");
#else
		appmgrDeviceLockPanic();
#endif
	}
	g_appmgrActiveAppId = -1;
}

void _deviceSetRunStatus(int appId, int isRunning)
{
	SceUID mutexOwner = g_appmgrMainMutexOwner;
	int index;

	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 469, "_deviceSetRunStatus");
#else
		appmgrDeviceLockPanic();
#endif
	}
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 382, "getAppByAppId");
#else
		appmgrDeviceLockPanic();
#endif
	}
	if (appId < 0)
		return;
	for (index = 0; index != 20; ++index) {
		if (g_appmgrAudioApps[index].appId == appId) {
			g_appmgrAudioApps[index].isRunning = isRunning;
			return;
		}
	}
}

void _deviceDeleteApp(int appId)
{
	SceUID mutexOwner = g_appmgrMainMutexOwner;
	AppmgrAudioApp *app = NULL;
	int index;

	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 482, "_deviceDeleteApp");
#else
		appmgrDeviceLockPanic();
#endif
	}
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 382, "getAppByAppId");
#else
		appmgrDeviceLockPanic();
#endif
	}
	if (appId >= 0) {
		for (index = 0; index != 20; ++index) {
			if (g_appmgrAudioApps[index].appId == appId) {
				app = &g_appmgrAudioApps[index];
				break;
			}
		}
	}
	if (app == NULL) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 484, "_deviceDeleteApp");
#else
		appmgrDeviceLockPanic();
#endif
	}
	app->appId = -1;
}

void _deviceGetBgmProxyAppName(int appId, char *name)
{
	SceUID mutexOwner = g_appmgrMainMutexOwner;
	int index;

	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 490, "_deviceGetBgmProxyAppName");
#else
		appmgrDeviceLockPanic();
#endif
	}
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 382, "getAppByAppId");
#else
		appmgrDeviceLockPanic();
#endif
	}
	if (appId >= 0) {
		for (index = 0; index != 20; ++index) {
			if (g_appmgrAudioApps[index].appId == appId) {
				__builtin_memcpy(name, g_appmgrAudioApps[index].proxyAppName, 32);
				return;
			}
		}
	}
	memset(name, 0, 32);
}

void getDeviceOwner(AppmgrDeviceOwners *owners)
{
	SceUID mutexOwner = g_appmgrMainMutexOwner;
	AppmgrAudioApp *shell;
	AppmgrAudioApp *active = NULL;
	AppmgrAudioApp *backRender = NULL;
	int appId, powerSaveMode, inputImposed, ctrlpMode;
	int index;

	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1734, "getDeviceOwner");
#else
		appmgrDeviceLockPanic();
#endif
	}
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 404, "getShell");
#else
		appmgrDeviceLockPanic();
#endif
	}
	mutexOwner = g_appmgrMainMutexOwner;
	shell = g_appmgrAudioApps[0].appId > 0 ? &g_appmgrAudioApps[0] : NULL;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 398, "getActiveApp");
#else
		appmgrDeviceLockPanic();
#endif
	}
	mutexOwner = g_appmgrMainMutexOwner;
	appId = g_appmgrActiveAppId;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 382, "getAppByAppId");
#else
		appmgrDeviceLockPanic();
#endif
	}
	if (appId >= 0) {
		for (index = 0; index != 20; ++index) {
			if (g_appmgrAudioApps[index].appId == appId) {
				active = &g_appmgrAudioApps[index];
				break;
			}
		}
	}
	mutexOwner = g_appmgrMainMutexOwner;
	appId = g_appmgrBackRenderAppId;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 382, "getAppByAppId");
#else
		appmgrDeviceLockPanic();
#endif
	}
	if (appId >= 0) {
		for (index = 0; index != 20; ++index) {
			if (g_appmgrAudioApps[index].appId == appId) {
				backRender = &g_appmgrAudioApps[index];
				break;
			}
		}
	}
	owners->shellPid = shell != NULL ? shell->processId : -1;
	powerSaveMode = g_appmgrPowerSaveMode;
	owners->backRenderPid = !powerSaveMode && backRender != NULL ?
		backRender->processId : -1;
	if (active != NULL) {
		SceUID processId = active->processId;

		owners->renderingPid = processId;
		owners->inputPid = processId;
	} else {
		owners->renderingPid = -1;
		owners->inputPid = -1;
		if (shell != NULL)
			owners->inputPid = shell->processId;
	}
	inputImposed = g_appmgrInputImposed != 0 || powerSaveMode != 0;
	ctrlpMode = g_appmgrCtrlpMode;
	owners->inputImposed = inputImposed;
	owners->btrmOwnerAppId = -1;
	owners->ctrlpMode = ctrlpMode;
	if (active != NULL && active->btrmAcquired)
		owners->btrmOwnerAppId = active->appId;
}

#if FW_DEBUG
#define APPMGR_CHECK_CHAT_OWNER(_line, _func) do { \
	if (mutexOwner != sceKernelGetThreadId()) { \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_CHECK_FUNC(0, _line, _func); \
	} \
} while (0)
#else
#define APPMGR_CHECK_CHAT_OWNER(_line, _func) do { \
	if (mutexOwner != sceKernelGetThreadId()) \
		appmgrDeviceLockPanic(); \
} while (0)
#endif

AppmgrAudioApp *getChatOwnerApp2(void)
{
	SceUID mutexOwner = g_appmgrMainMutexOwner;
	AppmgrAudioApp *chat = NULL;
	AppmgrAudioApp *active = NULL;
	int appId, processId, index;

	APPMGR_CHECK_CHAT_OWNER(752, "getChatOwnerApp2");
	mutexOwner = g_appmgrMainMutexOwner;
	if (g_appmgrPrioritizeSystemChat) {
		processId = g_appmgrSystemChatPid;
		APPMGR_CHECK_CHAT_OWNER(414, "getAppByProcessId");
		if (processId >= 0) {
			for (index = 0; index != 20; ++index) {
				AppmgrAudioApp *app = &g_appmgrAudioApps[index];

				if (app->appId > 0 && app->processId == processId) {
					chat = app;
					break;
				}
			}
		}
		if (chat != NULL && chat->isRunning && (chat->audioPortOpenFlags & 3))
			return chat;
		mutexOwner = g_appmgrMainMutexOwner;
		APPMGR_CHECK_CHAT_OWNER(398, "getActiveApp");
		mutexOwner = g_appmgrMainMutexOwner;
		appId = g_appmgrActiveAppId;
		APPMGR_CHECK_CHAT_OWNER(382, "getAppByAppId");
		if (appId >= 0) {
			for (index = 0; index != 20; ++index) {
				if (g_appmgrAudioApps[index].appId == appId) {
					active = &g_appmgrAudioApps[index];
					break;
				}
			}
		}
		if (active != NULL && active->isRunning && (active->audioPortOpenFlags & 3))
			return active;
		return NULL;
	}
	APPMGR_CHECK_CHAT_OWNER(398, "getActiveApp");
	mutexOwner = g_appmgrMainMutexOwner;
	appId = g_appmgrActiveAppId;
	APPMGR_CHECK_CHAT_OWNER(382, "getAppByAppId");
	if (appId >= 0) {
		for (index = 0; index != 20; ++index) {
			if (g_appmgrAudioApps[index].appId == appId) {
				active = &g_appmgrAudioApps[index];
				break;
			}
		}
	}
	if (active != NULL && active->isRunning && (active->audioPortOpenFlags & 3))
		return active;
	mutexOwner = g_appmgrMainMutexOwner;
	processId = g_appmgrSystemChatPid;
	APPMGR_CHECK_CHAT_OWNER(414, "getAppByProcessId");
	if (processId < 0)
		return NULL;
	for (index = 0; index != 20; ++index) {
		AppmgrAudioApp *app = &g_appmgrAudioApps[index];

		if (app->appId > 0 && app->processId == processId) {
			chat = app;
			break;
		}
	}
	if (chat == NULL)
		return NULL;
	if (chat->isRunning && (chat->audioPortOpenFlags & 3)) {
		mutexOwner = g_appmgrMainMutexOwner;
		APPMGR_CHECK_CHAT_OWNER(398, "getActiveApp");
		mutexOwner = g_appmgrMainMutexOwner;
		appId = g_appmgrActiveAppId;
		APPMGR_CHECK_CHAT_OWNER(382, "getAppByAppId");
		active = NULL;
		if (appId >= 0) {
			for (index = 0; index != 20; ++index) {
				if (g_appmgrAudioApps[index].appId == appId) {
					active = &g_appmgrAudioApps[index];
					break;
				}
			}
		}
		if (chat == active)
			return chat;
	}
	if (chat->isRunning && (chat->audioPortOpenFlags & 3) &&
	    g_appmgrVoiceOutPortOwner == chat->processId)
		return chat;
	return NULL;
}

#undef APPMGR_CHECK_CHAT_OWNER

#if FW_DEBUG
void updateAudioArbitration2(void)
{
	SceUID mutexOwner = g_appmgrMainMutexOwner;
	int result;

	if (mutexOwner == sceKernelGetThreadId()) {
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1105, "updateAudioArbitration2");
	}
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId()) {
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 147, "lockAudioUpdate");
	}
	result = sceKernelLockMutex(g_appmgrAudioUpdateMutex, 1, NULL);
	if (result != 0) {
		if ((SceUInt32)result + 0x7ffd7fd4U > 1) {
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 152, "lockAudioUpdate",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
		}
		return;
	}
	updateAudioArbitration2NoLock();
	result = sceKernelUnlockMutex(g_appmgrAudioUpdateMutex, 1);
	if (result != 0) {
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 161, "unlockAudioUpdate",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
	}
}
#endif

void appmgrOnAudioInClose(SceUID pid)
{
	AppmgrAudioApp *app = NULL;
	SceUID mutexOwner;
	int result, index;

	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 1173, "appmgrOnAudioInClose",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1173, "appmgrOnAudioInClose");
		mutexOwner = g_appmgrMainMutexOwner;
#else
		appmgrMainMutexPanic();
#endif
	} else {
		mutexOwner = sceKernelGetThreadId();
		g_appmgrMainMutexOwner = mutexOwner;
	}
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 414, "getAppByProcessId");
#else
		appmgrDeviceLockPanic();
#endif
	}
	if (pid >= 0) {
		for (index = 0; index != 20; ++index) {
			AppmgrAudioApp *candidate = &g_appmgrAudioApps[index];

			if (candidate->appId > 0 && candidate->processId == pid) {
				app = candidate;
				break;
			}
		}
	}
	if (app == NULL) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1177, "appmgrOnAudioInClose",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrMainMutexPanic();
#endif
		}
		return;
	}
	g_appmgrMainMutexOwner = -1;
	app->audioPortOpenFlags &= ~2U;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1182, "appmgrOnAudioInClose",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
#if FW_DEBUG
	updateAudioArbitration2();
#else
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	result = sceKernelLockMutex(g_appmgrAudioUpdateMutex, 1, NULL);
	if (result == 0) {
		updateAudioArbitration2NoLock();
		if (sceKernelUnlockMutex(g_appmgrAudioUpdateMutex, 1) != 0)
			appmgrDeviceLockPanic();
	} else if ((SceUInt32)result + 0x7ffd7fd4U > 1) {
		appmgrDeviceLockPanic();
	}
#endif
}

void appmgrOnAudioInOpen(SceUID pid)
{
	AppmgrAudioApp *app = NULL;
	SceUID mutexOwner;
	int result, index;

	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 1157, "appmgrOnAudioInOpen",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1157, "appmgrOnAudioInOpen");
		mutexOwner = g_appmgrMainMutexOwner;
#else
		appmgrMainMutexPanic();
#endif
	} else {
		mutexOwner = sceKernelGetThreadId();
		g_appmgrMainMutexOwner = mutexOwner;
	}
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 414, "getAppByProcessId");
#else
		appmgrDeviceLockPanic();
#endif
	}
	if (pid >= 0) {
		for (index = 0; index != 20; ++index) {
			AppmgrAudioApp *candidate = &g_appmgrAudioApps[index];

			if (candidate->appId > 0 && candidate->processId == pid) {
				app = candidate;
				break;
			}
		}
	}
	if (app == NULL) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1161, "appmgrOnAudioInOpen",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrMainMutexPanic();
#endif
		}
		return;
	}
	g_appmgrMainMutexOwner = -1;
	app->audioPortOpenFlags |= 2U;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1166, "appmgrOnAudioInOpen",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
#if FW_DEBUG
	updateAudioArbitration2();
#else
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	result = sceKernelLockMutex(g_appmgrAudioUpdateMutex, 1, NULL);
	if (result == 0) {
		updateAudioArbitration2NoLock();
		if (sceKernelUnlockMutex(g_appmgrAudioUpdateMutex, 1) != 0)
			appmgrDeviceLockPanic();
	} else if ((SceUInt32)result + 0x7ffd7fd4U > 1) {
		appmgrDeviceLockPanic();
	}
#endif
}

void appmgrOnAudioOutRelease(SceUID pid)
{
	AppmgrAudioApp *app = NULL;
	SceUID mutexOwner;
	int result, index;

	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 1141, "appmgrOnAudioOutRelease",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1141, "appmgrOnAudioOutRelease");
		mutexOwner = g_appmgrMainMutexOwner;
#else
		appmgrMainMutexPanic();
#endif
	} else {
		mutexOwner = sceKernelGetThreadId();
		g_appmgrMainMutexOwner = mutexOwner;
	}
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 414, "getAppByProcessId");
#else
		appmgrDeviceLockPanic();
#endif
	}
	if (pid >= 0) {
		for (index = 0; index != 20; ++index) {
			AppmgrAudioApp *candidate = &g_appmgrAudioApps[index];

			if (candidate->appId > 0 && candidate->processId == pid) {
				app = candidate;
				break;
			}
		}
	}
	if (app == NULL) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1145, "appmgrOnAudioOutRelease",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrMainMutexPanic();
#endif
		}
		return;
	}
	g_appmgrMainMutexOwner = -1;
	app->audioPortOpenFlags &= ~1U;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1150, "appmgrOnAudioOutRelease",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
#if FW_DEBUG
	updateAudioArbitration2();
#else
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	result = sceKernelLockMutex(g_appmgrAudioUpdateMutex, 1, NULL);
	if (result == 0) {
		updateAudioArbitration2NoLock();
		if (sceKernelUnlockMutex(g_appmgrAudioUpdateMutex, 1) != 0)
			appmgrDeviceLockPanic();
	} else if ((SceUInt32)result + 0x7ffd7fd4U > 1) {
		appmgrDeviceLockPanic();
	}
#endif
}

void appmgrOnAudioOutOpen(SceUID pid)
{
	AppmgrAudioApp *app = NULL;
	SceUID mutexOwner;
	int result, index;

	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 1125, "appmgrOnAudioOutOpen",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1125, "appmgrOnAudioOutOpen");
		mutexOwner = g_appmgrMainMutexOwner;
#else
		appmgrMainMutexPanic();
#endif
	} else {
		mutexOwner = sceKernelGetThreadId();
		g_appmgrMainMutexOwner = mutexOwner;
	}
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 414, "getAppByProcessId");
#else
		appmgrDeviceLockPanic();
#endif
	}
	if (pid >= 0) {
		for (index = 0; index != 20; ++index) {
			AppmgrAudioApp *candidate = &g_appmgrAudioApps[index];

			if (candidate->appId > 0 && candidate->processId == pid) {
				app = candidate;
				break;
			}
		}
	}
	if (app == NULL) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1129, "appmgrOnAudioOutOpen",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrMainMutexPanic();
#endif
		}
		return;
	}
	g_appmgrMainMutexOwner = -1;
	app->audioPortOpenFlags |= 1U;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1134, "appmgrOnAudioOutOpen",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
#if FW_DEBUG
	updateAudioArbitration2();
#else
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	result = sceKernelLockMutex(g_appmgrAudioUpdateMutex, 1, NULL);
	if (result == 0) {
		updateAudioArbitration2NoLock();
		if (sceKernelUnlockMutex(g_appmgrAudioUpdateMutex, 1) != 0)
			appmgrDeviceLockPanic();
	} else if ((SceUInt32)result + 0x7ffd7fd4U > 1) {
		appmgrDeviceLockPanic();
	}
#endif
}

#if FW_DEBUG
int _sceAppMgrAcquireSoundOutExclusive(int priority, SceUInt32 displayEnabled,
		SceUInt32 displayMode, int displayBlanked, int displayRequest)
{
	SceUInt32 syscallState = sceSyscallEnter();
	AppmgrAudioApp *app = NULL;
	SceUID pid, mutexOwner;
	int result, index;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201f;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
		SCE_DBG_PRINTF_FUNC(0, 1496, "_sceAppMgrAcquireSoundOutExclusive",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1496, "_sceAppMgrAcquireSoundOutExclusive");
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	pid = sceKernelGetProcessId();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId()) {
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 414, "getAppByProcessId");
	}
	if (pid >= 0) {
		for (index = 0; index != 20; ++index) {
			AppmgrAudioApp *candidate = &g_appmgrAudioApps[index];

			if (candidate->appId > 0 && candidate->processId == pid) {
				app = candidate;
				break;
			}
		}
	}
	if (app == NULL) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1502, "_sceAppMgrAcquireSoundOutExclusive",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
		}
		sceSyscallLeave(syscallState);
		return (int)0x80802012;
	}
	g_appmgrMainMutexOwner = -1;
	app->soundExclusivePriority = priority;
	app->displayRequestEnabled = displayEnabled;
	app->displayRequestMode = displayMode;
	app->displayRequestBlanked = displayBlanked;
	app->soundExclusiveDisplayRequest = displayRequest;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1515, "_sceAppMgrAcquireSoundOutExclusive",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
	}
	updateAudioArbitration2();
	sceSyscallLeave(syscallState);
	return 0;
}
#endif

int _sceAppMgrReleaseSoundOutExclusive(void)
{
	SceUInt32 syscallState = sceSyscallEnter();
	AppmgrAudioApp *app = NULL;
	SceUID pid, mutexOwner;
	int result, index;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201f;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 1535, "_sceAppMgrReleaseSoundOutExclusive",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1535, "_sceAppMgrReleaseSoundOutExclusive");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	pid = sceKernelGetProcessId();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 414, "getAppByProcessId");
#else
		appmgrDeviceLockPanic();
#endif
	}
	if (pid >= 0) {
		for (index = 0; index != 20; ++index) {
			AppmgrAudioApp *candidate = &g_appmgrAudioApps[index];

			if (candidate->appId > 0 && candidate->processId == pid) {
				app = candidate;
				break;
			}
		}
	}
	if (app == NULL) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1540, "_sceAppMgrReleaseSoundOutExclusive",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrMainMutexPanic();
#endif
		}
		sceSyscallLeave(syscallState);
		return (int)0x80802012;
	}
	if (app->soundExclusivePriority == -1) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1547, "_sceAppMgrReleaseSoundOutExclusive",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrMainMutexPanic();
#endif
		}
		sceSyscallLeave(syscallState);
		return (int)0x80802013;
	}
	g_appmgrMainMutexOwner = -1;
	app->soundExclusivePriority = -1;
	app->displayRequestEnabled = 0;
	app->displayRequestMode = 0;
	app->displayRequestBlanked = 0;
	app->soundExclusiveDisplayRequest = 0;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1560, "_sceAppMgrReleaseSoundOutExclusive",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
#if FW_DEBUG
	updateAudioArbitration2();
#else
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	result = sceKernelLockMutex(g_appmgrAudioUpdateMutex, 1, NULL);
	if (result == 0) {
		updateAudioArbitration2NoLock();
		if (sceKernelUnlockMutex(g_appmgrAudioUpdateMutex, 1) != 0)
			appmgrDeviceLockPanic();
	} else if ((SceUInt32)result + 0x7ffd7fd4U > 1) {
		appmgrDeviceLockPanic();
	}
#endif
	sceSyscallLeave(syscallState);
	return 0;
}

int sceAppMgrAcquireBgmPort(void)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID pid, mutexOwner;
	int result, index;

	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 1218, "sceAppMgrAcquireBgmPort",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1218, "sceAppMgrAcquireBgmPort");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	pid = sceKernelGetProcessId();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1190, "int_sceAppMgrGetBGMPort");
#else
		appmgrDeviceLockPanic();
#endif
	}
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 414, "getAppByProcessId");
#else
		appmgrDeviceLockPanic();
#endif
	}
	if (pid >= 0) {
		for (index = 0; index != 20; ++index) {
			AppmgrAudioApp *app = &g_appmgrAudioApps[index];

			if (app->appId > 0 && app->processId == pid) {
				app->bgmPriority = 256;
				break;
			}
		}
	}
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1223, "sceAppMgrAcquireBgmPort",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
#if FW_DEBUG
	updateAudioArbitration2();
#else
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	result = sceKernelLockMutex(g_appmgrAudioUpdateMutex, 1, NULL);
	if (result == 0) {
		updateAudioArbitration2NoLock();
		if (sceKernelUnlockMutex(g_appmgrAudioUpdateMutex, 1) != 0)
			appmgrDeviceLockPanic();
	} else if ((SceUInt32)result + 0x7ffd7fd4U > 1) {
		appmgrDeviceLockPanic();
	}
#endif
	sceSyscallLeave(syscallState);
	return 0;
}

int sceAppMgrAcquireBgmPortForMusicPlayer(void)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID pid, mutexOwner;
	int result, index;

	if (!sceSblACMgrIsSystemProgram(0) && !SceSblACMgrForDriver_6C5AB07F(0)) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201f;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 1245, "sceAppMgrAcquireBgmPortForMusicPlayer",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1245, "sceAppMgrAcquireBgmPortForMusicPlayer");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	pid = sceKernelGetProcessId();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1190, "int_sceAppMgrGetBGMPort");
#else
		appmgrDeviceLockPanic();
#endif
	}
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 414, "getAppByProcessId");
#else
		appmgrDeviceLockPanic();
#endif
	}
	if (pid >= 0) {
		for (index = 0; index != 20; ++index) {
			AppmgrAudioApp *app = &g_appmgrAudioApps[index];

			if (app->appId > 0 && app->processId == pid) {
				app->bgmPriority = 130;
				break;
			}
		}
	}
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1250, "sceAppMgrAcquireBgmPortForMusicPlayer",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
#if FW_DEBUG
	updateAudioArbitration2();
#else
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	result = sceKernelLockMutex(g_appmgrAudioUpdateMutex, 1, NULL);
	if (result == 0) {
		updateAudioArbitration2NoLock();
		if (sceKernelUnlockMutex(g_appmgrAudioUpdateMutex, 1) != 0)
			appmgrDeviceLockPanic();
	} else if ((SceUInt32)result + 0x7ffd7fd4U > 1) {
		appmgrDeviceLockPanic();
	}
#endif
	sceSyscallLeave(syscallState);
	return 0;
}

int sceAppMgrReleaseBgmPort(void)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID pid, mutexOwner;
	int result, index;

	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 1267, "sceAppMgrReleaseBgmPort",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1267, "sceAppMgrReleaseBgmPort");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	pid = sceKernelGetProcessId();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1190, "int_sceAppMgrGetBGMPort");
#else
		appmgrDeviceLockPanic();
#endif
	}
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 414, "getAppByProcessId");
#else
		appmgrDeviceLockPanic();
#endif
	}
	if (pid >= 0) {
		for (index = 0; index != 20; ++index) {
			AppmgrAudioApp *app = &g_appmgrAudioApps[index];

			if (app->appId > 0 && app->processId == pid) {
				app->bgmPriority = 0;
				break;
			}
		}
	}
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1272, "sceAppMgrReleaseBgmPort",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
#if FW_DEBUG
	updateAudioArbitration2();
#else
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	result = sceKernelLockMutex(g_appmgrAudioUpdateMutex, 1, NULL);
	if (result == 0) {
		updateAudioArbitration2NoLock();
		if (sceKernelUnlockMutex(g_appmgrAudioUpdateMutex, 1) != 0)
			appmgrDeviceLockPanic();
	} else if ((SceUInt32)result + 0x7ffd7fd4U > 1) {
		appmgrDeviceLockPanic();
	}
#endif
	sceSyscallLeave(syscallState);
	return 0;
}

int sceAppMgrAcquireBgmPortWithPriority(int priority)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID pid, mutexOwner;
	int result, index;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201f;
	}
	if (priority < 0) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201a;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 1303, "sceAppMgrAcquireBgmPortWithPriority",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1303, "sceAppMgrAcquireBgmPortWithPriority");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	pid = sceKernelGetProcessId();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1190, "int_sceAppMgrGetBGMPort");
#else
		appmgrDeviceLockPanic();
#endif
	}
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 414, "getAppByProcessId");
#else
		appmgrDeviceLockPanic();
#endif
	}
	if (pid >= 0) {
		for (index = 0; index != 20; ++index) {
			AppmgrAudioApp *app = &g_appmgrAudioApps[index];

			if (app->appId > 0 && app->processId == pid) {
				app->bgmPriority = priority;
				break;
			}
		}
	}
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1308, "sceAppMgrAcquireBgmPortWithPriority",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
#if FW_DEBUG
	updateAudioArbitration2();
#else
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	result = sceKernelLockMutex(g_appmgrAudioUpdateMutex, 1, NULL);
	if (result == 0) {
		updateAudioArbitration2NoLock();
		if (sceKernelUnlockMutex(g_appmgrAudioUpdateMutex, 1) != 0)
			appmgrDeviceLockPanic();
	} else if ((SceUInt32)result + 0x7ffd7fd4U > 1) {
		appmgrDeviceLockPanic();
	}
#endif
	sceSyscallLeave(syscallState);
	return 0;
}

int sceAppMgrSetBgmSubPriority(int enabled, int priority)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID pid, mutexOwner;
	int result, index;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201f;
	}
	if (priority < 0) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201a;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 1334, "sceAppMgrSetBgmSubPriority",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1334, "sceAppMgrSetBgmSubPriority");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	pid = sceKernelGetProcessId();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 414, "getAppByProcessId");
#else
		appmgrDeviceLockPanic();
#endif
	}
	if (pid >= 0) {
		for (index = 0; index != 20; ++index) {
			AppmgrAudioApp *app = &g_appmgrAudioApps[index];

			if (app->appId > 0 && app->processId == pid) {
				app->enableBgmSubPriority = enabled;
				app->bgmSubPriority = priority;
				break;
			}
		}
	}
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1344, "sceAppMgrSetBgmSubPriority",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
#if FW_DEBUG
	updateAudioArbitration2();
#else
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	result = sceKernelLockMutex(g_appmgrAudioUpdateMutex, 1, NULL);
	if (result == 0) {
		updateAudioArbitration2NoLock();
		if (sceKernelUnlockMutex(g_appmgrAudioUpdateMutex, 1) != 0)
			appmgrDeviceLockPanic();
	} else if ((SceUInt32)result + 0x7ffd7fd4U > 1) {
		appmgrDeviceLockPanic();
	}
#endif
	sceSyscallLeave(syscallState);
	return 0;
}

int sceAppMgrSetBgmSubPriorityForSystemChat(int enabled)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID pid, mutexOwner;
	int result, index;

	if (!sceSblACMgrIsSystemProgram(0) && !SceSblACMgrForDriver_6C5AB07F(0)) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201f;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 1366, "sceAppMgrSetBgmSubPriorityForSystemChat",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1366, "sceAppMgrSetBgmSubPriorityForSystemChat");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	pid = sceKernelGetProcessId();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 414, "getAppByProcessId");
#else
		appmgrDeviceLockPanic();
#endif
	}
	if (pid >= 0) {
		for (index = 0; index != 20; ++index) {
			AppmgrAudioApp *app = &g_appmgrAudioApps[index];

			if (app->appId > 0 && app->processId == pid) {
				app->enableBgmSubPriority = enabled;
				app->bgmSubPriority = 194;
				break;
			}
		}
	}
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1376, "sceAppMgrSetBgmSubPriorityForSystemChat",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
#if FW_DEBUG
	updateAudioArbitration2();
#else
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	result = sceKernelLockMutex(g_appmgrAudioUpdateMutex, 1, NULL);
	if (result == 0) {
		updateAudioArbitration2NoLock();
		if (sceKernelUnlockMutex(g_appmgrAudioUpdateMutex, 1) != 0)
			appmgrDeviceLockPanic();
	} else if ((SceUInt32)result + 0x7ffd7fd4U > 1) {
		appmgrDeviceLockPanic();
	}
#endif
	sceSyscallLeave(syscallState);
	return 0;
}

void _deviceSetName(int appId, const char *name)
{
	SceUID mutexOwner = g_appmgrMainMutexOwner;
	int index;

	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1466, "_deviceSetName");
#else
		appmgrDeviceLockPanic();
#endif
	}
	for (index = 0; index != 20; ++index) {
		AppmgrAudioApp *app = &g_appmgrAudioApps[index];

		if (app->appId != appId)
			continue;
		if (name != NULL)
			strncpy(app->appName, name, sizeof(app->appName));
		else
			__builtin_memset(app->appName, 0, sizeof(app->appName));
		return;
	}
}

int appmgrAcquireSoundOutExclusive(int priority, SceUInt32 displayEnabled,
		SceUInt32 displayMode)
{
#if FW_DEBUG
	return _sceAppMgrAcquireSoundOutExclusive(priority, displayEnabled,
		displayMode, 1, 1);
#else
	SceUInt32 syscallState = sceSyscallEnter();
	AppmgrAudioApp *app = NULL;
	SceUID pid, mutexOwner;
	int result, index;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201f;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0)
		appmgrMainMutexPanic();
	g_appmgrMainMutexOwner = sceKernelGetThreadId();
	pid = sceKernelGetProcessId();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	if (pid >= 0) {
		for (index = 0; index != 20; ++index) {
			AppmgrAudioApp *candidate = &g_appmgrAudioApps[index];

			if (candidate->appId > 0 && candidate->processId == pid) {
				app = candidate;
				break;
			}
		}
	}
	if (app == NULL) {
		g_appmgrMainMutexOwner = -1;
		if (sceKernelUnlockMutex(g_appmgrMainMutex, 1) != 0)
			appmgrMainMutexPanic();
		sceSyscallLeave(syscallState);
		return (int)0x80802012;
	}
	g_appmgrMainMutexOwner = -1;
	app->displayRequestMode = displayMode;
	app->soundExclusivePriority = priority;
	app->displayRequestEnabled = displayEnabled;
	app->displayRequestBlanked = 1;
	app->soundExclusiveDisplayRequest = 1;
	if (sceKernelUnlockMutex(g_appmgrMainMutex, 1) != 0)
		appmgrMainMutexPanic();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	result = sceKernelLockMutex(g_appmgrAudioUpdateMutex, 1, NULL);
	if (result == 0) {
		updateAudioArbitration2NoLock();
		if (sceKernelUnlockMutex(g_appmgrAudioUpdateMutex, 1) != 0)
			appmgrDeviceLockPanic();
	} else if ((SceUInt32)result + 0x7ffd7fd4U > 1) {
		appmgrDeviceLockPanic();
	}
	sceSyscallLeave(syscallState);
	return 0;
#endif
}

int appmgrReleaseSoundOutExclusive(void)
{
	return _sceAppMgrReleaseSoundOutExclusive();
}

int appmgrAcquireSoundOutExclusive2(int priority, SceUInt32 displayEnabled,
		SceUInt32 displayMode, int displayRequest)
{
#if FW_DEBUG
	return _sceAppMgrAcquireSoundOutExclusive(priority, displayEnabled,
		displayMode, 1, displayRequest);
#else
	SceUInt32 syscallState = sceSyscallEnter();
	AppmgrAudioApp *app = NULL;
	SceUID pid, mutexOwner;
	int result, index;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201f;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0)
		appmgrMainMutexPanic();
	g_appmgrMainMutexOwner = sceKernelGetThreadId();
	pid = sceKernelGetProcessId();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	if (pid >= 0) {
		for (index = 0; index != 20; ++index) {
			AppmgrAudioApp *candidate = &g_appmgrAudioApps[index];

			if (candidate->appId > 0 && candidate->processId == pid) {
				app = candidate;
				break;
			}
		}
	}
	if (app == NULL) {
		g_appmgrMainMutexOwner = -1;
		if (sceKernelUnlockMutex(g_appmgrMainMutex, 1) != 0)
			appmgrMainMutexPanic();
		sceSyscallLeave(syscallState);
		return (int)0x80802012;
	}
	g_appmgrMainMutexOwner = -1;
	app->displayRequestMode = displayMode;
	app->soundExclusivePriority = priority;
	app->displayRequestEnabled = displayEnabled;
	app->displayRequestBlanked = 1;
	app->soundExclusiveDisplayRequest = displayRequest;
	if (sceKernelUnlockMutex(g_appmgrMainMutex, 1) != 0)
		appmgrMainMutexPanic();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	result = sceKernelLockMutex(g_appmgrAudioUpdateMutex, 1, NULL);
	if (result == 0) {
		updateAudioArbitration2NoLock();
		if (sceKernelUnlockMutex(g_appmgrAudioUpdateMutex, 1) != 0)
			appmgrDeviceLockPanic();
	} else if ((SceUInt32)result + 0x7ffd7fd4U > 1) {
		appmgrDeviceLockPanic();
	}
	sceSyscallLeave(syscallState);
	return 0;
#endif
}

int appmgrReleaseSoundOutExclusive2(void)
{
	return _sceAppMgrReleaseSoundOutExclusive();
}

int appmgrAcquireSoundOutExclusive3(int priority, SceUInt32 displayEnabled,
		SceUInt32 displayMode, int displayBlanked, SceUInt32 unsupportedOptions)
{
	if (unsupportedOptions != 0)
		return (int)0x8080201a;
#if FW_DEBUG
	return _sceAppMgrAcquireSoundOutExclusive(priority, displayEnabled,
		displayMode, displayBlanked, 1);
#else
	SceUInt32 syscallState = sceSyscallEnter();
	AppmgrAudioApp *app = NULL;
	SceUID pid, mutexOwner;
	int result, index;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201f;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0)
		appmgrMainMutexPanic();
	g_appmgrMainMutexOwner = sceKernelGetThreadId();
	pid = sceKernelGetProcessId();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	if (pid >= 0) {
		for (index = 0; index != 20; ++index) {
			AppmgrAudioApp *candidate = &g_appmgrAudioApps[index];

			if (candidate->appId > 0 && candidate->processId == pid) {
				app = candidate;
				break;
			}
		}
	}
	if (app == NULL) {
		g_appmgrMainMutexOwner = -1;
		if (sceKernelUnlockMutex(g_appmgrMainMutex, 1) != 0)
			appmgrMainMutexPanic();
		sceSyscallLeave(syscallState);
		return (int)0x80802012;
	}
	g_appmgrMainMutexOwner = -1;
	app->soundExclusivePriority = priority;
	app->displayRequestEnabled = displayEnabled;
	app->displayRequestMode = displayMode;
	app->displayRequestBlanked = displayBlanked;
	app->soundExclusiveDisplayRequest = 1;
	if (sceKernelUnlockMutex(g_appmgrMainMutex, 1) != 0)
		appmgrMainMutexPanic();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	result = sceKernelLockMutex(g_appmgrAudioUpdateMutex, 1, NULL);
	if (result == 0) {
		updateAudioArbitration2NoLock();
		if (sceKernelUnlockMutex(g_appmgrAudioUpdateMutex, 1) != 0)
			appmgrDeviceLockPanic();
	} else if ((SceUInt32)result + 0x7ffd7fd4U > 1) {
		appmgrDeviceLockPanic();
	}
	sceSyscallLeave(syscallState);
	return 0;
#endif
}

int appmgrReleaseSoundOutExclusive3(void)
{
	return _sceAppMgrReleaseSoundOutExclusive();
}

int sceAppMgrDeclareSystemChatApp(void)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int result;
#if !FW_DEBUG
	SceUID mutexOwner;
#endif

	if (!sceSblACMgrIsSystemProgram(0) && !SceSblACMgrForDriver_6C5AB07F(0)) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201f;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 1608, "sceAppMgrDeclareSystemChatApp",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1608, "sceAppMgrDeclareSystemChatApp");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	g_appmgrSystemChatPid = sceKernelGetProcessId();
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1612, "sceAppMgrDeclareSystemChatApp",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
#if FW_DEBUG
	result = lockAudioUpdate();
#else
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	result = sceKernelLockMutex(g_appmgrAudioUpdateMutex, 1, NULL);
	if (result != 0 && (SceUInt32)result + 0x7ffd7fd4U > 1)
		appmgrDeviceLockPanic();
#endif
	if (result != 0)
		return result;
	setVolume(&g_appmgrSoundExclusivePid, &g_appmgrOtherPortVolume);
	result = sceKernelUnlockMutex(g_appmgrAudioUpdateMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 161, "unlockAudioUpdate",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrDeviceLockPanic();
#endif
	}
#if FW_DEBUG
	updateAudioArbitration2();
#else
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	result = sceKernelLockMutex(g_appmgrAudioUpdateMutex, 1, NULL);
	if (result == 0) {
		updateAudioArbitration2NoLock();
		if (sceKernelUnlockMutex(g_appmgrAudioUpdateMutex, 1) != 0)
			appmgrDeviceLockPanic();
	} else if ((SceUInt32)result + 0x7ffd7fd4U > 1) {
		appmgrDeviceLockPanic();
	}
#endif
	sceSyscallLeave(syscallState);
	return 0;
}

int sceAppMgrEnablePrioritizingSystemChat(int enabled)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int result;

	if (!sceSblACMgrIsSystemProgram(0) && !SceSblACMgrForDriver_6C5AB07F(0)) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201f;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 1655, "sceAppMgrEnablePrioritizingSystemChat",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1655, "sceAppMgrEnablePrioritizingSystemChat");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		sceKernelGetThreadId();
	}
#if FW_DEBUG
	g_appmgrMainMutexOwner = -1;
	g_appmgrPrioritizeSystemChat = enabled;
#else
	g_appmgrPrioritizeSystemChat = enabled;
	g_appmgrMainMutexOwner = -1;
#endif
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1658, "sceAppMgrEnablePrioritizingSystemChat",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
#if FW_DEBUG
	updateAudioArbitration2();
#else
	{
		SceUID mutexOwner = g_appmgrMainMutexOwner;

		if (mutexOwner == sceKernelGetThreadId())
			appmgrDeviceLockPanic();
		mutexOwner = g_appmgrMainMutexOwner;
		if (mutexOwner == sceKernelGetThreadId())
			appmgrDeviceLockPanic();
		result = sceKernelLockMutex(g_appmgrAudioUpdateMutex, 1, NULL);
		if (result == 0) {
			updateAudioArbitration2NoLock();
			if (sceKernelUnlockMutex(g_appmgrAudioUpdateMutex, 1) != 0)
				appmgrDeviceLockPanic();
		} else if ((SceUInt32)result + 0x7ffd7fd4U > 1) {
			appmgrDeviceLockPanic();
		}
	}
#endif
	sceSyscallLeave(syscallState);
	return 0;
}

int sceAppMgrEnableDuckingOnSystemChat(int enabled, SceUInt32 volume)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int result;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201f;
	}
	if (enabled) {
		if (volume > 256) {
			sceSyscallLeave(syscallState);
			return (int)0x8080201a;
		}
		result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
		if (result != 0) {
#if FW_DEBUG
			SCE_DBG_PRINTF_FUNC(0, 1678, "sceAppMgrEnableDuckingOnSystemChat",
				"Failed to lock ret=0x%08x\n", result);
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_CHECK_FUNC(0, 1678, "sceAppMgrEnableDuckingOnSystemChat");
#else
			appmgrMainMutexPanic();
#endif
		} else {
			sceKernelGetThreadId();
		}
#if FW_DEBUG
		g_appmgrMainMutexOwner = -1;
		g_appmgrSystemChatDuckingVolume = volume;
#else
		g_appmgrSystemChatDuckingVolume = volume;
		g_appmgrMainMutexOwner = -1;
#endif
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1680, "sceAppMgrEnableDuckingOnSystemChat",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrMainMutexPanic();
#endif
		}
	} else {
		result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
		if (result != 0) {
#if FW_DEBUG
			SCE_DBG_PRINTF_FUNC(0, 1682, "sceAppMgrEnableDuckingOnSystemChat",
				"Failed to lock ret=0x%08x\n", result);
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_CHECK_FUNC(0, 1682, "sceAppMgrEnableDuckingOnSystemChat");
#else
			appmgrMainMutexPanic();
#endif
		} else {
			sceKernelGetThreadId();
		}
#if FW_DEBUG
		g_appmgrMainMutexOwner = -1;
		g_appmgrSystemChatDuckingVolume = 256;
#else
		g_appmgrSystemChatDuckingVolume = 256;
		g_appmgrMainMutexOwner = -1;
#endif
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1684, "sceAppMgrEnableDuckingOnSystemChat",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrMainMutexPanic();
#endif
		}
	}
#if FW_DEBUG
	updateAudioArbitration2();
#else
	{
		SceUID mutexOwner = g_appmgrMainMutexOwner;

		if (mutexOwner == sceKernelGetThreadId())
			appmgrDeviceLockPanic();
		mutexOwner = g_appmgrMainMutexOwner;
		if (mutexOwner == sceKernelGetThreadId())
			appmgrDeviceLockPanic();
		result = sceKernelLockMutex(g_appmgrAudioUpdateMutex, 1, NULL);
		if (result == 0) {
			updateAudioArbitration2NoLock();
			if (sceKernelUnlockMutex(g_appmgrAudioUpdateMutex, 1) != 0)
				appmgrDeviceLockPanic();
		} else if ((SceUInt32)result + 0x7ffd7fd4U > 1) {
			appmgrDeviceLockPanic();
		}
	}
#endif
	sceSyscallLeave(syscallState);
	return 0;
}

int sceAppMgrSetBgmProxyApp(const char *appName)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID pid, mutexOwner;
	int result, index;
	AppmgrAudioApp *app;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201f;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 1704, "sceAppMgrSetBgmProxyApp",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1704, "sceAppMgrSetBgmProxyApp");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	pid = sceKernelGetProcessId();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 414, "getAppByProcessId");
#else
		appmgrDeviceLockPanic();
#endif
	}
	if (pid < 0)
		goto notFound;
	for (index = 0; index != 20; ++index) {
		app = &g_appmgrAudioApps[index];
		if (app->appId > 0 && app->processId == pid)
			break;
	}
	if (index == 20)
		goto notFound;
	if (appName != NULL)
		strncpy(app->proxyAppName, appName, 32);
	else
		memset(app->proxyAppName, 0, 32);
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1720, "sceAppMgrSetBgmProxyApp",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
#if FW_DEBUG
	updateAudioArbitration2();
#else
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	result = sceKernelLockMutex(g_appmgrAudioUpdateMutex, 1, NULL);
	if (result == 0) {
		updateAudioArbitration2NoLock();
		if (sceKernelUnlockMutex(g_appmgrAudioUpdateMutex, 1) != 0)
			appmgrDeviceLockPanic();
	} else if ((SceUInt32)result + 0x7ffd7fd4U > 1) {
		appmgrDeviceLockPanic();
	}
#endif
	sceSyscallLeave(syscallState);
	return 0;

notFound:
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1708, "sceAppMgrSetBgmProxyApp",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
	sceSyscallLeave(syscallState);
	return (int)0x80802012;
}

int _sceAppMgrDeviceDeclareShell(const char *appName, int appId, SceUID pid)
{
	AppmgrAudioApp *app;
	int result, index;

	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 234, "_sceAppMgrDeviceDeclareShell",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 234, "_sceAppMgrDeviceDeclareShell");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		sceKernelGetThreadId();
	}
	for (index = 0; index != 20; ++index) {
		app = &g_appmgrAudioApps[index];
		if (app->appId > 0 && app->processId == pid)
			app->appId = -1;
	}
	if (g_appmgrActiveAppId == appId)
		g_appmgrActiveAppId = -1;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 246, "_sceAppMgrDeviceDeclareShell",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
	_updateDeviceArbitration2();
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 249, "_sceAppMgrDeviceDeclareShell",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 249, "_sceAppMgrDeviceDeclareShell");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	app = &g_appmgrAudioApps[0];
	app->isRunning = 1;
	app->soundExclusivePriority = -1;
	g_appmgrShellPid = pid;
	app->appId = appId;
	app->processId = pid;
	__builtin_memset(app->appName, 0, sizeof(app->appName));
	app->bgmPriority = 0;
	app->enableBgmSubPriority = 0;
	app->bgmSubPriority = 0;
	app->displayRequestEnabled = 0;
	app->displayRequestMode = 0;
	app->displayRequestBlanked = 0;
	app->soundExclusiveDisplayRequest = 0;
	app->audioPortOpenFlags = 0;
	app->btrmAcquired = 0;
	__builtin_memset(app->proxyAppName, 0, sizeof(app->proxyAppName));
	SceDisplayForDriver_BC76296A(0);
	sceAudioOutSetAdoptMode(1);
	for (index = 0; index != 20; ++index) {
		app = &g_appmgrAudioApps[index];
		if (app->appId > 0 && app->processId == pid) {
			strncpy(app->appName, appName, 32);
			break;
		}
	}
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 271, "_sceAppMgrDeviceDeclareShell",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
	return _updateDeviceArbitration2();
}

int _sceAppMgrDeviceUndeclareShell(void)
{
	AppmgrAudioApp *app = &g_appmgrAudioApps[0];
	int result;

	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 279, "_sceAppMgrDeviceUndeclareShell",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 279, "_sceAppMgrDeviceUndeclareShell");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	app->appId = -1;
	app->processId = -1;
	app->soundExclusivePriority = -1;
	__builtin_memset(app->appName, 0, sizeof(app->appName));
	app->isRunning = 0;
	app->bgmPriority = 0;
	app->enableBgmSubPriority = 0;
	app->bgmSubPriority = 0;
	app->displayRequestEnabled = 0;
	app->displayRequestMode = 0;
	app->displayRequestBlanked = 0;
	app->soundExclusiveDisplayRequest = 0;
	app->audioPortOpenFlags = 0;
	app->btrmAcquired = 0;
	__builtin_memset(app->proxyAppName, 0, sizeof(app->proxyAppName));
	g_appmgrShellPid = -1;
	SceDisplayForDriver_BC76296A(0);
	sceAudioOutSetAdoptMode(1);
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 300, "_sceAppMgrDeviceUndeclareShell",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
	return _updateDeviceArbitration2();
}

int _changeDeviceArbitrationForImpose2(int imposed)
{
	int result;

	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 1890, "_changeDeviceArbitrationForImpose2",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1890, "_changeDeviceArbitrationForImpose2");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		sceKernelGetThreadId();
	}
#if FW_DEBUG
	g_appmgrMainMutexOwner = -1;
	g_appmgrInputImposed = imposed;
#else
	g_appmgrInputImposed = imposed;
	g_appmgrMainMutexOwner = -1;
#endif
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1892, "_changeDeviceArbitrationForImpose2",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
	return _updateDeviceArbitration2();
}

int _changeDeviceArbitrationForImpose3(SceUInt32 mode)
{
	int inputImposed, ctrlpMode, result;

	if (mode > 2)
		return (int)0x80802002;
	inputImposed = mode == 1;
	ctrlpMode = mode == 2;
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 1907, "_changeDeviceArbitrationForImpose3",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1907, "_changeDeviceArbitrationForImpose3");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		sceKernelGetThreadId();
	}
	g_appmgrMainMutexOwner = -1;
	g_appmgrInputImposed = inputImposed;
	g_appmgrCtrlpMode = ctrlpMode;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 1910, "_changeDeviceArbitrationForImpose3",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
	_updateDeviceArbitration2();
	return 0;
}

int sceAppMgrSetDisplayMergeConf(SceUInt32 setting, SceUInt32 alpha)
{
	SceUID mutexOwner = g_appmgrMainMutexOwner;
	int result, mergeResult;

	if (mutexOwner == sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1977, "sceAppMgrSetDisplayMergeConf");
#else
		appmgrDeviceLockPanic();
#endif
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 1978, "sceAppMgrSetDisplayMergeConf",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1978, "sceAppMgrSetDisplayMergeConf");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	if (g_appmgrDisplayMergeAlpha == alpha && g_appmgrDisplayMergeSetting == setting) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 1997, "sceAppMgrSetDisplayMergeConf",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrMainMutexPanic();
#endif
		}
		return 0;
	}
#if FW_DEBUG
	g_appmgrDisplayMergeAlpha = alpha;
	g_appmgrDisplayMergeSetting = setting;
#else
	g_appmgrDisplayMergeSetting = setting;
	g_appmgrDisplayMergeAlpha = alpha;
#endif
	mergeResult = SceDisplayForDriver_6B198052(0, setting, alpha);
	if (mergeResult < 0) {
		g_appmgrMainMutexOwner = -1;
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 2007, "sceAppMgrSetDisplayMergeConf",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrMainMutexPanic();
#endif
		}
		return mergeResult;
	}
	mergeResult = SceDisplayForDriver_6B198052(1, g_appmgrDisplayMergeSetting,
		g_appmgrDisplayMergeAlpha);
	g_appmgrMainMutexOwner = -1;
	if (mergeResult < 0) {
		result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
		if (result != 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_PRINTF_FUNC(0, 2015, "sceAppMgrSetDisplayMergeConf",
				"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
				result, result, 0, 0);
#else
			appmgrMainMutexPanic();
#endif
		}
		return mergeResult;
	}
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 2018, "sceAppMgrSetDisplayMergeConf",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
	return 0;
}

int sceAppMgrAcquireBtrm(void)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID pid, mutexOwner;
	int result, index;
	AppmgrAudioApp *app;

	if (!sceSblACMgrIsSystemProgram(0) && !SceSblACMgrForDriver_6C5AB07F(0)) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201f;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 2066, "sceAppMgrAcquireBtrm",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 2066, "sceAppMgrAcquireBtrm");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	pid = sceKernelGetProcessId();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 414, "getAppByProcessId");
#else
		appmgrDeviceLockPanic();
#endif
	}
	if (pid < 0)
		goto notFound;
	for (index = 0; index != 20; ++index) {
		app = &g_appmgrAudioApps[index];
		if (app->appId > 0 && app->processId == pid)
			break;
	}
	if (index == 20)
		goto notFound;
	g_appmgrMainMutexOwner = -1;
	app->btrmAcquired = 1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 2076, "sceAppMgrAcquireBtrm",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
	_updateDeviceArbitration2();
	sceSyscallLeave(syscallState);
	return 0;

notFound:
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 2070, "sceAppMgrAcquireBtrm",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
	sceSyscallLeave(syscallState);
	return (int)0x80802012;
}

int sceAppMgrReleaseBtrm(void)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID pid, mutexOwner;
	int result, index;
	AppmgrAudioApp *app;

	if (!sceSblACMgrIsSystemProgram(0) && !SceSblACMgrForDriver_6C5AB07F(0)) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201f;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 2093, "sceAppMgrReleaseBtrm",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 2093, "sceAppMgrReleaseBtrm");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	pid = sceKernelGetProcessId();
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 414, "getAppByProcessId");
#else
		appmgrDeviceLockPanic();
#endif
	}
	if (pid < 0)
		goto notFound;
	for (index = 0; index != 20; ++index) {
		app = &g_appmgrAudioApps[index];
		if (app->appId > 0 && app->processId == pid)
			break;
	}
	if (index == 20)
		goto notFound;
	g_appmgrMainMutexOwner = -1;
	app->btrmAcquired = 0;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 2103, "sceAppMgrReleaseBtrm",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
	_updateDeviceArbitration2();
	sceSyscallLeave(syscallState);
	return 0;

notFound:
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 2097, "sceAppMgrReleaseBtrm",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
	sceSyscallLeave(syscallState);
	return (int)0x80802012;
}

void _deviceRefreshAudioPort(SceUID pid)
{
	AppmgrAudioApp *shell;
	SceUID mutexOwner, owner;
	int result;

#if FW_DEBUG
	result = lockAudioUpdate();
#else
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner == sceKernelGetThreadId())
		appmgrDeviceLockPanic();
	result = sceKernelLockMutex(g_appmgrAudioUpdateMutex, 1, NULL);
	if (result != 0 && (SceUInt32)result + 0x7ffd7fd4U > 1)
		appmgrDeviceLockPanic();
#endif
	if (result != 0)
		return;
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 2213, "_deviceRefreshAudioPort",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 2213, "_deviceRefreshAudioPort");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	mutexOwner = g_appmgrMainMutexOwner;
	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 404, "getShell");
#else
		appmgrDeviceLockPanic();
#endif
	}
	g_appmgrMainMutexOwner = -1;
	shell = g_appmgrAudioApps[0].appId > 0 ? &g_appmgrAudioApps[0] : NULL;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 2218, "_deviceRefreshAudioPort",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}
	if (shell != NULL && g_appmgrShellMainPortEnabled != 0 &&
	    (pid < 0 || shell->processId == pid)) {
		owner = pid < 0 ? shell->processId : pid;
		if (owner != -1) {
			result = SceAudioForDriver_3440C1CA(owner, 0, 1, 100, 0);
			if (result < 0) {
#if FW_DEBUG
				sceAppMgrDump();
				dumpBgmState();
				SCE_DBG_ASSERT_CHECK_FUNC(0, 910, "enableOutPort");
#else
				appmgrEnableOutPortPanic();
#endif
			}
		}
	}
	owner = g_appmgrMainOutPortOwner;
	if (owner != -1 && (pid < 0 || owner == pid)) {
		result = SceAudioForDriver_3440C1CA(owner, 0, 1, 100, 0);
		if (result < 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_CHECK_FUNC(0, 910, "enableOutPort");
#else
			appmgrEnableOutPortPanic();
#endif
		}
	}
	owner = g_appmgrBgmOutPortOwner;
	if (owner != -1 && (pid < 0 || owner == pid)) {
		result = SceAudioForDriver_3440C1CA(owner, 1, 1, 100, 0);
		if (result < 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_CHECK_FUNC(0, 910, "enableOutPort");
#else
			appmgrEnableOutPortPanic();
#endif
		}
	}
	owner = g_appmgrVoiceOutPortOwner;
	if (owner != -1 && (pid < 0 || owner == pid)) {
		result = SceAudioForDriver_3440C1CA(owner, 2, 1, 100, 0);
		if (result < 0) {
#if FW_DEBUG
			sceAppMgrDump();
			dumpBgmState();
			SCE_DBG_ASSERT_CHECK_FUNC(0, 910, "enableOutPort");
#else
			appmgrEnableOutPortPanic();
#endif
		}
	}
	if (g_appmgrInputPortOwner != -1) {
		if (pid < 0)
			pid = g_appmgrVoiceOutPortOwner;
		if (pid == g_appmgrVoiceOutPortOwner && pid != -1) {
			result = SceAudioInForDriver_E4042F6F(pid, 0, 1, 0);
			if (result < 0) {
#if FW_DEBUG
				sceAppMgrDump();
				dumpBgmState();
				SCE_DBG_ASSERT_CHECK_FUNC(0, 930, "enableInPort");
#else
				SceDebugForDriver_391B5B74(&enableInPortPanicInfo, __builtin_return_address(0));
#endif
			}
		}
	}
	setVolume(&g_appmgrSoundExclusivePid, &g_appmgrOtherPortVolume);
	result = sceKernelUnlockMutex(g_appmgrAudioUpdateMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 161, "unlockAudioUpdate",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrDeviceLockPanic();
#endif
	}
}

void appmgrCaptureDeviceInfo(AppMgrDeviceInfo *info)
{
	SceUInt32 renderingCount = 0;
	SceUInt32 touchCount = 0;
	SceUInt32 count, index;
	SceUID pid;
	SceSize tailSize;

	info->soundExclusivePid = g_appmgrSoundExclusivePid;
	info->mainOutPortOwner = g_appmgrMainOutPortOwner;
	info->bgmOutPortOwner = g_appmgrBgmOutPortOwner;
	info->voiceOutPortOwner = g_appmgrVoiceOutPortOwner;
	info->inputPortOwner = g_appmgrInputPortOwner;
	info->backRenderPid = g_appmgrDeviceOwners.backRenderPid;
	info->inputOwnerPid = g_appmgrDeviceOwners.inputPid;
	info->bgmProxyAppId = g_appmgrDeviceOwners.btrmOwnerAppId;
	info->shellPid = g_appmgrShellPid;
	info->activeAppId = g_appmgrActiveAppId;
	if (g_appmgrRenderingPids[0] != -1)
		info->renderingPids[renderingCount++] = g_appmgrRenderingPids[0];
	if (g_appmgrRenderingPids[1] != -1)
		info->renderingPids[renderingCount++] = g_appmgrRenderingPids[1];
	if (g_appmgrRenderingPids[2] != -1)
		info->renderingPids[renderingCount++] = g_appmgrRenderingPids[2];
	if (g_appmgrRenderingPids[3] != -1)
		info->renderingPids[renderingCount++] = g_appmgrRenderingPids[3];
	if (g_appmgrRenderingPids[4] != -1)
		info->renderingPids[renderingCount++] = g_appmgrRenderingPids[4];
	if (g_appmgrRenderingPids[5] != -1)
		info->renderingPids[renderingCount++] = g_appmgrRenderingPids[5];
	if (g_appmgrRenderingPids[6] != -1)
		info->renderingPids[renderingCount++] = g_appmgrRenderingPids[6];
	if (g_appmgrRenderingPids[7] != -1)
		info->renderingPids[renderingCount++] = g_appmgrRenderingPids[7];
	memset(&info->renderingPids[renderingCount], -1,
		   sizeof(info->renderingPids) - sizeof(SceUID) * renderingCount);
	count = g_appmgrTouchProcessPriorities.count;
	if (count) {
		index = 0;
		do {
			pid = g_appmgrTouchProcessPriorities.processIds[index++];
			if (pid != -1) {
				info->touchPids[touchCount++] = pid;
				if (touchCount > 15)
					goto owners;
			}
		} while (index != count);
	}
	tailSize = sizeof(info->touchPids) - sizeof(SceUID) * touchCount;
	if (touchCount + 1 > 16)
		tailSize = sizeof(SceUID);
	memset(&info->touchPids[touchCount], -1, tailSize);
owners:
	info->displayOwner0 = g_appmgrDisplayOwner[0];
	info->displayOwner1 = g_appmgrDisplayOwner[1];
	info->systemChatPid = g_appmgrSystemChatPid;
	info->backRenderAppId = g_appmgrBackRenderAppId;
}

void _deviceSetRecommendedScreenOrientation(int orientation)
{
	SceUID mutexOwner = g_appmgrMainMutexOwner;

	if (mutexOwner != sceKernelGetThreadId()) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 2313, "_deviceSetRecommendedScreenOrientation");
#else
		appmgrDeviceLockPanic();
#endif
	}
	g_appmgrRecommendedScreenOrientation = orientation;
}

