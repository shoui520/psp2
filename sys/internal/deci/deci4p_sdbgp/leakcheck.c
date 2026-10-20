/*
 * Kernel resource leak checks: snapshots of the kernel's processes, heaps and
 * UIDs compared pass by pass, the kernel heap's allocations logged, and the
 * sections run with interrupts masked.
 */

#include <string.h>
#include <scetypes.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/heap_info.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <sysmem/uid_class.h>
#include <modulemgr/modulemgr.h>
#include <processmgr/processmgr.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/threadmgr_private.h>
#include "sdbgp_internal.h"

/* every log is one memblock of this size */
#define LEAKCHECK_LOG_SIZE		0x100000

/* UID attribute bits the UID list reports */
#define LEAKCHECK_UID_ATTR_AO		0x00100000
#define LEAKCHECK_UID_ATTR_AC		0x00200000
#define LEAKCHECK_UID_ATTR_ZO		0x00400000
#define LEAKCHECK_UID_ATTR_ZC		0x00800000

/* the kernel's own heaps and the GUID entry heap, by their fixed UIDs */
#define LEAKCHECK_GUID_ENTRY_HEAP	0x10003
#define LEAKCHECK_KERNEL_HEAP		0x1000b
#define LEAKCHECK_KERNEL_UNCACHE_HEAP	0x1000d

/* one allocation of a logged heap */
typedef struct LeakCheckChunk {
	SceUID heap;				/* a heap's GUID, or its address (bit 0 clear) */
	void *ptr;
	SceSize size;
	const void *caller;
	SceUInt32 func;				/* HEAP_FUNC_* */
	uint32_t generation;
} LeakCheckChunk;

/* the same with the heap's name (the leak check log in heap mode) */
typedef struct LeakCheckNamedChunk {
	LeakCheckChunk chunk;
	char name[32];
} LeakCheckNamedChunk;

/* a UID the UID list found new since the first pass */
typedef struct LeakCheckUIDListEntry {
	uint32_t generation;
	SceClass *cls;
	SceUID uid;
	SceUInt32 attr;
	uint32_t reserved[4];
	char name[32];
} LeakCheckUIDListEntry;

/* the leak check log in UID mode */
typedef struct LeakCheckUIDLogEntry {
	uint32_t generation;
	SceUID uid;
	SceUInt32 attr;
	uint32_t reserved[4];
	char name[32];
	char className[32];
} LeakCheckUIDLogEntry;

/* a UID the snapshot compare found new ("SceResourceInfo") */
typedef struct LeakCheckResource {
	uint32_t generation;
	const char *label;
	SceUID uid;
	uint32_t reserved[5];
	char name[32];
} LeakCheckResource;

typedef struct LeakCheckHeapFunc {
	SceUInt32 func;
	const char *name;
} LeakCheckHeapFunc;

static const LeakCheckHeapFunc s_heapFuncs[] = {
	{ HEAP_FUNC_ALLOC_HEAP_MEMORY, "sceKernelAllocHeapMemory" },
	{ HEAP_FUNC_ALLOC_HEAP_MEMORY_WITH_OPTION, "sceKernelAllocHeapMemoryWithOption" },
	{ HEAP_FUNC_ALLOC_UNCACHE, "sceKernelAllocUncacheHeapMemory" },
	{ HEAP_FUNC_ALLOC_UNCACHE_WITH_OPTION, "sceKernelAllocUncacheHeapMemoryWithOption" },
	{ HEAP_FUNC_ALLOC, "sceKernelAlloc(KernelHeap)" },
	{ HEAP_FUNC_ALLOC_WITH_OPT, "sceKernelAllocWithOpt(KernelHeap)" },
	{ HEAP_FUNC_ALLOC_METHOD, "AllocMethod" },
};

static __attribute__((noinline)) void LeakCheckHeapAlloc(SceUID heap, void *ptr, SceSize size, const void *caller,
			       SceUInt32 func);
static __attribute__((noinline)) void LeakCheckHeapFree(SceUID heap, void *ptr);
static __attribute__((noinline)) void LeakCheckNamedAlloc(SceUID heap, void *ptr, SceSize size, const void *caller,
				SceUInt32 func);
static __attribute__((noinline)) void LeakCheckNamedFree(SceUID heap, void *ptr);

static HeapHooks s_heapLogHooks = { LeakCheckHeapAlloc, LeakCheckHeapFree };
static HeapHooks s_namedLogHooks = { LeakCheckNamedAlloc, LeakCheckNamedFree };

/* the snapshots, compared in turn */
static SdbgpSystemResourceSnapshot *s_snapshotA;
static SdbgpSystemResourceSnapshot *s_snapshotB;
static SceUID s_snapshotAMemblock;
static SceUID s_snapshotBMemblock;
static uint32_t s_snapshotCount;
static uint32_t s_leakCheckInitDone;

/* the thread manager's classes */
static uint32_t s_threadmgrReady;
static struct SceKernelThreadmgrPrivate *s_threadmgr;
static SceUID s_leakCheckHeap;

/* the heap log */
static LeakCheckChunk *s_heapLog;		/* NULL: not logging */
static LeakCheckChunk *s_heapLogBuffer;
static SceUID s_heapLogMemblock;
static SceKernelSpinlock s_heapLogLock;
static uint32_t s_heapGeneration;
static uint32_t s_heapCheckByGeneration;
static uint32_t s_heapLogKeep;
static uint32_t s_heapLogging;
static uint32_t s_showModule;
static uint32_t s_chunkHeaderShown;
static SceUID s_heapFilter[16];
static SceSize s_heapFilterCount;
static char s_unknownFuncName[32];

/* the CPU interrupt mask log of the snapshots */
static struct CpuLongIrqHandlerRecord s_maskLogHook;
static SceUID s_maskLogMemblock;
static SdbgpCpuMaskEntry *s_maskLog;
static SceSize s_maskLogCount;
static SceSize s_maskLogMax;

/* the UIDs new since the snapshot compare's first pass */
static SceUID s_resourceMemblock;
static LeakCheckResource *s_resource;
static SceSize s_resourceCount;
static SceSize s_resourceMax;

/* the UID list: the first pass (B), the current one (C), what is new (L) */
static SceUID s_uidListBMemblock;
static SceKernelUIDEntryInfo *s_uidListB;
static SceSize s_uidListBMax;
static SceSize s_uidListBCount;
static SceUID s_uidListCMemblock;
static SceKernelUIDEntryInfo *s_uidListC;
static SceSize s_uidListCMax;
static SceUID s_uidListLMemblock;
static LeakCheckUIDListEntry *s_uidListL;
static SceSize s_uidListLCount;
static SceSize s_uidListLMax;
static uint32_t s_uidListCleared[2];		/* zeroed with the lists; nothing reads them */
static uint32_t s_uidListGeneration;
static uint32_t s_uidListShown;

static SceKernelSpinlock s_namedLogLock;
static uint32_t s_leakcheckResetDone;		/* cleared by _leakcheckReset; nothing reads it */

/* the snapshot compare's report state */
static uint32_t s_processHeaderShown;
static uint32_t s_partitionHeaderShown;
static uint32_t s_phyMemPartHeaderShown;
static void *s_partitionSeen[256];
static SceSize s_partitionSeenCount;
static void *s_phyMemPartSeen[256];
static SceSize s_phyMemPartSeenCount;

SdbgpCpuMaskLog sdbgpCpuMaskLog;

static __attribute__((noinline)) int LeakCheckCpuMaskLogAdd(int kind, uint32_t duration, unsigned core,
				  uintptr_t lockedCaller, const void *resumeCaller)
{
	SdbgpCpuMaskEntry *entry;

	if (s_maskLogCount < s_maskLogMax) {
		entry = &s_maskLog[s_maskLogCount++];
		entry->kind = kind;
		entry->duration = duration;
		entry->core = core;
		entry->lockedCaller = lockedCaller;
		entry->resumeCaller = resumeCaller;
	}
	return kind;
}

int sdbgpCpuMaskLogAdd(int kind, uint32_t duration, unsigned core, uintptr_t lockedCaller,
		       const void *resumeCaller)
{
	SdbgpCpuMaskEntry *entry;

	if (sdbgpCpuMaskLog.count < sdbgpCpuMaskLog.max) {
		entry = &sdbgpCpuMaskLog.entries[sdbgpCpuMaskLog.count];
		entry->kind = kind;
		entry->duration = duration;
		entry->core = core;
		entry->lockedCaller = lockedCaller;
		entry->resumeCaller = resumeCaller;
		sdbgpCpuMaskLog.count++;
	}
	return kind;
}

/* The fixed heaps of one of a process's object heaps. */
static __attribute__((noinline)) int LeakCheckGetFixedHeapInfo(SdbgpSystemResourceSnapshot *snap, int proc, int slot,
				     const ObjectHeapInfo *objectHeap)
{
	SceKernelFixedHeapInfo *info = snap->fixedHeap[proc][slot];
	unsigned int i;
	int result;

	for (i = 0; i < objectHeap->count; i++, info++) {
		if (objectHeap->sizeClasses[i].size == 0 ||
		    objectHeap->sizeClasses[i].size == (SceSize)-1)
			continue;
		info->size = sizeof(*info);
		result = sceKernelGetFixedHeapInfoByPointer(objectHeap->sizeClasses[i].heap, info);
		if (result < 0) {
			SceDebugForDriver_391B74B7("sceKernelGetFixedHeapInfoByPointer failed 0x%08x\n",
						   result);
			return result;
		}
	}
	return 0;
}

/* The processes of one snapshot the other does not have. */
static __attribute__((noinline)) int LeakCheckShowLostProcess(const char *label, const SdbgpSystemResourceSnapshot *snap,
				    const SdbgpSystemResourceSnapshot *other)
{
	const SceKernelProcessInfo *proc;
	int i, j;

	for (i = 0; i < snap->processCount; i++) {
		proc = &snap->process[i];
		for (j = 0; j < other->processCount; j++)
			if (other->process[j].pid == proc->pid)
				break;
		if (j < other->processCount)
			continue;
		if (s_processHeaderShown == 0) {
			SceDebugForDriver_391B74B7("\n");
			SceDebugForDriver_391B74B7("PRO:[name----------------------------] uid-------  AS--------\n");
			s_processHeaderShown++;
		}
		SceDebugForDriver_391B74B7("%s:[%-32s] 0x%08x  0x%08x\n", label, proc->name, proc->pid,
					   proc->addressSpace);
	}
	return 0;
}

