/* Sony venezia/dsp0_rpc.c. */
#include <sysmem/debug.h>
#include <psp2kern/kernel/threadmgr/semaphores.h>
#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include "../venezia_internal.h"
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <psp2kern/kernel/sysclib.h>
#include <lowio/pervasive.h>
#include <threadmgr/threadmgr.h>

VeneziaRpcState g_veneziaRpcState;

extern int _sceVeneziaRunInBackground(void);
extern int _sceVeneziaStopInBackground(void);
extern unsigned int getVeneziaClockLockCount(void);
#if FW_VERSION >= 0x04000000
extern int veneziaClockSelector(int frequency);
#else
extern int veneziaClockSelector(int frequency);
#endif

#if !FW_DEBUG
static inline
#endif
int _freeRpcCh(unsigned int channel, int clockDown)
{
	VeneziaRpcArg *arg;
	int intr, ret;
	unsigned int command;

	intr = SceCpuForDriver_D32ACE9E((SceKernelSpinlock *)&g_veneziaRpcState.lock);
	g_veneziaRpcState.channels[channel].inUse = 0;
	g_veneziaRpcState.freeChannels++;
#if FW_DEBUG
	SceDebugForDriver_35A35322(channel <= 15, SCE_DBGINFO_FUNC(222, "_getRpcArg"),
			 __builtin_return_address(0));
#endif
	arg = g_veneziaRpcState.memory ? &g_veneziaRpcState.memory->arguments[channel] : NULL;
#if FW_DEBUG
	SceDebugForDriver_35A35322(arg != NULL, SCE_DBGINFO_FUNC(637, "_freeRpcCh"),
			 __builtin_return_address(0));
#endif
	command = arg->command;
	if (command == 97) {
		if (arg->result < 0)
			goto clock;
		_sceVeneziaRunInBackground();
		command = arg->command;
	}
	if (command == 98 && arg->result >= 0)
		_sceVeneziaStopInBackground();
clock:
	if (clockDown && g_veneziaRpcState.freeChannels == 8 &&
	    getVeneziaClockLockCount() == 0) {
		ret = ScePervasiveForDriver_725B4F59(veneziaClockSelector(41));
#if FW_DEBUG
		SceDebugForDriver_35A35322(ret >= 0, SCE_DBGINFO_FUNC(650, "_freeRpcCh"),
				 __builtin_return_address(0));
#endif
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr((SceKernelSpinlock *)&g_veneziaRpcState.lock, intr);
	return 0;
}

void (*g_veneziaRpcTrace)(int event, int count, ...);
#if FW_VERSION >= 0x04000000
extern int getVeneziaPreferredClock(void);
extern int submitVeneziaRpc(unsigned int channel);
extern int waitVeneziaRpc(unsigned int channel);
extern unsigned int getCompletedVeneziaRpc(void);
#else
extern int getVeneziaPreferredClock(void);
extern int submitVeneziaRpc(unsigned int channel);
extern int waitVeneziaRpc(unsigned int channel);
extern unsigned int getCompletedVeneziaRpc(void);
#endif

int _callGenericThunk(unsigned int command, const unsigned int *input,
		     VeneziaRpcPack pack, VeneziaRpcUnpack unpack, unsigned int flags)
{
	VeneziaRpcThunk thunk;
	VeneziaRpcArg *arg;
	unsigned int channel, sequence;
	unsigned char completed;
	int intr, ret, allocated, clockDown = flags & 0x1000000;

#if FW_DEBUG
	SceDebugForDriver_35A35322(command < 0x200, SCE_DBGINFO_FUNC(693, "_callGenericThunk"),
			 __builtin_return_address(0));
#endif
	if (clockDown) {
		ret = sceKernelWaitSema(g_veneziaRpcState.semaphore, 1, NULL);
		if (ret < 0)
			return ret;
	}
	intr = SceCpuForDriver_D32ACE9E((SceKernelSpinlock *)&g_veneziaRpcState.lock);
	channel = (g_veneziaRpcState.lastChannel + 1) & 7;
	if (!g_veneziaRpcState.channels[channel].inUse)
		goto allocated;
	channel = (channel + 1) & 7;
	if (!g_veneziaRpcState.channels[channel].inUse)
		goto allocated;
	channel = (channel + 1) & 7;
	if (!g_veneziaRpcState.channels[channel].inUse)
		goto allocated;
	channel = (channel + 1) & 7;
	if (!g_veneziaRpcState.channels[channel].inUse)
		goto allocated;
	channel = (channel + 1) & 7;
	if (!g_veneziaRpcState.channels[channel].inUse)
		goto allocated;
	channel = (channel + 1) & 7;
	if (!g_veneziaRpcState.channels[channel].inUse)
		goto allocated;
	channel = (channel + 1) & 7;
	if (!g_veneziaRpcState.channels[channel].inUse)
		goto allocated;
	channel = (channel + 1) & 7;
	if (!g_veneziaRpcState.channels[channel].inUse)
		goto allocated;
	allocated = 0;
	goto allocationDone;
allocated:
	allocated = 1;
allocationDone:
	if (allocated) {
		g_veneziaRpcState.channels[channel].inUse = 1;
		g_veneziaRpcState.lastChannel = channel;
		if (clockDown && g_veneziaRpcState.freeChannels == 8) {
			ret = ScePervasiveForDriver_725B4F59(
				veneziaClockSelector(getVeneziaPreferredClock()));
#if FW_DEBUG
			SceDebugForDriver_35A35322(ret >= 0, SCE_DBGINFO_FUNC(604, "_allocRpcCh"),
					 __builtin_return_address(0));
#endif
		}
		g_veneziaRpcState.freeChannels--;
	} else {
		channel = -1;
	}
#if FW_DEBUG
	SceDebugForDriver_35A35322(allocated, SCE_DBGINFO_FUNC(614, "_allocRpcCh"),
			 __builtin_return_address(0));
#endif
	sceKernelSpinlockLowUnlockCpuResumeIntr((SceKernelSpinlock *)&g_veneziaRpcState.lock, intr);
	thunk.channel = channel;
	g_veneziaRpcState.channels[channel].flags = flags & 0xff0000;
	thunk.allocationCount = 0;
#if FW_DEBUG
	SceDebugForDriver_35A35322(channel <= 15, SCE_DBGINFO_FUNC(222, "_getRpcArg"),
			 __builtin_return_address(0));
#endif
	arg = g_veneziaRpcState.memory ? &g_veneziaRpcState.memory->arguments[channel] : NULL;
#if FW_DEBUG
	SceDebugForDriver_35A35322(arg != NULL, SCE_DBGINFO_FUNC(714, "_callGenericThunk"),
			 __builtin_return_address(0));
#endif
	sequence = g_veneziaRpcState.sequence;
	arg->command = command | g_veneziaRpcState.channels[thunk.channel].flags;
	g_veneziaRpcState.sequence = sequence + 1;
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
	arg->pid = sceKernelGetProcessId();
	if (g_veneziaRpcTrace)
		g_veneziaRpcTrace(0, 12, thunk.channel, command, input[0], input[1],
			input[2], input[3], arg->arguments[0], arg->arguments[1],
			arg->arguments[2], arg->arguments[3], flags, arg->pid);
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
	if (g_veneziaRpcTrace)
		g_veneziaRpcTrace(1, 4, thunk.channel, arg->command, arg->result, arg->pid);
	if (!(g_veneziaRpcState.channels[thunk.channel].flags & 0x20000)) {
		_freeRpcCh(thunk.channel, clockDown);
		if (clockDown)
			sceKernelSignalSema(g_veneziaRpcState.semaphore, 1);
	}
	if (g_veneziaRpcState.channels[thunk.channel].flags & 0x40000) {
		completed = getCompletedVeneziaRpc();
#if FW_DEBUG
		if (clockDown) {
			if (completed & (1U << 0)) {
				_freeRpcCh(0, clockDown);
				sceKernelSignalSema(g_veneziaRpcState.semaphore, 1);
			}
			if (completed & (1U << 1)) {
				_freeRpcCh(1, clockDown);
				sceKernelSignalSema(g_veneziaRpcState.semaphore, 1);
			}
			if (completed & (1U << 2)) {
				_freeRpcCh(2, clockDown);
				sceKernelSignalSema(g_veneziaRpcState.semaphore, 1);
			}
			if (completed & (1U << 3)) {
				_freeRpcCh(3, clockDown);
				sceKernelSignalSema(g_veneziaRpcState.semaphore, 1);
			}
			if (completed & (1U << 4)) {
				_freeRpcCh(4, clockDown);
				sceKernelSignalSema(g_veneziaRpcState.semaphore, 1);
			}
			if (completed & (1U << 5)) {
				_freeRpcCh(5, clockDown);
				sceKernelSignalSema(g_veneziaRpcState.semaphore, 1);
			}
			if (completed & (1U << 6)) {
				_freeRpcCh(6, clockDown);
				sceKernelSignalSema(g_veneziaRpcState.semaphore, 1);
			}
			if (completed & (1U << 7)) {
				_freeRpcCh(7, clockDown);
				sceKernelSignalSema(g_veneziaRpcState.semaphore, 1);
			}
		} else {
			if (completed & (1U << 0))
				_freeRpcCh(0, 0);
			if (completed & (1U << 1))
				_freeRpcCh(1, 0);
			if (completed & (1U << 2))
				_freeRpcCh(2, 0);
			if (completed & (1U << 3))
				_freeRpcCh(3, 0);
			if (completed & (1U << 4))
				_freeRpcCh(4, 0);
			if (completed & (1U << 5))
				_freeRpcCh(5, 0);
			if (completed & (1U << 6))
				_freeRpcCh(6, 0);
			if (completed & (1U << 7))
				_freeRpcCh(7, 0);
		}
#else
		for (unsigned int i = 0; i < 8; i++) {
			if (completed & (1U << i)) {
				_freeRpcCh(i, clockDown);
				if (clockDown)
					sceKernelSignalSema(g_veneziaRpcState.semaphore, 1);
			}
		}
#endif
	}
	return ret;
}

/* Sony venezia/dsp0_rpc.c. */
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

/* Sony's callers call it whole: not split after its first test */
int __attribute__((noinline))
_callDebugThunk(unsigned int command, const unsigned int *input, VeneziaRpcPack pack,
		VeneziaRpcUnpack unpack)
{
	VeneziaRpcThunk thunk;
	VeneziaRpcArg *arg;
	unsigned int channel, sequence;
	int intr, ret;
#if FW_DEBUG
	int allocated = 0;
#endif

#if FW_DEBUG
	SceDebugForDriver_35A35322(command <= 31, SCE_DBGINFO_FUNC(1062, "_callDebugThunk"),
			 __builtin_return_address(0));
#endif
	ret = sceKernelWaitSema(g_veneziaRpcState.debugSemaphore, 1, NULL);
	if (ret < 0)
		return ret;
	channel = -1;
	intr = SceCpuForDriver_D32ACE9E(&g_veneziaRpcState.debugLock);
	if (!g_veneziaRpcState.channels[8].inUse) {
		channel = 8;
#if FW_DEBUG
		allocated = 1;
#endif
		g_veneziaRpcState.channels[8].inUse = 1;
		g_veneziaRpcState.freeDebugChannels--;
	}
#if FW_DEBUG
	SceDebugForDriver_35A35322(allocated,
			 SCE_DBGINFO_FUNC(1013, "_allocRpcChDebug"),
			 __builtin_return_address(0));
#endif
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_veneziaRpcState.debugLock, intr);
	thunk.channel = channel;
	g_veneziaRpcState.channels[channel].flags = 0x10000;
	thunk.allocationCount = 0;
#if FW_DEBUG
	SceDebugForDriver_35A35322(allocated, SCE_DBGINFO_FUNC(222, "_getRpcArg"),
			 __builtin_return_address(0));
#endif
	arg = g_veneziaRpcState.memory ? &g_veneziaRpcState.memory->arguments[channel] : NULL;
#if FW_DEBUG
	SceDebugForDriver_35A35322(arg != NULL, SCE_DBGINFO_FUNC(1083, "_callDebugThunk"),
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
	arg->pid = sceKernelGetProcessId();
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
	SceDebugForDriver_35A35322(channel == 8, SCE_DBGINFO_FUNC(1025, "_freeRpcChDebug"),
			 __builtin_return_address(0));
#endif
	intr = SceCpuForDriver_D32ACE9E(&g_veneziaRpcState.debugLock);
	g_veneziaRpcState.channels[channel].inUse = 0;
	g_veneziaRpcState.freeDebugChannels++;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&g_veneziaRpcState.debugLock, intr);
	sceKernelSignalSema(g_veneziaRpcState.debugSemaphore, 1);
	return ret;
}

