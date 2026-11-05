/*
 * SceDeci4pSDbgp (os0:kd/deci4p_sdbgp.skprx): work queue, thread list, thread
 * stop cause, process thread resume and waypoint trace helpers (thread.c).
 */

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <excpmgr/excpmgr.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/threadmgr_debugger.h>
#include "sdbgp_internal.h"

int sdbgpGetKernelWorkQueueInfo(SdbgpCpuState *cpu, void *reply,
				ThreadmgrWorkQueueInfoFull *work, SceSize size, int kernelOnly)
{
	ThreadmgrWorkQueueInfoFull *cur = work;
	SceSize count;
	uint32_t *out;
	int res;
	uint32_t i;
	uint32_t written;

	(void)cpu;
	SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, size);
	res = SceThreadmgrForDriver_920EA1CA(5, cur, size, &count);
	if (res < 0)
		return res;

	out = (uint32_t *)((uintptr_t)reply + 36);
	written = 0;
	for (i = 0; (int)i < (int)count; i++) {
		uint32_t threadCount;

		if (kernelOnly) {
			if (cur->creatorPid != SDBGP_KERNEL_PROCESS_ID)
				continue;
			threadCount = cur->threadCount;
			out[0] = 4 * (threadCount + 16);
			out[1] = cur->workQueueId;
			out[2] = SDBGP_KERNEL_PROCESS_ID;
			out[11] = -1;
		} else {
			threadCount = cur->threadCount;
			out[0] = 4 * (threadCount + 16);
			out[1] = cur->workQueueId;
			out[2] = cur->creatorPid;
			if (cur->creatorPid == SDBGP_KERNEL_PROCESS_ID)
				out[11] = -1;
			else
				out[11] = cur->reservedId;
		}
		out[12] = cur->attr;
		out[13] = threadCount;
		out[14] = (uint32_t)cur->scheduled;
		out[15] = cur->pendingCount;
		written++;
		SceDeci4pSDfMgrForKernel_5996E2DB(&out[3], cur->name, 32);
		SceDeci4pSDfMgrForKernel_5996E2DB(&out[16], cur->threads, 4 * cur->threadCount);
		out = &out[16 + cur->threadCount];
		cur = (ThreadmgrWorkQueueInfoFull *)((uintptr_t)cur + cur->size);
	}
	*(uint32_t *)((uintptr_t)reply + 32) = written;
	return 0;
}

int sdbgpGetProcessWorkQueueInfo(SdbgpCpuState *cpu, void *reply,
				 ThreadmgrWorkQueueInfoFull *work, SceSize size)
{
	uint32_t *hdr = (uint32_t *)reply;
	SceUID pid = hdr[7];
	ThreadmgrWorkQueueInfoFull *cur = work;
	SceSize count;
	uint32_t *out;
	int res;
	int i;

	(void)cpu;
	SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, size);
	res = SceThreadmgrForDriver_8C8A76EF(pid, 5, cur, size, &count);
	if (res < 0)
		return res;

	out = &hdr[9];
	hdr[8] = count;
	for (i = 0; i < (int)count; i++) {
		uint32_t threadCount = cur->threadCount;
		SceUID wqId = cur->workQueueId;
		SceUID creatorPid = cur->creatorPid;
		uint32_t attr = cur->attr;

		if (creatorPid == SDBGP_KERNEL_PROCESS_ID)
			out[11] = -1;
		out[0] = 4 * (threadCount + 16);
		out[1] = wqId;
		out[2] = creatorPid;
		if (creatorPid != SDBGP_KERNEL_PROCESS_ID)
			out[11] = cur->reservedId;
		out[14] = (uint32_t)cur->scheduled;
		out[12] = attr;
		out[13] = threadCount;
		out[15] = cur->pendingCount;
		SceDeci4pSDfMgrForKernel_5996E2DB(&out[3], cur->name, 32);
		SceDeci4pSDfMgrForKernel_5996E2DB(&out[16], cur->threads, 4 * cur->threadCount);
		out = &out[16 + cur->threadCount];
		cur = (ThreadmgrWorkQueueInfoFull *)((uintptr_t)cur + cur->size);
	}
	return 0;
}

