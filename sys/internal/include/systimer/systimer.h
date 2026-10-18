/*
 * SceSystimer (os0:kd/systimer.skprx): SceSystimerForDriver.
 *
 * Timer ids are (type << 16) | index.
 */

#pragma once

#include <scetypes.h>

typedef SceInt32 SceSysTimerId;
typedef void (*SceSysTimerCallback)(SceSysTimerId timer, void *arg);

enum {
	SCE_SYSTIMER_TYPE_LONG = 1,	/* 5 64-bit long-range timers */
	SCE_SYSTIMER_TYPE_WORD = 2,	/* 7 32-bit word timers */
};

SceSysTimerId SceSystimerForDriver_E2B9D8E9(int type);	/* vitasdk: ksceKernelSysTimerAlloc */
int SceSystimerForDriver_8358611F(SceSysTimerId id);	/* vitasdk: ksceKernelSysTimerFree */
/* vitasdk: ksceKernelSysTimerSetClockSource */
int SceSystimerForDriver_0450933B(SceSysTimerId id, SceUInt32 source, SceUInt32 prescale);
/* vitasdk: ksceKernelSysTimerSetInterval */
int SceSystimerForDriver_A73351C1(SceSysTimerId id, SceUInt64 interval);
/* vitasdk: ksceKernelSysTimerSetHandler */
int SceSystimerForDriver_CCCD1EE3(SceSysTimerId id, SceSysTimerCallback handler,
				 SceUInt32 targetCpu, void *arg);
int SceSystimerForDriver_59EBD22D(SceSysTimerId id);	/* vitasdk: ksceKernelSysTimerStartCount */
int SceSystimerForDriver_BF8D42B0(SceSysTimerId id);	/* vitasdk: ksceKernelSysTimerStopCount */
int SceSystimerForDriver_7DEBA6A5(SceSysTimerId id);	/* vitasdk: ksceKernelSysTimerResetCount */
/* vitasdk: ksceKernelSysTimerSetCount */
int SceSystimerForDriver_1B14DFD6(SceSysTimerId id, SceUInt64 count);
/* vitasdk: ksceKernelSysTimerGetCount */
int SceSystimerForDriver_963F7C0D(SceSysTimerId id, SceUInt64 *count);

/* names unknown (NID) */
int SceSystimerForDriver_85AAE0DD(SceSysTimerId id, SceUInt64 *count);
int SceSystimerForDriver_904DF737(SceSysTimerId id, SceUInt32 a, SceUInt32 b);
int SceSystimerForDriver_C34D7940(SceSysTimerId id, SceUInt32 a);
int SceSystimerForDriver_FD5F1742(SceSysTimerId id, SceUInt64 *value);
