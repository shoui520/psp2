/*
 * ScePower: idle timers and the vblank tick.
 */

#include "power_private.h"
#include <sysmem/uid_guid.h>
#include <power/power.h>

/* exported, and inlined in this unit as in the original */

struct PowerIdleState s_powerIdleTimers;

struct PowerProcessState *volatile s_powerProcessList;
int powerEnableIdleVblank(uint32_t kind, void *argument)
{
	s_powerIdleTimers.tickSnapshot = sceKernelGetSystemTimeWide();
	sceKernelEnableSubIntr(8, 0x1a);
	return 0;
}

static inline __attribute__((always_inline)) void powerHandleProcessIdleTimers(uint64_t now)
{
	struct PowerProcessState *state;
	PowerProcessIdleCallback callback;
	SceKernelIntrStatus intrState;
	uint64_t interval;
	uint64_t elapsed;
	uint32_t idle;

	intrState = SceCpuForDriver_D32ACE9E(&s_powerIdleLock);
	for (state = s_powerProcessList; state; state = state->next) {
		idle = state->idleState;
		if (idle != 2) {
			if (state->tickRecorded) {
				state->tickRecorded = 0;
				if (idle) {
					callback = s_powerIdleTimers.processCallback.callback;
					state->idleState = 0;
					if (callback)
						callback(state->pid, 0,
							 s_powerIdleTimers.processCallback.argument);
				}
				continue;
			}
			if (idle == 1)
				continue;
		} else {
			state->tickRecorded = 0;
		}
		interval = state->idleInterval;
		if (!interval)
			interval = s_powerIdleTimers.timers[0].interval;
		if (!interval)
			continue;
		elapsed = now - state->lastIdleTick;
		if (idle == 2 || elapsed >= interval) {
			callback = s_powerIdleTimers.processCallback.callback;
			state->idleState = 1;
			if (callback)
				callback(state->pid, (uint32_t)elapsed,
					 s_powerIdleTimers.processCallback.argument);
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
}

int powerIdleVblankHandler(void *subintrArgument, void *registerArgument,
			   unsigned char priority)
{
	volatile struct PowerIdleTimer *timer;
	PowerIdleCallback callback;
	uint64_t now = sceKernelGetSystemTimeWide();
	uint64_t interval;
	uint64_t elapsed;
	uint32_t index;
	uint8_t state;

	(void)subintrArgument;
	(void)registerArgument;
	(void)priority;
	s_powerIdleTimers.tickSnapshot = now;
	for (index = 0; index < 8; index++) {
		timer = &s_powerIdleTimers.timers[index];
		callback = timer->callback;
		if (!callback)
			continue;
		state = timer->idleState;
		if (state != 2) {
			if (timer->tickRecorded) {
				timer->tickRecorded = 0;
				if (state) {
					timer->idleState = 0;
					callback(index, 0, timer->callbackArgument, state);
				}
				continue;
			}
			if (state == 1)
				continue;
		} else {
			timer->tickRecorded = 0;
		}
		interval = timer->interval;
		if (!interval)
			continue;
		elapsed = now - timer->lastTick;
		if (state == 2 || elapsed >= interval) {
			timer->idleState = 1;
			callback(index, (uint32_t)elapsed,
				 timer->callbackArgument, elapsed >> 32);
		}
	}
	powerHandleProcessIdleTimers(now);
	return -1;
}

int powerInitializeIdleControl(void)
{
	memset((void *)&s_powerIdleTimers, 0, sizeof(s_powerIdleTimers));
	s_powerIdleTimers.tickSnapshot = sceKernelGetSystemTimeWide();
	sceKernelRegisterSubIntrHandler(8, 0x1a, "ScePowerVblank",
				       powerIdleVblankHandler, NULL);
	sceKernelRegisterInitCallback(powerEnableIdleVblank, 8, 0, NULL);
	return 0;
}

static inline __attribute__((always_inline)) void powerTickProcess(void)
{
	struct PowerProcessState *state;

	if (SceIntrmgrForDriver_182EE3E3())
		return;
	state = SceProcessmgrForDriver_EE694840(s_processStorageKey);
	if (!state || state->tickRecorded)
		return;
	state->tickRecorded = 1;
	state->lastIdleTick = s_powerIdleTimers.tickSnapshot;
}

int ScePowerForDriver_EFD3C963(uint32_t mask)
{
	volatile struct PowerIdleTimer *timer;
	uint32_t selected = mask;
	uint32_t index;
	uint64_t snapshot;

	/* Expand from the incoming selection once, rather than transitively. */
	if (mask) {
		for (index = 0; index < 8; index++) {
			if (mask & (1U << index))
				selected |= s_powerIdleTimers.timers[index].propagationMask;
		}
	}
	for (index = 0; index < 8; index++) {
		timer = &s_powerIdleTimers.timers[index];
		if (mask) {
			if (!(selected & (1U << index)))
				continue;
		} else if (timer->tickFlags & (1U << 9)) {
			continue;
		}
		if ((timer->tickFlags & (1U << 10)) &&
		    !powerTickInternalPredicate() && !ScePowerForDriver_394DE492())
			continue;
		if (!timer->tickRecorded) {
			snapshot = s_powerIdleTimers.tickSnapshot;
			timer->tickRecorded = 1;
			timer->lastTick = snapshot;
		}
	}
	if (!mask || (selected & 1))
		powerTickProcess();
	return 0;
}

int powerTickProcessForPid(SceUID pid, uint32_t mask)
{
	struct PowerProcessState *state;
	int result;

	if (mask && !(mask & 1))
		return SCE_POWER_ERROR_INVALID_VALUE;
	result = SceProcessmgrForDriver_AF80F39C(pid,
			s_processStorageKey, (void **)&state, 0);
	if (result < 0)
		return result;
	if (!state->tickRecorded) {
		state->tickRecorded = 1;
		state->lastIdleTick = s_powerIdleTimers.tickSnapshot;
	}
	return 0;
}

int ScePowerForDriver_EDC13FE5(uint32_t timerId, uint64_t *elapsedOut,
			 uint64_t *remainingOut)
{
	volatile struct PowerIdleTimer *timer;
	uint64_t elapsed;

	if (timerId > 7)
		return SCE_POWER_ERROR_INVALID_VALUE;

	timer = &s_powerIdleTimers.timers[timerId];
	elapsed = sceKernelGetSystemTimeWide() - timer->lastTick;
	if (elapsedOut)
		*elapsedOut = elapsed;
	if (remainingOut)
		*remainingOut = timer->interval - elapsed;
	return 0;
}

inline __attribute__((always_inline)) int ScePowerForDriver_6BC26FC7(uint32_t timerId, uint64_t interval)
{
	volatile struct PowerIdleTimer *timer;
	uint32_t mask;
	uint32_t index;

	if (timerId > 7)
		return SCE_POWER_ERROR_INVALID_VALUE;

	timer = &s_powerIdleTimers.timers[timerId];
	if (!timer->callback)
		return SCE_POWER_ERROR_INVALID_VALUE;

	timer->interval = interval;
	mask = timer->propagationMask;
	for (index = 0; index < 8; index++) {
		if (index != timerId && (mask & (1U << index)))
			s_powerIdleTimers.timers[index].interval = interval;
	}
	return 0;
}

/* Force expiry on the next vblank when the timer is enabled. */
int ScePowerForDriver_660D5AB4(uint32_t timerId)
{
	volatile struct PowerIdleTimer *timer;

	if (timerId > 7)
		return SCE_POWER_ERROR_INVALID_VALUE;

	timer = &s_powerIdleTimers.timers[timerId];
	if (timer->interval)
		timer->idleState = 2;
	return 0;
}

int ScePowerForDriver_991BDEAF(uint32_t timerId, uint32_t mask)
{
	volatile struct PowerIdleTimer *timer;

	if (timerId > 7)
		return SCE_POWER_ERROR_INVALID_VALUE;

	timer = &s_powerIdleTimers.timers[timerId];
	timer->propagationMask |= mask;
	return 0;
}

/* A recorded tick clears the idle predicate until the next vblank. */
uint32_t powerIsIdleTimer(uint32_t timerId)
{
	volatile struct PowerIdleTimer *timer;

	if (timerId > 7)
		return SCE_POWER_ERROR_INVALID_VALUE;
	timer = &s_powerIdleTimers.timers[timerId];
	if (timer->tickRecorded)
		return 0;
	return timer->idleState != 0;
}

int ScePowerForDriver_1BA2FCAE(uint32_t timerId, uint32_t tickFlags,
			     uint64_t interval, PowerIdleCallback callback,
			     void *argument)
{
	volatile struct PowerIdleTimer *timer;
	SceKernelIntrStatus context;

	if (timerId > 7)
		return SCE_POWER_ERROR_INVALID_VALUE;

	timer = &s_powerIdleTimers.timers[timerId];
	context = SceCpuForDriver_D32ACE9E(&s_powerIdleLock);
	if (callback && timer->callback) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock,
						      context);
		return SCE_POWER_ERROR_ALREADY_REGISTERD;
	}
	timer->tickFlags = tickFlags;
	timer->lastTick = sceKernelGetSystemTimeWide();
	timer->interval = interval;
	timer->callback = callback;
	timer->callbackArgument = argument;
	timer->propagationMask = 0;
	timer->tickRecorded = 0;
	timer->idleState = 0xff;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, context);
	return 0;
}

