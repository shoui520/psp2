/*
 * SceDeci4pSDbgp (os0:kd/deci4p_sdbgp.skprx): VFP, CP14 debug, and CP15
 * system register access for the system debugger (cpu_reg.c).
 */

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <processmgr/processmgr.h>
#include <threadmgr/threadmgr_debugger.h>
#include "sdbgp_internal.h"

#define SDBGP_MRC(cp, op1, crn, crm, op2) ({				\
	uint32_t _val;							\
	__asm__ volatile("mrc " #cp ", " #op1 ", %0, " #crn ", "	\
			 #crm ", " #op2 : "=r"(_val));			\
	_val;								\
})

#define SDBGP_MCR(cp, op1, val, crn, crm, op2) do {			\
	uint32_t _val = (val);						\
	__asm__ volatile("mcr " #cp ", " #op1 ", %0, " #crn ", "	\
			 #crm ", " #op2 : : "r"(_val));			\
} while (0)

int sdbgpReadVfpSystemReg(uint32_t code, uint32_t *val, uintptr_t vfpCtx)
{
	uint32_t v;

	switch (code) {
	case 0x0a007000:
		__asm__ volatile("vmrs %0, fpsid" : "=r"(v));
		*val = v;
		break;
	case 0x0a007100:
		if (vfpCtx != 0)
			v = *(const uint32_t *)(vfpCtx + 0x100);
		else
			__asm__ volatile("vmrs %0, fpscr" : "=r"(v));
		*val = v;
		break;
	case 0x0a007600:
		__asm__ volatile("vmrs %0, mvfr1" : "=r"(v));
		*val = v;
		break;
	case 0x0a007700:
		__asm__ volatile("vmrs %0, mvfr0" : "=r"(v));
		*val = v;
		break;
	case 0x0a007800:
		if (vfpCtx != 0) {
			*val = *(const uint32_t *)(vfpCtx + 0x104);
			return 0;
		}
		__asm__ volatile("vmrs %0, fpexc" : "=r"(v));
		*val = v;
		break;
	default:
		break;
	}
	return 0;
}

