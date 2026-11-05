/*
 * System group (0x10) command handlers: target info, registers, virtual and
 * physical memory read/write, MMU page table queries, hardware and software
 * breakpoints/watchpoints, and system break/continue/step control.
 */

#include <fw.h>
#include <scetypes.h>
#include <sysmem/cpu.h>
#include <sysmem/dipsw.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <excpmgr/excpmgr.h>
#include <intrmgr/intrmgr.h>
#include <processmgr/processmgr.h>
#include <threadmgr/threadmgr.h>
#include "sdbgp_internal.h"

SceSize strnlen(const char *s, SceSize maxlen);
uint32_t SceSysrootForKernel_EEB867C0(void);
int SceExcpmgrForKernel_B615A7DA(void);

#define SCE_SDBGP_ERROR_INVALID_ARGUMENT	SDBGP_ERROR_PACKET_SIZE
#define SCE_SDBGP_ERROR_FAILED			SDBGP_ERROR_INVALID_ARGUMENT
#define SCE_SDBGP_ERROR_NO_MEMORY		((int)0x8002710b)
#define SDBGP_KERNEL_PID			SDBGP_KERNEL_PROCESS_ID
#define SDBGP_RESULT_INTERNAL_ERROR		6
#define SDBGP_RESULT_INVALID_SIZE		SDBGP_RESULT_PACKET_TOO_SHORT
#define SDBGP_RESULT_TOO_LARGE			SDBGP_RESULT_PACKET_TOO_LARGE
#define SDBGP_STOP_HWWATCH			SDBGP_STOP_HWWTPT
#define SDBGP_STOP_REQUEST			SDBGP_STOP_USER_DEBUG_EVENT
#define sdfbcopy(dst, src, n)			SceDeci4pSDfMgrForKernel_5996E2DB(dst, src, n)
#define sdfbzero(dst, n)			SceDeci4pSDfMgrForKernel_8A3B06F2(dst, 0, n)

int
sdbgpPackPhyMemPartInfo(uint32_t *out, const SceKernelPhyMemPartInfoForDebugger *info)
{
	SceUID uid = info->uid;
	const char *name = info->name;
	uint32_t freeSize = info->freeSize;
	uint32_t freeUnitCount = info->freeUnitCount;
	uint32_t segmentCount = info->segmentCount;
	uint32_t freeSizeMin = info->freeSizeMin;
	uint32_t i;

	out[0] = 8 * (info->freeBlockCount + 125);
	out[1] = uid;
	if (name != NULL)
		sdfbcopy(&out[2], name, strnlen(name, 32));

	out[10] = info->type;
	out[11] = info->segments[0].pbase;
	out[12] = info->segments[0].psize;
	out[13] = freeSize;
	out[24] = segmentCount;
	out[14] = freeUnitCount;
	for (i = 0; i < 9; i++)
		out[15 + i] = 0;

	for (i = 0; i < 16; i++) {
		uint32_t *seg = &out[25 + 14 * i];
		uint32_t j;

		seg[0] = info->segments[i].pbase;
		seg[1] = info->segments[i].psize;
		seg[2] = freeSize;
		seg[3] = freeSizeMin;
		seg[4] = freeUnitCount;
		for (j = 0; j < 9; j++)
			seg[5 + j] = 0;
	}

	out[249] = info->freeBlockCount;
	if (info->freeBlockCount != 0) {
		for (i = 0; i < info->freeBlockCount; i++) {
			out[250 + 2 * i] = info->freeBlocks[i].pbase;
			out[251 + 2 * i] = info->freeBlocks[i].size;
		}
	}
	return 0;
}

void
sdbgpWriteSuspendedArmRegs(const uint32_t *req, uint32_t *armRegs)
{
	uint32_t count;
	int mode;
	uint32_t i;

	if (req == NULL)
		return;

	count = req[7];
	mode = (armRegs[0] != 0) ? 1 : 0;
	for (i = 0; i < count; i++) {
		uint32_t regId = req[8 + 2 * i];
		uint32_t val = req[9 + 2 * i];
		uint32_t group = regId & 0xffff0000U;

		if (group == 0x20000U) {
			if (regId == 0x20000U) {
				if (mode != 0)
					armRegs[49] = val;
				else
					armRegs[16] = val;
			}
		} else if (group == 0xc0000U) {
			switch (regId) {
			case 0xc0000U: armRegs[30] = val; break;
			case 0xc0001U: armRegs[31] = val; break;
			case 0xc0002U: armRegs[32] = val; break;
			case 0xc0003U: armRegs[33] = val; break;
			case 0xc0004U: armRegs[34] = val; break;
			case 0xc0005U: armRegs[35] = val; break;
			case 0xc0006U: armRegs[36] = val; break;
			case 0xc0007U: armRegs[37] = val; break;
			case 0xc0008U: armRegs[38] = val; break;
			case 0xc0009U: armRegs[39] = val; break;
			case 0xc000aU: armRegs[40] = val; break;
			case 0xc000bU: armRegs[41] = val; break;
			default: break;
			}
		} else if (group == 0x10000U) {
			if (regId <= 0x1000cU)
				armRegs[(regId & 0xffffU) + 1] = val;
			else if (regId == 0x1000dU)
				armRegs[14] = val;
			else if (regId == 0x1000eU)
				armRegs[15] = val;

			if (regId == 0x1000fU) {
				if (mode != 0)
					armRegs[48] = val;
				else
					armRegs[15] = val;
			} else if (regId == 0x10020U) {
				armRegs[47] = val;
			}
		}
	}
}

void
sdbgpWriteSuspendedVfpRegs(const uint32_t *req, uint32_t *vfpRegs)
{
	uint32_t count;
	uint32_t i;

	if (req == NULL)
		return;

	count = req[7];
	for (i = 0; i < count; i++) {
		uint32_t regId = req[8 + 2 * i];
		uint32_t val = req[9 + 2 * i];
		uint32_t group = regId & 0xffff0000U;

		if (group == 0xa0000U) {
			if (regId <= 0xa003fU)
				vfpRegs[regId & 0xffffU] = val;
		} else if (group != 0x60000U && regId == 0x20001U) {
			vfpRegs[64] = val;
		}
	}
}

__attribute__((noinline)) int
sdbgpReadSuspendedThreadRegs(const uint32_t *req, uint32_t *reply, const uint32_t *armRegs,
			     uintptr_t vfpRegs, const uint32_t *cp15Regs, uint32_t excTtbr0)
{
	uint32_t count;
	int mode;
	uint32_t i;

	if (reply == NULL || req == NULL)
		return 1;

	count = req[7];
	if (armRegs != NULL)
		mode = (armRegs[0] != 0) ? 1 : 0;
	else
		mode = 0;

	for (i = 0; i < count; i++) {
		uint32_t regId = req[8 + i];
		uint32_t *dst = &reply[10 + i];
		uint32_t group = regId & 0xffff0000U;

		*dst = 0;
		switch (group) {
		case 0x10000U:
			if (armRegs == NULL)
				return 0;
			if (regId <= 0x1000cU) {
				*dst = armRegs[(regId & 0xfU) + 1];
			} else if (regId == 0x1000dU) {
				*dst = armRegs[14];
			} else if (regId == 0x1000eU) {
				*dst = armRegs[15];
			} else if (regId == 0x1000fU) {
				*dst = (mode == 1) ? armRegs[48] : armRegs[15];
			} else if (regId == 0x10020U) {
				*dst = armRegs[47];
			}
			break;
		case 0x20000U:
			if (armRegs == NULL)
				return 0;
			if (regId == 0x20000U) {
				*dst = (mode == 1) ? armRegs[49] : armRegs[16];
			} else if (regId == 0x20001U) {
				if (vfpRegs != 0)
					*dst = *(const uint32_t *)(vfpRegs + 0x100);
				else
					*dst = 0xffffffffU;
			}
			break;
		case 0x40000U:
			if (cp15Regs != NULL) {
				switch (regId) {
				case 0x40000U: *dst = cp15Regs[0]; break;
				case 0x40001U: *dst = cp15Regs[1]; break;
				case 0x40002U: *dst = cp15Regs[2]; break;
				case 0x40003U: *dst = cp15Regs[3]; break;
				case 0x40004U: *dst = cp15Regs[4]; break;
				case 0x40005U: *dst = cp15Regs[5]; break;
				case 0x40006U: *dst = cp15Regs[6]; break;
				case 0x40007U: *dst = cp15Regs[7]; break;
				case 0x40008U: *dst = cp15Regs[8]; break;
				case 0x40009U: *dst = cp15Regs[9]; break;
				case 0x4000aU: *dst = cp15Regs[10]; break;
				case 0x4000bU: *dst = cp15Regs[11]; break;
				case 0x4000cU: *dst = cp15Regs[12]; break;
				case 0x4000dU: *dst = cp15Regs[20]; break;
				case 0x4000eU: *dst = cp15Regs[21]; break;
				case 0x4000fU: *dst = cp15Regs[22]; break;
				case 0x40010U: *dst = cp15Regs[23]; break;
				case 0x40011U: *dst = cp15Regs[15]; break;
				case 0x40012U: *dst = cp15Regs[14]; break;
				default: *dst = 0xffffffffU; break;
				}
			} else {
				*dst = 0xffffffffU;
			}
			break;
		case 0xa0000U:
			if (vfpRegs != 0)
				*dst = *(const uint32_t *)(vfpRegs + 4 * (regId & 0xffffU));
			else
				*dst = 0xffffffffU;
			break;
		case 0xc0000U:
			if (armRegs == NULL)
				return 0;
			switch (regId) {
			case 0xc0000U: *dst = armRegs[30]; break;
			case 0xc0001U: *dst = armRegs[31]; break;
			case 0xc0002U: *dst = armRegs[32]; break;
			case 0xc0003U: *dst = armRegs[33]; break;
			case 0xc0004U: *dst = armRegs[34]; break;
			case 0xc0005U: *dst = armRegs[35]; break;
			case 0xc0006U: *dst = armRegs[36]; break;
			case 0xc0007U: *dst = armRegs[37]; break;
			case 0xc0008U: *dst = armRegs[38]; break;
			case 0xc0009U: *dst = armRegs[39]; break;
			case 0xc000aU: *dst = armRegs[40]; break;
			case 0xc000bU: *dst = armRegs[41]; break;
			default: break;
			}
			break;
		case 0x100000U:
			switch (regId) {
			case 0x100000U:
			case 0x100001U:
			case 0x100002U:
				sdbgpReadFiqRegister(regId, dst);
				break;
			case 0x100010U:
			case 0x100011U:
			case 0x100012U:
				sdbgpReadIrqRegister(regId, dst);
				/* FALLTHROUGH */
			case 0x100020U:
			case 0x100021U:
			case 0x100022U:
				sdbgpReadSvcRegister(regId, dst);
				break;
			case 0x100030U:
			case 0x100031U:
			case 0x100032U:
				sdbgpReadAbtRegister(regId, dst);
				break;
			case 0x100040U:
			case 0x100041U:
			case 0x100042U:
			case 0x100043U:
			case 0x100044U:
			case 0x100045U:
			case 0x100046U:
			case 0x100047U:
				sdbgpReadUndRegister(regId, dst);
				break;
			default:
				break;
			}
			break;
		case 0xa000000U:
			sdbgpReadVfpSystemReg(regId, dst, vfpRegs);
			break;
		case 0xe000000U:
			sdbgpReadDebugReg(regId, dst, (uintptr_t)armRegs, (SceUID)req[5]);
			break;
		case 0xf000000U:
			sdbgpReadCp15Reg(regId, dst, armRegs, excTtbr0);
			break;
		default:
			break;
		}
	}
	return 0;
}

