/*
 * SceDeci4pSDbgp (os0:kd/deci4p_sdbgp.skprx): kernel resource display and
 * leak-check utility commands (kshow).
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <scetypes.h>
#include <sysmem/address_space.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/heap_info.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <sysmem/uid_class.h>
#include <iofilemgr/iofilemgr.h>
#include <modulemgr/modulemgr.h>
#include <processmgr/process_info.h>
#include <processmgr/processmgr.h>
#include <threadmgr/threadmgr.h>

#include "sdbgp_internal.h"

int strncmp(const char *left, const char *right, size_t count) __attribute__((pure));
int sceKernelFixedHeapSetHook(SceUID uid, void *hook);
int SceSysmemForKernel_811C452B(void *heap, void *hook);
int sceUIDEntryHeapSetHook(SceUID guid, void *hook);
int SceSysmemForKernel_93D0A183(const void *callbacks);
int SceSysmemForKernel_2C7B8D65(const void *hook);
int SceSysmemForKernel_31FA81B5(const void *hook);
int SceSysmemForKernel_F57A97A0(const void *callbacks);
int SceSysmemForKernel_BD625136(void *info);

typedef struct SdbgpABTimeEntry {
	uint32_t count;
	uint32_t pad;
	uint64_t min;
	uint64_t max;
	uint64_t sum;
	uint64_t start;
} SdbgpABTimeEntry;

_Static_assert(sizeof(SdbgpABTimeEntry) == 40, "SdbgpABTimeEntry size");

typedef struct SdbgpFixedHeapLogEntry {
	void *heap;
	void *obj;
	const void *lr;
	uint32_t gen;
} SdbgpFixedHeapLogEntry;

typedef struct SdbgpFixedHeapLeakDesc {
	const char *title;
	const char *blockName;
	SceSize blockSize;
	int (*showHead)(void);
	int (*showLog)(uint32_t index, const SdbgpFixedHeapLogEntry *entry);
} SdbgpFixedHeapLeakDesc;

typedef struct SdbgpFixedHeapLeakCtx {
	const SdbgpFixedHeapLeakDesc *desc;
	SceKernelSpinlock lock;
	uint32_t gen;
	SceUID uid;
	void *heapPtr;
	SceUID memblock;
	SdbgpFixedHeapLogEntry *entries;
	uint32_t max;
	void *hookArg;
	int (*allocHook)(struct SdbgpFixedHeapLeakCtx *ctx, void *heap, void *obj, const void *lr);
	int (*freeHook)(struct SdbgpFixedHeapLeakCtx *ctx, void *heap, void *obj);
} SdbgpFixedHeapLeakCtx;

_Static_assert(sizeof(SdbgpFixedHeapLeakCtx) == 44, "SdbgpFixedHeapLeakCtx size");

typedef struct SdbgpLogCtx {
	uint32_t gen;
	const char *name;
	SceSize size;
	uint32_t max;
	SceUID memblock;
	SceKernelSpinlock lock;
	void *entries;
	SceSize entrySize;
	int (*isValid)(const void *entry);
	int (*showHead)(void);
	int (*showLog)(uint32_t index, const void *entry);
} SdbgpLogCtx;

_Static_assert(sizeof(SdbgpLogCtx) == 44, "SdbgpLogCtx size");

typedef struct __attribute__((may_alias)) SdbgpNameHeapLogEntry {
	uint32_t gen;
	uint32_t op;
	uint32_t core;
	uintptr_t lr;
	uint32_t flags;
	char name[32];
} SdbgpNameHeapLogEntry;

_Static_assert(sizeof(SdbgpNameHeapLogEntry) == 52, "SdbgpNameHeapLogEntry size");

typedef struct SdbgpMemBlockMCEntry {
	uint32_t gen;
	const void *lr;
	void *part;
	void *block;
	int count;
} SdbgpMemBlockMCEntry;

_Static_assert(sizeof(SdbgpMemBlockMCEntry) == 20, "SdbgpMemBlockMCEntry size");

typedef struct SdbgpPhyMemLowEntry {
	void *part;
	uintptr_t page;
	SceSize psize;
	const void *lr;
	uint32_t gen;
} SdbgpPhyMemLowEntry;

_Static_assert(sizeof(SdbgpPhyMemLowEntry) == 20, "SdbgpPhyMemLowEntry size");

typedef struct SdbgpPhyMemPartEntry {
	uintptr_t part;
	SceSize psize;
	const void *page;
	const void *lr;
	uint32_t gen;
} SdbgpPhyMemPartEntry;

_Static_assert(sizeof(SdbgpPhyMemPartEntry) == 20, "SdbgpPhyMemPartEntry size");

typedef struct SdbgpPhyMemPartListEntry {
	uintptr_t part;
	const void *lr;
	uint32_t gen;
} SdbgpPhyMemPartListEntry;

_Static_assert(sizeof(SdbgpPhyMemPartListEntry) == 12, "SdbgpPhyMemPartListEntry size");

typedef struct SdbgpPhyMemPartLogEntry {
	char name[32];
	uint32_t op;
	uint32_t index;
	uintptr_t part;
	uintptr_t pbase;
	SceSize psize;
} SdbgpPhyMemPartLogEntry;

_Static_assert(sizeof(SdbgpPhyMemPartLogEntry) == 52, "SdbgpPhyMemPartLogEntry size");

typedef struct SdbgpKData {
	SceKernelUIDEntryHeapInfo uidEntryHeap;
	SceKernelFixedHeapInfo fixedHeap[6];
	SdbgpFixedHeapLeakCtx *guidEntryLeak;
	SdbgpFixedHeapLeakCtx *fixedHeapLeak[6];
	SdbgpFixedHeapLeakCtx *procFixedHeapLeak;
	int processCount;
	SceKernelProcessInfo process[16];
	AddressSpaceInfo addressSpace[16];
	ObjectHeapInfo objectHeap[2];
	SceKernelFixedHeapInfo procFixedHeap[2];
} SdbgpKData;

_Static_assert(offsetof(SdbgpKData, guidEntryLeak) == 888, "SdbgpKData guidEntryLeak");
_Static_assert(offsetof(SdbgpKData, procFixedHeapLeak) == 916, "SdbgpKData procFixedHeapLeak");
_Static_assert(offsetof(SdbgpKData, processCount) == 920, "SdbgpKData processCount");
_Static_assert(offsetof(SdbgpKData, process) == 924, "SdbgpKData process");
_Static_assert(offsetof(SdbgpKData, addressSpace) == 4636, "SdbgpKData addressSpace");
_Static_assert(offsetof(SdbgpKData, objectHeap) == 30556, "SdbgpKData objectHeap");

typedef struct SdbgpKShowCmd {
	int type;
	int cmd;
	int (*fn)(int cmd);
} SdbgpKShowCmd;

typedef struct SdbgpKShowGroup {
	int prefix;
	const char *desc;
} SdbgpKShowGroup;

typedef struct SdbgpKShowTarget {
	int id;
	const char *name;
	SceUID uid;
	int index;
	SceSize itemSize;
} SdbgpKShowTarget;

typedef struct SdbgpSMBInfo {
	SceSize size;
	struct {
		uint32_t free;
		uint32_t total;
		uint32_t freeMin;
	} entry[9];
} SdbgpSMBInfo;

_Static_assert(sizeof(SdbgpSMBInfo) == 112, "SdbgpSMBInfo size");

static int NameHeapLogIsValid(const void *entry);
static int NameHeapLogShowHead(void);
static int NameHeapLogShowLog(uint32_t index, const void *entry);
static int MemBlockMCIsValid(const void *entry);
static int LeakCheckMemBlockMCShowHead(void);
static int LeakCheckMemBlockMCShowLog(uint32_t index, const void *entry);
static int FixedHeapShowHead(void);
static int FixedHeapShowLog(uint32_t index, const SdbgpFixedHeapLogEntry *entry);
static int GUIDEntryHeapShowHead(void);
static int GUIDEntryHeapShowLog(uint32_t index, const SdbgpFixedHeapLogEntry *entry);

static const SdbgpFixedHeapLeakDesc s_fixedHeapLeakDesc = {
	"FixedHeap",
	"SceKernelFixedHeapLeakCheckLog",
	0x100000,
	FixedHeapShowHead,
	FixedHeapShowLog,
};

static const SdbgpFixedHeapLeakDesc s_guidEntryHeapLeakDesc = {
	"GUIDEntryHeap",
	"SceGUIDEntryHeapLeakCheckLog",
	0x100000,
	GUIDEntryHeapShowHead,
	GUIDEntryHeapShowLog,
};

static SdbgpKShowGroup s_kshowGroups[14] = {
	{ 0x0000, "show help messages" },
	{ 0x1000, "show status for" },
	{ 0x2100, "start leak chcek for" },
	{ 0x2200, "stop leak chcek for" },
	{ 0x2300, "check leak for" },
	{ 0x2800, "check leak filter for" },
	{ 0x8000, "show A-B time" },
	{ 0x9100, "show CPU mask time" },
	{ 0x9200, "disable CPU mask hook" },
	{ 0x9300, "reset CPU mask log" },
	{ 0x3100, "log start for " },
	{ 0x3200, "log stop for " },
	{ 0x3300, "show log for " },
	{ 0x3400, "save log for " },
};

static SdbgpKShowTarget s_kshowTargets[24] = {
	{ 0x10, "GUID Entry Heap", 0x10003, 0, 0x0 },
	{ 0x21, "kernel fixed heap 16B", 0x10033, 0, 0x10 },
	{ 0x22, "kernel fixed heap 32B", 0x10035, 1, 0x20 },
	{ 0x23, "kernel fixed heap 64B", 0x10039, 2, 0x40 },
	{ 0x24, "kernel fixed heap 128B", 0x1003b, 3, 0x80 },
	{ 0x25, "kernel fixed heap 256B", 0x1003d, 4, 0x100 },
	{ 0x26, "kernel fixed heap 512B", 0x1003f, 5, 0x200 },
	{ 0x30, "Process", 0x0, 0, 0x0 },
	{ 0x40, "Object Heap", 0x0, 0, 0x0 },
	{ 0x50, "MemBlock", 0x0, 0, 0x0 },
	{ 0x51, "MemBlock Master Count", 0x0, 0, 0x0 },
	{ 0x60, "SMB", 0x0, 0, 0x0 },
	{ 0x70, "PhyMemPart", 0x0, 0, 0x0 },
	{ 0x71, "PhyMemPart Kernel", 0x1, 0, 0x0 },
	{ 0x72, "PhyMemPart Tool", 0x2, 0, 0x0 },
	{ 0x73, "PhyMemPart Game", 0x3, 0, 0x0 },
	{ 0x74, "PhyMemPart Phycont", 0x4, 0, 0x0 },
	{ 0x75, "PhyMemPart Shared", 0x5, 0, 0x0 },
	{ 0x76, "PhyMemPart Game CDialog", 0x6, 0, 0x0 },
	{ 0x77, "PhyMemPart Shell", 0x7, 0, 0x0 },
	{ 0x80, "PhyMemLow", -1, 0, 0x0 },
	{ 0x91, "SceKernelThreadMgrCBHeap", -1, 0, 0x0 },
	{ 0xa0, "NameHeap", -1, 0, 0x0 },
	{ 0xb4, "Process FixedHeap128B", -1, 0, 0x80 },
};

static const uint32_t s_smbSizes[9] = {
	0x1000, 0x2000, 0x4000, 0x8000, 0x10000, 0x20000, 0x40000, 0x80000, 0x100000,
};

static struct {
	SdbgpLogCtx log;
	void *hooks[4];
} s_nameHeapLogCtx = {
	{
		0,
		"SceKernelNameHeapLog",
		0x800000,
		0x27627,
		-1,
		0,
		NULL,
		sizeof(SdbgpNameHeapLogEntry),
		NameHeapLogIsValid,
		NameHeapLogShowHead,
		NameHeapLogShowLog,
	},
	{ NULL, NULL, NULL, NULL },
};

static SceUID s_kdataMemblock = -1;

static struct {
	uint32_t gen;
	SceSize size;
	uint32_t max;
	SceUID memblock;
	SdbgpPhyMemLowEntry *entries;
	SceKernelSpinlock lock;
	void *hooks[2];
} s_phyMemLowCtx = {
	0, 0x100000, 0xcccc, -1, NULL, 0, { NULL, NULL }
};

static SdbgpLogCtx s_memBlockMCCtx = {
	0,
	"SceKernelLeakCheckMemBlockMC",
	0x100000,
	0xcccc,
	-1,
	0,
	NULL,
	sizeof(SdbgpMemBlockMCEntry),
	MemBlockMCIsValid,
	LeakCheckMemBlockMCShowHead,
	LeakCheckMemBlockMCShowLog,
};
static void *s_memBlockMCHooks[2];

static struct {
	uint32_t gen;
	SceSize listSize;
	uint32_t max;
	SceUID memblock;
	SdbgpPhyMemPartEntry *entries;
	SceKernelSpinlock lock;
	SceSize partMaxBytes;
	uint32_t partMax;
	SceUID partMemblock;
	SdbgpPhyMemPartListEntry *partEntries;
	SceKernelSpinlock partLock;
	void *hooks[4];
} s_phyMemPartLeakCtx = {
	0, 0x100000, 0xcccc, -1, NULL, 0,
	0x100000, 0x15555, -1, NULL, 0,
	{ NULL, NULL, NULL, NULL }
};

static struct {
	uint32_t gen;
	SceSize size;
	uint32_t max;
	SceUID memblock;
	SdbgpPhyMemPartLogEntry *entries;
	SceKernelSpinlock lock;
	void *hooks[2];
} s_phyMemPartLogCtx = {
	0, 0x100000, 0x4ec4, -1, NULL, 0, { NULL, NULL }
};

static MemBlockInfoEx s_memBlockInfoList[2048];
static SdbgpABTimeEntry s_abTime[16];
static char s_nameHeapSaveBuf[0x200];
struct SysrootProfiler s_sdbgpProfiler;
static SdbgpKData *s_kdata;
static struct {
	int res;
	int count;
	SceUID puids[256];
	SceUID guids[256];
} s_procMemBlocks;

/* 0x81006cc4 */
static int ABTimeReset(void)
{
	int i;

	for (i = 0; i < 16; i++) {
		s_abTime[i].count = 0;
		s_abTime[i].min = (uint64_t)-1;
		s_abTime[i].max = 0;
		s_abTime[i].sum = 0;
	}
	return 0;
}

