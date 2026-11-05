/*
 * SceDeci4pSDbgp (os0:kd/deci4p_sdbgp.skprx): process, memblock, heap, budget
 * and process local storage commands (group 0x20, group_process.c).
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <modulemgr/modulemgr.h>
#include <processmgr/processmgr.h>
#include <sysmem/address_space.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/heap_info.h>
#include <sysmem/sysmem.h>
#include "sdbgp_internal.h"

SceSize strnlen(const char *s, SceSize maxlen);

#define SDBGP_ERROR_OUT_OF_MEMORY	((int)0x8002710b)
#define SCE_KERNEL_ERROR_UNKNOWN_MEMBLOCK ((int)0x80024b00)
#define SCE_KERNEL_ERROR_NO_MOD		((int)0x8002d082)

static int s_seenPartCount;
static TinyPartition *s_seenPartIds[64];

typedef struct SdbgpHeapEntry {
	SceSize size;				/* 0x000: 260 + 8 * blockCount */
	uint32_t reserved;			/* 0x004 */
	SceUID uid;				/* 0x008 */
	char name[32];				/* 0x00c */
	SceSize totalSize;			/* 0x02c */
	SceSize maxSize;			/* 0x030 */
	SceSize footprint;			/* 0x034 */
	SceSize arena;				/* 0x038 */
	SceSize ordblks;			/* 0x03c */
	SceSize hblkhd;				/* 0x040 */
	SceSize usmblks;			/* 0x044 */
	SceSize uordblks;			/* 0x048 */
	SceSize fordblks;			/* 0x04c */
	SceSize keepcost;			/* 0x050 */
	SceSize stateSize;			/* 0x054 */
	SceSize maxSystemBytes;			/* 0x058 */
	SceSize systemBytes;			/* 0x05c */
	SceSize inUseBytes;			/* 0x060 */
	int blockCount;				/* 0x064 */
	struct malloc_segment_range fixedBlocks[16]; /* 0x068 */
	SceSize peakTotalSize;			/* 0x0e8 */
	uint32_t chunks;			/* 0x0ec */
	uint32_t peakChunks;			/* 0x0f0 */
	SceSize used;				/* 0x0f4 */
	SceSize peakUsed;			/* 0x0f8 */
	SceSize maxChunk;			/* 0x0fc */
	uint32_t flags;				/* 0x100 */
	struct malloc_segment_range blocks[];	/* 0x104 */
} SdbgpHeapEntry;

_Static_assert(sizeof(SdbgpHeapEntry) == 260, "SdbgpHeapEntry");

typedef struct SdbgpBudgetEntry {
	SceSize size;				/* 0x00: 4 * (count + 11) */
	SceUID budgetId;			/* 0x04 */
	char name[32];				/* 0x08 */
	uint32_t count;				/* 0x28 */
	SceUID partUid[];			/* 0x2c */
} SdbgpBudgetEntry;

_Static_assert(sizeof(SdbgpBudgetEntry) == 44, "SdbgpBudgetEntry");

int sdbgpPackHeapInfo(SdbgpHeapEntry *out, HeapInfo *info, SceSize *size)
{
	SceSize entrySize;
	int count;
	int i;

	*size = 0;
	entrySize = 8 * info->blockCount + sizeof(SdbgpHeapEntry);
	SceDeci4pSDfMgrForKernel_8A3B06F2(out, 0, entrySize);
	out->size = entrySize;
	out->reserved = info->reserved;
	out->uid = info->uid;
	out->totalSize = info->totalSize;
	out->peakTotalSize = info->peakTotalSize;
	out->maxSize = info->maxSize;
	out->footprint = info->footprint;
	out->arena = info->arena;
	out->ordblks = info->ordblks;
	out->hblkhd = info->hblkhd;
	out->usmblks = info->usmblks;
	out->uordblks = info->uordblks;
	out->fordblks = info->fordblks;
	out->keepcost = info->keepcost;
	out->stateSize = info->stateSize;
	out->maxSystemBytes = info->maxSystemBytes;
	out->systemBytes = info->systemBytes;
	out->inUseBytes = info->inUseBytes;
	out->chunks = info->chunks;
	out->peakChunks = info->peakChunks;
	out->used = info->used;
	out->peakUsed = info->peakUsed;
	out->maxChunk = info->maxChunk;
	count = info->blockCount;
	out->blockCount = count;
	out->flags = info->flags;
	if (info->name != NULL) {
		SceDeci4pSDfMgrForKernel_5996E2DB(out->name, info->name,
						  strnlen(info->name, 32));
		count = out->blockCount;
		entrySize = out->size;
	}
	if (count > 0) {
		const struct malloc_segment_range *blocks = info->blockList;

		for (i = 0; i != count && i != 15; i++) {
			if (blocks != NULL) {
				out->fixedBlocks[i].base = blocks[i].base;
				out->fixedBlocks[i].size = blocks[i].size;
			}
		}
		for (i = 0; i != count; i++) {
			if (blocks != NULL)
				out->blocks[i] = blocks[i];
		}
	}
	*size = entrySize;
	return count;
}