int
sdbgpGetRegisters(SdbgpCpuState *cpu, const uint32_t *req, uint32_t *reply,
		  SceUID *pidOut, SceUID *tidOut)
{
	ThreadmgrThreadInfoForDebugger info;
	int hasTarget;
	int excNest;

	sdfbzero(&info, sizeof(info));
	if (*tidOut == -1) {
		hasTarget = 0;
	} else {
		info.size = sizeof(info);
		hasTarget = 1;
		if (SceThreadmgrForKernel_C976719E(*tidOut, 0, &info) < 0)
			*pidOut = -1;
		else
			*pidOut = info.processId;
	}

	excNest = SceExcpmgrForKernel_B615A7DA();
	if (excNest > 0) {
		uintptr_t excBottom = (uintptr_t)sceKernelGetExcpStackBottom(sceKernelCpuId());
		const uint32_t *excFrame;
		uint32_t count;
		uint32_t i;

		if (!hasTarget) {
			if (cpu->stoppedTid != 0) {
				*tidOut = cpu->stoppedTid;
				info.size = sizeof(info);
				if (SceThreadmgrForKernel_C976719E(cpu->stoppedTid, 0, &info) < 0)
					*pidOut = -1;
				else
					*pidOut = info.processId;
			}
			if (cpu->stoppedTid == 0)
				*pidOut = SDBGP_KERNEL_PID;
		} else if (cpu->stoppedTid != *tidOut) {
			return sdbgpReadSuspendedThreadRegs(
				req, reply, (const uint32_t *)info.registers,
				info.vfpContext,
				(const uint32_t *)info.userFrame,
				info.context);
		}

		excFrame = (const uint32_t *)(excBottom - 0x400);
		reply[7] = *pidOut;
		reply[8] = *tidOut;
		count = req[7];
		for (i = 0; i < count; i++) {
			uint32_t regId = req[8 + i];
			uint32_t *dst = &reply[10 + i];
			uint32_t group = regId & 0xffff0000U;

			*dst = 0;
			switch (group) {
			case 0x10000U:
				switch (regId) {
				case 0x10000U: *dst = excFrame[0]; break;
				case 0x10001U: *dst = excFrame[1]; break;
				case 0x10002U: *dst = excFrame[2]; break;
				case 0x10003U: *dst = excFrame[3]; break;
				case 0x10004U: *dst = excFrame[4]; break;
				case 0x10005U: *dst = excFrame[5]; break;
				case 0x10006U: *dst = excFrame[6]; break;
				case 0x10007U: *dst = excFrame[7]; break;
				case 0x10008U: *dst = excFrame[8]; break;
				case 0x10009U: *dst = excFrame[9]; break;
				case 0x1000aU: *dst = excFrame[10]; break;
				case 0x1000bU: *dst = excFrame[11]; break;
				case 0x1000cU: *dst = excFrame[12]; break;
				case 0x1000dU: *dst = excFrame[13]; break;
				case 0x1000eU: *dst = excFrame[14]; break;
				case 0x1000fU: *dst = excFrame[15]; break;
				case 0x10020U: *dst = excFrame[52]; break;
				default: break;
				}
				break;
			case 0x20000U:
				if (regId == 0x20000U)
					*dst = excFrame[17];
				else if (regId == 0x20001U)
					*dst = excFrame[19];
				break;
			case 0x40000U: {
				const uint32_t *tcb = (const uint32_t *)(uintptr_t)excFrame[24];
				const uint32_t *cp15 = (const uint32_t *)(uintptr_t)tcb[17];

				if (cp15 != NULL) {
					switch (regId) {
					case 0x40000U: *dst = cp15[0]; break;
					case 0x40001U: *dst = cp15[1]; break;
					case 0x40002U: *dst = cp15[2]; break;
					case 0x40003U: *dst = cp15[3]; break;
					case 0x40004U: *dst = cp15[4]; break;
					case 0x40005U: *dst = cp15[5]; break;
					case 0x40006U: *dst = cp15[6]; break;
					case 0x40007U: *dst = cp15[7]; break;
					case 0x40008U: *dst = cp15[8]; break;
					case 0x40009U: *dst = cp15[9]; break;
					case 0x4000aU: *dst = cp15[10]; break;
					case 0x4000bU: *dst = cp15[11]; break;
					case 0x4000cU: *dst = cp15[12]; break;
					case 0x4000dU: *dst = cp15[20]; break;
					case 0x4000eU: *dst = cp15[21]; break;
					case 0x4000fU: *dst = cp15[22]; break;
					case 0x40010U: *dst = cp15[23]; break;
					case 0x40011U: *dst = cp15[15]; break;
					case 0x40012U: *dst = cp15[14]; break;
					default: break;
					}
				}
				break;
			}
			case 0xa0000U:
				*dst = excFrame[64 + (regId & 0xffffU)];
				break;
			case 0xc0000U:
				switch (regId) {
				case 0xc0000U: *dst = excFrame[40]; break;
				case 0xc0001U: *dst = excFrame[41]; break;
				case 0xc0002U: *dst = excFrame[42]; break;
				case 0xc0003U: *dst = excFrame[43]; break;
				case 0xc0004U: *dst = excFrame[44]; break;
				case 0xc0005U: *dst = excFrame[45]; break;
				case 0xc0006U: *dst = excFrame[46]; break;
				case 0xc0007U: *dst = excFrame[47]; break;
				case 0xc0008U: *dst = excFrame[48]; break;
				case 0xc0009U: *dst = excFrame[49]; break;
				case 0xc000aU: *dst = excFrame[50]; break;
				case 0xc000bU: *dst = excFrame[51]; break;
				default: break;
				}
				break;
			case 0x100000U:
				switch (regId) {
				case 0x100000U:
				case 0x100001U:
				case 0x100002U:
					sdbgpReadFiqRegister(regId, dst);
					break;
				case 0x100010U:
				case 0x100011U:
				case 0x100012U:
					sdbgpReadIrqRegister(regId, dst);
					/* FALLTHROUGH */
				case 0x100020U:
				case 0x100021U:
				case 0x100022U:
					sdbgpReadSvcRegister(regId, dst);
					break;
				case 0x100030U:
				case 0x100031U:
				case 0x100032U:
					sdbgpReadAbtRegister(regId, dst);
					break;
				case 0x100040U:
				case 0x100041U:
				case 0x100042U:
				case 0x100043U:
				case 0x100044U:
				case 0x100045U:
				case 0x100046U:
				case 0x100047U:
					sdbgpReadUndRegister(regId, dst);
					break;
				default:
					break;
				}
				break;
			case 0xa000000U:
				sdbgpReadVfpSystemReg(regId, dst, 0);
				break;
			case 0xe000000U:
				sdbgpReadDebugReg(regId, dst, 0, (SceUID)req[5]);
				break;
			case 0xf000000U:
				sdbgpReadCp15Reg(regId, dst, NULL, excFrame[24]);
				break;
			default:
				break;
			}
		}
		return 0;
	} else {
		int intrNest = sceKernelIsIntrContext();
		uintptr_t intrBottom;
		const uint32_t *intrFrame;
		ThreadCBInfo ctxInfo;

		if (intrNest <= 0)
			return SCE_SDBGP_ERROR_FAILED;

		intrBottom = (uintptr_t)SceIntrmgrForKernel_01911612(sceKernelCpuId(), intrNest);
		sceKernelGetThreadContextInfo(&ctxInfo);
		if (hasTarget && ctxInfo.threadId != *tidOut) {
			return sdbgpReadSuspendedThreadRegs(
				req, reply, (const uint32_t *)info.registers,
				info.vfpContext,
				(const uint32_t *)info.userFrame,
				info.context);
		}

		intrFrame = (const uint32_t *)(intrBottom - 0x140);
		if (intrNest == 1) {
			const uint32_t *tcb;
			const uint32_t *armRegs;
			uintptr_t vfpRegs;
			const uint32_t *cp15Regs;
			int hasVfp;
			uint32_t count;
			uint32_t i;

			*pidOut = ctxInfo.processId;
			*tidOut = ctxInfo.threadId;
			tcb = (const uint32_t *)(uintptr_t)intrFrame[0];
			vfpRegs = tcb[4];
			cp15Regs = (const uint32_t *)(uintptr_t)tcb[17];
			armRegs = (const uint32_t *)(uintptr_t)tcb[3];
			hasVfp = (vfpRegs != 0) ? 1 : 0;
			count = req[7];
			for (i = 0; i < count; i++) {
				uint32_t regId = req[8 + i];
				uint32_t *dst = &reply[10 + i];
				uint32_t group = regId & 0xffff0000U;

				*dst = 0;
				switch (group) {
				case 0x10000U:
					if (regId <= 0x1000cU)
						*dst = armRegs[(regId & 0xfU) + 1];
					else if (regId == 0x1000dU)
						*dst = armRegs[14];
					else if (regId == 0x1000eU)
						*dst = armRegs[15];
					else if (regId == 0x1000fU)
						*dst = armRegs[48];
					else if (regId == 0x10020U)
						*dst = armRegs[47];
					break;
				case 0x20000U:
					if (regId == 0x20000U) {
						*dst = armRegs[49];
					} else if (regId == 0x20001U) {
						if (hasVfp)
							*dst = *(const uint32_t *)(vfpRegs + 0x100);
						else
							*dst = 0xffffffffU;
					}
					break;
				case 0x40000U:
					if (cp15Regs != NULL) {
						switch (regId) {
						case 0x40000U: *dst = cp15Regs[0]; break;
						case 0x40001U: *dst = cp15Regs[1]; break;
						case 0x40002U: *dst = cp15Regs[2]; break;
						case 0x40003U: *dst = cp15Regs[3]; break;
						case 0x40004U: *dst = cp15Regs[4]; break;
						case 0x40005U: *dst = cp15Regs[5]; break;
						case 0x40006U: *dst = cp15Regs[6]; break;
						case 0x40007U: *dst = cp15Regs[7]; break;
						case 0x40008U: *dst = cp15Regs[8]; break;
						case 0x40009U: *dst = cp15Regs[9]; break;
						case 0x4000aU: *dst = cp15Regs[10]; break;
						case 0x4000bU: *dst = cp15Regs[11]; break;
						case 0x4000cU: *dst = cp15Regs[12]; break;
						case 0x4000dU: *dst = cp15Regs[20]; break;
						case 0x4000eU: *dst = cp15Regs[21]; break;
						case 0x4000fU: *dst = cp15Regs[22]; break;
						case 0x40010U: *dst = cp15Regs[23]; break;
						case 0x40011U: *dst = cp15Regs[15]; break;
						case 0x40012U: *dst = cp15Regs[14]; break;
						default: break;
						}
					}
					break;
				case 0xa0000U:
					if (hasVfp)
						*dst = *(const uint32_t *)(vfpRegs + 4 * (regId & 0xffffU));
					else
						*dst = 0xffffffffU;
					break;
				case 0xc0000U:
					switch (regId) {
					case 0xc0000U: *dst = armRegs[30]; break;
					case 0xc0001U: *dst = armRegs[31]; break;
					case 0xc0002U: *dst = armRegs[32]; break;
					case 0xc0003U: *dst = armRegs[33]; break;
					case 0xc0004U: *dst = armRegs[34]; break;
					case 0xc0005U: *dst = armRegs[35]; break;
					case 0xc0006U: *dst = armRegs[36]; break;
					case 0xc0007U: *dst = armRegs[37]; break;
					case 0xc0008U: *dst = armRegs[38]; break;
					case 0xc0009U: *dst = armRegs[39]; break;
					case 0xc000aU: *dst = armRegs[40]; break;
					case 0xc000bU: *dst = armRegs[41]; break;
					default: break;
					}
					break;
				case 0x100000U:
					switch (regId) {
					case 0x100000U:
					case 0x100001U:
					case 0x100002U:
						sdbgpReadFiqRegister(regId, dst);
						break;
					case 0x100010U:
					case 0x100011U:
					case 0x100012U:
						sdbgpReadIrqRegister(regId, dst);
						/* FALLTHROUGH */
					case 0x100020U:
					case 0x100021U:
					case 0x100022U:
						sdbgpReadSvcRegister(regId, dst);
						break;
					case 0x100030U:
					case 0x100031U:
					case 0x100032U:
						sdbgpReadAbtRegister(regId, dst);
						break;
					case 0x100040U:
					case 0x100041U:
					case 0x100042U:
					case 0x100043U:
					case 0x100044U:
					case 0x100045U:
					case 0x100046U:
					case 0x100047U:
						sdbgpReadUndRegister(regId, dst);
						break;
					default:
						break;
					}
					break;
				case 0xa000000U:
					sdbgpReadVfpSystemReg(regId, dst, vfpRegs);
					break;
				case 0xe000000U:
					sdbgpReadDebugReg(regId, dst, (uintptr_t)armRegs, (SceUID)req[5]);
					break;
				case 0xf000000U:
					sdbgpReadCp15Reg(regId, dst, armRegs, (uint32_t)(uintptr_t)intrFrame);
					break;
				default:
					break;
				}
			}
			return 0;
		} else {
			uint32_t count;
			uint32_t i;

			*pidOut = SDBGP_KERNEL_PID;
			count = req[7];
			for (i = 0; i < count; i++) {
				uint32_t regId = req[8 + i];
				uint32_t *dst = &reply[10 + i];
				uint32_t group = regId & 0xffff0000U;

				*dst = 0;
				switch (group) {
				case 0x10000U:
					switch (regId) {
					case 0x10005U: *dst = intrFrame[68]; break;
					case 0x10006U: *dst = intrFrame[69]; break;
					case 0x10007U: *dst = intrFrame[70]; break;
					case 0x10008U: *dst = intrFrame[71]; break;
					case 0x10009U: *dst = intrFrame[72]; break;
					case 0x1000aU: *dst = intrFrame[73]; break;
					case 0x1000bU: *dst = intrFrame[74]; break;
					case 0x1000cU: *dst = intrFrame[75]; break;
					case 0x1000dU: *dst = intrFrame[76]; break;
					case 0x1000eU: *dst = intrFrame[77]; break;
					case 0x1000fU: *dst = intrFrame[78]; break;
					default: break;
					}
					break;
				case 0x20000U:
					if (regId == 0x20000U)
						*dst = intrFrame[79];
					else if (regId == 0x20001U)
						*dst = intrFrame[3];
					break;
				case 0xa0000U:
					*dst = intrFrame[4 + (regId & 0xffffU)];
					break;
				case 0xc0000U:
					*dst = 0xffffffffU;
					break;
				case 0x100000U:
					switch (regId) {
					case 0x100000U:
					case 0x100001U:
					case 0x100002U:
						sdbgpReadFiqRegister(regId, dst);
						break;
					case 0x100010U:
					case 0x100011U:
					case 0x100012U:
						sdbgpReadIrqRegister(regId, dst);
						/* FALLTHROUGH */
					case 0x100020U:
					case 0x100021U:
					case 0x100022U:
						sdbgpReadSvcRegister(regId, dst);
						break;
					case 0x100030U:
					case 0x100031U:
					case 0x100032U:
						sdbgpReadAbtRegister(regId, dst);
						break;
					case 0x100040U:
					case 0x100041U:
					case 0x100042U:
					case 0x100043U:
					case 0x100044U:
					case 0x100045U:
					case 0x100046U:
					case 0x100047U:
						sdbgpReadUndRegister(regId, dst);
						break;
					default:
						break;
					}
					break;
				case 0xa000000U:
					sdbgpReadVfpSystemReg(regId, dst, 0);
					break;
				case 0xe000000U:
					sdbgpReadDebugReg(regId, dst, 0, (SceUID)req[5]);
					break;
				case 0xf000000U:
					sdbgpReadCp15Reg(regId, dst, NULL, intrFrame[0]);
					break;
				default:
					break;
				}
			}
			return 0;
		}
	}
}