/* 0x81006cfc */
static int LogShowCommon(SdbgpLogCtx *ctx)
{
	uint32_t gen;

	if (ctx->gen == 0)
		return SCE_KERNEL_ERROR_ERROR;
	if (ctx->entries != NULL) {
		for (gen = 0; gen <= ctx->gen; gen++) {
			uint32_t i;
			const uint8_t *p;

			ctx->showHead();
			p = (const uint8_t *)ctx->entries;
			for (i = 0; i < ctx->max; i++) {
				if (ctx->isValid(p) && *(const uint32_t *)p == gen)
					ctx->showLog(i, p);
				p += ctx->entrySize;
			}
		}
	}
	ctx->gen++;
	return 0;
}

/* 0x81006d54 */
static int MemBlockMCIsValid(const void *entry)
{
	const SdbgpMemBlockMCEntry *e = (const SdbgpMemBlockMCEntry *)entry;

	return e->block != NULL;
}

/* 0x81006d60 */
static int LeakCheckMemBlockMCShow(int cmd)
{
	(void)cmd;
	LogShowCommon(&s_memBlockMCCtx);
	return 0;
}

/* 0x81006d74 */
static int NameHeapLogIsValid(const void *entry)
{
	const SdbgpNameHeapLogEntry *e = (const SdbgpNameHeapLogEntry *)entry;

	return e->op != -1;
}

/* 0x81006d80 */
static int NameHeapLogShow(int cmd)
{
	(void)cmd;
	return LogShowCommon(&s_nameHeapLogCtx.log);
}

/* 0x81006d90 */
static int CpuMaskReset(int cmd)
{
	(void)cmd;
	sdbgpCpuMaskLog.count = 0;
	return 0;
}

/* 0x81006da0 */
static int FixedHeapCommonAllocHook(SdbgpFixedHeapLeakCtx *ctx, void *heap, void *obj,
				    const void *lr)
{
	int intr;
	uint32_t i;
	SdbgpFixedHeapLogEntry *e;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&ctx->lock);
	e = ctx->entries;
	for (i = 0; i < ctx->max; i++, e++) {
		if (e->obj == NULL) {
			uint32_t gen = ctx->gen;

			e->heap = heap;
			e->obj = obj;
			e->lr = lr;
			e->gen = gen;
			break;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
	return 0;
}

/* 0x81006df4 */
static int FixedHeapAllocHook(SdbgpFixedHeapLeakCtx *ctx, void *heap, void *obj, const void *lr)
{
	return FixedHeapCommonAllocHook(ctx, heap, obj, lr);
}

/* 0x81006dfc */
static int GUIDEntryHeapAllocHook(SdbgpFixedHeapLeakCtx *ctx, void *heap, void *obj, const void *lr)
{
	return FixedHeapCommonAllocHook(ctx, heap, obj, lr);
}

/* 0x81006e04 */
static int MemBlockMCIncHook(void *part, void *block, int count, const void *lr)
{
	int intr;
	SdbgpMemBlockMCEntry *entries;
	uint32_t i;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_memBlockMCCtx.lock);
	entries = (SdbgpMemBlockMCEntry *)s_memBlockMCCtx.entries;
	if (s_memBlockMCCtx.max != 0) {
		SdbgpMemBlockMCEntry *e = entries;

		for (i = 0; i < s_memBlockMCCtx.max; i++, e++) {
			if (e->block == block) {
				e->part = part;
				e->block = block;
				e->count = count;
				e->lr = lr;
				goto out;
			}
		}
		e = entries;
		for (i = 0; i < s_memBlockMCCtx.max; i++, e++) {
			if (e->block == NULL) {
				uint32_t gen = s_memBlockMCCtx.gen;

				e->count = count;
				e->part = part;
				e->block = block;
				e->gen = gen;
				e->lr = lr;
				break;
			}
		}
	}
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_memBlockMCCtx.lock, intr);
	return 0;
}

/* 0x81006e90 */
static int PhyMemLowAllocHook(void *part, uintptr_t page, SceSize psize, const void *lr)
{
	int intr;
	SdbgpPhyMemLowEntry *e;
	uint32_t i;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_phyMemLowCtx.lock);
	e = s_phyMemLowCtx.entries;
	for (i = 0; i < s_phyMemLowCtx.max; i++, e++) {
		if (e->page == 0) {
			uint32_t gen = s_phyMemLowCtx.gen;

			e->part = part;
			e->page = page;
			e->psize = psize;
			e->lr = lr;
			e->gen = gen;
			break;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_phyMemLowCtx.lock, intr);
	return 0;
}

/* 0x81006ef0 */
static int PhyMemPartAllocHook(uintptr_t part, SceSize psize, const void *page, const void *lr)
{
	int intr;
	SdbgpPhyMemPartEntry *e;
	uint32_t i;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_phyMemPartLeakCtx.lock);
	e = s_phyMemPartLeakCtx.entries;
	for (i = 0; i < s_phyMemPartLeakCtx.max; i++, e++) {
		if (e->page == NULL) {
			uint32_t gen = s_phyMemPartLeakCtx.gen;

			e->part = part;
			e->psize = psize;
			e->page = page;
			e->lr = lr;
			e->gen = gen;
			break;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_phyMemPartLeakCtx.lock, intr);
	return 0;
}

/* 0x81006f50 */
static int FixedHeapShowLog(uint32_t index, const SdbgpFixedHeapLogEntry *entry)
{
	char modName[27] = "NULL";
	uintptr_t textBase;
	const char *name = NULL;
	uintptr_t offset;
	uintptr_t cls;
	const char *clsName;
	uintptr_t pa;

	if (SceModulemgrForKernel_5564A860(SDBGP_KERNEL_PROCESS_ID, (uintptr_t)entry->lr, NULL,
					   (void **)&textBase, modName) < 0)
		offset = 0;
	else
		offset = (uintptr_t)entry->lr - textBase;
	cls = *((const uint32_t *)entry->obj + 1);
	if (sceKernelVAtoPA((const void *)cls, &pa) < 0) {
		cls = 0;
		clsName = NULL;
	} else {
		clsName = (const char *)cls;
		if (cls != 0)
			clsName = *(const char **)(cls + 12);
	}
	if ((SceClass *)cls == SceSysmemForKernel_86681B64()) {
		if (sceGUIDGetName(*((const SceUID *)entry->obj + 15), &name) < 0)
			name = clsName;
	} else {
		name = clsName;
	}
	return SCE_PRINTF_WITH_INFO(0, 1627,
		"\t%4d: 0x%08x: %p[0x%08x], %p[%-32s], %p[%s:0x%08x]\n",
		entry->gen, index, entry->heap,
		*((const uint16_t *)entry->heap + 16),
		entry->obj, name, entry->lr, modName, offset);
}

/* 0x8100703c */
static int FixedHeapShowHead(void)
{
	return SCE_PRINTF_WITH_INFO(0, 1566,
		"\tgen-: index-----: pHeap-----[----------], pObject---[--------------------------------], lr--------\n");
}

/* 0x81007058 */
static int NameHeapLogShowHead(void)
{
	return SCE_PRINTF_WITH_INFO(0, 2359,
		"\tgen-: index-----, op--------, name----[--------------------------------], lr--------[--------------------------------]+----------\n");
}

/* 0x81007074 */
static int ABTimeShow(int cmd)
{
	int i;

	(void)cmd;
	SCE_PRINTF_WITH_INFO(0, 2642, "AB Time (usec)\n");
	for (i = 0; i < 16; i++) {
		if (s_abTime[i].count != 0) {
			SCE_PRINTF_WITH_INFO(0, 2648,
				"\t%5d: count:%4d: min:0x%016llx max:0x%016llx sum:0x%016llx ave:0x%016llx\n",
				i, s_abTime[i].count, s_abTime[i].min,
				s_abTime[i].max, s_abTime[i].sum,
				s_abTime[i].sum / s_abTime[i].count);
		}
	}
	return 0;
}

/* 0x8100710c */
static int LeakCheckPhyMemLowShow(int cmd)
{
	uint32_t gen;

	(void)cmd;
	if (s_phyMemLowCtx.gen == 0)
		return SCE_KERNEL_ERROR_ERROR;
	SCE_PRINTF_WITH_INFO(0, 3345, "Leak check PhyMemLow\n");
	if (s_phyMemLowCtx.entries != NULL) {
		for (gen = 0; gen <= s_phyMemLowCtx.gen; gen++) {
			uint32_t i;
			const SdbgpPhyMemLowEntry *e;

			SCE_PRINTF_WITH_INFO(0, 3349,
				"\tgen-: index-----: [--------------------------------], pPhyPage--, psize-----, lr---------\n");
			e = s_phyMemLowCtx.entries;
			for (i = 0; i < s_phyMemLowCtx.max; i++, e++) {
				if (e->page != 0 && e->gen == gen) {
					SCE_PRINTF_WITH_INFO(0, 3357,
						"\t%4d: 0x%08x: %p, 0x%08x, 0x%08x, %p\n",
						gen, i, e->part, e->page, e->psize, e->lr);
				}
			}
		}
	}
	s_phyMemLowCtx.gen++;
	return 0;
}