int sdbgpPackBudgetInfo(uint32_t *out,
			SceKernelBudgetInfoForDebugger *info,
			SceSize *size)
{
	uint32_t count;
	uint32_t i;

	count = info->count;
	out[10] = count;
	out[0] = (count + 11) * 4;
	out[1] = info->budgetId;
	SceDeci4pSDfMgrForKernel_5996E2DB(&out[2], info->name,
					  strnlen(info->name, 32));
	*size = out[0];
	for (i = 0; i < info->count; i++)
		out[i + 11] = info->part[i].info.uid;
	return 0;
}

int sdbgpPackProcessInfo(SdbgpCpuState *cpu, uint32_t *out,
			 SceKernelProcessInfo *info, SceUID pid,
			 SceSize *size)
{
	SceKernelModuleInfo modInfo;
	SceSize entrySize;
	SceSize pathLen;
	SceSize alignedLen;
	int noModule;
	int res;
	uint8_t *tail;

	SceDeci4pSDfMgrForKernel_8A3B06F2(info, 0, sizeof(*info));
	info->size = sizeof(*info);
	res = sceKernelGetProcessInfo(pid, info);
	if (res < 0)
		return res;
	if (info->pid == SDBGP_KERNEL_PROCESS_ID) {
		noModule = 1;
		entrySize = 0xf8;
		pathLen = 1;
		alignedLen = 4;
	} else if (info->status == 4) {
		entrySize = 0xf8;
		noModule = 0;
		pathLen = 1;
		alignedLen = 4;
	} else {
		SceUID modid = sceKernelGetModuleIdByPid(pid);

		res = SceModulemgrForKernel_79E761B5(modid, (char *)&modInfo,
						     sizeof(modInfo));
		if (res == SCE_KERNEL_ERROR_NO_MOD) {
			entrySize = 0xf8;
			alignedLen = 4;
			noModule = 0;
			pathLen = 1;
		} else if (res < 0) {
			return res;
		} else {
			SceSize len;

			SceModulemgrForKernel_337A3908(modid, &out[15]);
			len = strnlen((const char *)&modInfo, 256);
			pathLen = len + 1;
			noModule = pathLen & 3;
			if (noModule != 0) {
				alignedLen = pathLen + 4 - noModule;
				noModule = 0;
			} else {
				alignedLen = pathLen;
			}
			entrySize = alignedLen + 0xf4;
		}
	}
	out[1] = info->pid;
	out[0] = entrySize;
	res = sdbgpGetProcessStateFlags(cpu, out[1], &out[3]);
	if (res < 0)
		return res;
	out[2] = info->budgetId;
	out[12] = info->param;
	out[13] = info->cpuAffinity;
	out[14] = noModule ? 0 : (uintptr_t)info->entryPoint;
	out[22] = pathLen;
	out[16] = info->parentPid;
	SceDeci4pSDfMgrForKernel_5996E2DB(&out[4], info->name, 32);
	res = sdbgpGetProcessStopInfo(cpu, &out[17], out[1]);
	if (res < 0)
		return res;
	tail = (uint8_t *)&out[23] + alignedLen;
	if (!noModule && info->status != 4) {
		SceDeci4pSDfMgrForKernel_5996E2DB(&out[23], &modInfo,
						  out[22] - 1);
		if (info->status != 4) {
			if (sceKernelGetModuleInfo(pid, info->pid, &modInfo) < 0)
				memset(&modInfo, 0, sizeof(modInfo));
			SceDeci4pSDfMgrForKernel_5996E2DB(tail,
							  &modInfo.exidx_btm, 4);
			SceDeci4pSDfMgrForKernel_5996E2DB(tail + 4,
							  &modInfo.exidx_btm, 4);
			SceDeci4pSDfMgrForKernel_5996E2DB(tail + 8,
							  &modInfo.extab_top, 4);
			SceDeci4pSDfMgrForKernel_5996E2DB(tail + 12,
							  &modInfo.extab_btm, 4);
		}
	}
	*(uint32_t *)(tail + 16) = 32;
	SceDeci4pSDfMgrForKernel_5996E2DB(tail + 20, info->localStorage, 128);
	*(uint32_t *)(tail + 148) = info->libraryDBFlags;
	*size = out[0];
	return 0;
}

/* 0x00: list all processes */
int sdbgpCmd20_00(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	SceUID pids[16];
	int count = 0;
	SceSize entrySize = 0;
	SceKernelProcessInfo *work;
	uint32_t *reply;
	uint8_t *dst;
	SceSize totalSize;
	int i = 0;
	int res;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x13) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
	memset(pids, 0, sizeof(pids));
	res = SceProcessmgrForKernel_EFFF2975(5, pids, sizeof(pids),
					      (SceSize *)&count);
	if (res < 0)
		goto fail;
	reply = sdbgpAllocSendBuf(cpu, SDBGP_SEND_BUF_SIZE);
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	dst = (uint8_t *)(reply + 8);
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, SDBGP_SEND_BUF_SIZE);
	reply[7] = count;
	totalSize = 32;
	for (; i < count; i++) {
		res = sdbgpPackProcessInfo(cpu, (uint32_t *)dst, work, pids[i],
					   &entrySize);
		if (res < 0)
			goto fail;
		dst += entrySize;
		totalSize += entrySize;
	}
	sdbgpSetSendSize(cpu, totalSize);
	sdbgpSendErrorHeader(cpu, packet->group, 0x01, 0, 0);
	return 0;