void
sdbgpWriteSystemRegs(const uint32_t *req, SceUID pid, uint32_t *armRegs, uintptr_t vfpRegs)
{
	uint32_t count;
	uint32_t i;

	if (req == NULL)
		return;

	count = req[7];
	for (i = 0; i < count; i++) {
		uint32_t regId = req[8 + 2 * i];
		uint32_t val = req[9 + 2 * i];
		uint32_t group = regId & 0xffff0000U;

		if (group == 0xe000000U)
			sdbgpWriteDebugReg(regId, val, (uintptr_t)armRegs, pid);
		else if (group == 0xf000000U)
			sdbgpWriteCp15Reg(regId, val, armRegs);
		else if (group == 0xa000000U)
			sdbgpWriteVfpSystemReg(regId, val, vfpRegs);
	}
}

int
sdbgpSetRegisters(SdbgpCpuState *cpu, const uint32_t *req, SceUID *pidOut, SceUID *tidOut)
{
	ThreadmgrThreadInfoForDebugger info;
	int hasTarget;
	int excNest;

	sdfbzero(&info, sizeof(info));
	if (*tidOut == -1) {
		hasTarget = 0;
	} else {
		info.size = sizeof(info);
		hasTarget = 1;
		if (SceThreadmgrForKernel_C976719E(*tidOut, 0, &info) < 0)
			*pidOut = -1;
		else
			*pidOut = info.processId;
	}

	excNest = SceExcpmgrForKernel_B615A7DA();
	if (excNest > 0) {
		uintptr_t excBottom = (uintptr_t)sceKernelGetExcpStackBottom(sceKernelCpuId());
		uint32_t *excFrame;
		uint32_t count;
		uint32_t i;

		if (!hasTarget) {
			if (cpu->stoppedTid != 0) {
				*tidOut = cpu->stoppedTid;
				info.size = sizeof(info);
				if (SceThreadmgrForKernel_C976719E(cpu->stoppedTid, 0, &info) < 0)
					*pidOut = -1;
				else
					*pidOut = info.processId;
			}
			if (cpu->stoppedTid == 0)
				*pidOut = SDBGP_KERNEL_PID;
		} else if (cpu->stoppedTid != *tidOut) {
			sdbgpWriteSuspendedArmRegs(req, (uint32_t *)info.registers);
			sdbgpWriteSuspendedVfpRegs(req, (uint32_t *)info.vfpContext);
			sdbgpWriteSystemRegs(req, info.processId,
					     (uint32_t *)info.registers,
					     info.vfpContext);
			return 0;
		}

		excFrame = (uint32_t *)(excBottom - 0x400);
		count = req[7];
		for (i = 0; i < count; i++) {
			uint32_t regId = req[8 + 2 * i];
			uint32_t val = req[9 + 2 * i];
			const uint32_t *tcb = (const uint32_t *)(uintptr_t)excFrame[24];
			uint32_t group = regId & 0xffff0000U;

			if (group == 0xa0000U) {
				excFrame[64 + (regId & 0xffffU)] = val;
			} else if (group > 0xa0000U) {
				if (group == 0xa000000U) {
					sdbgpWriteVfpSystemReg(regId, val, 0);
				} else if (group == 0xc0000U) {
					switch (regId) {
					case 0xc0000U: excFrame[40] = val; break;
					case 0xc0001U: excFrame[41] = val; break;
					case 0xc0002U: excFrame[42] = val; break;
					case 0xc0003U: excFrame[43] = val; break;
					case 0xc0004U: excFrame[44] = val; break;
					case 0xc0005U: excFrame[45] = val; break;
					case 0xc0006U: excFrame[46] = val; break;
					case 0xc0007U: excFrame[47] = val; break;
					case 0xc0008U: excFrame[48] = val; break;
					case 0xc0009U: excFrame[49] = val; break;
					case 0xc000aU: excFrame[50] = val; break;
					case 0xc000bU: excFrame[51] = val; break;
					default: break;
					}
				} else if (group == 0xe000000U) {
					sdbgpWriteDebugReg(regId, val, 0, (SceUID)tcb[14]);
				} else if (group == 0xf000000U) {
					sdbgpWriteCp15Reg(regId, val, NULL);
				}
			} else if (group == 0x10000U || group == 0x20000U) {
				switch (regId) {
				case 0x10000U: excFrame[0] = val; break;
				case 0x10001U: excFrame[1] = val; break;
				case 0x10002U: excFrame[2] = val; break;
				case 0x10003U: excFrame[3] = val; break;
				case 0x10004U: excFrame[4] = val; break;
				case 0x10005U: excFrame[5] = val; break;
				case 0x10006U: excFrame[6] = val; break;
				case 0x10007U: excFrame[7] = val; break;
				case 0x10008U: excFrame[8] = val; break;
				case 0x10009U: excFrame[9] = val; break;
				case 0x1000aU: excFrame[10] = val; break;
				case 0x1000bU: excFrame[11] = val; break;
				case 0x1000cU: excFrame[12] = val; break;
				case 0x1000dU: excFrame[13] = val; break;
				case 0x1000eU: excFrame[14] = val; break;
				case 0x1000fU: excFrame[15] = val; break;
				case 0x10020U: excFrame[52] = val; break;
				case 0x20000U: excFrame[17] = val; break;
				case 0x20001U: excFrame[19] = val; break;
				default: break;
				}
			}
		}
		return 0;
	} else {
		int intrNest = sceKernelIsIntrContext();
		uint32_t *intrFrame;
		ThreadCBInfo ctxInfo;

		if (intrNest <= 0)
			return SCE_SDBGP_ERROR_FAILED;

		intrFrame = (uint32_t *)((uintptr_t)SceIntrmgrForKernel_01911612(sceKernelCpuId(), intrNest) - 0x140);
		sceKernelGetThreadContextInfo(&ctxInfo);
		if (hasTarget && ctxInfo.threadId != *tidOut) {
			sdbgpWriteSuspendedArmRegs(req, (uint32_t *)info.registers);
			sdbgpWriteSuspendedVfpRegs(req, (uint32_t *)info.vfpContext);
			sdbgpWriteSystemRegs(req, info.processId,
					     (uint32_t *)info.registers,
					     info.vfpContext);
			return 0;
		}

		if (intrNest == 1) {
			uint32_t count;
			uint32_t i;

			*pidOut = ctxInfo.processId;
			*tidOut = ctxInfo.threadId;
			count = req[7];
			for (i = 0; i < count; i++) {
				uint32_t regId = req[8 + 2 * i];
				uint32_t val = req[9 + 2 * i];
				uint32_t *tcb = (uint32_t *)(uintptr_t)intrFrame[0];
				uint32_t *armRegs = (uint32_t *)(uintptr_t)tcb[3];
				uintptr_t vfpRegs = tcb[4];
				uint32_t group = regId & 0xffff0000U;

				if (group == 0xa0000U) {
					*(uint32_t *)(vfpRegs + 4 * (regId & 0xffffU)) = val;
				} else if (group > 0xa0000U) {
					if (group == 0xa000000U) {
						sdbgpWriteVfpSystemReg(regId, val, vfpRegs);
					} else if (group == 0xc0000U) {
						switch (regId) {
						case 0xc0000U: tcb[33] = val; break;
						case 0xc0001U: tcb[34] = val; break;
						case 0xc0002U: tcb[35] = val; break;
						case 0xc0003U: tcb[36] = val; break;
						case 0xc0004U: tcb[37] = val; break;
						case 0xc0005U: tcb[38] = val; break;
						case 0xc0006U: tcb[39] = val; break;
						case 0xc0007U: tcb[40] = val; break;
						case 0xc0008U: tcb[41] = val; break;
						case 0xc0009U: tcb[42] = val; break;
						case 0xc000aU: tcb[43] = val; break;
						case 0xc000bU: tcb[44] = val; break;
						default: break;
						}
					} else if (group == 0xe000000U) {
						sdbgpWriteDebugReg(regId, val, (uintptr_t)armRegs, (SceUID)tcb[14]);
					} else if (group == 0xf000000U) {
						sdbgpWriteCp15Reg(regId, val, armRegs);
					}
				} else if (group == 0x20000U) {
					armRegs[49] = val;
				} else if (group == 0x20001U) {
					*(uint32_t *)(vfpRegs + 0x100) = val;
				} else if (group == 0x10000U) {
					switch (regId) {
					case 0x10000U: armRegs[1] = val; break;
					case 0x10001U: armRegs[2] = val; break;
					case 0x10002U: armRegs[3] = val; break;
					case 0x10003U: armRegs[4] = val; break;
					case 0x10004U: armRegs[5] = val; break;
					case 0x10005U: armRegs[6] = val; break;
					case 0x10006U: armRegs[7] = val; break;
					case 0x10007U: armRegs[8] = val; break;
					case 0x10008U: armRegs[9] = val; break;
					case 0x10009U: armRegs[10] = val; break;
					case 0x1000aU: armRegs[11] = val; break;
					case 0x1000bU: armRegs[12] = val; break;
					case 0x1000cU: armRegs[13] = val; break;
					case 0x1000dU: armRegs[14] = val; break;
					case 0x1000eU: armRegs[15] = val; break;
					case 0x1000fU: armRegs[48] = val; break;
					case 0x10020U: armRegs[47] = val; break;
					default: break;
					}
					armRegs[49] = val;
				}
			}
			return 0;
		} else {
			uint32_t count = req[7];
			uint32_t i;

			for (i = 0; i < count; i++) {
				uint32_t regId = req[8 + 2 * i];
				uint32_t val = req[9 + 2 * i];
				uint32_t group = regId & 0xffff0000U;

				if (group == 0xa0000U) {
					intrFrame[4 + (regId & 0xffffU)] = val;
				} else if (group > 0xa0000U) {
					if (group == 0xa000000U)
						sdbgpWriteVfpSystemReg(regId, val, 0);
					else if (group == 0xe000000U)
						sdbgpWriteDebugReg(regId, val, 0, -1);
					else if (group == 0xf000000U)
						sdbgpWriteCp15Reg(regId, val, NULL);
				} else if (group == 0x10000U || group == 0x20000U) {
					switch (regId) {
					case 0x10005U: intrFrame[68] = val; break;
					case 0x10006U: intrFrame[69] = val; break;
					case 0x10007U: intrFrame[70] = val; break;
					case 0x10008U: intrFrame[71] = val; break;
					case 0x10009U: intrFrame[72] = val; break;
					case 0x1000aU: intrFrame[73] = val; break;
					case 0x1000bU: intrFrame[74] = val; break;
					case 0x1000cU: intrFrame[75] = val; break;
					case 0x1000dU: intrFrame[76] = val; break;
					case 0x1000eU: intrFrame[77] = val; break;
					case 0x1000fU: intrFrame[78] = val; break;
					case 0x20000U: intrFrame[79] = val; break;
					case 0x20001U: intrFrame[3] = val; break;
					default: break;
					}
				}
			}
			*pidOut = SDBGP_KERNEL_PID;
			return 0;
		}
	}
}