/* 0x810071d0 */
static int LeakCheckMemBlockMCShowHead(void)
{
	return SCE_PRINTF_WITH_INFO(0, 2116,
		"\tgen-: index-----: pPart-----, pBlock----[--------------------------------], count-----, lr--------[--------------------------------]+----------\n");
}

/* 0x810071ec */
static int GUIDEntryHeapShowHead(void)
{
	return SCE_PRINTF_WITH_INFO(0, 1402,
		"\tgen-: index-----: pHeap-----[----------], pObject---[--------------------------------], lr--------\n");
}

static inline const char *findGroupDesc(int prefix)
{
	uint32_t i;

	for (i = 0; i < 14; i++) {
		if (s_kshowGroups[i].prefix == prefix)
			return s_kshowGroups[i].desc;
	}
	return "NOOP";
}

static inline int findTarget(int id, const SdbgpKShowTarget **out)
{
	const SdbgpKShowTarget *t = s_kshowTargets;
	uint32_t i;

	for (i = 0; i < 24; i++, t++) {
		if (t->id == id) {
			*out = t;
			return 0;
		}
	}
	*out = NULL;
	return SCE_KERNEL_ERROR_ERROR;
}

/* 0x81007208 */
static int help(int cmd)
{
	extern SdbgpKShowCmd s_kshowCmds[53];
	uint32_t i;

	(void)cmd;
	SCE_PRINTF_WITH_INFO(0, 3756, "kshow help message\n");
	for (i = 0; i < 53; i++) {
		SCE_PRINTF_WITH_INFO(0, 3760, "\t0x%04X: ", s_kshowCmds[i].cmd);
		if (s_kshowCmds[i].type == 1) {
			SCE_PRINTF_WITH_INFO(0, 3763, "%s\n",
				findGroupDesc(s_kshowCmds[i].cmd & 0xff00));
		} else if (s_kshowCmds[i].type == 2) {
			const char *g = findGroupDesc(s_kshowCmds[i].cmd & 0xff00);
			const SdbgpKShowTarget *t;

			findTarget((uint8_t)s_kshowCmds[i].cmd, &t);
			SCE_PRINTF_WITH_INFO(0, 3766, "%s %s\n", g, t->name);
		}
	}
	return 0;
}

static inline int FixedHeapCommonShow(SdbgpFixedHeapLeakCtx *ctx)
{
	uint32_t gen;

	SCE_PRINTF_WITH_INFO_FUNC(0, 1247, "FixedHeapCommonShow",
		"Leak check for %s - %d\n", ctx->desc->title, ctx->gen);
	if (ctx->entries != NULL) {
		for (gen = 0; gen <= ctx->gen; gen++) {
			uint32_t i;
			int first = 1;
			const SdbgpFixedHeapLogEntry *e = ctx->entries;

			for (i = 0; i < ctx->max; i++, e++) {
				if (e->obj != NULL && e->gen == gen) {
					if (first) {
						ctx->desc->showHead();
						first = 0;
					}
					ctx->desc->showLog(i, e);
				}
			}
		}
	}
	ctx->gen++;
	return 0;
}

/* 0x81007330 */
static int FixedHeapCommonLeakCheck(SdbgpFixedHeapLeakCtx *ctx)
{
	SCE_DBG_PRINTF_LEVEL(1, 11, 1386, "()\n");
	return FixedHeapCommonShow(ctx);
}

/* 0x810073c8 */
static int LogPhyMemPartShow(int cmd)
{
	uint32_t i;
	int first;
	const SdbgpPhyMemPartLogEntry *e;

	(void)cmd;
	SCE_DBG_PRINTF_LEVEL_FUNC(1, 11, 3606, "PhyMemPartLogCheck", "()\n");
	if (s_phyMemPartLogCtx.gen == 0)
		return SCE_KERNEL_ERROR_ERROR;
	SCE_PRINTF_WITH_INFO(0, 3455, "Log PhyMemPartList\n");
	e = s_phyMemPartLogCtx.entries;
	if (e != NULL && s_phyMemPartLogCtx.max != 0) {
		first = 1;
		for (i = 0; i < s_phyMemPartLogCtx.max; i++, e++) {
			const char *opStr;

			if (e->part == 0)
				continue;
			if (first) {
				SCE_PRINTF_WITH_INFO(0, 3462,
					"\tindex-----: -----: --: [--------------------------------]: pbase-----+psize-----\n");
			}
			if (e->op == 1)
				opStr = "alloc";
			else if (e->op == 2)
				opStr = "free";
			else
				opStr = "-----";
			SCE_PRINTF_WITH_INFO(0, 3480,
				"\t0x%08x: %-5s: %2d: [%-32s]: 0x%08x+0x%08x\n",
				i, opStr, e->index, e->name, e->pbase, e->psize);
			first = 0;
		}
	}
	s_phyMemPartLogCtx.gen++;
	return 0;
}

/* 0x810074c0 */
static int PhyMemLowFreeHook(void *part, uintptr_t page)
{
	int intr;
	SdbgpPhyMemLowEntry *e;
	uint32_t i;

	(void)part;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_phyMemLowCtx.lock);
	e = s_phyMemLowCtx.entries;
	for (i = 0; i < s_phyMemLowCtx.max; i++, e++) {
		if (e->page == page) {
			e->page = 0;
			goto out;
		}
	}
	SCE_DBG_PRINTF_LEVEL(1, 11, 3268, "not found 0x%08x\n", page);
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_phyMemLowCtx.lock, intr);
	return 0;
}

/* 0x8100752c */
static int PhyMemPartDeleteHook(uintptr_t part)
{
	int intr;
	SdbgpPhyMemPartListEntry *e;
	uint32_t i;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_phyMemPartLeakCtx.partLock);
	e = s_phyMemPartLeakCtx.partEntries;
	for (i = 0; i < s_phyMemPartLeakCtx.partMax; i++, e++) {
		if (e->part == part) {
			e->part = 0;
			goto out;
		}
	}
	SCE_DBG_PRINTF_LEVEL(1, 11, 2981, "not found %p\n", (const void *)part);
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_phyMemPartLeakCtx.partLock, intr);
	return 0;
}

/* 0x81007598 */
static int PhyMemPartCreateHook(uintptr_t part, const void *lr)
{
	int intr;
	SdbgpPhyMemPartListEntry *e;
	uint32_t i;

	SCE_DBG_PRINTF_LEVEL(1, 11, 2939, "(%p, %p)\n", (const void *)part, lr);
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_phyMemPartLeakCtx.partLock);
	e = s_phyMemPartLeakCtx.partEntries;
	for (i = 0; i < s_phyMemPartLeakCtx.partMax; i++, e++) {
		if (e->part == 0) {
			uint32_t gen = s_phyMemPartLeakCtx.gen;

			e->part = part;
			e->lr = lr;
			e->gen = gen;
			break;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_phyMemPartLeakCtx.partLock, intr);
	return 0;
}

/* 0x81007608 */
static int PhyMemPartFreeHook(uintptr_t part, const void *page)
{
	int intr;
	SdbgpPhyMemPartEntry *e;
	uint32_t i;

	(void)part;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_phyMemPartLeakCtx.lock);
	e = s_phyMemPartLeakCtx.entries;
	for (i = 0; i < s_phyMemPartLeakCtx.max; i++, e++) {
		if (e->page == page) {
			e->page = NULL;
			goto out;
		}
	}
	SCE_DBG_PRINTF_LEVEL(1, 11, 2922, "not found %p\n", page);
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_phyMemPartLeakCtx.lock, intr);
	return 0;
}

/* 0x81007674 */
static int MemBlockMCDecHook(void *part, void *block, int count, const void *lr)
{
	int intr;
	SdbgpMemBlockMCEntry *entries;
	uint32_t i;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_memBlockMCCtx.lock);
	entries = (SdbgpMemBlockMCEntry *)s_memBlockMCCtx.entries;
	if (s_memBlockMCCtx.max != 0) {
		SdbgpMemBlockMCEntry *e = entries;

		for (i = 0; i < s_memBlockMCCtx.max; i++, e++) {
			if (e->block == block) {
				SCE_DBG_PRINTF_LEVEL(1, 11, 2243, "found log %p\n", block);
				if (count != 0)
					e->count = count;
				else
					e->block = NULL;
				goto out;
			}
		}
		if (count != 0) {
			e = entries;
			for (i = 0; i < s_memBlockMCCtx.max; i++, e++) {
				if (e->block == NULL) {
					uint32_t gen = s_memBlockMCCtx.gen;

					e->count = count;
					e->gen = gen;
					e->part = part;
					e->block = block;
					e->lr = lr;
					break;
				}
			}
		}
	}
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_memBlockMCCtx.lock, intr);
	return 0;
}

/* 0x81007730 */
static int LogStartPhase1(SdbgpLogCtx *ctx)
{
	SceUID mb;

	if (ctx->gen != 0)
		return SCE_KERNEL_ERROR_ERROR;
	mb = sceKernelAllocMemBlockForKernel(ctx->name,
					     SceDeci4pSDfMgrForKernel_35238B8A(),
					     ctx->size, NULL);
	if (mb < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 11, 1957, "Alloc Memory Block failed (0x%08X)\n", mb);
		SCE_DBG_ASSERT_FAIL(1958);
		return mb;
	}
	sceKernelGetMemBlockBase(mb, &ctx->entries);
	ctx->memblock = mb;
	return 0;
}

/* 0x810077a8 */
static __attribute__((noinline)) SdbgpKData *GetKData(void)
{
	SceUID mb;
	SdbgpKData *kdata;
	int i;

	if (s_kdata != NULL)
		return s_kdata;
	mb = sceKernelAllocMemBlockForKernel("SceKData",
					     SceDeci4pSDfMgrForKernel_35238B8A(),
					     0x100000, NULL);
	s_kdataMemblock = mb;
	if (mb < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 11, 432,
			"sceKernelAllocMemBlockForKernel failed 0x%08x\n", mb);
		return NULL;
	}
	sceKernelGetMemBlockBase(mb, (void **)&s_kdata);
	kdata = s_kdata;
	kdata->guidEntryLeak = NULL;
	for (i = 0; i < 6; i++)
		kdata->fixedHeapLeak[i] = NULL;
	return kdata;
}

/* 0x81007820 */
static int FixedHeapLeakCheck(int cmd)
{
	const SdbgpKShowTarget *t;
	SdbgpKData *kdata;

	if (findTarget((uint8_t)cmd, &t) < 0)
		return SCE_KERNEL_ERROR_ERROR;
	kdata = GetKData();
	if (kdata != NULL && kdata->fixedHeapLeak[t->index] != NULL)
		return FixedHeapCommonLeakCheck(kdata->fixedHeapLeak[t->index]);
	return SCE_KERNEL_ERROR_ERROR;
}

/* 0x81007864 */
static int GUIDEntryHeapLeakCheck(int cmd)
{
	SdbgpKData *kdata;

	(void)cmd;
	kdata = GetKData();
	if (kdata != NULL && kdata->guidEntryLeak != NULL)
		return FixedHeapCommonLeakCheck(kdata->guidEntryLeak);
	return SCE_KERNEL_ERROR_ERROR;
}

