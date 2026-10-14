/*
 * SceSystimer (os0:kd/systimer.skprx): SceSystimerForDriver.
 *
 * Timer ids are (type << 16) | index.
 */

#pragma once

#include <psp2kern/types.h>

typedef SceInt32 SceSysTimerId;
typedef void (*SceSysTimerCallback)(SceSysTimerId timer, void *arg);

enum {
	SCE_SYSTIMER_TYPE_LONG = 1,	/* 5 64-bit long-range timers */
	SCE_SYSTIMER_TYPE_WORD = 2,	/* 7 32-bit word timers */
};

SceSysTimerId ksceKernelSysTimerAlloc(int type);
int ksceKernelSysTimerFree(SceSysTimerId id);
int ksceKernelSysTimerSetClockSource(SceSysTimerId id, SceUInt32 source, SceUInt32 prescale);
int ksceKernelSysTimerSetInterval(SceSysTimerId id, SceUInt64 interval);
int ksceKernelSysTimerSetHandler(SceSysTimerId id, SceSysTimerCallback handler,
				 SceUInt32 targetCpu, void *arg);
int ksceKernelSysTimerStartCount(SceSysTimerId id);
int ksceKernelSysTimerStopCount(SceSysTimerId id);
int ksceKernelSysTimerResetCount(SceSysTimerId id);
int ksceKernelSysTimerSetCount(SceSysTimerId id, SceUInt64 count);
int ksceKernelSysTimerGetCount(SceSysTimerId id, SceUInt64 *count);

/* names unknown (NID) */
int SceSystimerForDriver_85AAE0DD(SceSysTimerId id, SceUInt64 *count);
int SceSystimerForDriver_904DF737(SceSysTimerId id, SceUInt32 a, SceUInt32 b);
int SceSystimerForDriver_C34D7940(SceSysTimerId id, SceUInt32 a);
int SceSystimerForDriver_FD5F1742(SceSysTimerId id, SceUInt64 *value);