fail:
	sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
		       res);
	return SDBGP_ERROR_INVALID_ARGUMENT;
}

/* 0x10: attach to a process */
int sdbgpCmd20_10(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	SceUID pid;
	uint32_t err;
	void *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x17) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	pid = *(const SceUID *)((const uint8_t *)packet + 20);
	if (isIllegalProcessId(pid)) {
		err = SDBGP_RESULT_ILLEGAL_PROCESS_ID;
		goto fail;
	}
	if (isAlreadyAttached(cpu, pid)) {
		err = SDBGP_RESULT_ALREADY_ATTACHED;
		goto fail;
	}
	if (isIllegalAttachState()) {
		err = 0x20053;
		goto fail;
	}
	cpu->flags = pid;
	reply = sdbgpAllocSendBuf(cpu, sizeof(SdbgpHeader));
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, sizeof(SdbgpHeader));
	sdbgpSetSendSize(cpu, sizeof(SdbgpHeader));
	sdbgpSendErrorHeader(cpu, packet->group, 0x11, 0, 0);
	return 0;

fail:
	sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
		       err);
	return SDBGP_ERROR_INVALID_ARGUMENT;
}

/* 0x12: detach from a process */
int sdbgpCmd20_12(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	SceUID pid;
	void *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x1b) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	pid = *(const SceUID *)((const uint8_t *)packet + 20);
	if (isIllegalProcessId(pid)) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	cpu->flags = 0;
	reply = sdbgpAllocSendBuf(cpu, sizeof(SdbgpHeader));
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, sizeof(SdbgpHeader));
	sdbgpSetSendSize(cpu, sizeof(SdbgpHeader));
	sdbgpSendErrorHeader(cpu, packet->group, 0x13, 0, 0);
	return 0;
}

/* 0x20: debug-suspend a process */
int sdbgpCmd20_20(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	SceSize threadCount = 0;
	SceSize procSize = 0;
	SceSize threadSize = 0;
	SceUID pid;
	void *work;
	uint32_t *reply;
	int res;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x17) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	pid = *(const SceUID *)((const uint8_t *)packet + 20);
	if (pid == 0 || isIllegalProcessId(pid) ||
	    pid == SDBGP_KERNEL_PROCESS_ID) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	if (isIllegalProcessStatus(pid)) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_PROCESS_SUSPENDED);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
	sceKernelCpuResumeIntr(0);
	res = sceKernelProcessDebugSuspend(pid, 17);
	sceKernelCpuSuspendIntr();
	if (res < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, res);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	reply = sdbgpAllocSendBuf(cpu, SDBGP_SEND_BUF_SIZE);
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, SDBGP_SEND_BUF_SIZE);
	res = sdbgpPackProcessInfo(cpu, reply + 14, work, pid, &procSize);
	if (res < 0 ||
	    (res = sdbgpGetThreadInfoList(cpu,
					  (uint8_t *)reply + procSize + 148,
					  work, pid, 0, &threadSize,
					  &threadCount)) < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, res);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	reply[8] = 0;
	reply[7] = threadCount;
	sdbgpGetProcessStopInfo(cpu, reply + 9, pid);
	sdbgpSetSendSize(cpu, procSize + threadSize + 148);
	sdbgpSendErrorHeader(cpu, packet->group, 0x21, 0, 0);
	return 0;
}

/* 0x22: debug-resume a process */
int sdbgpCmd20_22(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	SceKernelProcessInfo info;
	SceUID pid;
	void *work;
	uint32_t *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x1b) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	pid = *(const SceUID *)((const uint8_t *)packet + 20);
	if (pid == 0 || isIllegalProcessId(pid)) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	if (isProcessNotSuspended(pid)) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_PROCESS_NOT_SUSPENDED);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	work = sdbgpAllocWorkBuf(cpu, SDBGP_PACKET_MAX);
	info.size = sizeof(info);
	if (sceKernelGetProcessInfo(pid, &info) < 0 ||
	    sdbgpResumeProcessThreads(pid, work) < 0 ||
	    sceKernelProcessDebugResume(pid, info.status & 0xff) < 0)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	reply = sdbgpAllocSendBuf(cpu, 36);
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, 36);
	sdbgpSetSendSize(cpu, 36);
	reply[7] = pid;
	reply[8] = 0;
	sdbgpSendErrorHeader(cpu, packet->group, 0x23, 0, 0);
	return 0;
}

