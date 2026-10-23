/* Audio and device arbitration. */

#include <sysmem/debug.h>
#include <intrmgr/syscall.h>
#include <threadmgr/threadmgr.h>
#include <touch/touch.h>
#include <appmgr/appmgr_debugger.h>
#include <acmgr/acmgr.h>
#include <sysmem/sysclib.h>
#include "app_state_internal.h"
#include "appmgr_device.h"
#include "appmgr_shell_event.h"
#include <display/display.h>
#include <audio/audio.h>
#include <audioin/audioin.h>
#include <av_config/av_config.h>
#include <appmgr/appmgr.h>

extern SceUID g_appmgrMainMutex;
extern SceUID g_appmgrMainMutexOwner;
extern int g_appmgrPowerSaveMode;
extern int dumpBgmState(void);
extern int _sceAppMgrReleaseSoundOutExclusive(void);
extern int _updateDeviceArbitration2(void);

SceUInt32 g_appmgrDisplayMergeAlpha = 256;
SceUInt32 g_appmgrDisplayMergeSetting = 128;
AppmgrDevice g_appmgrDevice;

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

static const SceKernelDebugInfo disableInPortPanicInfo = {
	.fileHash = 0xbbec1541,
	.lineHash = 0x41ac3252,
	.funcHash = 0x09807dff,
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

static const SceKernelDebugInfo disableOutPortPanicInfo = {
	.fileHash = 0xbbec1541,
	.lineHash = 0x2883fc50,
	.funcHash = 0x04cee74f,
};

__attribute__((noinline)) _Noreturn void appmgrDeviceLockPanic(void)
{
	SceDebugForDriver_391B5B74(&deviceLockPanicInfo, __builtin_return_address(0));
}

__attribute__((noinline)) _Noreturn void appmgrMainMutexPanic(void)
{
	SceDebugForDriver_391B5B74(&mainMutexPanicInfo, __builtin_return_address(0));
}
#endif

#if FW_DEBUG
#define APPMGR_DEVICE_FAIL(_line, _func) do { \
	sceAppMgrDump(); \
	dumpBgmState(); \
	SCE_DBG_ASSERT_CHECK_FUNC(0, _line, _func); \
} while (0)
#else
#define APPMGR_DEVICE_FAIL(_line, _func) appmgrDeviceLockPanic()
#endif

/* the device state is read and written with the main mutex held */
#define APPMGR_DEVICE_ASSERT_LOCKED(_line, _func) do { \
	SceUID _owner = g_appmgrMainMutexOwner; \
	if (_owner != sceKernelGetThreadId()) \
		APPMGR_DEVICE_FAIL(_line, _func); \
} while (0)

/* audio ports are switched with the main mutex released */
#define APPMGR_DEVICE_ASSERT_UNLOCKED(_line, _func) do { \
	SceUID _owner = g_appmgrMainMutexOwner; \
	if (_owner == sceKernelGetThreadId()) \
		APPMGR_DEVICE_FAIL(_line, _func); \
} while (0)

static inline __attribute__((always_inline)) AppmgrAudioApp *getAppByProcessId(SceUID pid)
{
	int index;

	APPMGR_DEVICE_ASSERT_LOCKED(414, "getAppByProcessId");
	if (pid < 0)
		return NULL;
	for (index = 0; index < 20; ++index) {
		if (g_appmgrDevice.apps[index].appId > 0 &&
		    g_appmgrDevice.apps[index].processId == pid)
			return &g_appmgrDevice.apps[index];
	}
	return NULL;
}

static inline __attribute__((always_inline)) AppmgrAudioApp *getAppByAppId(int appId)
{
	int index;

	APPMGR_DEVICE_ASSERT_LOCKED(382, "getAppByAppId");
	if (appId < 0)
		return NULL;
	for (index = 0; index < 20; ++index) {
		if (g_appmgrDevice.apps[index].appId == appId)
			return &g_appmgrDevice.apps[index];
	}
	return NULL;
}

static inline __attribute__((always_inline)) AppmgrAudioApp *getActiveApp(void)
{
	APPMGR_DEVICE_ASSERT_LOCKED(398, "getActiveApp");
	return getAppByAppId(g_appmgrDevice.activeAppId);
}

/* the shell is declared first, in the first entry */
static inline __attribute__((always_inline)) AppmgrAudioApp *getShell(void)
{
	APPMGR_DEVICE_ASSERT_LOCKED(404, "getShell");
	return g_appmgrDevice.apps[0].appId > 0 ? &g_appmgrDevice.apps[0] : NULL;
}