static __attribute__((noinline)) int LeakCheckShowPartition(const char *label, const PartitionInfo *part)
{
	SceDebugForDriver_391B74B7("\t%s:[%-32s] 0x%08x+0x%08x  0x%08x  0x%08x\n", label, part->name,
				   part->vbase, part->vsize, part->vsize - part->freeSize,
				   part->freeSize);
	return SceDebugForDriver_391B74B7("\t   :0x%08x(0x%08x)  0x%08x(0x%08x)  0x%08x(0x%08x)\n",
					  part->blockCount, part->blockCountMax, part->l2Count,
					  part->l2CountMax, part->virPageCount,
					  part->virPageCountMax);
}

static __attribute__((noinline)) int LeakCheckShowHeap(const HeapInfo *old, const HeapInfo *new)
{
	SceDebugForDriver_391B74B7("\tOLD: 0x%08x(0x%08x)  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x :0x%08x\n",
				   old->totalSize, old->peakTotalSize, old->maxSize, old->footprint,
				   old->fordblks, old->uordblks, old->chunks, old->peakChunks);
	SceDebugForDriver_391B74B7("\tNEW: ");
	if (old->totalSize == new->totalSize)
		SceDebugForDriver_391B74B7("          ");
	else
		SceDebugForDriver_391B74B7("0x%08x", new->totalSize);
	if (old->peakTotalSize == new->peakTotalSize)
		SceDebugForDriver_391B74B7("(          ) ");
	else
		SceDebugForDriver_391B74B7("(0x%08x) ", new->peakTotalSize);
	if (old->maxSize == new->maxSize)
		SceDebugForDriver_391B74B7(":           ");
	else
		SceDebugForDriver_391B74B7(":0x%08x ", new->maxSize);
	if (old->footprint == new->footprint)
		SceDebugForDriver_391B74B7(":           ");
	else
		SceDebugForDriver_391B74B7(":0x%08x ", new->footprint);
	if (old->fordblks == new->fordblks)
		SceDebugForDriver_391B74B7(":           ");
	else
		SceDebugForDriver_391B74B7(":0x%08x ", new->fordblks);
	if (old->uordblks == new->uordblks)
		SceDebugForDriver_391B74B7(":           ");
	else
		SceDebugForDriver_391B74B7(":0x%08x ", new->uordblks);
	if (old->chunks == new->chunks)
		SceDebugForDriver_391B74B7(":          ");
	else
		SceDebugForDriver_391B74B7(":0x%08x ", new->chunks);
	if (old->peakChunks == new->peakChunks)
		return SceDebugForDriver_391B74B7(":         \n");
	return SceDebugForDriver_391B74B7(":0x%08x\n", new->peakChunks);
}

static __attribute__((noinline)) int LeakCheckCompareHeap(const char *label, const HeapInfo *old, const HeapInfo *new)
{
	if (old->size != new->size || old->arena != new->arena ||
	    old->footprint != new->footprint || old->fordblks != new->fordblks ||
	    old->systemBytes != new->systemBytes || old->hblkhd != new->hblkhd ||
	    old->keepcost != new->keepcost || old->blockCount != new->blockCount ||
	    old->totalSize != new->totalSize || old->peakTotalSize != new->peakTotalSize ||
	    old->maxSize != new->maxSize || old->uordblks != new->uordblks ||
	    old->inUseBytes != new->inUseBytes || old->usmblks != new->usmblks ||
	    old->chunks != new->chunks || old->peakChunks != new->peakChunks)
		SceDebugForDriver_391B74B7("\t%s\n", label);
	SceDebugForDriver_391B74B7("\tHEP: sizeAlloc-(max page--) :MAX------- :footprint- :fordblks-- :uordblks-- :nChunk---- :nChunkMax-\n");
	LeakCheckShowHeap(old, new);
	SceDebugForDriver_391B74B7("\n");
	return SceDebugForDriver_391B74B7("\n");
}

/* The variable-size heap of an object heap in both snapshots. */
static __attribute__((noinline)) int LeakCheckCompareProcessHeap(int slot, const SdbgpSystemResourceSnapshot *oldSnap,
				       const ObjectHeapInfo *oldHeap, int oldProc,
				       const SdbgpSystemResourceSnapshot *newSnap,
				       const ObjectHeapInfo *newHeap, int newProc)
{
	const HeapInfo *newInfo = &newSnap->processHeap[slot + 2 * newProc];
	const HeapInfo *oldInfo = &oldSnap->processHeap[slot + 2 * oldProc];
	unsigned int i, j;

	for (i = 0; i < newHeap->count; i++) {
		if (newHeap->sizeClasses[i].size != (SceSize)-1)
			continue;
		for (j = 0; j < oldHeap->count; j++)
			if (oldHeap->sizeClasses[j].heap == newHeap->sizeClasses[i].heap)
				break;
		if (j == oldHeap->count)
			continue;
		LeakCheckCompareHeap("VH-", oldInfo, newInfo);
	}
	return 0;
}

/*
 * The variable-size heap of one of a process's object heaps. Like Sony's, the
 * entry is taken 17 entries apart per heap here, one apart in the compare.
 */
static __attribute__((noinline)) int LeakCheckGetHeapInfo(SdbgpSystemResourceSnapshot *snap, int proc, int slot,
				const ObjectHeapInfo *objectHeap)
{
	HeapInfo *info;
	unsigned int i;
	int result;

	if (objectHeap->count == 0)
		return 0;
	for (i = 0; objectHeap->sizeClasses[i].size != (SceSize)-1; )
		if (++i == objectHeap->count)
			return 0;
	info = &snap->processHeap[17 * (2 * proc + slot)];
	info->size = sizeof(*info);
	info->maxBlocks = LEAKCHECK_HEAP_BLOCK_MAX;
	info->blockList = snap->processHeapBlocks;
	result = sceKernelGetHeapInfoByPtr(objectHeap->sizeClasses[i].heap, info);
	if (result < 0)
		SceDebugForDriver_391B74B7("sceKernelGetHeapInfoByPtr failed 0x%08x\n", result);
	return result;
}

int LeakCheckHeapInit(void)
{
	SceUID uid;

	uid = sceKernelAllocMemBlockForKernel("SceKernelLeakCheckHeapList",
					      SceDeci4pSDfMgrForKernel_35238B8A(),
					      LEAKCHECK_LOG_SIZE, NULL);
	if (uid < 0) {
		SceDebugForDriver_391B74B7("Alloc Memory Block failed (0x%08X)\n", uid);
		SCE_DBG_ASSERT_FAIL(722);
		return uid;
	}
	sceKernelGetMemBlockBase(uid, (void **)&s_heapLogBuffer);
	s_heapLogMemblock = uid;
	s_heapLogLock = 0;
	return 0;
}

static __attribute__((noinline)) int LeakCheckResourceInit(void)
{
	int result;

	s_resourceMemblock = sceKernelAllocMemBlockForKernel("SceResourceInfo",
							     SceDeci4pSDfMgrForKernel_35238B8A(),
							     LEAKCHECK_LOG_SIZE, NULL);
	result = sceKernelGetMemBlockBase(s_resourceMemblock, (void **)&s_resource);
	s_resourceMax = 0x4000;
	return result;
}

static __attribute__((noinline)) int LeakCheckUIDListInit(void)
{
	int result;

	s_uidListBMemblock = sceKernelAllocMemBlockForKernel("SceUIDLogB",
							     SceDeci4pSDfMgrForKernel_35238B8A(),
							     LEAKCHECK_LOG_SIZE, NULL);
	sceKernelGetMemBlockBase(s_uidListBMemblock, (void **)&s_uidListB);
	s_uidListCleared[0] = 0;
	s_uidListCleared[1] = 0;
	s_uidListBMax = 20164;
	s_uidListCMemblock = sceKernelAllocMemBlockForKernel("SceUIDLogC",
							     SceDeci4pSDfMgrForKernel_35238B8A(),
							     LEAKCHECK_LOG_SIZE, NULL);
	sceKernelGetMemBlockBase(s_uidListCMemblock, (void **)&s_uidListC);
	s_uidListCMax = 20164;
	s_uidListLMemblock = sceKernelAllocMemBlockForKernel("SceUIDLogL",
							     SceDeci4pSDfMgrForKernel_35238B8A(),
							     LEAKCHECK_LOG_SIZE, NULL);
	result = sceKernelGetMemBlockBase(s_uidListLMemblock, (void **)&s_uidListL);
	s_uidListLMax = 20164;
	return result;
}

static __attribute__((noinline)) int _GetResourceGUIDInfo(SceUID heap, SceClass *cls, SceUInt32 level,
				LeakCheckGUIDVector *vector)
{
	SceUID *uid;
	SceSize num;
	int count, result;

	for (;;) {
		count = sceGUIDGetUIDVectorByClass(cls, level, NULL, 0, NULL);
		if (count < 0) {
			SceDebugForDriver_391B74B7("sceGUIDGetUIDVectorByClass failed 0x%08x\n", count);
			SCE_DBG_ASSERT_FAIL(2509);
			return count;
		}
		uid = sceKernelAllocHeapMemory(heap, count * sizeof(SceUID));
		result = sceGUIDGetUIDVectorByClass(cls, level, uid, count * sizeof(SceUID), &num);
		if (result < 0) {
			SceDebugForDriver_391B74B7("sceGUIDGetUIDVectorByClass failed 0x%08x\n", result);
			sceKernelFreeHeapMemory(heap, uid);
			SCE_DBG_ASSERT_FAIL(2523);
			return result;
		}
		if (result == (int)num)
			break;
		sceKernelFreeHeapMemory(heap, uid);
	}
	vector->num = result;
	vector->uid = uid;
	return 0;
}

static __attribute__((noinline)) int LeakCheckFreeGUIDInfo(SdbgpSystemResourceSnapshot *snap)
{
	sceKernelFreeHeapMemory(s_leakCheckHeap, snap->guid[0].uid);
	sceKernelFreeHeapMemory(s_leakCheckHeap, snap->guid[1].uid);
	sceKernelFreeHeapMemory(s_leakCheckHeap, snap->guid[2].uid);
	sceKernelFreeHeapMemory(s_leakCheckHeap, snap->guid[3].uid);
	sceKernelFreeHeapMemory(s_leakCheckHeap, snap->guid[4].uid);
	sceKernelFreeHeapMemory(s_leakCheckHeap, snap->guid[5].uid);
	sceKernelFreeHeapMemory(s_leakCheckHeap, snap->guid[6].uid);
	sceKernelFreeHeapMemory(s_leakCheckHeap, snap->guid[8].uid);
	sceKernelFreeHeapMemory(s_leakCheckHeap, snap->guid[9].uid);
	sceKernelFreeHeapMemory(s_leakCheckHeap, snap->guid[10].uid);
	sceKernelFreeHeapMemory(s_leakCheckHeap, snap->guid[11].uid);
	sceKernelFreeHeapMemory(s_leakCheckHeap, snap->guid[12].uid);
	sceKernelFreeHeapMemory(s_leakCheckHeap, snap->guid[13].uid);
	sceKernelFreeHeapMemory(s_leakCheckHeap, snap->guid[14].uid);
	sceKernelFreeHeapMemory(s_leakCheckHeap, snap->guid[15].uid);
	sceKernelFreeHeapMemory(s_leakCheckHeap, snap->guid[16].uid);
	sceKernelFreeHeapMemory(s_leakCheckHeap, snap->guid[17].uid);
	sceKernelFreeHeapMemory(s_leakCheckHeap, snap->guid[18].uid);
	return sceKernelFreeHeapMemory(s_leakCheckHeap, snap->guid[19].uid);
}

