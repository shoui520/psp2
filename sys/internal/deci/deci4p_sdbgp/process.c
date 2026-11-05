/*
 * SceDeci4pSDbgp (os0:kd/deci4p_sdbgp.skprx): process stop cause and state
 * flags (process.c).
 */

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <processmgr/processmgr.h>
#include <threadmgr/threadmgr.h>
#include "sdbgp_internal.h"

int sdbgpGetProcessStopInfo(SdbgpCpuState *cpu, uint32_t *out, SceUID pid)
{
	SdbgpState *state;
	uint32_t status = 0;
	uint32_t regs[36];
	ThreadmgrThreadInfoForDebugger info;
	int res;
	int i;

	state = sdbgpGetState();
	res = sceKernelGetProcessStatus(pid, &status);
	if (res < 0)
		return res;

	out[1] = 0;
	out[2] = 0;
	out[3] = 0;
	out[4] = 0;
	out[0] = 20;
	if (!state)
		return 0;

	for (i = 0; i < 4; i++) {
		SceUID tid = state->stoppedTid[i];

		if (tid != 0) {
			info.size = sizeof(info);
			SceThreadmgrForKernel_C976719E(tid, 0, &info);
			if (info.processId == pid)
				break;
		}
	}
	if (i == 4) {
		status = (uint8_t)status;
		switch (status) {
		case 1:
			out[1] = 0x1001;
			return 0;
		case 2:
		case 4:
			out[1] = 0;
			return 0;
		case 3:
			out[1] = 0x1002;
			return 0;
		case 17:
			out[1] = 0x10001;
			return 0;
		case 18:
			out[1] = 0x20004;
			goto find_swbkpt;
		case 19:
			out[1] = 0x30003;
			return 0;
		case 20:
			out[1] = 0x30004;
			return 0;
		case 21:
			out[1] = 0x30002;
			return 0;
		case 22:
			out[1] = 0x20001;
			return 0;
		case 23:
			out[1] = 0x40001;
			return 0;
		case 24:
			out[1] = 0x10006;
			return 0;
		case 25:
			out[1] = 0x10007;
			return 0;
		case 26:
			out[1] = 0x10008;
			return 0;
		case 27:
			out[1] = 0x70000;
			return 0;
		case 28:
			out[1] = 0x20007;
			return 0;
		case 29:
			out[1] = 0x10009;
			return 0;
		default:
			out[1] = 0;
			if (status == 18)
				goto find_swbkpt;
			return 0;
		}
	}

	out[1] = state->stopReason[i];
	if (out[1] == 0x20004)
		return 0;

find_swbkpt:
	SceThreadmgrForKernel_C4735A7E(cpu->stoppedTid, (ThreadmgrDebugRegisters *)regs);
	for (i = 0; i < SDBGP_SWBKPT_MAX; i++) {
		if (state->swbkpt[i].address == regs[15]) {
			SceUID tid = state->swbkpt[i].threadId;

			if (tid == cpu->stoppedTid || tid == -1) {
				out[2] = state->swbkpt[i].id;
				return 0;
			}
		}
	}
	return 0;
}

int sdbgpGetProcessStateFlags(SdbgpCpuState *cpu, SceUID pid, uint32_t *flags)
{
	SceKernelProcessInfo info;
	uint32_t val;
	uint32_t st;

	info.size = sizeof(info);
	if (sceKernelGetProcessInfo(pid, &info) < 0)
		return (int)0x80029000;

	st = (uint8_t)info.status;
	info.status = st;
	if ((SceUID)cpu->flags == pid) {
		val = *flags | 0x20000;
		*flags = val;
	} else {
		val = *flags;
	}
	if (st == 2)
		val |= 0x10002;
	else
		val |= 0x10000;
	*flags = val | 1;
	return 0;
}
