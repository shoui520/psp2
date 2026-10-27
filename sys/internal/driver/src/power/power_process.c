/*
 * ScePower: per-process clock requests, idle timers and wireless flags.
 */

#include "power_private.h"
#include <sysmem/uid_guid.h>

static inline __attribute__((always_inline)) int powerGpuLimitOverride(void);

static inline __attribute__((always_inline))
int getProcessClockFrequency(SceUID pid, enum PowerProcessClock clock, int syscall);

static inline __attribute__((always_inline))
int getProcessWirelessState(SceUID pid, unsigned int view, int syscall);

static inline __attribute__((always_inline)) void updateGpuWirelessLimits(void)
{
	struct PowerProcessState *process;
	SceKernelIntrStatus intrState;
	uint32_t flags = 0;
	int maximum = 0, unlimited;

	powerLockClockControl();
	intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&s_powerIdleLock);
	for (process = s_powerProcessList; process; process = process->next) {
		if (!process->clockExcluded) {
			if (process->clockFrequency[POWER_PROCESS_CLOCK_GPU] > maximum)
				maximum = process->clockFrequency[POWER_PROCESS_CLOCK_GPU];
			flags |= process->wirelessFlags;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
	unlimited = powerGpuLimitOverride() || maximum < 166 || !(flags & 3);
	ScePowerForDriver_77027B6B(unlimited ? 0x10000 : 0xc4ec);
	powerNotifyWirelessLimit(unlimited);
	powerUnlockClockControl();
}

static int setProcessWirelessFlags(SceUID pid, uint32_t mask, uint32_t enabled)
{
	struct PowerProcessState *state;
	int result;

	if (enabled > 1)
		return SCE_POWER_ERROR_INVALID_VALUE;
	result = sceKernelGetRemoteKPLS(pid,
			s_processStorageKey, (void **)&state, 0);
	if (result < 0)
		return result;
	if (enabled)
		state->wirelessFlags |= mask;
	else
		state->wirelessFlags &= ~mask;
	powerLockClockControl();
	updateGpuWirelessLimits();
	powerNotifyProcessFlags(pid, state->wirelessFlags & 0x33);
	powerUnlockClockControl();
	return result;
}

static inline __attribute__((always_inline)) int powerGpuLimitOverride(void)
{
	return s_powerClockControl.gpuLimitOverride != 0;
}

void powerRecomputeClockFrequencies(void)
{
	struct PowerProcessState *process;
	SceKernelIntrStatus intrState;
	uint32_t flags = 0;
	int bus = 0, arm = 0, xbar = 0, gpu = 0;
	int unlimited;

	powerLockClockControl();
	intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&s_powerIdleLock);
	for (process = s_powerProcessList; process; process = process->next) {
		if (process->clockExcluded)
			continue;
		if (bus < process->clockFrequency[POWER_PROCESS_CLOCK_BUS])
			bus = process->clockFrequency[POWER_PROCESS_CLOCK_BUS];
		if (arm < process->clockFrequency[POWER_PROCESS_CLOCK_ARM])
			arm = process->clockFrequency[POWER_PROCESS_CLOCK_ARM];
		if (xbar < process->clockFrequency[POWER_PROCESS_CLOCK_GPU_XBAR])
			xbar = process->clockFrequency[POWER_PROCESS_CLOCK_GPU_XBAR];
		if (gpu < process->clockFrequency[POWER_PROCESS_CLOCK_GPU])
			gpu = process->clockFrequency[POWER_PROCESS_CLOCK_GPU];
		flags |= process->wirelessFlags;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
	scePowerSetBusClockFrequency(bus);
	scePowerSetArmClockFrequency(arm);
	scePowerSetGpuXbarClockFrequency(xbar);
	scePowerSetGpuClockFrequencyInternal(gpu, gpu);
	unlimited = powerGpuLimitOverride() || gpu < 166 || !(flags & 3);
	ScePowerForDriver_77027B6B(unlimited ? 0x10000 : 0xc4ec);
	powerNotifyWirelessLimit(unlimited);
	if (powerGpuLimitOverride())
		flags &= ~0x30U;
	powerNotifyProcessFlags(SCE_KERNEL_ALL_PROCESS_ID, flags);
	powerUnlockClockControl();
}

static inline __attribute__((always_inline)) void powerInitializeProcessClockFlags(uint32_t limitOverride)
{
	s_powerClockControl.gpuLimitOverride = limitOverride;
}

static inline __attribute__((always_inline)) void powerResetProcessClockFlags(void)
{
	s_powerClockControl.kernelClockReduced = 0;
}

int powerInitializeKernelProcessState(void)
{
	struct PowerProcessState *state;
	SceKernelIntrStatus intrState;
	uint32_t override;
	int result;

	s_powerProcessList = NULL;
	powerResetProcessClockFlags();
	override = sceKernelCheckDipsw(DIPSW_DEVELOPMENT_MODE) &&
		   sceKernelCheckDipsw(DIPSW_GPU_OVERCLOCK);
	powerInitializeProcessClockFlags(override);
	result = sceKernelGetRemoteKPLS(SCE_KERNEL_PROCESS_ID,
			s_processStorageKey, (void **)&state, 1);
	if (result < 0)
		return 0;
	state->clockExcluded = 0;
	state->powerLockCount = 0;
	state->wirelessFlags = 0;
	state->clockFrequency[POWER_PROCESS_CLOCK_GPU_XBAR] = 111;
	state->clockFrequency[POWER_PROCESS_CLOCK_GPU] = 111;
	state->pid = SCE_KERNEL_PROCESS_ID;
	state->clockFrequency[POWER_PROCESS_CLOCK_BUS] = 222;
	state->clockFrequency[POWER_PROCESS_CLOCK_ARM] = 333;
	powerInitializeProcessIdleState(state);
	intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&s_powerIdleLock);
	state->next = s_powerProcessList;
	s_powerProcessList = state;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
	return 0;
}