/* 0x81007880 */
static int FixedHeapCommonLeakCheckStart(const SdbgpFixedHeapLeakDesc *desc,
					 SdbgpFixedHeapLeakCtx **out)
{
	SdbgpFixedHeapLeakCtx *ctx;
	SceUID mb;
	SdbgpFixedHeapLogEntry *base;
	uint32_t max;
	uint32_t i;

	ctx = sceKernelAllocHeapMemory(0x1002b, sizeof(*ctx));
	if (ctx == NULL)
		return SCE_KERNEL_ERROR_ERROR;
	ctx->desc = desc;
	mb = sceKernelAllocMemBlockForKernel(desc->blockName,
					     SceDeci4pSDfMgrForKernel_35238B8A(),
					     ctx->desc->blockSize, NULL);
	if (mb < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 11, 1352,
			"sceKernelAllocMemBlockForKernel failed (0x%08X)\n", mb);
		sceKernelFreeHeapMemory(0x1002b, ctx);
		SCE_DBG_ASSERT_FAIL(1354);
		return mb;
	}
	sceKernelGetMemBlockBase(mb, (void **)&base);
	ctx->memblock = mb;
	max = ctx->desc->blockSize >> 4;
	ctx->gen = 0;
	ctx->entries = base;
	ctx->max = max;
	for (i = 0; i < max; i++, base++)
		base->obj = NULL;
	ctx->lock = 0;
	*out = ctx;
	return 0;
}

static int FixedHeapFreeHook(SdbgpFixedHeapLeakCtx *ctx, void *heap, void *obj);
static int GUIDEntryHeapFreeHook(SdbgpFixedHeapLeakCtx *ctx, void *heap, void *obj);

/* 0x81007960 */
static int FixedHeapLeakCheckStartByUid(SceUID uid, SdbgpFixedHeapLeakCtx **out)
{
	SdbgpFixedHeapLeakCtx *ctx;
	int res;

	res = FixedHeapCommonLeakCheckStart(&s_fixedHeapLeakDesc, &ctx);
	if (res >= 0) {
		ctx->allocHook = FixedHeapAllocHook;
		ctx->uid = uid;
		ctx->hookArg = ctx;
		ctx->freeHook = FixedHeapFreeHook;
		res = sceKernelFixedHeapSetHook(uid, &ctx->hookArg);
		if (res >= 0) {
			res = 0;
			*out = ctx;
		}
	}
	return res;
}

/* 0x810079c4 */
static int FixedHeapLeakCheckStart(int cmd)
{
	const SdbgpKShowTarget *t;
	SdbgpKData *kdata;
	SdbgpFixedHeapLeakCtx *ctx;
	int res;

	if (findTarget((uint8_t)cmd, &t) < 0)
		return SCE_KERNEL_ERROR_ERROR;
	kdata = GetKData();
	if (kdata == NULL || kdata->fixedHeapLeak[t->index] != NULL)
		return SCE_KERNEL_ERROR_ERROR;
	res = FixedHeapLeakCheckStartByUid(t->uid, &ctx);
	if (res >= 0)
		kdata->fixedHeapLeak[t->index] = ctx;
	return res;
}

static int NameHeapLogHook1(const char *name, uint32_t flags, uintptr_t lr);
static int NameHeapLogHook2(const char *name, uint32_t flags, uintptr_t lr);
static int NameHeapLogHook3(const char *name, uint32_t flags, uintptr_t lr);
static int NameHeapLogHook4(const char *name, uint32_t flags, uintptr_t lr);

/* 0x81007a3c */
static int NameHeapLogStart(int cmd)
{
	int res;
	uint32_t i;

	(void)cmd;
	res = LogStartPhase1(&s_nameHeapLogCtx.log);
	if (res >= 0) {
		SdbgpNameHeapLogEntry *entries = s_nameHeapLogCtx.log.entries;

		for (i = 0; i < s_nameHeapLogCtx.log.max; i++, entries++)
			entries->op = (uint32_t)-1;
		s_nameHeapLogCtx.log.gen++;
		s_nameHeapLogCtx.hooks[0] = NameHeapLogHook1;
		s_nameHeapLogCtx.hooks[1] = NameHeapLogHook4;
		s_nameHeapLogCtx.hooks[2] = NameHeapLogHook2;
		s_nameHeapLogCtx.hooks[3] = NameHeapLogHook3;
		s_nameHeapLogCtx.log.lock = 0;
		SceSysmemForKernel_6DBDA03B(s_nameHeapLogCtx.hooks);
		return 0;
	}
	return res;
}

/* 0x81007ab4 */
static int CpuMaskDisable(int cmd)
{
	(void)cmd;
	SceCpuForKernel_B331AB16(NULL);
	return 0;
}

/* 0x81007ac0 */
static int FixedHeapLeakCheckStop(int cmd)
{
	const SdbgpKShowTarget *t;
	SdbgpKData *kdata;
	SdbgpFixedHeapLeakCtx *ctx;

	if (findTarget((uint8_t)cmd, &t) < 0)
		return SCE_KERNEL_ERROR_ERROR;
	kdata = GetKData();
	if (kdata != NULL) {
		ctx = kdata->fixedHeapLeak[t->index];
		if (ctx != NULL && sceKernelFixedHeapSetHook(ctx->uid, NULL) >= 0) {
			sceKernelFreeMemBlock(ctx->memblock);
			sceKernelFreeHeapMemory(0x1002b, ctx);
			kdata->fixedHeapLeak[t->index] = NULL;
			return 0;
		}
	}
	return SCE_KERNEL_ERROR_ERROR;
}

/* 0x81007b30 */
static int PhyMemPartLogStop(int cmd)
{
	(void)cmd;
	if (s_phyMemPartLogCtx.gen == 0)
		return SCE_KERNEL_ERROR_ERROR;
	SceSysmemForKernel_93D0A183(NULL);
	sceKernelFreeMemBlock(s_phyMemPartLogCtx.memblock);
	s_phyMemPartLogCtx.gen = 0;
	return 0;
}

/* 0x81007b58 */
static int PhyMemPartLogAdd(uintptr_t part, uint32_t op)
{
	int intr;
	uint32_t i, idx;
	SdbgpPhyMemPartLogEntry *e;
	const uint32_t *node;

	if (s_phyMemPartLogCtx.gen == 0)
		return SCE_KERNEL_ERROR_ERROR;
	idx = 0;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_phyMemPartLogCtx.lock);
	e = s_phyMemPartLogCtx.entries;
	node = *(const uint32_t **)(part + 124);
	for (i = 0; i < s_phyMemPartLogCtx.max; i++, e++) {
		if (e->part == 0) {
			strncpy(e->name, (const char *)(part + 140), 0x20);
			e->op = op;
			e->index = idx++;
			e->part = part;
			if (node == NULL)
				break;
			e->pbase = node[1];
			e->psize = node[2];
			node = (const uint32_t *)*node;
			if (node == NULL)
				break;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_phyMemPartLogCtx.lock, intr);
	return 0;
}

/* 0x81007bf0 */
static int PhyMemPartLogFreeHook(uintptr_t part)
{
	return PhyMemPartLogAdd(part, 2);
}

/* 0x81007bfc */
static int PhyMemPartLogAllocHook(uintptr_t part)
{
	return PhyMemPartLogAdd(part, 1);
}

/* 0x81007c08 */
static int NameHeapLogAdd(int32_t op, const char *name, uint32_t flags, uintptr_t lr)
{
	int intr;
	SdbgpNameHeapLogEntry *e;
	uint32_t i;
	int res;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_nameHeapLogCtx.log.lock);
	e = s_nameHeapLogCtx.log.entries;
	for (i = 0; i < s_nameHeapLogCtx.log.max; i++, e++) {
		if (e->op == -1) {
			uint32_t core;

			e->op = op;
			__asm__ volatile("mrc p15, 0, %0, c0, c0, 5" : "=r"(core));
			e->flags = flags;
			e->core = core & 3;
			e->lr = lr;
			res = 0;
			strncpy(e->name, name, 0x20);
			goto out;
		}
	}
	res = SCE_KERNEL_ERROR_ERROR;
	SCE_DBG_PRINTF_LEVEL(1, 11, 2438, "OVERFLOW: stop hook\n");
	SceSysmemForKernel_6DBDA03B(NULL);
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_nameHeapLogCtx.log.lock, intr);
	return res;
}

/* 0x81007ca0 */
static int NameHeapLogHook3(const char *name, uint32_t flags, uintptr_t lr)
{
	return NameHeapLogAdd(0x30000, name, flags, lr);
}

/* 0x81007cb4 */
static int NameHeapLogHook2(const char *name, uint32_t flags, uintptr_t lr)
{
	return NameHeapLogAdd(0x20000, name, flags, lr);
}

/* 0x81007cc8 */
static int NameHeapLogHook4(const char *name, uint32_t flags, uintptr_t lr)
{
	return NameHeapLogAdd(0x40000, name, flags, lr);
}

/* 0x81007cdc */
static int NameHeapLogHook1(const char *name, uint32_t flags, uintptr_t lr)
{
	return NameHeapLogAdd(0x10000, name, flags, lr);
}

/* 0x81007cf0 */
static int LeakCheckFilterCmd(int cmd)
{
	uint8_t lo = (uint8_t)cmd;
	const SdbgpKShowTarget *t;

	if (findTarget(lo, &t) < 0)
		return SCE_KERNEL_ERROR_ERROR;
	if (lo == 0x91)
		LeakCheckHeapFilerAdd("SceKernelThreadMgrCBHeap");
	return 0;
}

/* 0x81007d30 */
static int PhyMemLowLeakCheckStop(int cmd)
{
	(void)cmd;
	if (s_phyMemLowCtx.gen == 0)
		return SCE_KERNEL_ERROR_ERROR;
	SceSysmemForKernel_2C7B8D65(NULL);
	sceKernelFreeMemBlock(s_phyMemLowCtx.memblock);
	s_phyMemLowCtx.gen = 0;
	return 0;
}

/* 0x81007d58 */
static int PhyMemLowLeakCheckStart(int cmd)
{
	SceUID mb;
	int i;

	(void)cmd;
	if (s_phyMemLowCtx.gen != 0)
		return SCE_KERNEL_ERROR_ERROR;
	mb = sceKernelAllocMemBlockForKernel("SceKernelLeakCheckPhyMemLow",
					     SceDeci4pSDfMgrForKernel_35238B8A(),
					     s_phyMemLowCtx.size, NULL);
	if (mb < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 11, 3296, "Alloc Memory Block failed (0x%08X)\n", mb);
		SCE_DBG_ASSERT_FAIL(3297);
		return mb;
	}
	sceKernelGetMemBlockBase(mb, (void **)&s_phyMemLowCtx.entries);
	s_phyMemLowCtx.memblock = mb;
	for (i = 0; i < s_phyMemLowCtx.max; i++)
		s_phyMemLowCtx.entries[i].page = 0;
	s_phyMemLowCtx.gen++;
	s_phyMemLowCtx.hooks[0] = PhyMemLowAllocHook;
	s_phyMemLowCtx.hooks[1] = PhyMemLowFreeHook;
	s_phyMemLowCtx.lock = 0;
	SceSysmemForKernel_2C7B8D65(s_phyMemLowCtx.hooks);
	return 0;
}

/* 0x81007e20 */
static int PhyMemPartLeakCheckStop(int cmd)
{
	(void)cmd;
	if (s_phyMemPartLeakCtx.gen == 0)
		return SCE_KERNEL_ERROR_ERROR;
	SceSysmemForKernel_31FA81B5(NULL);
	sceKernelFreeMemBlock(s_phyMemPartLeakCtx.memblock);
	sceKernelFreeMemBlock(s_phyMemPartLeakCtx.partMemblock);
	s_phyMemPartLeakCtx.gen = 0;
	return 0;
}