/* 0x52: memblock details (processGetMemblkInfoDetailCmd) */
int sdbgpCmd20_52(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	MemBlockPhyInfo stackPhyInfo;
	SceUInt32 partCount;
	SceUID pid;
	SceUID uid;
	uintptr_t vbase;
	SceSize vsize;
	void *work;
	SceUID asid;
	SceSize sendSize;
	int maxCount;
	uint32_t *reply;
	int res;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x27) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	uid = *(const SceUID *)((const uint8_t *)packet + 24);
	pid = *(const SceUID *)((const uint8_t *)packet + 20);
	if (pid == 0)
		pid = SDBGP_KERNEL_PROCESS_ID;
	vbase = *(const uintptr_t *)((const uint8_t *)packet + 28);
	vsize = *(const SceSize *)((const uint8_t *)packet + 32);
	work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
	SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, sizeof(SceKernelProcessInfo));
	((SceKernelProcessInfo *)work)->size = sizeof(SceKernelProcessInfo);
	if (sceKernelGetProcessInfo(pid, work) < 0 ||
	    ((SceKernelProcessInfo *)work)->status == 4) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	asid = ((SceKernelProcessInfo *)work)->addressSpace;
	if (uid != -1) {
		MemBlockPhyInfo *phy = work;

		if (uid & 1) {
			stackPhyInfo.ex.size = sizeof(stackPhyInfo);
			res = sceKernelGetMemBlockInfo(uid, 5, &stackPhyInfo);
			if (res < 0)
				goto send_err;
			s_seenPartCount = 0;
			SceDeci4pSDfMgrForKernel_8A3B06F2(s_seenPartIds, 0,
							  sizeof(s_seenPartIds));
			SceDeci4pSDfMgrForKernel_8A3B06F2(phy, 0, sizeof(*phy));
			phy->ex.size = sizeof(*phy);
			if (sceKernelGetMemBlockInfo(uid, 5, phy) < 0) {
				res = SCE_KERNEL_ERROR_UNKNOWN_MEMBLOCK;
				goto send_err;
			}
		} else {
			s_seenPartCount = 0;
			SceDeci4pSDfMgrForKernel_8A3B06F2(s_seenPartIds, 0,
							  sizeof(s_seenPartIds));
			SceDeci4pSDfMgrForKernel_8A3B06F2(phy, uid & 1,
							  sizeof(*phy));
			phy->ex.size = sizeof(*phy);
			if (SceSysmemForKernel_C3F9607E(SDBGP_KERNEL_PROCESS_ID,
							uid, 5, phy) < 0) {
				res = SCE_KERNEL_ERROR_UNKNOWN_MEMBLOCK;
				goto send_err;
			}
		}
		if ((int)(phy->count * 8) < 0) {
			res = phy->count * 8;
			goto send_err;
		}
		maxCount = 1;
		sendSize = 80 + (phy->count * 8 + 40);
		if (sendSize > SDBGP_PACKET_MAX) {
			maxCount = 5000;
			sendSize = 400040;
		}
	} else {
		AddressSpaceInfo *asInfo = work;
		MemBlockInfoEx *blocks = (MemBlockInfoEx *)(asInfo + 1);
		int nParts;

		s_seenPartCount = 0;
		SceDeci4pSDfMgrForKernel_8A3B06F2(s_seenPartIds, 0,
						  sizeof(s_seenPartIds));
		SceDeci4pSDfMgrForKernel_8A3B06F2(asInfo, 0, sizeof(*asInfo));
		asInfo->size = sizeof(*asInfo);
		res = sceKernelGetAddressSpaceInfo(asid, asInfo);
		if (res < 0)
			goto send_err;
		nParts = asInfo->partitionCount;
		if (nParts > 0) {
			int i;
			int total = 0;
			int lastCount = 0;

			for (i = 0; i < nParts; i++) {
				TinyPartition *part;
				int j;
				int dup = 0;

				if (pid != SDBGP_KERNEL_PROCESS_ID &&
				    asInfo->partition[i].vbase < 0x40000000u)
					continue;
				part = (TinyPartition *)asInfo->partition[i].partition;
				for (j = 0; j < i; j++) {
					if ((TinyPartition *)asInfo->partition[j].partition == part) {
						dup = 1;
						break;
					}
				}
				if (dup)
					continue;
				for (j = 0; j < s_seenPartCount; j++) {
					if (s_seenPartIds[j] == part) {
						dup = 1;
						break;
					}
				}
				if (dup || s_seenPartCount == 64)
					continue;
				s_seenPartIds[s_seenPartCount] = part;
				s_seenPartCount++;
				SceDeci4pSDfMgrForKernel_8A3B06F2(blocks, 0,
								  240000);
				if (SceSysmemForKernel_AD19285A(part, vbase,
								vbase + vsize,
								5, blocks,
								240000,
								&partCount) >= 0)
					lastCount = partCount;
				nParts = asInfo->partitionCount;
				total += lastCount;
			}
			maxCount = total;
			sendSize = 80 * total + 40;
			if (sendSize > SDBGP_PACKET_MAX) {
				maxCount = 5000;
				sendSize = 400040;
			}
		} else {
			maxCount = 0;
			sendSize = 40;
		}
	}
	reply = sdbgpAllocSendBuf(cpu, sendSize);
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, sendSize);
	sdbgpSetSendSize(cpu, sendSize);
	reply[7] = pid;
	if (uid == -1) {
		AddressSpaceInfo *asInfo = work;
		MemBlockInfoEx *blocks = (MemBlockInfoEx *)(asInfo + 1);
		uint32_t *dst = reply + 10;
		int filled = 0;
		int i;

		for (i = 0; i < s_seenPartCount; i++) {
			TinyPartition *part = s_seenPartIds[i];
			int k;

			if (!part ||
			    (pid != SDBGP_KERNEL_PROCESS_ID &&
			     part->vbase < 0x40000000u))
				continue;
			SceDeci4pSDfMgrForKernel_8A3B06F2(blocks, 0, 240000);
			res = SceSysmemForKernel_AD19285A(part, vbase,
							  vbase + vsize, 5,
							  blocks, 240000,
							  &partCount);
			if (res < 0)
				goto send_err;
			if ((int)partCount <= 0 || filled == maxCount)
				continue;
			for (k = 0; k < (int)partCount && filled != maxCount; k++) {
				const MemBlockInfoEx *b = &blocks[k];
				uint32_t *phyPart = b->details.phyMemPart;

				dst[16] = 0;
				dst[17] = 0;
				dst[1] = b->details.type;
				dst[2] = b->details.guid;
				dst[11] = b->details.vbase;
				dst[12] = b->details.size;
				dst[13] = b->details.mapSize;
				dst[0] = 80;
				dst[14] = b->details.headFree;
				dst[15] = b->details.tailFree;
				if (b->details.name != NULL)
					SceDeci4pSDfMgrForKernel_5996E2DB(&dst[3],
									  b->details.name,
									  32);
				SceDeci4pSDfMgrForKernel_5996E2DB(&dst[18],
								  &b->details.allocationAttributes,
								  4);
				filled++;
				if (phyPart != NULL)
					dst[19] = phyPart[4];
				else
					dst[19] = -1;
				dst = (uint32_t *)((uint8_t *)dst + dst[0]);
			}
		}
		if (filled < 0) {
			res = filled;
			goto send_err;
		}
		if (maxCount >= filled)
			maxCount = filled;
	} else {
		MemBlockPhyInfo *phy = work;
		uint32_t count;
		uint32_t *p;
		uint32_t *phyPart;

		SceDeci4pSDfMgrForKernel_8A3B06F2(phy, 0, sizeof(*phy));
		phy->ex.size = sizeof(*phy);
		if (uid & 1) {
			if (sceKernelGetMemBlockInfo(uid, 5, phy) < 0) {
				res = SCE_KERNEL_ERROR_UNKNOWN_MEMBLOCK;
				goto send_err;
			}
		} else if (SceSysmemForKernel_C3F9607E(SDBGP_KERNEL_PROCESS_ID,
						       uid, 5, phy) < 0) {
			res = SCE_KERNEL_ERROR_UNKNOWN_MEMBLOCK;
			goto send_err;
		}
		count = phy->count;
		reply[11] = phy->ex.details.type;
		reply[12] = phy->ex.details.guid;
		reply[21] = phy->ex.details.vbase;
		reply[10] = (count + 10) * 8;
		reply[22] = phy->ex.details.size;
		reply[23] = phy->ex.details.mapSize;
		reply[24] = phy->ex.details.headFree;
		reply[25] = phy->ex.details.tailFree;
		reply[26] = count;
		reply[27] = phy->rangeCount;
		if (phy->ex.details.name != NULL) {
			SceDeci4pSDfMgrForKernel_5996E2DB(&reply[13],
							  phy->ex.details.name,
							  32);
			count = phy->count;
		}
		SceDeci4pSDfMgrForKernel_5996E2DB(&reply[28], phy->pbase,
						  count * 4);
		count = phy->count;
		p = &reply[28 + count];
		SceDeci4pSDfMgrForKernel_5996E2DB(p, phy->size, count * 4);
		p += phy->count;
		SceDeci4pSDfMgrForKernel_5996E2DB(p,
						  &phy->ex.details.allocationAttributes,
						  4);
		phyPart = phy->ex.details.phyMemPart;
		if (phyPart != NULL)
			p[1] = phyPart[4];
		else
			p[1] = -1;
	}
	reply[9] = maxCount;
	sdbgpSendErrorHeader(cpu, packet->group, 0x53, 0, 0);
	return 0;