int ScePowerForDriver_0856FD0A(PowerProcessIdleCallback callback, void *argument)
{
	s_powerIdleTimers.processCallback.callback = callback;
	s_powerIdleTimers.processCallback.argument = argument;
	return 0;
}

int ScePowerForDriver_FB5C3C75(SceUID pid, uint32_t unused,
				    uint64_t interval)
{
	struct PowerProcessState *state;
	SceKernelIntrStatus context;
	int result;

	(void)unused;
	if (pid == SCE_KERNEL_ALL_PROCESS_ID)
		return ScePowerForDriver_6BC26FC7(0, interval);
	result = SceProcessmgrForDriver_AF80F39C(pid,
			s_processStorageKey, (void **)&state, 0);
	if (result < 0)
		return result;
	context = SceCpuForDriver_D32ACE9E(&s_powerIdleLock);
	state->idleInterval = interval;
	state->lastIdleTick = sceKernelGetSystemTimeWide();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, context);
	return 0;
}

int scePowerSetIdleTimerCount(uint64_t interval)
{
	struct PowerProcessState *state;
	SceKernelIntrStatus intrState;
	uint32_t context;
	int result;

	context = sceSyscallEnter();
	result = SceProcessmgrForDriver_AF80F39C(0,
			s_processStorageKey, (void **)&state, 0);
	if (result >= 0) {
		intrState = SceCpuForDriver_D32ACE9E(&s_powerIdleLock);
		state->idleInterval = interval;
		state->lastIdleTick = sceKernelGetSystemTimeWide();
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
		result = 0;
	}
	sceSyscallLeave(context);
	return result;
}

/* The export name is unresolved; the process word records idle state. */
int ScePowerForDriver_C715F4F0(SceUID pid)
{
	struct PowerProcessState *state;
	int result;

	if (pid == SCE_KERNEL_ALL_PROCESS_ID)
		return powerIsIdleTimer(0);
	result = SceProcessmgrForDriver_AF80F39C(pid,
			s_processStorageKey, (void **)&state, 0);
	if (result >= 0)
		result = state->idleState != 0;
	return result;
}

void powerInitializeProcessIdleState(struct PowerProcessState *state)
{
	state->idleInterval = 0;
	state->lastIdleTick = sceKernelGetSystemTimeWide();
	state->tickRecorded = 0;
	state->idleState = 0;
}

void powerRecordProcessIdleTick(struct PowerProcessState *state)
{
	state->lastIdleTick = sceKernelGetSystemTimeWide();
	state->tickRecorded = 1;
}