/* follows the BGM proxy names, at most 20 hops; a cycle leaves the BGM with the application */
static inline AppmgrAudioApp *appmgrResolveBgmProxy(AppmgrAudioApp *app)
{
	AppmgrAudioApp *current = app;
	int remaining = 20;
	int index;

	if (app == NULL)
		return app;
	while (current->proxyAppName[0] != '\0') {
		for (index = 0; index != 20; ++index) {
			if (g_appmgrDevice.apps[index].appId > 0 &&
			    strncmp(g_appmgrDevice.apps[index].appName,
				    (const char *)current->proxyAppName, 32) == 0)
				break;
		}
		if (index == 20)
			return current;
		current = &g_appmgrDevice.apps[index];
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
		g_appmgrDevice.bgmState.soundExclusivePid);
	SCE_DBG_PRINTF_FUNC(0, 505, "dumpBgmState", "soundExclusivePriority=0x%08x\n",
		g_appmgrDevice.bgmState.soundExclusivePriority);
	SCE_DBG_PRINTF_FUNC(0, 506, "dumpBgmState", "shellMainPortEnabled=0x%08x\n",
		g_appmgrDevice.bgmState.shellMainPortEnabled);
	SCE_DBG_PRINTF_FUNC(0, 507, "dumpBgmState", "mainOutPortOwner=0x%08x\n",
		g_appmgrDevice.bgmState.mainOutPortOwner);
	SCE_DBG_PRINTF_FUNC(0, 508, "dumpBgmState", "bgmOutPortOwner=0x%08x\n",
		g_appmgrDevice.bgmState.bgmOutPortOwner);
	SCE_DBG_PRINTF_FUNC(0, 509, "dumpBgmState", "bgmOutPortOwnerPriority=%d\n",
		g_appmgrDevice.bgmState.bgmOutPortOwnerPriority);
	SCE_DBG_PRINTF_FUNC(0, 510, "dumpBgmState", "bgmOutPortOwnerSubPriority=%d\n",
		g_appmgrDevice.bgmState.bgmOutPortOwnerSubPriority);
	SCE_DBG_PRINTF_FUNC(0, 511, "dumpBgmState", "voiceOutPortOwner=0x%08x\n",
		g_appmgrDevice.bgmState.voiceOutPortOwner);
	SCE_DBG_PRINTF_FUNC(0, 512, "dumpBgmState", "inputPortOwner=0x%08x\n",
		g_appmgrDevice.bgmState.inputPortOwner);
	SCE_DBG_PRINTF_FUNC(0, 514, "dumpBgmState", "displayOwner0=0x%08x\n",
		g_appmgrDevice.displayOwner[0]);
	SCE_DBG_PRINTF_FUNC(0, 515, "dumpBgmState", "displayOwner1=0x%08x\n",
		g_appmgrDevice.displayOwner[1]);
	SCE_DBG_PRINTF_FUNC(0, 516, "dumpBgmState", "BTRM owner=0x%08x\n",
		g_appmgrDevice.appMgrState.btrmOwner);
	for (index = 0; index != 20; ++index) {
		AppmgrAudioApp *app = &g_appmgrDevice.apps[index];

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
	result = sceKernelLockMutex(g_appmgrDevice.audioUpdateMutex, 1, NULL);
	if (result != 0 && (SceUInt32)result + 0x7ffd7fd4U > 1) {
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 152, "lockAudioUpdate",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
	}
	return result;
}
#else
/* the release build inlines it at every call */
static inline __attribute__((always_inline)) int lockAudioUpdate(void)
{
	int result;

	APPMGR_DEVICE_ASSERT_UNLOCKED(147, "lockAudioUpdate");
	result = sceKernelLockMutex(g_appmgrDevice.audioUpdateMutex, 1, NULL);
	if (result != 0 && (SceUInt32)result + 0x7ffd7fd4U > 1)
		appmgrDeviceLockPanic();
	return result;
}
#endif

#if !FW_DEBUG
__attribute__((noinline)) _Noreturn void appmgrDisableOutPortPanic(void)
{
	SceDebugForDriver_391B5B74(&disableOutPortPanicInfo, __builtin_return_address(0));
}

__attribute__((noinline)) _Noreturn void appmgrEnableOutPortPanic(void)
{
	SceDebugForDriver_391B5B74(&enableOutPortPanicInfo, __builtin_return_address(0));
}
#endif

void setVolume(const SceUID *exclusivePid, const SceUInt32 *otherVolume)
{
	int index;

	for (index = 0; index != 20; ++index) {
		AppmgrAudioApp *app = &g_appmgrDevice.apps[index];
		SceUID processId;
		SceUInt32 volume;
		int result;

		if (app->appId <= 0)
			continue;
		processId = app->processId;
		if (processId <= 0)
			continue;
		if (*exclusivePid == -1)
			volume = processId == g_appmgrDevice.systemChatPid ? 256 : *otherVolume;
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
		app = &g_appmgrDevice.apps[index];
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
		app = &g_appmgrDevice.apps[index];
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
		app = &g_appmgrDevice.apps[index];
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
		app = &g_appmgrDevice.apps[index];
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
	g_appmgrDevice.activeAppId = appId;
	g_appmgrDevice.backRenderAppId = appId;
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
	g_appmgrDevice.activeAppId = -1;
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
		if (g_appmgrDevice.apps[index].appId == appId) {
			g_appmgrDevice.apps[index].isRunning = isRunning;
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
			if (g_appmgrDevice.apps[index].appId == appId) {
				app = &g_appmgrDevice.apps[index];
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
			if (g_appmgrDevice.apps[index].appId == appId) {
				__builtin_memcpy(name, g_appmgrDevice.apps[index].proxyAppName, 32);
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
	shell = g_appmgrDevice.apps[0].appId > 0 ? &g_appmgrDevice.apps[0] : NULL;
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
	appId = g_appmgrDevice.activeAppId;
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
			if (g_appmgrDevice.apps[index].appId == appId) {
				active = &g_appmgrDevice.apps[index];
				break;
			}
		}
	}
	mutexOwner = g_appmgrMainMutexOwner;
	appId = g_appmgrDevice.backRenderAppId;
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
			if (g_appmgrDevice.apps[index].appId == appId) {
				backRender = &g_appmgrDevice.apps[index];
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
	inputImposed = g_appmgrDevice.appMgrState.inputImposed != 0 || powerSaveMode != 0;
	ctrlpMode = g_appmgrDevice.appMgrState.ctrlpMode;
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
	if (g_appmgrDevice.appMgrState.prioritizeSystemChat) {
		processId = g_appmgrDevice.systemChatPid;
		APPMGR_CHECK_CHAT_OWNER(414, "getAppByProcessId");
		if (processId >= 0) {
			for (index = 0; index != 20; ++index) {
				AppmgrAudioApp *app = &g_appmgrDevice.apps[index];

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
		appId = g_appmgrDevice.activeAppId;
		APPMGR_CHECK_CHAT_OWNER(382, "getAppByAppId");
		if (appId >= 0) {
			for (index = 0; index != 20; ++index) {
				if (g_appmgrDevice.apps[index].appId == appId) {
					active = &g_appmgrDevice.apps[index];
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
	appId = g_appmgrDevice.activeAppId;
	APPMGR_CHECK_CHAT_OWNER(382, "getAppByAppId");
	if (appId >= 0) {
		for (index = 0; index != 20; ++index) {
			if (g_appmgrDevice.apps[index].appId == appId) {
				active = &g_appmgrDevice.apps[index];
				break;
			}
		}
	}
	if (active != NULL && active->isRunning && (active->audioPortOpenFlags & 3))
		return active;
	mutexOwner = g_appmgrMainMutexOwner;
	processId = g_appmgrDevice.systemChatPid;
	APPMGR_CHECK_CHAT_OWNER(414, "getAppByProcessId");
	if (processId < 0)
		return NULL;
	for (index = 0; index != 20; ++index) {
		AppmgrAudioApp *app = &g_appmgrDevice.apps[index];

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
		appId = g_appmgrDevice.activeAppId;
		APPMGR_CHECK_CHAT_OWNER(382, "getAppByAppId");
		active = NULL;
		if (appId >= 0) {
			for (index = 0; index != 20; ++index) {
				if (g_appmgrDevice.apps[index].appId == appId) {
					active = &g_appmgrDevice.apps[index];
					break;
				}
			}
		}
		if (chat == active)
			return chat;
	}
	if (chat->isRunning && (chat->audioPortOpenFlags & 3) &&
	    g_appmgrDevice.bgmState.voiceOutPortOwner == chat->processId)
		return chat;
	return NULL;
}

#undef APPMGR_CHECK_CHAT_OWNER

#if FW_DEBUG
#define APPMGR_PORT_FAIL(_line, _func, _panic) APPMGR_DEVICE_FAIL(_line, _func)
#else
#define APPMGR_PORT_FAIL(_line, _func, _panic) _panic
#endif

static inline __attribute__((always_inline)) void enableOutPort(SceUID pid, int type)
{
	if (pid != -1 && SceAudioForDriver_3440C1CA(pid, type, 1, 100, 0) < 0)
		APPMGR_PORT_FAIL(910, "enableOutPort", appmgrEnableOutPortPanic());
}

/* an exclusive owner takes the port at once; otherwise it fades out */
static inline __attribute__((always_inline)) void disableOutPort(SceUID pid, int type, int wait)
{
	if (pid != -1 && SceAudioForDriver_3440C1CA(pid, type, 0, wait ? 0 : 400, wait) < 0)
		APPMGR_PORT_FAIL(921, "disableOutPort", appmgrDisableOutPortPanic());
}

static inline __attribute__((always_inline)) void enableInPort(SceUID pid)
{
	if (pid != -1 && SceAudioInForDriver_E4042F6F(pid, 0, 1, 0) < 0)
		APPMGR_PORT_FAIL(930, "enableInPort",
			SceDebugForDriver_391B5B74(&enableInPortPanicInfo, __builtin_return_address(0)));
}

static inline __attribute__((always_inline)) void disableInPort(SceUID pid)
{
	if (pid != -1 && SceAudioInForDriver_E4042F6F(pid, 0, 0, 0) < 0)
		APPMGR_PORT_FAIL(939, "disableInPort",
			SceDebugForDriver_391B5B74(&disableInPortPanicInfo, __builtin_return_address(0)));
}

/*
 * The BGM port goes to the running application of the highest BGM priority,
 * else to the active application, else to the shell, each through its BGM
 * proxy. In power save mode the suspended active application keeps a BGM
 * priority it has.
 */
static inline __attribute__((always_inline)) AppmgrAudioApp *getHighestBgmPriorityApp(
		int powerSaveMode, int *priority, int *subPriority)
{
	AppmgrAudioApp *active, *shell, *proxy;
	AppmgrAudioApp *highest = NULL, *owner = NULL;
	int highestPriority = -1, highestSubPriority = 0, subPriorityEnabled = 0;
	int index;

	APPMGR_DEVICE_ASSERT_LOCKED(583, "getHighestBgmPriorityApp");
	active = getActiveApp();
	shell = getShell();
	for (index = 0; index != 20; ++index) {
		AppmgrAudioApp *app = &g_appmgrDevice.apps[index];

		if (!app->isRunning &&
		    !(powerSaveMode && app == active && app->bgmPriority > 0))
			continue;
		if (app->bgmPriority > 0 && app->bgmPriority > highestPriority) {
			highestPriority = app->bgmPriority;
			highest = app;
		}
		if (app->enableBgmSubPriority) {
			subPriorityEnabled = 1;
			if (highestSubPriority < app->bgmSubPriority)
				highestSubPriority = app->bgmSubPriority;
		}
	}
	if (highest != NULL) {
		if (!subPriorityEnabled || highest->bgmPriority > highestSubPriority) {
			owner = appmgrResolveBgmProxy(highest);
			*priority = owner->bgmPriority;
		} else {
			if (active != NULL) {
				if (active->isRunning || !powerSaveMode)
					owner = appmgrResolveBgmProxy(active);
			} else if (shell != NULL) {
				proxy = appmgrResolveBgmProxy(shell);
				owner = proxy != NULL ? proxy : shell;
			}
			*priority = highestSubPriority;
		}
	} else {
		if (active != NULL) {
			if (active->isRunning || (subPriorityEnabled && !powerSaveMode)) {
				proxy = appmgrResolveBgmProxy(active);
				owner = proxy != NULL ? proxy : active;
			}
		} else if (shell != NULL) {
			proxy = appmgrResolveBgmProxy(shell);
			owner = proxy != NULL ? proxy : shell;
		}
		*priority = highestSubPriority;
	}
	*subPriority = highestSubPriority;
	return owner;
}

/*
 * The audio ports' owners: a running sound-exclusive application takes them
 * all (one with a display request first, then by priority); otherwise the
 * active application has the main port, the chat owner the voice and input
 * ports, and getHighestBgmPriorityApp's the BGM port.
 */
static inline __attribute__((always_inline)) void getCurrentPortOwener(AppmgrBgmState *owner,
		int powerSaveMode)
{
	AppmgrBgmState state;
	AppmgrAudioApp *exclusive = NULL;
	AppmgrAudioApp *shell, *active, *chat, *bgmOwner;
	int priority, subPriority;
	int index;

	APPMGR_DEVICE_ASSERT_LOCKED(783, "getCurrentPortOwener");
	if (g_appmgrDevice.audioPortsDisabled) {
		state.soundExclusivePid = -1;
		state.soundExclusivePriority = -1;
		state.displayRequestEnabled = 0;
		state.displayRequestMode = 0;
		state.displayRequestBlanked = 0;
		state.soundExclusiveDisplayRequest = 0;
		state.shellMainPortEnabled = 0;
		state.mainOutPortOwner = -1;
		state.bgmOutPortOwner = -1;
		state.bgmOutPortOwnerPriority = 0xFFFF;
		state.bgmOutPortOwnerSubPriority = 0xFFFF;
		state.voiceOutPortOwner = -1;
		state.voiceOutPortOwnerAppId = -1;
		state.inputPortOwner = -1;
		state.otherPortVolume = 256;
		state.systemChatDucking = 0;
		*owner = state;
		return;
	}
	for (index = 0; index != 20; ++index) {
		AppmgrAudioApp *app = &g_appmgrDevice.apps[index];

		if (!app->isRunning || app->soundExclusivePriority < 0)
			continue;
		if (exclusive == NULL)
			exclusive = app;
		else if (!exclusive->soundExclusiveDisplayRequest) {
			if (app->soundExclusiveDisplayRequest ||
			    app->soundExclusivePriority > exclusive->soundExclusivePriority)
				exclusive = app;
		} else if (app->soundExclusiveDisplayRequest &&
			   app->soundExclusivePriority > exclusive->soundExclusivePriority) {
			exclusive = app;
		}
	}
	shell = getShell();
	if (exclusive != NULL) {
		state.soundExclusivePid = exclusive->processId;
		state.soundExclusivePriority = exclusive->soundExclusivePriority;
		state.displayRequestEnabled = exclusive->displayRequestEnabled;
		state.displayRequestMode = exclusive->displayRequestMode;
		state.displayRequestBlanked = exclusive->displayRequestBlanked;
		state.soundExclusiveDisplayRequest = exclusive->soundExclusiveDisplayRequest;
		state.shellMainPortEnabled = exclusive->processId == g_appmgrDevice.shellPid;
		state.mainOutPortOwner = exclusive->processId == g_appmgrDevice.shellPid ?
			-1 : exclusive->processId;
		state.bgmOutPortOwner = exclusive->processId;
		state.bgmOutPortOwnerPriority = exclusive->bgmPriority;
		state.bgmOutPortOwnerSubPriority = 0;
		chat = getChatOwnerApp2();
		state.voiceOutPortOwner = chat != NULL ? chat->processId : -1;
		state.voiceOutPortOwnerAppId = chat != NULL ? chat->appId : -1;
		state.inputPortOwner = state.voiceOutPortOwner;
		state.otherPortVolume = 256;
		state.systemChatDucking = 0;
		*owner = state;
		return;
	}
	state.soundExclusivePid = -1;
	state.soundExclusivePriority = -1;
	state.displayRequestEnabled = 0;
	state.displayRequestMode = 0;
	state.displayRequestBlanked = 0;
	state.soundExclusiveDisplayRequest = 0;
	state.shellMainPortEnabled = shell != NULL;
	active = getActiveApp();
	chat = getChatOwnerApp2();
	state.mainOutPortOwner = active != NULL ? active->processId : -1;
	if (chat != NULL) {
		state.voiceOutPortOwner = chat->processId;
		state.voiceOutPortOwnerAppId = chat->appId;
		if (chat->processId == g_appmgrDevice.systemChatPid) {
			state.systemChatDucking = 1;
			state.otherPortVolume = g_appmgrDevice.systemChatDuckingVolume;
		} else {
			state.otherPortVolume = 256;
			state.systemChatDucking = 0;
		}
	} else {
		state.voiceOutPortOwner = -1;
		state.voiceOutPortOwnerAppId = -1;
		state.otherPortVolume = 256;
		state.systemChatDucking = 0;
	}
	state.inputPortOwner = state.voiceOutPortOwner;
	bgmOwner = getHighestBgmPriorityApp(powerSaveMode, &priority, &subPriority);
	if (!g_appmgrDevice.bgmEnabled) {
		state.bgmOutPortOwner = -1;
		state.bgmOutPortOwnerPriority = 0xFFFF;
		state.bgmOutPortOwnerSubPriority = 0xFFFF;
	} else if (bgmOwner == NULL) {
		state.bgmOutPortOwner = -1;
		state.bgmOutPortOwnerPriority = 0xFFFF;
		state.bgmOutPortOwnerSubPriority = subPriority;
	} else {
		state.bgmOutPortOwner = bgmOwner->processId;
		state.bgmOutPortOwnerPriority = priority;
		state.bgmOutPortOwnerSubPriority = subPriority;
	}
	*owner = state;
}

/*
 * Works out the ports' owners under the main mutex, then, with it released,
 * takes the ports from their old owners and gives them to the new ones.
 */
void updateAudioArbitration2NoLock(void)
{
	AppmgrBgmState *current = &g_appmgrDevice.bgmState;
	AppmgrBgmState next;
	AppmgrAudioApp *shell;
	int wait, result;

	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 980, "updateAudioArbitration2NoLock",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 980, "updateAudioArbitration2NoLock");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	getCurrentPortOwener(&next, g_appmgrPowerSaveMode);
	shell = getShell();
	g_appmgrDevice.appMgrState.systemChatDucking = next.systemChatDucking;
	g_appmgrMainMutexOwner = -1;
	result = sceKernelUnlockMutex(g_appmgrMainMutex, 1);
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 986, "updateAudioArbitration2NoLock",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrMainMutexPanic();
#endif
	}

	wait = next.soundExclusivePid != -1;
	if (current->shellMainPortEnabled != next.shellMainPortEnabled && shell != NULL &&
	    current->shellMainPortEnabled)
		disableOutPort(shell->processId, 0, wait);
	if (current->mainOutPortOwner != next.mainOutPortOwner)
		disableOutPort(current->mainOutPortOwner, 0, wait);
	if (current->bgmOutPortOwner != next.bgmOutPortOwner)
		disableOutPort(current->bgmOutPortOwner, 1, wait);
	if (current->voiceOutPortOwner != next.voiceOutPortOwner)
		disableOutPort(current->voiceOutPortOwner, 2, wait);
	if (current->inputPortOwner != next.inputPortOwner)
		disableInPort(current->voiceOutPortOwner);

	if (current->voiceOutPortOwner != next.voiceOutPortOwner &&
	    current->voiceOutPortOwnerAppId != -1 && next.voiceOutPortOwnerAppId != -1 &&
	    current->soundExclusivePid == -1 && next.soundExclusivePid == -1) {
		APPMGR_DEVICE_ASSERT_UNLOCKED(1023, "updateAudioArbitration2NoLock");
		_shellEventVoiceChatAppChangedQueuePush(current->voiceOutPortOwnerAppId,
			next.voiceOutPortOwnerAppId, current->voiceOutPortOwner,
			next.voiceOutPortOwner);
	}

	if ((next.soundExclusivePid != current->soundExclusivePid ||
	     current->soundExclusiveDisplayRequest != next.soundExclusiveDisplayRequest ||
	     current->soundExclusivePriority != next.soundExclusivePriority) &&
	    current->soundExclusivePid != -1 && current->soundExclusiveDisplayRequest)
		SceAVConfigForDriver_91792D6F(current->soundExclusivePriority);
	if (current->otherPortVolume != next.otherPortVolume ||
	    current->soundExclusivePid != next.soundExclusivePid)
		setVolume(&next.soundExclusivePid, &next.otherPortVolume);
	if ((current->soundExclusivePid != next.soundExclusivePid ||
	     current->soundExclusivePriority != next.soundExclusivePriority ||
	     current->displayRequestMode != next.displayRequestMode ||
	     current->displayRequestEnabled != next.displayRequestEnabled ||
	     current->displayRequestBlanked != next.displayRequestBlanked ||
	     current->soundExclusiveDisplayRequest != next.soundExclusiveDisplayRequest) &&
	    next.soundExclusivePid != -1 && next.soundExclusiveDisplayRequest)
		SceAVConfigForDriver_2A2E9042(next.soundExclusivePriority,
			next.displayRequestEnabled, next.displayRequestMode,
			next.displayRequestBlanked, 0);

	if (current->shellMainPortEnabled != next.shellMainPortEnabled && shell != NULL &&
	    next.shellMainPortEnabled)
		enableOutPort(shell->processId, 0);
	if (current->mainOutPortOwner != next.mainOutPortOwner) {
		if (g_appmgrDebugNotifier != NULL)
			g_appmgrDebugNotifier(16, next.mainOutPortOwner, current->mainOutPortOwner, 0);
		enableOutPort(next.mainOutPortOwner, 0);
	}
	if (current->bgmOutPortOwner != next.bgmOutPortOwner) {
		if (g_appmgrDebugNotifier != NULL)
			g_appmgrDebugNotifier(17, next.bgmOutPortOwner, current->bgmOutPortOwner, 0);
		enableOutPort(next.bgmOutPortOwner, 1);
	}
	if (current->voiceOutPortOwner != next.voiceOutPortOwner) {
		if (g_appmgrDebugNotifier != NULL)
			g_appmgrDebugNotifier(18, next.voiceOutPortOwner,
				current->voiceOutPortOwner, 0);
		enableOutPort(next.voiceOutPortOwner, 2);
	}
	if (current->inputPortOwner != next.inputPortOwner) {
		if (g_appmgrDebugNotifier != NULL)
			g_appmgrDebugNotifier(19, next.inputPortOwner, current->inputPortOwner, 0);
		enableInPort(next.voiceOutPortOwner);
	}
	*current = next;
}

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
	result = sceKernelLockMutex(g_appmgrDevice.audioUpdateMutex, 1, NULL);
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
	result = sceKernelUnlockMutex(g_appmgrDevice.audioUpdateMutex, 1);
	if (result != 0) {
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 161, "unlockAudioUpdate",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
	}
}
#else
/* the release build inlines it at every call */
static inline __attribute__((always_inline)) void updateAudioArbitration2(void)
{
	int result;

	APPMGR_DEVICE_ASSERT_UNLOCKED(1105, "updateAudioArbitration2");
	result = lockAudioUpdate();
	if (result != 0)
		return;
	updateAudioArbitration2NoLock();
	if (sceKernelUnlockMutex(g_appmgrDevice.audioUpdateMutex, 1) != 0)
		appmgrDeviceLockPanic();
}
#endif

void appmgrOnAudioInClose(SceUID pid)
{
	AppmgrAudioApp *app;
	int result;

	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 1173, "appmgrOnAudioInClose",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1173, "appmgrOnAudioInClose");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	app = getAppByProcessId(pid);
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
	updateAudioArbitration2();
}

void appmgrOnAudioInOpen(SceUID pid)
{
	AppmgrAudioApp *app;
	int result;

	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 1157, "appmgrOnAudioInOpen",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1157, "appmgrOnAudioInOpen");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	app = getAppByProcessId(pid);
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
	updateAudioArbitration2();
}

void appmgrOnAudioOutRelease(SceUID pid)
{
	AppmgrAudioApp *app;
	int result;

	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 1141, "appmgrOnAudioOutRelease",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1141, "appmgrOnAudioOutRelease");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	app = getAppByProcessId(pid);
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
	updateAudioArbitration2();
}

void appmgrOnAudioOutOpen(SceUID pid)
{
	AppmgrAudioApp *app;
	int result;

	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF_FUNC(0, 1125, "appmgrOnAudioOutOpen",
			"Failed to lock ret=0x%08x\n", result);
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1125, "appmgrOnAudioOutOpen");
#else
		appmgrMainMutexPanic();
#endif
	} else {
		g_appmgrMainMutexOwner = sceKernelGetThreadId();
	}
	app = getAppByProcessId(pid);
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
	updateAudioArbitration2();
}

#if FW_DEBUG
int _sceAppMgrAcquireSoundOutExclusive(int priority, SceUInt32 displayEnabled,
		SceUInt32 displayMode, int displayBlanked, int displayRequest)
{
	SceUInt32 syscallState = sceSyscallEnter();
	AppmgrAudioApp *app = NULL;
	SceUID pid;
	int result;

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
	app = getAppByProcessId(pid);
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
	SceUID pid;
	int result;

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
	app = getAppByProcessId(pid);
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
	updateAudioArbitration2();
	sceSyscallLeave(syscallState);
	return 0;
}

/* the BGM priority of the calling process's application */
static inline __attribute__((always_inline)) void int_sceAppMgrGetBGMPort(SceUID pid, int priority)
{
	AppmgrAudioApp *app;

	APPMGR_DEVICE_ASSERT_LOCKED(1190, "int_sceAppMgrGetBGMPort");
	app = getAppByProcessId(pid);
	if (app != NULL)
		app->bgmPriority = priority;
}

int sceAppMgrAcquireBgmPort(void)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID pid;
	int result;

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
	int_sceAppMgrGetBGMPort(pid, 256);
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
	updateAudioArbitration2();
	sceSyscallLeave(syscallState);
	return 0;
}

int sceAppMgrAcquireBgmPortForMusicPlayer(void)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID pid;
	int result;

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
	int_sceAppMgrGetBGMPort(pid, 130);
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
	updateAudioArbitration2();
	sceSyscallLeave(syscallState);
	return 0;
}

