#ifndef KERMIT_LOWIO_INT_H
#define KERMIT_LOWIO_INT_H

#include <kermit.h>

/* =========================================================================
 * External kernel imports (matching module.yml)
 * ========================================================================= */
extern int sceKernelDmaRegisterDdrFlush(void *func);
extern int sceKernelCallSubIntrHandler(int intr_code, int subintr_code, void *subintr_arg, void *frame);
extern void sceKernelCpuResumeIntrWithSync(int state);
extern int sceKernelPollEventFlag(SceUID evid, u32 bits, u32 wait, u32 *out_bits);
extern void sceKernelDcacheWritebackAll(void);
extern void *memset(void *dst, int c, u32 n);
extern void *memcpy(void *dst, const void *src, u32 n);

#define REG32(addr) (*(volatile u32 *)(addr))

/* Kernel interrupt handler callback descriptor (size = 0x2C bytes) */
typedef struct SceLowioIntrCb {
	u32 size;
	void *cb_register;
	void *cb_release;
	void *cb_reserved_reg;
	void *cb_reserved_rel;
	s32 (*cb_clear)(s32 intr_code, s32 subintr);
	s32 (*cb_enable)(s32 intr_code, s32 subintr);
	s32 (*cb_disable)(s32 intr_code, s32 subintr);
	s32 (*cb_suspend)(s32 intr_code, s32 subintr, u32 *prev_mask);
	s32 (*cb_resume)(s32 intr_code, s32 subintr, u32 prev_mask);
	s32 (*cb_is_occurred)(s32 intr_code, s32 subintr);
} SceLowioIntrCb;

typedef struct SceLowioIntrOpt {
	u32 size;
	u32 subintr_count;
	SceLowioIntrCb *cb_table;
} SceLowioIntrOpt;

typedef struct SceLowioSysEventParam {
	u32 header;
	union {
		s32 suspend_kind;
		const u32 *resume_flags;
	} body;
} SceLowioSysEventParam;

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

/* Handwritten PIC boot-RAM DDR clock-change block (copied to 0xBFC00C00) */
s32 ddrChangeClockBootRam(s32 freq_idx, s32 cas_latency, s32 saved_delay,
                          s32 delay_lo, s32 delay_hi, const u32 *test_pat);
extern u32 ddrBootRamEnd[];

/* Cross-driver exports referenced by other drivers */
s32 sceSysregPllUpdateFrequency(void);
s32 sceSysregApbTimerClkEnable(s32 timer);
s32 sceSysregApbTimerClkDisable(s32 timer);
s32 sceSysregApbTimerClkSelect(s32 timer, s32 clk);
s32 sceSysreg_driver_9746F3B2(s32 clk_idx, s32 clk_div);
s32 sceDdrFlush(s32 channel_mask);
void sceDdr_driver_6DE74D80(s32 freq_idx, s32 cas_latency);
s32 sceDdr_driver_4F30BFE8(s32 delay_count);
s32 sceDdr_driver_F1005384(s32 delay_lo, s32 delay_hi);

#endif /* KERMIT_LOWIO_INT_H */
