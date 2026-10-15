/*
 * SceSysmem: physical memory partitions. Only the global partitions'
 * accessors and the hooks are converted so far; the rest of the file is
 * still in sysmem.c's stubs.
 */

#include "sysmem_internal.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "phymem.c"

/* the globals are not static: the unconverted part of the file sets them */

#define PHYMEM_PAGE_SHIFT		12
#define PHYMEM_PAGES_PER_1M		0x100
#define PHYMEM_BUDDY_ORDERS		9	/* 4 KiB .. 1 MiB */

/*
 * A physical page descriptor. The table holds one 32-bit word per page
 * index; a descriptor spans the words of the pages it stands for.
 */
typedef struct PhyPage {
	SceUInt32 info;			/* pindex | order | state */
	SceSize size;
	struct PhyPage *prev;
	struct PhyPage *next;
} PhyPage;

#define PHYPAGE_PINDEX(_info)		((_info) & 0xfffff)
#define PHYPAGE_ORDER_MASK		0x1f00000
#define PHYPAGE_ORDER(_info)		(((_info) << 7) >> 27)
#define PHYPAGE_LINK_MASK		0x30000000
#define PHYPAGE_LINK_MORE		0x10000000	/* the buddy list continues at pindex */
#define PHYPAGE_LINK_LAST		0x20000000
#define PHYPAGE_SIZED_MASK		0xc0000000	/* both set: size holds the length */
#define PHYPAGE_FREE_1M			0xd0000000
#define PHYPAGE_KIND_BUDDY		0x40000000	/* a buddy block: size from the order */
#define PHYPAGE_BUDDY_FREE_LAST		0x60000000
#define PHYPAGE_BUDDY_USED		0x70000000

/* the page descriptors of a range of page indices */
typedef struct PhyPageTable {
	uintptr_t pbase;		/* of page index 0 */
	uintptr_t pbaseEnd;		/* 0x04: printed beside pbase */
	SceUInt32 reserved0;
	SceUInt32 *entries;		/* 0x0c */
	SceUInt32 reserved1[2];
	SceUInt32 basePindex;		/* 0x18 */
	SceUInt32 pindexMin;		/* 0x1c */
	SceUInt32 pindexMax;		/* 0x20 */
} PhyPageTable;

typedef struct PhyPageTableMgr {
	PhyPageTable *tables[2];
} PhyPageTableMgr;

/* a physically contiguous part of a partition */
typedef struct PhyMemSegment {
	struct PhyMemSegment *next;
	uintptr_t pbase;
	SceSize psize;
	PhyPageTableMgr *tables;	/* 0x0c */
	SceUInt32 initialized;		/* 0x10 */
	SceUInt32 tableIndex;		/* 0x14: the first page's index in the tables */
	SceUInt32 pindexStart;		/* 0x18 */
	SceUInt32 pindexEnd;		/* 0x1c */
	void *phyMemLow;		/* 0x20: CDRAM partitions */
} PhyMemSegment;

/* the per-CPU cache of single pages */
typedef struct PhyMemPartCpuCache {
	SceUInt16 max;
	SceUInt16 count;
	PhyPage *pages[7];
} PhyMemPartCpuCache;

typedef struct PhyMemPartBuddy {
	SceUInt32 state;
	SceUInt32 pindex;		/* the first free block's */
} PhyMemPartBuddy;

#define PHYMEM_BUDDY_EMPTY		0x10001
#define PHYMEM_BUDDY_HAS		0x10002

typedef struct PhyMemPart {
	SceObjectBase base;
	SceInt32 refCount;		/* 0x08 */
	SceUInt32 flags;		/* 0x0c: bit0 = active */
	SceUID uid;			/* 0x10 */
	SceUInt32 type;			/* 0x14 */
	SceKernelSpinlock lock;		/* 0x18 */
	PhyMemPartCpuCache *cpuCache;	/* 0x1c */
	PhyPage *freeList;		/* 0x20: whole 1 MiB units, sorted */
	PhyPage freeListHead;		/* 0x24 */
	PhyMemPartBuddy buddy[PHYMEM_BUDDY_ORDERS];	/* 0x34 */
	PhyMemSegment *segments;	/* 0x7c */
	SceInt32 useCount;		/* 0x80: creators and openers; the last delete frees */
	volatile SceInt32 freeSize;	/* 0x84: Sony rereads it on every use */
	SceInt32 freeSizeMin;		/* 0x88 */
	char name[0x20];		/* 0x8c */
} PhyMemPart;

_Static_assert(sizeof(PhyMemPart) == 0xac, "PhyMemPart size");

#define PHYMEM_PART_ACTIVE		0x1
#define PHYMEM_PART_TYPE(_part)		((_part)->type & 0xf0000)
#define PHYMEM_PART_TYPE_LPDDR2		0x10000
#define PHYMEM_PART_TYPE_CDRAM		0x30000
#define PHYMEM_PART_HAS_CPU_CACHE	0x8
#define PHYMEM_PART_ALLOC_LOW		0x10	/* buddies split from the low end */
#define PHYMEM_PART_SHOW_ON_FAIL	0x20	/* dump the free lists when an allocation fails */

/* fill of a destroyed partition */
#define PHYMEM_PART_DEAD_FILL		0x07fd5a4c

/* sceKernelGetPhyMemInfo's ranges (henkaku wiki names) */
#define SCE_KERNEL_PHYMEMPART_KERNEL_INDEX	1
#define SCE_KERNEL_PAGE_VENEZIA_INDEX		8
#define SCE_KERNEL_PAGE_VIP_INDEX		9

/* the pages of Venezia or the VIP */
typedef struct PhyMemPageRange {
	SceUInt32 page;
	SceSize size;
} PhyMemPageRange;


SceClass *s_phyMemPartClass;
const PhyMemPageRange *s_phyPageVenezia;
const PhyMemPageRange *s_phyPageVip;

/* the kernel's page table manager, set by PhyMemModuleStartPhase2 */
PhyPageTableMgr *s_phyPageTableMgr;
PhyMemPart *s_phyPartKernel;
PhyMemPart *s_phyPartTool;
PhyMemPart *s_phyPartGame;
PhyMemPart *s_phyPartCdram;
/* how much the game partition has grown */
SceSize s_phyMemPartGrownSize;
/* PLACEHOLDER: the partition tracker; only the allocation callback is known */
typedef struct PhyMemPartHook {
	void (*alloc)(struct PhyMemPart *part, SceSize size, struct PhyPage *page);
	void (*free)(struct PhyMemPart *part, struct PhyPage *page);
	void (*create)(struct PhyMemPart *part);
	void (*delete)(struct PhyMemPart *part, const void *caller);
} PhyMemPartHook;

/* PLACEHOLDER: the partition log; only the creation callback is known */
typedef struct PhyMemPartLogCallback {
	void (*create)(struct PhyMemPart *part);
	void (*delete)(struct PhyMemPart *part);
} PhyMemPartLogCallback;

#if FW_DEBUG
/* debug builds only: the partition tracker and log */
const PhyMemPartHook *s_phyMemPartHook;
const PhyMemPartLogCallback *s_phyMemPartLogCallback;
#endif

/* sceKernelSetPhyMemPartHook */
int SceSysmemForKernel_31FA81B5(const PhyMemPartHook *hook)
{
#if FW_DEBUG
	s_phyMemPartHook = hook;
#endif
	return 0;
}

/* sceKernelSetPhyMemPartLogCallback */
int SceSysmemForKernel_93D0A183(const PhyMemPartLogCallback *callbacks)
{
#if FW_DEBUG
	s_phyMemPartLogCallback = callbacks;
#endif
	return 0;
}

/* sceKernelGetPhyPageReset */
void *SceSysmemForKernel_8FD11E02(void)
{
	return g_uidManager->phyPageReset;
}

/* sceKernelGetGrownPhyMemPartSize */
SceSize SceSysmemForKernel_BC36755F(void)
{
	return s_phyMemPartGrownSize;
}

/* sceKernelGetPhyPartKernel (sysmem_internal.h declares it with void *) */
void *SceSysmemForKernel_0164D817(void)
{
	return s_phyPartKernel;
}

/* sceKernelGetPhyPartTool */
PhyMemPart *SceSysmemForKernel_0B1B4ACA(void)
{
	return s_phyPartTool;
}

/* sceKernelGetPhyPartGame */
PhyMemPart *SceSysmemForKernel_62800A6B(void)
{
	return s_phyPartGame;
}

/* sceKernelSetPhyPartGame */
void SceSysmemForKernel_3753C584(PhyMemPart *part)
{
	s_phyPartGame = part;
}

/* sceKernelSetPhyPartCdram */
void SceSysmemForKernel_DCEF10B2(PhyMemPart *part)
{
	s_phyPartCdram = part;
}

