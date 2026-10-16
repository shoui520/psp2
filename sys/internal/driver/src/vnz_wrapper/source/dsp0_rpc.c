/* Sony venezia/dsp0_rpc.c. */
#undef __FILE_NAME__
#define __FILE_NAME__ "venezia/dsp0_rpc.c"
#include <sce/debug.h>
#include <psp2kern/kernel/threadmgr/semaphores.h>
#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include "venezia_internal.h"

VeneziaRpcState g_veneziaRpcState;

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

#if !FW_DEBUG
static inline
#endif
int _freeRpcCh(unsigned int channel, int clockDown)
{
	VeneziaRpcArg *arg;
	int intr, ret;
	unsigned int command;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr((SceKernelSpinlock *)&g_veneziaRpcState.lock);
	g_veneziaRpcState.channels[channel].inUse = 0;
	g_veneziaRpcState.freeChannels++;
#if FW_DEBUG
	ksceKernelAssert(channel <= 15, SCE_DBGINFO_FUNC(222, "_getRpcArg"),
			 __builtin_return_address(0));
#endif
	arg = g_veneziaRpcState.memory ? &g_veneziaRpcState.memory->arguments[channel] : NULL;
#if FW_DEBUG
	ksceKernelAssert(arg != NULL, SCE_DBGINFO_FUNC(637, "_freeRpcCh"),
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
		ksceKernelAssert(ret >= 0, SCE_DBGINFO_FUNC(650, "_freeRpcCh"),
				 __builtin_return_address(0));
#endif
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr((SceKernelSpinlock *)&g_veneziaRpcState.lock, intr);
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
	ksceKernelAssert(command < 0x200, SCE_DBGINFO_FUNC(693, "_callGenericThunk"),
			 __builtin_return_address(0));
#endif
	if (clockDown) {
		ret = ksceKernelWaitSema(g_veneziaRpcState.semaphore, 1, NULL);
		if (ret < 0)
			return ret;
	}
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr((SceKernelSpinlock *)&g_veneziaRpcState.lock);
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
			ksceKernelAssert(ret >= 0, SCE_DBGINFO_FUNC(604, "_allocRpcCh"),
					 __builtin_return_address(0));
#endif
		}
		g_veneziaRpcState.freeChannels--;
	} else {
		channel = -1;
	}
#if FW_DEBUG
	ksceKernelAssert(allocated, SCE_DBGINFO_FUNC(614, "_allocRpcCh"),
			 __builtin_return_address(0));
#endif
	ksceKernelSpinlockLowUnlockCpuResumeIntr((SceKernelSpinlock *)&g_veneziaRpcState.lock, intr);
	thunk.channel = channel;
	g_veneziaRpcState.channels[channel].flags = flags & 0xff0000;
	thunk.allocationCount = 0;
#if FW_DEBUG
	ksceKernelAssert(channel <= 15, SCE_DBGINFO_FUNC(222, "_getRpcArg"),
			 __builtin_return_address(0));
#endif
	arg = g_veneziaRpcState.memory ? &g_veneziaRpcState.memory->arguments[channel] : NULL;
#if FW_DEBUG
	ksceKernelAssert(arg != NULL, SCE_DBGINFO_FUNC(714, "_callGenericThunk"),
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
	arg->pid = ksceKernelGetProcessId();
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
			ksceKernelSignalSema(g_veneziaRpcState.semaphore, 1);
	}
	if (g_veneziaRpcState.channels[thunk.channel].flags & 0x40000) {
		completed = getCompletedVeneziaRpc();
#if FW_DEBUG
		if (clockDown) {
			if (completed & (1U << 0)) {
				_freeRpcCh(0, clockDown);
				ksceKernelSignalSema(g_veneziaRpcState.semaphore, 1);
			}
			if (completed & (1U << 1)) {
				_freeRpcCh(1, clockDown);
				ksceKernelSignalSema(g_veneziaRpcState.semaphore, 1);
			}
			if (completed & (1U << 2)) {
				_freeRpcCh(2, clockDown);
				ksceKernelSignalSema(g_veneziaRpcState.semaphore, 1);
			}
			if (completed & (1U << 3)) {
				_freeRpcCh(3, clockDown);
				ksceKernelSignalSema(g_veneziaRpcState.semaphore, 1);
			}
			if (completed & (1U << 4)) {
				_freeRpcCh(4, clockDown);
				ksceKernelSignalSema(g_veneziaRpcState.semaphore, 1);
			}
			if (completed & (1U << 5)) {
				_freeRpcCh(5, clockDown);
				ksceKernelSignalSema(g_veneziaRpcState.semaphore, 1);
			}
			if (completed & (1U << 6)) {
				_freeRpcCh(6, clockDown);
				ksceKernelSignalSema(g_veneziaRpcState.semaphore, 1);
			}
			if (completed & (1U << 7)) {
				_freeRpcCh(7, clockDown);
				ksceKernelSignalSema(g_veneziaRpcState.semaphore, 1);
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
					ksceKernelSignalSema(g_veneziaRpcState.semaphore, 1);
			}
		}
#endif
	}
	return ret;
}