/* Sony venezia/dsp0_rpc.c: RPC initialization. */
extern void (*g_veneziaRpcTrace)(int event, int count, ...);
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
	ret = SceSysmemForDriver_C94850C9("SceVeneziaSpram", 0x20108006, 0x20000, &opt);
	if (ret < 0)
		return ret;
	g_veneziaRpcState.memoryBlock = ret;
	SceSysmemForDriver_A841EDDA(ret, &base);
	g_veneziaRpcState.memory = base;
	ret = initVeneziaRpcCommunicationMemory();
	if (ret < 0)
		return ret;
	ret = initVeneziaRpcDma();
	if (ret < 0)
		return ret;
	ret = sceKernelCreateSema("SceCodecEngineDsp0Rpc", 0x2000, 8, 8, NULL);
	if (ret < 0)
		return ret;
	g_veneziaRpcState.semaphore = ret;
	ret = sceKernelCreateSema("SceCodecEngineDsp0RpcDebug", 0x2000, 1, 1, NULL);
	if (ret < 0)
		return ret;
	g_veneziaRpcState.debugSemaphore = ret;
	ret = initVeneziaMailbox();
	if (ret < 0)
		return ret;
	g_veneziaRpcTrace = (void (*)(int, int, ...))SceSysrootForDriver_49843C16();
	return 0;
}
#if FW_VERSION >= 0x04000000
extern int veneziaClockSelector(int frequency);
#else
extern int veneziaClockSelector(int frequency);
#endif