send_err:
	sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
		       res);
	return SDBGP_ERROR_INVALID_ARGUMENT;
}

/* 0x54: heap information (_getHeapInfoAll) */
int _getHeapInfoAll(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	HeapInfo stackInfo;
	SceSize count;
	SceSize entrySize;
	SceUID uid;
	void *work;
	uint32_t *reply;
	SceSize payloadSize;
	int outCount;
	int res;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x1b) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	uid = *(const SceUID *)((const uint8_t *)packet + 24);
	if (uid != -1) {
		stackInfo.size = sizeof(stackInfo);
		stackInfo.maxBlocks = 0;
		stackInfo.blockList = NULL;
		res = sceKernelGetHeapInfo(uid, 5, &stackInfo);
		if (res < 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1,
				       packet->tag, res);
			return SDBGP_ERROR_INVALID_ARGUMENT;
		}
	}
	work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
	reply = sdbgpAllocSendBuf(cpu, SDBGP_SEND_BUF_SIZE);
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, SDBGP_SEND_BUF_SIZE);
	if (uid == -1) {
		HeapInfo *infos = work;
		struct malloc_segment_range *ranges;
		uint32_t maxBlocks = 0;
		uint32_t nBlocks;
		uint32_t perHeapWords;
		int i;

		payloadSize = 0;
		count = 0;
		entrySize = 0;
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_WORK_BUF_SIZE);
		res = sceKernelGetHeapInfoAll(5, infos, SDBGP_WORK_BUF_SIZE,
					      &count);
		if (res >= 0) {
			for (i = 0; i < (int)count; i++) {
				if ((int)maxBlocks < (int)infos[i].blockCount)
					maxBlocks = infos[i].blockCount;
			}
			nBlocks = maxBlocks + 3;
			perHeapWords = maxBlocks + 17;
			SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0,
							  SDBGP_WORK_BUF_SIZE);
			if (8 * count * perHeapWords > SDBGP_WORK_BUF_SIZE)
				SCE_DBG_PRINTF(0, 2139,
					"#### sdbgp buffer overflow 0x%08x>0x%08x\n",
					8 * count * perHeapWords,
					SDBGP_WORK_BUF_SIZE);
			ranges = work;
			infos = (HeapInfo *)(ranges + count * nBlocks);
			for (i = 0; i < (int)count; i++) {
				infos[i].maxBlocks = nBlocks;
				infos[i].blockList = ranges;
				ranges += nBlocks;
			}
			res = sceKernelGetHeapInfoAll(5, infos,
						      SDBGP_WORK_BUF_SIZE,
						      &count);
			if (res >= 0) {
				uint8_t *dst = (uint8_t *)(reply + 9);

				payloadSize = 0;
				for (i = 0; i < (int)count; i++) {
					sdbgpPackHeapInfo((SdbgpHeapEntry *)dst,
							  &infos[i], &entrySize);
					payloadSize += entrySize;
					dst += entrySize;
				}
				outCount = count;
				goto done;
			}
		}
		outCount = 1;
	} else {
		HeapInfo *first = work;

		payloadSize = 0;
		SceDeci4pSDfMgrForKernel_8A3B06F2(first, 0, sizeof(*first));
		first->size = sizeof(*first);
		first->maxBlocks = 0;
		first->blockList = NULL;
		res = sceKernelGetHeapInfo(uid, 5, first);
		if (res >= 0) {
			uint32_t nBlocks = first->blockCount + 3;
			struct malloc_segment_range *ranges = work;
			HeapInfo *info = (HeapInfo *)(ranges + nBlocks);

			SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0,
							  8 * nBlocks + sizeof(*info));
			info->size = sizeof(*info);
			info->maxBlocks = nBlocks;
			info->blockList = ranges;
			res = sceKernelGetHeapInfo(uid, 5, info);
			if (res >= 0) {
				res = 0;
				sdbgpPackHeapInfo((SdbgpHeapEntry *)(reply + 9),
						  info, &entrySize);
				payloadSize = entrySize;
			}
		}
		outCount = 1;
	}
	if (res != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, res);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}

