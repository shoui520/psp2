/*
 * SceDeci4pSDbgp background sysmem checker (bg_check.c).
 */

#include <string.h>
#include <scetypes.h>
#include <sysmem/debug.h>
#include <sysmem/heap_info.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include "sdbgp_internal.h"

static const SceUID s_fixedHeapUids[8] = {
	0x10033, 0x10035, 0x10037, 0x10039,
	0x1003b, 0x1003d, 0x1003f, 0x10041,
};

static const SceUID s_partitionUids[11] = {
	0x10009, 0x10043, 0x10045, 0x10047,
	0x1004b, 0x1004d, 0x1004f, 0x10051,
	0x10053, 0x10055, 0x10057,
};

static const SceUID s_heapUids[2] = {
	0x1000b, 0x1000d,
};

static SceUInt32 gCountUpdate = 1;
static SceSize gFreeSizeMin = 0xffffffff;
static SceUID gWQ = -1;

static SceSize gFreeSize;
static SceUInt32 gCheckStep;
static SceUInt32 gFixedHeapIdx;
static void *gFixedHeapLeakCheckCtx;
static SceUInt32 gHeapIdx;
static SceUInt32 gPartitionIdx;
static SceUInt32 gPhyMemPartIdx;
static SceUInt32 gCheckerCount;
static int gCountFixedHeapLeakCheck;

static int FixedHeapLeakCheck(void)
{
	int ret;

	SCE_DBG_PRINTF(0x0b, 84, "(), gCountFixedHeapLeakCheck=%d\n", gCountFixedHeapLeakCheck);
	if (gCountFixedHeapLeakCheck == 0) {
		ret = KernelUtilsFixedHeapLeakCheckStart(0x1003b, &gFixedHeapLeakCheckCtx);
		if (ret < 0) {
			SCE_DBG_PRINTF(0x0b, 88,
				       "KernelUtilsFixedHeapLeakCheckStart failed 0x%08x\n", ret);
			return ret;
		}
	} else {
		KernelUtilsFixedHeapLeakCheck(gFixedHeapLeakCheckCtx);
	}
	gCountFixedHeapLeakCheck++;
	return 0;
}

static int getFixedHeapInfoCore(SceUID uid, SceKernelFixedHeapInfo *info)
{
	int ret;

	memset(info, 0, sizeof(*info));
	info->size = sizeof(*info);
	ret = sceKernelGetFixedHeapInfo(uid, info);
	if (ret < 0) {
		SCE_DBG_PRINTF(0x0b, 111, "sceKernelGetFixedHeapInfo failed 0x%08x\n", ret);
		return ret;
	}
	return ret;
}

static int getFixedHeapInfo(void)
{
	SceKernelFixedHeapInfo info;
	SceUID uid;

	uid = s_fixedHeapUids[gFixedHeapIdx & 7];
	SCE_DBG_PRINTF(0x0b, 122, "(0x%08x)\n", uid);
	getFixedHeapInfoCore(uid, &info);
	gFixedHeapIdx++;
	return 0;
}

static int showFixedHeap(void)
{
	SceKernelFixedHeapInfo info;
	unsigned int i;

	for (i = 0; i < 8; i++) {
		if (getFixedHeapInfoCore(s_fixedHeapUids[i], &info) >= 0) {
			SCE_DBG_PRINTF(0x0b, 137, ".objectSize     0x%08x\n", info.itemSize);
			SCE_DBG_PRINTF(0x0b, 138, ".totalHeapSize  0x%08x\n", info.totalSize);
			SCE_DBG_PRINTF(0x0b, 139, ".totalItemCount 0x%08x\n", info.perPage);
			SCE_DBG_PRINTF(0x0b, 140, ".totalFreeCount 0x%08x\n", info.freeCount);
			SCE_DBG_PRINTF(0x0b, 141, ".nUsedMax       0x%08x\n", info.peakCount);
			SCE_DBG_PRINTF(0x0b, 142, ".nItemInPage    0x%08x\n", info.usedInPage);
			SCE_DBG_PRINTF(0x0b, 143, ".nMaxItemInPage 0x%08x\n", info.perPage16);
			SCE_DBG_PRINTF(0x0b, 144, ".nListCount     0x%08x\n", info.listCount);
			SCE_DBG_PRINTF(0x0b, 145, ".nItem          0x%08x\n", info.totalCount);
			SCE_DBG_PRINTF(0x0b, 146, ".nPage          0x%08x\n", info.pageCount);
			SCE_DBG_PRINTF(0x0b, 147, ".cpuCache[0].nCount         0x%08x\n",
				       info.caches[0].count);
			SCE_DBG_PRINTF(0x0b, 148, ".cpuCache[1].nCount         0x%08x\n",
				       info.caches[1].count);
			SCE_DBG_PRINTF(0x0b, 149, ".cpuCache[2].nCount         0x%08x\n",
				       info.caches[2].count);
			SCE_DBG_PRINTF(0x0b, 150, ".cpuCache[3].nCount         0x%08x\n",
				       info.caches[3].count);
		}
	}
	return 0;
}

static int showGUIDInfo(void)
{
	SceKernelUIDEntryHeapInfo info = { sizeof(info) };

	if (sceKernelUIDEntryHeapGetInfo(0x10003, &info) >= 0) {
		SCE_DBG_PRINTF(0x0b, 177,
			       ".objectSize=0x%08x, .totalHeapSize=0x%08x, .totalItemCount=0x%08x\n",
			       info.itemSize, info.totalSize, info.perPage);
	}
	return 0;
}