__attribute__((noinline)) int
sdbgpDecodeThumbHalfword(const uint8_t *buf, uint32_t *out)
{
	uint32_t hi = buf[1];
	uint32_t val = *out + (hi << 8);

	*out = val;
	*out = val + buf[0];
	return 0;
}

int
sdbgpCmd10_00(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	uint32_t *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x13) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	reply = sdbgpAllocSendBuf(cpu, 0x4c);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, 0x4c);
	sdbgpSetSendSize(cpu, 0x4c);
	sdbgpSendErrorHeader(cpu, packet->group, 0x01, 0, 0);
	reply[7] = 48;
	reply[8] = 7;
	reply[9] = 20;
	reply[10] = 6;
	reply[11] = 0;
	reply[12] = 1;
	reply[14] = FW_VERSION;
	reply[16] = SceDipswForDriver_B2AD48BE(3);
	reply[17] = (uint32_t)(uintptr_t)sceKernelSysrootGetSysroot();
	reply[18] = SceSysrootForKernel_EEB867C0();
	return 0;
}

int
sdbgpCmd10_1a(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const uint32_t *req = (const uint32_t *)packet;
	SceUID ids[2];
	int res;
	SceSize replySize;
	uint32_t *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x1f) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	ids[0] = (SceUID)req[5];
	ids[1] = (SceUID)req[6];
	if (ids[1] != -1 && isIllegalThreadId(ids[1]) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_THREAD_ID);
		return SCE_SDBGP_ERROR_FAILED;
	}
	if (isIllegalGetRegisterState() != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       0x20020);
		return SCE_SDBGP_ERROR_FAILED;
	}
	res = isIllegalGetRegisterIndex();
	if (res != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       0x20021);
		return SCE_SDBGP_ERROR_FAILED;
	}

	replySize = 4 * (req[7] + 10);
	if (replySize > SDBGP_PACKET_MAX) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_TOO_LARGE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	reply = sdbgpAllocSendBuf(cpu, replySize);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, replySize);
	sdbgpSetSendSize(cpu, replySize);
	res = sdbgpGetRegisters(cpu, req, reply, &ids[0], &ids[1]);
	if (res < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	sdbgpSendErrorHeader(cpu, packet->group, 0x1b, 0, 0);
	reply[9] = req[7];
	reply[7] = ids[0];
	reply[8] = ids[1];
	return 0;
}

int
sdbgpCmd10_1c(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const uint32_t *req = (const uint32_t *)packet;
	SceUID ids[2];
	int res;
	uint32_t *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x1f) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	ids[0] = (SceUID)req[5];
	ids[1] = (SceUID)req[6];
	if (ids[1] != -1 && isIllegalThreadId(ids[1]) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_THREAD_ID);
		return SCE_SDBGP_ERROR_FAILED;
	}
	if (isIllegalSetRegisterState() != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       0x20020);
		return SCE_SDBGP_ERROR_FAILED;
	}
	res = isIllegalSetRegisterIndex();
	if (res != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       0x20021);
		return SCE_SDBGP_ERROR_FAILED;
	}

	reply = sdbgpAllocSendBuf(cpu, 0x28);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, 0x28);
	sdbgpSetSendSize(cpu, 0x28);
	res = sdbgpSetRegisters(cpu, req, &ids[0], &ids[1]);
	if (res < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
		return SCE_SDBGP_ERROR_FAILED;
	}

	sdbgpSendErrorHeader(cpu, packet->group, 0x1d, 0, 0);
	reply[9] = req[7];
	reply[7] = ids[0];
	reply[8] = ids[1];
	return 0;
}

int
sdbgpCmd10_08(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const uint32_t *req = (const uint32_t *)packet;
	SceUID pid;
	void *work;
	uint32_t *reply;
	SceKernelProcessInfo *procInfo;
	AddressSpaceInfo *asInfo;
	SceKernelMMUL1AllInfo *l1Info;
	SceUID asid;
	int res;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x17) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	pid = (req[5] != 0) ? (SceUID)req[5] : SDBGP_KERNEL_PID;
	if (isIllegalProcessId(pid) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SCE_SDBGP_ERROR_FAILED;
	}

	work = sdbgpAllocWorkBuf(cpu, SDBGP_PACKET_MAX);
	reply = sdbgpAllocSendBuf(cpu, 0x402c);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, 0x402c);
	sdbgpSetSendSize(cpu, 0x402c);
	procInfo = (SceKernelProcessInfo *)work;
	sdfbzero(procInfo, sizeof(*procInfo));
	procInfo->size = sizeof(*procInfo);
	res = sceKernelGetProcessInfo(pid, procInfo);
	if (res >= 0) {
		SceUID procAsid = procInfo->addressSpace;

		asInfo = (AddressSpaceInfo *)work;
		sdfbzero(asInfo, sizeof(*asInfo));
		asInfo->size = sizeof(*asInfo);
		res = sceKernelGetAddressSpaceInfo(procAsid, asInfo);
		if (res >= 0) {
			asid = asInfo->uid;
			l1Info = (SceKernelMMUL1AllInfo *)work;
			sdfbzero(l1Info, sizeof(*l1Info));
			l1Info->size = sizeof(*l1Info);
			sceKernelAddressSpaceGetMMUL1AllInfo(asid, l1Info);
			{
				uint32_t split = l1Info->split;
				uint32_t l2Count = l1Info->l2Count;

				reply[8] = sizeof(*l1Info);
				reply[9] = split;
				reply[10] = l2Count;
			}
			sdfbcopy(&reply[11], l1Info->table, 0x4000);
			sdbgpSendErrorHeader(cpu, packet->group, 0x09, 0, 0);
			reply[7] = pid;
			return 0;
		}
	}

	sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
	return SCE_SDBGP_ERROR_FAILED;
}

int
sdbgpCmd10_0a(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const uint32_t *req = (const uint32_t *)packet;
	SceUID pid;
	int err;
	uint32_t count;
	SceSize replySize;
	uint32_t *reply;
	void *work;
	SceKernelProcessInfo *procInfo;
	int res;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x1b) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	pid = (req[5] != 0) ? (SceUID)req[5] : SDBGP_KERNEL_PID;
	err = isIllegalProcessId(pid);
	if (err != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SCE_SDBGP_ERROR_FAILED;
	}

	count = req[6];
	if (count >= 0x40)
		count = 0x40;
	replySize = 1028 * count + 36;
	reply = sdbgpAllocSendBuf(cpu, replySize);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, replySize);
	sdbgpSetSendSize(cpu, replySize);
	work = sdbgpAllocWorkBuf(cpu, SDBGP_PACKET_MAX);
	procInfo = (SceKernelProcessInfo *)work;
	sdfbzero(procInfo, sizeof(*procInfo));
	procInfo->size = sizeof(*procInfo);
	res = sceKernelGetProcessInfo(pid, procInfo);
	if (res >= 0) {
		SceUID procAsid = procInfo->addressSpace;
		AddressSpaceInfo *asInfo = (AddressSpaceInfo *)work;

		sdfbzero(asInfo, sizeof(*asInfo));
		asInfo->size = sizeof(*asInfo);
		res = sceKernelGetAddressSpaceInfo(procAsid, asInfo);
		if (res >= 0) {
			SceUID asid = asInfo->uid;
			uint32_t n = (req[6] >= 0x40) ? 0x40 : req[6];
			uint32_t i;

			for (i = 0; i < n; i++) {
				SceKernelMMUL2AllInfo *l2Info = (SceKernelMMUL2AllInfo *)work;
				uintptr_t va = req[7 + i];
				uint32_t *dst = &reply[9 + 257 * i];

				sdfbzero(l2Info, sizeof(*l2Info));
				l2Info->size = sizeof(*l2Info);
				if (sceKernelAddressSpaceGetMMUL2AllInfo(asid, va, l2Info) >= 0) {
					dst[0] = va;
					sdfbcopy(&dst[1], l2Info->table, 1024);
				} else {
					dst[0] = 0xffffffffU;
				}
			}
			sdbgpSendErrorHeader(cpu, packet->group, 0x0b, 0, 0);
			reply[7] = pid;
			reply[8] = count;
			return 0;
		}
	}

	sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
	return SCE_SDBGP_ERROR_FAILED;
}

