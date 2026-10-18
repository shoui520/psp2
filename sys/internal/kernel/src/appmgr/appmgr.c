/* Application manager initialization and suspend notifications. */

#include <sysmem/debug.h>
#include <iofilemgr/iofilemgr.h>
#include <sysmem/sysevent.h>
#include <sysmem/sysroot_appmgr.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <sysmem/pmmgr.h>
#include <string.h>
#include "app_state_internal.h"
#include "appmgr_event.h"
#include "appmgr_shell_event.h"
#include <sysmem/sysroot.h>
#include <power/power.h>
#include <appmgr/appmgr.h>
#include <appmgr/appmgr_rdrfp.h>

extern int _sceAppMgrSystemResume(void);
extern int _sceAppMgrSystemSuspend(void);
extern int sharedFbInitialize(void);
extern int sceAppMgrInit(void);
extern SceUID g_appmgrCoreHeap;
extern SceUID g_appmgrMountHeap;
extern SceUID g_appmgrMountMutex;
extern SceUID g_appmgrSaveDataHeap;
extern SceUID g_appmgrSaveDataMutex;
extern int g_appmgrAppInfoCacheKey;
extern int appmgrInitializeBgdlQueueStorage(void);
extern int appMgrLockInitialize(void);
extern void initProcessStateUpdaterThread(void);
extern int _sceAppMgrAbortHandlerInitialize(void);
extern int _appmgrProcessInitialize(int (*callback)(struct AppState *app));
extern int _appArbitrationOnProcessUnloaded(struct AppState *app);
extern void initializeAppMgrDevice(void);
extern int _appmgrQueueInit(void);
extern int appmgrInitializeDebugSettings(void);
extern int appmgrRegisterMountDebugSettingCallback(void);
extern int appmgrInitializeSaveDataSlotCache(void);
extern int appmgrMountShellExternalDirectories(SceUID pid);
extern int dumpBgmState(void);
extern void appmgrRegisterDebuggerCallbacks(void);
extern int appmgrMapPath(const char *path, SceUID pid, char *mapped, SceSize size);

int g_appmgrPowerSaveMode;
SceUID g_appmgrAppMutex = -1;
int g_appmgrPowerSaveSuspendBackgroundApps;
SceUID g_appmgrMainMutex = -1;
SceUID g_appmgrMainMutexOwner = -1;
SceUInt32 g_appmgrDebugNotifier;
SceUID g_appmgrPhotoMutex = -1;
int g_appmgrSafeMemoryKey;
SceUID g_appmgrSafeMemoryMutex = -1;
SceUID g_appmgrDebugShellEventSemaphore = -1;
SceUID g_appmgrBottomSemaphore = -1;
SceUID g_appmgrLiveAreaOperationMutex = -1;
SceUID g_appmgrLiveAreaResultMutex = -1;
SceUID g_appmgrLiveAreaResultCondition = -1;
AppmgrExtraAppParam g_appmgrExtraAppParam;

#if !FW_DEBUG
static const SceKernelDebugInfo moduleStartPanicInfo = {
	.fileHash = 0x3b692bac,
	.lineHash = 0xf625ec42,
	.funcHash = 0xe39e1e2a,
};

/* The release-only failure helper retains its original hashed debug record. */
static const SceKernelDebugInfo initializationPanicInfo = {
	.fileHash = 0xc9656e8e,
	.lineHash = 0xc879dadc,
	.funcHash = 0x11a05ee1,
};

#define INIT_RECORD(name, hash) \
	static const SceKernelDebugInfo name = { \
		.fileHash = 0x3b692bac, .lineHash = hash, .funcHash = 0x43109186, \
	}

INIT_RECORD(coreHeapPanicInfo, 0xa73266e8);
INIT_RECORD(appMutexPanicInfo, 0x4e1cbef4);
INIT_RECORD(mountHeapPanicInfo, 0xf7f08368);
INIT_RECORD(mountMutexPanicInfo, 0x3252bf98);
INIT_RECORD(saveDataHeapPanicInfo, 0x5b6cef9f);
INIT_RECORD(saveDataMutexPanicInfo, 0x726444a6);
INIT_RECORD(photoMutexPanicInfo, 0x2fc2dec6);
INIT_RECORD(safeMemoryStoragePanicInfo, 0x172fb5e7);
INIT_RECORD(safeMemoryMutexPanicInfo, 0xbb207bac);
INIT_RECORD(appInfoCacheStoragePanicInfo, 0x1baad379);
INIT_RECORD(lockInitializePanicInfo, 0x19a65ec2);
INIT_RECORD(debugShellSemaphorePanicInfo, 0x06842ff5);
INIT_RECORD(bottomSemaphorePanicInfo, 0x1398e0e4);
INIT_RECORD(liveAreaOperationPanicInfo, 0xd61e301d);
INIT_RECORD(liveAreaResultMutexPanicInfo, 0x921410a0);
INIT_RECORD(liveAreaResultConditionPanicInfo, 0x3f3a5801);
INIT_RECORD(idleCallbackPanicInfo, 0x7f9279d4);
INIT_RECORD(productModePanicInfo, 0x551c1b34);

