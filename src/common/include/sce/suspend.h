/*
 * SceKernelSuspendForDriver (os0:kd/sysmem.skprx), as used so far.
 */

#pragma once

#include <psp2kern/types.h>

int SceKernelSuspendForDriver_4DF40893(int argument);
int SceKernelSuspendForDriver_2BB92967(int argument);

/* names unknown (NID): register a handler for suspend / resume event idx */
SceUID SceKernelSuspendForDriver_CE7A2207(SceUInt32 idx, int (*handler)(void *args), void *args);
SceUID SceKernelSuspendForDriver_105C5752(SceUInt32 idx, int (*handler)(void *args), void *args);

/*
 * The power service's side of SceKernelSuspendForDriver: power registers it
 * (SceKernelSuspendForDriver_250ACD90) and sysmem's exports call through it.
 * The names are ours; the functions power puts in each slot are noted.
 */
/* name unknown: an idle timer's handler; elapsed ticks are split around the argument */
typedef void (*PowerIdleCallback)(SceUInt32 timerId, SceUInt32 elapsed, void *argument,
				  SceUInt32 elapsedHigh);

typedef struct SuspendPowerCallbacks {
	SceSize size;
	int (*lockSuspend)(SceUInt32 kind);		/* powerAcquireSystemSwitchLock */
	int (*unlockSuspend)(SceUInt32 kind);		/* powerReleaseSystemSwitchLock */
	int (*powerLock)(SceUInt32 kind);		/* powerAcquireProcessSwitchLock */
	int (*powerUnlock)(SceUInt32 kind);		/* powerReleaseProcessSwitchLock */
	int (*requestStandby)(void);
	int (*requestSuspend)(void);
	int (*requestColdReset)(void);
	int (*requestShutdown)(SceUInt32 argument);	/* 0x20 */
	int (*powerTick)(SceUInt32 mask);
	int (*tickProcess)(SceUID pid, SceUInt32 mask);
	int (*setIdleCallback)(SceUInt32 timerId, SceUInt32 tickFlags, SceUInt64 interval,
			       PowerIdleCallback callback, void *argument);
	int (*expireIdleTimer)(SceUInt32 timerId);	/* 0x30: ScePowerForDriver_660D5AB4 */
	int (*addIdleTimerMask)(SceUInt32 timerId, SceUInt32 mask);	/* ScePowerForDriver_991BDEAF */
} SuspendPowerCallbacks;

_Static_assert(sizeof(SuspendPowerCallbacks) == 0x38, "SuspendPowerCallbacks size");

/* names unknown (NID): power requests (os0:kd/deci4p_sdfctl: from the host) */
int SceKernelSuspendForDriver_250ACD90(const SuspendPowerCallbacks *callbacks);	/* power registers its callbacks */
int SceKernelSuspendForDriver_D4958E6F(int value);
int SceKernelSuspendForDriver_FE2118BD(int value);
int SceKernelSuspendForDriver_0106C0F0(void);	/* suspend */
int SceKernelSuspendForDriver_0A6CA124(void);	/* standby */
int SceKernelSuspendForDriver_AEA9440D(void);	/* cold reset */