int
sdbgpCmd10_10(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const uint32_t *req = (const uint32_t *)packet;
	SceSize size;
	SceUID pid;
	uintptr_t addr;
	int isKernelAddr;
	int res;
	SceSize replySize;
	uint32_t *reply;
	uint8_t *work;
	SceSSize i, n;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x1f) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	size = req[7];
	pid = (req[5] != 0) ? (SceUID)req[5] : SDBGP_KERNEL_PID;
	addr = req[6];
	if (isIllegalSize(size) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 7);
		return SCE_SDBGP_ERROR_FAILED;
	}
	if (isIllegalProcessId(pid) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SCE_SDBGP_ERROR_FAILED;
	}

	if (pid == SDBGP_KERNEL_PID) {
		if (isIllegalAddress(SDBGP_KERNEL_PID, addr, size, 1) != 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
				       SDBGP_RESULT_ILLEGAL_ADDRESS);
			return SCE_SDBGP_ERROR_FAILED;
		}
		res = isIllegalMemoryType((const void *)addr);
		if (res != 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
			return SCE_SDBGP_ERROR_FAILED;
		}
		isKernelAddr = 1;
	} else if (isIllegalAddress(pid, addr, size, 16) == 0) {
		isKernelAddr = 0;
	} else if (isIllegalAddress(pid, addr, size, 1) == 0) {
		isKernelAddr = 1;
	} else {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_ADDRESS);
		return SCE_SDBGP_ERROR_FAILED;
	}

	res = isAddressOverflow(addr, size);
	if (res != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ADDRESS_OVERFLOW);
		return SCE_SDBGP_ERROR_FAILED;
	}

	replySize = req[7] + 40;
	if (replySize > SDBGP_PACKET_MAX) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_TOO_LARGE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	reply = sdbgpAllocSendBuf(cpu, replySize);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, replySize);
	sdbgpSetSendSize(cpu, replySize);
	work = sdbgpAllocWorkBuf(cpu, SDBGP_PACKET_MAX);
	sdfbzero(work, SDBGP_PACKET_MAX);
	if (isKernelAddr) {
		sdfbcopy(work, (const void *)addr, size);
	} else {
		res = sceKernelCopyFromUserProc(pid, work, (const void *)addr, size);
		if (res < 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
			return SCE_SDBGP_ERROR_FAILED;
		}
	}

	res = swbkptHideInBuffer(pid, addr, size, work);
	if (res < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
		return SCE_SDBGP_ERROR_FAILED;
	}

	n = (SceSSize)req[7];
	for (i = 0; i < n; i++)
		((uint8_t *)reply)[40 + i] = work[i];

	sdbgpSendErrorHeader(cpu, packet->group, 0x11, 0, 0);
	reply[7] = pid;
	reply[8] = size;
	reply[9] = addr;
	return 0;
}

int
sdbgpCmd10_94(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const uint32_t *req = (const uint32_t *)packet;
	SceSize size;
	SceUID pid;
	uintptr_t addr;
	uint32_t width;
	int isKernelAddr;
	int res;
	uint32_t *reply;
	uint8_t *work;
	SceSSize i;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x23) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	size = req[7];
	pid = (SceUID)req[5];
	addr = req[6];
	width = req[8];
	if (isIllegalSize(size) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 7);
		return SCE_SDBGP_ERROR_FAILED;
	}
	if (isIllegalProcessId(pid) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SCE_SDBGP_ERROR_FAILED;
	}

	if (pid == SDBGP_KERNEL_PID) {
		if (isIllegalAddress(SDBGP_KERNEL_PID, addr, size, 1) != 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
				       SDBGP_RESULT_ILLEGAL_ADDRESS);
			return SCE_SDBGP_ERROR_FAILED;
		}
		isKernelAddr = 1;
	} else if (isIllegalAddress(pid, addr, size, 16) == 0) {
		isKernelAddr = 0;
	} else if (isIllegalAddress(pid, addr, size, 1) == 0) {
		isKernelAddr = 1;
	} else {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_ADDRESS);
		return SCE_SDBGP_ERROR_FAILED;
	}

	if (isIllegalAccessSize(width) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 0x20031);
		return SCE_SDBGP_ERROR_FAILED;
	}
	res = isAddressOverflow(addr, size);
	if (res != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ADDRESS_OVERFLOW);
		return SCE_SDBGP_ERROR_FAILED;
	}
	if (size + 44 > SDBGP_PACKET_MAX) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_TOO_LARGE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	reply = sdbgpAllocSendBuf(cpu, size + 44);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, size + 44);
	sdbgpSetSendSize(cpu, size + 44);
	work = sdbgpAllocWorkBuf(cpu, SDBGP_PACKET_MAX);
	sdfbzero(work, SDBGP_PACKET_MAX);
	if (isKernelAddr) {
		SceSize off;

		switch (width) {
		case 0:
			for (off = 0; off < size; off += 1)
				sceKernelVMRead8(&work[off], (const void *)(addr + off));
			break;
		case 1:
			for (off = 0; off < size; off += 2)
				sceKernelVMRead16((uint16_t *)&work[off], (const void *)(addr + off));
			break;
		case 2:
		case 3:
		case 4:
			for (off = 0; off < size; off += 4)
				sceKernelVMRead32((uint32_t *)&work[off], (const void *)(addr + off));
			break;
		default:
			break;
		}
	} else {
		res = sceKernelCopyFromUserProc(pid, work, (const void *)addr, size);
		if (res < 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
			return SCE_SDBGP_ERROR_FAILED;
		}
	}

	res = swbkptHideInBuffer(pid, addr, size, work);
	if (res < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
		return SCE_SDBGP_ERROR_FAILED;
	}

	for (i = 0; i < (SceSSize)size; i++)
		((uint8_t *)reply)[44 + i] = work[i];

	sdbgpSendErrorHeader(cpu, packet->group, 0x95, 0, 0);
	reply[7] = pid;
	reply[8] = size;
	reply[9] = addr;
	reply[10] = width;
	return 0;
}

int
sdbgpCmd10_12(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const uint32_t *req = (const uint32_t *)packet;
	SceUID pid;
	uintptr_t addr;
	SceSize size;
	int isKernelAddr;
	int res;
	uint32_t *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x1f) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	pid = (req[5] != 0) ? (SceUID)req[5] : SDBGP_KERNEL_PID;
	addr = req[6];
	size = req[7];
	if (isIllegalProcessId(pid) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SCE_SDBGP_ERROR_FAILED;
	}

	if (pid == SDBGP_KERNEL_PID) {
		if (isIllegalAddress(SDBGP_KERNEL_PID, addr, size, 1) != 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
				       SDBGP_RESULT_ILLEGAL_ADDRESS);
			return SCE_SDBGP_ERROR_FAILED;
		}
		res = isIllegalMemoryType((const void *)addr);
		if (res != 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
			return SCE_SDBGP_ERROR_FAILED;
		}
		isKernelAddr = 1;
	} else if (isIllegalAddress(pid, addr, size, 16) == 0) {
		isKernelAddr = 0;
	} else if (isIllegalAddress(pid, addr, size, 1) == 0) {
		isKernelAddr = 1;
	} else {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_ADDRESS);
		return SCE_SDBGP_ERROR_FAILED;
	}

	res = isAddressOverflow(addr, size);
	if (res != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ADDRESS_OVERFLOW);
		return SCE_SDBGP_ERROR_FAILED;
	}

	reply = sdbgpAllocSendBuf(cpu, 0x24);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, 0x24);
	sdbgpSetSendSize(cpu, 0x24);
	if (!isKernelAddr) {
		res = sceKernelCopyToUserProcTextDomainForDBGP(pid, (void *)addr, &req[8], size);
		if (res < 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
			return SCE_SDBGP_ERROR_FAILED;
		}
	} else {
		uint32_t dacr;
		uintptr_t start = addr & ~0x1fU;
		uintptr_t end = (addr + size + 0x1fU) & ~0x1fU;

		asm volatile("mrc p15, 0, %0, c3, c0, 0" : "=r"(dacr));
		asm volatile("mcr p15, 0, %0, c3, c0, 0" : : "r"(0xffff0000U) : "memory");
		memcpy((void *)addr, &req[8], size);
		sceKernelDcacheCleanRangeForL1WBWA((void *)start, end - start);
		asm volatile("mcr p15, 0, %0, c3, c0, 0" : : "r"(dacr) : "memory");
	}

	sceKernelL1IcacheInvalidateEntireAllCore();
	sdbgpSendErrorHeader(cpu, packet->group, 0x13, 0, 0);
	reply[7] = pid;
	reply[8] = req[7];
	return 0;
}

int
sdbgpCmd10_96(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const uint32_t *req = (const uint32_t *)packet;
	SceUID pid;
	uintptr_t addr;
	SceSize size;
	uint32_t width;
	int isKernelAddr;
	int res;
	uint32_t *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x23) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	pid = (SceUID)req[5];
	addr = req[6];
	size = req[7];
	width = req[8];
	if (isIllegalProcessId(pid) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SCE_SDBGP_ERROR_FAILED;
	}

	if (pid == SDBGP_KERNEL_PID) {
		if (isIllegalAddress(SDBGP_KERNEL_PID, addr, size, 1) != 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
				       SDBGP_RESULT_ILLEGAL_ADDRESS);
			return SCE_SDBGP_ERROR_FAILED;
		}
		isKernelAddr = 1;
	} else if (isIllegalAddress(pid, addr, size, 16) == 0) {
		isKernelAddr = 0;
	} else if (isIllegalAddress(pid, addr, size, 1) == 0) {
		isKernelAddr = 1;
	} else {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_ADDRESS);
		return SCE_SDBGP_ERROR_FAILED;
	}

	if (isIllegalAccessSize(width) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 0x20031);
		return SCE_SDBGP_ERROR_FAILED;
	}
	res = isAddressOverflow(addr, size);
	if (res != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ADDRESS_OVERFLOW);
		return SCE_SDBGP_ERROR_FAILED;
	}

	reply = sdbgpAllocSendBuf(cpu, 0x28);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, 0x28);
	sdbgpSetSendSize(cpu, 0x28);
	if (isKernelAddr) {
		const uint8_t *src = (const uint8_t *)&req[9];
		uintptr_t start = addr & ~0x3fU;
		SceSize off;

		switch (width) {
		case 0:
			for (off = 0; off < size; off += 1)
				sceKernelVMWrite8((void *)(addr + off), src[off]);
			break;
		case 1:
			for (off = 0; off < size; off += 2)
				sceKernelVMWrite16((void *)(addr + off), *(const uint16_t *)&src[off]);
			break;
		case 2:
		case 3:
		case 4:
			for (off = 0; off < size; off += 4)
				sceKernelVMWrite32((void *)(addr + off), *(const uint32_t *)&src[off]);
			break;
		default:
			break;
		}
		sceKernelDcacheCleanInvalidateRange((void *)start,
						    ((addr + size - start) & ~0x3fU) + 64);
	} else {
		res = sceKernelCopyToUserProcTextDomainForDBGP(pid, (void *)addr, &req[9], size);
		if (res < 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
			return SCE_SDBGP_ERROR_FAILED;
		}
	}

	sceKernelL1IcacheInvalidateEntireAllCore();
	sdbgpSendErrorHeader(cpu, packet->group, 0x97, 0, 0);
	reply[7] = pid;
	reply[8] = size;
	reply[9] = width;
	return 0;
}

int
sdbgpCmd10_30(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const uint32_t *req = (const uint32_t *)packet;
	uint32_t id;
	int res;
	uint32_t count;
	SceSize replySize;
	SdbgpHwwtptGetReply *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x17) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	id = req[5];
	res = isHwwatchNotOwner((SceUID)id);
	if (res != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 0x20040);
		return SCE_SDBGP_ERROR_FAILED;
	}

	if (id == 0xffffffffU) {
		count = (uint32_t)hwwtptIsSet();
		replySize = 24 * count + 32;
		if (replySize > SDBGP_PACKET_MAX) {
			sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
				       SDBGP_RESULT_TOO_LARGE);
			return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
		}
	} else {
		replySize = 56;
		count = 1;
	}

	reply = sdbgpAllocSendBuf(cpu, replySize);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, replySize);
	sdbgpSetSendSize(cpu, replySize);
	res = _getHwwtpt(reply, req[5]);
	if (res < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
		return SCE_SDBGP_ERROR_FAILED;
	}

	reply->num = count;
	sdbgpSendErrorHeader(cpu, packet->group, 0x31, 0, 0);
	return 0;
}