int sdbgpReadDebugReg(uint32_t code, uint32_t *val, uintptr_t cpuCtx, SceUID pid)
{
	uint32_t bvr = 0;
	uint32_t bcr = 0;
	uint32_t ctrl;

	if (pid == -1)
		pid = SDBGP_KERNEL_PROCESS_ID;

	switch (code) {
	case 0x0e000000:
		*val = SDBGP_MRC(p14, 0, c0, c0, 0);
		break;
	case 0x0e000002:
		*val = SDBGP_MRC(p14, 0, c0, c0, 2);
		break;
	case 0x0e000004:
		*val = SDBGP_MRC(p14, 0, c0, c0, 4);
		break;
	case 0x0e000005:
		*val = SDBGP_MRC(p14, 0, c0, c0, 5);
		break;
	case 0x0e000006:
		*val = SDBGP_MRC(p14, 0, c0, c0, 6);
		break;
	case 0x0e000007:
		*val = SDBGP_MRC(p14, 0, c0, c0, 7);
		break;
	case 0x0e000010:
		if (cpuCtx != 0)
			*val = *(const uint32_t *)(cpuCtx + 0xc8);
		else
			*val = SDBGP_MRC(p14, 0, c0, c1, 0);
		break;
	case 0x0e000012:
		*val = SDBGP_MRC(p14, 0, c0, c8, 2);
		break;
	case 0x0e000016:
	case 0x0e000017:
	case 0x0e000026:
	case 0x0e000027:
	case 0x0e000036:
	case 0x0e000037:
		if (sceKernelGetPHWP(pid, (uint8_t)code >> 4, &bvr, &bcr) < 0)
			*val = -1;
		else if ((code & 0xf) == 6)
			*val = bvr;
		else
			*val = bcr;
		break;
	case 0x0e000022:
		*val = SDBGP_MRC(p14, 0, c0, c2, 2);
		break;
	case 0x0e000024:
	case 0x0e000025:
	case 0x0e000034:
	case 0x0e000035:
	case 0x0e000044:
	case 0x0e000045:
		if (sceKernelGetPHBP(pid, (uint8_t)code >> 4, &bvr, &bcr) < 0)
			*val = -1;
		else if ((code & 0xf) == 4)
			*val = bvr;
		else
			*val = bcr;
		break;
	case 0x0e000032:
		*val = SDBGP_MRC(p14, 0, c0, c3, 2);
		break;
	case 0x0e000050:
		*val = SDBGP_MRC(p14, 0, c0, c5, 0);
		break;
	case 0x0e000054:
	case 0x0e000055:
		if (sceKernelGetTHBP(pid, 5, &bvr, &bcr) < 0)
			*val = -1;
		else if ((code & 0xf) == 4)
			*val = bvr;
		else
			*val = bcr;
		break;
	case 0x0e000060:
		*val = SDBGP_MRC(p14, 0, c0, c6, 0);
		break;
	case 0x0e000070:
		*val = SDBGP_MRC(p14, 0, c0, c7, 0);
		break;
	case 0x0e000090:
		*val = SDBGP_MRC(p14, 0, c0, c9, 0);
		break;
	case 0x0e0000a0:
		*val = SDBGP_MRC(p14, 0, c0, c10, 0);
		break;
	case 0x0e0000b0:
		*val = SDBGP_MRC(p14, 0, c0, c11, 0);
		break;
	case 0x0e000100:
		*val = SDBGP_MRC(p14, 0, c1, c0, 0);
		break;
	case 0x0e000114:
		*val = SDBGP_MRC(p14, 0, c1, c1, 4);
		break;
	case 0x0e000124:
		*val = SDBGP_MRC(p14, 0, c1, c2, 4);
		break;
	case 0x0e000144:
		*val = SDBGP_MRC(p14, 0, c1, c4, 4);
		break;
	case 0x0e000154:
		*val = SDBGP_MRC(p14, 0, c1, c5, 4);
		break;
	case 0x0e000200:
		*val = SDBGP_MRC(p14, 0, c2, c0, 0);
		break;
	case 0x0e000704:
		*val = SDBGP_MRC(p14, 0, c7, c0, 4);
		break;
	case 0x0e000727:
		*val = SDBGP_MRC(p14, 0, c7, c2, 7);
		break;
	case 0x0e000737:
		*val = SDBGP_MRC(p14, 0, c7, c3, 7);
		break;
	case 0x0e000747:
		*val = SDBGP_MRC(p14, 0, c7, c4, 7);
		break;
	case 0x0e000757:
		*val = SDBGP_MRC(p14, 0, c7, c5, 7);
		break;
	case 0x0e000767:
		*val = SDBGP_MRC(p14, 0, c7, c6, 7);
		break;
	case 0x0e000777:
		*val = SDBGP_MRC(p14, 0, c7, c7, 7);
		break;
	case 0x0e000786:
		*val = SDBGP_MRC(p14, 0, c7, c8, 6);
		break;
	case 0x0e000787:
		*val = SDBGP_MRC(p14, 0, c7, c8, 7);
		break;
	case 0x0e000796:
		*val = SDBGP_MRC(p14, 0, c7, c9, 6);
		break;
	case 0x0e0007c7:
		*val = SDBGP_MRC(p14, 0, c7, c12, 7);
		break;
	case 0x0e0007d6:
		*val = SDBGP_MRC(p14, 0, c7, c13, 6);
		break;
	case 0x0e0007d7:
		*val = SDBGP_MRC(p14, 0, c7, c13, 7);
		break;
	case 0x0e0007e6:
		*val = SDBGP_MRC(p14, 0, c7, c14, 6);
		break;
	case 0x0e0007e7:
		*val = SDBGP_MRC(p14, 0, c7, c14, 7);
		break;
	case 0x0e0007f7:
		*val = SDBGP_MRC(p14, 0, c7, c15, 7);
		break;
	case 0x0e006000:
		*val = SDBGP_MRC(p14, 6, c0, c0, 0);
		break;
	case 0x0e006100:
		if (cpuCtx != 0)
			*val = *(const uint32_t *)(cpuCtx + 0xb8);
		else
			*val = SDBGP_MRC(p14, 6, c1, c0, 0);
		break;
	case 0x0e007000:
		*val = SDBGP_MRC(p14, 7, c0, c0, 0);
		break;
	case 0x0e007100:
		ctrl = SDBGP_MRC(p14, 7, c2, c0, 0);
		SDBGP_MCR(p14, 7, ctrl | 1, c2, c0, 0);
		*val = SDBGP_MRC(p14, 7, c1, c0, 0);
		SDBGP_MCR(p14, 7, ctrl, c2, c0, 0);
		break;
	case 0x0e007200:
		*val = SDBGP_MRC(p14, 7, c2, c0, 0);
		break;
	case 0x0e007300:
		ctrl = SDBGP_MRC(p14, 7, c2, c0, 0);
		SDBGP_MCR(p14, 7, ctrl | 1, c2, c0, 0);
		*val = SDBGP_MRC(p14, 7, c3, c0, 0);
		SDBGP_MCR(p14, 7, ctrl, c2, c0, 0);
		break;
	case 0x0e007400:
		*val = -1;
		break;
	default:
		break;
	}
	return 0;
}