int sceAppMgrReleaseBgmPort(void)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID pid;
	int result;

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
	int_sceAppMgrGetBGMPort(pid, 0);
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
	updateAudioArbitration2();
	sceSyscallLeave(syscallState);
	return 0;
}

int sceAppMgrAcquireBgmPortWithPriority(int priority)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID pid;
	int result;

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
	int_sceAppMgrGetBGMPort(pid, priority);
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
	updateAudioArbitration2();
	sceSyscallLeave(syscallState);
	return 0;
}

int sceAppMgrSetBgmSubPriority(int enabled, int priority)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID pid;
	AppmgrAudioApp *app;
	int result;

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
	app = getAppByProcessId(pid);
	if (app != NULL) {
		app->enableBgmSubPriority = enabled;
		app->bgmSubPriority = priority;
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
	updateAudioArbitration2();
	sceSyscallLeave(syscallState);
	return 0;
}

int sceAppMgrSetBgmSubPriorityForSystemChat(int enabled)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID pid;
	AppmgrAudioApp *app;
	int result;

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
	app = getAppByProcessId(pid);
	if (app != NULL) {
		app->enableBgmSubPriority = enabled;
		app->bgmSubPriority = 194;
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
	updateAudioArbitration2();
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
		AppmgrAudioApp *app = &g_appmgrDevice.apps[index];

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
	SceUID pid;
	int result;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201f;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0)
		appmgrMainMutexPanic();
	g_appmgrMainMutexOwner = sceKernelGetThreadId();
	pid = sceKernelGetProcessId();
	app = getAppByProcessId(pid);
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
	updateAudioArbitration2();
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
	SceUID pid;
	int result;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201f;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0)
		appmgrMainMutexPanic();
	g_appmgrMainMutexOwner = sceKernelGetThreadId();
	pid = sceKernelGetProcessId();
	app = getAppByProcessId(pid);
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
	updateAudioArbitration2();
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
	SceUID pid;
	int result;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return (int)0x8080201f;
	}
	result = sceKernelLockMutex(g_appmgrMainMutex, 1, NULL);
	if (result != 0)
		appmgrMainMutexPanic();
	g_appmgrMainMutexOwner = sceKernelGetThreadId();
	pid = sceKernelGetProcessId();
	app = getAppByProcessId(pid);
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
	updateAudioArbitration2();
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
	g_appmgrDevice.systemChatPid = sceKernelGetProcessId();
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
	result = lockAudioUpdate();
	if (result != 0)
		return result;
	setVolume(&g_appmgrDevice.bgmState.soundExclusivePid, &g_appmgrDevice.bgmState.otherPortVolume);
	result = sceKernelUnlockMutex(g_appmgrDevice.audioUpdateMutex, 1);
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
	updateAudioArbitration2();
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
	g_appmgrDevice.appMgrState.prioritizeSystemChat = enabled;