int
sdbgpCmd10_32(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const SdbgpHwwtptSetRequest *req = (const SdbgpHwwtptSetRequest *)packet;
	SceUID pid;
	uint32_t mode;
	int res;
	SdbgpHwwtptSetReply *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x2b) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}
	if (SceDeci4pSDfMgrForKernel_0A6437B6() == 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 0x200b0);
		return SCE_SDBGP_ERROR_FAILED;
	}

	pid = ((uint32_t)(req->processId + 1) <= 1U) ? SDBGP_KERNEL_PID : req->processId;
	if (isIllegalBreakpointAttribute() != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 1);
		return SCE_SDBGP_ERROR_FAILED;
	}
	if (isIllegalProcessId(pid) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SCE_SDBGP_ERROR_FAILED;
	}

	mode = (pid == SDBGP_KERNEL_PID) ? 1 : 16;
	if (isIllegalAddress(pid, req->value, 4, mode) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_ADDRESS);
		return SCE_SDBGP_ERROR_FAILED;
	}
	if (isIllegalAlignment() != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 0x20031);
		return SCE_SDBGP_ERROR_FAILED;
	}
	res = isHwwatchInUse();
	if (res != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 0x20041);
		return SCE_SDBGP_ERROR_FAILED;
	}

	reply = sdbgpAllocSendBuf(cpu, sizeof(*reply));
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, sizeof(*reply));
	sdbgpSetSendSize(cpu, sizeof(*reply));
	res = hwwtptSetHwwtpt(req, reply);
	if (res < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
		return SCE_SDBGP_ERROR_FAILED;
	}

	sdbgpSendErrorHeader(cpu, packet->group, 0x33, 0, 0);
	return 0;
}

int
sdbgpCmd10_34(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const uint32_t *req = (const uint32_t *)packet;
	uint32_t id;
	int res;
	uint32_t *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x17) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}
	if (SceDeci4pSDfMgrForKernel_0A6437B6() == 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 0x200b0);
		return SCE_SDBGP_ERROR_FAILED;
	}

	id = req[5];
	res = isHwwatchNotOwner((SceUID)id);
	if (res != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 0x20040);
		return SCE_SDBGP_ERROR_FAILED;
	}

	res = clearHwwtpt(id);
	if (res < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
		return SCE_SDBGP_ERROR_FAILED;
	}

	reply = sdbgpAllocSendBuf(cpu, 0x20);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, 0x20);
	sdbgpSetSendSize(cpu, 0x20);
	sdbgpSendErrorHeader(cpu, packet->group, 0x35, 0, 0);
	reply[7] = req[5];
	return 0;
}

int
sdbgpCmd10_40(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const uint32_t *req = (const uint32_t *)packet;
	SceSize count = 1;
	SceUID pid, tid;
	int isAll;
	void *work;
	SceSize replySize;
	SdbgpHwbkptListReply *reply;
	int res;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x1b) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	pid = (SceUID)req[5];
	tid = (SceUID)req[6];
	if (pid == -1 && tid == -1) {
		work = sdbgpAllocWorkBuf(cpu, 0x88000);
		res = hwbkptGetHwbkptAll((SdbgpHwbkptInfo *)work, &count);
		if (res < 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
			return SCE_SDBGP_ERROR_FAILED;
		}
		isAll = 1;
	} else {
		isAll = 0;
		work = sdbgpAllocWorkBuf(cpu, 0x88000);
	}

	replySize = 20 * count + 32;
	if (replySize > SDBGP_PACKET_MAX) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_TOO_LARGE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	reply = sdbgpAllocSendBuf(cpu, replySize);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, replySize);
	sdbgpSetSendSize(cpu, replySize);
	if (isAll) {
		reply->num = count;
		sdfbcopy(reply->info, work, 20 * count);
	} else {
		res = _getHwbkpt(reply, (SceUID)req[5], (SceUID)req[6],
				 (ThreadmgrThreadInfoForDebugger *)work);
		if (res < 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
			return SCE_SDBGP_ERROR_FAILED;
		}
	}

	sdbgpSendErrorHeader(cpu, packet->group, 0x41, 0, 0);
	return 0;
}

int
sdbgpCmd10_42(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const SdbgpHwbkptSetRequest *req = (const SdbgpHwbkptSetRequest *)packet;
	SceUID pid, tid;
	int res;
	void *reply;
	int wasStopped;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x27) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	pid = req->processId;
	tid = req->threadId;
	if (isIllegalBreakpointAttribute() != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 1);
		return SCE_SDBGP_ERROR_FAILED;
	}
	if (pid != -1 && isIllegalProcessId(pid) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SCE_SDBGP_ERROR_FAILED;
	}
	if (tid != -1 && isIllegalThreadId(tid) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_THREAD_ID);
		return SCE_SDBGP_ERROR_FAILED;
	}
	if (isIllegalAlignment() != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 0x20031);
		return SCE_SDBGP_ERROR_FAILED;
	}
	res = isHwbkptInUse(pid, tid);
	if (res != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 0x20045);
		return SCE_SDBGP_ERROR_FAILED;
	}

	reply = sdbgpAllocSendBuf(cpu, 0x1c);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, 0x1c);
	sdbgpSetSendSize(cpu, 0x1c);
	wasStopped = SceDeci4pSDfMgrForKernel_0A6437B6();
	if (!wasStopped)
		sceSDfMgrBreakSystem();

	res = hwbkptSetHwbkpt(req);
	if (res < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
		return SCE_SDBGP_ERROR_FAILED;
	}

	sdbgpSendErrorHeader(cpu, packet->group, 0x43, 0, 0);
	if (!wasStopped)
		sceSDfMgrContinueSystem();
	return 0;
}

int
sdbgpCmd10_44(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const uint32_t *req = (const uint32_t *)packet;
	SceUID pid, tid;
	uint32_t value;
	int res;
	void *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x1f) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	pid = (SceUID)req[5];
	tid = (SceUID)req[6];
	value = req[7];
	if (pid == -1 && tid == -1 && value == 0xffffffffU) {
		void *work = sdbgpAllocWorkBuf(cpu, 0x88000);
		res = hwbkptClearHwbkptAll((ThreadmgrThreadInfoForDebugger *)work);
	} else {
		if (!(pid == -1 && tid == -1)) {
			if (pid != -1 && isIllegalProcessId(pid) != 0) {
				sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
					       SDBGP_RESULT_ILLEGAL_PROCESS_ID);
				return SCE_SDBGP_ERROR_FAILED;
			}
			if (tid != -1 && isIllegalThreadId(tid) != 0) {
				sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
					       SDBGP_RESULT_ILLEGAL_THREAD_ID);
				return SCE_SDBGP_ERROR_FAILED;
			}
		}
		res = clearHwbkpt(pid, tid, value);
	}

	if (res < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
		return SCE_SDBGP_ERROR_FAILED;
	}

	reply = sdbgpAllocSendBuf(cpu, 0x1c);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, 0x1c);
	sdbgpSetSendSize(cpu, 0x1c);
	sdbgpSendErrorHeader(cpu, packet->group, 0x45, 0, 0);
	return 0;
}

int
sdbgpCmd10_48(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const uint32_t *req = (const uint32_t *)packet;
	uint32_t id;
	int res;
	SceSize replySize;
	SdbgpSwbkptListReply *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x1b) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	id = req[6];
	res = isSwbkptNotFound(id);
	if (res != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 0x20046);
		return SCE_SDBGP_ERROR_FAILED;
	}

	if (id == 0xffffffffU) {
		replySize = 36 * swbkptCount() + 32;
		if (replySize > SDBGP_PACKET_MAX) {
			sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
				       SDBGP_RESULT_TOO_LARGE);
			return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
		}
	} else {
		replySize = 68;
	}

	reply = sdbgpAllocSendBuf(cpu, replySize);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, replySize);
	sdbgpSetSendSize(cpu, replySize);
	res = swbkptGetList(reply, id);
	if (res < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
		return SCE_SDBGP_ERROR_FAILED;
	}

	sdbgpSendErrorHeader(cpu, packet->group, 0x49, 0, 0);
	return 0;
}

int
sdbgpCmd10_4a(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const SdbgpSwbkptSetRequest *req = (const SdbgpSwbkptSetRequest *)packet;
	SceUID pid, tid;
	uintptr_t addr;
	int isKernelAddr;
	int res;
	SdbgpSwbkptSetReply *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x37) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	pid = (req->processId != 0) ? req->processId : SDBGP_KERNEL_PID;
	tid = req->threadId;
	addr = req->address;
	if (isIllegalBreakpointAttribute() != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 1);
		return SCE_SDBGP_ERROR_FAILED;
	}
	if (isIllegalProcessId(pid) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SCE_SDBGP_ERROR_FAILED;
	}
	if (tid != 0 && isIllegalThreadIdOrAll(tid) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_THREAD_ID);
		return SCE_SDBGP_ERROR_FAILED;
	}

	if (pid != SDBGP_KERNEL_PID && isIllegalAddress(pid, addr, 4, 16) == 0) {
		isKernelAddr = 0;
		res = isSwbkptAlreadySet(pid, tid, addr);
	} else {
		if (isIllegalAddress(pid, addr, 4, 1) != 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
				       SDBGP_RESULT_ILLEGAL_ADDRESS);
			return SCE_SDBGP_ERROR_FAILED;
		}
		isKernelAddr = 1;
		res = isSwbkptAlreadySet(SDBGP_KERNEL_PID, tid, addr);
	}

	if (res != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 3);
		return SCE_SDBGP_ERROR_FAILED;
	}

	reply = sdbgpAllocSendBuf(cpu, sizeof(*reply));
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, sizeof(*reply));
	sdbgpSetSendSize(cpu, sizeof(*reply));
	if (isKernelAddr)
		res = swbkptSetKernel(req, reply);
	else
		res = swbkptSetUser(req, reply);
	if (res < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
		return SCE_SDBGP_ERROR_FAILED;
	}

	sdbgpSendErrorHeader(cpu, packet->group, 0x4b, 0, 0);
	return 0;
}

int
sdbgpCmd10_4c(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const uint32_t *req = (const uint32_t *)packet;
	uint32_t id;
	int res;
	uint32_t *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x17) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	id = req[5];
	res = isSwbkptNotFound(id);
	if (res != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 0x20046);
		return SCE_SDBGP_ERROR_FAILED;
	}

	reply = sdbgpAllocSendBuf(cpu, 0x20);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, 0x20);
	sdbgpSetSendSize(cpu, 0x20);
	res = swbkptClearById(id);
	if (res < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
		return SCE_SDBGP_ERROR_FAILED;
	}

	sdbgpSendErrorHeader(cpu, packet->group, 0x4d, 0, 0);
	reply[7] = req[5];
	return 0;
}

int
sdbgpCmd10_50(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	uint32_t *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x13) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	if (SceDeci4pSDfMgrForKernel_0A6437B6() == 0) {
		sdbgpUpdateStatus(cpu, 4, 1);
		sceSDfMgrBreakSystem();
		sdbgpUpdateStatus(cpu, 4, 0);
	}

	reply = sdbgpAllocSendBuf(cpu, 0x20);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, 0x20);
	sdbgpSetSendSize(cpu, 0x20);
	sdbgpSendErrorHeader(cpu, packet->group, 0x51, 0, 0);
	reply[7] = 0;
	return 0;
}