/* sceKernelGetPhyMemPartAll */
int SceSysmemForKernel_021053DD(SceUInt32 visibility, SceUID *vector, SceSize vectorSize,
				SceUInt32 *count)
{
	if (count == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return ksceGUIDGetUIDVectorByClass(s_phyMemPartClass, visibility, vector, vectorSize >> 2,
					   count);
}

/* a physical range by index */
int sceKernelGetPhyMemInfo(SceUInt32 type, SceKernelPhyMemRangeInfo *info)
{
	const PhyMemPageRange *pages;
	const PhyMemSegment *segment;

	if (info == NULL || info->size != sizeof(*info))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	switch (type) {
	case SCE_KERNEL_PAGE_VENEZIA_INDEX:
		pages = s_phyPageVenezia;
		break;
	case SCE_KERNEL_PAGE_VIP_INDEX:
		pages = s_phyPageVip;
		break;
	case SCE_KERNEL_PHYMEMPART_KERNEL_INDEX:
		if (s_phyPartKernel == NULL)
			return SCE_KERNEL_ERROR_PHYMEM_ERROR;
		segment = s_phyPartKernel->segments;
		info->base = segment->pbase;
		info->rangeSize = segment->psize;
		return 0;
	default:
		return SCE_KERNEL_ERROR_PHYMEM_ERROR;
	}
	if (pages == NULL)
		return SCE_KERNEL_ERROR_PHYMEM_ERROR;
	info->base = pages->page << 12;
	info->rangeSize = pages->size;
	return 0;
}

int ksceKernelVAtoPA(const void *va, uintptr_t *pa);
SceInt32 ksceKernelAtomicAddAndGet32(SceInt32 *address, SceInt32 value);
SceInt32 ksceKernelAtomicSubAndGet32(SceInt32 *address, SceInt32 value);
SceInt32 ksceKernelAtomicCompareAndSet32(SceInt32 *address, SceInt32 expected, SceInt32 value);

/* phymem_low.c */
typedef struct PhyMemLow PhyMemLow;
int PhyMemLowAlloc(PhyMemLow *pml, SceSize size, SceSize alignment, SceUInt32 flags,
		   uintptr_t *pa, SceSize *allocated);
int PhyMemLowFree(PhyMemLow *pml, uintptr_t pa, SceSize size);
int PhyMemLowGetFreeSize(PhyMemLow *pml, SceSize *free);

/* the page descriptor of pindex in one table, NULL outside it */
static inline __attribute__((always_inline))
PhyPage *PhyPageTableGetPhyPage(PhyPageTable *table, SceUInt32 pindex)
{
	PhyPage *page;

	if (pindex < table->pindexMin) {
		SCE_DBG_PRINTF_FUNC(0xb, 1124, "PhyPageTableGetPhyPage",
				    "pindex=0x%08x < 0x%08x[pindexMin]\n", pindex, table->pindexMin);
		return NULL;
	}
	if (pindex >= table->pindexMax)
		return NULL;
	page = (PhyPage *)(table->entries + pindex - table->basePindex);
	return page;
}

static inline __attribute__((always_inline))
PhyPage *PhyPageTableMgrGetPhyPage(PhyPageTableMgr *mgr, SceUInt32 pindex)
{
	PhyPage *page = PhyPageTableGetPhyPage(mgr->tables[0], pindex);
	PhyPageTable *table;

	if (page != NULL)
		return page;
	table = mgr->tables[1];
	page = PhyPageTableGetPhyPage(table, pindex);
	if (page != NULL)
		return page;
	SCE_PANIC_PRINTF_FUNC(1194, "PhyPageTableMgrGetPhyPage",
			      "no pNext: %p: 0x%08x <= 0x%08x=pindex\n", table, table->pindexMax,
			      pindex);
}

/* the descriptor of pindex in whichever segment holds it */
static __attribute__((noinline)) PhyPage *PhyMemPartGetPhyPageByIndexRaw(PhyMemPart *part, SceUInt32 pindex)
{
	PhyMemSegment *segment;

	for (segment = part->segments; segment != NULL; segment = segment->next)
		if (segment->pindexStart <= pindex && pindex < segment->pindexEnd)
			return PhyPageTableMgrGetPhyPage(segment->tables, pindex);
	SCE_DBG_PRINTF_FUNC(0xb, 1705, "PhyMemPartGetPhyPageByIndexRaw",
			    "There is no segment for pindex=0x%08x\n", pindex);
	for (segment = part->segments; segment != NULL; segment = segment->next)
		SCE_DBG_PRINTF_FUNC(0xb, 1709, "PhyMemPartGetPhyPageByIndexRaw",
				    "pSegment:0x%08x - 0x%08x\n", segment->pindexStart,
				    segment->pindexEnd);
	return NULL;
}

static inline SceInt32 PhyMemPartRefer(PhyMemPart *part)
{
	SceInt32 count = ksceKernelAtomicAddAndGetPositive32InRange(&part->refCount, 1, 0x7fffffff);

	SCE_DBG_ASSERT_CHECK_FUNC(count >= 0, 1489, "PhyMemPartRefer");
	return count;
}

static inline SceInt32 PhyMemPartRelease(PhyMemPart *part)
{
	SceInt32 count = ksceKernelAtomicDecIfPositive32(&part->refCount);

	SCE_DBG_ASSERT_CHECK_FUNC(count > 0, 1499, "PhyMemPartRelease");
	return count;
}

/* name unknown: a free 1 MiB run back into the sorted free list, merged with its neighbours */
int PhyMemPartInsertFreePhyPage1MRaw(PhyMemPart *part, PhyPage *page)
{
	PhyMemSegment *segment;
	PhyPage *prev, *next;
	SceUInt32 pindex = PHYPAGE_PINDEX(page->info), prevIndex, nextIndex = 0;
	SceSize size;

	for (segment = part->segments; ; segment = segment->next) {
		if (segment == NULL)
			return SCE_KERNEL_ERROR_PHYMEMPART_OUT_OF_INDEX;
		if (segment->pindexStart <= pindex && pindex < segment->pindexEnd)
			break;
	}
	size = page->size;
	next = part->freeList;
	do {
		prevIndex = nextIndex;
		prev = next;
		next = prev->next;
		if (part->freeList == next) {
			next = NULL;
			nextIndex = prevIndex;
			break;
		}
		nextIndex = PHYPAGE_PINDEX(next->info);
	} while (!(prevIndex < pindex && pindex < nextIndex));
	if (prevIndex + (prev->size >> 20) * PHYMEM_PAGES_PER_1M == pindex) {
		prev->size += size;
		page->info &= 0x3fffffff;
		size = prev->size;
		pindex = PHYPAGE_PINDEX(prev->info);
		page = prev;
	} else if (prev != page) {
		page->prev = prev;
		page->next = prev->next;
		prev->next->prev = page;
		prev->next = page;
	}
	if (next != NULL && pindex + (size >> 20) * PHYMEM_PAGES_PER_1M == nextIndex) {
		SceUInt32 info = next->info;
		SceSize nextSize = next->size;
		PhyPage *after = next->next;

		page->next = after;
		after->prev = page;
		next->info = info & 0x3fffffff;
		page->size = nextSize + size;
	}
	return 0;
}

int PhyMemPartAddFreePhyPage1MRaw(PhyMemPart *part, PhyPage *page)
{
	SceUInt32 pindex = PHYPAGE_PINDEX(page->info), prevIndex, nextIndex = 0;
	PhyPage *prev, *next = part->freeList;

	do {
		prevIndex = nextIndex;
		prev = next;
		next = prev->next;
		if (part->freeList == next)
			goto insert;
		nextIndex = PHYPAGE_PINDEX(next->info);
	} while (!(prevIndex < pindex && pindex < nextIndex));
	SCE_DBG_PRINTF(0xb, 2792, "pindex: 0x%08x<0x%08x<0x%08x\n", prevIndex, pindex, nextIndex);
insert:
	page->prev = prev;
	page->next = prev->next;
	prev->next->prev = page;
	prev->next = page;
	return 0;
}

#if FW_DEBUG
/* whether va lies in one of the manager's tables; mode 1 panics when not */
int PhyPageTableMgrCheckPhyPage(PhyPageTableMgr *mgr, const void *va, int mode)
{
	PhyPageTable *table = NULL;
	uintptr_t address = (uintptr_t)va, pa;
	int ret;

	ret = ksceKernelVAtoPA(va, &pa);
	if (ret >= 0) {
		table = mgr->tables[0];
		if (address >= (uintptr_t)table->entries &&
		    address < (uintptr_t)table->entries + (table->pindexMax - table->basePindex) * 4)
			return 0;
		table = mgr->tables[1];
		if (address >= (uintptr_t)table->entries &&
		    address < (uintptr_t)table->entries + (table->pindexMax - table->basePindex) * 4)
			return 0;
		ret = SCE_KERNEL_ERROR_ERROR;
	}
	if (mode == 1)
		SCE_PANIC_PRINTF(1244, "no pNext: %p: %p: %p-%p\n", table, va, table->entries,
				 (char *)table->entries + (table->pindexMax - table->basePindex) * 4);
	return ret;
}
#endif

/* name unknown: an empty segment */
PhyMemSegment *PhyMemSegmentCreate(void)
{
	PhyMemSegment *segment = ksceKernelAlloc(sizeof(*segment));

	if (segment != NULL) {
		segment->initialized = 0;
		segment->phyMemLow = NULL;
		segment->next = NULL;
	}
	return segment;
}

/* frees a partition, its cache and segments */
void PhyMemPartFreeRaw(PhyMemPart *part)
{
	PhyMemSegment *segment, *next;

	if (part->cpuCache != NULL) {
		ksceKernelFree(part->cpuCache);
		part->cpuCache = NULL;
	}
	for (segment = part->segments; segment != NULL; segment = next) {
		next = segment->next;
		ksceKernelFree(segment);
	}
	part->segments = NULL;
	SetMemory32Core(part, PHYMEM_PART_DEAD_FILL, sizeof(*part));
	ksceKernelFree(part);
}

/* name unknown: a segment's first page as one free unit of the whole segment */
__attribute__((noinline))
PhyPage *PhyMemSegmentGetFreePhyPage1M(PhyMemSegment *segment)
{
	SceUInt32 pindex = segment->pindexStart;
	SceSize psize = segment->psize;
	PhyPage *page = PhyPageTableMgrGetPhyPage(segment->tables, pindex);

	page->size = psize;
	page->info = (page->info & PHYPAGE_ORDER_MASK) | PHYPAGE_PINDEX(pindex) | PHYPAGE_FREE_1M;
	return page;
}

/* the buddies empty */
static inline void PhyMemPartResetBuddy(PhyMemPart *part)
{
	int i;

	for (i = 0; i < PHYMEM_BUDDY_ORDERS; i++) {
		part->buddy[i].state = PHYMEM_BUDDY_EMPTY;
		part->buddy[i].pindex = 0;
	}
}

/* the free size and its low-water mark: the size of every segment */
static inline void PhyMemPartResetFreeSize(PhyMemPart *part, SceSize psize,
					   PhyMemSegment *segment)
{
	part->freeSize = psize;
	part->freeSizeMin = psize;
	for (; segment != NULL; segment = segment->next) {
		ksceKernelAtomicAddAndGet32((SceInt32 *)&part->freeSize, segment->psize);
		ksceKernelAtomicAddAndGet32(&part->freeSizeMin, segment->psize);
	}
}

int PhyMemPartInitCdramRaw(PhyMemPart *part, uintptr_t pbase, SceSize psize)
{
	PhyMemSegment *segment;
	SceUInt32 flags = PHYMEM_PART_ACTIVE;

	SCE_DBG_ASSERT_CHECK(part->segments != NULL, 3135);
	segment = part->segments;
	if (segment->phyMemLow == NULL) {
		if ((pbase & 0xfffff) || (psize & 0xfffff))
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		segment->pbase = pbase;
		segment->psize = psize;
	} else {
		psize = segment->psize;
	}
	part->flags = 0;
	part->lock = 0;
	segment = segment->next;
	part->freeSize = psize;
	part->freeSizeMin = psize;
	part->useCount = 1;
	if (segment != NULL) {
		do {
			ksceKernelAtomicAddAndGet32((SceInt32 *)&part->freeSize, segment->psize);
			ksceKernelAtomicAddAndGet32(&part->freeSizeMin, segment->psize);
			segment = segment->next;
		} while (segment != NULL);
		flags = part->flags | PHYMEM_PART_ACTIVE;
	}
	PhyMemPartResetBuddy(part);
	part->flags = flags;
	part->freeList = NULL;
	memset(&part->freeListHead, 0, sizeof(part->freeListHead));
	return 0;
}

/* back to all free */
int PhyMemPartRecycleRaw(PhyMemPart *part)
{
	PhyMemSegment *segment;
	PhyPage *page, *head;
	SceUInt32 pindex;

	switch (PHYMEM_PART_TYPE(part)) {
	case PHYMEM_PART_TYPE_LPDDR2:
		break;
	case PHYMEM_PART_TYPE_CDRAM:
		segment = part->segments;
		part->useCount = 1;
		PhyMemPartResetFreeSize(part, segment->psize, segment->next);
		return 0;
	default:
		SCE_DBG_ASSERT_FAIL(3518);
		break;
	}
	PhyMemPartResetBuddy(part);
	part->freeList = &part->freeListHead;
	part->freeListHead.prev = &part->freeListHead;
	part->freeListHead.next = &part->freeListHead;
	for (segment = part->segments; segment != NULL; segment = segment->next) {
		pindex = segment->pindexStart;
		page = PhyPageTableMgrGetPhyPage(segment->tables, pindex);
		head = part->freeList;
		page->size = segment->psize;
		page->next = head;
		page->info = (page->info & PHYPAGE_ORDER_MASK) | PHYPAGE_PINDEX(pindex) | PHYPAGE_FREE_1M;
		page->prev = head->prev;
		head->prev->next = page;
		head->prev = page;
	}
	segment = part->segments;
	PhyMemPartResetFreeSize(part, segment->psize, segment->next);
	return 0;
}

int PhyMemPartInitWoSegmentRaw(PhyMemPart *part)
{
	PhyMemSegment *segment, *next;
	SceSize psize;
	int i, j;

	SCE_DBG_ASSERT(part->segments != NULL, 3058);
	segment = part->segments;
	part->flags = 0;
	part->lock = 0;
	psize = segment->psize;
	next = segment->next;
	part->useCount = 1;
	PhyMemPartResetFreeSize(part, psize, next);
	PhyMemPartResetBuddy(part);
	part->freeList = &part->freeListHead;
	part->freeListHead.prev = &part->freeListHead;
	part->freeListHead.next = &part->freeListHead;
	for (segment = part->segments; segment != NULL; segment = segment->next)
		PhyMemPartAddFreePhyPage1MRaw(part, PhyMemSegmentGetFreePhyPage1M(segment));
	for (i = 0; i < 4; i++) {
		part->cpuCache[i].max = 7;
		part->cpuCache[i].count = 0;
		for (j = 0; j < 7; j++)
			part->cpuCache[i].pages[j] = NULL;
	}
	part->flags |= PHYMEM_PART_ACTIVE;
	return 0;
}

/* the blocks on a buddy list, from its first pindex */
int PhyMemPartFreeCountLinkByIndexRaw(PhyMemPart *part, SceUInt32 pindex)
{
	PhyPage *page;
	SceUInt32 info;
	int count = 1;

	for (;;) {
		page = PhyMemPartGetPhyPageByIndexRaw(part, pindex);
		info = page->info;
		if ((info & PHYPAGE_LINK_MASK) != PHYPAGE_LINK_MORE) {
			if ((info & PHYPAGE_LINK_MASK) == PHYPAGE_LINK_LAST)
				return count;
			SCE_DBG_ASSERT_FAIL(2132);
			info = page->info;
		}
		pindex = PHYPAGE_PINDEX(info);
		count++;
	}
}

int PhyMemPartGetFreeSizeRaw(PhyMemPart *part, SceSize *freeSize)
{
	PhyPage *page;
	SceSize size = 0, unit = 0x1000;
	int i, count;

	switch (PHYMEM_PART_TYPE(part)) {
	case PHYMEM_PART_TYPE_LPDDR2:
		break;
	case PHYMEM_PART_TYPE_CDRAM:
		return PhyMemLowGetFreeSize(part->segments->phyMemLow, freeSize);
	default:
		SCE_DBG_ASSERT_FAIL(4066);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	for (page = part->freeList->next; part->freeList != page; page = page->next)
		size += page->size;
	for (i = 0; i < PHYMEM_BUDDY_ORDERS; i++, unit <<= 1) {
		switch (part->buddy[i].state) {
		case PHYMEM_BUDDY_EMPTY:
			count = 0;
			break;
		case PHYMEM_BUDDY_HAS:
			count = PhyMemPartFreeCountLinkByIndexRaw(part, part->buddy[i].pindex) * unit;
			break;
		default:
			SCE_DBG_ASSERT_CHECK_FUNC(0, 4029, "PhyMemPartGetFreeSizeLpddr2Raw");
			count = 0;
			break;
		}
		size += count;
	}
	*freeSize = size;
	return 0;
}

/* the free size less size, lowering the low-water mark with it */
static inline void PhyMemPartSubFreeSize(PhyMemPart *part, SceSize size)
{
	SceInt32 freeSize = ksceKernelAtomicSubAndGet32((SceInt32 *)&part->freeSize, size), low;

	do {
		low = part->freeSizeMin;
		if (low <= freeSize)
			break;
	} while (ksceKernelAtomicCompareAndSet32(&part->freeSizeMin, low, freeSize) != low);
}

#if FW_DEBUG
/* the free size kept against a recount; a mismatch is fatal (it stops here) */
int PhyMemPartCheckAndFixFreeSizeRaw(PhyMemPart *part, const char *op, SceSize size)
{
	SceSize real;
	int ret;

	ret = PhyMemPartGetFreeSizeRaw(part, &real);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 2022, "PhyMemPartCdramGetFreeSizeRaw failed 0x%08x\n", ret);
		return ret;
	}
	if (real != (SceSize)part->freeSize) {
		SCE_DBG_PRINTF(0, 2028, "%s [%s] psize=0x%08x, 0x%08x != 0x%08x (real)\n", op,
			       part->name, size, part->freeSize, real);
		part->freeSize = real;
		for (;;)
			;
	}
	return 0;
}
#endif