int sdbgpWriteVfpSystemReg(uint32_t code, uint32_t val, uintptr_t vfpCtx)
{
	switch (code) {
	case 0x0a007100:
		if (vfpCtx != 0)
			*(uint32_t *)(vfpCtx + 0x100) = val;
		else
			__asm__ volatile("vmsr fpscr, %0" : : "r"(val));
		break;
	case 0x0a007800:
		if (vfpCtx != 0)
			*(uint32_t *)(vfpCtx + 0x104) = val;
		else
			__asm__ volatile("vmsr fpexc, %0" : : "r"(val));
		break;
	default:
		break;
	}
	return 0;
}

int sdbgpWriteDebugReg(uint32_t code, uint32_t val, uintptr_t cpuCtx, SceUID pid)
{
	uint32_t bvr = 0;
	uint32_t bcr = 0;
	uint32_t ctrl;
	int idx;
	int reg;

	if (pid == -1)
		pid = SDBGP_KERNEL_PROCESS_ID;

	switch (code) {
	case 0x0e000004:
	case 0x0e000005:
	case 0x0e000024:
	case 0x0e000025:
	case 0x0e000034:
	case 0x0e000035:
	case 0x0e000044:
	case 0x0e000045:
	case 0x0e000054:
	case 0x0e000055:
		idx = (uint8_t)code >> 4;
		reg = code & 0xf;
		if (sceKernelGetPHBP(pid, idx, &bvr, &bcr) >= 0) {
			if (reg == 4)
				bvr = val;
			else
				bcr = val;
			sceKernelSetPHBP(pid, idx, bvr, bcr);
		}
		break;
	case 0x0e000006:
	case 0x0e000007:
	case 0x0e000016:
	case 0x0e000017:
	case 0x0e000026:
	case 0x0e000027:
	case 0x0e000036:
	case 0x0e000037:
		idx = (uint8_t)code >> 4;
		reg = code & 0xf;
		if (sceKernelGetPHWP(pid, idx, &bvr, &bcr) >= 0) {
			if (reg == 4)
				bvr = val;
			else
				bcr = val;
			sceKernelSetPHWP(pid, idx, bvr, bcr);
		}
		break;
	case 0x0e000014:
		SDBGP_MCR(p14, 0, val, c0, c1, 4);
		break;
	case 0x0e000015:
		SDBGP_MCR(p14, 0, val, c0, c1, 5);
		break;
	case 0x0e000022:
		SDBGP_MCR(p14, 0, val, c0, c2, 2);
		break;
	case 0x0e006000:
		SDBGP_MCR(p14, 6, val, c0, c0, 0);
		break;
	case 0x0e006100:
		if (cpuCtx != 0)
			*(uint32_t *)(cpuCtx + 0xb8) = val;
		else
			SDBGP_MCR(p14, 6, val, c1, c0, 0);
		break;
	case 0x0e007100:
		ctrl = SDBGP_MRC(p14, 7, c2, c0, 0);
		SDBGP_MCR(p14, 7, ctrl | 1, c2, c0, 0);
		SDBGP_MCR(p14, 7, val, c1, c0, 0);
		SDBGP_MCR(p14, 7, ctrl, c2, c0, 0);
		break;
	case 0x0e007200:
		SDBGP_MCR(p14, 7, val, c2, c0, 0);
		break;
	case 0x0e007300:
		ctrl = SDBGP_MRC(p14, 7, c2, c0, 0);
		SDBGP_MCR(p14, 7, ctrl | 1, c2, c0, 0);
		SDBGP_MCR(p14, 7, val, c3, c0, 0);
		SDBGP_MCR(p14, 7, ctrl, c2, c0, 0);
		break;
	case 0x0e007400:
		ctrl = SDBGP_MRC(p14, 7, c2, c0, 0);
		SDBGP_MCR(p14, 7, ctrl | 1, c2, c0, 0);
		SDBGP_MCR(p14, 7, val, c4, c0, 0);
		SDBGP_MCR(p14, 7, ctrl, c2, c0, 0);
		break;
	default:
		break;
	}
	return 0;
}

