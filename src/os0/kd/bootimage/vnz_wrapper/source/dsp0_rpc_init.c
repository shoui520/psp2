/* Sony venezia/dsp0_rpc.c: RPC initialization. */
#undef __FILE_NAME__
#define __FILE_NAME__ "venezia/dsp0_rpc.c"
#include <sce/debug.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/threadmgr/semaphores.h>
#include "venezia_internal.h"

extern void (*g_veneziaRpcTrace)(int event, int count, ...);
extern void (*SceSysrootForDriver_49843C16(void))(int event, int count, ...);
#if FW_VERSION >= 0x04000000
extern int initVeneziaRpcCommunicationMemory(void);
extern int initVeneziaRpcDma(void);
extern int initVeneziaMailbox(void);
#else
extern int initVeneziaRpcCommunicationMemory(void);
extern int initVeneziaRpcDma(void);
extern int initVeneziaMailbox(void);
#endif

int initVeneziaRpc(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	void *base;
	int ret;

	memset(&g_veneziaRpcState, 0, sizeof(g_veneziaRpcState));
	g_veneziaRpcState.lastChannel = 7;
	g_veneziaRpcState.lock = 0;
	g_veneziaRpcState.memoryBlock = -1;
	g_veneziaRpcState.semaphore = -1;
	g_veneziaRpcState.debugSemaphore = -1;
	g_veneziaRpcState.debugLock = 0;
	g_veneziaRpcState.physicalBase = 0x1f840000;
	g_veneziaRpcState.memorySize = 0x20000;
	g_veneziaRpcState.freeChannels = 8;
	g_veneziaRpcState.freeDebugChannels = 1;
	opt = (SceKernelAllocMemBlockKernelOpt) {
		.size = sizeof(opt),
		.attr = 2,
		.paddr = 0x1f840000,
	};
	ret = ksceKernelAllocMemBlock("SceVeneziaSpram", 0x20108006, 0x20000, &opt);
	if (ret < 0)
		return ret;
	g_veneziaRpcState.memoryBlock = ret;
	ksceKernelGetMemBlockBase(ret, &base);
	g_veneziaRpcState.memory = base;
	ret = initVeneziaRpcCommunicationMemory();
	if (ret < 0)
		return ret;
	ret = initVeneziaRpcDma();
	if (ret < 0)
		return ret;
	ret = ksceKernelCreateSema("SceCodecEngineDsp0Rpc", 0x2000, 8, 8, NULL);
	if (ret < 0)
		return ret;
	g_veneziaRpcState.semaphore = ret;
	ret = ksceKernelCreateSema("SceCodecEngineDsp0RpcDebug", 0x2000, 1, 1, NULL);
	if (ret < 0)
		return ret;
	g_veneziaRpcState.debugSemaphore = ret;
	ret = initVeneziaMailbox();
	if (ret < 0)
		return ret;
	g_veneziaRpcTrace = SceSysrootForDriver_49843C16();
	return 0;
}

extern unsigned int getVeneziaClockLockCount(void);
extern unsigned int ScePervasiveForDriver_725B4F59(int clock);
#if FW_VERSION >= 0x04000000
extern int veneziaClockSelector(int frequency);
#else
extern int veneziaClockSelector(int frequency);
#endif