done:
	sdbgpSetSendSize(cpu, payloadSize + 36);
	reply[8] = outCount;
	reply[7] = SDBGP_KERNEL_PROCESS_ID;
	sdbgpSendErrorHeader(cpu, packet->group, 0x55, 0, 0);
	return 0;
}

/* 0x56: budget information */
int sdbgpCmd20_56(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	SceSize count;
	SceSize entrySize;
	SceUID budgetId;
	SceKernelBudgetInfoForDebugger *work;
	uint32_t *reply;
	SceSize totalSize;
	int res;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x17) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	budgetId = *(const SceUID *)((const uint8_t *)packet + 20);
	work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
	if (budgetId != -1) {
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, sizeof(*work));
		work->size = sizeof(*work);
		res = sceKernelGetBudgetInfoForDebugger(budgetId, 5, work);
		if (res < 0) {
			sdbgpSendError(cpu, packet->group, packet->command + 1,
				       packet->tag, res);
			return SDBGP_ERROR_INVALID_ARGUMENT;
		}
	}
	reply = sdbgpAllocSendBuf(cpu, SDBGP_PACKET_MAX);
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, SDBGP_PACKET_MAX);
	if (budgetId == -1) {
		uint8_t *dst = (uint8_t *)(reply + 8);
		int i;

		count = 0;
		entrySize = 0;
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_WORK_BUF_SIZE);
		if (SceProcessmgrForKernel_23EF9D8E(5, work,
						    SDBGP_WORK_BUF_SIZE,
						    &count) < 0)
			return SDBGP_ERROR_INVALID_ARGUMENT;
		reply[7] = count;
		totalSize = 32;
		for (i = 0; i < (int)count; i++) {
			sdbgpPackBudgetInfo((uint32_t *)dst, &work[i],
					    &entrySize);
			dst += entrySize;
			totalSize += entrySize;
		}
	} else {
		entrySize = 0;
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, SDBGP_WORK_BUF_SIZE);
		work->size = sizeof(*work);
		if (sceKernelGetBudgetInfoForDebugger(budgetId, 5, work) < 0)
			return SDBGP_ERROR_INVALID_ARGUMENT;
		reply[7] = 1;
		sdbgpPackBudgetInfo(reply + 8, work, &entrySize);
		totalSize = entrySize + 32;
	}
	sdbgpSetSendSize(cpu, totalSize);
	sdbgpSendErrorHeader(cpu, packet->group, 0x57, 0, 0);
	return 0;
}

/* 0x58: process local storage keys */
int sdbgpCmd20_58(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	ProcessLocalStorageInfo *work;
	uint32_t *reply;
	uint32_t *dst;
	int count = 0;
	int filled = 0;
	SceSize sendSize;
	int key;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x33) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	work = sdbgpAllocWorkBuf(cpu, SDBGP_PACKET_MAX);
	for (key = 0; key < 32; key++) {
		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, sizeof(*work));
		work->size = sizeof(*work);
		if (SceProcessmgrForDriver_C5CF15ED(key, work) < 0) {
			count = 0;
			sendSize = 32;
			goto alloc_reply;
		}
		if (work->storageSize != 0)
			count++;
	}
	sendSize = count * 44 + 32;
	if (sendSize > SDBGP_PACKET_MAX) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_PACKET_TOO_LARGE);
		return SDBGP_ERROR_PACKET_SIZE;
	}