int PhyMemPartCdramFreeCore(PhyMemPart *part, uintptr_t pa, SceSize size)
{
	int flags, ret;

	PhyMemPartRefer(part);
	flags = ksceKernelSpinlockLowLockCpuSuspendIntr(&part->lock);
	if (part->flags & PHYMEM_PART_ACTIVE) {
		ret = PhyMemLowFree(part->segments->phyMemLow, pa, size);
		if (ret >= 0) {
			ksceKernelAtomicAddAndGet32((SceInt32 *)&part->freeSize, size);
#if FW_DEBUG
			PhyMemPartCheckAndFixFreeSizeRaw(part, "Add", size);
#endif
		}
	} else {
		SCE_DBG_PRINTF(0xb, 4455, "pPhyMemPart is not active\n");
		ret = SCE_KERNEL_ERROR_PHYMEMPART_NOT_ACTIVE;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&part->lock, flags);
	PhyMemPartRelease(part);
	return ret;
}

int PhyMemPartCdramAllocCore(PhyMemPart *part, SceSize size, SceSize alignment, uintptr_t *pa)
{
	SceSize allocated;
	int flags, ret;

	PhyMemPartRefer(part);
	flags = ksceKernelSpinlockLowLockCpuSuspendIntr(&part->lock);
	if (part->flags & PHYMEM_PART_ACTIVE) {
		ret = PhyMemLowAlloc(part->segments->phyMemLow, size, alignment, 1, pa, &allocated);
		if (ret < 0) {
			ret = SCE_KERNEL_ERROR_NO_FREE_PHYSICAL_PAGE_CDRAM;
		} else {
			PhyMemPartSubFreeSize(part, size);
#if FW_DEBUG
			PhyMemPartCheckAndFixFreeSizeRaw(part, "Sub", size);
#endif
		}
	} else {
		SCE_DBG_PRINTF(0xb, 4422, "pPhyMemPart is not active\n");
		ret = SCE_KERNEL_ERROR_PHYMEMPART_NOT_ACTIVE;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&part->lock, flags);
	PhyMemPartRelease(part);
	return ret;
}

#define PHYMEM_CPU_COUNT		4

/* the bytes a descriptor stands for */
static inline SceSize PhyPageGetSize(const PhyPage *page)
{
	if ((page->info & PHYPAGE_SIZED_MASK) == PHYPAGE_SIZED_MASK)
		return page->size;
	return 1 << PHYPAGE_ORDER(page->info);
}

/* name unknown: the out-of-line copy other files call (virpage.c) */
SceSize PhyPageGetSizeRaw(const PhyPage *page)
{
	return PhyPageGetSize(page);
}

/* the physical address of pindex, 0 outside every segment */
static inline uintptr_t PhyMemPartGetPbaseByIndex(PhyMemPart *part, SceUInt32 pindex)
{
	PhyMemSegment *segment;

	for (segment = part->segments; segment != NULL; segment = segment->next)
		if (segment->pindexStart <= pindex && pindex < segment->pindexEnd)
			return segment->pbase + ((pindex - segment->pindexStart) << PHYMEM_PAGE_SHIFT);
	return 0;
}

/* the free memory held in one CPU's page cache */
static inline SceSize PhyMemPartCpuCacheGetFreeSize(const PhyMemPartCpuCache *cache)
{
	SceSize size = 0;
	int i;

	if (cache->count != 0)
		for (i = 0; i < cache->max; i++)
			if (cache->pages[i] != NULL)
				size += 1 << PHYMEM_PAGE_SHIFT;
	return size;
}

int PhyMemPartGetInfoForDebugger(PhyMemPart *part, SceKernelPhyMemPartInfoForDebugger *info)
{
	struct SceKernelPhyMemPartFreeBlock *block;
	PhyMemSegment *segment;
	PhyPage *page, *head;
	SceUInt32 pindex, prev, count;
	SceSize real, cached;
	int flags, ret, i, links, n;

	PhyMemPartRefer(part);
	flags = ksceKernelSpinlockLowLockCpuSuspendIntr(&part->lock);
	if (!(part->flags & PHYMEM_PART_ACTIVE)) {
		SCE_DBG_PRINTF(0xb, 4606, "PhyMemPart is not active\n");
		ret = SCE_KERNEL_ERROR_PHYMEMPART_NOT_ACTIVE;
		goto out;
	}
	info->freeUnitCount = 0;
	info->uid = part->uid;
	info->type = part->type;
	info->name = part->name;
	info->freeSize = part->freeSize;
	switch (PHYMEM_PART_TYPE(part)) {
	case PHYMEM_PART_TYPE_LPDDR2:
		for (page = part->freeList->next; page != part->freeList; ) {
			prev = page->info;
			page = page->next;
			info->freeUnitCount++;
			if (page != part->freeList &&
			    page->info << PHYMEM_PAGE_SHIFT < prev << PHYMEM_PAGE_SHIFT) {
				SCE_DBG_PRINTF(0xb, 4635, "pPageUnit: 0x%08x < 0x%08x\n", page->size,
					       prev << PHYMEM_PAGE_SHIFT);
				SCE_DBG_ASSERT_FAIL(4636);
			}
		}
		break;
	case PHYMEM_PART_TYPE_CDRAM:
		break;
	default:
		SCE_DBG_ASSERT_FAIL(4645);
		break;
	}
	info->freeSizeMin = part->freeSizeMin;
	n = 0;
	for (segment = part->segments; segment != NULL; segment = segment->next, n++) {
		if (n < 16) {
			info->segments[n].pbase = segment->pbase;
			info->segments[n].psize = segment->psize;
		}
	}
	info->segmentCount = n;

	/* the buddies' blocks, then the free 1 MiB units */
	block = info->freeBlocks;
	info->freeBlockCount = 0;
	for (i = 0; i < PHYMEM_BUDDY_ORDERS; i++) {
		if (part->buddy[i].state != PHYMEM_BUDDY_HAS)
			continue;
		pindex = part->buddy[i].pindex;
		links = 1;
		count = PhyMemPartFreeCountLinkByIndexRaw(part, pindex);
		for (;;) {
			page = PhyMemPartGetPhyPageByIndexRaw(part, pindex);
			if (block != NULL && info->freeBlockCount < info->freeBlockMax) {
				block->pbase = PhyMemPartGetPbaseByIndex(part, pindex);
				block->size = PhyPageGetSize(page);
				block++;
			}
			info->freeBlockCount++;
			if ((page->info & PHYPAGE_LINK_MASK) != PHYPAGE_LINK_MORE)
				break;
			pindex = PHYPAGE_PINDEX(page->info);
			links++;
		}
		if (links != (int)count)
			SCE_DBG_ASSERT_FAIL(4697);
	}
	if (PHYMEM_PART_TYPE(part) != PHYMEM_PART_TYPE_CDRAM) {
		head = part->freeList;
		for (page = head->next; page != head; page = page->next) {
			if (block != NULL && info->freeBlockCount < info->freeBlockMax) {
				block->pbase = page->info << PHYMEM_PAGE_SHIFT;
				block->size = page->size;
				block++;
			}
			info->freeBlockCount++;
		}
	}

	PhyMemPartGetFreeSizeRaw(part, &real);
	if (info->freeSize != real) {
		SCE_DBG_PRINTF(0xb, 4734, "freeSize mismatch:[%s] 0x%08x != 0x%08x[real]: FIX\n",
			       part->name, info->freeSize, real);
		part->freeSize = real;
		info->freeSize = part->freeSize;
	}
	if (part->type & PHYMEM_PART_HAS_CPU_CACHE) {
		cached = 0;
		for (i = 0; i < PHYMEM_CPU_COUNT; i++)
			cached += PhyMemPartCpuCacheGetFreeSize(&part->cpuCache[i]);
		info->cacheSize += cached;
	} else {
		info->cacheSize = 0;
	}
	ret = 0;
out:
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&part->lock, flags);
	PhyMemPartRelease(part);
	return ret;
}

#if FW_DEBUG
/* the free list and every buddy list reachable */
static inline int PhyMemPartLpddr2CheckRaw(PhyMemPart *part)
{
	PhyPage *page;
	SceUInt32 pindex;
	int i;

	for (page = part->freeList->next; page != part->freeList; page = page->next) {
		if (page == NULL) {
			SCE_DBG_PRINTF(0xb, 1735, "pPhyPage1M is NULL\n");
			SCE_DBG_ASSERT_FAIL(1736);
			return SCE_KERNEL_ERROR_ERROR;
		}
	}
	for (i = 0; i < PHYMEM_BUDDY_ORDERS; i++) {
		if (part->buddy[i].state != PHYMEM_BUDDY_HAS)
			continue;
		pindex = part->buddy[i].pindex;
		for (;;) {
			page = PhyMemPartGetPhyPageByIndexRaw(part, pindex);
			if (page == NULL) {
				SCE_DBG_PRINTF(0xb, 1752, "pPhyPage is NULL\n");
				SCE_DBG_ASSERT_FAIL(1753);
				return SCE_KERNEL_ERROR_ERROR;
			}
			if ((page->info & PHYPAGE_LINK_MASK) != PHYPAGE_LINK_MORE)
				break;
			pindex = PHYPAGE_PINDEX(page->info);
		}
	}
	return 0;
}

int PhyMemPartCheckRaw(PhyMemPart *part)
{
	int ret;

	if (PHYMEM_PART_TYPE(part) != PHYMEM_PART_TYPE_LPDDR2)
		return 0;
	ret = PhyMemPartLpddr2CheckRaw(part);
	if (ret < 0)
		SCE_DBG_PRINTF(0xb, 1774, "PhyMemPartLpddr2CheckRaw failed 0x%08x\n", ret);
	return ret;
}
#endif