/* 0x81007e50 */
static int PhyMemPartLeakCheckStart(int cmd)
{
	SceUID mb;
	int i;

	(void)cmd;
	if (s_phyMemPartLeakCtx.gen != 0)
		return SCE_KERNEL_ERROR_ERROR;
	mb = sceKernelAllocMemBlockForKernel("SceKernelLeakCheckPhyMemPart",
					     SceDeci4pSDfMgrForKernel_35238B8A(),
					     0x100000, NULL);
	if (mb < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 11, 3010, "Alloc Memory Block failed (0x%08X)\n", mb);
		SCE_DBG_ASSERT_FAIL(3011);
		return mb;
	}
	sceKernelGetMemBlockBase(mb, (void **)&s_phyMemPartLeakCtx.entries);
	s_phyMemPartLeakCtx.memblock = mb;
	for (i = 0; i < s_phyMemPartLeakCtx.max; i++)
		s_phyMemPartLeakCtx.entries[i].page = NULL;
	s_phyMemPartLeakCtx.lock = 0;
	mb = sceKernelAllocMemBlockForKernel("SceKernelLeakCheckPhyMemPartList",
					     SceDeci4pSDfMgrForKernel_35238B8A(),
					     s_phyMemPartLeakCtx.listSize, NULL);
	if (mb < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 11, 3031, "Alloc Memory Block failed (0x%08X)\n", mb);
		SCE_DBG_ASSERT_FAIL(3032);
		return mb;
	}
	sceKernelGetMemBlockBase(mb, (void **)&s_phyMemPartLeakCtx.partEntries);
	s_phyMemPartLeakCtx.partMemblock = mb;
	if (s_phyMemPartLeakCtx.partMax != 0)
		s_phyMemPartLeakCtx.partEntries[0].part = 0;
	s_phyMemPartLeakCtx.gen++;
	s_phyMemPartLeakCtx.hooks[0] = PhyMemPartAllocHook;
	s_phyMemPartLeakCtx.hooks[1] = PhyMemPartFreeHook;
	s_phyMemPartLeakCtx.hooks[2] = PhyMemPartCreateHook;
	s_phyMemPartLeakCtx.hooks[3] = PhyMemPartDeleteHook;
	s_phyMemPartLeakCtx.partLock = 0;
	SceSysmemForKernel_31FA81B5(s_phyMemPartLeakCtx.hooks);
	return 0;
}

/* 0x81007fb8 */
static int GUIDEntryHeapLeakCheckStop(int cmd)
{
	SdbgpKData *kdata;
	SdbgpFixedHeapLeakCtx *ctx;

	(void)cmd;
	kdata = GetKData();
	if (kdata == NULL)
		return SCE_KERNEL_ERROR_ERROR;
	ctx = kdata->guidEntryLeak;
	if (ctx == NULL)
		return SCE_KERNEL_ERROR_ERROR;
	sceUIDEntryHeapSetHook(ctx->uid, NULL);
	sceKernelFreeMemBlock(ctx->memblock);
	sceKernelFreeHeapMemory(0x1002b, ctx);
	kdata->guidEntryLeak = NULL;
	return 0;
}

/* 0x81007ff4 */
static int GUIDEntryHeapLeakCheckStart(int cmd)
{
	SdbgpKData *kdata;
	SdbgpFixedHeapLeakCtx *ctx;
	int res;

	(void)cmd;
	kdata = GetKData();
	if (kdata == NULL || kdata->guidEntryLeak != NULL)
		return SCE_KERNEL_ERROR_ERROR;
	res = FixedHeapCommonLeakCheckStart(&s_guidEntryHeapLeakDesc, &ctx);
	if (res >= 0) {
		ctx->hookArg = ctx;
		ctx->allocHook = GUIDEntryHeapAllocHook;
		ctx->uid = 0x10003;
		ctx->freeHook = GUIDEntryHeapFreeHook;
		res = sceUIDEntryHeapSetHook(0x10003, &ctx->hookArg);
		if (res >= 0)
			kdata->guidEntryLeak = ctx;
	}
	return res;
}

/* 0x81008070 */
static int ProcFixedHeapLeakCheckStart(int cmd)
{
	const SdbgpKShowTarget *t;
	SdbgpKData *kdata;
	SdbgpFixedHeapLeakCtx *ctx;
	int res, i;
	void *targetHeap;

	if (findTarget((uint8_t)cmd, &t) < 0)
		return SCE_KERNEL_ERROR_ERROR;
	kdata = GetKData();
	if (kdata == NULL || kdata->procFixedHeapLeak != NULL)
		return SCE_KERNEL_ERROR_ERROR;
	res = FixedHeapCommonLeakCheckStart(&s_fixedHeapLeakDesc, &ctx);
	if (res < 0)
		return res;
	kdata->processCount = SceProcessmgrForDriver_BA0A06A4(5, kdata->process,
							      sizeof(kdata->process),
							      (SceSize *)&kdata->processCount);
	targetHeap = NULL;
	for (i = 0; i < kdata->processCount; i++) {
		uint32_t j;

		if (strncmp("SceKernelProcess", kdata->process[i].name, 16) == 0) {
			SCE_DBG_PRINTF_LEVEL(1, 11, 1817, "Skip SceKernelProcess\n");
			continue;
		}
		if (strncmp("SceShell", kdata->process[i].name, 8) == 0) {
			SCE_DBG_PRINTF_LEVEL(1, 11, 1819, "SceShell\n");
			continue;
		}
		SCE_DBG_PRINTF_LEVEL(1, 11, 1821, "check [%s]\n", kdata->process[i].name);
		if (kdata->process[i].objectHeap[0] == NULL)
			continue;
		kdata->objectHeap[0].size = sizeof(ObjectHeapInfo);
		res = SceSysmemForKernel_DBEACE2C(kdata->process[i].objectHeap[0],
						  &kdata->objectHeap[0]);
		if (res < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 11, 1827,
				"sceKernelObjectHeapGetInfo failed 0x%08x\n", res);
			return res;
		}
		for (j = 0; j < kdata->objectHeap[0].count; j++) {
			if (kdata->objectHeap[0].sizeClasses[j].size == 128) {
				targetHeap = kdata->objectHeap[0].sizeClasses[j].heap;
				if (targetHeap != NULL)
					break;
			}
		}
	}
	if (targetHeap == NULL) {
		SCE_DBG_PRINTF_LEVEL(1, 11, 1843, "There is no process FixedHeap\n");
		return SCE_KERNEL_ERROR_ERROR;
	}
	ctx->allocHook = FixedHeapAllocHook;
	ctx->uid = -1;
	ctx->heapPtr = targetHeap;
	ctx->hookArg = ctx;
	ctx->freeHook = FixedHeapFreeHook;
	res = SceSysmemForKernel_811C452B(targetHeap, &ctx->hookArg);
	if (res < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 11, 1856,
			"sceKernelFixedHeapSetHookByPtr failed 0x%08x\n", res);
		return res;
	}
	kdata->procFixedHeapLeak = ctx;
	return res;
}

/* 0x81008278 */
static int LeakCheckMemBlockMCStart(int cmd)
{
	int res;
	int i;

	(void)cmd;
	s_memBlockMCCtx.gen = 0;
	res = LogStartPhase1(&s_memBlockMCCtx);
	if (res >= 0) {
		SdbgpMemBlockMCEntry *entries = s_memBlockMCCtx.entries;

		for (i = 0; i < s_memBlockMCCtx.max; i++)
			entries[i].block = NULL;
		s_memBlockMCHooks[0] = MemBlockMCIncHook;
		s_memBlockMCHooks[1] = MemBlockMCDecHook;
		s_memBlockMCCtx.gen++;
		s_memBlockMCCtx.lock = 0;
		SceSysmemForKernel_F57A97A0(s_memBlockMCHooks);
		return 0;
	}
	return res;
}

/* 0x810082d4 */
static int GUIDEntryHeapShowLog(uint32_t index, const SdbgpFixedHeapLogEntry *entry)
{
	UIDEntry *obj = entry->obj;
	const char *name = NULL;
	const char *stateStr = NULL;
	uint32_t attr = 0, uid = 0, extra = 0;

	if (obj != NULL) {
		name = SceSysmemForKernel_5FD15C6E(obj);
		attr = SceSysmemForKernel_17F12DCE(obj);
		uid = SceSysmemForKernel_F3DB2D71(obj);
		extra = SceSysmemForKernel_797C858B(obj);
		switch (attr & 0xf00000) {
		case 0x100000:
			stateStr = "Active Object";
			break;
		case 0x200000:
			stateStr = "Active Chain";
			break;
		case 0x400000:
			stateStr = "Zombie Object";
			break;
		case 0x800000:
			stateStr = "Zombie Chain";
			break;
		default:
			stateStr = "Unknown";
			break;
		}
	}
	return SCE_PRINTF_WITH_INFO(0, 1445,
		"\t%4d: 0x%08x: %p[0x%08x], %p[%-32s], %p, 0x%08x [%s],0x%08x,0x%08x\n",
		entry->gen, index, entry->heap,
		*((const uint16_t *)entry->heap + 16),
		obj, name, entry->lr, attr, stateStr, uid, extra);
}

/* 0x8100839c */
static int SMBShowStatus(int cmd)
{
	SdbgpSMBInfo info;
	int res, i;

	(void)cmd;
	info.size = sizeof(info);
	res = SceSysmemForKernel_BD625136(&info);
	if (res >= 0) {
		for (i = 0; i < 9; i++) {
			SCE_PRINTF_WITH_INFO(0, 1169,
				"\t%d: 0x%08x: Free %4d/%4d: FreeMin %4d\n",
				i, s_smbSizes[i], info.entry[i].free,
				info.entry[i].total, info.entry[i].freeMin);
		}
		res = 0;
	}
	return res;
}

static int ObjectHeapGetFixedHeapInfoAll(ObjectHeapInfo *objInfo,
					 SceKernelFixedHeapInfo *fixedInfo)
{
	int res;
	uint32_t i;

	SCE_PRINTF_WITH_INFO_FUNC(0, 908, "ObjectHeapGetFixedHeapInfoAll",
		"\tObjectSize, tHeapCount / tItemCount  FreeCount-  UsedMax---\n");
	for (i = 0; i < objInfo->count; i++) {
		SceSize size = objInfo->sizeClasses[i].size;

		if (size == 0 || size == (SceSize)-1)
			continue;
		fixedInfo->size = sizeof(*fixedInfo);
		res = sceKernelGetFixedHeapInfoByPointer(objInfo->sizeClasses[i].heap, fixedInfo);
		if (res < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 11, 922, "ObjectHeapGetFixedHeapInfoAll",
				"sceKernelGetFixedHeapInfoByPointer failed 0x%08x\n", res);
			return res;
		}
		SCE_PRINTF_WITH_INFO_FUNC(0, 928, "ObjectHeapGetFixedHeapInfoAll",
			"\t0x%08x, 0x%08x / 0x%08x  0x%08x  0x%08x\n",
			fixedInfo->itemSize, fixedInfo->totalSize,
			fixedInfo->perPage, fixedInfo->freeCount,
			fixedInfo->peakCount);
		SCE_PRINTF_WITH_INFO_FUNC(0, 938, "ObjectHeapGetFixedHeapInfoAll",
			"\t\tpItem=0x%08x, 0x%04x/0x%04x, nListCount=0x%08x, %2d/%2d, %2d/%2d, %2d/%2d, %2d/%2d\n",
			fixedInfo->cursor, fixedInfo->usedInPage,
			0x1000u / fixedInfo->itemSize, fixedInfo->listCount,
			fixedInfo->caches[0].count, fixedInfo->caches[0].max,
			fixedInfo->caches[1].count, fixedInfo->caches[1].max,
			fixedInfo->caches[2].count, fixedInfo->caches[2].max,
			fixedInfo->caches[3].count, fixedInfo->caches[3].max);
	}
	return 0;
}