int
sdbgpCmd10_52(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const uint32_t *req = (const uint32_t *)packet;
	SdbgpState *state = sdbgpGetState();
	uint32_t stopReason = 0;
	uint32_t procStatus = 0;
	int isBkpt = 0;
	uint32_t mode;
	SceUID pid, tid;
	uintptr_t targetAddr;
	uint32_t targetThumb;
	int core;
	int excNest;
	int intrNest;
	const SdfMgrDebugCpuState *dbgCpu;
	uintptr_t stopPc;
	uint32_t stopCpsr;
	int res;
	uint32_t *reply;
	SdbgpSwbkpt tmpBkpt;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x13) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}
	if (SceDeci4pSDfMgrForKernel_0A6437B6() == 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 0x200b0);
		return SCE_SDBGP_ERROR_FAILED;
	}
	if (state->debugCpu == NULL) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INTERNAL_ERROR);
		return SCE_SDBGP_ERROR_FAILED;
	}

	mode = req[5];
	pid = (SceUID)req[6];
	tid = (SceUID)req[7];
	targetAddr = req[8];
	targetThumb = req[9];
	core = sceKernelCpuId();
	excNest = SceExcpmgrForKernel_B615A7DA();
	intrNest = sceKernelIsIntrContext();
	dbgCpu = &state->debugCpu[core];
	stopPc = dbgCpu->pc;
	stopCpsr = dbgCpu->cpsr;

	if (excNest > 0) {
		res = getCpuStopReason(cpu, &stopReason);
		if (res < 0)
			goto err_failed;
		if (pid == 0)
			pid = state->debugCpu[core].pid;
		if (tid == 0)
			tid = state->debugCpu[core].tid;
	} else {
		if (intrNest == 1) {
			pid = SDBGP_KERNEL_PID;
		} else if (intrNest > 1) {
			pid = SDBGP_KERNEL_PID;
			tid = -1;
		}
	}

	if (mode == 1) {
		if (stopReason == SDBGP_STOP_HWBKPT) {
			sdbgpUpdateStatus(cpu, 0x1100, mode);
			hwbkptDisableHWBP();
			SceDeci4pSDfMgrForKernel_4EA65698(mode);
		} else if (stopReason == SDBGP_STOP_SWBKPT) {
			int idx = swbkptFindByAddress(pid, stopPc);

			if (idx >= 0) {
				res = swbkptDisableSWBP(state->swbkpt[idx]);
				if (res < 0)
					goto err_failed;
				sdbgpUpdateStatus(cpu, 0x4100, mode);
			} else {
				res = swbkptIsBkpt(pid, stopPc, stopCpsr, &isBkpt);
				if (res < 0)
					goto err_failed;
				if (!isBkpt) {
					sdbgpUpdateStatus(cpu, 4, mode);
					sceSDfMgrContinueSystem();
					sdbgpUpdateStatus(cpu, 4, 0);
					goto alloc_reply;
				}
				sdfbzero(&tmpBkpt, sizeof(tmpBkpt));
				if (swbkptMakeEntry(pid, tid, stopPc, stopCpsr, mode, &tmpBkpt) < 0)
					return SCE_SDBGP_ERROR_FAILED;
				res = swbkptDisableSWBP(tmpBkpt);
				if (res < 0)
					goto err_failed;
				sdbgpUpdateStatus(cpu, 0x8100, mode);
			}
			sdbgpUpdateStatus(cpu, 4, 1);
			SceDeci4pSDfMgrForKernel_4EA65698(1);
			sdbgpUpdateStatus(cpu, 4, 0);
		} else if (stopReason == SDBGP_STOP_HWWATCH) {
			sdbgpUpdateStatus(cpu, 0x2100, mode);
			hwwtptDisableHWWP();
			cpu->hostMode |= SDBGP_HOST_HWWTPT_OFF;
			SceDeci4pSDfMgrForKernel_4EA65698(mode);
		} else {
			sdbgpUpdateStatus(cpu, 4, mode);
			sceSDfMgrContinueSystem();
			sdbgpUpdateStatus(cpu, 4, 0);
		}
		sdfbzero(&cpu->stoppedTid, sizeof(cpu->stoppedTid));
		sdfbzero(state->stoppedTid, sizeof(state->stoppedTid));
	} else if (mode == 2) {
		uint32_t stepCpsr = (targetThumb == 1) ? 0x20 : 0;

		sdfbzero(&state->stepSwbkpt, sizeof(state->stepSwbkpt));
		res = swbkptMakeEntry(pid, tid, targetAddr, stepCpsr, 0, &state->stepSwbkpt);
		if (res < 0)
			goto err_failed;

		switch (stopReason) {
		case SDBGP_STOP_HWBKPT:
			hwbkptDisableHWBP();
			sdbgpUpdateStatus(cpu, 0x1000, 1);
			break;
		case SDBGP_STOP_SWBKPT: {
			int idx = swbkptFindByAddress(pid, stopPc);

			if (idx >= 0) {
				res = swbkptDisableSWBP(state->swbkpt[idx]);
				if (res < 0)
					goto err_failed;
				sdbgpUpdateStatus(cpu, 0x4000, 1);
			} else {
				res = swbkptIsBkpt(pid, stopPc, stopCpsr, &isBkpt);
				if (res < 0)
					goto err_failed;
				if (isBkpt) {
					sdfbzero(&tmpBkpt, sizeof(tmpBkpt));
					res = swbkptMakeEntry(pid, tid, stopPc, stopCpsr, 1, &tmpBkpt);
					if (res < 0)
						goto err_failed;
					res = swbkptDisableSWBP(tmpBkpt);
					if (res < 0)
						goto err_failed;
					sdbgpUpdateStatus(cpu, 0x8000, 1);
				}
			}
			break;
		}
		case SDBGP_STOP_HWWATCH:
			hwwtptDisableHWWP();
			cpu->hostMode |= SDBGP_HOST_HWWTPT_OFF;
			sdbgpUpdateStatus(cpu, 0x2000, 1);
			break;
		default:
			break;
		}
		sdbgpUpdateStatus(cpu, 0x100, 1);
		sdbgpUpdateFlags(state, 0x10, 1);
		SceDeci4pSDfMgrForKernel_4EA65698(1);
		sdfbzero(&cpu->stoppedTid, sizeof(cpu->stoppedTid));
		sdfbzero(state->stoppedTid, sizeof(state->stoppedTid));
	} else {
		void *work = sdbgpAllocWorkBuf(cpu, SDBGP_PACKET_MAX);

		if (pid == -1 && tid == -1) {
			if (SceDeci4pSDfMgrForKernel_0A6437B6() != 0) {
				sdbgpUpdateStatus(cpu, 4, 1);
				sceSDfMgrContinueSystem();
				sdbgpUpdateStatus(cpu, 4, 0);
			}
		} else if (pid == -1) {
			if (tid != -1) {
				int susp = sceKernelIsThreadDebugSuspended(tid);

				if (susp < 0) {
					res = susp;
					goto err_failed;
				}
				res = sceKernelDebugResumeThread(tid, susp);
				if (res < 0)
					goto err_failed;
			}
		} else {
			res = sdbgpResumeProcessThreads(pid, work);
			if (res < 0)
				goto err_failed;
			res = sceKernelGetProcessStatus(pid, &procStatus);
			if (res < 0)
				goto err_failed;
			procStatus &= 0xffU;
			res = sceKernelProcessDebugResume(pid, (int)procStatus);
			if (res < 0)
				goto err_failed;
		}
	}

alloc_reply:
	reply = sdbgpAllocSendBuf(cpu, 0xa4);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, 0xa4);
	sdbgpSetSendSize(cpu, 0xa4);
	sdbgpSendErrorHeader(cpu, packet->group, 0x53, 0, 0);
	reply[7] = 0;
	return 0;

err_failed:
	sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
	return SCE_SDBGP_ERROR_FAILED;
}

int
sdbgpCmd10_62(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	void *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x13) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	cpu->hostMode = 0;
	reply = sdbgpAllocSendBuf(cpu, 0x1c);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, 0x1c);
	sdbgpSetSendSize(cpu, 0x1c);
	sdbgpSendErrorHeader(cpu, packet->group, 0x63, 0, 0);
	return 0;
}

int
sdbgpCmd10_64(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const uint32_t *req = (const uint32_t *)packet;
	void *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x17) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	sdbgpGetState()->enabled = req[5];
	reply = sdbgpAllocSendBuf(cpu, 0x1c);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, 0x1c);
	sdbgpSetSendSize(cpu, 0x1c);
	sdbgpSendErrorHeader(cpu, packet->group, 0x65, 0, 0);
	return 0;
}

int
sdbgpCmd10_72(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const uint32_t *req = (const uint32_t *)packet;
	SceUID uid;
	int res;
	void *work;
	uint32_t *reply;
	SceSize totalSize;
	uint32_t count;
	SceKernelPhyMemPartInfoForDebugger checkInfo;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x23) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	uid = (SceUID)req[5];
	if (uid != -1) {
		checkInfo.size = sizeof(checkInfo);
		checkInfo.freeBlockMax = 0;
		checkInfo.freeBlocks = NULL;
		res = sceKernelGetPhyMemPartInfoByID(uid, &checkInfo);
		if (res < 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
			return SCE_SDBGP_ERROR_FAILED;
		}
	}

	work = sdbgpAllocWorkBuf(cpu, 0x88000);
	reply = sdbgpAllocSendBuf(cpu, 0x87f3c);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, 0x87f3c);
	if (uid == -1) {
		SceUID *uids = (SceUID *)work;
		SceUInt32 num = 0;

		count = 0;
		sdfbzero(work, 0x88000);
		res = sceKernelGetPhyMemPartAll(5, uids, 0x88000, &num);
		if (res >= 0) {
			SceKernelPhyMemPartInfoForDebugger *info =
				(SceKernelPhyMemPartInfoForDebugger *)&uids[num];
			uint32_t *dst = &reply[8];
			uint32_t maxBlocks = (0x88000 - sizeof(*info) - 4 * num) / 8;
			SceUInt32 i;

			totalSize = 0;
			for (i = 0; i < num; i++) {
				sdfbzero(info, sizeof(*info));
				info->size = sizeof(*info);
				info->freeBlockMax = maxBlocks;
				info->freeBlocks = (struct SceKernelPhyMemPartFreeBlock *)(info + 1);
				res = sceKernelGetPhyMemPartInfoByID(uids[i], info);
				if (res < 0)
					break;
				count++;
				totalSize += 8 * (info->freeBlockCount + 125);
				sdbgpPackPhyMemPartInfo(dst, info);
				dst += 2 * info->freeBlockCount + 250;
			}
		} else {
			totalSize = 0;
		}
	} else {
		SceKernelPhyMemPartInfoForDebugger *info = (SceKernelPhyMemPartInfoForDebugger *)work;

		sdfbzero(info, sizeof(*info));
		info->size = sizeof(*info);
		info->freeBlockMax = (0x88000 - sizeof(*info)) / 8;
		info->freeBlocks = (struct SceKernelPhyMemPartFreeBlock *)(info + 1);
		res = sceKernelGetPhyMemPartInfoByID(uid, info);
		if (res >= 0) {
			totalSize = 8 * (info->freeBlockCount + 125);
			sdbgpPackPhyMemPartInfo(&reply[8], info);
			res = 0;
		} else {
			totalSize = 0;
		}
		count = 1;
	}

	if (res < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
		return SCE_SDBGP_ERROR_FAILED;
	}

	sdbgpSetSendSize(cpu, totalSize + 32);
	reply[7] = count;
	sdbgpSendErrorHeader(cpu, packet->group, 0x73, 0, 0);
	return 0;
}

int
sdbgpCmd10_90(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const uint32_t *req = (const uint32_t *)packet;
	SceSize size;
	int res;
	SceSize replySize;
	uint32_t *reply;
	uint8_t *work;
	uintptr_t paddr;
	SceSize width;
	SceSSize i, n;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x1b) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	size = req[6];
	res = isIllegalSize(size);
	if (res != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 7);
		return SCE_SDBGP_ERROR_FAILED;
	}

	replySize = size + 36;
	if (replySize > SDBGP_PACKET_MAX) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_TOO_LARGE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	reply = sdbgpAllocSendBuf(cpu, replySize);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, replySize);
	sdbgpSetSendSize(cpu, replySize);
	size = req[6];
	work = sdbgpAllocWorkBuf(cpu, size);
	sdfbzero(work, size);
	size = req[6];
	paddr = req[5];
	if ((size & 1) != 0)
		width = 1;
	else if ((size & 2) != 0)
		width = 2;
	else
		width = 4;

	res = sceKernelPhysicalMemReadForDebugger(work, paddr, size, width);
	if (res < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
		return SCE_SDBGP_ERROR_FAILED;
	}

	n = (SceSSize)req[6];
	for (i = 0; i < n; i++)
		((uint8_t *)reply)[36 + i] = work[i];

	sdbgpSendErrorHeader(cpu, packet->group, 0x91, 0, 0);
	{
		uintptr_t p = req[5];
		SceSize s = req[6];

		reply[7] = s;
		reply[8] = p;
	}
	return 0;
}