void PhyMemPartCannotAllocateRaw(PhyMemPart *part, SceSize request, const char *caller)
{
#if FW_DEBUG
	PhyMemSegment *segment;
	PhyPage *page;
	SceUInt32 pindex;
	int i, n;

	SCE_DBG_PRINTF(6, 2456, "Cannot allocate physical page: <%s>\n", caller);
	SCE_DBG_PRINTF(6, 2457, "\tPhyMemPart->name:       [%s]\n", part->name);
	SCE_DBG_PRINTF(6, 2458, "\tPhyMemPart->freeSize:    0x%08x\n", part->freeSize);
	SCE_DBG_PRINTF(6, 2460, "\tPhyMemPart->freeSizeMin: 0x%08x\n", part->freeSizeMin);
	SCE_DBG_PRINTF(6, 2461, "\trequest:                 0x%08x\n", request);
	if (PHYMEM_PART_TYPE(part) != PHYMEM_PART_TYPE_CDRAM) {
		n = 0;
		for (page = part->freeList->next; page != part->freeList; page = page->next, n++)
			SCE_DBG_PRINTF(6, 2475, "\tFree UNIT:%4d:0x%08x/0x%08x\n", n,
				       page->info << PHYMEM_PAGE_SHIFT, page->size);
		for (i = 0; i < PHYMEM_BUDDY_ORDERS; i++) {
			if (part->buddy[i].state != PHYMEM_BUDDY_HAS)
				continue;
			pindex = part->buddy[i].pindex;
			for (n = 0; ; n++) {
				page = PhyMemPartGetPhyPageByIndexRaw(part, pindex);
				SCE_DBG_PRINTF(6, 2492, "\tFreeBuddy:%d:%4d:0x%08x+0x%08x\n", i, n,
					       PhyMemPartGetPbaseByIndex(part, pindex),
					       PhyPageGetSize(page));
				if ((page->info & PHYPAGE_LINK_MASK) != PHYPAGE_LINK_MORE)
					break;
				pindex = PHYPAGE_PINDEX(page->info);
			}
		}
	}
	n = 0;
	for (segment = part->segments; segment != NULL; segment = segment->next, n++)
		SCE_DBG_PRINTF(6, 2503, "\tSegment: %d: 0x%08x+0x%08x\n", n, segment->pbase,
			       segment->psize);
#else
	SCE_PRINTF_WITH_INFO(0, 2507, "Cannot allocate physical page: <%s>\n", caller);
	SCE_PRINTF_WITH_INFO(0, 2508, "\tPhyMemPart->name [%s]\n", part->name);
	SCE_PRINTF_WITH_INFO(0, 2509, "\trequest          0x%08x\n", request);
#endif
}

/* name unknown: two buddies joined into the first, one order up */
void PhyMemPartBuddyMergeRaw(PhyMemPart *part, SceUInt32 pindex, SceUInt32 buddyIndex)
{
	PhyPage *page = PhyMemPartGetPhyPageByIndexRaw(part, pindex);
	PhyPage *buddy = PhyMemPartGetPhyPageByIndexRaw(part, buddyIndex);

	page->info = PHYPAGE_PINDEX(pindex) | (page->info & PHYPAGE_SIZED_MASK) | PHYPAGE_LINK_MASK |
		     (((page->info & PHYPAGE_ORDER_MASK) + (1 << 20)) & PHYPAGE_ORDER_MASK);
	buddy->info &= PHYPAGE_SIZED_MASK;
}

#if FW_DEBUG
/* one buddy list, block by block */
static inline void PhyMemPartShowBuddyLinkByIndexRaw(PhyMemPart *part, SceUInt32 pindex)
{
	PhyMemSegment *segment;
	PhyPage *page;
	SceUInt32 link;
	int n = 1;

	for (;;) {
		page = PhyMemPartGetPhyPageByIndexRaw(part, pindex);
		SCE_DBG_PRINTF_WITH_INFO_FUNC(0, 2229, "PhyMemPartShowBuddyLinkByIndexRaw",
					      "\t\t%4d:0x%04x: 0x%08x+0x%08x\n", n, pindex,
					      PhyMemPartGetPbaseByIndex(part, pindex),
					      PhyPageGetSize(page));
		for (segment = part->segments; ; segment = segment->next) {
			if (segment == NULL)
				SCE_PANIC_FUNC(2234, "PhyMemPartShowBuddyLinkByIndexRaw");
			if (segment->pindexStart <= pindex && pindex < segment->pindexEnd)
				break;
		}
		if (segment->pbase + ((pindex - segment->pindexStart) << PHYMEM_PAGE_SHIFT) <
		    segment->pbase)
			SCE_PANIC_FUNC(2238, "PhyMemPartShowBuddyLinkByIndexRaw");
		link = page->info & PHYPAGE_LINK_MASK;
		if (link != PHYPAGE_LINK_MORE) {
			if (link == PHYPAGE_LINK_LAST)
				return;
			SCE_DBG_ASSERT_CHECK_FUNC(0, 2249, "PhyMemPartShowBuddyLinkByIndexRaw");
		}
		pindex = PHYPAGE_PINDEX(page->info);
		n++;
	}
}

static inline void PhyMemPartCpuCacheShowFreeRaw(const PhyMemPartCpuCache *cache)
{
	const PhyPage *page;
	int i;

	if (cache->count == 0)
		return;
	for (i = 0; i < cache->max; i++) {
		page = cache->pages[i];
		if (page != NULL)
			SCE_DBG_PRINTF_WITH_INFO_FUNC(0, 1365, "PhyMemPartCpuCacheShowFreeRaw",
						      "\t\t%4d: 0x%08x+0x%08x\n", i,
						      page->info << PHYMEM_PAGE_SHIFT,
						      PhyPageGetSize(page));
	}
}

int PhyMemPartShowFreeRaw(PhyMemPart *part)
{
	PhyMemSegment *segment;
	PhyPage *page;
	SceSize total = 0, unit, cached;
	SceUInt32 count;
	int i;

	SCE_DBG_PRINTF_WITH_INFO(0, 2270, "[%s]\n", part->name);
	for (segment = part->segments; segment != NULL; segment = segment->next)
		SCE_DBG_PRINTF_WITH_INFO(0, 2274, "\t0x%08x+0x%08x\n", segment->pbase,
					 segment->psize);
	if (PHYMEM_PART_TYPE(part) == PHYMEM_PART_TYPE_CDRAM)
		return 0;
	for (page = part->freeList->next; page != part->freeList; page = page->next) {
		total += page->size;
		SCE_DBG_PRINTF_WITH_INFO(0, 2294, "\tFree UNIT:0x%08x[0x%08x/0x%08x]\n", total,
					 PHYPAGE_PINDEX(page->info), page->size);
	}
	for (i = 0, unit = 1 << PHYMEM_PAGE_SHIFT; i < PHYMEM_BUDDY_ORDERS; i++, unit <<= 1) {
		count = 0;
		if (part->buddy[i].state == PHYMEM_BUDDY_HAS)
			count = PhyMemPartFreeCountLinkByIndexRaw(part, part->buddy[i].pindex);
		else if (part->buddy[i].state != PHYMEM_BUDDY_EMPTY)
			SCE_DBG_ASSERT_FAIL(2307);
		SCE_DBG_PRINTF_WITH_INFO(0, 2314, "\tFree Buddy:0x%08x + 0x%08x:%d = 0x%08x\n", total,
					 unit, count, total + unit * count);
		total += unit * count;
		if (count != 0)
			PhyMemPartShowBuddyLinkByIndexRaw(part, part->buddy[i].pindex);
	}
	for (i = 0; i < PHYMEM_CPU_COUNT; i++) {
		cached = PhyMemPartCpuCacheGetFreeSize(&part->cpuCache[i]);
		total += cached;
		SCE_DBG_PRINTF_WITH_INFO(0, 2334, "\tFree Cache[%d]:0x%08x: freeSize=0x%08x\n", i,
					 cached, total);
		PhyMemPartCpuCacheShowFreeRaw(&part->cpuCache[i]);
	}
	SCE_DBG_PRINTF(6, 2341, "\tPhyMemPart->freeSize:    0x%08x\n", part->freeSize);
	SCE_DBG_PRINTF(6, 2343, "\tPhyMemPart->freeSizeMin: 0x%08x\n", part->freeSizeMin);
	return 0;
}
#endif

#if FW_DEBUG
/* the free units up to one; Sony's loop never advances, so it repeats the first */
static inline void PhyMemPartShowUnitUntilRaw(PhyMemPart *part, PhyPage *until)
{
	PhyPage *unit;

	SCE_DBG_PRINTF_FUNC(0xb, 2521, "PhyMemPartShowUnitUntilRaw", "(%p, %p)\n", part, until);
	unit = part->freeList->next;
	while (unit != part->freeList && unit != until)
		SCE_DBG_PRINTF_FUNC(0xb, 2531, "PhyMemPartShowUnitUntilRaw", "%p: size=0x%08x\n",
				    unit, unit->size);
}
#endif

/* a free unit must lie in the page table */
static inline void PhyMemPartAllocPhyPage1MCoreAssertRaw(PhyMemPart *part, PhyPage *unit)
{
#if FW_DEBUG
	if (PhyPageTableMgrCheckPhyPage(s_phyPageTableMgr, unit, 0) < 0) {
		PhyMemPartShowUnitUntilRaw(part, unit);
		SCE_DBG_ASSERT_CHECK_FUNC(0, 2545, "PhyMemPartAllocPhyPage1MCoreAssertRaw");
	}
#endif
}

/* a whole free unit off the list */
static inline void PhyMemPartTakePhyPage1M(PhyPage *unit, PhyPage **page, SceUInt32 *pindex)
{
	SceUInt32 info = unit->info;

	unit->info = (info & PHYPAGE_ORDER_MASK) | (info & (PHYPAGE_SIZED_MASK | 0xfffff)) |
		     PHYPAGE_LINK_MASK;
	unit->prev->next = unit->next;
	unit->next->prev = unit->prev;
	*page = unit;
	*pindex = PHYPAGE_PINDEX(info);
}

/*
 * 1 MiB units from the free list: the first fit from the low end, or from the
 * high end with flags bit 1; (flags & 0x30) == 0x20 also takes a smaller unit.
 */
int PhyMemPartAllocPhyPage1MCoreAssertRaw2(PhyMemPart *part, SceSize size, SceUInt32 flags,
					   PhyPage **page, SceUInt32 *pindex)
{
	PhyPage *head, *unit, *rest;
	SceUInt32 start, split;
	SceUInt32 fromHigh;
#if FW_DEBUG
	PhyPageTable *table;
#endif

	if (size & 0xfffff)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	*page = NULL;
	head = part->freeList;
	if (head == head->next) {
		PhyMemPartCannotAllocateRaw(part, size, "try 1M");
#if FW_DEBUG
		if (part->type & PHYMEM_PART_SHOW_ON_FAIL)
			PhyMemPartShowFreeRaw(part);
#endif
		*page = NULL;
		return SCE_KERNEL_ERROR_NO_FREE_PHYSICAL_PAGE_UNIT;
	}
	fromHigh = flags & 2;
	for (unit = head; ; head = part->freeList) {
		unit = fromHigh ? unit->prev : unit->next;
		if (unit == head)
			break;
		PhyMemPartAllocPhyPage1MCoreAssertRaw(part, unit);
		if (unit->size == size) {
			PhyMemPartTakePhyPage1M(unit, page, pindex);
			return 0;
		}
		if (unit->size > size) {
			start = PHYPAGE_PINDEX(unit->info);
			if (fromHigh) {
				/* the top of the unit */
				split = start + ((unit->size - size) >> 20) * PHYMEM_PAGES_PER_1M;
				rest = PhyMemPartGetPhyPageByIndexRaw(part, split);
				rest->size = size;
				rest->info |= PHYPAGE_SIZED_MASK;
				unit->size -= size;
				rest->info = (rest->info & (PHYPAGE_SIZED_MASK | PHYPAGE_ORDER_MASK)) |
					     PHYPAGE_LINK_MASK | PHYPAGE_PINDEX(split);
				*page = rest;
				*pindex = split;
				return 0;
			}
			/* the bottom; the rest stays on the list */
			split = start + (size >> 20) * PHYMEM_PAGES_PER_1M;
			rest = PhyMemPartGetPhyPageByIndexRaw(part, split);
#if FW_DEBUG
			if (rest == NULL) {
				SCE_DBG_PRINTF(0xb, 2561, "%p [%s]\n", part, part->name);
				SCE_DBG_PRINTF(0xb, 2562, "Found large area\n");
				SCE_DBG_PRINTF(0xb, 2566, "FROM LOW: %p<-%p->%p\n", unit->prev, unit,
					       unit->next);
				SCE_DBG_PRINTF(0xb, 2567, "pindex1=0x%08x\n", split);
				table = part->segments->tables->tables[0];
				SCE_DBG_PRINTF(0xb, 2577, "%p: 0x%08x:0x%08x-0x%08x\n", table,
					       table->basePindex, table->pindexMin, table->pindexMax);
				SCE_DBG_PRINTF(0xb, 2580, "%p: 0x%08x:0x%08x-0x%08x\n", table,
					       table->entries, table->pbase, table->pbaseEnd);
				table = part->segments->tables->tables[1];
				SCE_DBG_PRINTF(0xb, 2585, "%p: 0x%08x:0x%08x-0x%08x\n", table,
					       table->basePindex, table->pindexMin, table->pindexMax);
				SCE_DBG_PRINTF(0xb, 2588, "%p: 0x%08x:0x%08x-0x%08x\n", table,
					       table->entries, table->pbase, table->pbaseEnd);
				SCE_DBG_ASSERT_FAIL(2590);
			}
#endif
			rest->info |= PHYPAGE_SIZED_MASK;
			rest->size = unit->size - size;
			unit->size = size;
			rest->info = (rest->info & (PHYPAGE_SIZED_MASK | PHYPAGE_ORDER_MASK)) |
				     PHYPAGE_LINK_MORE | PHYPAGE_PINDEX(split);
			unit->info = (unit->info & PHYPAGE_ORDER_MASK) | start |
				     (unit->info & PHYPAGE_SIZED_MASK) | PHYPAGE_LINK_MASK;
			rest->prev = unit->prev;
			rest->next = unit->next;
			unit->next->prev = rest;
			unit->prev->next = rest;
			*page = unit;
			*pindex = start;
			return 0;
		}
		if ((flags & 0x30) == 0x20) {
			PhyMemPartTakePhyPage1M(unit, page, pindex);
			return 0;
		}
	}
	PhyMemPartCannotAllocateRaw(part, size, "Try 1M");
	/* a walk of the free list whose body is compiled out */
	for (unit = part->freeList->next; unit != part->freeList; unit = unit->next)
		;
	return SCE_KERNEL_ERROR_NO_FREE_PHYSICAL_PAGE_UNIT;
}