int sceKernelHeapLoggingStart(void)
{
	SCE_DBG_PRINTF_LEVEL(1, 11, 696, "()\n");
	if (s_heapLog == NULL) {
		SceDeci4pSDfMgrForKernel_8A3B06F2(s_heapLogBuffer, 0, LEAKCHECK_LOG_SIZE);
		s_heapLog = s_heapLogBuffer;
	}
	return SceSysmemForKernel_42834422(&s_heapLogHooks);
}

static inline __attribute__((always_inline)) const char *LeakCheckUIDAttrName(SceUInt32 attr)
{
	if (attr & LEAKCHECK_UID_ATTR_AO)
		return "AO";
	if (attr & LEAKCHECK_UID_ATTR_AC)
		return "AC";
	if (attr & LEAKCHECK_UID_ATTR_ZO)
		return "ZO";
	if (attr & LEAKCHECK_UID_ATTR_ZC)
		return "ZC";
	return "??";
}

/*
 * The UID list: the kernel's UIDs at the first pass, then at each later one the
 * UIDs new since it, kept until they go.
 */
static __attribute__((noinline)) void LeakCheckUIDList(void)
{
	SceKernelUIDEntryInfo *entries;
	LeakCheckUIDListEntry *entry;
	const char *kind;
	SceSize max, num, i, j;
	uint32_t generation;
	int result;

	if (s_uidListGeneration != 0) {
		entries = s_uidListC;
		max = s_uidListCMax;
	} else {
		entries = s_uidListB;
		max = s_uidListBMax;
	}
	result = SceSysmemForKernel_3BF0DE65(7, 3, entries, max * sizeof(*entries), &num);
	if (result < 0)
		return;
	if (s_uidListGeneration == 0) {
		s_uidListBCount = num;
		return;
	}
	for (i = 0; i < num; i++) {
		for (j = 0; j < s_uidListBCount; j++)
			if (s_uidListB[j].uid == entries[i].uid)
				break;
		if (s_uidListBCount != 0 && j < s_uidListBCount)
			continue;
		for (j = 0; j < s_uidListLCount; j++)
			if (s_uidListL[j].uid != -1 && s_uidListL[j].uid == entries[i].uid)
				break;
		if (j < s_uidListLCount)
			continue;
		for (j = 0; j < s_uidListLCount; j++)
			if (s_uidListL[j].uid == -1)
				break;
		if (j < s_uidListLCount) {
			entry = &s_uidListL[j];
		} else if (s_uidListLMax <= s_uidListLCount) {
			SceDebugForDriver_391B74B7("Overflow\n");
			continue;
		} else {
			entry = &s_uidListL[++s_uidListLCount];
		}
		entry->generation = s_uidListGeneration;
		entry->cls = entries[i].cls;
		entry->uid = entries[i].uid;
		entry->attr = entries[i].attr;
		strncpy(entry->name, entries[i].name, sizeof(entry->name) - 1);
	}

	/* the ones gone since */
	entry = s_uidListL;
	for (j = 0; j < s_uidListLCount; j++) {
		if (entry->uid == -1)
			continue;
		for (i = 0; i < num; i++)
			if (entries[i].uid == entry->uid)
				break;
		if (i == num)
			entry->uid = -1;
		entry++;
	}

	generation = 0;
	do {
		entry = s_uidListL;
		for (j = 0; j < s_uidListLCount; j++, entry++) {
			if (entry->uid == -1 || entry->generation != generation)
				continue;
			kind = LeakCheckUIDAttrName(entry->attr);
			if (s_uidListShown == 0) {
				SceDebugForDriver_391B74B7("===== UID List =====\n");
				s_uidListShown = 1;
			}
			SceDebugForDriver_391B74B7("%02d:%4d/%4d:0x%08x [%-32s]\n", generation, j, num,
						   entry->uid, entry->name);
			SceDebugForDriver_391B74B7("\tattr 0x%08x:%2s [%s]\n", entry->attr,
						   LeakCheckUIDAttrName(entry->attr),
						   entry->cls->name);
		}
	} while (++generation <= s_uidListGeneration);
	s_uidListShown = 0;
}

/* A UID new since the snapshot compare's first pass. */
static __attribute__((noinline)) void LeakCheckResourceAdd(const char *label, SceUID uid)
{
	LeakCheckResource *resource;
	const char *name;
	SceSize i;

	if (s_resource == NULL)
		return;
	resource = s_resource;
	for (i = 0; i < s_resourceCount; i++, resource++) {
		if (resource->uid == -1)
			goto found;
		if (resource->uid == uid)
			return;
	}
	if (s_resourceCount >= s_resourceMax)
		return;
	resource = &s_resource[s_resourceCount++];
	if (resource == NULL)
		return;
found:
	resource->label = label;
	resource->uid = uid;
	resource->generation = s_snapshotCount;
	name = SceSysmemForDriver_E655852F(uid);
	if (name != NULL) {
		strncpy(resource->name, name, sizeof(resource->name) - 1);
		resource->name[sizeof(resource->name) - 1] = '\0';
	} else {
		resource->name[0] = '\0';
	}
}

static __attribute__((noinline)) void LeakCheckNamedFree(SceUID heap, void *ptr)
{
	LeakCheckNamedChunk *log = sdbgpGetState()->leakCheck.log;
	SceKernelIntrStatus intr;
	unsigned int i;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_namedLogLock);
	for (i = 0; i < LEAKCHECK_LOG_SIZE / sizeof(*log); i++) {
		if (log[i].chunk.ptr == ptr) {
			log[i].chunk.ptr = NULL;
			log[i].chunk.caller = NULL;
			log[i].chunk.heap = 0;
			log[i].chunk.size = 0;
			log[i].chunk.func = 0;
			log[i].chunk.generation = 0;
			SceDeci4pSDfMgrForKernel_8A3B06F2(log[i].name, 0, sizeof(log[i].name));
			break;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_namedLogLock, intr);
}

static __attribute__((noinline)) void LeakCheckNamedAlloc(SceUID heap, void *ptr, SceSize size, const void *caller,
				SceUInt32 func)
{
	SdbgpState *state = sdbgpGetState();
	LeakCheckNamedChunk *log;
	SceKernelIntrStatus intr;
	const char *name;
	unsigned int i;

	if (state->leakCheck.log == NULL)
		return;
	log = state->leakCheck.log;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_namedLogLock);
	for (i = 0; i < LEAKCHECK_LOG_SIZE / sizeof(*log); i++) {
		if (log[i].chunk.ptr == NULL) {
			log[i].chunk.size = size;
			log[i].chunk.caller = caller;
			log[i].chunk.heap = heap;
			log[i].chunk.ptr = ptr;
			log[i].chunk.generation = state->leakCheck.generation;
			log[i].chunk.func = func;
			name = SceSysmemForDriver_E655852F(heap);
			if (name != NULL)
				SceDeci4pSDfMgrForKernel_5996E2DB(log[i].name, name,
								  sizeof(log[i].name) - 1);
			break;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_namedLogLock, intr);
}

static __attribute__((noinline)) const char *LeakCheckHeapFuncName(SceUInt32 func)
{
	unsigned int i;

	for (i = 0; i < sizeof(s_heapFuncs) / sizeof(s_heapFuncs[0]); i++)
		if (s_heapFuncs[i].func == func)
			return s_heapFuncs[i].name;
	snprintf(s_unknownFuncName, sizeof(s_unknownFuncName), "Unknown_0x%08x", func);
	return s_unknownFuncName;
}

/* The heaps the leak check leaves out: the kernel's own long-lived ones. */
static inline __attribute__((always_inline)) int LeakCheckIsIgnoredHeap(const char *name)
{
	return strncmp("SceIoVfsHeap", name, 12) == 0 ||
	       strncmp("SceExfatfsCommon", name, 16) == 0 ||
	       strncmp("SceKernelModulemgrTool", name, 22) == 0 ||
	       (strncmp("unique: SceKernelKernel", name, 23) != 0 &&
		strncmp("kpls: SceKernelKernel", name, 21) != 0 &&
		(strncmp("unique:", name, 7) == 0 || strncmp("kpls:", name, 5) == 0));
}

/* The chunks still allocated: all of them, or those of one generation. */
int chunkAll(int all, uint32_t generation)
{
	LeakCheckChunk *chunk;
	const char *name;
	void *object;
	void *textBase;
	unsigned int i;

	SCE_DBG_PRINTF_LEVEL(1, 11, 761, "(%d)\n", generation);
	for (i = 0; i < LEAKCHECK_LOG_SIZE / sizeof(*chunk); i++) {
		chunk = &s_heapLog[i];
		if (chunk->ptr == NULL || (all == 0 && chunk->generation != generation))
			continue;
		if (s_chunkHeaderShown == 0) {
			SceDebugForDriver_391B74B7("\t[ Leaked heap check ]\n");
			SceDebugForDriver_391B74B7("\t  index:gen-       ptr         lr        nbytes     heapname(id) : <allocated by>\n");
			s_chunkHeaderShown = 1;
		}
		if ((chunk->heap & 1) == 0) {
			SceDebugForDriver_391B74B7("\t  0x%04x:%04x, %p, %p, 0x%08x, (pHeap:0x%08x) :  <%s>\n",
						   i, chunk->generation, chunk->ptr, chunk->caller,
						   chunk->size, chunk->heap,
						   LeakCheckHeapFuncName(chunk->func));
			continue;
		}
		if (sceGUIDGetObject(chunk->heap, &object) != 0)
			continue;
		{
			char moduleName[0x1c] = { 0 };

			name = SceSysmemForDriver_E655852F(s_heapLog[i].heap);
			if (LeakCheckIsIgnoredHeap(name))
				continue;
			SceDebugForDriver_391B74B7("\t  0x%04x:%04x, %p, %p, 0x%08x, [%s](0x%08x) :  <%s>\n",
						   i, s_heapLog[i].generation, s_heapLog[i].ptr,
						   s_heapLog[i].caller, s_heapLog[i].size, name,
						   s_heapLog[i].heap,
						   LeakCheckHeapFuncName(s_heapLog[i].func));
			if (s_showModule == 0)
				continue;
			if (SceModulemgrForKernel_5564A860(SDBGP_KERNEL_PROCESS_ID,
							   (SceUInt32)s_heapLog[i].caller, NULL,
							   &textBase, moduleName) < 0) {
				SceDebugForDriver_391B74B7("\t\tnomodule\n");
				continue;
			}
			SceDebugForDriver_391B74B7("\t\t%s+0x%08x\n", moduleName,
						   (uintptr_t)s_heapLog[i].caller - (uintptr_t)textBase);
		}
	}
	s_chunkHeaderShown = 0;
	return 0;
}