#else
	g_appmgrDevice.appMgrState.prioritizeSystemChat = enabled;
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
	updateAudioArbitration2();
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
		g_appmgrDevice.systemChatDuckingVolume = volume;
#else
		g_appmgrDevice.systemChatDuckingVolume = volume;
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
		g_appmgrDevice.systemChatDuckingVolume = 256;
#else
		g_appmgrDevice.systemChatDuckingVolume = 256;
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
	updateAudioArbitration2();
	sceSyscallLeave(syscallState);
	return 0;
}

int sceAppMgrSetBgmProxyApp(const char *appName)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID pid;
	int result;
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
	app = getAppByProcessId(pid);
	if (app == NULL)
		goto notFound;
	if (appName != NULL)
		strncpy((char *)app->proxyAppName, appName, 32);
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
	updateAudioArbitration2();
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
		app = &g_appmgrDevice.apps[index];
		if (app->appId > 0 && app->processId == pid)
			app->appId = -1;
	}
	if (g_appmgrDevice.activeAppId == appId)
		g_appmgrDevice.activeAppId = -1;
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
	app = &g_appmgrDevice.apps[0];
	app->isRunning = 1;
	app->soundExclusivePriority = -1;
	g_appmgrDevice.shellPid = pid;
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
		app = &g_appmgrDevice.apps[index];
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
	AppmgrAudioApp *app = &g_appmgrDevice.apps[0];
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
	g_appmgrDevice.shellPid = -1;
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
	g_appmgrDevice.appMgrState.inputImposed = imposed;