/* phymem_low.c */
int PhyMemLowMainAlloc(SceSize size, SceSize alignment, SceUInt32 flags, uintptr_t *pa,
		       SceSize *allocated);
int PhyMemLowMainMarkUsed(uintptr_t pa, SceSize size);
int PhyMemLowCreate(uintptr_t base, SceSize size, PhyMemLow **out);
int PhyMemLowStaticAlloc(SceSize size, SceSize alignment, uintptr_t *pa);
int PhyMemLowStaticMarkUsed(uintptr_t pa, SceSize size);

uintptr_t sceKernelPhyPageTableMgrGetPbase(PhyPageTableMgr *mgr)
{
	SCE_DBG_ASSERT_CHECK(mgr != NULL, 1205);
	SCE_DBG_ASSERT_CHECK(mgr->tables[0] != NULL, 1206);
	return mgr->tables[0]->pbase;
}

/* name unknown: a segment over [pbase, pbase + psize), once */
int PhyMemSegmentInit(PhyMemSegment *segment, PhyPageTableMgr *mgr, uintptr_t pbase,
		      SceSize psize)
{
	if (segment->initialized == 1)
		return 0;
	if ((pbase & 0xfffff) || (psize & 0xfffff))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	segment->psize = psize;
	segment->pbase = pbase;
	segment->tableIndex = (pbase - sceKernelPhyPageTableMgrGetPbase(mgr)) >> PHYMEM_PAGE_SHIFT;
	segment->tables = mgr;
	segment->pindexEnd = (psize + pbase) >> PHYMEM_PAGE_SHIFT;
	segment->pindexStart = pbase >> PHYMEM_PAGE_SHIFT;
	segment->initialized = 1;
	return 0;
}

/* the segment's memory, taken from the main (LPDDR2) or static (CDRAM) allocator */
int PhyMemPartCreatePhyMemPartSegmentByPbaseRaw(PhyMemPart *part, PhyMemSegment *segment,
						uintptr_t pbase, SceSize psize, SceSize *allocated)
{
	PhyMemLow *pml;
	int ret;

	switch (PHYMEM_PART_TYPE(part)) {
	case PHYMEM_PART_TYPE_LPDDR2:
		if (pbase == 0) {
			ret = PhyMemLowMainAlloc(psize, 0, 1, &pbase, allocated);
			if (ret < 0)
				return ret;
		} else {
			ret = PhyMemLowMainMarkUsed(pbase, psize);
			if (ret < 0) {
				SCE_DBG_PRINTF(0xb, 3263,
					       "PhyMemLpddr2MainAllocByAddr(0x%08x,0x%08x) failed 0x%08x\n",
					       pbase, psize, ret);
				return ret;
			}
		}
		ret = PhyMemSegmentInit(segment, s_phyPageTableMgr, pbase, psize);
		return ret < 0 ? ret : 0;
	case PHYMEM_PART_TYPE_CDRAM:
		if (pbase == 0)
			ret = PhyMemLowStaticAlloc(psize, 0, &pbase);
		else
			ret = PhyMemLowStaticMarkUsed(pbase, psize);
		if (ret < 0)
			return ret;
		ret = PhyMemLowCreate(pbase, psize, &pml);
		if (ret < 0)
			return ret;
		if (segment->phyMemLow == NULL) {
			if ((pbase & 0xfffff) || (psize & 0xfffff))
				return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
			segment->pbase = pbase;
			segment->psize = psize;
		}
		segment->phyMemLow = pml;
		return 0;
	default:
		SCE_DBG_ASSERT_FAIL(3315);
		return SCE_KERNEL_ERROR_ERROR;
	}
}

/* name unknown: a partition over its first segment, all free */
int PhyMemPartInitWithSegmentRaw(PhyMemPart *part, PhyPageTableMgr *mgr, uintptr_t pbase,
				 SceSize psize)
{
	int ret = PhyMemSegmentInit(part->segments, mgr, pbase, psize);

	if (ret >= 0)
		ret = PhyMemPartInitWoSegmentRaw(part);
	return ret;
}

/* whether a segment of the partition holds pindex */
static inline __attribute__((always_inline)) int PhyMemPartHasIndex(PhyMemPart *part, SceUInt32 pindex)
{
	PhyMemSegment *segment;

	for (segment = part->segments; segment != NULL; segment = segment->next)
		if (segment->pindexStart <= pindex && pindex < segment->pindexEnd)
			return 1;
	return 0;
}

/* a single page from this CPU's cache, NULL when it has none */
static inline PhyPage *PhyMemPartCpuCacheAllocRaw(PhyMemPart *part)
{
	PhyMemPartCpuCache *cache;
	PhyPage *page;
	int intr, i;

	intr = ksceKernelCpuSuspendIntr();
	if (part->cpuCache != NULL) {
		cache = &part->cpuCache[SceCpuId()];
		if (cache != NULL && cache->count != 0) {
			for (i = 0; i < cache->max; i++) {
				page = cache->pages[i];
				if (page != NULL) {
					cache->count--;
					cache->pages[i] = NULL;
					ksceKernelCpuResumeIntr(intr);
					return page;
				}
			}
		}
	}
	ksceKernelCpuResumeIntr(intr);
	return NULL;
}

/* a block of 4 KiB .. 512 KiB from the buddies, splitting larger ones; 1 MiB and up whole */
static inline int PhyMemPartAllocPhyPageBuddyRaw(PhyMemPart *part, SceSize size, SceUInt32 flags,
						 PhyPage **pPage)
{
	PhyPage *page, *half;
	SceUInt32 pindex, next, info, order, i, newOrder;
	int intr, ret;

	if (flags & ~0x33)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&part->lock);
	if (size >= 0x100000) {
		ret = PhyMemPartAllocPhyPage1MCoreAssertRaw2(part, size, flags | 1, pPage, &pindex);
		if (ret < 0)
			goto failed;
		page = *pPage;
		goto allocated;
	}
	if (size != 1u << (31 - __builtin_clz(size)) || size < 0x1000) {
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		goto failed;
	}
	order = 31 - __builtin_clz(size) - PHYMEM_PAGE_SHIFT;
	if (part->buddy[order].state == PHYMEM_BUDDY_EMPTY) {
		/* the smallest larger block, a 1 MiB unit at the end */
		for (i = order + 1; ; i++) {
			if (i > PHYMEM_BUDDY_ORDERS - 2) {
				ret = PhyMemPartAllocPhyPage1MCoreAssertRaw2(part, 0x100000,
						(part->type & PHYMEM_PART_ALLOC_LOW) ? 0x11 : 0x13,
						&page, &pindex);
				if (ret < 0)
					goto failed_clear;
				page->info = PHYPAGE_BUDDY_FREE_LAST | (20 << 20);
				part->buddy[PHYMEM_BUDDY_ORDERS - 1].state = PHYMEM_BUDDY_HAS;
				if (PhyMemPartHasIndex(part, pindex))
					part->buddy[PHYMEM_BUDDY_ORDERS - 1].pindex = pindex;
			}
			if (part->buddy[i].state == PHYMEM_BUDDY_HAS)
				break;
		}
		/* halve it down to the order asked for */
		while (order < i) {
			pindex = part->buddy[i].pindex;
			page = PhyMemPartGetPhyPageByIndexRaw(part, pindex);
			switch (page->info & PHYPAGE_LINK_MASK) {
			case PHYPAGE_LINK_MORE:
				next = PHYPAGE_PINDEX(page->info);
				part->buddy[i].state = PHYMEM_BUDDY_HAS;
				if (PhyMemPartHasIndex(part, next))
					part->buddy[i].pindex = next;
				break;
			case PHYPAGE_LINK_LAST:
				part->buddy[i].state = PHYMEM_BUDDY_EMPTY;
				part->buddy[i].pindex = 0;
				break;
			default:
				SCE_DBG_ASSERT_FAIL(3682);
				ret = SCE_KERNEL_ERROR_PHYMEM_ERROR;
				goto failed_clear;
			}
			i--;
			if (PhyMemPartHasIndex(part, pindex)) {
				part->buddy[i].state = PHYMEM_BUDDY_HAS;
				part->buddy[i].pindex = pindex;
			}
			newOrder = ((page->info & PHYPAGE_ORDER_MASK) - (1 << 20)) & PHYPAGE_ORDER_MASK;
			next = pindex + (1 << (PHYPAGE_ORDER(newOrder) - PHYMEM_PAGE_SHIFT));
			page->info = PHYPAGE_PINDEX(next) | (page->info & PHYPAGE_SIZED_MASK) |
				     PHYPAGE_LINK_MORE | newOrder;
			half = PhyMemPartGetPhyPageByIndexRaw(part, next);
			half->info = (page->info & PHYPAGE_ORDER_MASK) | PHYPAGE_BUDDY_FREE_LAST;
			part->buddy[i].state = PHYMEM_BUDDY_HAS;
			if (PhyMemPartHasIndex(part, pindex))
				part->buddy[i].pindex = pindex;
		}
	} else if (part->buddy[order].state != PHYMEM_BUDDY_HAS) {
		SCE_DBG_ASSERT_FAIL(3714);
	}

	/* the first block of the order off its list */
	pindex = part->buddy[order].pindex;
	page = PhyMemPartGetPhyPageByIndexRaw(part, pindex);
	info = page->info;
	switch (info & PHYPAGE_LINK_MASK) {
	case PHYPAGE_LINK_MORE:
		next = PHYPAGE_PINDEX(info);
		if (PhyMemPartHasIndex(part, next)) {
			part->buddy[order].state = PHYMEM_BUDDY_HAS;
			part->buddy[order].pindex = next;
		}
		break;
	case PHYPAGE_LINK_LAST:
		part->buddy[order].state = PHYMEM_BUDDY_EMPTY;
		part->buddy[order].pindex = 0;
		break;
	default:
		SCE_DBG_PRINTF(0xb, 3732, "illegal page status for %p:pindex=0x%08x\n", page,
			       pindex);
		SCE_DBG_ASSERT_FAIL(3733);
		ret = SCE_KERNEL_ERROR_PHYMEM_ERROR;
		goto failed_clear;
	}
	page->info = (info & PHYPAGE_ORDER_MASK) | PHYPAGE_PINDEX(pindex) | PHYPAGE_BUDDY_USED;
	*pPage = page;
allocated:
	size = PhyPageGetSize(page);
	PhyMemPartSubFreeSize(part, size);
#if FW_DEBUG
	PhyMemPartCheckAndFixFreeSizeRaw(part, "Sub", size);
#endif
#if FW_DEBUG
	PhyMemPartCheckRaw(part);
#endif
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&part->lock, intr);
	return 0;

failed_clear:
	*pPage = NULL;
failed:
#if FW_DEBUG
	PhyMemPartCheckRaw(part);
#endif
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&part->lock, intr);
	PhyMemPartCannotAllocateRaw(part, size, "Final");
	return ret;
}

int PhyMemPartAllocPhyPage(PhyMemPart *part, SceSize size, SceUInt32 flags, PhyPage **page)
{
	int ret;

	PhyMemPartRefer(part);
	if (!(part->flags & PHYMEM_PART_ACTIVE)) {
		SCE_DBG_PRINTF(0xb, 4191, "pPhyMemPart is not active\n");
		ret = SCE_KERNEL_ERROR_PHYMEMPART_NOT_ACTIVE;
		goto out;
	}
	if (PHYMEM_PART_TYPE(part) != PHYMEM_PART_TYPE_LPDDR2) {
		SCE_DBG_ASSERT_FAIL(4202);
		ret = SCE_KERNEL_ERROR_PHYMEM_ERROR;
		goto out;
	}
	if (size == 0x1000 && (part->type & PHYMEM_PART_HAS_CPU_CACHE)) {
		*page = PhyMemPartCpuCacheAllocRaw(part);
		if (*page != NULL)
			goto allocated;
	}
	ret = PhyMemPartAllocPhyPageBuddyRaw(part, size, flags, page);
	if (ret < 0)
		goto out;
allocated:
	ret = 0;
#if FW_DEBUG
	if (s_phyMemPartHook != NULL)
		s_phyMemPartHook->alloc(part, size, *page);
#endif
out:
	PhyMemPartRelease(part);
	return ret;
}

