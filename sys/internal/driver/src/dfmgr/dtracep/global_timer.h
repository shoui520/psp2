/*
 * DTrace's time base, the Cortex-A9 global timer (global_timer.c).
 */
#pragma once

#include <scetypes.h>

SceUInt64 GlobalTimerGetCount(void);
int GlobalTimerInit(void);
int GlobalTimerFini(void);
int GlobalTimerSuspend(void);
int GlobalTimerResume(void);