alloc_reply:
	reply = sdbgpAllocSendBuf(cpu, sendSize);
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, sendSize);
	dst = reply + 8;
	sdbgpSetSendSize(cpu, sendSize);
	for (key = 0; key < 32; key++) {
		SceSize storageSize;

		SceDeci4pSDfMgrForKernel_8A3B06F2(work, 0, sizeof(*work));
		work->size = sizeof(*work);
		if (SceProcessmgrForDriver_C5CF15ED(key, work) < 0)
			return SDBGP_ERROR_INVALID_ARGUMENT;
		storageSize = work->storageSize;
		if (storageSize != 0) {
			dst[0] = key;
			dst[9] = storageSize;
			filled++;
			SceDeci4pSDfMgrForKernel_5996E2DB(&dst[1], &work->name,
							  32);
			dst += 10;
			if (filled == count)
				break;
		}
	}
	reply[7] = count;
	sdbgpSendErrorHeader(cpu, packet->group, 0x59, 0, 0);
	return 0;
}

/* 0x5a: kernel fixed heaps (_getFixedHeapInfo) */
int _getFixedHeapInfo(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	SceKernelFixedHeapInfo *work;
	uint32_t *reply;
	uint8_t *dst;
	SceSize totalSize = 0;
	int count = 0;
	SceUID uid;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x33) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	work = sdbgpAllocWorkBuf(cpu, SDBGP_PACKET_MAX);
	reply = sdbgpAllocSendBuf(cpu, SDBGP_SEND_BUF_SIZE);
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	dst = (uint8_t *)(reply + 8);
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, SDBGP_SEND_BUF_SIZE);
	for (uid = 0x10033; uid != 0x10043; uid += 2) {
		int res;
		uint32_t pageCount;
		uintptr_t cursor;
		uint32_t usedInPage;
		uint32_t perPage16;
		uint8_t *p;
		SceSize tailOff;
		void **node;
		uint32_t listWords;
		SceSize entrySize;
		int c;

		work->size = sizeof(*work);
		res = sceKernelGetFixedHeapInfo(uid, work);
		if (res < 0) {
			SceDeci4pSDfMgrForKernel_501CE61D(15, SCE_DBGINFO(2458),
				"sceKernelGetFixedHeapInfo failed 0x%08x\n",
				res);
			continue;
		}
		*(uint16_t *)(dst + 70) = work->freeCount16;
		*(uint16_t *)(dst + 72) = work->usedInPage;
		*(uint16_t *)(dst + 74) = work->perPage16;
		*(uint32_t *)(dst + 40) = work->attr;
		*(uint32_t *)(dst + 44) = work->itemSize;
		*(uint32_t *)(dst + 48) = work->totalSize;
		*(uint32_t *)(dst + 52) = work->perPage;
		*(uint32_t *)(dst + 56) = work->freeCount;
		*(uint32_t *)(dst + 60) = work->peakCount;
		*(uint32_t *)(dst + 64) = (uintptr_t)work->cursor;
		*(uint32_t *)(dst + 4) = uid;
		*(uint16_t *)(dst + 68) = work->totalCount;
		*(uint32_t *)(dst + 76) = work->pageSize;
		*(uint32_t *)(dst + 80) = work->maxPages;
		*(uint32_t *)(dst + 84) = (uintptr_t)work->currentPage;
		count++;
		for (c = 0; c < FIXED_HEAP_CPUS; c++) {
			uint8_t *cdst = dst + 88 + c * 16;

			*(uint16_t *)(cdst + 2) = work->caches[c].count;
			*(uint16_t *)(cdst + 0) = work->caches[c].max;
			*(uint32_t *)(cdst + 4) = work->caches[c].reserved;
			*(uint32_t *)(cdst + 8) = (uintptr_t)work->caches[c].head;
			*(uint32_t *)(cdst + 12) = (uintptr_t)work->caches[c].tail;
		}
		*(uint32_t *)(dst + 152) = work->pageCount;
		SceDeci4pSDfMgrForKernel_5996E2DB(dst + 8, work->name, 32);
		SceDeci4pSDfMgrForKernel_5996E2DB(dst + 156, work->pages,
						  work->pageCount * 4);
		p = dst + 156 + work->pageCount * 4;
		SceDeci4pSDfMgrForKernel_5996E2DB(p, work->blocks,
						  work->pageCount * 4);
		pageCount = work->pageCount;
		cursor = (uintptr_t)work->cursor;
		p += pageCount * 4;
		*(uint32_t *)p = 0;
		if (cursor != 0 &&
		    (usedInPage = work->usedInPage,
		     perPage16 = work->perPage16,
		     usedInPage < perPage16)) {
			uint32_t itemSize = work->itemSize;
			uint32_t n = 0;
			uint32_t *out = (uint32_t *)p;

			do {
				n++;
				out[n] = cursor;
				cursor += itemSize;
			} while (n != perPage16 - usedInPage);
			out[0] = n;
			tailOff = (perPage16 - usedInPage + 1) * 4;
		} else {
			tailOff = 4;
		}
		node = work->freeHead;
		entrySize = 156 + pageCount * 8 + tailOff;
		p += tailOff;
		*(uint32_t *)p = 0;
		listWords = 1;
		if (node != NULL) {
			uint32_t *out = (uint32_t *)p;
			uint32_t n = 0;

			for (;;) {
				out[n + 1] = (uintptr_t)node;
				out[0] = n + 1;
				res = sceKernelIsAccessibleRange(1, node, 4);
				if (res < 0) {
					SceDeci4pSDfMgrForKernel_501CE61D(15,
						SCE_DBGINFO(2527),
						"sceKernelIsAccessibleRange failed 0x%08x:%p\n",
						res, node);
					listWords = out[0] + 1;
					break;
				}
				node = (void **)*node;
				n = out[0];
				if (node == NULL) {
					listWords = n + 1;
					break;
				}
			}
		}
		entrySize += listWords * 4;
		p += listWords * 4;
		*(uint32_t *)p = 0;
		for (c = 0; c < FIXED_HEAP_CPUS; c++) {
			uint32_t *out = (uint32_t *)p;
			uint32_t words = 1;

			node = work->caches[c].head;
			if (node != NULL) {
				uint32_t n = 0;

				for (;;) {
					out[n + 1] = (uintptr_t)node;
					out[0] = n + 1;
					res = sceKernelIsAccessibleRange(1,
									 node,
									 4);
					if (res < 0) {
						SceDeci4pSDfMgrForKernel_501CE61D(15,
							SCE_DBGINFO(2552),
							"sceKernelIsAccessibleRange failed 0x%08x:%p\n",
							res, node);
						words = out[0] + 1;
						break;
					}
					node = (void **)*node;
					n = out[0];
					if (node == NULL) {
						words = n + 1;
						break;
					}
				}
			}
			p += words * 4;
			entrySize += words * 4;
			*(uint32_t *)p = 0;
		}
		*(uint32_t *)dst = entrySize;
		dst += entrySize;
		totalSize += entrySize;
	}
	sdbgpSetSendSize(cpu, totalSize + 32);
	reply[7] = count;
	sdbgpSendErrorHeader(cpu, packet->group, 0x5b, 0, 0);
	return 0;
}