static __attribute__((noinline)) int LeakCheckHeapCheck(void)
{
	uint32_t generation;
	int result = 0;

	if (s_heapLog == NULL)
		return result;
	if (s_heapCheckByGeneration != 0) {
		generation = 0;
		do
			result = chunkAll(0, generation++);
		while (s_heapGeneration >= generation);
	} else {
		result = chunkAll(1, 0);
	}
	if (s_heapLogKeep == 0) {
		s_heapLog = NULL;
		s_heapLogging = 0;
	}
	return result;
}

static __attribute__((noinline)) int LeakCheckShowPartitionHeader(void)
{
	int result;

	SceDebugForDriver_391B74B7("\n");
	SceDebugForDriver_391B74B7("\tPAR:[name----------------------------] base------+vsize-----  UsedSize--  FreeSize--\n");
	SceDebugForDriver_391B74B7("\t   :nBlock----(max-------)  ");
	SceDebugForDriver_391B74B7("L2--------(max-------)  ");
	result = SceDebugForDriver_391B74B7("VirPage---(max-------)\n");
	s_partitionHeaderShown++;
	return result;
}

static __attribute__((noinline)) int LeakCheckShowPhyMemPartHeader(void)
{
	int result;

	SceDebugForDriver_391B74B7("\n");
	result = SceDebugForDriver_391B74B7("\tPMP:[name----------------------------] pbase-----+psize-----  freeSize--(MinFreeSz-)\n");
	s_phyMemPartHeaderShown++;
	return result;
}

/* The leak check state's three memblocks. */
static __attribute__((noinline)) int LeakCheckUIDInit(SdbgpState *state)
{
	SdbgpLeakCheck *leakCheck = &state->leakCheck;
	SceUID uid;
	int result;

	uid = sceKernelAllocMemBlockForKernel("SceDeci4pSdbgpLCBase",
					      SceDeci4pSDfMgrForKernel_35238B8A(),
					      LEAKCHECK_LOG_SIZE, NULL);
	if (uid < 0)
		return uid;
	leakCheck->base.memblock = uid;
	leakCheck->base.num = LEAKCHECK_LOG_SIZE / sizeof(SceKernelUIDEntryInfo);
	leakCheck->base.mode = LEAKCHECK_MODE_UID;
	result = sceKernelGetMemBlockBase(uid, (void **)&leakCheck->base.entries);
	if (result < 0)
		return result;
	SceDeci4pSDfMgrForKernel_8A3B06F2(leakCheck->base.entries, 0, LEAKCHECK_LOG_SIZE);

	uid = sceKernelAllocMemBlockForKernel("SceDeci4pSdbgpLCCurr",
					      SceDeci4pSDfMgrForKernel_35238B8A(),
					      LEAKCHECK_LOG_SIZE, NULL);
	if (uid < 0)
		return uid;
	leakCheck->current.memblock = uid;
	leakCheck->current.num = LEAKCHECK_LOG_SIZE / sizeof(SceKernelUIDEntryInfo);
	result = sceKernelGetMemBlockBase(uid, (void **)&leakCheck->current.entries);
	if (result < 0)
		return result;
	SceDeci4pSDfMgrForKernel_8A3B06F2(leakCheck->current.entries, 0, LEAKCHECK_LOG_SIZE);

	uid = sceKernelAllocMemBlockForKernel("SceDeci4pSdbgpLCLog",
					      SceDeci4pSDfMgrForKernel_35238B8A(),
					      LEAKCHECK_LOG_SIZE, NULL);
	if (uid < 0)
		return uid;
	leakCheck->logMemblock = uid;
	leakCheck->logMax = 0x4000;
	result = sceKernelGetMemBlockBase(uid, &leakCheck->log);
	if (result < 0)
		return result;
	SceDeci4pSDfMgrForKernel_8A3B06F2(leakCheck->log, 0, LEAKCHECK_LOG_SIZE);
	return 0;
}

static __attribute__((noinline)) void LeakCheckThreadmgrInit(void)
{
	s_threadmgr = SceThreadmgrForKernel_19440C83();
	s_threadmgrReady++;
}

void LeakCheckInit(void)
{
	SceKernelHeapCreateOpt opt;
	SceUID heap;
	int result;

	opt.size = sizeof(opt);
	opt.flags = HEAP_AUTO_EXTEND | HEAP_HAS_MEMTYPE;
	opt.memtype = SceDeci4pSDfMgrForKernel_35238B8A();
	heap = sceKernelCreateHeap("SceKernelLeakCheck", 0x10000, &opt);
	if (heap < 0) {
		SceDebugForDriver_391B74B7("CreateHeap failed (0x%08X)\n", heap);
		SCE_DBG_ASSERT_FAIL(2847);
	}
	s_leakCheckHeap = heap;
	if (s_threadmgrReady == 0)
		LeakCheckThreadmgrInit();
	result = LeakCheckHeapInit();
	if (result < 0) {
		SceDebugForDriver_391B74B7("LeakCheckHeapInit failed 0x%08X\n", result);
		SCE_DBG_ASSERT_FAIL(2858);
	}
}

/* The heap log's hooks: the heaps filtered by LeakCheckHeapFilerAdd only. */
static __attribute__((noinline)) void LeakCheckHeapFree(SceUID heap, void *ptr)
{
	SceKernelIntrStatus intr;
	unsigned int i;

	if (s_heapFilterCount != 0) {
		for (i = 0; i < s_heapFilterCount; i++)
			if (s_heapFilter[i] == heap)
				break;
		if (i == s_heapFilterCount)
			return;
	}
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_heapLogLock);
	for (i = 0; i < LEAKCHECK_LOG_SIZE / sizeof(*s_heapLog); i++) {
		if (s_heapLog[i].ptr == ptr) {
			s_heapLog[i].ptr = NULL;
			s_heapLog[i].caller = NULL;
			s_heapLog[i].heap = 0;
			s_heapLog[i].size = 0;
			s_heapLog[i].func = 0;
			s_heapLog[i].generation = 0;
			break;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_heapLogLock, intr);
}

static __attribute__((noinline)) void LeakCheckHeapAlloc(SceUID heap, void *ptr, SceSize size, const void *caller,
			       SceUInt32 func)
{
	SceKernelIntrStatus intr;
	unsigned int i;

	if (s_heapFilterCount != 0) {
		for (i = 0; i < s_heapFilterCount; i++)
			if (s_heapFilter[i] == heap)
				break;
		if (i == s_heapFilterCount)
			return;
	}
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_heapLogLock);
	for (i = 0; i < LEAKCHECK_LOG_SIZE / sizeof(*s_heapLog); i++) {
		if (s_heapLog[i].ptr == NULL) {
			s_heapLog[i].heap = heap;
			s_heapLog[i].ptr = ptr;
			s_heapLog[i].size = size;
			s_heapLog[i].caller = caller;
			s_heapLog[i].func = func;
			s_heapLog[i].generation = s_heapGeneration;
			break;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_heapLogLock, intr);
}

/* The counters a UID entry heap and a fixed heap share. */
static inline int LeakCheckShowHeapCount(const SceKernelUIDEntryHeapInfo *old,
				  const SceKernelUIDEntryHeapInfo *new)
{
	SceDebugForDriver_391B74B7("\t%s: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n", "OLD",
				   old->itemSize, old->totalSize, old->perPage, old->freeCount,
				   old->peakCount);
	SceDebugForDriver_391B74B7("\tNEW: ");
	if (old->itemSize == new->itemSize)
		SceDebugForDriver_391B74B7("           ");
	else
		SceDebugForDriver_391B74B7("0x%08x ", new->itemSize);
	if (old->totalSize == new->totalSize)
		SceDebugForDriver_391B74B7(":           ");
	else
		SceDebugForDriver_391B74B7(":0x%08x ", new->totalSize);
	if (old->perPage == new->perPage)
		SceDebugForDriver_391B74B7(":           ");
	else
		SceDebugForDriver_391B74B7(":0x%08x ", new->perPage);
	if (old->freeCount == new->freeCount)
		SceDebugForDriver_391B74B7(":           ");
	else
		SceDebugForDriver_391B74B7(":0x%08x ", new->freeCount);
	if (old->peakCount == new->peakCount)
		return SceDebugForDriver_391B74B7(":         \n");
	return SceDebugForDriver_391B74B7(":0x%08x\n", new->peakCount);
}

/*
 * The UIDs of one class in both snapshots, in UID order: those gone (-) and
 * those new (+). Like Sony's, a new UID is recorded under the old list's index.
 */
static __attribute__((noinline)) void LeakCheckCompareGUID(const char *label, const LeakCheckGUIDVector *old,
				 const LeakCheckGUIDVector *new)
{
	LeakCheckResource *resource;
	SceSize i, j, k;
	SceUID uid;

	if (old->num == new->num) {
		for (i = 0, j = 0; i < old->num; i++, j++) {
			if (j >= old->num)
				goto changed;
			if (old->uid[i] != new->uid[j]) {
				for (k = j + 1; k < old->num; k++)
					if (new->uid[k] == old->uid[i])
						break;
				if (k >= old->num || j < k)
					goto changed;
				j = k;
			}
		}
		if (i <= j)
			return;
	}
changed:
	SceDebugForDriver_391B74B7("\t%s\n", label);
	SceDebugForDriver_391B74B7("\t  prev %d, after %d\n", old->num, new->num);
	j = 0;
	for (i = 0; i < old->num; i++) {
		uid = old->uid[i];
		if (j < new->num) {
			if (uid == new->uid[j]) {
				j++;
				continue;
			}
			for (k = j + 1; k < new->num; k++)
				if (new->uid[k] == uid)
					break;
			if (k < new->num) {
				for (; j < k; j++) {
					SceDebugForDriver_391B74B7("\t  (+) : 0x%08x, [%s]\n", new->uid[j],
								   SceSysmemForDriver_E655852F(new->uid[j]));
					LeakCheckResourceAdd(label, new->uid[i]);
				}
				j = k + 1;
				continue;
			}
		}
		SceDebugForDriver_391B74B7("\t  (-) : 0x%08x\n", uid);
		if (s_resource != NULL && s_resourceCount != 0) {
			resource = s_resource;
			for (k = 0; k < s_resourceCount; k++, resource++) {
				if (resource->uid == old->uid[i]) {
					resource->uid = -1;
					break;
				}
			}
		}
	}
	if (j > new->num) {
		SceDebugForDriver_391B74B7("\n");
		return;
	}
	for (; j < new->num; j++) {
		SceDebugForDriver_391B74B7("\t  (+) : 0x%08x, [%s]\n", new->uid[j],
					   SceSysmemForDriver_E655852F(new->uid[j]));
		LeakCheckResourceAdd(label, new->uid[i]);
	}
	SceDebugForDriver_391B74B7("\n");
}