int
sdbgpCmd10_98(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const uint32_t *req = (const uint32_t *)packet;
	SceSize size;
	int res;
	SceSize replySize;
	uint32_t *reply;
	uint8_t *work;
	SceSSize i, n;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x1f) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	size = req[6];
	if (isIllegalSize(size) != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 7);
		return SCE_SDBGP_ERROR_FAILED;
	}
	res = isIllegalAccessSize(req[7]);
	if (res != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 0x20031);
		return SCE_SDBGP_ERROR_FAILED;
	}

	replySize = size + 40;
	if (replySize > SDBGP_PACKET_MAX) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_TOO_LARGE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	reply = sdbgpAllocSendBuf(cpu, replySize);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, replySize);
	sdbgpSetSendSize(cpu, replySize);
	size = req[6];
	work = sdbgpAllocWorkBuf(cpu, size);
	sdfbzero(work, size);
	res = sceKernelPhysicalMemReadForDebugger(work, req[5], req[6], 1U << req[7]);
	if (res < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
		return SCE_SDBGP_ERROR_FAILED;
	}

	n = (SceSSize)req[6];
	for (i = 0; i < n; i++)
		((uint8_t *)reply)[40 + i] = work[i];

	sdbgpSendErrorHeader(cpu, packet->group, 0x99, 0, 0);
	{
		uint32_t p = req[5];
		uint32_t w = req[7];
		uint32_t s = req[6];

		reply[7] = s;
		reply[8] = p;
		reply[9] = w;
	}
	return 0;
}

int
sdbgpCmd10_92(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const uint32_t *req = (const uint32_t *)packet;
	uint32_t *reply;
	uintptr_t paddr;
	SceSize size;
	SceSize width;
	int res;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x1b) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	reply = sdbgpAllocSendBuf(cpu, 0x20);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, 0x20);
	sdbgpSetSendSize(cpu, 0x20);
	paddr = req[5];
	size = req[6];
	if ((size & 1) != 0)
		width = 1;
	else if ((size & 2) != 0)
		width = 2;
	else
		width = 4;

	res = sceKernelPhysicalMemWriteForDebugger(paddr, &req[7], size, width);
	if (res < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
		return SCE_SDBGP_ERROR_FAILED;
	}

	sdbgpSendErrorHeader(cpu, packet->group, 0x93, 0, 0);
	reply[7] = req[6];
	return 0;
}

int
sdbgpCmd10_9a(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const uint32_t *req = (const uint32_t *)packet;
	int res;
	uint32_t *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x1f) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}

	res = isIllegalAccessSize(req[7]);
	if (res != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 0x20031);
		return SCE_SDBGP_ERROR_FAILED;
	}

	reply = sdbgpAllocSendBuf(cpu, 0x24);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, 0x24);
	sdbgpSetSendSize(cpu, 0x24);
	res = sceKernelPhysicalMemWriteForDebugger(req[5], &req[8], req[6], 1U << req[7]);
	if (res < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, res);
		return SCE_SDBGP_ERROR_FAILED;
	}

	sdbgpSendErrorHeader(cpu, packet->group, 0x9b, 0, 0);
	{
		uint32_t s = req[6];
		uint32_t w = req[7];

		reply[7] = s;
		reply[8] = w;
	}
	return 0;
}

int
sdbgpDecodeOpcode(int thumb, const void *buf, uint32_t *opcode)
{
	const uint8_t *bytes = (const uint8_t *)buf;
	int i;

	if (bytes == NULL)
		return SCE_SDBGP_ERROR_FAILED;

	if (thumb != 0) {
		if (thumb == 1)
			return sdbgpDecodeThumbHalfword(bytes, opcode);
		return SCE_SDBGP_ERROR_FAILED;
	}

	for (i = 3; i >= 0; i--)
		*opcode += (uint32_t)bytes[i] << (8 * i);
	return 0;
}

int
sdbgpCheckStepOverCall(SdbgpState *state, SceUID pid, uintptr_t pc, uint32_t cpsr, int *isCall)
{
	uint32_t opcode = 0;
	uint8_t buf16[4];
	uint8_t buf32[4];
	int res;
	uint32_t masked;

	*isCall = 0;
	if ((cpsr & 0x20) != 0) {
		res = sdbgpReadMemory(pid, (const void *)pc, 2, buf16);
		if (res < 0)
			return res;
		sdbgpDecodeThumbHalfword(buf16, &opcode);
		if (opcode < 0xe800U) {
			masked = opcode & 0xff87U;
			if (masked != 0x4780U)
				return 0;
		} else {
			uint32_t hi;

			res = sdbgpReadMemory(pid, (const void *)(pc + 2), 2, buf16);
			if (res < 0)
				return res;
			hi = opcode << 16;
			opcode = 0;
			sdbgpDecodeThumbHalfword(buf16, &opcode);
			masked = (hi + opcode) & 0xf800d000U;
			if (masked != 0xf000d000U) {
				masked = (hi + opcode) & 0xf800d001U;
				if (masked != 0xf000c000U)
					return 0;
			}
		}
	} else {
		res = sdbgpReadMemory(pid, (const void *)pc, 4, buf32);
		if (res < 0)
			return res;
		sdbgpDecodeOpcode(0, buf32, &opcode);
		masked = opcode & 0x0f000000U;
		if (masked != 0x0b000000U) {
			masked = opcode & 0xfe000000U;
			if (masked != 0xfa000000U) {
				if ((opcode & 0x0ffffff0U) != 0x012fff30U)
					return 0;
				masked = 0x012fff30U;
			}
		}
	}

	opcode = masked;
	*isCall = 1;
	if (state->debugCpu != NULL) {
		uintptr_t stopPc = state->debugCpu[sceKernelCpuId()].pc;
		SdfMgrDebugCpuState *dbgCpu = state->debugCpu;
		uintptr_t nextPc = (masked == 0x4780U) ? (stopPc + 2) : (stopPc + 4);

		dbgCpu[sceKernelCpuId()].stepAddress = nextPc;
	}
	return 0;
}

int
sdbgpCmd10_54(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const uint32_t *req = (const uint32_t *)packet;
	SdbgpState *state = sdbgpGetState();
	uint32_t stopReason = 0;
	int isCall = 0;
	SdbgpSwbkpt tmpBkpt;
	uint32_t count;
	int core;
	uint32_t stepMode;
	uint32_t reqMode;
	void *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x97) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_INVALID_ARGUMENT;
	}
	if (SceDeci4pSDfMgrForKernel_0A6437B6() == 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 0x200b0);
		return SCE_SDBGP_ERROR_FAILED;
	}
	if (state->debugCpu == NULL) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_INVALID_SIZE);
		return SCE_SDBGP_ERROR_FAILED;
	}

	count = req[7];
	if ((int)count < 1)
		count = 1;
	state->stepCount = count;
	core = sceKernelCpuId();
	if (SceExcpmgrForKernel_B615A7DA() <= 0) {
		int intrNest;
		uintptr_t intrBottom;
		uintptr_t intrPc;

		stopReason = SDBGP_STOP_REQUEST;
		intrNest = sceKernelIsIntrContext();
		if (intrNest < 0)
			return SCE_SDBGP_ERROR_FAILED;
		intrBottom = (uintptr_t)SceIntrmgrForKernel_01911612(sceKernelCpuId(), intrNest);
		if (intrNest == 1) {
			const uint32_t *intrFrame = (const uint32_t *)(intrBottom - 0x140);
			const uint32_t *tcb = (const uint32_t *)(uintptr_t)intrFrame[0];
			const uint32_t *armRegs = (const uint32_t *)(uintptr_t)tcb[3];

			intrPc = armRegs[48];
		} else {
			intrPc = *(const uint32_t *)(intrBottom - 8);
		}
		state->debugCpu[core].pc = intrPc;
	} else {
		getCpuStopReason(cpu, &stopReason);
		if (stopReason == SDBGP_STOP_HWBKPT) {
			sdbgpUpdateStatus(cpu, 0x1000, 1);
			hwbkptDisableHWBP();
		} else if (stopReason <= SDBGP_STOP_HWBKPT) {
			if (stopReason != SDBGP_STOP_HWWATCH) {
				sdbgpSendError(cpu, packet->group, packet->command + 1,
					       packet->tag, 0x200b1);
				return SCE_SDBGP_ERROR_FAILED;
			}
			sdbgpUpdateStatus(cpu, 0x2000, 1);
			hwwtptDisableHWWP();
		} else if (stopReason == SDBGP_STOP_SWBKPT) {
			const SdfMgrDebugCpuState *dbgCpu = &state->debugCpu[core];
			uintptr_t stopPc = dbgCpu->pc;
			SceUID stopPid = dbgCpu->pid;
			uint32_t stopCpsr = dbgCpu->cpsr;
			int idx = swbkptFindByAddress(stopPid, stopPc);

			if (idx >= 0) {
				if (swbkptDisableSWBP(state->swbkpt[idx]) < 0)
					return SCE_SDBGP_ERROR_FAILED;
				sdbgpUpdateStatus(cpu, 0x4000, 1);
			} else {
				sdfbzero(&tmpBkpt, sizeof(tmpBkpt));
				if (swbkptMakeEntry(stopPid, -1, stopPc, stopCpsr, 1, &tmpBkpt) < 0 ||
				    swbkptDisableSWBP(tmpBkpt) < 0)
					return SCE_SDBGP_ERROR_FAILED;
				sdbgpUpdateStatus(cpu, 0x8000, 1);
			}
		} else if (stopReason == SDBGP_STOP_REQUEST) {
			int intrNest = sceKernelIsIntrContext();
			uintptr_t intrBottom;
			uintptr_t intrPc;

			if (intrNest < 0)
				return SCE_SDBGP_ERROR_FAILED;
			intrBottom = (uintptr_t)SceIntrmgrForKernel_01911612(sceKernelCpuId(), intrNest);
			if (intrNest == 1) {
				const uint32_t *intrFrame = (const uint32_t *)(intrBottom - 0x140);
				const uint32_t *tcb = (const uint32_t *)(uintptr_t)intrFrame[0];
				const uint32_t *armRegs = (const uint32_t *)(uintptr_t)tcb[3];

				intrPc = armRegs[48];
			} else {
				intrPc = *(const uint32_t *)(intrBottom - 8);
			}
			state->debugCpu[core].pc = intrPc;
		} else {
			sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, 0x200b1);
			return SCE_SDBGP_ERROR_FAILED;
		}
	}

	stepMode = req[5];
	isCall = 0;
	if (stepMode != 1) {
		const SdfMgrDebugCpuState *dbgCpu = &state->debugCpu[sceKernelCpuId()];

		if ((stopReason != SDBGP_STOP_HWWATCH &&
		     stopReason != SDBGP_STOP_HWBKPT &&
		     stopReason != SDBGP_STOP_SWBKPT &&
		     stopReason != SDBGP_STOP_REQUEST) ||
		    (sdbgpCheckStepOverCall(state, dbgCpu->pid, dbgCpu->pc,
					    dbgCpu->cpsr, &isCall) < 0 || !isCall)) {
			stepMode = 1;
		}
	}

	reqMode = req[5];
	if (reqMode == 1) {
		sdbgpUpdateStatus(cpu, 0x400, 1);
		reqMode = req[5];
	}
	if (reqMode == 2)
		sdbgpUpdateStatus(cpu, 0xc00, 1);

	if (stepMode == 1) {
		if (stopReason == SDBGP_STOP_REQUEST)
			SceDeci4pSDfMgrForKernel_4EA65698(0x10);
		else
			SceDeci4pSDfMgrForKernel_4EA65698(stepMode);
	} else if (stepMode == 2) {
		if (stopReason == SDBGP_STOP_REQUEST)
			SceDeci4pSDfMgrForKernel_4EA65698(0x20);
		else
			SceDeci4pSDfMgrForKernel_4EA65698(stepMode);
	}

	state->enabled = 0x8000;
	reply = sdbgpAllocSendBuf(cpu, 0x9c);
	if (reply == NULL)
		return SCE_SDBGP_ERROR_NO_MEMORY;

	sdfbzero(reply, 0x9c);
	sdbgpSetSendSize(cpu, 0x9c);
	sdbgpSendErrorHeader(cpu, packet->group, 0x55, 0, 0);
	return 0;
}
