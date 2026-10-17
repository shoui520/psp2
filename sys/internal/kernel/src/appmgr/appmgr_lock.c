/* Core locks. */

#include <sysmem/debug.h>
#include <psp2kern/kernel/threadmgr.h>

extern SceUID g_appmgrMainMutex;
extern int sceAppMgrDump(void);
extern int dumpBgmState(void);

SceUID g_appmgrCore2Mutex = -1;
SceUID g_appmgrCore2MutexOwner = -1;
SceUID g_appmgrCoreKmMutex;

#if !FW_DEBUG
static const SceKernelDebugInfo mainMutexInitializePanicInfo = {
	.fileHash = 0x3e35b696,
	.lineHash = 0x4c80cdd0,
	.funcHash = 0x28171cc4,
};

static const SceKernelDebugInfo core2MutexInitializePanicInfo = {
	.fileHash = 0x3e35b696,
	.lineHash = 0x3dfb1467,
	.funcHash = 0x28171cc4,
};

static const SceKernelDebugInfo coreKmMutexInitializePanicInfo = {
	.fileHash = 0x3e35b696,
	.lineHash = 0x0c7ebdc1,
	.funcHash = 0x28171cc4,
};
#endif

int appMgrLockInitialize(void)
{
	SceUID mutex;

	mutex = ksceKernelCreateMutex("SceAppMgrCore", 0, 0, NULL);
	if (mutex < 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 38, "appMgrLockInitialize");
#else
		ksceKernelPanic(&mainMutexInitializePanicInfo, __builtin_return_address(0));
#endif
	}
	g_appmgrMainMutex = mutex;
	mutex = ksceKernelCreateMutex("SceAppMgrCore2", 0x8000, 0, NULL);
	if (mutex < 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 42, "appMgrLockInitialize");
#else
		ksceKernelPanic(&core2MutexInitializePanicInfo, __builtin_return_address(0));
#endif
	}
	g_appmgrCore2Mutex = mutex;
	mutex = ksceKernelCreateMutex("SceAppMgrCoreKM", 0x8000, 0, NULL);
	if (mutex < 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 46, "appMgrLockInitialize");
#else
		ksceKernelPanic(&coreKmMutexInitializePanicInfo, __builtin_return_address(0));
#endif
	}
	g_appmgrCoreKmMutex = mutex;
	return 0;
}