#else
	g_appmgrDevice.appMgrState.inputImposed = imposed;
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
	g_appmgrDevice.appMgrState.inputImposed = inputImposed;
	g_appmgrDevice.appMgrState.ctrlpMode = ctrlpMode;
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
	SceUID pid;
	int result;
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
	app = getAppByProcessId(pid);
	if (app == NULL)
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
	int result;
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
	app = getAppByProcessId(sceKernelGetProcessId());
	if (app == NULL)
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

	result = lockAudioUpdate();
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
	shell = g_appmgrDevice.apps[0].appId > 0 ? &g_appmgrDevice.apps[0] : NULL;
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
	if (shell != NULL && g_appmgrDevice.bgmState.shellMainPortEnabled != 0 &&
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
	owner = g_appmgrDevice.bgmState.mainOutPortOwner;
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
	owner = g_appmgrDevice.bgmState.bgmOutPortOwner;
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
	owner = g_appmgrDevice.bgmState.voiceOutPortOwner;
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
	if (g_appmgrDevice.bgmState.inputPortOwner != -1) {
		if (pid < 0)
			pid = g_appmgrDevice.bgmState.voiceOutPortOwner;
		if (pid == g_appmgrDevice.bgmState.voiceOutPortOwner && pid != -1) {
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
	setVolume(&g_appmgrDevice.bgmState.soundExclusivePid, &g_appmgrDevice.bgmState.otherPortVolume);
	result = sceKernelUnlockMutex(g_appmgrDevice.audioUpdateMutex, 1);
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

	info->soundExclusivePid = g_appmgrDevice.bgmState.soundExclusivePid;
	info->mainOutPortOwner = g_appmgrDevice.bgmState.mainOutPortOwner;
	info->bgmOutPortOwner = g_appmgrDevice.bgmState.bgmOutPortOwner;
	info->voiceOutPortOwner = g_appmgrDevice.bgmState.voiceOutPortOwner;
	info->inputPortOwner = g_appmgrDevice.bgmState.inputPortOwner;
	info->backRenderPid = g_appmgrDevice.deviceOwners.backRenderPid;
	info->inputOwnerPid = g_appmgrDevice.deviceOwners.inputPid;
	info->bgmProxyAppId = g_appmgrDevice.deviceOwners.btrmOwnerAppId;
	info->shellPid = g_appmgrDevice.shellPid;
	info->activeAppId = g_appmgrDevice.activeAppId;
	if (g_appmgrDevice.renderingPids[0] != -1)
		info->renderingPids[renderingCount++] = g_appmgrDevice.renderingPids[0];
	if (g_appmgrDevice.renderingPids[1] != -1)
		info->renderingPids[renderingCount++] = g_appmgrDevice.renderingPids[1];
	if (g_appmgrDevice.renderingPids[2] != -1)
		info->renderingPids[renderingCount++] = g_appmgrDevice.renderingPids[2];
	if (g_appmgrDevice.renderingPids[3] != -1)
		info->renderingPids[renderingCount++] = g_appmgrDevice.renderingPids[3];
	if (g_appmgrDevice.renderingPids[4] != -1)
		info->renderingPids[renderingCount++] = g_appmgrDevice.renderingPids[4];
	if (g_appmgrDevice.renderingPids[5] != -1)
		info->renderingPids[renderingCount++] = g_appmgrDevice.renderingPids[5];
	if (g_appmgrDevice.renderingPids[6] != -1)
		info->renderingPids[renderingCount++] = g_appmgrDevice.renderingPids[6];
	if (g_appmgrDevice.renderingPids[7] != -1)
		info->renderingPids[renderingCount++] = g_appmgrDevice.renderingPids[7];
	memset(&info->renderingPids[renderingCount], -1,
		   sizeof(info->renderingPids) - sizeof(SceUID) * renderingCount);
	count = g_appmgrDevice.touchProcessPriorities.count;
	if (count) {
		index = 0;
		do {
			pid = g_appmgrDevice.touchProcessPriorities.processIds[index++];
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
	info->displayOwner0 = g_appmgrDevice.displayOwner[0];
	info->displayOwner1 = g_appmgrDevice.displayOwner[1];
	info->systemChatPid = g_appmgrDevice.systemChatPid;
	info->backRenderAppId = g_appmgrDevice.backRenderAppId;
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
	g_appmgrDevice.appMgrState.recommendedScreenOrientation = orientation;
}