static __attribute__((noinline)) void LeakCheckCompareFixedHeap(const char *label, const SceKernelFixedHeapInfo *old,
				      const SceKernelFixedHeapInfo *new)
{
	if (old->size == new->size && old->itemSize == new->itemSize &&
	    old->totalSize == new->totalSize && old->perPage == new->perPage &&
	    old->freeCount == new->freeCount && old->peakCount == new->peakCount)
		return;
	SceDebugForDriver_391B74B7("\t%s\n", label);
	SceDebugForDriver_391B74B7("\tFIX: size------  tolHeapSiz  toItemCnt-  toFreeCnt-  usedMax---\n");
	LeakCheckShowHeapCount((const SceKernelUIDEntryHeapInfo *)old,
			       (const SceKernelUIDEntryHeapInfo *)new);
	SceDebugForDriver_391B74B7("\n");
}

/* The fixed heaps of an object heap in both snapshots. */
static __attribute__((noinline)) int LeakCheckCompareProcessFixedHeap(int slot, const SdbgpSystemResourceSnapshot *oldSnap,
					    const ObjectHeapInfo *oldHeap, int oldProc,
					    const SdbgpSystemResourceSnapshot *newSnap,
					    const ObjectHeapInfo *newHeap, int newProc)
{
	const SceKernelFixedHeapInfo *newInfo = newSnap->fixedHeap[newProc][slot];
	const SceKernelFixedHeapInfo *oldInfo;
	unsigned int i, j;

	for (i = 0; i < newHeap->count; i++, newInfo++) {
		if (newHeap->sizeClasses[i].size == 0 ||
		    newHeap->sizeClasses[i].size == (SceSize)-1)
			continue;
		oldInfo = oldSnap->fixedHeap[oldProc][slot];
		for (j = 0; j < oldHeap->count; j++, oldInfo++)
			if (oldHeap->sizeClasses[j].heap == newHeap->sizeClasses[i].heap)
				break;
		if (j < oldHeap->count) {
			LeakCheckCompareFixedHeap("FIX", oldInfo, newInfo);
			continue;
		}
		SceDebugForDriver_391B74B7("\tFIX: size------  tolHeapSiz  toItemCnt-  toFreeCnt-  usedMax---\n");
		SceDebugForDriver_391B74B7("\t%s: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n", "NEW",
					   newInfo->itemSize, newInfo->totalSize, newInfo->perPage,
					   newInfo->freeCount, newInfo->peakCount);
	}
	return 0;
}

static inline __attribute__((always_inline)) void LeakCheckComparePartitions(const AddressSpaceInfo *oldAs,
				       const AddressSpaceInfo *newAs)
{
	const PartitionInfo *part, *oldPart;
	SceSize k, m;

	for (k = 0; k < newAs->partitionCount; k++) {
		part = &newAs->partition[k];
		for (m = 0; m < s_partitionSeenCount; m++)
			if (s_partitionSeen[m] == part->partition)
				break;
		if (s_partitionSeenCount != 0 && m < s_partitionSeenCount)
			continue;
		s_partitionSeen[s_partitionSeenCount++] = part->partition;
		oldPart = &oldAs->partition[k];
		if (oldAs->partitionCount == 0 || oldPart->partition != part->partition) {
			if (s_partitionHeaderShown == 0)
				LeakCheckShowPartitionHeader();
			LeakCheckShowPartition("NEW", part);
			continue;
		}
		if (oldPart->vbase == part->vbase && oldPart->vsize == part->vsize &&
		    oldPart->freeSize == part->freeSize && oldPart->blockCount == part->blockCount &&
		    oldPart->blockCountMax == part->blockCountMax &&
		    oldPart->l2Count == part->l2Count && oldPart->l2CountMax == part->l2CountMax &&
		    oldPart->virPageCount == part->virPageCount &&
		    oldPart->virPageCountMax == part->virPageCountMax)
			continue;
		if (s_partitionHeaderShown == 0)
			LeakCheckShowPartitionHeader();
		LeakCheckShowPartition("OLD", oldPart);
		SceDebugForDriver_391B74B7("\tNEW:[%-32s] ", part->name);
		if (oldPart->vbase == part->vbase && oldPart->vsize == part->vsize)
			SceDebugForDriver_391B74B7("          +          ");
		else
			SceDebugForDriver_391B74B7("0x%08x+0x%08x ", part->vbase, part->vsize);
		if (oldPart->freeSize == part->freeSize)
			SceDebugForDriver_391B74B7(":                    \n");
		else
			SceDebugForDriver_391B74B7(":0x%08x  0x%08x\n", part->vsize - part->freeSize,
						   part->freeSize);
		SceDebugForDriver_391B74B7("\t   :");
		if (oldPart->blockCount == part->blockCount)
			SceDebugForDriver_391B74B7("          ");
		else
			SceDebugForDriver_391B74B7("0x%08x", part->blockCount);
		if (oldPart->blockCountMax == part->blockCountMax)
			SceDebugForDriver_391B74B7("(          ) ");
		else
			SceDebugForDriver_391B74B7("(0x%08x) ", part->blockCountMax);
		if (oldPart->l2Count == part->l2Count)
			SceDebugForDriver_391B74B7(":          ");
		else
			SceDebugForDriver_391B74B7(":0x%08x", part->l2Count);
		if (oldPart->l2CountMax == part->l2CountMax)
			SceDebugForDriver_391B74B7("(          ) ");
		else
			SceDebugForDriver_391B74B7("(0x%08x) ", part->l2CountMax);
		if (oldPart->virPageCount == part->virPageCount)
			SceDebugForDriver_391B74B7(":          ");
		else
			SceDebugForDriver_391B74B7(":0x%08x", part->virPageCount);
		if (oldPart->virPageCountMax == part->virPageCountMax)
			SceDebugForDriver_391B74B7("(          )\n");
		else
			SceDebugForDriver_391B74B7("(0x%08x)\n", part->virPageCountMax);
	}
}

static inline __attribute__((always_inline)) void LeakCheckComparePhyMemParts(const SdbgpSystemResourceSnapshot *old, int oldProc,
					const SdbgpSystemResourceSnapshot *new, int newProc)
{
	const AddressSpaceInfo *oldAs = &old->addressSpace[oldProc];
	const AddressSpaceInfo *newAs = &new->addressSpace[newProc];
	const SceKernelPhyMemPartInfoForDebugger *info, *oldInfo;
	void *part;
	SceSize k, m;

	for (k = 0; k < newAs->phyMemPartCount; k++) {
		part = newAs->phyMemPart[k];
		info = &new->phyMemPart[newProc][k];
		if (part == NULL)
			continue;
		for (m = 0; m < s_phyMemPartSeenCount; m++)
			if (s_phyMemPartSeen[m] == part)
				break;
		if (s_phyMemPartSeenCount != 0 && m < s_phyMemPartSeenCount)
			continue;
		s_phyMemPartSeen[s_phyMemPartSeenCount++] = part;
		for (m = 0; m < oldAs->phyMemPartCount; m++)
			if (oldAs->phyMemPart[m] == part)
				break;
		if (m == oldAs->phyMemPartCount) {
			if (s_phyMemPartHeaderShown == 0)
				LeakCheckShowPhyMemPartHeader();
			SceDebugForDriver_391B74B7("\t%s:[%-32s] 0x%08x+0x%08x  0x%08x(0x%08x)\n", "NEW",
						   info->name, info->segments[0].pbase,
						   info->segments[0].psize, info->freeSize,
						   info->freeSizeMin);
			continue;
		}
		oldInfo = &old->phyMemPart[oldProc][m];
		if (oldInfo->segments[0].pbase == info->segments[0].pbase &&
		    oldInfo->segments[0].psize == info->segments[0].psize &&
		    oldInfo->freeSize == info->freeSize && oldInfo->freeSizeMin == info->freeSizeMin)
			continue;
		if (s_phyMemPartHeaderShown == 0)
			LeakCheckShowPhyMemPartHeader();
		SceDebugForDriver_391B74B7("\t%s:[%-32s] 0x%08x+0x%08x  0x%08x(0x%08x)\n", "OLD",
					   oldInfo->name, oldInfo->segments[0].pbase,
					   oldInfo->segments[0].psize, oldInfo->freeSize,
					   oldInfo->freeSizeMin);
		SceDebugForDriver_391B74B7("\tNEW:[%-32s] ", info->name);
		if (oldInfo->segments[0].pbase == info->segments[0].pbase &&
		    oldInfo->segments[0].psize == info->segments[0].psize)
			SceDebugForDriver_391B74B7("          +           ");
		else
			SceDebugForDriver_391B74B7("0x%08x+0x%08x ", info->segments[0].pbase,
						   info->segments[0].psize);
		if (oldInfo->freeSize == info->freeSize)
			SceDebugForDriver_391B74B7(":         ");
		else
			SceDebugForDriver_391B74B7(":0x%08x ", info->freeSize);
		if (oldInfo->freeSizeMin == info->freeSizeMin)
			SceDebugForDriver_391B74B7(":        \n");
		else
			SceDebugForDriver_391B74B7(":0x%08x\n", info->freeSizeMin);
	}
}