#undef INIT_RECORD

void appmgrInitializationPanic(void)
{
	SceDebugForDriver_391B5B74(&initializationPanicInfo, __builtin_return_address(0));
}
#endif

int appmgrSysEventHandler(int resume, int eventid, void *args, void *opt)
{
	(void)args;
	(void)opt;
	if (resume != 0) {
		if (resume == 1 && eventid == 0x400000)
			return _sceAppMgrSystemResume();
		return 0;
	}
	if (eventid == 0x20f)
		return _sceAppMgrSystemSuspend();
	return 0;
}

#if FW_DEBUG
#define INIT_ASSERT(condition, line, record) do { \
	if (!(condition)) { \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_CHECK_FUNC(0, line, "sceAppMgrInit"); \
	} \
} while (0)
#define INIT_RESULT(result, line) do { \
	if ((result) != 0) { \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_PRINTF_FUNC(0, line, "sceAppMgrInit", \
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n", \
			result, result, 0, 0); \
	} \
} while (0)
#else
#define INIT_ASSERT(condition, line, record) do { \
	if (!(condition)) \
		SceDebugForDriver_391B5B74(&(record), __builtin_return_address(0)); \
} while (0)
#define INIT_RESULT(result, line) do { \
	if ((result) != 0) \
		appmgrInitializationPanic(); \
} while (0)
#endif