static inline __attribute__((always_inline)) int powerBeginKernelClockReduction(void)
{
	if (s_powerClockControl.kernelClockReduced)
		return 0;
	s_powerClockControl.kernelClockReduced = 1;
	return 1;
}

void powerCreateProcessState(SceUID pid)
{
	struct PowerProcessState *state;
	SceKernelIntrStatus intrState;
	int game;

	if (sceKernelGetRemoteKPLS(pid,
			s_processStorageKey, (void **)&state, 1) < 0)
		return;
	state->pid = pid;
	state->clockExcluded = 0;
	state->powerLockCount = 0;
	game = pid == SCE_KERNEL_PROCESS_ID ? 1 : sceSblACMgrIsGameProgram(pid);
	state->clockFrequency[POWER_PROCESS_CLOCK_BUS] = 222;
	state->clockFrequency[POWER_PROCESS_CLOCK_ARM] = 333;
	state->clockFrequency[POWER_PROCESS_CLOCK_GPU_XBAR] = 111;
	state->clockFrequency[POWER_PROCESS_CLOCK_GPU] = game ? 111 : 0;
	state->wirelessFlags = pid != SCE_KERNEL_PROCESS_ID && game ? 3 : 0;
	if (pid != SCE_KERNEL_PROCESS_ID && game)
		sceKernelSysrootDisableAutoClockDown();
	powerInitializeProcessIdleState(state);
	intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&s_powerIdleLock);
	state->next = s_powerProcessList;
	s_powerProcessList = state;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
	if (pid == SCE_KERNEL_PROCESS_ID)
		return;
	if (powerBeginKernelClockReduction() &&
	    sceKernelGetRemoteKPLS(SCE_KERNEL_PROCESS_ID,
			s_processStorageKey, (void **)&state, 0) >= 0) {
		state->clockFrequency[POWER_PROCESS_CLOCK_BUS] = 55;
		if (SceDisplayForDriver_8D9A1CCE(1, NULL))
			state->clockFrequency[POWER_PROCESS_CLOCK_BUS] = 222;
		state->clockFrequency[POWER_PROCESS_CLOCK_ARM] = 41;
		state->clockFrequency[POWER_PROCESS_CLOCK_GPU] = 41;
	}
	powerRecomputeClockFrequencies();
}

