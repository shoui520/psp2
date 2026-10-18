/* Shared event-flag wait objects. */

#include <sysmem/debug.h>
#include <psp2kern/kernel/threadmgr.h>
#include "appmgr_internal_util.h"
#include <appmgr/appmgr.h>

extern int dumpBgmState(void);

struct AppmgrWaitObject g_appmgrWaitObjects[7];

#if !FW_DEBUG
static const SceKernelDebugInfo waitObjectNotifyPanicInfo = {
	.fileHash = 0x062507c7,
	.lineHash = 0x2dd1fe29,
	.funcHash = 0x0d23895c,
};

static const SceKernelDebugInfo waitObjectWaitPanicInfo = {
	.fileHash = 0x062507c7,
	.lineHash = 0x69d711dc,
	.funcHash = 0x584a426a,
};

static const SceKernelDebugInfo waitObjectInitializePanicInfo = {
	.fileHash = 0x062507c7,
	.lineHash = 0x5ebd9154,
	.funcHash = 0x45bd71ae,
};

static const SceKernelDebugInfo waitObjectClearPanicInfo = {
	.fileHash = 0x062507c7,
	.lineHash = 0xac4fbd8c,
	.funcHash = 0xb9c2f4eb,
};
#endif

int appmgrUtilWaitObjectInitialize(void)
{
	SceUID eventFlag = sceKernelCreateEventFlag("SceAppMgrEv", 0xa000, 0, NULL);

	if (eventFlag <= 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 28, "appmgrUtilWaitObjectInitialize");
#else
		SceDebugForDriver_391B5B74(&waitObjectInitializePanicInfo, __builtin_return_address(0));
#endif
	}
	g_appmgrWaitObjects[0].eventFlag = eventFlag;
	g_appmgrWaitObjects[1].eventFlag = eventFlag;
	g_appmgrWaitObjects[2].eventFlag = eventFlag;
	g_appmgrWaitObjects[3].eventFlag = eventFlag;
	g_appmgrWaitObjects[4].eventFlag = eventFlag;
	g_appmgrWaitObjects[5].eventFlag = eventFlag;
	g_appmgrWaitObjects[6].eventFlag = eventFlag;
	g_appmgrWaitObjects[0].bits = 1;
	g_appmgrWaitObjects[1].bits = 2;
	g_appmgrWaitObjects[2].bits = 4;
	g_appmgrWaitObjects[3].bits = 8;
	g_appmgrWaitObjects[4].bits = 16;
	g_appmgrWaitObjects[5].bits = 32;
	g_appmgrWaitObjects[6].bits = 64;
	g_appmgrWaitObjects[0].next = 0;
	g_appmgrWaitObjects[1].next = 0;
	g_appmgrWaitObjects[2].next = 0;
	g_appmgrWaitObjects[3].next = 0;
	g_appmgrWaitObjects[4].next = 0;
	g_appmgrWaitObjects[5].next = 0;
	g_appmgrWaitObjects[6].next = 0;
	return 0;
}

struct AppmgrWaitObject *appmgrUtilGetWaitObject(int index)
{
	return &g_appmgrWaitObjects[index];
}

void appmgrUtilWaitObjectClear(struct AppmgrWaitObject *waitObject)
{
	int result = sceKernelClearEventFlag(waitObject->eventFlag, ~waitObject->bits);

	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 48, "appmgrUtilWaitObjectClear",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		SceDebugForDriver_00CCE39C(&waitObjectClearPanicInfo, __builtin_return_address(0),
			"0x%08x,0x%08x\n", result, 0);
#endif
	}
}

void appmgrUtilWaitObjectNotify(struct AppmgrWaitObject *waitObject)
{
	int result = sceKernelSetEventFlag(waitObject->eventFlag, waitObject->bits);

	if (result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 55, "appmgrUtilWaitObjectNotify",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		SceDebugForDriver_00CCE39C(&waitObjectNotifyPanicInfo, __builtin_return_address(0),
			"0x%08x,0x%08x\n", result, 0);
#endif
	}
}

int appmgrUtilWaitObjectWait(struct AppmgrWaitObject *waitObject, SceUInt32 *timeout)
{
	int result = sceKernelWaitEventFlag(waitObject->eventFlag, waitObject->bits,
		3, NULL, timeout);

	if ((SceUInt32)result + 0x7ffd7fd4U > 1 &&
	    result != (int)0x80028005U && result != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 65, "appmgrUtilWaitObjectWait",
			"[SceAppMgr] The actual value is %d(0x%08x), but expected value is %d(0x%08x)\n",
			result, result, 0, 0);
#else
		SceDebugForDriver_00CCE39C(&waitObjectWaitPanicInfo, __builtin_return_address(0),
			"0x%08x,0x%08x\n", result, 0);
#endif
	}
	return result;
}