int sdbgpReadCp15Reg(uint32_t code, uint32_t *val, const uint32_t *cpuCtx, uint32_t excTtbr0)
{
	switch (code) {
	case 0x0f000000:
		*val = SDBGP_MRC(p15, 0, c0, c0, 0);
		break;
	case 0x0f000001:
		*val = SDBGP_MRC(p15, 0, c0, c0, 1);
		break;
	case 0x0f000003:
		*val = SDBGP_MRC(p15, 0, c0, c0, 3);
		break;
	case 0x0f000005:
		*val = SDBGP_MRC(p15, 0, c0, c0, 5);
		break;
	case 0x0f000100:
		*val = SDBGP_MRC(p15, 0, c1, c0, 0);
		break;
	case 0x0f000101:
		*val = SDBGP_MRC(p15, 0, c1, c0, 1);
		break;
	case 0x0f000102:
		if (cpuCtx != NULL)
			*val = cpuCtx[19];
		else
			*val = SDBGP_MRC(p15, 0, c1, c0, 2);
		break;
	case 0x0f000112:
		*val = SDBGP_MRC(p15, 0, c1, c1, 2);
		break;
	case 0x0f000200:
		*val = SDBGP_MRC(p15, 0, c2, c0, 0);
		break;
	case 0x0f000201:
		if (cpuCtx != NULL)
			*val = cpuCtx[22];
		else
			*val = SDBGP_MRC(p15, 0, c2, c0, 1);
		break;
	case 0x0f000202:
		*val = SDBGP_MRC(p15, 0, c2, c0, 2);
		break;
	case 0x0f000300:
		*val = SDBGP_MRC(p15, 0, c3, c0, 0);
		break;
	case 0x0f000500:
		if (cpuCtx != NULL)
			*val = cpuCtx[42];
		else
			*val = SDBGP_MRC(p15, 0, c5, c0, 0);
		break;
	case 0x0f000501:
		if (cpuCtx != NULL)
			*val = cpuCtx[43];
		else
			*val = SDBGP_MRC(p15, 0, c5, c0, 1);
		break;
	case 0x0f000510:
		*val = SDBGP_MRC(p15, 0, c5, c1, 0);
		break;
	case 0x0f000511:
		*val = SDBGP_MRC(p15, 0, c5, c1, 1);
		break;
	case 0x0f000600:
		if (cpuCtx != NULL)
			*val = cpuCtx[44];
		else
			*val = SDBGP_MRC(p15, 0, c6, c0, 0);
		break;
	case 0x0f000601:
		if (cpuCtx != NULL)
			*val = cpuCtx[45];
		else
			*val = SDBGP_MRC(p15, 0, c6, c0, 2);
		break;
	case 0x0f000740:
		if (cpuCtx != NULL)
			*val = cpuCtx[24];
		else
			*val = SDBGP_MRC(p15, 0, c7, c4, 0);
		break;
	case 0x0f0009c0:
		*val = SDBGP_MRC(p15, 0, c9, c12, 0);
		break;
	case 0x0f0009c1:
		if (cpuCtx != NULL)
			*val = cpuCtx[60];
		else
			*val = SDBGP_MRC(p15, 0, c9, c12, 1);
		break;
	case 0x0f0009c2:
		if (cpuCtx != NULL)
			*val = cpuCtx[60];
		else
			*val = SDBGP_MRC(p15, 0, c9, c12, 2);
		break;
	case 0x0f0009c3:
		*val = SDBGP_MRC(p15, 0, c9, c12, 3);
		break;
	case 0x0f0009c5:
		if (cpuCtx != NULL)
			*val = cpuCtx[27];
		else
			*val = SDBGP_MRC(p15, 0, c9, c12, 5);
		break;
	case 0x0f0009d0:
		if (cpuCtx != NULL)
			*val = cpuCtx[29];
		else
			*val = SDBGP_MRC(p15, 0, c9, c13, 0);
		break;
	case 0x0f0009d1:
		if (cpuCtx != NULL && cpuCtx[27] <= 5)
			*val = cpuCtx[2 * cpuCtx[27] + 30];
		else
			*val = SDBGP_MRC(p15, 0, c9, c13, 1);
		break;
	case 0x0f0009d2:
		if (cpuCtx != NULL && cpuCtx[27] <= 5)
			*val = cpuCtx[2 * cpuCtx[27] + 31];
		else
			*val = SDBGP_MRC(p15, 0, c9, c13, 2);
		break;
	case 0x0f0009e0:
		if (cpuCtx != NULL)
			*val = cpuCtx[28];
		else
			*val = SDBGP_MRC(p15, 0, c9, c14, 0);
		break;
	case 0x0f0009e1:
		*val = SDBGP_MRC(p15, 0, c9, c14, 1);
		break;
	case 0x0f0009e2:
		*val = SDBGP_MRC(p15, 0, c9, c14, 2);
		break;
	case 0x0f000a20:
		*val = SDBGP_MRC(p15, 0, c10, c2, 0);
		break;
	case 0x0f000a21:
		*val = SDBGP_MRC(p15, 0, c10, c2, 1);
		break;
	case 0x0f000b00:
		if ((SDBGP_MRC(p15, 0, c1, c1, 2) & 0x10000) != 0)
			*val = SDBGP_MRC(p15, 0, c11, c0, 0);
		else
			*val = -1;
		break;
	case 0x0f000b02:
		if ((SDBGP_MRC(p15, 0, c1, c1, 2) & 0x10000) != 0)
			*val = SDBGP_MRC(p15, 0, c11, c0, 2);
		else
			*val = -1;
		break;
	case 0x0f000b04:
		if ((SDBGP_MRC(p15, 0, c1, c1, 2) & 0x10000) != 0)
			*val = SDBGP_MRC(p15, 0, c11, c0, 4);
		else
			*val = -1;
		break;
	case 0x0f000b10:
		if ((SDBGP_MRC(p15, 0, c1, c1, 2) & 0x10000) != 0)
			*val = SDBGP_MRC(p15, 0, c11, c1, 0);
		else
			*val = -1;
		break;
	case 0x0f000b11:
		if ((SDBGP_MRC(p15, 0, c1, c1, 2) & 0x10000) != 0)
			*val = SDBGP_MRC(p15, 0, c11, c1, 1);
		else
			*val = -1;
		break;
	case 0x0f000c00:
		*val = SDBGP_MRC(p15, 0, c12, c0, 0);
		break;
	case 0x0f000c10:
		*val = SDBGP_MRC(p15, 0, c12, c1, 0);
		break;
	case 0x0f000d01:
		if (cpuCtx != NULL)
			*val = cpuCtx[23];
		else
			*val = SDBGP_MRC(p15, 0, c13, c0, 1);
		break;
	case 0x0f000d02:
		if (cpuCtx != NULL)
			*val = cpuCtx[20];
		else
			*val = SDBGP_MRC(p15, 0, c13, c0, 2);
		break;
	case 0x0f000d03:
		if (cpuCtx != NULL)
			*val = cpuCtx[21];
		else
			*val = SDBGP_MRC(p15, 0, c13, c0, 3);
		break;
	case 0x0f000d04:
		if (excTtbr0 != 0)
			*val = excTtbr0;
		break;
	case 0x0f000f00:
		*val = SDBGP_MRC(p15, 0, c15, c0, 0);
		break;
	case 0x0f000f10:
		*val = SDBGP_MRC(p15, 0, c15, c1, 0);
		break;
	case 0x0f001000:
		*val = SDBGP_MRC(p15, 1, c0, c0, 0);
		break;
	case 0x0f001001:
		*val = SDBGP_MRC(p15, 1, c0, c0, 1);
		break;
	case 0x0f001007:
		*val = SDBGP_MRC(p15, 1, c0, c0, 7);
		break;
	case 0x0f002000:
		*val = SDBGP_MRC(p15, 2, c0, c0, 0);
		break;
	case 0x0f004f00:
		*val = SDBGP_MRC(p15, 4, c15, c0, 0);
		break;
	default:
		break;
	}
	return 0;
}