#if FW_DEBUG
/* the page must lie in one of the partition's page tables (a miss panics) */
static inline void PhyMemPartCheckPhyPageR1(PhyMemPart *part, PhyPage *page)
{
	PhyMemSegment *segment;
	int intr;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&part->lock);
	switch (PHYMEM_PART_TYPE(part)) {
	case PHYMEM_PART_TYPE_LPDDR2:
		for (segment = part->segments; segment != NULL; segment = segment->next)
			if (PhyPageTableMgrCheckPhyPage(segment->tables, page, 1) >= 0)
				break;
		break;
	case PHYMEM_PART_TYPE_CDRAM:
		SCE_DBG_ASSERT_FUNC(0, 4152, "PhyMemPartCheckPhyPageR1");
		break;
	default:
		SCE_DBG_ASSERT_FUNC(0, 4156, "PhyMemPartCheckPhyPageR1");
		break;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&part->lock, intr);
}
#endif

/* a single page back into this CPU's cache; 0 when the cache is full */
static inline int PhyMemPartCpuCacheFreeRaw(PhyMemPart *part, PhyPage *page)
{
	PhyMemPartCpuCache *cache;
	int intr, i;

	intr = ksceKernelCpuSuspendIntr();
	if (part->cpuCache != NULL) {
		cache = &part->cpuCache[SceCpuId()];
		if (cache != NULL && cache->count < cache->max) {
			for (i = 0; i < cache->max; i++) {
				if (cache->pages[i] == NULL) {
					cache->pages[i] = page;
					cache->count++;
					ksceKernelCpuResumeIntr(intr);
					return 1;
				}
			}
		}
	}
	ksceKernelCpuResumeIntr(intr);
	return 0;
}

/* whether low and high are the two halves of one block of the next order */
static inline int PhyMemPartIsBuddy(SceUInt32 low, SceUInt32 high, SceUInt32 order)
{
	return !((low >> order) & 1) && high == low + (1 << order);
}

/* the link word: on to next, or the end of the list */
static inline SceUInt32 PhyPageLinkTo(SceUInt32 info, SceUInt32 next)
{
	if (next == 0xffffffff)
		return (info & (PHYPAGE_SIZED_MASK | PHYPAGE_ORDER_MASK)) | PHYPAGE_LINK_LAST;
	return (info & (PHYPAGE_SIZED_MASK | PHYPAGE_ORDER_MASK)) | PHYPAGE_LINK_MORE |
	       PHYPAGE_PINDEX(next);
}

/* the head of a buddy list, or the list empty */
static inline __attribute__((always_inline)) void PhyMemPartBuddySetHead(PhyMemPart *part, SceUInt32 order, SceUInt32 pindex)
{
	if (pindex == 0xffffffff) {
		part->buddy[order].state = PHYMEM_BUDDY_EMPTY;
		part->buddy[order].pindex = 0;
		return;
	}
	part->buddy[order].state = PHYMEM_BUDDY_HAS;
	if (PhyMemPartHasIndex(part, pindex))
		part->buddy[order].pindex = pindex;
}

/*
 * A free block into its buddy list, kept sorted by page index. While its
 * buddy is free too, the two are merged and go one order up.
 */
static inline int PhyMemPartAddToListRaw(PhyMemPart *part, PhyPage *page, SceUInt32 order)
{
	PhyPage *link;
	SceUInt32 pindex, prevprev, prev, next, after;

	for (;; order++) {
		pindex = PHYPAGE_PINDEX(page->info);
		if (part->buddy[order].state == PHYMEM_BUDDY_EMPTY) {
			PhyMemPartBuddySetHead(part, order, pindex);
			page->info = (page->info & (PHYPAGE_SIZED_MASK | PHYPAGE_ORDER_MASK)) |
				     PHYPAGE_LINK_LAST;
			return 0;
		}
		if (part->buddy[order].state != PHYMEM_BUDDY_HAS) {
			SCE_DBG_ASSERT_FAIL(2904);
			return SCE_KERNEL_ERROR_ILLEGAL_PHYPAGE_STATUS;
		}

		/* prev < pindex < next; next is 0xffffffff at the end of the list */
		prevprev = 0;
		prev = 0;
		next = part->buddy[order].pindex;
		while (pindex <= prev || next <= pindex) {
			link = PhyMemPartGetPhyPageByIndexRaw(part, next);
			if (link == NULL) {
				SCE_DBG_PRINTF(0xb, 2923, "PhyMemPartGetPhyPage returns SCE_NULL\n");
				return SCE_KERNEL_ERROR_PHYMEM_ERROR;
			}
			prevprev = prev;
			prev = next;
			switch (link->info & PHYPAGE_LINK_MASK) {
			case PHYPAGE_LINK_MORE:
				next = PHYPAGE_PINDEX(link->info);
				break;
			case PHYPAGE_LINK_LAST:
				next = 0xffffffff;
				break;
			default:
				SCE_DBG_ASSERT_FAIL(2932);
				break;
			}
		}

		if (order == PHYMEM_BUDDY_ORDERS - 1)
			goto insert;
		if ((pindex >> order) & 1) {
			/* the upper half: merge with prev */
			if (prev == 0)
				goto insert;
			if (!PhyMemPartIsBuddy(prev, pindex, order))
				goto insert;
			PhyMemPartBuddyMergeRaw(part, prev, pindex);
			page = PhyMemPartGetPhyPageByIndexRaw(part, prev);
			if (page == NULL) {
				SCE_DBG_PRINTF(0xb, 2948,
					       "PhyMemPartGetPhyPageByIndexRaw for *ppPageNew returns SCE_NULL\n");
				return SCE_KERNEL_ERROR_PHYMEM_ERROR;
			}
			if (prevprev != 0) {
				link = PhyMemPartGetPhyPageByIndexRaw(part, prevprev);
				if (link == NULL) {
					SCE_DBG_PRINTF(0xb, 2959,
						       "PhyMemPartGetPhyPageByIndexRaw for pPagePP returns SCE_NULL\n");
					return SCE_KERNEL_ERROR_PHYMEM_ERROR;
				}
				link->info = PhyPageLinkTo(link->info, next);
			} else {
				PhyMemPartBuddySetHead(part, order, next);
			}
		} else {
			/* the lower half: merge with next */
			if (next == 0xffffffff || !PhyMemPartIsBuddy(pindex, next, order))
				goto insert;
			link = PhyMemPartGetPhyPageByIndexRaw(part, next);
			if (link == NULL) {
				SCE_DBG_PRINTF(0xb, 2975,
					       "PhyMemPartGetPhyPage for pPageN returns SCE_NULL\n");
				return SCE_KERNEL_ERROR_PHYMEM_ERROR;
			}
			after = (link->info & PHYPAGE_LINK_MASK) == PHYPAGE_LINK_LAST ?
				0xffffffff : PHYPAGE_PINDEX(link->info);
			PhyMemPartBuddyMergeRaw(part, pindex, next);
			if (prev == 0) {
				PhyMemPartBuddySetHead(part, order, after);
			} else {
				link = PhyMemPartGetPhyPageByIndexRaw(part, prev);
				if (link == NULL) {
					SCE_DBG_PRINTF(0xb, 2995,
						       "PhyMemPartGetPhyPageByIndexRaw for pPageP returns SCE_NULL\n");
					return SCE_KERNEL_ERROR_PHYMEM_ERROR;
				}
				link->info = PhyPageLinkTo(link->info, after);
			}
		}
	}

insert:
	if (prev == 0) {
		PhyMemPartBuddySetHead(part, order, pindex);
	} else {
		link = PhyMemPartGetPhyPageByIndexRaw(part, prev);
		if (link == NULL) {
			SCE_DBG_PRINTF(0xb, 3015, "PhyMemPartGetPhyPage for pPageP returns SCE_NULL\n");
			return SCE_KERNEL_ERROR_PHYMEM_ERROR;
		}
		link->info = PhyPageLinkTo(link->info, pindex);
	}
	page->info = PhyPageLinkTo(page->info, next);
	return 0;
}

static inline int PhyMemPartFreePhyPageBuddyRaw(PhyMemPart *part, PhyPage *page)
{
	PhyPage *unit;
	SceUInt32 pindex = PHYPAGE_PINDEX(page->info), order;
	int ret;

	if (!PhyMemPartHasIndex(part, pindex))
		return SCE_KERNEL_ERROR_PHYMEMPART_OUT_OF_INDEX;
	order = PHYPAGE_ORDER(page->info) - PHYMEM_PAGE_SHIFT;
	if (order > PHYMEM_BUDDY_ORDERS - 1)
		return 0;
	ret = PhyMemPartAddToListRaw(part, page, order);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 3926, "PhyMemPartAddToList returns 0x%08x\n", ret);
		SCE_DBG_ASSERT_FAIL(3927);
		return ret;
	}

	/* a whole 1 MiB block free again goes back to the free list */
	switch (part->buddy[PHYMEM_BUDDY_ORDERS - 1].state) {
	case PHYMEM_BUDDY_EMPTY:
		break;
	case PHYMEM_BUDDY_HAS:
		pindex = part->buddy[PHYMEM_BUDDY_ORDERS - 1].pindex;
		unit = PhyMemPartGetPhyPageByIndexRaw(part, pindex);
		unit->size = 0x100000;
		unit->info = PHYPAGE_PINDEX(pindex) | (unit->info & 0x3ff00000) | PHYPAGE_SIZED_MASK;
		PhyMemPartInsertFreePhyPage1MRaw(part, unit);
		part->buddy[PHYMEM_BUDDY_ORDERS - 1].state = PHYMEM_BUDDY_EMPTY;
		part->buddy[PHYMEM_BUDDY_ORDERS - 1].pindex = 0;
		break;
	default:
		SCE_DBG_ASSERT_FAIL(3917);
		break;
	}
	return 0;
}

int PhyMemPartFreePhyPage(PhyMemPart *part, PhyPage *page)
{
	SceSize size;
	int intr, ret;

	PhyMemPartRefer(part);
	if (!(part->flags & PHYMEM_PART_ACTIVE)) {
		SCE_DBG_PRINTF(0xb, 4287, "pPhyMemPart is not active\n");
		ret = SCE_KERNEL_ERROR_PHYMEMPART_NOT_ACTIVE;
		goto out;
	}
#if FW_DEBUG
	if (s_phyMemPartHook != NULL)
		s_phyMemPartHook->free(part, page);
#endif
	if (PHYMEM_PART_TYPE(part) != PHYMEM_PART_TYPE_LPDDR2) {
		SCE_DBG_ASSERT_FAIL(4300);
		ret = SCE_KERNEL_ERROR_PHYMEM_ERROR;
		goto out;
	}
#if FW_DEBUG
	PhyMemPartCheckPhyPageR1(part, page);
#endif
	size = PhyPageGetSize(page);
	if (size == 0x1000 && (part->type & PHYMEM_PART_HAS_CPU_CACHE) &&
	    PhyMemPartCpuCacheFreeRaw(part, page)) {
		ret = 0;
		goto out;
	}

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&part->lock);
	switch (page->info & PHYPAGE_SIZED_MASK) {
	case PHYPAGE_KIND_BUDDY:
		ret = PhyMemPartFreePhyPageBuddyRaw(part, page);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 4330, "PhyMemPartFreePhyPageBuddyRaw failed 0x%08x\n", ret);
			goto unlock;
		}
		break;
	case PHYPAGE_SIZED_MASK:
		ret = PhyMemPartInsertFreePhyPage1MRaw(part, page);
		if (ret < 0)
			goto unlock;
		break;
	default:
		SCE_DBG_ASSERT_FAIL(4341);
		ret = SCE_KERNEL_ERROR_ERROR;
		goto unlock;
	}
	ksceKernelAtomicAddAndGet32((SceInt32 *)&part->freeSize, size);
#if FW_DEBUG
	PhyMemPartCheckAndFixFreeSizeRaw(part, "Add", size);
#endif
unlock:
#if FW_DEBUG
	PhyMemPartCheckRaw(part);
#endif
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&part->lock, intr);
out:
	PhyMemPartRelease(part);
	return ret;
}