int sceAppMgrInit(void)
{
	SceKernelHeapCreateOpt heapOpt;
	SceProductMode productMode = 0;
	struct AppState *app;
	SceUID uid;
	int result;

	heapOpt.size = sizeof(heapOpt);
	heapOpt.flags = HEAP_HAS_MAX_SIZE;
	heapOpt.maxSize = 0x80000;
	result = appmgrInitializeShellEventQueue();
	INIT_RESULT(result, 170);
	uid = SceSysmemForDriver_9328E0E8("SceAppMgrApp", 4096, &heapOpt);
	INIT_ASSERT(uid >= 0, 175, coreHeapPanicInfo);
	g_appmgrCoreHeap = uid;
	uid = sceKernelCreateMutex("SceAppMgrApp", 0, 0, NULL);
	INIT_ASSERT(uid >= 0, 180, appMutexPanicInfo);
	g_appmgrAppMutex = uid;
	result = appmgrInitializeBgdlQueueStorage();
#if FW_DEBUG
	INIT_ASSERT(result >= 0, 184, initializationPanicInfo);
#else
	if (result < 0)
		appmgrInitializationPanic();
#endif
	uid = SceSysmemForDriver_9328E0E8("SceAppMgrMount", 0x10000, &heapOpt);
	INIT_ASSERT(uid >= 0, 189, mountHeapPanicInfo);
	g_appmgrMountHeap = uid;
	uid = sceKernelCreateMutex("SceAppMgrMount", 0, 0, NULL);
	INIT_ASSERT(uid >= 0, 193, mountMutexPanicInfo);
	g_appmgrMountMutex = uid;
	uid = SceSysmemForDriver_9328E0E8("SceAppMgrSaveData", 40960, &heapOpt);
	INIT_ASSERT(uid >= 0, 200, saveDataHeapPanicInfo);
	g_appmgrSaveDataHeap = uid;
	uid = sceKernelCreateMutex("SceAppMgrSaveData", 0, 0, NULL);
	INIT_ASSERT(uid >= 0, 205, saveDataMutexPanicInfo);
	g_appmgrSaveDataMutex = uid;
	uid = sceKernelCreateMutex("SceAppMgrPhoto", 0, 0, NULL);
	INIT_ASSERT(uid >= 0, 214, photoMutexPanicInfo);
	g_appmgrPhotoMutex = uid;
	result = SceProcessmgrForDriver_3801D7D6("SceAppMgrSafeMemory", 0x10000);
	INIT_ASSERT(result >= 0, 224, safeMemoryStoragePanicInfo);
	g_appmgrSafeMemoryKey = result;
	uid = sceKernelCreateMutex("SceAppMgrSafeMemory", 0, 0, NULL);
	INIT_ASSERT(uid >= 0, 229, safeMemoryMutexPanicInfo);
	g_appmgrSafeMemoryMutex = uid;
	result = SceProcessmgrForDriver_3801D7D6("SceAppMgrAppInfoCache", 1024);
	INIT_ASSERT(result >= 0, 238, appInfoCacheStoragePanicInfo);
	g_appmgrAppInfoCacheKey = result;
	for (app = g_appmgrAppStates; app != g_appmgrAppStates + 20; ++app) {
		result = appmgrInitializeAppState(app);
		INIT_RESULT(result, 246);
	}
	result = appMgrLockInitialize();
	INIT_ASSERT(result == 0, 250, lockInitializePanicInfo);
	uid = sceKernelCreateSema("SceAppMgrDbgShellEventCount", 0, 5, 5, NULL);
	INIT_ASSERT(uid >= 0, 258, debugShellSemaphorePanicInfo);
	g_appmgrDebugShellEventSemaphore = uid;
	g_appmgrBottomSemaphore = sceKernelCreateSema("SceAppMgrBottomSema",
		0x8000, 0, 1, NULL);
	INIT_ASSERT(g_appmgrBottomSemaphore != 0, 265, bottomSemaphorePanicInfo);
	uid = sceKernelCreateMutex("SceAppMgrLiveAreaOperation", 0x8000, 0, NULL);
	INIT_ASSERT(uid >= 0, 269, liveAreaOperationPanicInfo);
	g_appmgrLiveAreaOperationMutex = uid;
	uid = sceKernelCreateMutex("SceAppMgrLiveAreaOpeResultMx", 0x8000, 0, NULL);
	INIT_ASSERT(uid >= 0, 273, liveAreaResultMutexPanicInfo);
	g_appmgrLiveAreaResultMutex = uid;
	uid = sceKernelCreateCond("SceAppMgrLiveAreaOpeResultCd", 0x8000, uid, NULL);
	INIT_ASSERT(uid >= 0, 280, liveAreaResultConditionPanicInfo);
	g_appmgrLiveAreaResultCondition = uid;
	initProcessStateUpdaterThread();
	result = _sceAppMgrAbortHandlerInitialize();
	INIT_RESULT(result, 287);
	result = _appmgrProcessInitialize(_appArbitrationOnProcessUnloaded);
	INIT_RESULT(result, 291);
	initializeAppMgrDevice();
	/* The provider's opaque registration type has an ignored int result. */
	result = ScePowerForDriver_0856FD0A(
		(PowerProcessIdleCallback)appmgrProcessIdleCallback, NULL);
#if FW_DEBUG
	INIT_RESULT(result, 296);
#else
	if (result != 0)
		SceDebugForDriver_00CCE39C(&idleCallbackPanicInfo,
			__builtin_return_address(0), "0x%08x,0x%08x\n", result, 0);
#endif
	result = _appmgrQueueInit();
	INIT_RESULT(result, 299);
	result = ScePmMgrForDriver_2AC815A2(&productMode);
#if FW_DEBUG
	INIT_RESULT(result, 303);
#else
	if (result != 0)
		SceDebugForDriver_00CCE39C(&productModePanicInfo,
			__builtin_return_address(0), "0x%08x,0x%08x\n", result, 0);
#endif
	result = appmgrInitializeDebugSettings();
	INIT_RESULT(result, 306);
	result = appmgrRegisterMountDebugSettingCallback();
	INIT_RESULT(result, 309);
	result = appmgrInitializeSaveDataSlotCache();
	INIT_RESULT(result, 312);
	if (!SceSysrootForDriver_834439A7() && !SceSysrootForDriver_B0E1FC67() && productMode == 0) {
		result = appmgrMountShellExternalDirectories(sceKernelGetProcessId());
		INIT_RESULT(result, 320);
	}
	memset(&g_appmgrExtraAppParam, 0, sizeof(g_appmgrExtraAppParam));
	return 0;
}

#undef INIT_ASSERT
#undef INIT_RESULT

int _sceAppMgrModuleStart(SceSize arglen, const void *argp)
{
	int result;

	(void)arglen;
	(void)argp;
	result = sharedFbInitialize();
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(result == 0, 126, "_sceAppMgrModuleStart",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		SceDebugForDriver_00CCE39C(&moduleStartPanicInfo,
			__builtin_return_address(0), "0x%08x,0x%08x\n", result, 0);
#endif
	}
	result = sceAppMgrInit();
	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(result == 0, 129, "_sceAppMgrModuleStart",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		appmgrInitializationPanic();
#endif
	}
	SceSysrootForDriver_9A8871F1(sceAppMgrSpawnProcessForDriver);
	appmgrRegisterDebuggerCallbacks();
	SceSysrootForDriver_E9B4221F(sceAppMgrGetShellPidForDriver);
	SceIofilemgrForDriver_CFAECF18(appmgrMapPath);
	sceKernelRegisterSysEventHandler("SceAppMgr", appmgrSysEventHandler, NULL);
	g_appmgrDebugNotifier = SceSysrootForDriver_525B24C5();
	return 0;
}

int appmgrReturnSuccess(void)
{
	return 0;
}