int sdbgpWriteCp15Reg(uint32_t code, uint32_t val, uint32_t *cpuCtx)
{
	switch (code) {
	case 0x0f000100:
		SDBGP_MCR(p15, 0, val, c1, c0, 0);
		break;
	case 0x0f000101:
		SDBGP_MCR(p15, 0, val, c1, c0, 1);
		break;
	case 0x0f000102:
		if (cpuCtx != NULL)
			cpuCtx[19] = val;
		else
			SDBGP_MCR(p15, 0, val, c1, c0, 2);
		break;
	case 0x0f000200:
		SDBGP_MCR(p15, 0, val, c2, c0, 0);
		break;
	case 0x0f000201:
		if (cpuCtx != NULL)
			cpuCtx[22] = val;
		else
			SDBGP_MCR(p15, 0, val, c2, c0, 1);
		break;
	case 0x0f000202:
		SDBGP_MCR(p15, 0, val, c2, c0, 2);
		break;
	case 0x0f000300:
		SDBGP_MCR(p15, 0, val, c3, c0, 0);
		break;
	case 0x0f000500:
		if (cpuCtx != NULL)
			cpuCtx[42] = val;
		else
			SDBGP_MCR(p15, 0, val, c5, c0, 0);
		break;
	case 0x0f000501:
		if (cpuCtx != NULL)
			cpuCtx[43] = val;
		else
			SDBGP_MCR(p15, 0, val, c5, c0, 1);
		break;
	case 0x0f000510:
		SDBGP_MCR(p15, 0, val, c5, c1, 0);
		break;
	case 0x0f000511:
		SDBGP_MCR(p15, 0, val, c5, c1, 1);
		break;
	case 0x0f000600:
		if (cpuCtx != NULL)
			cpuCtx[44] = val;
		else
			SDBGP_MCR(p15, 0, val, c6, c0, 0);
		break;
	case 0x0f000601:
		if (cpuCtx != NULL)
			cpuCtx[45] = val;
		else
			SDBGP_MCR(p15, 0, val, c6, c0, 0);
		break;
	case 0x0f000740:
		if (cpuCtx != NULL)
			cpuCtx[24] = val;
		else
			SDBGP_MCR(p15, 0, val, c7, c4, 0);
		break;
	case 0x0f000780:
		SDBGP_MCR(p15, 0, val, c7, c8, 0);
		break;
	case 0x0f000781:
		SDBGP_MCR(p15, 0, val, c7, c8, 1);
		break;
	case 0x0f000782:
		SDBGP_MCR(p15, 0, val, c7, c8, 2);
		break;
	case 0x0f000783:
		SDBGP_MCR(p15, 0, val, c7, c8, 3);
		break;
	case 0x0f0009c0:
		SDBGP_MCR(p15, 0, val, c9, c12, 0);
		break;
	case 0x0f0009c1:
		if (cpuCtx != NULL)
			cpuCtx[60] |= val;
		else
			SDBGP_MCR(p15, 0, val, c9, c12, 1);
		break;
	case 0x0f0009c2:
		if (cpuCtx != NULL)
			cpuCtx[60] &= ~val;
		else
			SDBGP_MCR(p15, 0, val, c9, c12, 2);
		break;
	case 0x0f0009c3:
		SDBGP_MCR(p15, 0, val, c9, c12, 3);
		break;
	case 0x0f0009c4:
		SDBGP_MCR(p15, 0, val, c9, c12, 4);
		break;
	case 0x0f0009c5:
		if (cpuCtx != NULL)
			cpuCtx[27] = val;
		else
			SDBGP_MCR(p15, 0, val, c9, c12, 5);
		break;
	case 0x0f0009d0:
		if (cpuCtx != NULL)
			cpuCtx[29] = val;
		else
			SDBGP_MCR(p15, 0, val, c9, c13, 0);
		break;
	case 0x0f0009d1:
		if (cpuCtx != NULL) {
			if (cpuCtx[27] <= 5)
				cpuCtx[2 * cpuCtx[27] + 30] = val;
		} else {
			SDBGP_MCR(p15, 0, val, c9, c13, 1);
		}
		break;
	case 0x0f0009d2:
		if (cpuCtx != NULL) {
			if (cpuCtx[27] <= 5)
				cpuCtx[2 * cpuCtx[27] + 31] = val;
		} else {
			SDBGP_MCR(p15, 0, val, c9, c13, 2);
		}
		break;
	case 0x0f0009e0:
		if (cpuCtx != NULL)
			cpuCtx[28] = val;
		else
			SDBGP_MCR(p15, 0, val, c9, c14, 0);
		break;
	case 0x0f0009e1:
		SDBGP_MCR(p15, 0, val, c9, c14, 1);
		break;
	case 0x0f0009e2:
		SDBGP_MCR(p15, 0, val, c9, c14, 2);
		break;
	case 0x0f000a20:
		SDBGP_MCR(p15, 0, val, c10, c2, 0);
		break;
	case 0x0f000a21:
		SDBGP_MCR(p15, 0, val, c10, c2, 1);
		break;
	case 0x0f000b10:
		if ((SDBGP_MRC(p15, 0, c1, c1, 2) & 0x10000) == 0)
			return SDBGP_ERROR_INVALID_ARGUMENT;
		SDBGP_MCR(p15, 0, val, c11, c1, 0);
		break;
	case 0x0f000b11:
		if ((SDBGP_MRC(p15, 0, c1, c1, 2) & 0x10000) == 0)
			return SDBGP_ERROR_INVALID_ARGUMENT;
		SDBGP_MCR(p15, 0, val, c11, c1, 1);
		break;
	case 0x0f000b21:
		if ((SDBGP_MRC(p15, 0, c1, c1, 2) & 0x10000) == 0)
			return SDBGP_ERROR_INVALID_ARGUMENT;
		SDBGP_MCR(p15, 0, 0, c11, c2, 1);
		break;
	case 0x0f000b30:
		if ((SDBGP_MRC(p15, 0, c1, c1, 2) & 0x10000) == 0)
			return SDBGP_ERROR_INVALID_ARGUMENT;
		SDBGP_MCR(p15, 0, 0, c11, c3, 0);
		break;
	case 0x0f000b31:
		if ((SDBGP_MRC(p15, 0, c1, c1, 2) & 0x10000) == 0)
			return SDBGP_ERROR_INVALID_ARGUMENT;
		SDBGP_MCR(p15, 0, 0, c11, c3, 1);
		break;
	case 0x0f000b32:
		if ((SDBGP_MRC(p15, 0, c1, c1, 2) & 0x10000) == 0)
			return SDBGP_ERROR_INVALID_ARGUMENT;
		SDBGP_MCR(p15, 0, 0, c11, c3, 2);
		break;
	case 0x0f000c00:
		SDBGP_MCR(p15, 0, val, c12, c0, 0);
		break;
	case 0x0f000d01:
		if (cpuCtx != NULL)
			cpuCtx[23] = val;
		else
			SDBGP_MCR(p15, 0, val, c13, c0, 1);
		break;
	case 0x0f000d02:
		if (cpuCtx != NULL)
			cpuCtx[21] = val;
		else
			SDBGP_MCR(p15, 0, val, c13, c0, 3);
		break;
	case 0x0f000d03:
		if (cpuCtx != NULL)
			cpuCtx[20] = val;
		else
			SDBGP_MCR(p15, 0, val, c13, c0, 3);
		break;
	case 0x0f000d04:
		SDBGP_MCR(p15, 0, val, c13, c0, 4);
		break;
	case 0x0f000f00:
		SDBGP_MCR(p15, 0, val, c15, c0, 0);
		break;
	case 0x0f002000:
		SDBGP_MCR(p15, 2, val, c0, c0, 0);
		break;
	default:
		break;
	}
	return 0;
}