/* 0x81008418 */
static int ObjectHeapSubbudgetUserProcess(struct ObjectHeap *heap, ObjectHeapInfo *objInfo,
					  SceKernelFixedHeapInfo *fixedInfo)
{
	int res;

	objInfo->size = sizeof(*objInfo);
	SCE_DBG_PRINTF_LEVEL(1, 11, 953, "->sceKernelObjectHeapGetInfo(%p,%p)\n", heap, objInfo);
	res = SceSysmemForKernel_DBEACE2C(heap, objInfo);
	if (res < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 11, 956, "sceKernelObjectHeapGetInfo failed 0x%08x\n", res);
		return res;
	}
	res = ObjectHeapGetFixedHeapInfoAll(objInfo, fixedInfo);
	if (res < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 11, 962,
			"ObjectHeapGetFixedHeapInfoAll failed 0x%08x\n", res);
		return res;
	}
	return 0;
}

/* 0x81008594 */
static int GUIDEntryHeapShowStatus(int cmd)
{
	SdbgpKData *kdata;

	(void)cmd;
	kdata = GetKData();
	kdata->uidEntryHeap.size = sizeof(kdata->uidEntryHeap);
	if (sceKernelUIDEntryHeapGetInfo(0x10003, &kdata->uidEntryHeap) < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 11, 452, "KernelUIDHeapUpdateInfo",
			"Cannot get info\n");
	}
	SCE_PRINTF_WITH_INFO(0, 745, "GUID Entry Heap status\n");
	SCE_PRINTF_WITH_INFO(0, 746, "\tobject size:                0x%08x\n",
		kdata->uidEntryHeap.itemSize);
	SCE_PRINTF_WITH_INFO(0, 747, "\ttotal page size:            0x%08x\n",
		kdata->uidEntryHeap.totalSize);
	SCE_PRINTF_WITH_INFO(0, 748, "\tnumber of item:             0x%08x\n",
		kdata->uidEntryHeap.totalSize / kdata->uidEntryHeap.itemSize);
	SCE_PRINTF_WITH_INFO(0, 749, "\tnumber of initialized item: 0x%08x\n",
		kdata->uidEntryHeap.perPage);
	SCE_PRINTF_WITH_INFO(0, 750, "\tnumber of free item:        0x%08x\n",
		kdata->uidEntryHeap.freeCount);
	SCE_PRINTF_WITH_INFO(0, 751, "\tnumber of allocated item:   0x%08x\n",
		kdata->uidEntryHeap.perPage - kdata->uidEntryHeap.freeCount);
	return 0;
}

/* 0x81008680 */
static int ABTimeStop(uint32_t idx)
{
	uint64_t now, diff;
	SdbgpABTimeEntry *e;

	if (idx > 15)
		return SCE_KERNEL_ERROR_ERROR;
	now = sceKernelGetSystemTimeWide();
	e = &s_abTime[idx];
	diff = now - e->start;
	e->count++;
	if (diff < e->min)
		e->min = diff;
	if (e->max < diff)
		e->max = diff;
	e->sum += diff;
	return 0;
}

/* 0x810086ec */
static int ABTimeStart(uint32_t idx)
{
	if (idx > 15)
		return SCE_KERNEL_ERROR_ERROR;
	s_abTime[idx].start = sceKernelGetSystemTimeWide();
	return 0;
}

/* 0x8100871c */
static int FixedHeapCommonFreeHook(SdbgpFixedHeapLeakCtx *ctx, void *obj)
{
	int intr;
	SdbgpFixedHeapLogEntry *e;
	uint32_t i;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&ctx->lock);
	e = ctx->entries;
	for (i = 0; i < ctx->max; i++, e++) {
		if (e->obj == obj) {
			e->obj = NULL;
			break;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ctx->lock, intr);
	return 0;
}

/* 0x81008764 */
static int FixedHeapFreeHook(SdbgpFixedHeapLeakCtx *ctx, void *heap, void *obj)
{
	(void)heap;
	return FixedHeapCommonFreeHook(ctx, obj);
}

/* 0x81008770 */
static int GUIDEntryHeapFreeHook(SdbgpFixedHeapLeakCtx *ctx, void *heap, void *obj)
{
	(void)heap;
	return FixedHeapCommonFreeHook(ctx, obj);
}

/* 0x8100877c */
static int NameHeapLogShowLog(uint32_t index, const void *entry)
{
	const SdbgpNameHeapLogEntry *e = (const SdbgpNameHeapLogEntry *)entry;
	uintptr_t textBase;
	char modName[28];

	SCE_PRINTF_WITH_INFO(0, 2373, "\t%4d: 0x%08x, 0x%08x, %p[%-32s], %p",
		e->gen, index, e->op, e->name, e->name, (const void *)e->lr);
	if (SceModulemgrForKernel_5564A860(SDBGP_KERNEL_PROCESS_ID, e->lr, NULL,
					   (void **)&textBase, modName) < 0)
		return SCE_PRINTF_WITH_INFO(0, 2379, "\n");
	return SCE_PRINTF_WITH_INFO(0, 2382, "[%-32s]+0x%08x\n", modName, e->lr - textBase);
}

/* 0x81008818 */
static int PhyMemPartLeakShow(int filter)
{
	uint32_t gen;

	if (s_phyMemPartLeakCtx.gen == 0)
		return SCE_KERNEL_ERROR_ERROR;
	SCE_PRINTF_WITH_INFO(0, 3095, "Leak check PhyMemPart\n");
	if (s_phyMemPartLeakCtx.entries != NULL) {
		for (gen = 0; gen <= s_phyMemPartLeakCtx.gen; gen++) {
			uint32_t i;
			int first = 1;
			const SdbgpPhyMemPartEntry *e = s_phyMemPartLeakCtx.entries;

			for (i = 0; i < s_phyMemPartLeakCtx.max; i++, e++) {
				if (e->page == NULL || e->gen != gen)
					continue;
				if (filter == 1 &&
				    strncmp("ScePhyMemPartGameCDialog",
					    (const char *)(e->part + 140), 24) != 0)
					continue;
				if (first) {
					SCE_PRINTF_WITH_INFO(0, 3112,
						"\tgen-: index-----: [--------------------------------], pPhyPage--, psize-----, lr---------\n");
				}
				first = 0;
				SCE_PRINTF_WITH_INFO(0, 3119,
					"\t%4d: 0x%08x: [%-32s], %p, 0x%08x, %p\n",
					e->gen, i, (const char *)(e->part + 140),
					e->page, e->psize, e->lr);
			}
		}
	}
	SCE_PRINTF_WITH_INFO_FUNC(0, 3137, "PhyMemPartListShow", "Leak check PhyMemPartList\n");
	if (s_phyMemPartLeakCtx.gen != 0 && s_phyMemPartLeakCtx.partEntries != NULL) {
		for (gen = 0; gen <= s_phyMemPartLeakCtx.gen; gen++) {
			uint32_t i;
			int first = 1;
			const SdbgpPhyMemPartListEntry *e = s_phyMemPartLeakCtx.partEntries;

			for (i = 0; i < s_phyMemPartLeakCtx.partMax; i++, e++) {
				if (e->part == 0 || e->gen != gen)
					continue;
				if (first) {
					SCE_PRINTF_WITH_INFO_FUNC(0, 3150, "PhyMemPartListShow",
						"\tgen-: index-----: [--------------------------------], lr---------\n");
				}
				first = 0;
				SCE_PRINTF_WITH_INFO_FUNC(0, 3156, "PhyMemPartListShow",
					"\t%4d: 0x%08x: [%-32s], %p\n",
					e->gen, i, (const char *)(e->part + 140), e->lr);
			}
		}
	}
	s_phyMemPartLeakCtx.gen++;
	return 0;
}

/* 0x810089e4 */
static int LeakCheckMemBlockMCShowLog(uint32_t index, const void *entry)
{
	const SdbgpMemBlockMCEntry *e = (const SdbgpMemBlockMCEntry *)entry;
	const char *blockName = ((const char *const *)e->block)[11];
	uintptr_t textBase;
	char modName[28];

	SCE_PRINTF_WITH_INFO(0, 2132, "\t%4d: 0x%08x: %p, %p[%-32s], 0x%08x, %p",
		e->gen, index, e->part, e->block, blockName, e->count, e->lr);
	if (SceModulemgrForKernel_5564A860(SDBGP_KERNEL_PROCESS_ID, (uintptr_t)e->block, NULL,
					   (void **)&textBase, modName) < 0)
		return SCE_PRINTF_WITH_INFO(0, 2138, "\n");
	return SCE_PRINTF_WITH_INFO(0, 2141, "[%-32s]+0x%08x\n", modName,
		(uintptr_t)e->block - textBase);
}

/* 0x81008a90 */
static int ProcFixedHeapLeakCheck(int cmd)
{
	SdbgpKData *kdata;

	(void)cmd;
	SCE_DBG_PRINTF_LEVEL(1, 11, 1869, "()\n");
	kdata = GetKData();
	if (kdata == NULL) {
		SCE_DBG_PRINTF_LEVEL(1, 11, 1875, "There is no pData\n");
		return SCE_KERNEL_ERROR_ERROR;
	}
	if (kdata->procFixedHeapLeak != NULL)
		return FixedHeapCommonLeakCheck(kdata->procFixedHeapLeak);
	SCE_DBG_PRINTF_LEVEL(1, 11, 1880, "There is no pLeakCheckProcFixedHeap\n");
	return SCE_KERNEL_ERROR_ERROR;
}

/* 0x81008aec */
static int NameHeapLogWriteLines(SceUID fd, const char *const *lines)
{
	int res = fd;

	for (; *lines != NULL; lines++) {
		int len = snprintf(s_nameHeapSaveBuf, sizeof(s_nameHeapSaveBuf), "%s", *lines);

		res = sceIoWrite(fd, s_nameHeapSaveBuf, len);
	}
	return res;
}

/* 0x81008b28 */
static int CpuMaskShow(int cmd)
{
	(void)cmd;
	if (sdbgpCpuMaskLog.shown != 0) {
		sdbgpCpuMaskLog.shown++;
		if (sdbgpCpuMaskLog.count != 0) {
			SdbgpCpuMaskEntry *e = sdbgpCpuMaskLog.entries;
			SceSize i;

			SCE_PRINTF_WITH_INFO(0, 2762, "\n");
			SCE_PRINTF_WITH_INFO(0, 2763, "\t[ CPU Intr Mask %d ]\n",
				sdbgpCpuMaskLog.shown);
			SCE_PRINTF_WITH_INFO(0, 2764,
				"\tusec----  C  Suspend LR  Resume LR-  Module+offset\n");
			for (i = 0; i < sdbgpCpuMaskLog.count; i++, e++) {
				SdbgpCpuMaskEntry *other;
				SceSize j;
				uintptr_t textBase;
				char modName[28];
				int res;

				if (e->duration == 0)
					continue;
				other = &sdbgpCpuMaskLog.entries[i + 1];
				for (j = i + 1; j < sdbgpCpuMaskLog.count; j++, other++) {
					if (other->duration != 0 &&
					    other->lockedCaller == e->lockedCaller &&
					    e->resumeCaller == other->resumeCaller) {
						if (other->duration > e->duration)
							goto next_entry;
						other->duration = 0;
					}
				}
				res = SceModulemgrForKernel_5564A860(SDBGP_KERNEL_PROCESS_ID,
								     e->lockedCaller, NULL,
								     (void **)&textBase, modName);
				SCE_PRINTF_WITH_INFO(0, 2797, "\t%8d  %d  0x%08X  0x%08X  ",
					e->duration, e->core, e->lockedCaller,
					(uintptr_t)e->resumeCaller);
				if (res < 0) {
					SCE_PRINTF_WITH_INFO(0, 2799, "\n");
				} else {
					SCE_PRINTF_WITH_INFO(0, 2804, "[%-32s]+0x%08x 0x%08x\n",
						modName, e->lockedCaller - textBase,
						(uintptr_t)e->resumeCaller - textBase);
				}
next_entry:
				;
			}
		}
	} else {
		sdbgpCpuMaskLog.memblock = sceKernelAllocMemBlockForKernel(
			"SceCpuMaskLog", SceDeci4pSDfMgrForKernel_35238B8A(), 0x10000, NULL);
		sceKernelGetMemBlockBase(sdbgpCpuMaskLog.memblock,
					 (void **)&sdbgpCpuMaskLog.entries);
		sdbgpCpuMaskLog.hook.notify = sdbgpCpuMaskLogAdd;
		sdbgpCpuMaskLog.max = 3276;
		SceCpuForKernel_B331AB16(&sdbgpCpuMaskLog.hook);
		sdbgpCpuMaskLog.shown++;
	}
	return 0;
}