/* 0x60: change a process's CPU affinity */
int sdbgpCmd20_60(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	const SdbgpProcessControlRequest *req =
		(const SdbgpProcessControlRequest *)packet;
	SceUID pid;
	uint32_t affinity;
	int res;
	void *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x53) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	pid = req->processId;
	affinity = req->flags;
	if (isIllegalProcessId(pid)) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	if (isIllegalProcessControl(req)) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_ILLEGAL_VALUE);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	res = sceKernelChangeProcessCpuAffinity(pid, affinity);
	if (res < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, res);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	reply = sdbgpAllocSendBuf(cpu, 92);
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, 92);
	sdbgpSetSendSize(cpu, 92);
	sdbgpSendErrorHeader(cpu, packet->group, 0x61, 0, 0);
	return 0;
}

/* 0x80: suspend a process for the debugger */
int sdbgpCmd20_80(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	SceUID pid;
	uint32_t *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x17) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	pid = *(const SceUID *)((const uint8_t *)packet + 20);
	if (pid == 0 || isIllegalProcessId(pid)) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	if (sceKernelSuspendProcessForDebugger(pid) < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_PROCESS_NOT_SUSPENDED);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	reply = sdbgpAllocSendBuf(cpu, 32);
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, 32);
	sdbgpSetSendSize(cpu, 32);
	reply[7] = pid;
	sdbgpSendErrorHeader(cpu, packet->group, 0x81, 0, 0);
	return 0;
}

/* 0x82: resume a process for the debugger */
int sdbgpCmd20_82(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	SceUID pid;
	uint32_t *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x17) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	pid = *(const SceUID *)((const uint8_t *)packet + 20);
	if (pid == 0 || isIllegalProcessId(pid)) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	if (sceKernelResumeProcessForDebugger(pid) < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_PROCESS_NOT_SUSPENDED);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	reply = sdbgpAllocSendBuf(cpu, 32);
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, 32);
	sdbgpSetSendSize(cpu, 32);
	reply[7] = pid;
	sdbgpSendErrorHeader(cpu, packet->group, 0x83, 0, 0);
	return 0;
}

/* 0x84: kill a process for the debugger */
int sdbgpCmd20_84(SdbgpCpuState *cpu, SdbgpHeader *packet, uint8_t command)
{
	SceUID pid;
	int res;
	uint32_t *reply;

	(void)command;
	if (sdbgpGetRecvSize(cpu) <= 0x17) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	pid = *(const SceUID *)((const uint8_t *)packet + 20);
	if (isIllegalProcessId(pid)) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, SDBGP_RESULT_ILLEGAL_PROCESS_ID);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	res = sceKernelKillProcessForDebugger(pid, 0);
	if (res < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1,
			       packet->tag, res);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	reply = sdbgpAllocSendBuf(cpu, 32);
	if (!reply)
		return SDBGP_ERROR_OUT_OF_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, 32);
	sdbgpSetSendSize(cpu, 32);
	reply[7] = pid;
	sdbgpSendErrorHeader(cpu, packet->group, 0x85, 0, 0);
	return 0;
}
