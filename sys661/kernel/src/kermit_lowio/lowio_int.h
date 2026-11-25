#ifndef KERMIT_LOWIO_INT_H
#define KERMIT_LOWIO_INT_H

#include <kermit.h>

/* =========================================================================
 * External kernel imports (matching scetables.S)
 * ========================================================================= */
extern int sceKernelDmaRegisterDdrFlush(void *func);
extern int sceKernelCallSubIntrHandler(int intr_code, int sub_intr_code, void *arg1, void *arg2);
extern void sceKernelCpuResumeIntrWithSync(int state);
extern int sceKernelPollEventFlag(SceUID evid, u32 bits, u32 wait, u32 *out_bits);
extern void sceKernelDcacheWritebackAll(void);
extern void *memset(void *dst, int c, u32 n);
extern void *memcpy(void *dst, const void *src, u32 n);

#define REG32(addr) (*(volatile u32 *)(addr))



/* Cross-driver internal helpers */
s32 sysregDmacplusBusClockEnable(void);
s32 sysregDmacplusBusClockDisable(void);
s32 sysregLcdcClkEnable(void);
s32 sysregLcdcClkDisable(void);
s32 sysregLcdcIoEnable(void);
s32 sysregLcdcIoDisable(void);

s32 sysregModuleStart(SceSize args);
s32 sysregModuleStop(SceSize args);
s32 ddrModuleStart(SceSize args, void *argp);
s32 ddrModuleStop(SceSize args);
s32 dmacplusModuleStart(SceSize args, void *argp);
s32 dmacplusModuleStop(SceSize args);
s32 lcdcModuleStart(SceSize args, void *argp);
s32 lcdcModuleStop(SceSize args);

/* Cross-driver exports referenced by other drivers */
s32 sceSysregPllUpdateFrequency(void);
s32 sceSysregApbTimerClkEnable(s32 timer);
s32 sceSysregApbTimerClkDisable(s32 timer);
s32 sceSysregApbTimerClkSelect(s32 timer, s32 clk);
s32 sceSysreg_driver_9746F3B2(s32 a0, s32 a1);
s32 sceDdrFlush(s32 a0);
void sceDdr_driver_6DE74D80(s32 idx, s32 cas);
s32 sceDdr_driver_4F30BFE8(s32 cnt);

#endif /* KERMIT_LOWIO_INT_H */