__attribute__((noinline)) int
sdbgpGetThreadStopInfo(SdbgpCpuState *cpu, uint32_t *out, SceUID tid)
{
	SdbgpState *state;
	uint32_t regs[36];
	int susp;
	uint32_t masked;
	int i;

	(void)cpu;
	state = sdbgpGetState();
	susp = sceKernelIsThreadDebugSuspended(tid);
	if (susp < 0)
		return susp;

	out[1] = 0;
	out[2] = 0;
	out[3] = 0;
	out[4] = 0;
	out[0] = 20;
	for (i = 0; i < 4; i++) {
		if (state->stoppedTid[i] == tid) {
			out[1] = state->stopReason[i];
			return 0;
		}
	}

	if (susp & 1) {
		out[1] = 0x10002;
		return 0;
	}
	if (susp == 2) {
		out[1] = 0x10001;
		return 0;
	}

	masked = (uint32_t)susp & ~3U;
	switch (masked) {
	case 0:
		return 0;
	case 0x10:
		out[1] = 0x40001;
		return 0;
	case 0x100:
	case 0x800:
		out[1] = 0x30003;
		return 0;
	case 0x200:
		out[1] = 0x30004;
		return 0;
	case 0x400:
		out[1] = 0x30002;
		return 0;
	case 0x1000:
		out[1] = 0x20004;
		SceThreadmgrForKernel_C4735A7E(tid, (ThreadmgrDebugRegisters *)regs);
		for (i = 0; i < SDBGP_SWBKPT_MAX; i++) {
			if (state->swbkpt[i].address == regs[15]) {
				SceUID bkptTid = state->swbkpt[i].threadId;

				if (bkptTid == tid || bkptTid == -1)
					out[2] = state->swbkpt[i].id;
			}
		}
		return 0;
	case 0x2000:
		out[1] = 0x20001;
		return 0;
	case 0x4000:
		out[1] = 0x20007;
		return 0;
	case 0x10000:
		out[1] = 0x10006;
		return 0;
	case 0x20000:
		out[1] = 0x10007;
		return 0;
	case 0x40000:
		out[1] = 0x10008;
		return 0;
	case 0x80000:
		out[1] = 0x10009;
		return 0;
	default:
		return (int)0x80020005;
	}
}

int sdbgpGetThreadInfoList(SdbgpCpuState *cpu, void *out, void *work, SceUID pid,
			   uint32_t detail, SceSize *size, SceSize *count)
{
	const uint32_t *infos = (const uint32_t *)work;
	int isAll = (pid == 0);
	int isKernel = (pid == SDBGP_KERNEL_PROCESS_ID);
	SceSize rawCount = 0;
	uint32_t written;
	uint32_t i;
	int res;

	SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_WORK_BUF_SIZE);
	*size = 0;
	*count = 0;
	if (isAll || isKernel)
		res = SceThreadmgrForKernel_B011EA69(detail != 0, work, SDBGP_WORK_BUF_SIZE, &rawCount);
	else
		res = SceThreadmgrForKernel_6351A13A(pid, detail != 0, work, SDBGP_WORK_BUF_SIZE, &rawCount);
	if (res < 0)
		return res;

	if (268 * rawCount > SDBGP_SEND_BUF_SIZE)
		rawCount = 2077;

	written = 0;
	for (i = 0; i < rawCount; i++) {
		const uint32_t *in = &infos[74 * i];
		uint32_t *dst = (uint32_t *)((uintptr_t)out + 268 * written);
		SceUID threadPid = (SceUID)in[2];
		const uint32_t *regs;

		if (isKernel) {
			if (threadPid != SDBGP_KERNEL_PROCESS_ID)
				continue;
			dst[0] = 268;
			dst[1] = in[1];
			dst[47] = -1;
		} else {
			dst[0] = 268;
			if (threadPid == SDBGP_KERNEL_PROCESS_ID) {
				dst[1] = in[1];
				dst[47] = -1;
			} else if (isAll) {
				dst[1] = in[1];
				dst[47] = SceThreadmgrForDriver_F311808F(in[1]);
			} else {
				dst[1] = scePUIDtoGUID(threadPid, in[1]);
				dst[47] = in[1];
			}
		}

		threadPid = (SceUID)in[2];
		dst[2] = threadPid;
		dst[11] = in[11];
		dst[12] = in[12];
		dst[13] = in[13];
		dst[14] = in[14];
		dst[15] = in[15];
		dst[16] = in[16];
		dst[17] = in[17];
		dst[18] = in[28];
		dst[19] = in[29];
		dst[20] = in[30];
		dst[21] = in[31];
		dst[22] = in[32];
		dst[23] = in[33] + (in[34] << 16);
		dst[24] = in[36] + (in[35] << 16);
		dst[25] = in[37];
		if (threadPid == SDBGP_KERNEL_PROCESS_ID || isAll) {
			dst[26] = in[38];
		} else {
			SceUID guid = scePUIDtoGUID(threadPid, in[38]);

			dst[26] = (guid < 0) ? in[38] : (uint32_t)guid;
		}
		dst[27] = in[39];
		dst[28] = in[40];
		dst[34] = in[41];
		dst[35] = in[43];
		dst[36] = in[44];
		dst[37] = in[45];
		dst[38] = in[46];
		dst[41] = in[61];
		dst[44] = in[19];
		dst[45] = in[20];
		dst[46] = in[21];
		dst[56] = in[52];
		dst[65] = in[63];
		dst[66] = in[65];
		SceDeci4pSDfMgrForKernel_5996E2DB(&dst[57], &in[53], 32);
		dst[39] = (in[12] != 0);
		sdbgpGetThreadStopInfo(cpu, &dst[29], (SceUID)dst[1]);
		regs = (const uint32_t *)(uintptr_t)in[48];
		if (regs) {
			if (regs[0] == 1)
				dst[40] = regs[48];
			else
				dst[40] = regs[15];
		} else {
			dst[40] = 0;
		}
		if (in[13] == 8) {
			uint32_t wt = in[37];

			SceDeci4pSDfMgrForKernel_5996E2DB(&dst[48], &in[66], 32);
			switch (wt) {
			case 1:
			case 4:
			case 16:
			case 0x80000001U:
			case 0x80000004U:
			case 0x80000010U:
				dst[42] = in[66];
				dst[43] = in[67];
				break;
			case 32:
			case 64:
			case 128:
			case 0x10000:
			case 0x100000:
			case 0x80000020U:
			case 0x80000040U:
			case 0x80000080U:
			case 0x80010000U:
			case 0x80100000U:
				dst[42] = in[66];
				break;
			default:
				break;
			}
		}
		written++;
		SceDeci4pSDfMgrForKernel_5996E2DB(&dst[3], &in[3], 32);
		*size += dst[0];
	}
	*count = written;
	return 0;
}

