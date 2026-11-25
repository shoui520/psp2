#include "lowio_int.h"

/* =========================================================================
 * Part 2: sceDdr_driver (14 exported functions)
 * ========================================================================= */

static s32 g_ddrFreqIdx = -1;
static u32 g_ddrSavedParam[16] = {
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1
};
static const u32 g_ddrCfgTable[8][2] = {
	{ 1, 0 }, { 9, 0 }, { 0x11, 0 }, { 0x19, 0 },
	{ 0, 0 }, { 0, 0 }, { 0, 0 },    { 0, 0 }
};

s32 sceDdrFlush(s32 flag)
{
	__asm__ volatile("sync");
	u32 m = flag & 0xF;
	REG32(0xBD000004) = m;
	while ((REG32(0xBD000004) & m) != 0)
		;
	return 0;
}
s32 sceDdr_driver_E0A39D3E(s32 val)
{
	REG32(0xBD00002C) = (val >= 1) ? ((u32)val | 0x80000000U) : 0;
	__asm__ volatile("sync");
	return 0;
}
s32 sceDdr_driver_00E36648(void)
{
	s32 v = (s32)REG32(0xBD00002C);
	return (v < 0) ? (v & 0x7FFFFFFF) : 0;
}
s32 sceDdr_driver_95B9A692(void)
{
	s32 count = 1;
	s32 i = 64;
	while (i > 0) {
		REG32(0xBD000040) = 0x2B;
		(void)REG32(0xBD000040);
		if ((REG32(0xBD000040) & 0x100) != 0)
			break;
		i--;
		if (i != 0)
			count++;
	}
	for (i = count + 1; i > 0; i--) {
		REG32(0xBD000040) = 0x2A;
		(void)REG32(0xBD000040);
	}
	REG32(0xBD000040) = 0x20;
	__asm__ volatile("sync");
	return count;
}
void sceDdr_driver_77CD1FB3(s32 a0, s32 a1, s32 a2)
{
	while (((REG32(0xBD000020) >> 16) & 1) != 0)
		;
	REG32(0xBD000024) = ((a0 & 1) << 10) | (a1 << 16) | (a2 & 0x7BFF);
	__asm__ volatile("sync");
	REG32(0xBD000020) = a0 | 0x8000;
	__asm__ volatile("sync");
	while (((REG32(0xBD000020) >> 16) & 1) != 0)
		;
}
void sceDdr_driver_3D50DEC9(s32 idx)
{
	u32 v0 = g_ddrCfgTable[idx & 7][0];
	u32 v1 = g_ddrCfgTable[idx & 7][1];
	REG32(0xBD000030) = (REG32(0xBD000030) & ~0x31U) | v0;
	REG32(0xBD000038) = v1;
	__asm__ volatile("sync");
}
s32 sceDdr_driver_2AA39A80(void)
{
	u32 v = REG32(0xBD000030);
	return ((v & 1) | ((v >> 3) & 6)) - 1;
}
void sceDdr_driver_397756C0(s32 a0, s32 a1)
{
	sceDdr_driver_77CD1FB3(0x25, 0, 0);
	sceDdr_driver_77CD1FB3(0x22, 0, 0);
	sceDdr_driver_77CD1FB3(0x22, 0, 0);
	sceDdr_driver_77CD1FB3(0x20, 0, (a0 << 4) | 1);
	sceDdr_driver_77CD1FB3(0x20, 2, a1 << 5);
}
void sceDdr_driver_0BAAE4C5(s32 a0, s32 a1)
{
	(void)a0;
	sceDdr_driver_77CD1FB3(0x20, 2, a1 << 5);
}
void sceDdr_driver_6DE74D80(s32 idx, s32 cas)
{
	static const u32 ddr_timing[8][2] = {
		{ 0x11110000, 0x0006000F },
		{ 0x11110000, 0x0007000F },
		{ 0x11110000, 0x0007000F },
		{ 0x11110400, 0x00070000 },
		{ 0x11110400, 0x00070008 },
		{ 0x11110400, 0x0007000A },
		{ 0x00000000, 0x00000000 },
		{ 0x00000000, 0x00000000 }
	};
	u32 v0 = ddr_timing[idx & 7][0] | ((cas - 3) << 12);
	u32 v1 = ddr_timing[idx & 7][1];
	REG32(0xBD000030) = (REG32(0xBD000030) & 0x31) | v0;
	REG32(0xBD000044) = v1 & 0xFFFF;
	REG32(0xBD000034) = v1 >> 16;
	__asm__ volatile("sync");
}
void sceDdr_driver_9F882141(s32 a0, s32 a1, s32 a2, s32 a3)
{
	sceDdr_driver_3D50DEC9(a2);
	sceDdr_driver_6DE74D80(a0, a1);
	sceDdr_driver_397756C0(a1, (a3 >= 0) ? a3 : 1);
}
s32 sceDdr_driver_4F30BFE8(s32 count)
{
	s32 i;
	for (i = 64; i > 0; i--) {
		REG32(0xBD000040) = 0x2B;
		(void)REG32(0xBD000040);
	}
	for (i = count + 1; i > 0; i--) {
		REG32(0xBD000040) = 0x2A;
		(void)REG32(0xBD000040);
	}
	return 0;
}
s32 sceDdr_driver_F1005384(s32 a0, s32 a1)
{
	REG32(0xBD000040) = a1 | 0x10;
	(void)REG32(0xBD000040);
	REG32(0xBD000040) = a0 | 0x10;
	(void)REG32(0xBD000040);
	REG32(0xBD000040) = 0x20;
	__asm__ volatile("sync");
	return 0;
}
static const u32 g_ddrTestPattern[4] = {
	0xA5A55A5A, 0xFFFFFFFF, 0x55AA55AA, 0xFF00FF00
};