int _sceVeneziaRpcClockDown(void)
{
	int intr, ret;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(
		(SceKernelSpinlock *)&g_veneziaRpcState.lock);
	if (g_veneziaRpcState.freeChannels == 8 && getVeneziaClockLockCount() == 0) {
		ret = ScePervasiveForDriver_725B4F59(veneziaClockSelector(41));
#if FW_DEBUG
		ksceKernelAssert(ret >= 0, SCE_DBGINFO_FUNC(555, "_sceVeneziaRpcClockDown"),
				 __builtin_return_address(0));
#endif
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(
		(SceKernelSpinlock *)&g_veneziaRpcState.lock, intr);
	return 0;
}

int SceCodecEngineWrapperForDriver_391BD4FD(unsigned int command,
		const unsigned int *input, VeneziaRpcPack pack, VeneziaRpcUnpack unpack)
{
	return _callGenericThunk(command, input, pack, unpack, 0x1010000);
}

int SceCodecEngineWrapperForDriver_0D3BB164(unsigned int command, unsigned int argument0,
		unsigned int argument1, unsigned int argument2, unsigned int argument3)
{
	unsigned int arguments[4] __attribute__((aligned(8))) = {
		argument0, argument1, argument2, argument3
	};
	return _callGenericThunk(command, arguments, NULL, NULL, 0x1010000);
}

/* name unknown: synchronous RPC using a clock held by the caller. */
int callVeneziaRpcThunkWithoutClockLock(unsigned int command, const unsigned int *input,
				      VeneziaRpcPack pack, VeneziaRpcUnpack unpack)
{
	return _callGenericThunk(command, input, pack, unpack, 0x10000);
}

/* name unknown: synchronous RPC using a clock held by the caller. */
int callVeneziaRpcWithoutClockLock(unsigned int command, unsigned int argument0,
		unsigned int argument1, unsigned int argument2, unsigned int argument3)
{
	unsigned int arguments[4] __attribute__((aligned(8))) = {
		argument0, argument1, argument2, argument3
	};
	return _callGenericThunk(command, arguments, NULL, NULL, 0x10000);
}

/* name unknown: issue an RPC without waiting for its result. */
int callVeneziaRpc(unsigned int command, unsigned int argument0,
		unsigned int argument1, unsigned int argument2, unsigned int argument3)
{
	unsigned int arguments[4] __attribute__((aligned(8))) = {
		argument0, argument1, argument2, argument3
	};
	return _callGenericThunk(command, arguments, NULL, NULL, 0);
}

/* name unknown: hold all normal and debug RPC channel permits. */
int waitVeneziaRpcSemas(void)
{
	int ret, debugRet;

	ret = ksceKernelWaitSema(g_veneziaRpcState.semaphore, 8, NULL);
	debugRet = ksceKernelWaitSema(g_veneziaRpcState.debugSemaphore, 1, NULL);
	if (debugRet < 0)
		return debugRet;
	return ret < 0 ? ret : 0;
}

/* name unknown: release all normal and debug RPC channel permits. */
int signalVeneziaRpcSemas(void)
{
	int ret, debugRet;

	ret = ksceKernelSignalSema(g_veneziaRpcState.semaphore, 8);
	debugRet = ksceKernelSignalSema(g_veneziaRpcState.debugSemaphore, 1);
	if (debugRet < 0)
		return debugRet;
	return ret < 0 ? ret : 0;
}

/* name unknown: RPC state accessor. */

#if FW_DEBUG
unsigned int getRpcFreeChannelCount(void)
{
	return g_veneziaRpcState.freeChannels;
}
#endif

/* name unknown: RPC state accessor. */
unsigned int getRpcMemoryPhysicalBase(void)
{
	return g_veneziaRpcState.physicalBase;
}

/* name unknown: RPC state accessor. */
VeneziaRpcMemory * getRpcMemoryBase(void)
{
	return g_veneziaRpcState.memory;
}

/* name unknown: RPC state accessor. */
SceSize getRpcMemorySize(void)
{
	return g_veneziaRpcState.memorySize;
}

/* name unknown: heap follows the control and RPC packet area. */
void *getRpcHeapBase(void)
{
	return g_veneziaRpcState.memory ? g_veneziaRpcState.memory + 1 : NULL;
}

/* name unknown: heap excludes the control and RPC packet area. */
SceSize getRpcHeapSize(void)
{
	return g_veneziaRpcState.memorySize ?
		g_veneziaRpcState.memorySize - sizeof(VeneziaRpcMemory) : 0;
}

/* name unknown: boot/resume handshake word in DSP communication memory. */
unsigned int getRpcBootState(void)
{
	return g_veneziaRpcState.memory ? g_veneziaRpcState.memory->control.bootState : 1;
}

/* name unknown: set the boot/resume handshake word. */
unsigned int setRpcBootState(unsigned int state)
{
	if (g_veneziaRpcState.memory)
		g_veneziaRpcState.memory->control.bootState = state;
	return state;
}