/* the free size, recounted */
SceSize PhyMemPartGetFreeSizeInternal(PhyMemPart *part)
{
	SceSize size = 0;
	int intr;

	PhyMemPartRefer(part);
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&part->lock);
	if (!(part->flags & PHYMEM_PART_ACTIVE))
		SCE_DBG_PRINTF(0xb, 4368, "Warnig: pPhyMemPart is not active\n");
	PhyMemPartGetFreeSizeRaw(part, &size);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&part->lock, intr);
	PhyMemPartRelease(part);
	return size;
}

int PhyMemPartCdramAlloc(PhyMemPart *part, SceSize size, SceSize alignment, uintptr_t *pa)
{
	if (PHYMEM_PART_TYPE(part) == PHYMEM_PART_TYPE_CDRAM)
		return PhyMemPartCdramAllocCore(part, size, alignment, pa);
	SCE_DBG_ASSERT_FAIL(4486);
	return SCE_KERNEL_ERROR_PHYMEM_ERROR;
}

int PhyMemPartCdramFree(PhyMemPart *part, uintptr_t pa, SceSize size)
{
	if (PHYMEM_PART_TYPE(part) == PHYMEM_PART_TYPE_CDRAM)
		return PhyMemPartCdramFreeCore(part, pa, size);
	SCE_DBG_ASSERT_FAIL(4505);
	return SCE_KERNEL_ERROR_PHYMEM_ERROR;
}

/* sceKernelGetPhyMemPartInfoCore */
static inline void PhyMemPartGetInfoCoreRaw(PhyMemPart *part, SceKernelPhyMemPartInfoCore *info)
{
	PhyMemSegment *segment;
	PhyPage *page;
	SceSize real = 0;

	SCE_DBG_PRINTF(0xb, 4081, "(%p)\n", part);
	info->psize = 0;
	info->freeUnitCount = 0;
	info->pbase = 0xffffffff;
	for (segment = part->segments; segment != NULL; segment = segment->next) {
		if (segment->pbase < info->pbase)
			info->pbase = segment->pbase;
		info->psize += segment->psize;
	}
	PhyMemPartGetFreeSizeRaw(part, &real);
	if ((SceSize)part->freeSize != real) {
		SCE_DBG_PRINTF(0xb, 4099, "Warning:mismatch:[%s]:0x%08x <- 0x%08x\n", part->name,
			       part->freeSize, real);
		part->freeSize = real;
	}
	info->freeSize = part->freeSize;
	switch (PHYMEM_PART_TYPE(part)) {
	case PHYMEM_PART_TYPE_LPDDR2:
		for (page = part->freeList->next; page != part->freeList; page = page->next)
			info->freeUnitCount++;
		break;
	case PHYMEM_PART_TYPE_CDRAM:
		break;
	default:
		SCE_DBG_ASSERT_FAIL(4124);
		break;
	}
}

int PhyMemPartGetInfoCore(PhyMemPart *part, SceKernelPhyMemPartInfoCore *info)
{
	int intr, ret;

	PhyMemPartRefer(part);
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&part->lock);
	if (part->flags & PHYMEM_PART_ACTIVE) {
		PhyMemPartGetInfoCoreRaw(part, info);
		ret = 0;
	} else {
		SCE_DBG_PRINTF(0xb, 4560, "pPhyMemPart is not active\n");
		ret = SCE_KERNEL_ERROR_PHYMEMPART_NOT_ACTIVE;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&part->lock, intr);
	PhyMemPartRelease(part);
	return ret;
}

int sceKernelGetPhyMemPartInfoCore(PhyMemPart *part, SceKernelPhyMemPartInfoCore *info)
{
	int ret;

	if (info == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = PhyMemPartGetInfoCore(part, info);
	SCE_DBG_ASSERT_CHECK((info->pbase & 0xfffff) == 0, 3159);
	SCE_DBG_ASSERT_CHECK((info->psize & 0xfffff) == 0, 3160);
	return ret;
}

/* name unknown */
const char *PhyMemPartGetName(PhyMemPart *part)
{
	PhyMemPartRefer(part);
	PhyMemPartRelease(part);
	return part->name;
}

/* name unknown: the partition's attribute bits (type & 0x7f) within mask */
SceUInt32 PhyMemPartTestAttr(PhyMemPart *part, SceUInt32 mask)
{
	SceUInt32 type;

	PhyMemPartRefer(part);
	type = part->type;
	PhyMemPartRelease(part);
	return type & 0x7f & mask;
}

/* sceKernelAllocWithOptForKernel */
void *SceSysmemForKernel_CF3C3F7A(SceSize size, SceAllocOpt *opt);
int PhyMemLowMainFree(uintptr_t pa, SceSize size);

/* the segments' memory back to the main allocator, then the segments and cache freed */
static inline void PhyMemPartRecoveryRaw(PhyMemPart *part)
{
	PhyMemSegment *segment, *next;

	SCE_DBG_PRINTF(0xb, 3222, "()\n");
	if (part->cpuCache != NULL) {
		ksceKernelFree(part->cpuCache);
		part->cpuCache = NULL;
	}
	for (segment = part->segments; segment != NULL; segment = next) {
		PhyMemLowMainFree(segment->pbase, segment->psize);
		next = segment->next;
		ksceKernelFree(segment);
	}
	part->segments = NULL;
}

/*
 * A partition of psize bytes at pbase (0: anywhere). When one contiguous
 * LPDDR2 range cannot be had, the partition is built from a chain of the
 * largest free ranges instead (unless type bit 0 forbids it).
 */
int CreatePhyMemPartByPbase(const char *name, SceUInt32 type, uintptr_t pbase, SceSize psize,
			    PhyMemPart **out)
{
	SceAllocOpt opt;
	PhyMemPart *part;
	PhyMemSegment *segment, *prev;
	uintptr_t chunkBase;
	SceSize chunkSize;
	int ret;

	if (out == NULL) {
		SCE_DBG_ASSERT_FAIL(3362);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	opt.size = sizeof(opt);
	opt.align = 0x20;
	part = SceSysmemForKernel_CF3C3F7A(sizeof(*part), &opt);
	if (part == NULL)
		return SCE_KERNEL_ERROR_ERROR;
	part->type = type;
	part->refCount = 0;
	strncpy(part->name, name, sizeof(part->name) - 1);
	part->name[sizeof(part->name) - 1] = '\0';
	part->cpuCache = SceSysmemForKernel_CF3C3F7A(sizeof(*part->cpuCache) * PHYMEM_CPU_COUNT,
						     &opt);
	part->segments = PhyMemSegmentCreate();
	part->segments->next = NULL;
	PhyMemPartRefer(part);

	ret = PhyMemPartCreatePhyMemPartSegmentByPbaseRaw(part, part->segments, pbase, psize,
							  &chunkSize);
	if (ret < 0) {
		if (pbase != 0 || (type & 1))
			goto failed;
		SCE_DBG_PRINTF(0xb, 3415, "Try chain allocation\n");
		SCE_DBG_PRINTF(0xb, 3416, "name [%s], psize=0x%08x, %p\n", name, psize, out);
		segment = part->segments;
		prev = NULL;
		while (psize != 0) {
			ret = PhyMemLowMainAlloc(psize, 0, 2, &chunkBase, &chunkSize);
			if (ret < 0) {
				SCE_DBG_PRINTF(0xb, 3430, "PhyMemLpddr2MainAlloc(0x%08x, FF) failed\n",
					       psize);
				if (segment != part->segments) {
					SCE_DBG_PRINTF(0xb, 3432,
						       "->FreePhyMemPartSegmentObject(p1=%p)\n", segment);
					ksceKernelFree(segment);
				}
				goto recovery;
			}
			SCE_DBG_PRINTF(0xb, 3439,
				       "chain:FF:OK,prest=0x%08x,pbase=0x%08x,rsize=0x%08x\n", psize,
				       chunkBase, chunkSize);
			ret = PhyMemSegmentInit(segment, s_phyPageTableMgr, chunkBase, chunkSize);
			if (ret < 0) {
				if (segment != part->segments) {
					SCE_DBG_PRINTF(0xb, 3445,
						       "->FreePhyMemPartSegmentObject(p1=%p)\n", segment);
					ksceKernelFree(segment);
				}
				goto recovery;
			}
			if (segment != part->segments) {
				prev->next = segment;
				ksceKernelAtomicAddAndGet32((SceInt32 *)&part->freeSize, segment->psize);
			}
			psize -= chunkSize;
			SCE_DBG_PRINTF(0xb, 3458, "prest=0x%08x\n", psize);
			if (psize == 0)
				break;
			prev = segment;
			segment = PhyMemSegmentCreate();
		}
		ret = PhyMemPartInitWoSegmentRaw(part);
		if (ret < 0)
			goto failed;
		SCE_DBG_PRINTF(0xb, 3474, "chain done\n");
		goto created;
	}
	switch (type & 0xf0000) {
	case PHYMEM_PART_TYPE_LPDDR2:
		ret = PhyMemPartInitWithSegmentRaw(part, s_phyPageTableMgr, pbase, psize);
		break;
	case PHYMEM_PART_TYPE_CDRAM:
		ret = PhyMemPartInitCdramRaw(part, pbase, psize);
		break;
	default:
		ret = SCE_KERNEL_ERROR_ERROR;
		goto failed;
	}
	if (ret < 0)
		goto failed;
created:
	PhyMemPartRelease(part);
	*out = part;
	part->base.sce_class = s_phyMemPartClass;
	part->uid = SceSysmemForKernel_6DB44A42(s_phyMemPartClass, part->name, part);
#if FW_DEBUG
	if (s_phyMemPartHook != NULL)
		s_phyMemPartHook->create(*out);
	if (s_phyMemPartLogCallback != NULL)
		s_phyMemPartLogCallback->create(*out);
#endif
	return 0;

recovery:
	PhyMemPartRecoveryRaw(part);
	PhyMemPartRelease(part);
	SetMemory32Core(part, PHYMEM_PART_DEAD_FILL, sizeof(*part));
	ksceKernelFree(part);
	*out = NULL;
	return ret;
failed:
	PhyMemPartRelease(part);
	SCE_DBG_PRINTF(0xb, 3492, "->PhyMemPartFreeRaw(%p)\n", part);
	PhyMemPartFreeRaw(part);
	*out = NULL;
	return ret;
}

/* name unknown: a partition anywhere */
int SceSysmemForKernel_A2DFA383(const char *name, SceUInt32 type, SceSize psize, PhyMemPart **out)
{
	return CreatePhyMemPartByPbase(name, type, 0, psize, out);
}

int PhyMemLowDestroy(PhyMemLow *pml);
int PhyMemLowStaticFree(uintptr_t pa, SceSize size);

/* the size of every segment */
static inline SceSize PhyMemPartGetPsize(PhyMemPart *part)
{
	PhyMemSegment *segment;
	SceSize psize = 0;

	for (segment = part->segments; segment != NULL; segment = segment->next)
		psize += segment->psize;
	return psize;
}

/* the segments' memory back to where it came from */
static inline void PhyMemPartDeleteRaw(PhyMemPart *part)
{
	PhyMemSegment *segment;

	switch (PHYMEM_PART_TYPE(part)) {
	case PHYMEM_PART_TYPE_LPDDR2:
		for (segment = part->segments; segment != NULL; segment = segment->next)
			PhyMemLowMainFree(segment->pbase, segment->psize);
		break;
	case PHYMEM_PART_TYPE_CDRAM:
		for (segment = part->segments; segment != NULL; segment = segment->next) {
			PhyMemLowDestroy(segment->phyMemLow);
			PhyMemLowStaticFree(segment->pbase, segment->psize);
		}
		break;
	default:
		SCE_DBG_ASSERT_FAIL(3972);
		break;
	}
}

/*
 * The last delete frees the partition: type bit 1 keeps it as it is, bit 2
 * only recycles it.
 */
int sceKernelDeletePhyMemPart(PhyMemPart *part)
{
	int intr, ret;

	if (part == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
#if FW_DEBUG
	if (s_phyMemPartHook != NULL)
		s_phyMemPartHook->delete(part, __builtin_return_address(0));
#endif
	if (ksceKernelAtomicSubAndGet32(&part->useCount, 1) > 0)
		return 0;
	/* no reference may be left */
	while ((ret = ksceKernelAtomicCompareAndSet32(&part->refCount, 0, -1)) != 0)
		SCE_DBG_PRINTF(3, 4845, "PhyMemPartInvalidate failed 0x%08x, 0x%08x\n", ret,
			       part->refCount);
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&part->lock);
	if (!(part->flags & PHYMEM_PART_ACTIVE)) {
		SCE_DBG_PRINTF(0xb, 4851, "pPhyMemPart is not active\n");
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&part->lock, intr);
		return SCE_KERNEL_ERROR_PHYMEMPART_NOT_ACTIVE;
	}
	if (part->type & 2) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&part->lock, intr);
		return 0;
	}
	if (part->type & 4) {
		SCE_DBG_PRINTF(0xb, 4872, "Call Recycle for [%s]\n", part->name);
		PhyMemPartRecycleRaw(part);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&part->lock, intr);
		return 0;
	}
	if ((SceSize)part->freeSize != PhyMemPartGetPsize(part)) {
		SCE_DBG_PRINTF(6, 4882, "Warning:There is a physical alloc:pPhyMemPart=%p\n", part);
		SCE_DBG_PRINTF(6, 4884, "\tfreeSize:0x%08x!=0x%08x:psize\n", part->freeSize,
			       PhyMemPartGetPsize(part));
	}
	PhyMemPartDeleteRaw(part);
	GUIDUnregisterChecked(part->uid);
	part->flags &= ~PHYMEM_PART_ACTIVE;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&part->lock, intr);