/* 0x81008d04 */
static int NameHeapLogStop(int cmd)
{
	(void)cmd;
	if (s_nameHeapLogCtx.log.gen == 0)
		return SCE_KERNEL_ERROR_ERROR;
	s_nameHeapLogCtx.log.entries = NULL;
	SceSysmemForKernel_6DBDA03B(NULL);
	sceKernelFreeMemBlock(s_nameHeapLogCtx.log.memblock);
	s_nameHeapLogCtx.log.entries = NULL;
	s_nameHeapLogCtx.log.gen = 0;
	return 0;
}

/* 0x81008d34 */
static int PhyMemPartLogStart(int cmd)
{
	SceUID mb;

	(void)cmd;
	if (s_phyMemPartLogCtx.gen != 0)
		return SCE_KERNEL_ERROR_ERROR;
	mb = sceKernelAllocMemBlockForKernel("SceKernelLogPhyMemPart",
					     SceDeci4pSDfMgrForKernel_35238B8A(),
					     s_phyMemPartLogCtx.size, NULL);
	if (mb < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 11, 3563, "Alloc Memory Block failed (0x%08X)\n", mb);
		SCE_DBG_ASSERT_FAIL(3564);
		return mb;
	}
	sceKernelGetMemBlockBase(mb, (void **)&s_phyMemPartLogCtx.entries);
	s_phyMemPartLogCtx.memblock = mb;
	s_phyMemPartLogCtx.lock = 0;
	s_phyMemPartLogCtx.gen++;
	s_phyMemPartLogCtx.hooks[0] = PhyMemPartLogAllocHook;
	s_phyMemPartLogCtx.hooks[1] = PhyMemPartLogFreeHook;
	SceSysmemForKernel_93D0A183(s_phyMemPartLogCtx.hooks);
	return 0;
}

/* 0x81008ddc */
static int LeakCheckMemBlockMCStop(int cmd)
{
	(void)cmd;
	if (s_memBlockMCCtx.gen == 0)
		return SCE_KERNEL_ERROR_ERROR;
	s_memBlockMCCtx.entries = NULL;
	SceSysmemForKernel_2C7B8D65(NULL);
	sceKernelFreeMemBlock(s_memBlockMCCtx.memblock);
	s_memBlockMCCtx.entries = NULL;
	s_memBlockMCCtx.gen = 0;
	return 0;
}

/* 0x81008e0c */
static int MemBlockShowStatus(int cmd)
{
	SdbgpKData *kdata;
	int i;

	(void)cmd;
	kdata = GetKData();
	kdata->processCount = SceProcessmgrForDriver_BA0A06A4(5, kdata->process,
							      sizeof(kdata->process),
							      (SceSize *)&kdata->processCount);
	for (i = 0; i < kdata->processCount; i++) {
		int j, res;
		uint32_t p;

		SCE_DBG_PRINTF_LEVEL(1, 0, 1063, "\t0x%08x: [%-32s]\n",
			kdata->process[i].pid, kdata->process[i].name);
		if (strncmp("SceKernelProcess", kdata->process[i].name, 16) == 0)
			continue;
		s_procMemBlocks.res = scePUIDGetUIDVectorByClass(
			kdata->process[i].pid, SceSysmemForKernel_86681B64(), 5,
			s_procMemBlocks.puids, 256, (SceSize *)&s_procMemBlocks.count);
		if (s_procMemBlocks.res >= 0 && s_procMemBlocks.count > 0) {
			for (j = 0; j < s_procMemBlocks.count; j++)
				s_procMemBlocks.guids[j] = scePUIDtoGUID(kdata->process[i].pid,
									 s_procMemBlocks.puids[j]);
		}
		SceDeci4pSDfMgrForKernel_8A3B06F2(&kdata->addressSpace[i], 0,
						  sizeof(kdata->addressSpace[i]));
		kdata->addressSpace[i].size = sizeof(kdata->addressSpace[i]);
		res = sceKernelGetAddressSpaceInfo(kdata->process[i].addressSpace,
						   &kdata->addressSpace[i]);
		if (res < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 11, 1074,
				"AddressSpaceUpdateInfo failed 0x%08x\n", res);
			return res;
		}
		for (p = 0; p < kdata->addressSpace[i].partitionCount; p++) {
			PartitionInfo *part = &kdata->addressSpace[i].partition[p];
			uint32_t k;
			int count = 0;

			for (k = 0; k < p; k++) {
				if (kdata->addressSpace[i].partition[k].partition == part->partition)
					goto next_part;
			}
			if (SceSysmemForKernel_AD19285A((TinyPartition *)part->partition,
							part->vbase, part->vbase + part->vsize,
							5, s_memBlockInfoList, 2048,
							(SceUInt32 *)&count) >= 0 &&
			    count > 0) {
				int m;

				for (m = 0; m < count; m++) {
					MemBlockInfoEx *mb = &s_memBlockInfoList[m];
					SceUID puid = -1;

					if ((SceSysmemForKernel_48D2E408(mb->details.type) & 0xf0) == 0)
						continue;
					for (j = 0; j < s_procMemBlocks.count; j++) {
						if (s_procMemBlocks.guids[j] == mb->details.guid) {
							puid = s_procMemBlocks.puids[j];
							break;
						}
					}
					SCE_DBG_PRINTF_LEVEL(1, 0, 1124,
						"\t0x%08x: 0x%08x+0x%08x [%s]\n",
						puid, mb->details.vbase, mb->details.size, mb->details.name);
				}
			}
next_part:
			;
		}
	}
	return 0;
}

static inline int ObjectHeapUserProcess(SdbgpKData *kdata, SceKernelProcessInfo *proc)
{
	int res;

	res = ObjectHeapSubbudgetUserProcess(proc->objectHeap[0], &kdata->objectHeap[0],
					     &kdata->procFixedHeap[0]);
	if (res < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 11, 982, "ObjectHeapUserProcess",
			"ObjectHeapSubbudgetUserProcess failed 0x%08x\n", res);
		return res;
	}
	if (proc->objectHeap[1] != NULL) {
		res = ObjectHeapSubbudgetUserProcess(proc->objectHeap[1], &kdata->objectHeap[1],
						     &kdata->procFixedHeap[1]);
		if (res < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 11, 991, "ObjectHeapUserProcess",
				"ObjectHeapSubbudgetUserProcess failed 0x%08x\n", res);
			return res;
		}
	}
	return 0;
}

/* 0x8100909c */
static int ObjectHeapShowStatus(int cmd)
{
	SdbgpKData *kdata;
	SceKernelProcessInfo *proc;
	int i, res;

	(void)cmd;
	kdata = GetKData();
	proc = kdata->process;
	kdata->processCount = SceProcessmgrForDriver_BA0A06A4(5, proc,
							      sizeof(kdata->process),
							      (SceSize *)&kdata->processCount);
	for (i = 0; i < kdata->processCount; i++) {
		if (proc->pid != 0) {
			if (strncmp("SceKernelProcess", proc->name, 16) == 0) {
				SCE_DBG_PRINTF_LEVEL(1, 11, 1023, "Skip SceKernelProcess\n");
			} else if (strncmp("SceShell", proc->name, 8) == 0) {
				SCE_DBG_PRINTF_LEVEL(1, 11, 1025, "SceShell\n");
			} else {
				SCE_PRINTF_WITH_INFO(0, 1027, "0x%08x: [%-32s]\n",
					proc->pid, proc->name);
				res = ObjectHeapUserProcess(kdata, proc);
				if (res < 0) {
					SCE_DBG_PRINTF_LEVEL(1, 11, 1030,
						"ObjectHeapUserProcess failed 0x%08x\n", res);
					return res;
				}
			}
			proc++;
		}
	}
	return 0;
}

static inline void KernelFixedHeapUpdateInfo(SceUID uid, SceSize itemSize,
					     SceKernelFixedHeapInfo *info)
{
	int res;

	info->size = sizeof(*info);
	if (sceKernelGetKernelFixedHeap(itemSize) != NULL) {
		res = sceKernelGetFixedHeapInfo(uid, info);
		if (res < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 11, 472, "KernelFixedHeapUpdateInfo",
				"sceKernelGetFixedHeapInfoByPointer failed 0x%08x\n", res);
		}
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 11, 467, "KernelFixedHeapUpdateInfo",
			"sceKernelGetKernelFixedHeap(0x%08x) failed\n", itemSize);
	}
}

/* 0x810091dc */
static int FixedHeapShowStatus(int cmd)
{
	SdbgpKData *kdata;
	const SdbgpKShowTarget *t = NULL;
	SceKernelFixedHeapInfo *info;

	kdata = GetKData();
	findTarget((uint8_t)cmd, &t);
	info = &kdata->fixedHeap[t->index];
	KernelFixedHeapUpdateInfo(t->uid, t->itemSize, info);
	SCE_PRINTF_WITH_INFO(0, 769, "GUID Entry Heap status\n");
	SCE_PRINTF_WITH_INFO(0, 770, "\tobject size:                0x%08x\n", info->itemSize);
	SCE_PRINTF_WITH_INFO(0, 771, "\ttotal page size:            0x%08x\n", info->totalSize);
	SCE_PRINTF_WITH_INFO(0, 772, "\tnumber of item:             0x%08x\n",
		info->totalSize / info->itemSize);
	SCE_PRINTF_WITH_INFO(0, 773, "\tnumber of initialized item: 0x%08x\n", info->perPage);
	SCE_PRINTF_WITH_INFO(0, 774, "\tnumber of free item:        0x%08x\n", info->freeCount);
	SCE_PRINTF_WITH_INFO(0, 775, "\tnumber of allocated item:   0x%08x\n",
		info->perPage - info->freeCount);
	return 0;
}