int _sceVeneziaRpcClockDown(void)
{
	int intr, ret;

	intr = SceCpuForDriver_D32ACE9E(
		(SceKernelSpinlock *)&g_veneziaRpcState.lock);
	if (g_veneziaRpcState.freeChannels == 8 && getVeneziaClockLockCount() == 0) {
		ret = ScePervasiveForDriver_725B4F59(veneziaClockSelector(41));
#if FW_DEBUG
		SceDebugForDriver_35A35322(ret >= 0, SCE_DBGINFO_FUNC(555, "_sceVeneziaRpcClockDown"),
				 __builtin_return_address(0));
#endif
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(
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

	ret = sceKernelWaitSema(g_veneziaRpcState.semaphore, 8, NULL);
	debugRet = sceKernelWaitSema(g_veneziaRpcState.debugSemaphore, 1, NULL);
	if (debugRet < 0)
		return debugRet;
	return ret < 0 ? ret : 0;
}

/* name unknown: release all normal and debug RPC channel permits. */
int signalVeneziaRpcSemas(void)
{
	int ret, debugRet;

	ret = sceKernelSignalSema(g_veneziaRpcState.semaphore, 8);
	debugRet = sceKernelSignalSema(g_veneziaRpcState.debugSemaphore, 1);
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

/* Sony venezia/dsp0_rpc.c: RPC history and diagnostics. */
static inline VeneziaRpcArg *_getRpcArg(unsigned int channel)
{
#if FW_DEBUG
	SceDebugForDriver_35A35322(channel <= 15, SCE_DBGINFO_FUNC(222, "_getRpcArg"),
			 __builtin_return_address(0));
#endif
	return g_veneziaRpcState.memory ? &g_veneziaRpcState.memory->arguments[channel] : NULL;
}

int _sceVeneziaRpcGetHistory(VeneziaRpcHistoryEntry *history)
{
	unsigned int count, i, channel, previous;
	VeneziaRpcArg *arg, *previousArg;

#if FW_DEBUG
	SceDebugForDriver_35A35322(history != NULL, SCE_DBGINFO_FUNC(923, "_sceVeneziaRpcGetHistory"),
			 __builtin_return_address(0));
#endif
	if (!g_veneziaRpcState.memory)
		return 0;
	unsigned int channels[8] __attribute__((aligned(8))) = {0, 1, 2, 3, 4, 5, 6, 7};
	count = 8;
	do {
		for (i = 1; i < count; i++) {
			channel = channels[i];
			arg = _getRpcArg(channel);
			previous = channels[i - 1];
			previousArg = _getRpcArg(previous);
			if (arg->sequence < previousArg->sequence) {
				channels[i - 1] = channel;
				channels[i] = previous;
			}
		}
	} while (--count != 2);
	for (i = 0; i < 8; i++) {
		channel = channels[i];
		arg = _getRpcArg(channel);
#if FW_DEBUG
		SceDebugForDriver_35A35322(arg != NULL, SCE_DBGINFO_FUNC(946, "_sceVeneziaRpcGetHistory"),
				 __builtin_return_address(0));
#endif
		history[i].resultFlags = arg->resultFlags;
		history[i].pid = arg->pid;
		history[i].channel = channel;
		history[i].command = arg->command;
		history[i].arguments[0] = arg->arguments[0];
		history[i].arguments[1] = arg->arguments[1];
		history[i].arguments[2] = arg->arguments[2];
		history[i].arguments[3] = arg->arguments[3];
		history[i].result = arg->result;
	}
	return 0;
}

void _sceVeneziaRpcPrintBlockingCalls(SceUID pid)
{
#if FW_DEBUG
	VeneziaRpcArg *arg;
	unsigned int channel;

	if (!g_veneziaRpcState.memory)
		return;
	for (channel = 0; channel < 8; channel++) {
		arg = _getRpcArg(channel);
		SceDebugForDriver_35A35322(arg != NULL,
			 SCE_DBGINFO_FUNC(969, "_sceVeneziaRpcPrintBlockingCalls"),
			 __builtin_return_address(0));
		if (!(arg->resultFlags & 2) && arg->pid == pid)
			SCE_DBG_PRINTF_FUNC(0, 975, "_sceVeneziaRpcPrintBlockingCalls",
				"Warning: Codec Engine is waiting for suspend... (0x%08X, %2u, %3u)\n",
				pid, channel, arg->command & 0x1ff);
	}
#endif
}

/* name unknown: invoke the dedicated DSP debug channel. */
int callVeneziaDebugRpcThunk(unsigned int command, const unsigned int *input,
			    VeneziaRpcPack pack, VeneziaRpcUnpack unpack)
{
	return _callDebugThunk(command, input, pack, unpack);
}

/* name unknown: invoke the dedicated DSP debug channel. */
int callVeneziaDebugRpc(unsigned int command, unsigned int argument0,
		unsigned int argument1, unsigned int argument2, unsigned int argument3)
{
	unsigned int arguments[4] __attribute__((aligned(8)));

	arguments[0] = argument0;
	arguments[1] = argument1;
	arguments[2] = argument2;
	arguments[3] = argument3;
	return _callDebugThunk(command, arguments, NULL, NULL);
}

#if FW_DEBUG
/* name unknown: debug channel permits available. */
unsigned int getRpcFreeDebugChannelCount(void)
{
	return g_veneziaRpcState.freeDebugChannels;
}
#endif

#if FW_VERSION >= 0x04000000
extern int finalizeVeneziaRpcCommunicationMemory(void);
#else
extern int finalizeVeneziaRpcCommunicationMemory(void);
#endif

int _sceVeneziaStoreContext(void)
{
	if (g_veneziaRpcState.memory)
		memcpy(g_veneziaRpcState.savedCommunicationMemory,
		       g_veneziaRpcState.memory, sizeof(VeneziaRpcMemory));
	return finalizeVeneziaRpcCommunicationMemory();
}

#if FW_VERSION >= 0x04000000
extern int enableVeneziaMailboxInterrupts(void);
extern int initVeneziaRpcCommunicationMemory(void);
#else
extern int enableVeneziaMailboxInterrupts(void);
extern int initVeneziaRpcCommunicationMemory(void);
#endif

int _sceVeneziaLoadContext(void)
{
	int ret;

	ret = enableVeneziaMailboxInterrupts();
	if (ret >= 0) {
		ret = initVeneziaRpcCommunicationMemory();
		if (ret >= 0 && g_veneziaRpcState.memory)
			memcpy(g_veneziaRpcState.memory,
			       g_veneziaRpcState.savedCommunicationMemory, sizeof(VeneziaRpcMemory));
	}
	return ret;
}