/* Two snapshots compared: per process, then the kernel's UIDs and heaps. */
static __attribute__((noinline)) void LeakCheckCompare(SdbgpSystemResourceSnapshot *old, SdbgpSystemResourceSnapshot *new)
{
	const SceKernelProcessInfo *proc;
	const HeapInfo *heap;
	const LeakCheckResource *resource;
	const SdbgpCpuMaskEntry *entry, *other;
	void *textBase;
	SceSize k, m;
	uint32_t generation;
	int i, j, ret;

	SceDebugForDriver_391B74B7("=========================================\n");
	SceDebugForDriver_391B74B7("----- Kernel resource summary check -----\n");
	s_phyMemPartSeenCount = 0;
	if (old->processCount != new->processCount) {
		SceDebugForDriver_391B74B7("\n");
		SceDebugForDriver_391B74B7("SYS:nprocess\n");
		SceDebugForDriver_391B74B7("OLD:%d\n", old->processCount);
		SceDebugForDriver_391B74B7("NEW:%d\n", new->processCount);
	}
	LeakCheckShowLostProcess("NEW", new, old);
	LeakCheckShowLostProcess("OLD", old, new);
	for (i = 0; i < new->processCount; i++) {
		proc = &new->process[i];
		for (j = 0; j < old->processCount; j++)
			if (old->process[j].pid == proc->pid)
				break;
		if (j == old->processCount)
			continue;
		SceDebugForDriver_391B74B7("%s:[%-32s] 0x%08x  0x%08x\n", "PRO", proc->name, proc->pid,
					   proc->addressSpace);
		LeakCheckComparePartitions(&old->addressSpace[j], &new->addressSpace[i]);
		LeakCheckComparePhyMemParts(old, j, new, i);
		LeakCheckCompareProcessFixedHeap(0, old, &old->objectHeap[j][0], j,
						 new, &new->objectHeap[i][0], i);
		LeakCheckCompareProcessHeap(0, old, &old->objectHeap[j][0], j,
					    new, &new->objectHeap[i][0], i);
		LeakCheckCompareProcessFixedHeap(1, old, &old->objectHeap[j][1], j,
						 new, &new->objectHeap[i][1], i);
		LeakCheckCompareProcessHeap(1, old, &old->objectHeap[j][1], j,
					    new, &new->objectHeap[i][1], i);
		SceDebugForDriver_391B74B7("\tUID: size------  tolHeapSiz  toItemCnt-  toFreeCnt-  usedMax---\n");
		LeakCheckShowHeapCount(&old->uidEntryHeap[j], &new->uidEntryHeap[i]);
		SceDebugForDriver_391B74B7("\n");
		if (proc->pid != SDBGP_KERNEL_PROCESS_ID ||
		    (old->nameHeap.heaps[0].count == new->nameHeap.heaps[0].count &&
		     old->nameHeap.heaps[0].peak == new->nameHeap.heaps[0].peak))
			continue;
		SceDebugForDriver_391B74B7("\tNH-:Max------ :nUsedNow--(Max-------)\n");
		SceDebugForDriver_391B74B7("\t%s:0x%08x(0x%08x)\n", "OLD", old->nameHeap.heaps[0].count,
					   old->nameHeap.heaps[0].peak);
		SceDebugForDriver_391B74B7("\tNEW: ");
		if (old->nameHeap.heaps[0].count == new->nameHeap.heaps[0].count)
			SceDebugForDriver_391B74B7(":          ");
		else
			SceDebugForDriver_391B74B7(":0x%08x ", new->nameHeap.heaps[0].count);
		if (old->nameHeap.heaps[0].peak == new->nameHeap.heaps[0].peak)
			SceDebugForDriver_391B74B7("(         )\n");
		else
			SceDebugForDriver_391B74B7("(0x%08x)\n", new->nameHeap.heaps[0].peak);
		SceDebugForDriver_391B74B7("\n");
	}

	SceDebugForDriver_391B74B7("----- Kernel resource leak check -----\n");
	LeakCheckCompareGUID("[ callback ]", &old->guid[0], &new->guid[0]);
	LeakCheckCompareGUID("[ thread ]", &old->guid[1], &new->guid[1]);
	LeakCheckCompareGUID("[ fast mutex ]", &old->guid[2], &new->guid[2]);
	LeakCheckCompareGUID("[ semaphore ]", &old->guid[3], &new->guid[3]);
	LeakCheckCompareGUID("[ event flag ]", &old->guid[4], &new->guid[4]);
	LeakCheckCompareGUID("[ mutex ]", &old->guid[5], &new->guid[5]);
	LeakCheckCompareGUID("[ cond ]", &old->guid[6], &new->guid[6]);
	LeakCheckCompareGUID("[ alarm ]", &old->guid[8], &new->guid[8]);
	LeakCheckCompareGUID("[ timer ]", &old->guid[9], &new->guid[9]);
	LeakCheckCompareGUID("[ msg pipe ]", &old->guid[10], &new->guid[10]);
	LeakCheckCompareGUID("[ lw mutex ]", &old->guid[11], &new->guid[11]);
	LeakCheckCompareGUID("[ lw cond ]", &old->guid[12], &new->guid[12]);
	LeakCheckCompareGUID("[ rw lock ]", &old->guid[13], &new->guid[13]);
	LeakCheckCompareGUID("[ simple event ]", &old->guid[14], &new->guid[14]);
	LeakCheckCompareGUID("[ process ]", &old->guid[15], &new->guid[15]);
	LeakCheckCompareGUID("[ address space ]", &old->guid[16], &new->guid[16]);
	LeakCheckCompareGUID("[ partition ]", &old->guid[17], &new->guid[17]);
	LeakCheckCompareGUID("[ memory block ]", &old->guid[18], &new->guid[18]);
	LeakCheckCompareGUID("[ heap ]", &old->guid[19], &new->guid[19]);
	LeakCheckCompareFixedHeap("[ Fixed Heap  32B ]", &old->kernelFixedHeap[0], &new->kernelFixedHeap[0]);
	LeakCheckCompareFixedHeap("[ Fixed Heap  64B ]", &old->kernelFixedHeap[1], &new->kernelFixedHeap[1]);
	LeakCheckCompareFixedHeap("[ Fixed Heap 128B ]", &old->kernelFixedHeap[2], &new->kernelFixedHeap[2]);
	LeakCheckCompareFixedHeap("[ Fixed Heap 256B ]", &old->kernelFixedHeap[3], &new->kernelFixedHeap[3]);
	LeakCheckCompareFixedHeap("[ Fixed Heap 512B ]", &old->kernelFixedHeap[4], &new->kernelFixedHeap[4]);
	LeakCheckCompareFixedHeap("[ GUID Entry Heap ]", &old->kernelFixedHeap[5], &new->kernelFixedHeap[5]);
	LeakCheckCompareHeap("[ Kernel Heap (cached) ]", &old->kernelHeap[0].info, &new->kernelHeap[0].info);
	LeakCheckCompareHeap("[ Kernel Heap (uncach) ]", &old->kernelHeap[1].info, &new->kernelHeap[1].info);

	SceDebugForDriver_391B74B7("\t[ Heap (total usage changed) ]\n");
	for (i = 0, j = 0; i < old->heapCount; i++) {
		heap = &old->heap[i];
		if (heap->uid == s_leakCheckHeap || new->heapCount <= j)
			continue;
		for (m = j; (int)m < new->heapCount; m++)
			if (new->heap[m].uid == heap->uid)
				break;
		if ((int)m == new->heapCount)
			continue;
		j = m + 1;
		if (heap->uordblks == new->heap[m].uordblks)
			continue;
		SceDebugForDriver_391B74B7("\t  heapid 0x%08x, before 0x%08x, after 0x%08x, increased %05d: [%s]\n",
					   heap->uid, heap->uordblks, new->heap[m].uordblks,
					   new->heap[m].uordblks - heap->uordblks,
					   SceSysmemForDriver_E655852F(heap->uid));
	}
	SceDebugForDriver_391B74B7("\n");
	LeakCheckHeapCheck();

	if (s_resource != NULL) {
		SceDebugForDriver_391B74B7("\n");
		SceDebugForDriver_391B74B7("\tUID Resource:%d/%d\n", s_snapshotCount, s_resourceCount);
		SceDebugForDriver_391B74B7("\tgen-:idx-: [                ] uid------- [name                            ]\n");
		generation = 0;
		do {
			resource = s_resource;
			for (k = 0; k < s_resourceCount; k++, resource++) {
				if (resource->uid == -1 || resource->generation != generation)
					continue;
				SceDebugForDriver_391B74B7("\t%4d:%4d: [%-16s] 0x%08x [%-32s]\n", generation,
							   k, resource->label, resource->uid,
							   resource->name);
			}
		} while (++generation <= s_snapshotCount);
	}

	if (s_maskLogCount == 0)
		return;
	SceDebugForDriver_391B74B7("\n");
	SceDebugForDriver_391B74B7("\t[ CPU Intr Mask ]\n");
	SceDebugForDriver_391B74B7("\tusec----  C  Suspend LR  Resume LR-  Module+offset\n");
	for (k = 0; k < s_maskLogCount; k++) {
		entry = &s_maskLog[k];
		if (entry->duration == 0)
			continue;
		/* a section logged again keeps its longest run */
		for (m = k + 1; m < s_maskLogCount; m++) {
			other = &s_maskLog[m];
			if (other->duration == 0 || other->lockedCaller != entry->lockedCaller ||
			    other->resumeCaller != entry->resumeCaller)
				continue;
			if (other->duration > entry->duration)
				break;
			s_maskLog[m].duration = 0;
		}
		if (m < s_maskLogCount)
			continue;
		char moduleName[0x1c] = { 0 };

		ret = SceModulemgrForKernel_5564A860(SDBGP_KERNEL_PROCESS_ID, entry->lockedCaller,
						     NULL, &textBase, moduleName);
		SceDebugForDriver_391B74B7("\t%8d  %d  0x%08X  0x%08X  ", entry->duration, entry->core,
					   entry->lockedCaller, entry->resumeCaller);
		if (ret < 0)
			SceDebugForDriver_391B74B7("\n");
		else
			SceDebugForDriver_391B74B7("[%-32s]+0x%08x 0x%08x\n", moduleName,
						   entry->lockedCaller - (uintptr_t)textBase,
						   (uintptr_t)entry->resumeCaller - (uintptr_t)textBase);
	}
}

/* Logs only the heap of this name (at most 16). */
int LeakCheckHeapFilerAdd(const char *name)
{
	SceSize i;
	SceUID uid;

	SCE_DBG_PRINTF_LEVEL(1, 11, 547, "([%s]\n", name);
	uid = SceSysmemForKernel_F49C0BE8(name);
	if (uid < 0)
		return uid;
	if (s_heapFilterCount > 15) {
		SCE_DBG_PRINTF_LEVEL(1, 11, 554, "Overflow\n");
		return 0;
	}
	for (i = 0; i < s_heapFilterCount; i++)
		if (s_heapFilter[i] == uid)
			return 0;
	SCE_DBG_PRINTF_LEVEL(1, 11, 563, "add %2d:0x%08x\n", s_heapFilterCount, uid);
	s_heapFilter[s_heapFilterCount++] = uid;
	return 0;
}