static inline __attribute__((always_inline)) void powerUnlinkProcessState(SceUID pid)
{
	struct PowerProcessState *state, *process;
	SceKernelIntrStatus intrState;

	if (sceKernelGetRemoteKPLS(pid,
			s_processStorageKey, (void **)&state, 0) < 0)
		return;
	intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&s_powerIdleLock);
	if (s_powerProcessList == state)
		s_powerProcessList = state->next;
	else {
		for (process = s_powerProcessList; process; process = process->next) {
			if (process->next == state) {
				process->next = state->next;
				break;
			}
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
}

void powerUnlinkKilledProcess(SceUID pid)
{
	powerUnlinkProcessState(pid);
}

void powerUnlinkExitedProcess(SceUID pid)
{
	powerUnlinkProcessState(pid);
}

void powerProcessSuspendEvent(SceUID pid, uint32_t event)
{
	struct PowerProcessState *state;

	if (event != 0x800 || sceKernelGetRemoteKPLS(pid,
			s_processStorageKey, (void **)&state, 0) < 0)
		return;
	state->clockExcluded = 1;
	if (sceSblACMgrIsGameProgram(pid))
		sceKernelSysrootEnableAutoClockDown();
	powerRecomputeClockFrequencies();
}

void powerProcessResumeEvent(SceUID pid, uint32_t event)
{
	struct PowerProcessState *state;

	if (event != 0x10000 || sceKernelGetRemoteKPLS(pid,
			s_processStorageKey, (void **)&state, 0) < 0)
		return;
	state->clockExcluded = 0;
	powerRecordProcessIdleTick(state);
	powerRecomputeClockFrequencies();
	if (sceSblACMgrIsGameProgram(pid))
		sceKernelSysrootDisableAutoClockDown();
}

static inline __attribute__((always_inline)) int setProcessClockFrequency(SceUID pid, int frequency,
				    enum PowerProcessClock lane, int syscall)
{
	struct PowerProcessState *state, *process;
	const int limits[] = { 222, 444, 166 };
	SceKernelIntrStatus intrState;
	uint32_t context = 0;
	int result, maximum = 0;

	if (syscall)
		context = sceSyscallEnter();
	if (frequency < 1 || frequency > limits[lane]) {
		result = SCE_POWER_ERROR_INVALID_VALUE;
		goto out;
	}
	result = sceKernelGetRemoteKPLS(pid,
			s_processStorageKey, (void **)&state, 0);
	if (result < 0)
		goto out;
	state->clockFrequency[lane] = frequency;
	/* The native caller deliberately ignores the outer lock result. */
	powerLockClockControl();
	intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&s_powerIdleLock);
	for (process = s_powerProcessList; process; process = process->next) {
		int requested = process->clockFrequency[lane];

		if (!process->clockExcluded && requested > maximum)
			maximum = requested;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
	if (lane == POWER_PROCESS_CLOCK_BUS)
		result = scePowerSetBusClockFrequency(maximum);
	else if (lane == POWER_PROCESS_CLOCK_ARM)
		result = scePowerSetArmClockFrequency(maximum);
	else
		result = scePowerSetGpuXbarClockFrequency(maximum);
	powerUnlockClockControl();
out:
	if (syscall)
		sceSyscallLeave(context);
	return result;
}

int ScePowerForDriver_20A33D58(SceUID pid, int frequency)
{
	return setProcessClockFrequency(pid, frequency, POWER_PROCESS_CLOCK_BUS, 0);
}

int scePowerSetBusClockFrequencyForUser(int frequency)
{
	return setProcessClockFrequency(0, frequency, POWER_PROCESS_CLOCK_BUS, 1);
}

/* Inlining keeps each public function's native protected-call boundary. */
static inline __attribute__((always_inline))
int getProcessClockFrequency(SceUID pid, enum PowerProcessClock clock, int syscall)
{
	struct PowerProcessState *state;
	uint32_t context = 0;
	int result;

	if (syscall)
		context = sceSyscallEnter();
	result = sceKernelGetRemoteKPLS(pid,
			s_processStorageKey, (void **)&state, 0);
	if (result >= 0)
		result = state->clockFrequency[clock];
	if (syscall)
		sceSyscallLeave(context);
	return result;
}

int ScePowerForDriver_2777A517(SceUID pid)
{
	return getProcessClockFrequency(pid,
			POWER_PROCESS_CLOCK_BUS, 0);
}

int scePowerGetBusClockFrequencyForUser(void)
{
	return getProcessClockFrequency(0,
			POWER_PROCESS_CLOCK_BUS, 1);
}

int ScePowerForDriver_86CB5218(SceUID pid, int frequency)
{
	return setProcessClockFrequency(pid, frequency, POWER_PROCESS_CLOCK_ARM, 0);
}

int scePowerSetArmClockFrequencyForUser(int frequency)
{
	return setProcessClockFrequency(0, frequency, POWER_PROCESS_CLOCK_ARM, 1);
}

int ScePowerForDriver_2E9000F7(SceUID pid)
{
	return getProcessClockFrequency(pid,
			POWER_PROCESS_CLOCK_ARM, 0);
}

int scePowerGetArmClockFrequencyForUser(void)
{
	return getProcessClockFrequency(0,
			POWER_PROCESS_CLOCK_ARM, 1);
}

int ScePowerForDriver_8D18F728(SceUID pid, int frequency)
{
	return setProcessClockFrequency(pid, frequency, POWER_PROCESS_CLOCK_GPU_XBAR, 0);
}

int scePowerSetGpuXbarClockFrequencyForUser(int frequency)
{
	return setProcessClockFrequency(0, frequency, POWER_PROCESS_CLOCK_GPU_XBAR, 1);
}

int ScePowerForDriver_62C5406C(SceUID pid)
{
	return getProcessClockFrequency(pid,
			POWER_PROCESS_CLOCK_GPU_XBAR, 0);
}

int scePowerGetGpuXbarClockFrequencyForUser(void)
{
	return getProcessClockFrequency(0,
			POWER_PROCESS_CLOCK_GPU_XBAR, 1);
}

static inline __attribute__((always_inline)) int setProcessGpuFrequency(SceUID pid, int frequency)
{
	struct PowerProcessState *state, *process;
	SceKernelIntrStatus intrState;
	int result, maximum = 0;

	if (frequency < 1 || frequency > 222)
		return SCE_POWER_ERROR_INVALID_VALUE;
	result = sceKernelGetRemoteKPLS(pid,
			s_processStorageKey, (void **)&state, 0);
	if (result < 0)
		return result;
	state->clockFrequency[POWER_PROCESS_CLOCK_GPU] = frequency;
	powerLockClockControl();
	intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&s_powerIdleLock);
	for (process = s_powerProcessList; process; process = process->next)
		if (!process->clockExcluded && process->clockFrequency[POWER_PROCESS_CLOCK_GPU] > maximum)
			maximum = process->clockFrequency[POWER_PROCESS_CLOCK_GPU];
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
	result = scePowerSetGpuClockFrequencyInternal(maximum, maximum);
	updateGpuWirelessLimits();
	powerUnlockClockControl();
	return result;
}

