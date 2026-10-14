/* Sony venezia/dsp0_rpc.c. */
#undef __FILE_NAME__
#define __FILE_NAME__ "venezia/dsp0_rpc.c"
#include <sce/debug.h>
#include <psp2kern/kernel/threadmgr/semaphores.h>
#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include "venezia_internal.h"

extern SceUID ksceKernelGetProcessId(void) __asm__("sceKernelGetProcessId");
extern int _sceVeneziaRunInBackground(void);
extern int _sceVeneziaStopInBackground(void);
extern unsigned int getVeneziaClockLockCount(void);
extern unsigned int ScePervasiveForDriver_725B4F59(int clock);
#if FW_VERSION >= 0x04000000
extern int veneziaClockSelector(int frequency);
#else
extern int veneziaClockSelector(int frequency);
#endif


#if FW_VERSION >= 0x04000000
extern int submitVeneziaRpc(unsigned int channel);
extern int waitVeneziaRpc(unsigned int channel);
#else
extern int submitVeneziaRpc(unsigned int channel);
extern int waitVeneziaRpc(unsigned int channel);
#endif

int _callDebugThunk(unsigned int command, const unsigned int *input,
		    VeneziaRpcPack pack, VeneziaRpcUnpack unpack)
{
	VeneziaRpcThunk thunk;
	VeneziaRpcArg *arg;
	unsigned int channel, sequence;
	int intr, ret;
#if FW_DEBUG
	int allocated = 0;
#endif

#if FW_DEBUG
	ksceKernelAssert(command <= 31, SCE_DBGINFO_FUNC(1062, "_callDebugThunk"),
			 __builtin_return_address(0));
#endif
	ret = ksceKernelWaitSema(g_veneziaRpcState.debugSemaphore, 1, NULL);
	if (ret < 0)
		return ret;
	channel = -1;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&g_veneziaRpcState.debugLock);
	if (!g_veneziaRpcState.channels[8].inUse) {
		channel = 8;
#if FW_DEBUG
		allocated = 1;
#endif
		g_veneziaRpcState.channels[8].inUse = 1;
		g_veneziaRpcState.freeDebugChannels--;
	}
#if FW_DEBUG
	ksceKernelAssert(allocated,
			 SCE_DBGINFO_FUNC(1013, "_allocRpcChDebug"),
			 __builtin_return_address(0));
#endif
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&g_veneziaRpcState.debugLock, intr);
	thunk.channel = channel;
	g_veneziaRpcState.channels[channel].flags = 0x10000;
	thunk.allocationCount = 0;
#if FW_DEBUG
	ksceKernelAssert(allocated, SCE_DBGINFO_FUNC(222, "_getRpcArg"),
			 __builtin_return_address(0));
#endif
	arg = g_veneziaRpcState.memory ? &g_veneziaRpcState.memory->arguments[channel] : NULL;
#if FW_DEBUG
	ksceKernelAssert(arg != NULL, SCE_DBGINFO_FUNC(1083, "_callDebugThunk"),
			 __builtin_return_address(0));
#endif
	sequence = g_veneziaRpcState.debugSequence;
	arg->command = command | g_veneziaRpcState.channels[thunk.channel].flags;
	g_veneziaRpcState.debugSequence = sequence + 1;
	arg->sequence = sequence;
	if (pack) {
		thunk.packing = 1;
		pack(&thunk, input, arg->arguments);
	} else {
		arg->arguments[0] = input[0];
		arg->arguments[1] = input[1];
		arg->arguments[2] = input[2];
		arg->arguments[3] = input[3];
	}
	arg->result = 0;
	arg->resultFlags = 0;
	arg->pid = ksceKernelGetProcessId();
	submitVeneziaRpc(thunk.channel);
	ret = 0;
	if (g_veneziaRpcState.channels[thunk.channel].flags & 0x10000) {
		waitVeneziaRpc(thunk.channel);
		if (unpack) {
			thunk.packing = 0;
			unpack(&thunk);
		}
		ret = arg->result;
	}
	channel = thunk.channel;
#if FW_DEBUG
	ksceKernelAssert(channel == 8, SCE_DBGINFO_FUNC(1025, "_freeRpcChDebug"),
			 __builtin_return_address(0));
#endif
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&g_veneziaRpcState.debugLock);
	g_veneziaRpcState.channels[channel].inUse = 0;
	g_veneziaRpcState.freeDebugChannels++;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&g_veneziaRpcState.debugLock, intr);
	ksceKernelSignalSema(g_veneziaRpcState.debugSemaphore, 1);
	return ret;
}