int sceSdbgpGetSystemResourceSnapshot(SceUInt32 level, SdbgpSystemResourceSnapshot *snap)
{
	SceKernelProcessInfo *proc;
	AddressSpaceInfo *as;
	ObjectHeap *heap, *heap2;
	SceUID entryHeap;
	SceSize count;
	int i, k, result;

	SCE_DBG_PRINTF_LEVEL(1, 11, 2564, "(%p)\n", snap);
	snap->level = level;
	snap->processCount = SceProcessmgrForDriver_BA0A06A4(5, snap->process, sizeof(snap->process),
							     (SceSize *)&snap->processCount);
	for (i = 0; i < snap->processCount; i++) {
		proc = &snap->process[i];
		as = &snap->addressSpace[i];
		SceDeci4pSDfMgrForKernel_8A3B06F2(as, 0, sizeof(*as));
		as->size = sizeof(*as);
		if (SceSysmemForKernel_C3EF4055(proc->addressSpace, as) < 0)
			continue;
		for (k = 0; k < (int)as->phyMemPartCount; k++) {
			if (as->phyMemPart[k] == NULL)
				continue;
			snap->phyMemPart[i][k].size = sizeof(snap->phyMemPart[i][k]);
			result = sceKernelGetPhyMemPartInfoForDebugger(as->phyMemPart[k],
								       &snap->phyMemPart[i][k]);
			if (result < 0)
				SceDebugForDriver_391B74B7("sceKernelGetPhyMemPartInfo failed 0x%08x\n",
							   result);
		}
		if (proc->pid == SDBGP_KERNEL_PROCESS_ID) {
			heap = SceSysmemForKernel_857408DA();
			snap->objectHeap[i][0].size = sizeof(ObjectHeapInfo);
			SceSysmemForKernel_DBEACE2C(heap, &snap->objectHeap[i][0]);
			LeakCheckGetFixedHeapInfo(snap, i, 0, &snap->objectHeap[i][0]);
			LeakCheckGetHeapInfo(snap, i, 0, &snap->objectHeap[i][0]);
		} else {
			heap = proc->objectHeap[0];
			heap2 = proc->objectHeap[1];
			snap->objectHeap[i][0].size = sizeof(ObjectHeapInfo);
			SceSysmemForKernel_DBEACE2C(heap, &snap->objectHeap[i][0]);
			LeakCheckGetFixedHeapInfo(snap, i, 0, &snap->objectHeap[i][0]);
			LeakCheckGetHeapInfo(snap, i, 0, &snap->objectHeap[i][0]);
			if (heap2 != NULL) {
				snap->objectHeap[i][1].size = sizeof(ObjectHeapInfo);
				SceSysmemForKernel_DBEACE2C(heap2, &snap->objectHeap[i][1]);
				LeakCheckGetFixedHeapInfo(snap, i, 1, &snap->objectHeap[i][1]);
				LeakCheckGetHeapInfo(snap, i, 1, &snap->objectHeap[i][1]);
			}
		}
		if (proc->pid == SDBGP_KERNEL_PROCESS_ID)
			entryHeap = LEAKCHECK_GUID_ENTRY_HEAP;
		else
			entryHeap = proc->entryHeap;
		snap->uidEntryHeap[i].size = sizeof(snap->uidEntryHeap[i]);
		result = SceSysmemForKernel_CCD47B97(entryHeap, &snap->uidEntryHeap[i]);
		if (result < 0) {
			SceDebugForDriver_391B74B7("sceKernelUIDEntryHeapGetInfo failed 0x%08x\n", result);
		} else if (proc->pid == SDBGP_KERNEL_PROCESS_ID) {
			snap->nameHeap.size = sizeof(snap->nameHeap);
			result = SceSysmemForKernel_01194C2E(&snap->nameHeap);
			if (result < 0)
				SceDebugForDriver_391B74B7("sceKernelNameHeapGetInfo failed 0x%08x\n", result);
		}
	}

	if (s_leakCheckInitDone == 0) {
		LeakCheckInit();
		s_leakCheckInitDone = 1;
	}
	if (s_threadmgrReady == 0)
		LeakCheckThreadmgrInit();
	result = _GetResourceGUIDInfo(s_leakCheckHeap, s_threadmgr->callbackClass, level, &snap->guid[0]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2579);
	result = _GetResourceGUIDInfo(s_leakCheckHeap, s_threadmgr->threadClass, level, &snap->guid[1]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2585);
	result = _GetResourceGUIDInfo(s_leakCheckHeap, s_threadmgr->fastMutexClass, level, &snap->guid[2]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2591);
	result = _GetResourceGUIDInfo(s_leakCheckHeap, s_threadmgr->semaphoreClass, level, &snap->guid[3]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2597);
	result = _GetResourceGUIDInfo(s_leakCheckHeap, s_threadmgr->eventFlagClass, level, &snap->guid[4]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2603);
	result = _GetResourceGUIDInfo(s_leakCheckHeap, s_threadmgr->mutexClass, level, &snap->guid[5]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2609);
	result = _GetResourceGUIDInfo(s_leakCheckHeap, s_threadmgr->condClass, level, &snap->guid[6]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2615);
	result = _GetResourceGUIDInfo(s_leakCheckHeap, s_threadmgr->alarmClass, level, &snap->guid[8]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2629);
	result = _GetResourceGUIDInfo(s_leakCheckHeap, s_threadmgr->timerClass, level, &snap->guid[9]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2635);
	result = _GetResourceGUIDInfo(s_leakCheckHeap, s_threadmgr->msgPipeClass, level, &snap->guid[10]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2641);
	result = _GetResourceGUIDInfo(s_leakCheckHeap, s_threadmgr->lwMutexClass, level, &snap->guid[11]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2647);
	result = _GetResourceGUIDInfo(s_leakCheckHeap, s_threadmgr->lwCondClass, level, &snap->guid[12]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2653);
	result = _GetResourceGUIDInfo(s_leakCheckHeap, s_threadmgr->rwLockClass, level, &snap->guid[13]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2659);
	result = _GetResourceGUIDInfo(s_leakCheckHeap, s_threadmgr->simpleEventClass, level,
				      &snap->guid[14]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2665);
	result = _GetResourceGUIDInfo(s_leakCheckHeap, SceProcessmgrForKernel_98AE4BC8(), level,
				      &snap->guid[15]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2671);
	result = _GetResourceGUIDInfo(s_leakCheckHeap, SceSysmemForKernel_99D3BF76(), level,
				      &snap->guid[16]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2695);
	result = _GetResourceGUIDInfo(s_leakCheckHeap, SceSysmemForKernel_5C0FC6BE(), level,
				      &snap->guid[17]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2701);
	result = _GetResourceGUIDInfo(s_leakCheckHeap, SceSysmemForKernel_86681B64(), level,
				      &snap->guid[18]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2707);
	result = _GetResourceGUIDInfo(s_leakCheckHeap, SceSysmemForKernel_7C878E94(), level,
				      &snap->guid[19]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2713);

	for (i = 0; i < LEAKCHECK_HEAP_MAX; i++) {
		snap->heap[i].blockList = snap->heapBlocks[i];
		snap->heap[i].size = sizeof(snap->heap[i]);
		snap->heap[i].maxBlocks = LEAKCHECK_HEAP_BLOCK_MAX;
	}
	result = SceSysmemForKernel_1380E144(7, snap->heap, sizeof(snap->heap), &count);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2732);
	snap->heapCount = count;
	SCE_DBG_PRINTF_LEVEL(2, 11, 2735, "heap num %d\n", count);

	snap->kernelFixedHeap[0].size = sizeof(snap->kernelFixedHeap[0]);
	result = sceKernelGetFixedHeapInfoByPointer(SceSysmemForKernel_3717C322(32),
						    &snap->kernelFixedHeap[0]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2742);
	snap->kernelFixedHeap[1].size = sizeof(snap->kernelFixedHeap[1]);
	result = sceKernelGetFixedHeapInfoByPointer(SceSysmemForKernel_3717C322(64),
						    &snap->kernelFixedHeap[1]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2750);
	snap->kernelFixedHeap[2].size = sizeof(snap->kernelFixedHeap[2]);
	result = sceKernelGetFixedHeapInfoByPointer(SceSysmemForKernel_3717C322(128),
						    &snap->kernelFixedHeap[2]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2757);
	snap->kernelFixedHeap[3].size = sizeof(snap->kernelFixedHeap[3]);
	result = sceKernelGetFixedHeapInfoByPointer(SceSysmemForKernel_3717C322(256),
						    &snap->kernelFixedHeap[3]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2764);
	snap->kernelFixedHeap[4].size = sizeof(snap->kernelFixedHeap[4]);
	result = sceKernelGetFixedHeapInfoByPointer(SceSysmemForKernel_3717C322(512),
						    &snap->kernelFixedHeap[4]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2771);
	snap->kernelFixedHeap[5].size = sizeof(snap->kernelFixedHeap[5]);
	result = sceKernelGetFixedHeapInfo(LEAKCHECK_GUID_ENTRY_HEAP, &snap->kernelFixedHeap[5]);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2779);
	snap->kernelHeap[0].info.size = sizeof(snap->kernelHeap[0].info);
	snap->kernelHeap[0].info.maxBlocks = LEAKCHECK_HEAP_BLOCK_MAX;
	snap->kernelHeap[0].info.blockList = snap->kernelHeap[0].blocks;
	result = sceKernelGetHeapInfo(LEAKCHECK_KERNEL_HEAP, 7, &snap->kernelHeap[0].info);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2788);
	snap->kernelHeap[1].info.size = sizeof(snap->kernelHeap[1].info);
	snap->kernelHeap[1].info.maxBlocks = LEAKCHECK_HEAP_BLOCK_MAX;
	snap->kernelHeap[1].info.blockList = snap->kernelHeap[1].blocks;
	result = sceKernelGetHeapInfo(LEAKCHECK_KERNEL_UNCACHE_HEAP, 7, &snap->kernelHeap[1].info);
	if (result < 0)
		SCE_DBG_ASSERT_FAIL(2797);
	return 0;
}