static int UpdateKernelBudget(void)
{
	PartitionInfo partInfo = { sizeof(partInfo) };
	HeapInfo heapInfo;

	SCE_DBG_PRINTF(0x0b, 192, "%d: Update %d: gFreeSizeMin 0x%08x <- 0x%08x\n",
		       gCheckerCount, gCountUpdate, gFreeSize, gFreeSizeMin);
	gFreeSizeMin = gFreeSize;
	gCountUpdate++;
	if (sceKernelPartitionGetInfo(0x10009, &partInfo) >= 0)
		SCE_DBG_PRINTF(0x0b, 203, "[%s] nBlock=%d\n", partInfo.name, partInfo.blockCount);
	memset(&heapInfo, 0, sizeof(heapInfo));
	heapInfo.size = sizeof(heapInfo);
	if (sceKernelGetHeapInfo(0x1000b, 5, &heapInfo) >= 0) {
		SCE_DBG_PRINTF(0x0b, 213, "nChunk=0x%08x, nChunkMax=0x%08x\n",
			       heapInfo.chunks, heapInfo.peakChunks);
		SCE_DBG_PRINTF(0x0b, 215, "sizeChunkTotal=0x%08x, sizeChunkTotalMax=0x%08x\n",
			       heapInfo.used, heapInfo.peakUsed);
	}
	sceKernelShowPhyMemPart(SceSysmemForKernel_0164D817());
	showFixedHeap();
	showGUIDInfo();
	if (gCountUpdate > 3)
		FixedHeapLeakCheck();
	return 0;
}

static int CheckPhyMemPart(void)
{
	SceKernelPhyMemPartInfoForDebugger info;
	PhyMemPart *part;
	int ret;

	SCE_DBG_PRINTF(0x0b, 237, "()\n");
	switch (gPhyMemPartIdx % 3) {
	case 0:
		part = SceSysmemForKernel_0164D817();
		break;
	case 1:
		part = SceSysmemForKernel_0B1B4ACA();
		break;
	case 2:
		part = SceSysmemForKernel_62800A6B();
		break;
	default:
		part = NULL;
		break;
	}
	if (part != NULL) {
		memset(&info, 0, sizeof(info));
		info.size = sizeof(info);
		ret = sceKernelGetPhyMemPartInfoForDebugger(part, &info);
		if (ret < 0) {
			SCE_DBG_PRINTF(0x0b, 257,
				       "sceKernelGetPhyMemPartInfoForDebugger failed 0x%08x\n",
				       ret);
		} else {
			gFreeSize = info.freeSize;
			if (gFreeSize < gFreeSizeMin)
				UpdateKernelBudget();
		}
	}
	gPhyMemPartIdx++;
	return 0;
}

static int CheckPartition(void)
{
	PartitionInfo info = { sizeof(info) };
	SceUID uid;
	int ret;

	uid = s_partitionUids[gPartitionIdx % 11];
	ret = sceKernelPartitionGetInfo(uid, &info);
	if (ret < 0) {
		SCE_DBG_PRINTF(0x0b, 284, "sceKernelPartitionGetInfo(0x%08x) failed 0x%08x\n",
			       uid, ret);
	}
	gPartitionIdx++;
	return 0;
}

static int sysmemChecker(void)
{
	SCE_DBG_PRINTF(0x0b, 294, "() %d\n", gCheckerCount);
	gCheckerCount++;
	switch (gCheckStep & 3) {
	case 0:
		SCE_DBG_PRINTF(0x0b, 298, "->sceKernelVerifyHeap()\n");
		sceKernelVerifyHeap(s_heapUids[gHeapIdx & 1], NULL);
		gHeapIdx++;
		break;
	case 1:
		CheckPhyMemPart();
		break;
	case 2:
		getFixedHeapInfo();
		break;
	case 3:
		CheckPartition();
		break;
	}
	gCheckStep++;
	return 0;
}

static int CleanerLowPrioTask(void *arg)
{
	int ret;

	(void)arg;
	sysmemChecker();
	sceKernelDelayThread(20000000);
	ret = sceKernelScheduleWorkTask(gWQ, "SceKernelCleanerLowPrioTask", CleanerLowPrioTask, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF(0x0b, 339, "sceKernelScheduleWorkTask failed 0x%08x\n", ret);
		return ret;
	}
	return 0;
}

int bgCheckStart(void)
{
	ThreadmgrWorkQueueOpt opt;
	int ret;

	SCE_DBG_PRINTF(0x0b, 351, "()\n");
	opt.size = sizeof(opt);
	opt.threadCount = 1;
	opt.priority = 0xfd;
	opt.stackSize = 0x1000;
	opt.cpuAffinityMask = 0x80000;
	gWQ = sceKernelCreateWorkQueue("SceKernelCleanerLowPrio", 0x0f, &opt);
	if (gWQ < 0) {
		SCE_DBG_PRINTF(0x0b, 364, "sceKernelCreateWorkQueue failed 0x%08x\n", gWQ);
		return gWQ;
	}
	SCE_DBG_PRINTF(0x0b, 367, "gWQ=0x%08x\n", gWQ);
	ret = sceKernelScheduleWorkTask(gWQ, "SceKernelCleanerLowPrioTask", CleanerLowPrioTask, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF(0x0b, 372, "sceKernelScheduleWorkTask failed 0x%08x\n", ret);
		return ret;
	}
	return gWQ;
}