static s32 ddrFlushBootRamHelper(s32 idx, s32 cas, s32 saved_cnt, s32 a3, s32 t0, const u32 *test_pat)
{
	sceDdr_driver_6DE74D80(idx, cas);
	if (saved_cnt >= 0)
		sceDdr_driver_4F30BFE8(saved_cnt);
	REG32(0xBC100068) = idx | 0x80;
	__asm__ volatile("sync");
	if (a3 >= 0) {
		sceDdr_driver_F1005384(a3, t0);
		s32 outer = 2;
		s32 timeout = 350;
		while (outer > 0) {
			do {
				u32 v = REG32(0xBD000040);
				if (--timeout < 0)
					goto check_pat;
				if ((v & 0x100) != 0)
					break;
			} while (1);
			do {
				u32 v = REG32(0xBD000040);
				if (--timeout < 0)
					goto check_pat;
				if ((v & 0x100) == 0)
					break;
			} while (1);
			outer--;
		}
check_pat:
		if (test_pat != NULL) {
			u32 chk;
			do {
				__asm__ volatile("cache 0x19, 0(%0)" :: "r"(test_pat));
				u32 w0 = test_pat[0];
				u32 w1 = test_pat[1];
				u32 w2 = test_pat[2];
				u32 w3 = test_pat[3];
				u32 c1 = ~(w1 & (w2 | w3));
				u32 r2 = ((w2 >> 1) | (w2 << 31)) ^ w3;
				chk = c1 | w0 | r2;
			} while (chk != 0);
		}
	}
	return 0;
}

s32 sceDdr_driver_CF07B9E2(s32 idx)
{
	if ((u32)(idx | 8) >= 14)
		return 0x800001FE;
	s32 intr = sceKernelCpuSuspendIntr();
	if (g_ddrFreqIdx >= 0)
		g_ddrSavedParam[g_ddrFreqIdx] = sceDdr_driver_95B9A692();
	u32 saved = g_ddrSavedParam[idx];
	g_ddrFreqIdx = idx;
	sceKernelDcacheWritebackAll();
	u32 mpriv = REG32(0xBC000044);
	REG32(0xBC000044) = mpriv & 0xC000FFFFU;
	(void)REG32(0xBC100068);
	s32 flush_flag = ((REG32(0xBC10004C) & 4) != 0) ? 5 : 0xF;
	sceDdrFlush(flush_flag);
	((s32 (*)(s32, s32, s32, s32, s32, const u32 *))0xBFC00C00)(idx, 3, (s32)saved, 8, 10, g_ddrTestPattern);
	REG32(0xBC000044) = mpriv;
	sceSysregPllUpdateFrequency();
	sceKernelCpuResumeIntrWithSync(intr);
	return 0;
}
s32 ddrModuleStart(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	memcpy((void *)0xBFC00C00, (const void *)ddrFlushBootRamHelper, 0x240);
	sceKernelDmaRegisterDdrFlush(sceDdrFlush);
	return 0;
}
s32 ddrModuleStop(SceSize args)
{
	(void)args;
	sceKernelDmaRegisterDdrFlush(NULL);
	return 0;
}