/* A snapshot pass: the snapshot taken and compared with the one before. */
int sceKernelSnapshot(void)
{
	SceCpuForKernel_B331AB16(NULL);
	if (s_snapshotCount == 0) {
		if (s_leakCheckInitDone == 0) {
			LeakCheckInit();
			s_leakCheckInitDone = 1;
		}
		s_snapshotAMemblock = sceKernelAllocMemBlockForKernel("SceSnapshotA",
								      SceDeci4pSDfMgrForKernel_35238B8A(),
								      LEAKCHECK_LOG_SIZE, NULL);
		SCE_DBG_ASSERT_CHECK(s_snapshotAMemblock >= 0, 2878);
		sceKernelGetMemBlockBase(s_snapshotAMemblock, (void **)&s_snapshotA);
		s_snapshotBMemblock = sceKernelAllocMemBlockForKernel("SceSnapshotB",
								      SceDeci4pSDfMgrForKernel_35238B8A(),
								      LEAKCHECK_LOG_SIZE, NULL);
		SCE_DBG_ASSERT_CHECK(s_snapshotBMemblock >= 0, 2884);
		sceKernelGetMemBlockBase(s_snapshotBMemblock, (void **)&s_snapshotB);
	}
	if (s_heapGeneration == 0) {
		s_showModule = 1;
		s_heapLogKeep = 1;
		s_heapCheckByGeneration = 1;
		sceKernelHeapLoggingStart();
		LeakCheckResourceInit();
	}
	if (s_snapshotCount == 0) {
		s_maskLogMemblock = sceKernelAllocMemBlockForKernel("SceMaskLog",
								    SceDeci4pSDfMgrForKernel_35238B8A(),
								    0x10000, NULL);
		sceKernelGetMemBlockBase(s_maskLogMemblock, (void **)&s_maskLog);
		s_maskLogMax = 0x10000 / sizeof(SdbgpCpuMaskEntry);
		s_maskLogHook.notify = LeakCheckCpuMaskLogAdd;
		SceCpuForKernel_B331AB16(&s_maskLogHook);
	}
	if (s_uidListGeneration == 0)
		LeakCheckUIDListInit();
	LeakCheckUIDList();
	s_maskLogCount = 0;
	s_uidListGeneration++;
	SceCpuForKernel_B331AB16(&s_maskLogHook);
	if (s_snapshotCount & 1) {
		if (s_snapshotCount != 1)
			LeakCheckFreeGUIDInfo(s_snapshotB);
		sceSdbgpGetSystemResourceSnapshot(5, s_snapshotB);
		if (s_snapshotA != NULL && s_snapshotB != NULL)
			LeakCheckCompare(s_snapshotA, s_snapshotB);
	} else {
		if (s_snapshotCount != 0)
			LeakCheckFreeGUIDInfo(s_snapshotA);
		sceSdbgpGetSystemResourceSnapshot(5, s_snapshotA);
		if (s_snapshotCount != 0 && s_snapshotB != NULL && s_snapshotA != NULL)
			LeakCheckCompare(s_snapshotB, s_snapshotA);
	}
	s_snapshotCount++;
	s_heapGeneration++;
	return 0;
}

int LeakCheckUIDSnapshot(void)
{
	if (s_uidListGeneration == 0)
		LeakCheckUIDListInit();
	LeakCheckUIDList();
	s_uidListGeneration++;
	return 0;
}

int LeakCheckHeapSnapshot(void)
{
	if (s_heapGeneration == 0) {
		LeakCheckHeapInit();
		s_showModule = 1;
		s_heapLogKeep = 1;
		s_heapCheckByGeneration = 1;
		sceKernelHeapLoggingStart();
		LeakCheckResourceInit();
	}
	if (s_heapLogging != 0) {
		SceSysmemForKernel_42834422(NULL);
		s_heapLogging = 0;
	} else {
		s_heapLogging = 1;
		sceKernelHeapLoggingStart();
	}
	LeakCheckHeapCheck();
	s_heapGeneration++;
	return 0;
}

/*
 * The kernel's UIDs at the first pass, then at each later pass the UIDs new
 * since it, as records of 80 bytes.
 */
int _leakcheckUID(LeakCheckUIDRecord *out, uint32_t *generation, SceSize *count, SceSize *size)
{
	SdbgpState *state;
	SdbgpLeakCheck *leakCheck;
	SceKernelUIDEntryInfo *entries;
	LeakCheckUIDLogEntry *log, *entry;
	SceSize max, num, i, j;
	uint32_t pass;
	int result;

	if (count == NULL || out == NULL || size == NULL)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	state = sdbgpGetState();
	if (state == NULL)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	leakCheck = &state->leakCheck;
	*count = 0;
	*size = 0;
	*generation = 0;
	if (leakCheck->base.entries == NULL)
		LeakCheckUIDInit(state);
	if (leakCheck->generation != 0) {
		if (leakCheck->base.mode != LEAKCHECK_MODE_UID)
			return SDBGP_RESULT_ILLEGAL_VALUE;
		entries = leakCheck->current.entries;
		max = leakCheck->current.num;
		leakCheck->current.mode = LEAKCHECK_MODE_UID;
	} else {
		max = leakCheck->base.num;
		entries = leakCheck->base.entries;
		leakCheck->base.mode = LEAKCHECK_MODE_UID;
	}
	result = SceSysmemForKernel_3BF0DE65(7, 3, entries, max * sizeof(*entries), &num);
	if (result < 0) {
		SceDeci4pSDfMgrForKernel_501CE61D(0, SCE_DBGINFO(3084),
						  "sceGUIDGetEntryInfoAll failed 0x%08x\n", result);
	} else if (leakCheck->generation == 0) {
		leakCheck->base.num = max;
	} else {
		log = leakCheck->log;
		for (i = 0; i < num; i++) {
			for (j = 0; j < leakCheck->base.num; j++)
				if (leakCheck->base.entries[j].uid == entries[i].uid)
					break;
			if (leakCheck->base.num != 0 && j < leakCheck->base.num)
				continue;
			for (j = 0; j < leakCheck->logCount; j++)
				if (log[j].uid != -1 && log[j].uid == entries[i].uid)
					break;
			if (j < leakCheck->logCount)
				continue;
			for (j = 0; j < leakCheck->logCount; j++)
				if (log[j].uid == -1)
					break;
			if (j < leakCheck->logCount) {
				entry = &log[j];
			} else if (leakCheck->logMax < leakCheck->logCount) {
				SceDebugForDriver_391B74B7("Overflow\n");
				log = leakCheck->log;
				continue;
			} else {
				entry = &log[++leakCheck->logCount];
			}
			entry->generation = leakCheck->generation;
			entry->uid = entries[i].uid;
			entry->attr = entries[i].attr;
			SceDeci4pSDfMgrForKernel_5996E2DB(entry->name, entries[i].name,
							  sizeof(entry->name) - 1);
			SceDeci4pSDfMgrForKernel_5996E2DB(entry->className, entries[i].cls->name,
							  sizeof(entry->className) - 1);
			log = leakCheck->log;
		}

		/* the ones gone since */
		entry = log;
		for (j = 0; j < leakCheck->logCount; j++) {
			if (entry->uid == -1)
				continue;
			for (i = 0; i < num; i++)
				if (entries[i].uid == entry->uid)
					break;
			if (i == num)
				entry->uid = -1;
			entry++;
		}

		pass = 0;
		do {
			entry = log;
			for (j = 0; j < leakCheck->logCount; j++, entry++) {
				if (entry->uid == -1 || entry->generation != pass)
					continue;
				out->size = sizeof(*out);
				out->uid = entry->uid;
				out->attr = entry->attr;
				out->generation = pass;
				SceDeci4pSDfMgrForKernel_5996E2DB(out->name, entry->name, sizeof(out->name));
				SceDeci4pSDfMgrForKernel_5996E2DB(out->className, entry->className,
								  sizeof(out->className));
				out++;
				(*count)++;
				*size += sizeof(*out);
			}
			log = leakCheck->log;
		} while (++pass <= leakCheck->generation);
	}
	*generation = leakCheck->generation;
	leakCheck->generation++;
	return 0;
}

/*
 * The heap log: at the first pass the allocation hooks set, at each later one
 * the chunks still allocated, as records of 60 bytes.
 */
int _leakcheckHeap(LeakCheckHeapRecord *out, uint32_t *generation, SceSize *count, SceSize *size)
{
	SdbgpState *state;
	SdbgpLeakCheck *leakCheck;
	LeakCheckNamedChunk *log;
	SceKernelIntrStatus intr;
	unsigned int i;

	if (count == NULL || out == NULL)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	if (size == NULL)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	state = sdbgpGetState();
	if (state == NULL)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	leakCheck = &state->leakCheck;
	*count = 0;
	*size = 0;
	*generation = 0;
	if (leakCheck->base.entries == NULL)
		LeakCheckUIDInit(state);
	if (leakCheck->generation != 0) {
		if (leakCheck->base.mode != LEAKCHECK_MODE_HEAP)
			return SDBGP_RESULT_ILLEGAL_VALUE;
		log = leakCheck->log;
		leakCheck->current.mode = LEAKCHECK_MODE_HEAP;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_namedLogLock);
		for (i = 0; i < LEAKCHECK_LOG_SIZE / sizeof(*log); i++) {
			if (log[i].chunk.ptr == NULL)
				continue;
			if ((log[i].chunk.heap & 1) != 0 && LeakCheckIsIgnoredHeap(log[i].name))
				continue;
			out->size = sizeof(*out);
			out->heap = log[i].chunk.heap;
			out->ptr = log[i].chunk.ptr;
			out->chunkSize = log[i].chunk.size;
			out->caller = log[i].chunk.caller;
			out->func = log[i].chunk.func;
			out->generation = log[i].chunk.generation;
			SceDeci4pSDfMgrForKernel_5996E2DB(out->name, log[i].name, sizeof(out->name) - 1);
			out++;
			(*count)++;
			*size += sizeof(*out);
		}
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_namedLogLock, intr);
	} else {
		leakCheck->base.mode = LEAKCHECK_MODE_HEAP;
		if (leakCheck->log != NULL) {
			SceDeci4pSDfMgrForKernel_8A3B06F2(leakCheck->log, 0, 4);
			SceSysmemForKernel_42834422(&s_namedLogHooks);
		}
	}
	*generation = leakCheck->generation;
	leakCheck->generation++;
	return 0;
}

/* Both logs back to their first pass. */
int _leakcheckReset(void)
{
	SdbgpState *state = sdbgpGetState();
	SdbgpLeakCheck *leakCheck;
	SceKernelIntrStatus intr;

	if (state == NULL)
		return SDBGP_ERROR_INVALID_ARGUMENT;
	leakCheck = &state->leakCheck;
	leakCheck->base.mode = 0;
	leakCheck->current.mode = 0;
	leakCheck->generation = 0;
	if (leakCheck->base.entries == NULL) {
		LeakCheckUIDInit(state);
		return 0;
	}
	SceSysmemForKernel_42834422(NULL);
	SceDeci4pSDfMgrForKernel_8A3B06F2(leakCheck->base.entries, 0, LEAKCHECK_LOG_SIZE);
	SceDeci4pSDfMgrForKernel_8A3B06F2(leakCheck->current.entries, 0, LEAKCHECK_LOG_SIZE);
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_namedLogLock);
	SceDeci4pSDfMgrForKernel_8A3B06F2(leakCheck->log, 0, LEAKCHECK_LOG_SIZE);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_namedLogLock, intr);
	s_leakcheckResetDone = 0;
	return 0;
}