#if FW_DEBUG
	if (s_phyMemPartHook != NULL)
		s_phyMemPartHook->delete(part, __builtin_return_address(0));
	if (s_phyMemPartLogCallback != NULL)
		s_phyMemPartLogCallback->delete(part);
#endif
	PhyMemPartFreeRaw(part);
	return 0;
}

/* the last reset drops every segment but the first and frees everything */
int sceKernelResetPhyMemPart(PhyMemPart *part)
{
	PhyMemSegment *segment, *next;
	int intr;

	SCE_DBG_PRINTF_LEVEL(2, 0xb, 4909, "(%p [%s])\n", part, PhyMemPartGetName(part));
	if (part == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (ksceKernelAtomicSubAndGet32(&part->useCount, 1) > 0)
		return 0;
	while (part->refCount != 0)
		SCE_DBG_PRINTF(3, 4929, "PhyMemPartGetARCount failed 0x%08x\n", part->refCount);
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&part->lock);
	if (!(part->flags & PHYMEM_PART_ACTIVE)) {
		SCE_DBG_PRINTF(0xb, 4935, "pPhyMemPart is not active\n");
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&part->lock, intr);
		return SCE_KERNEL_ERROR_PHYMEMPART_NOT_ACTIVE;
	}
	if ((SceSize)part->freeSize != PhyMemPartGetPsize(part)) {
		SCE_DBG_PRINTF(6, 4941, "Warning:There is a physical alloc:pPhyMemPart=%p\n", part);
		SCE_DBG_PRINTF(6, 4943, "\tfreeSize:0x%08x!=0x%08x:psize\n", part->freeSize,
			       PhyMemPartGetPsize(part));
	}
	if (PHYMEM_PART_TYPE(part) == PHYMEM_PART_TYPE_LPDDR2) {
		segment = part->segments->next;
		part->segments->next = NULL;
		for (; segment != NULL; segment = next) {
			SCE_DBG_PRINTF(0xb, 4958, "Shrink 0x%08x+0x%08x\n", segment->pbase,
				       segment->psize);
			PhyMemLowMainFree(segment->pbase, segment->psize);
			PhyMemPartSubFreeSize(part, segment->psize);
			next = segment->next;
			ksceKernelFree(segment);
		}
	} else {
		SCE_DBG_ASSERT_FAIL(4968);
	}
	PhyMemPartRecycleRaw(part);
	if (part == s_phyPartGame)
		s_phyMemPartGrownSize = 0;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&part->lock, intr);
	SCE_DBG_PRINTF_LEVEL(2, 0xb, 4981, "<-SCE_OK\n");
	return 0;
}

/* one more segment, at pbase (0: anywhere) */
int sceKernelGrowPhyMemPartByPbase(PhyMemPart *part, uintptr_t pbase, SceSize psize)
{
	PhyMemSegment *segment, *last;
	SceSize allocated;
	int intr, ret;

	PhyMemPartRefer(part);
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&part->lock);
	if (!(part->flags & PHYMEM_PART_ACTIVE)) {
		SCE_DBG_PRINTF(0xb, 5008, "pPMP is not active\n");
		ret = SCE_KERNEL_ERROR_PHYMEMPART_NOT_ACTIVE;
		goto out;
	}
	switch (PHYMEM_PART_TYPE(part)) {
	case PHYMEM_PART_TYPE_LPDDR2:
		segment = PhyMemSegmentCreate();
		if (segment == NULL) {
			ret = SCE_KERNEL_ERROR_CANNOT_GROW_PHYMEMPART;
			break;
		}
		ret = PhyMemPartCreatePhyMemPartSegmentByPbaseRaw(part, segment, pbase, psize,
								  &allocated);
		if (ret < 0) {
			ksceKernelFree(segment);
		} else {
			for (last = part->segments; last->next != NULL; last = last->next)
				;
			last->next = segment;
			ret = PhyMemPartAddFreePhyPage1MRaw(part,
							    PhyMemSegmentGetFreePhyPage1M(segment));
			if (ret >= 0) {
				ksceKernelAtomicAddAndGet32((SceInt32 *)&part->freeSize, psize);
#if FW_DEBUG
				PhyMemPartCheckAndFixFreeSizeRaw(part, "Add", psize);
#endif
				if (part == s_phyPartGame)
					s_phyMemPartGrownSize += psize;
				goto out;
			}
			SCE_DBG_PRINTF(0xb, 5054, "PhyMemPartAddFreePhyPage1MRaw failed 0x%08x\n", ret);
			ksceKernelFree(segment);
			last->next = NULL;
		}
		if (ret == SCE_KERNEL_ERROR_CANNOT_ALLOC_PHYMEMLOW)
			ret = SCE_KERNEL_ERROR_CANNOT_GROW_PHYMEMPART;
		break;
	case PHYMEM_PART_TYPE_CDRAM:
		ret = 0;
		break;
	default:
		SCE_DBG_ASSERT_FAIL(5022);
		ret = SCE_KERNEL_ERROR_PHYMEM_ERROR;
		break;
	}
out:
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&part->lock, intr);
	PhyMemPartRelease(part);
	return ret;
}

/* name unknown: grow anywhere */
int SceSysmemForKernel_5B434019(PhyMemPart *part, SceSize psize)
{
	return sceKernelGrowPhyMemPartByPbase(part, 0, psize);
}

/* name unknown: a page or block (flags 0x10) */
int SceSysmemForKernel_B488C82E(PhyMemPart *part, SceSize size, PhyPage **page)
{
	if (part == NULL || page == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	*page = NULL;
	return PhyMemPartAllocPhyPage(part, size, 0x10, page);
}

/* name unknown */
int SceSysmemForKernel_193A9D2F(PhyMemPart *part, PhyPage *page)
{
	if (page == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return PhyMemPartFreePhyPage(part, page);
}

/* name unknown: one more user, undone by sceKernelDeletePhyMemPart */
SceInt32 SceSysmemForKernel_6D563733(PhyMemPart *part)
{
	SceInt32 count;

	PhyMemPartRefer(part);
	count = ksceKernelAtomicAddAndGet32(&part->useCount, 1);
	PhyMemPartRelease(part);
	return count;
}

int sceKernelGetPhyMemPartInfoForDebugger(PhyMemPart *part, SceKernelPhyMemPartInfoForDebugger *info)
{
	if (info == NULL) {
		SCE_DBG_ASSERT_FAIL(5147);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (info->size != sizeof(*info)) {
		SCE_DBG_ASSERT_FAIL(5152);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	}
	return PhyMemPartGetInfoForDebugger(part, info);
}

int sceKernelShowPhyMemPart(PhyMemPart *part)
{
#if FW_DEBUG
	int intr, ret;

	PhyMemPartRefer(part);
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&part->lock);
	if (part->flags & PHYMEM_PART_ACTIVE) {
		ret = 0;
		PhyMemPartShowFreeRaw(part);
	} else {
		SCE_DBG_PRINTF(0xb, 5172, "pPMP is not active\n");
		ret = SCE_KERNEL_ERROR_PHYMEMPART_NOT_ACTIVE;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&part->lock, intr);
	PhyMemPartRelease(part);
	return ret;
#else
	return 0;
#endif
}

int sceKernelGetPhyMemPartInfoByID(SceUID uid, SceKernelPhyMemPartInfoForDebugger *info)
{
	PhyMemPart *part;
	int ret;

	if (info == NULL) {
		SCE_DBG_ASSERT_FAIL(5211);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (info->size != sizeof(*info)) {
		SCE_DBG_ASSERT_FAIL(5215);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	}
	ret = ksceGUIDGetObject(uid, (SceObjectBase **)&part);
	if (ret >= 0)
		ret = PhyMemPartGetInfoForDebugger(part, info);
	return ret;
}

int ksceKernelCheckDipsw(unsigned int bit);
int SceSysmemForKernel_BF275C5A(SceUID guid, SceClass *cls, const char *name,
				SceObjectBase *object);
int PhyMemLowStaticInit(void);

/* the fixed GUIDs of the kernel and tool partitions */

static SceClass s_phyMemPartClassBody;

/* the boot CDRAM partition, a static one over the static allocator's range */
static PhyMemSegment s_phyMemSegmentCdramBoot;
static PhyMemPart s_phyMemPartCdramBoot;
PhyMemPart *s_phyPartCdramBoot;

/* name unknown: the partitions the boot loader made, and the kernel's free size recounted */
int PhyMemModuleStart(const UIDManager *manager)
{
	PhyMemPart *part = manager->phyPartKernel;
	PhyPage *page;
	SceUInt32 pindex;
	SceSize size = 0;
	int i;

	s_phyPageTableMgr = manager->phyPageTableMgr;
	s_phyPartTool = manager->phyPartTool;
	s_phyPartKernel = part;
	PhyMemPartRefer(part);
	if (PHYMEM_PART_TYPE(part) == PHYMEM_PART_TYPE_LPDDR2)
		for (page = part->freeList->next; page != part->freeList; page = page->next)
			size += page->size;
	for (i = 0; i < PHYMEM_BUDDY_ORDERS; i++) {
		if (part->buddy[i].state != PHYMEM_BUDDY_HAS)
			continue;
		pindex = part->buddy[i].pindex;
		for (;;) {
			page = PhyMemPartGetPhyPageByIndexRaw(part, pindex);
			size += PhyPageGetSize(page);
			if ((page->info & PHYPAGE_LINK_MASK) != PHYPAGE_LINK_MORE)
				break;
			pindex = PHYPAGE_PINDEX(page->info);
		}
	}
	PhyMemPartRelease(part);
	s_phyPartKernel->freeSize = size;
	return 0;
}

int PhyMemModuleStartPhase2(const UIDManager *manager)
{
	int ret;

	s_phyMemPartClass = &s_phyMemPartClassBody;
	ret = ksceUIDClassInitClass(&s_phyMemPartClassBody, "SceUIDPhyMemPartClass",
				    ksceKernelGetUIDClass(), sizeof(PhyMemPart), NULL, NULL);
	if (ret < 0)
		SCE_PANIC(5406);
	ret = SceSysmemForKernel_BF275C5A(SCE_KERNEL_PHYMEMPART_ID, s_phyMemPartClass, NULL,
					  &s_phyPartKernel->base);
	if (ret < 0)
		return ret;
	if (ksceKernelCheckDipsw(0xd2)) {
		ret = SceSysmemForKernel_BF275C5A(SCE_KERNEL_PHYMEMPART_TOOL_ID, s_phyMemPartClass, NULL,
						  &s_phyPartTool->base);
		if (ret < 0)
			return ret;
	}
	s_phyPageVenezia = manager->phyPageVenezia;
	s_phyPageVip = manager->phyPageVip;
	return 0;
}

/* run after processmgr is up; Sony name from the 0.990 "PhyMemPartInitCdram failed" message (it wraps PhyMemPartInitCdramRaw) */
int PhyMemPartInitCdram(void)
{
	s_phyPartCdramBoot = &s_phyMemPartCdramBoot;
	s_phyMemPartCdramBoot.segments = &s_phyMemSegmentCdramBoot;
	s_phyMemSegmentCdramBoot.phyMemLow = NULL;
	PhyMemPartInitCdramRaw(&s_phyMemPartCdramBoot, 0x20000000, 0x8000000);
	return 0;
}

/* name unknown: CDRAM from the partition sceKernelSetPhyPartCdram set */
int PhyMemCdramAlloc(SceSize size, SceSize alignment, uintptr_t *pa)
{
	return PhyMemPartCdramAllocCore(s_phyPartCdram, size, alignment, pa);
}

/* name unknown */
int PhyMemCdramFree(uintptr_t pa, SceSize size)
{
	return PhyMemPartCdramFreeCore(s_phyPartCdram, pa, size);
}

/* name unknown: the static CDRAM allocator reset */
int SceSysmemForKernel_02DAD732(void)
{
	return PhyMemLowStaticInit();
}