int sdbgpResumeProcessThreads(SceUID pid, void *work)
{
	SceUID *ids = (SceUID *)work;
	SceSize count;
	int num;
	int i;

	num = sceKernelGetThreadIdList(pid, NULL, 0, NULL);
	if (num < 0)
		return (int)0x80028000;
	SceDeci4pSDfMgrForKernel_8A3B06F2(ids, 0, 4 * num);
	if (sceKernelGetThreadIdList(pid, ids, 4 * num, &count) < 0)
		return (int)0x80028000;

	for (i = 0; i < num; i++) {
		SceUID tid = ids[i];
		int susp = sceKernelIsThreadDebugSuspended(tid);

		if (susp < 0)
			return (int)0x80028000;
		if ((susp & 1) == 0 &&
		    SceThreadmgrForDriver_04C6764B(tid, susp & 3) < 0)
			return (int)0x80028000;
	}
	return 0;
}

int sdbgpGetThreadWaypointTrace(SceUID *pid, void *work, void *out, SceSize *count,
				SceSize *size)
{
	SdbgpState *state;
	const uint32_t *infos = (const uint32_t *)work;
	uint32_t *dst = (uint32_t *)out;
	SceExcpmgrContext *ctx = NULL;
	SceSize rawCount = 0;
	uint32_t i;
	int res;

	state = sdbgpGetState();
	*count = 0;
	*size = 0;
	res = SceThreadmgrForKernel_B011EA69(0, work, SDBGP_WORK_BUF_SIZE, &rawCount);
	if (res < 0)
		return res;

	for (i = 0; i < rawCount; i++) {
		const uint32_t *in = &infos[74 * i];
		SceUID tid;
		int cpu;
		uint32_t idx;
		int j;

		if (*pid != 0 && *pid != (SceUID)in[2])
			continue;
		tid = (SceUID)in[1];
		for (cpu = 0; cpu < 4; cpu++) {
			if (state->stoppedTid[cpu] == tid)
				break;
		}
		if (cpu == 4)
			continue;

		ctx = state->debugCpu[cpu].context;
		dst[0] = 528;
		dst[1] = in[2];
		dst[2] = tid;
		dst[3] = 32;
		idx = ((ctx->waypointControl >> 8) & 0x1f) + 1;
		for (j = 0; j < 32; j++) {
			SceDeci4pSDfMgrForKernel_5996E2DB(
				&dst[4 + 4 * j],
				&ctx->waypoint[(idx + j) & 0x1f],
				16);
		}
		*size = dst[0];
		(*count)++;
		*pid = (SceUID)dst[1];
	}

	if (!ctx) {
		for (i = 0; i < rawCount; i++) {
			const uint32_t *in = &infos[74 * i];
			SceExcpmgrContext *excCtx;
			uint32_t idx;
			int j;

			if ((in[42] & 0x3f00) == 0)
				continue;
			excCtx = (SceExcpmgrContext *)((uintptr_t)sceKernelGetExcpStackBottom(in[36]) - 0x400);
			dst[0] = 528;
			dst[1] = in[2];
			dst[2] = in[1];
			dst[3] = 32;
			idx = ((excCtx->waypointControl >> 8) & 0x1f) + 1;
			for (j = 0; j < 32; j++) {
				SceDeci4pSDfMgrForKernel_5996E2DB(
					&dst[4 + 4 * j],
					&excCtx->waypoint[(idx + j) & 0x1f],
					16);
			}
			*size = dst[0];
			(*count)++;
			*pid = (SceUID)dst[1];
		}
	}
	return 0;
}
