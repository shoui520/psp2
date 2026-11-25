#include "lowio_int.h"

/* =========================================================================
 * Part 2: sceDdr_driver (14 exported functions)
 * ========================================================================= */

static s32 g_ddrFreqIdx = -1;
static s32 g_ddrSavedDelay[16] = {
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1
};
static const u32 g_ddrCfgTable[8][2] = {
	{ 1, 0 }, { 9, 0 }, { 0x11, 0 }, { 0x19, 0 },
	{ 0, 0 }, { 0, 0 }, { 0, 0 },    { 0, 0 }
};
static const u32 g_ddrTestPattern[8] = {
	0x00000000, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA,
	0x00000000, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA
};

s32 ddrInitHook(void);
s32 ddrStopHook(void);
s32 ddrSuspendHook(void);
s32 ddrResumeHook(void);

s32 sceDdrFlush(s32 channel_mask)
{
	__asm__ volatile("sync");
	u32 mask = (u32)channel_mask & 0xFU;
	REG32(0xBD000004) = mask;
	while ((REG32(0xBD000004) & mask) != 0)
		;
	return 0;
}

s32 sceDdr_driver_E0A39D3E(s32 interval)
{
	u32 reg_val = (interval >= 1) ? ((u32)interval | 0x80000000U) : 0U;
	REG32(0xBD00002C) = reg_val;
	__asm__ volatile("sync");
	return 0;
}

s32 sceDdr_driver_00E36648(void)
{
	s32 reg_val = (s32)REG32(0xBD00002C);
	u32 sign_mask = (u32)(reg_val >> 31) >> 1;
	return (s32)((u32)reg_val & sign_mask);
}

s32 sceDdr_driver_95B9A692(void)
{
	s32 count = 1;
	s32 step = 64;
	while (step > 0) {
		REG32(0xBD000040) = 0x2B;
		(void)REG32(0xBD000040);
		if ((REG32(0xBD000040) & 0x100) != 0)
			break;
		step--;
		if (step != 0)
			count++;
	}
	for (step = count + 1; step > 0; step--) {
		REG32(0xBD000040) = 0x2A;
		(void)REG32(0xBD000040);
	}
	REG32(0xBD000040) = 0x20;
	__asm__ volatile("sync");
	return count;
}

void sceDdr_driver_77CD1FB3(s32 cmd_code, s32 bank_addr, s32 mode_bits)
{
	while (((REG32(0xBD000020) >> 16) & 1) != 0)
		;
	REG32(0xBD000024) = ((cmd_code & 1) << 10) | (bank_addr << 16) | (mode_bits & 0x7BFF);
	__asm__ volatile("sync");
	REG32(0xBD000020) = (u32)cmd_code | 0x8000U;
	__asm__ volatile("sync");
	while (((REG32(0xBD000020) >> 16) & 1) != 0)
		;
}

void sceDdr_driver_3D50DEC9(s32 cfg_idx)
{
	u32 cfg_lo = g_ddrCfgTable[cfg_idx][0];
	u32 cfg_hi = g_ddrCfgTable[cfg_idx][1];
	REG32(0xBD000030) = (REG32(0xBD000030) & ~0x31U) | cfg_lo;
	REG32(0xBD000038) = cfg_hi;
	__asm__ volatile("sync");
}

s32 sceDdr_driver_2AA39A80(void)
{
	u32 reg_val = REG32(0xBD000030);
	return (s32)(((reg_val & 1U) | ((reg_val >> 3) & 6U)) - 1U);
}

void sceDdr_driver_397756C0(s32 cas_latency, s32 drive_strength)
{
	sceDdr_driver_77CD1FB3(0x25, 0, 0);
	sceDdr_driver_77CD1FB3(0x22, 0, 0);
	sceDdr_driver_77CD1FB3(0x22, 0, 0);
	sceDdr_driver_77CD1FB3(0x20, 0, (cas_latency << 4) | 1);
	sceDdr_driver_77CD1FB3(0x20, 2, drive_strength << 5);
}

void sceDdr_driver_0BAAE4C5(s32 unused_cas, s32 drive_strength)
{
	(void)unused_cas;
	sceDdr_driver_77CD1FB3(0x20, 2, drive_strength << 5);
}

void sceDdr_driver_9F882141(s32 freq_idx, s32 cas_latency, s32 cfg_idx, s32 drive_strength)
{
	sceDdr_driver_3D50DEC9(cfg_idx);
	sceDdr_driver_6DE74D80(freq_idx, cas_latency);
	sceDdr_driver_397756C0(cas_latency, (drive_strength >= 0) ? drive_strength : 1);
}

s32 sceDdr_driver_CF07B9E2(s32 freq_idx)
{
	if ((u32)(freq_idx | 8) >= 14U)
		return (s32)0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	if (g_ddrFreqIdx >= 0)
		g_ddrSavedDelay[g_ddrFreqIdx] = sceDdr_driver_95B9A692();
	s32 saved = g_ddrSavedDelay[freq_idx];
	g_ddrFreqIdx = freq_idx;
	sceKernelDcacheWritebackAll();
	u32 mpriv = REG32(0xBC000044);
	REG32(0xBC000044) = mpriv & 0xC000FFFFU;
	s32 flush_mask = ((REG32(0xBC10004C) & 4U) != 0) ? 5 : 0xF;
	sceDdrFlush(flush_mask);
	((s32 (*)(s32, s32, s32, s32, s32, const u32 *))0xBFC00C00)(
		freq_idx, 3, saved, 8, 10, g_ddrTestPattern);
	REG32(0xBC000044) = mpriv;
	sceSysregPllUpdateFrequency();
	sceKernelCpuResumeIntrWithSync(intr);
	return 0;
}

s32 ddrModuleStart(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	ddrInitHook();
	memcpy((void *)0xBFC00C00, (const void *)ddrChangeClockBootRam,
	       (u32)ddrBootRamEnd - (u32)ddrChangeClockBootRam);
	sceKernelDmaRegisterDdrFlush(sceDdrFlush);
	return 0;
}

s32 ddrModuleStop(SceSize args)
{
	(void)args;
	sceKernelDmaRegisterDdrFlush(NULL);
	ddrStopHook();
	return 0;
}

s32 ddrInitHook(void)
{
	return 0;
}

s32 ddrStopHook(void)
{
	return 0;
}

s32 ddrSuspendHook(void)
{
	return 0;
}

s32 ddrResumeHook(void)
{
	return 0;
}