int ScePowerForDriver_B104EFE2(SceUID pid, int frequency)
{
	return setProcessGpuFrequency(pid, frequency);
}

int scePowerSetGpuClockFrequency(int frequency)
{
	uint32_t context = sceSyscallEnter();
	int result;

	if (!sceSblACMgrIsSystemProgram(0) && ((unsigned int)frequency - 1) >= 166)
		result = SCE_POWER_ERROR_INVALID_VALUE;
	else
		result = setProcessGpuFrequency(0, frequency);
	sceSyscallLeave(context);
	return result;
}

int ScePowerForDriver_642E0AF2(SceUID pid)
{
	return getProcessClockFrequency(pid,
			POWER_PROCESS_CLOCK_GPU, 0);
}

int scePowerGetGpuClockFrequency(void)
{
	return getProcessClockFrequency(0,
			POWER_PROCESS_CLOCK_GPU, 1);
}

int powerSetProcessConfigurationFlags(uint32_t mask, uint32_t enabled)
{
	struct PowerProcessState *state;
	volatile struct PowerProcessState *process;
	SceKernelIntrStatus intrState;
	uint32_t previous, current, flags = 0;

	if (enabled > 1)
		return SCE_POWER_ERROR_INVALID_VALUE;
	state = SceProcessmgrForDriver_EE694840(s_processStorageKey);
	if (!state)
		return SCE_POWER_ERROR_INVALID_VALUE;
	previous = state->wirelessFlags;
	if (enabled) {
		if (((mask & 0x10) && (previous & 1)) ||
		    ((mask & 0x20) && (previous & 2)))
			return SCE_POWER_ERROR_INVALID_VALUE;
		current = previous | mask;
	} else {
		current = previous & ~mask;
	}
	state->wirelessFlags = current;
	if (!((previous ^ current) & 0x30))
		return 0;
	powerLockClockControl();
	intrState = sceKernelSpinlockLowLockCpuSuspendIntr(&s_powerIdleLock);
	for (process = s_powerProcessList; process; process = process->next) {
		if (!process->clockExcluded)
			flags |= process->wirelessFlags;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
	if (powerGpuLimitOverride())
		flags &= ~0x30U;
	powerNotifyProcessFlags(SCE_KERNEL_ALL_PROCESS_ID, flags);
	powerUnlockClockControl();
	return 0;
}

static inline __attribute__((always_inline))
int getProcessWirelessState(SceUID pid, unsigned int view, int syscall)
{
	struct PowerProcessState *state;
	uint32_t context = 0, flags;
	int result;

	if (syscall)
		context = sceSyscallEnter();
	result = sceKernelGetRemoteKPLS(pid,
			s_processStorageKey, (void **)&state, 0);
	if (result >= 0) {
		flags = state->wirelessFlags;
		if (view == 2)
			result = !!(flags & 3);
		else
			result = (flags >> view) & 1;
	}
	if (syscall)
		sceSyscallLeave(context);
	return result;
}

/* Meanings of the two wireless flag bits remain unresolved. */
int ScePowerForDriver_4159E0D9(SceUID pid)
{
	return getProcessWirelessState(pid, 4, 0);
}

int ScePowerForDriver_5210A6FE(SceUID pid)
{
	return getProcessWirelessState(pid, 5, 0);
}

int ScePowerForDriver_4A69163A(SceUID pid, uint32_t enabled)
{
	return setProcessWirelessFlags(pid, 1, enabled);
}

int ScePowerForDriver_661D670D(SceUID pid, uint32_t enabled)
{
	return setProcessWirelessFlags(pid, 2, enabled);
}

int scePowerSetUsingWireless(uint32_t enabled)
{
	uint32_t context = sceSyscallEnter();
	int result = setProcessWirelessFlags(0, 3, enabled);

	sceSyscallLeave(context);
	return result;
}

int ScePowerForDriver_06AF03DB(SceUID pid)
{
	return getProcessWirelessState(pid, 2, 0);
}

int ScePowerForDriver_2784A6BD(SceUID pid)
{
	return getProcessWirelessState(pid, 0, 0);
}

int ScePowerForDriver_0D80B917(SceUID pid)
{
	return getProcessWirelessState(pid, 1, 0);
}

int scePowerGetUsingWireless(void)
{
	return getProcessWirelessState(0, 2, 1);
}

/* name unknown: the calling process's wireless flags, in its own syscall context */
static __attribute__((noinline)) int powerSetConfigurationWireless(uint32_t mask, uint32_t enabled)
{
	uint32_t context = sceSyscallEnter();
	int result = setProcessWirelessFlags(0, mask, enabled);

	sceSyscallLeave(context);
	return result;
}

int scePowerSetConfigurationMode(uint32_t mode)
{
	uint32_t context = sceSyscallEnter();
	int result = SCE_POWER_ERROR_INVALID_VALUE;

	switch (mode) {
	case 0x80:
		result = scePowerSetArmClockFrequencyForUser(333);
		if (result < 0)
			break;
		result = scePowerSetGpuClockFrequency(111);
		if (result < 0)
			break;
		result = scePowerSetGpuXbarClockFrequencyForUser(111);
		if (result < 0)
			break;
		powerSetProcessConfigurationFlags(0x30, 0);
		result = powerSetConfigurationWireless(3, 1);
		break;
	case 0x1080:
		result = scePowerSetArmClockFrequencyForUser(333);
		if (result < 0)
			break;
		powerSetProcessConfigurationFlags(0x30, 0);
		result = powerSetConfigurationWireless(3, 1);
		if (result < 0)
			break;
		result = scePowerSetGpuXbarClockFrequencyForUser(166);
		if (result < 0)
			break;
		result = scePowerSetGpuClockFrequency(111);
		break;
	case 0x800:
		result = powerSetConfigurationWireless(3, 0);
		if (result < 0)
			break;
		powerSetProcessConfigurationFlags(0x30, 1);
		result = scePowerSetArmClockFrequencyForUser(333);
		if (result < 0)
			break;
		result = scePowerSetGpuXbarClockFrequencyForUser(166);
		if (result >= 0)
			result = scePowerSetGpuClockFrequency(166);
		break;
	case 0x10880:
		result = scePowerSetArmClockFrequencyForUser(333);
		if (result < 0)
			break;
		powerSetProcessConfigurationFlags(0x30, 0);
		result = powerSetConfigurationWireless(3, 1);
		if (result < 0)
			break;
		goto setMediaClocks;
	case 0x10888:
		/* Native rejection leaves the shifted syscall context in place. */
		if (!SceSblAIMgrForDriver_71608CA3())
			return SCE_POWER_ERROR_INVALID_VALUE;
		powerSetProcessConfigurationFlags(0x30, 0);
		result = powerSetConfigurationWireless(3, 1);
		goto setHighClocks;
	case 0x10080:
		result = SceProcessmgrForDriver_879153D9(0, 0x20000);
		if (result < 0)
			break;
		if (result == 1) {
			result = SCE_POWER_ERROR_INVALID_VALUE;
			break;
		}
		result = powerSetConfigurationWireless(2, 0);
		if (result < 0)
			break;
		powerSetProcessConfigurationFlags(0x20, 1);
		powerSetProcessConfigurationFlags(0x10, 0);
		result = powerSetConfigurationWireless(1, 1);
setHighClocks:
		if (result < 0)
			break;
		result = scePowerSetArmClockFrequencyForUser(444);
		if (result < 0)
			break;
setMediaClocks:
		result = scePowerSetGpuXbarClockFrequencyForUser(166);
		if (result >= 0)
			result = scePowerSetGpuClockFrequency(166);
		break;
	}
	sceSyscallLeave(context);
	return result < 0 ? result : 0;
}
