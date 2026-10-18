/*
 * kernel/processmgr.h: the user process functions, under Sony's names (SDK Release 03.570.011,
 * target/include/kernel/processmgr.h). LibKernel exports them.
 */

#pragma once

#include <scetypes.h>
#include <kernel/constant.h>
#include <kernel/types.h>
#include <kernel/sysmem/memblock.h>

#ifdef __cplusplus
extern "C" {
#endif

/* process priorities */
#define SCE_KERNEL_PROCESS_PRIORITY_SYSTEM_HIGH		32
#define SCE_KERNEL_PROCESS_PRIORITY_USER_HIGH		64
#define SCE_KERNEL_PROCESS_PRIORITY_USER_DEFAULT	96
#define SCE_KERNEL_PROCESS_PRIORITY_SYSTEM_DEFAULT	96
#define SCE_KERNEL_PROCESS_PRIORITY_USER_LOW		127
#define SCE_KERNEL_PROCESS_PRIORITY_SYSTEM_LOW		159

/* sceKernelPowerTick: the power save timers to cancel */
#define SCE_KERNEL_POWER_TICK_DEFAULT			0x00	/* all of them */
#define SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND	0x01
#define SCE_KERNEL_POWER_TICK_DISABLE_DISPLAY_DIMMING	0x06
#define SCE_KERNEL_POWER_TICK_DISABLE_DISPLAY_OFF	0x04
#define SCE_KERNEL_POWER_TICK_DISABLE_OLED_DIMMING \
	SCE_KERNEL_POWER_TICK_DISABLE_DISPLAY_DIMMING
#define SCE_KERNEL_POWER_TICK_DISABLE_OLED_OFF \
	SCE_KERNEL_POWER_TICK_DISABLE_DISPLAY_OFF

int sceKernelExitProcess(SceInt32 res);
SceInt32 sceKernelPowerTick(SceInt32 arg);

/* the time since the process started, in microseconds */
SceInt32 sceKernelGetProcessTime(SceKernelSysClock *pClock);
SceUInt32 sceKernelGetProcessTimeLow(void);
SceUInt64 sceKernelGetProcessTimeWide(void);

#ifdef __cplusplus
}
#endif