/* 0x81009328 */
int NameHeapLogSaveCore(void)
{
	SceUID fd;
	int intr;
	uint32_t i;
	const char *lines1[] = {
		"#include <kernel.h>\n",
		"#include <deci4p_sdbgp_test.h>\n",
		"typedef struct NameHeapLogSave {\n",
		"    SceUInt32  op;\n",
		"    SceUInt32  core;\n",
		"    const char *name;\n",
		"    SceUInt32  flags;\n",
		"} NameHeapLogSave;\n",
		"NameHeapLogSave gNameHeapLogSave[] = {\n",
		NULL,
	};
	const char *lines2[] = {
		"    { .op=SCE_TEST_LOG_OP_FREE, .name=SCE_NULL, .flags=0x0 }\n",
		"};\n",
		NULL,
	};

	if (s_nameHeapLogCtx.log.gen == 0)
		return SCE_KERNEL_ERROR_ERROR;
	SCE_DBG_PRINTF_LEVEL(1, 6, 2593, "->sceIoOpen()\n");
	fd = sceIoOpen("host0:SceNameHeapLog.c", 0x603, 0600);
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_nameHeapLogCtx.log.lock);
	SceSysmemForKernel_6DBDA03B(NULL);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_nameHeapLogCtx.log.lock, intr);
	NameHeapLogWriteLines(fd, lines1);
	if (s_nameHeapLogCtx.log.entries != NULL && s_nameHeapLogCtx.log.max != 0) {
		const SdbgpNameHeapLogEntry *e = s_nameHeapLogCtx.log.entries;

		for (i = 0; i < s_nameHeapLogCtx.log.max; i++, e++) {
			if (e->op != -1) {
				int len = snprintf(s_nameHeapSaveBuf, sizeof(s_nameHeapSaveBuf),
					"    { .op = 0x%08x, .core=0x%08x, .name=\"%s\", .flags=0x%08x },\n",
					e->op, e->core, e->name, e->flags);

				sceIoWrite(fd, s_nameHeapSaveBuf, len);
			}
		}
	}
	NameHeapLogWriteLines(fd, lines2);
	SCE_DBG_PRINTF_LEVEL(1, 11, 2614, "->SceIoClose\n");
	sceIoClose(fd);
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_nameHeapLogCtx.log.lock);
	SceSysmemForKernel_6DBDA03B(s_nameHeapLogCtx.hooks);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_nameHeapLogCtx.log.lock, intr);
	SCE_DBG_PRINTF_LEVEL(1, 11, 2621, "<-SCE_OK\n");
	return 0;
}

/* 0x810094a4 */
static int PhyMemPartShowStatus(int cmd)
{
	uint8_t lo = (uint8_t)cmd;
	const SdbgpKShowTarget *t;
	PhyMemPart *part;
	SdbgpKData *kdata;
	int i;

	if ((lo & 0xf0) == 0x70) {
		if (findTarget(lo, &t) >= 0) {
			part = (PhyMemPart *)SceProcessmgrForKernel_923AF3CC(t->uid);
			if (part != NULL) {
				sceKernelShowPhyMemPart(part);
				return 0;
			}
		}
	}
	kdata = GetKData();
	kdata->processCount = SceProcessmgrForDriver_BA0A06A4(5, kdata->process,
							      sizeof(kdata->process),
							      (SceSize *)&kdata->processCount);
	if (kdata->processCount > 0) {
		SceKernelProcessInfo *proc = kdata->process;
		AddressSpaceInfo *as = kdata->addressSpace;

		for (i = 0; i < kdata->processCount; i++, proc++, as++) {
			SceDeci4pSDfMgrForKernel_8A3B06F2(as, 0, sizeof(*as));
			as->size = sizeof(*as);
			sceKernelGetAddressSpaceInfo(proc->addressSpace, as);
		}
		as = kdata->addressSpace;
		for (i = 0; i < kdata->processCount; i++, as++) {
			uint32_t j;

			for (j = 0; j < as->phyMemPartCount; j++) {
				if (as->phyMemPart[j] != NULL)
					sceKernelShowPhyMemPart((PhyMemPart *)as->phyMemPart[j]);
			}
		}
	}
	return 0;
}

static inline void ShowResCountItem(const char *cat, const char *sub, uint32_t cur,
				    uint32_t max, uint32_t lim)
{
	SCE_PRINTF_WITH_INFO_FUNC(0, 794, "ShowResCount",
		"\t\t[%-16s/%-16s] C=0x%08x/M=0x%08x/L=0x%08x\n",
		cat, sub, cur, max, lim);
}

/* 0x81009584 */
static int ShowResCount(int cmd)
{
	SdbgpKData *kdata;
	int i;

	(void)cmd;
	kdata = GetKData();
	kdata->processCount = SceProcessmgrForDriver_BA0A06A4(5, kdata->process,
							      sizeof(kdata->process),
							      (SceSize *)&kdata->processCount);
	for (i = 0; i < kdata->processCount; i++) {
		uint32_t *obj;

		SCE_PRINTF_WITH_INFO_FUNC(0, 876, "ProcessShowStatus",
			"\t0x%08x: [%-32s]\n", kdata->process[i].pid, kdata->process[i].name);
		if (sceGUIDGetObject(kdata->process[i].pid, (void **)&obj) >= 0) {
			ShowResCountItem("thread", "Main", obj[182], obj[183], 0);
			ShowResCountItem("thread", "CDialog", obj[184], obj[185], 0);
			ShowResCountItem("Sync", "Main", obj[215], obj[216], 0);
			ShowResCountItem("Sync", "CDialog", obj[217], obj[218], 0);
			ShowResCountItem("UserMemBlock", "Main", obj[274], obj[275], obj[273]);
			ShowResCountItem("UserMemBlock", "CDialog", obj[278], obj[279], obj[277]);
			ShowResCountItem("Callback", "-", obj[219], obj[220], 0);
			ShowResCountItem("Cond", "-", obj[223], obj[224], 0);
			ShowResCountItem("EventFlag", "-", obj[225], obj[226], 0);
			ShowResCountItem("LwConds", "-", obj[227], obj[228], 0);
			ShowResCountItem("LwMutex", "-", obj[229], obj[230], 0);
			ShowResCountItem("MsgPipe", "-", obj[231], obj[232], 0);
			ShowResCountItem("Mutex", "-", obj[233], obj[234], 0);
			ShowResCountItem("RWLocks", "-", obj[235], obj[236], 0);
			ShowResCountItem("Semas", "-", obj[237], obj[238], 0);
			ShowResCountItem("SimpleEvent", "-", obj[239], obj[240], 0);
			ShowResCountItem("Timer", "-", obj[241], obj[242], 0);
			ShowResCountItem("FastMutex", "-", obj[243], obj[244], 0);
			ShowResCountItem("Alarm", "-", obj[245], obj[246], 0);
			ShowResCountItem("Delay", "-", obj[247], obj[248], 0);
		}
	}
	return 0;
}

#if FW_DEBUG
/* 0x810099e8 */
int KernelUtilsFixedHeapLeakCheckStart(SceUID uid, void **ctx)
{
	return FixedHeapLeakCheckStartByUid(uid, (SdbgpFixedHeapLeakCtx **)ctx);
}

/* 0x810099f0 */
int KernelUtilsFixedHeapLeakCheck(void *ctx)
{
	return FixedHeapCommonLeakCheck((SdbgpFixedHeapLeakCtx *)ctx);
}
#endif

SdbgpKShowCmd s_kshowCmds[53] = {
	{ 1, 0x0000, help },
	{ 2, 0x1010, GUIDEntryHeapShowStatus },
	{ 2, 0x1022, FixedHeapShowStatus },
	{ 2, 0x1023, FixedHeapShowStatus },
	{ 2, 0x1024, FixedHeapShowStatus },
	{ 2, 0x1025, FixedHeapShowStatus },
	{ 2, 0x1026, FixedHeapShowStatus },
	{ 2, 0x1030, ShowResCount },
	{ 2, 0x1040, ObjectHeapShowStatus },
	{ 2, 0x1050, MemBlockShowStatus },
	{ 2, 0x1060, SMBShowStatus },
	{ 2, 0x1070, PhyMemPartShowStatus },
	{ 2, 0x2110, GUIDEntryHeapLeakCheckStart },
	{ 2, 0x2122, FixedHeapLeakCheckStart },
	{ 2, 0x2123, FixedHeapLeakCheckStart },
	{ 2, 0x2124, FixedHeapLeakCheckStart },
	{ 2, 0x2125, FixedHeapLeakCheckStart },
	{ 2, 0x2126, FixedHeapLeakCheckStart },
	{ 2, 0x2151, LeakCheckMemBlockMCStart },
	{ 2, 0x2170, PhyMemPartLeakCheckStart },
	{ 2, 0x2180, PhyMemLowLeakCheckStart },
	{ 2, 0x21b4, ProcFixedHeapLeakCheckStart },
	{ 2, 0x2210, GUIDEntryHeapLeakCheckStop },
	{ 2, 0x2222, FixedHeapLeakCheckStop },
	{ 2, 0x2223, FixedHeapLeakCheckStop },
	{ 2, 0x2224, FixedHeapLeakCheckStop },
	{ 2, 0x2225, FixedHeapLeakCheckStop },
	{ 2, 0x2226, FixedHeapLeakCheckStop },
	{ 2, 0x2251, LeakCheckMemBlockMCStop },
	{ 2, 0x2270, PhyMemPartLeakCheckStop },
	{ 2, 0x2280, PhyMemLowLeakCheckStop },
	{ 2, 0x2310, GUIDEntryHeapLeakCheck },
	{ 2, 0x2322, FixedHeapLeakCheck },
	{ 2, 0x2323, FixedHeapLeakCheck },
	{ 2, 0x2324, FixedHeapLeakCheck },
	{ 2, 0x2325, FixedHeapLeakCheck },
	{ 2, 0x2326, FixedHeapLeakCheck },
	{ 2, 0x2351, LeakCheckMemBlockMCShow },
	{ 2, 0x2370, PhyMemPartLeakShow },
	{ 2, 0x2380, LeakCheckPhyMemLowShow },
	{ 2, 0x23b4, ProcFixedHeapLeakCheck },
	{ 2, 0x2891, LeakCheckFilterCmd },
	{ 2, 0x3170, PhyMemPartLogStart },
	{ 2, 0x31a0, NameHeapLogStart },
	{ 2, 0x3270, PhyMemPartLogStop },
	{ 2, 0x32a0, NameHeapLogStop },
	{ 2, 0x3370, LogPhyMemPartShow },
	{ 2, 0x33a0, NameHeapLogShow },
	{ 2, 0x34a0, (int (*)(int))NameHeapLogSaveCore },
	{ 1, 0x8000, ABTimeShow },
	{ 1, 0x9100, CpuMaskShow },
	{ 1, 0x9200, CpuMaskDisable },
	{ 1, 0x9300, CpuMaskReset },
};

static inline void kshowHeadMessage(int id)
{
	SCE_PRINTF_WITH_INFO_FUNC(0, 3779, "kshowHeadMessage", "***** kshow : 0x%08x *****\n", id);
	SCE_PRINTF_WITH_INFO_FUNC(0, 3780, "kshowHeadMessage",
		"\tkshow: kernel resource display command\n");
	SCE_PRINTF_WITH_INFO_FUNC(0, 3781, "kshowHeadMessage",
		"\t\tkshow displays information and results on console\n");
	SCE_PRINTF_WITH_INFO_FUNC(0, 3782, "kshowHeadMessage",
		"\t\tThis will be replaced with a standard debugger command.\n");
	SCE_PRINTF_WITH_INFO_FUNC(0, 3783, "kshowHeadMessage", "\t\tUsage: kshow <hex number>\n");
	SCE_PRINTF_WITH_INFO_FUNC(0, 3784, "kshowHeadMessage",
		"\t\ttHelp:  <kshow 0x0> display help messages\n");
	SCE_PRINTF_WITH_INFO_FUNC(0, 3785, "kshowHeadMessage", "\n");
}

/* 0x810099f8 */
int kernelUtility(int id)
{
	const SdbgpKShowCmd *c = s_kshowCmds;
	uint32_t i;
	int res;

	kshowHeadMessage(id);
	for (i = 0; i < 53; i++, c++) {
		if (c->cmd == id) {
			res = c->fn(id);
			if (res < 0)
				SCE_PRINTF_WITH_INFO(0, 3807, "command failed 0x%08x\n", res);
			return res;
		}
	}
	return 0;
}

/* 0x81009ae0 */
void sdbgpInitProfiler(struct SysrootProfiler *profiler)
{
	int i;

	for (i = 0; i < 16; i++) {
		s_abTime[i].count = 0;
		s_abTime[i].min = (uint64_t)-1;
		s_abTime[i].max = 0;
		s_abTime[i].sum = 0;
	}
	profiler->reset = ABTimeReset;
	profiler->start = ABTimeStart;
	profiler->stop = ABTimeStop;
}
