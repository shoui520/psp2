/*
 * SceSysmem: the structures and internal functions its source files share.
 *
 * Sony's file names (from the debug records) are kept: oheap.c, uid.c, ...
 * Internal functions without a debug-record name have descriptive names and
 * are marked "name unknown" where they are defined.
 */

#pragma once

#include <psp2kern/types.h>
#include "fw.h"
#include "mmu.h"
#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/sysmem/memtype.h>
#include "sce/cpu.h"
#include "sce/sysmem.h"
#include "sce/debug.h"
#include "sce/error.h"
#include "sce/threadcb.h"

/*
 * l1pagetable.c: an address space's two L1 tables (below and above the
 * TTBR0/TTBR1 split) under one lock
 */
typedef struct L1PageTable {
	SceKernelRWSpinlock lock;
	uint32_t *low;
	uint32_t *high;
	uintptr_t split;
	SceUInt32 reserved;			/* 0x10: copied from the kernel's, never read */
} L1PageTable;

/* an address space's translation context and its L1 tables */
typedef struct AddressSpaceContext {
	MMUContext mmu;				/* contextId's low byte is the ASID */
	L1PageTable *l1pt;			/* 0x0c */
	SceUInt32 *l2Tables;			/* 0x10: per MiB, an L2PageTable | 1 */
	struct MemBlockObject *l2TablesBlock;	/* 0x14: holds l2Tables */
	struct FixedHeap *memBlockHeap;		/* 0x18 */
	struct FixedHeap *virPageHeap;		/* 0x1c */
	struct FixedHeap *l2ptHeap;		/* 0x20 */
	struct Partition *partition;		/* 0x24: the partition that owns the context */
} AddressSpaceContext;

/* PartitionInit's parameters (sceKernelCreatePartition) */
typedef struct PartitionInitParam {
	SceSize size;				/* sizeof(PartitionInitParam) */
	SceUInt32 attr;				/* bits 1..3 only */
	AddressSpaceContext *context;
	SceUInt32 entryAttr;			/* 0x0c: AddressSpacePartitionEntry.attr */
	SceUInt32 entryAttrMask;		/* 0x10: AddressSpacePartitionEntry.attrMask */
	SceUID pid;				/* 0x14: the owning process */
	SceUID uid;				/* -1: a new GUID */
} PartitionInitParam;

/* a memblock type (memblock_db.c) */
/* the internal codes of the memblock types the code tests for (memblock_db.c) */
#define MEMBLOCK_CODE_KERNEL_CDRAM_L1WBWA_RW	0x40c04006
#define MEMBLOCK_CODE_KERNEL_CDRAM_RW		0x40c08006
#define MEMBLOCK_CODE_KERNEL_ROOT_R		0x1060d004
#define MEMBLOCK_CODE_KERNEL_ROOT_RW		0x1060d006
#define MEMBLOCK_CODE_KERNEL_ROOT_RX		0x1060d005
#define MEMBLOCK_CODE_KERNEL_TOOL_RW		0x14f0d006
#define MEMBLOCK_CODE_KERNEL_TOOL_RX		0x14f0d005
#define MEMBLOCK_CODE_USER_CDIALOG_NC_R		0x0fa08040
#define MEMBLOCK_CODE_USER_CDIALOG_NC_RW	0x0fa08060
#define MEMBLOCK_CODE_USER_CDIALOG_R		0x0ea0d040
#define MEMBLOCK_CODE_USER_CDIALOG_RW		0x0ea0d060
#define MEMBLOCK_CODE_USER_CDRAM_L1WBWA_RW	0x09c04060
#define MEMBLOCK_CODE_USER_CDRAM_R		0x09c08040
#define MEMBLOCK_CODE_USER_CDRAM_RW		0x09c08060
#define MEMBLOCK_CODE_USER_IO_DEVICE_R		0x0b100840
#define MEMBLOCK_CODE_USER_IO_DEVICE_RW		0x0b100860
#define MEMBLOCK_CODE_USER_MAIN_NC_RW		0x0db08060
#define MEMBLOCK_CODE_USER_MAIN_R		0x0cb0d040
#define MEMBLOCK_CODE_USER_MAIN_RW		0x0cb0d060
#define MEMBLOCK_CODE_USER_SHARED_TOOL_RW	0x0af0d060

typedef struct MemBlockTypeInfo {
	SceUInt32 type;
	SceUInt32 code;
	SceUInt32 attr;			/* 0x20: tool only, 0x100: physically contiguous */
	SceSize maxAlignment;		/* 0x0c */
} MemBlockTypeInfo;

/*
 * The parameters of a memblock allocation (partition.c fills them; virpage.c
 * has the initialisers). PLACEHOLDER: only the fields used so far are named.
 */
struct MemBlockObject;
/* a memblock code's entry in the type database (memblock_db.c) */
typedef struct MemBlockCodeInfo {
	SceUInt32 code;
	const MemBlockTypeInfo *typeInfo;
} MemBlockCodeInfo;

/* what a mirror opened by name knows of its base block (opt state 4) */
typedef struct MemBlockBaseInfo {
	SceUID pid;				/* the base's partition's process */
	SceUID partitionUid;
	SceUID guid;				/* 0x08: the base block's */
	struct MemBlockObject *block;		/* 0x0c */
} MemBlockBaseInfo;

/* a physical address vector (opt.paddr_list, the VA range queries) */
typedef struct MemBlockPAVector {
	SceSize size;
	SceUInt32 capacity;			/* 0x04: of ranges */
	SceUInt32 count;			/* 0x08: ranges filled in */
	SceUInt32 total;			/* 0x0c: ranges there are */
	SceKernelPARange *ranges;		/* 0x10 */
} MemBlockPAVector;

/* where an allocation landed (the start of SceKernelMemBlockInfo) */
typedef struct MemBlockAllocResult {
	SceSize size;			/* sizeof(MemBlockAllocResult) */
	uintptr_t mappedBase;		/* 0x04: past the free head */
	SceSize mappedSize;		/* 0x08 */
	SceUInt32 memoryType;		/* 0x0c: code bits 12, 14 and 15 */
	SceUInt32 access;		/* 0x10: code bits 5 and 6 (W, R) */
} MemBlockAllocResult;

typedef struct SceKernelAllocMemBlockCommand {
	SceSize cmdSize;		/* sizeof(SceKernelAllocMemBlockCommand) */
	SceUInt32 code;			/* 0x04: the memblock code */
	SceUInt32 type;			/* 0x08: the memblock type */
	const char *name;		/* 0x0c: the new block's */
	SceSize size;			/* 0x10 */
	SceSize offset;			/* 0x14: the lowest start, from the partition's base */
	SceSize padLow;			/* 0x18: unbacked pages (type 2) below the backed ones */
	SceSize padHigh;		/* 0x1c: and above them */
	SceKernelAllocMemBlockKernelOpt opt;	/* 0x20 */
	struct MemBlockObject *block;	/* 0x78: the new block */
	struct MemBlockAllocResult *result;	/* 0x7c: filled once mapped, if set */
	const struct MemBlockTypeInfo *typeInfo;	/* 0x80 */
	struct AddressSpaceObject *addressSpace;	/* 0x84 */
	void *phyMemPart;		/* 0x88 */
	struct Partition *partition;	/* 0x8c */
	SceUInt32 cmdFlags;		/* 0x90 */
} SceKernelAllocMemBlockCommand;

_Static_assert(sizeof(SceKernelAllocMemBlockCommand) == 0x94, "SceKernelAllocMemBlockCommand size");

/* a piece of a memblock's address range (virpage.c) */
struct PhyPage;
typedef struct VirPage {
	struct VirPage *next;
	SceUInt32 reserved;
	SceUInt32 flags;		/* 0x08: type in bits 0..5 */
	uintptr_t vbase;		/* 0x0c */
	SceSize size;			/* 0x10 */
	struct PhyPage *page;		/* 0x14 */
	uintptr_t pbase;		/* 0x18 */
	SceUInt32 magic;		/* 0x1c */
} VirPage;

_Static_assert(sizeof(VirPage) == 0x20, "VirPage size");

#define VIRPAGE_TYPE(_vp)		((_vp)->flags & 0x3f)
#define VIRPAGE_TYPE_1			1	/* PLACEHOLDER: types 1, 8 and 32 are not decoded */
#define VIRPAGE_TYPE_PHY		2	/* to be backed by physical pages */
#define VIRPAGE_TYPE_DIVIDED		4	/* backed (a piece when divided) */
#define VIRPAGE_TYPE_8			8
#define VIRPAGE_TYPE_16			16	/* pbase/size preset */
#define VIRPAGE_TYPE_32			32
#define VIRPAGE_HAS_PBASE		0x10000	/* pbase is given */
#define VIRPAGE_ROUND_UP		0x20000	/* a power of two (1 MiB multiples from 1 MiB) */


/*
 * A tiny partition: an address range and the memblocks placed in it
 * (partition.c).
 */
struct MemBlockObject;
typedef struct TinyPartition {
	SceObjectBase base;
	const char *name;			/* 0x08 */
	SceKernelSpinlock lock;			/* 0x0c */
	struct MemBlockObject *blocks;		/* 0x10: the list sentinel */
	uintptr_t vbase;			/* 0x14 */
	SceSize size;				/* 0x18 */
	AddressSpaceContext *context;		/* 0x1c */
	volatile SceUInt32 blockCount;		/* 0x20: the low half in use, the high half the peak (atomics; Sony reads the word and masks) */
	SceSize freeSize;			/* 0x24 */
	SceUInt32 attr;				/* 0x28: 0x10000 = its domain for the maps of others */
	SceUInt32 reserved0;			/* 0x2c: 0, never read */
	SceUInt32 reserved1;			/* 0x30: -1, never read */
	SceUInt32 magic;			/* 0x34 */
} TinyPartition;

#define TINY_PARTITION_MAGIC		0xd946f262
#define TINY_PARTITION_DEAD		0x07f317d8

_Static_assert(sizeof(TinyPartition) == 0x38, "TinyPartition size");

/* the L2 page table of one MiB of an address space (l2pagetable.c) */
struct MemBlockObject;
struct Partition;
typedef struct L2PageTable {
	struct L2PageTable *next;		/* 0x00: in its partition's cache */
	SceUInt32 reserved0;
	SceKernelRWSpinlock lock;		/* 0x08 */
	SceUInt16 flags;			/* 0x0c: bits 0..3 its quarter of the block; 0x8000 kept */
	SceUInt16 reserved1;
	SceInt16 mappedPages;			/* 0x10 */
	SceInt16 reservedPages;			/* 0x12 */
	uintptr_t vbase;			/* 0x14: the MiB it covers */
	uint32_t *table;			/* 0x18 */
	SceUInt32 reserved2;			/* 0x1c: 0, never read */
	uintptr_t tablePa;			/* 0x20: the table's physical address */
	struct MemBlockObject *block;		/* 0x24: the simple memblock holding the table */
	struct Partition *partition;		/* 0x28 */
	struct MemBlockObject *firstBlock;	/* 0x2c: the first memblock in the MiB */
	SceUInt32 reserved3[3];
	SceUInt32 magic;			/* 0x3c: L2PAGETABLE_MAGIC while allocated */
} L2PageTable;

#define L2PAGETABLE_MAGIC		0xdfb73368

_Static_assert(sizeof(L2PageTable) == 0x40, "L2PageTable size");

/* a partition: a tiny partition with its own L2 page tables (partition.c) */
typedef struct Partition {
	TinyPartition tiny;
	SceInt32 refCount;			/* 0x38 */
	SceKernelRWSpinlock l2Lock;		/* 0x3c: the address space's L2 table slots */
	SceKernelRWSpinlock l2BlockLock;	/* 0x40: guards l2Blocks */
	struct MemBlockObject *l2Blocks;	/* 0x44: blocks with free L2 table quarters */
	SceKernelRWSpinlock mapLock;		/* 0x48 */
	SceUInt32 attr;				/* 0x4c */
	SceUInt8 domain;			/* 0x50 */
	SceUInt8 padding[3];
	struct AddressSpaceObject *addressSpace;	/* 0x54 */
	SceUInt32 reserved;			/* 0x58: zeroed by PartitionInit, never read */
	SceUInt32 entryAttr;			/* 0x5c: the creation table entry's attr ... */
	SceUInt32 entryAttrMask;		/* 0x60: ... and mask; stored, never read */
	SceUID pid;				/* 0x64: the owning process */
	SceUID uid;				/* 0x68 */
	SceKernelRWSpinlock l2CacheLock;	/* 0x6c */
	struct L2PageTable *l2Cache;		/* 0x70: free L2 page tables kept for reuse */
	SceUInt32 l2CacheCount;			/* 0x74 */
	SceUInt32 l2Count;			/* 0x78: the low half in use, the high half the peak */
	SceUInt32 virPageCount;			/* 0x7c: the low half in use, the high half the peak */
} Partition;

_Static_assert(sizeof(Partition) == 0x80, "Partition size");

/* the name the converted files first used */
typedef Partition PartitionObject;

/* debug_dump.c (sce/debug.h leaves it to the modules) */
void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool cond, const SceKernelDebugInfo *info,
				 const void *lr, const char *fmt, ...);

struct UIDManager;

/* an allocator lent by the owner of an object heap (a process's heap) */
typedef struct SceKernelHeapHook {
	void *arg;
	void *(*alloc)(void *arg, SceSize size);
	void (*free)(void *arg, void *ptr);
} SceKernelHeapHook;

/* uid_class.c: a heap of UID objects; its class says how to allocate from it */
typedef struct UIDHeapClass {
	SceClass cls;
	int (*alloc)(void *heap, SceSize size, void **object);	/* 0x2c */
	int (*free)(void *heap, void *object);			/* 0x30 */
} UIDHeapClass;

typedef struct UIDHeap {
	void *object;				/* as SceObjectBase */
	struct FixedHeapClass *cls;		/* 0x04 */
} UIDHeap;

/* fixed_heap.c: a heap of equal-size objects carved from 4K or 64K pages */
#define FIXED_HEAP_MAGIC		0xfd9b28a6
#define FIXED_HEAP_CPUS			4
#define FIXED_HEAP_CACHE_MAX		12

#define FIXED_HEAP_ATTR_CPU_CACHE	0x00000001	/* a free-object cache per CPU */
#define FIXED_HEAP_ATTR_FROM_HEAP	0x00010000	/* a process's: its memory from the kernel heap */
#define FIXED_HEAP_ATTR_FROM_HOOK	0x00040000	/* a process's: its memory from the hook */
#define FIXED_HEAP_ATTR_TABLES_HEAP	0x00100000	/* page tables from the kernel heap */
#define FIXED_HEAP_ATTR_TABLES_KERNEL	0x00200000	/* page tables from a kernel fixed heap */
#define FIXED_HEAP_ATTR_TABLES_HOOK	0x00400000	/* page tables from the hook */
#define FIXED_HEAP_ATTR_NAME_HEAP	0x01000000	/* name from the kernel heap */
#define FIXED_HEAP_ATTR_NAME_HOOK	0x04000000	/* name from the hook */
#define FIXED_HEAP_ATTR_UID_ENTRIES	0x10000000	/* a heap of UID entries */
#define FIXED_HEAP_ATTR_PROCESS		0x40000000	/* made by FixedHeapInitForProcess */

struct FixedHeap;

typedef struct FixedHeapClass {
	UIDHeapClass heapClass;
	/* a new object was carved from the current page */
	void (*onCarve)(void *object, SceUInt32 index);		/* 0x34 */
	/* a new page is the current page */
	int (*setPage)(struct FixedHeap *heap, void *page);	/* 0x38 */
} FixedHeapClass;

/*
 * sceKernelFixedHeapSetHook: told about every object handed out and taken
 * back, and the return address of the fixed heap function that did it
 */
typedef struct FixedHeapObjectHook {
	void *arg;
	void (*onAlloc)(void *arg, struct FixedHeap *heap, void *object, const void *caller);
	void (*onFree)(void *arg, struct FixedHeap *heap, void *object, const void *caller);
} FixedHeapObjectHook;

/* freed objects a CPU keeps for itself, linked through each object's first word */
typedef struct FixedHeapCpuCache {
	SceUInt16 max;
	SceUInt16 count;			/* "nCount" */
	SceUInt32 reserved;			/* only copied into SceKernelFixedHeapInfo */
	void *head;
	void *tail;
} FixedHeapCpuCache;

struct ObjectHeap;
typedef struct FixedHeap {
	UIDHeap uidHeap;
	struct ObjectHeap *owner;		/* 0x08: the object heap it serves */
	void *freeHead;				/* 0x0c: freed objects, oldest first */
	void *freeTail;
	struct Partition *optPartition;		/* 0x14: allocations for it keep their opt */
	SceUInt32 attr;				/* 0x18: FIXED_HEAP_ATTR_* */
	SceSize pageSize;			/* 0x1c: 0x1000 or 0x10000 */
	SceUInt16 itemSize;			/* 0x20 */
	SceUInt16 totalCount;			/* 0x22: objects carved so far */
	SceInt16 freeCount;			/* 0x24: carved and free (atomic) */
	SceUInt16 peakCount;			/* 0x26: most objects in use */
	SceKernelRWSpinlock lock;		/* 0x28 */
	SceUInt32 magic;			/* 0x2c: FIXED_HEAP_MAGIC */
	SceUInt32 maxPages;			/* 0x30: room in the page tables */
	SceUInt32 pageCount;			/* 0x34 */
	void **pages;				/* 0x38: page addresses */
	void *currentPage;			/* 0x3c: the page being carved */
	void *cursor;				/* 0x40: its next object */
	SceUInt16 usedInPage;			/* 0x44: objects carved from it */
	SceUInt16 perPage;			/* 0x46 */
	SceKernelHeapHook *hook;		/* 0x48: who allocated it, NULL: a kernel heap */
	void *heap;				/* 0x4c: the kernel heap it grows from */
	void **blocks;				/* 0x50: each page's simple memblock */
	void *partition;			/* 0x54 */
	char *name;				/* 0x58 */
	FixedHeapCpuCache caches[FIXED_HEAP_CPUS];	/* 0x5c */
	FixedHeapObjectHook *objectHook;	/* 0x9c */
	SceUInt32 fifoCount;			/* 0xa0: "nItemInFifo", objects on the free list */
} FixedHeap;

_Static_assert(sizeof(FixedHeap) == 0xa4, "FixedHeap size");

/* sceKernelGetFixedHeapInfo */
typedef struct SceKernelFixedHeapInfo {
	SceSize size;				/* 0x90 */
	SceUInt32 itemSize;
	SceSize totalSize;			/* of all pages */
	SceUInt32 perPage;
	SceUInt32 freeCount;
	SceUInt32 peakCount;
	void *cursor;				/* 0x18 */
	SceUInt16 usedInPage;
	SceUInt16 perPage16;
	SceUInt32 listCount;			/* 0x20: "nListCount", counted on the free list */
	FixedHeapCpuCache caches[FIXED_HEAP_CPUS];	/* 0x24 */
	void *freeHead;				/* 0x64 */
	void *freeTail;
	SceUInt32 attr;
	SceSize pageSize;			/* 0x70 */
	SceUInt16 totalCount;
	SceUInt16 freeCount16;
	SceUInt32 maxPages;
	SceUInt32 pageCount;
	void **pages;				/* 0x80 */
	void *currentPage;
	char *name;
	void **blocks;
} SceKernelFixedHeapInfo;

_Static_assert(sizeof(SceKernelFixedHeapInfo) == 0x90, "SceKernelFixedHeapInfo size");

/* oheap.c: object heaps, a fixed heap per object size plus per-type limits */
#define OBJECT_HEAP_TYPES	6

typedef struct ObjectHeapSizeClass {
	SceSize size;				/* objects up to this size; -1: the rest */
	void *heap;				/* NULL: fall through to a larger class */
	SceUID uid;				/* the fixed heap's GUID, -1 if none */
} ObjectHeapSizeClass;

typedef struct ObjectHeapLimit {
	SceUInt32 max;				/* objects allowed at once */
	SceUInt32 reserve;			/* objects kept preallocated */
	SceUInt32 count;			/* in use */
	SceUInt32 peak;
	SceUInt32 freeCount;			/* preallocated, unused */
	void *freeHead;				/* linked through each object's first word */
	void *freeTail;
} ObjectHeapLimit;

typedef struct ObjectHeap {
	SceKernelSpinlock lock;
	void *partition;			/* the physical partition objects come from */
	SceUInt32 sizeClassCount;
	ObjectHeapSizeClass *sizeClasses;
	SceKernelHeapHook *hook;		/* NULL: allocated from the kernel heap */
	void *heap;				/* 0x14: the heap of the shared size classes */
	void *defaultHeap;			/* 0x18: the 0x40-byte class */
	ObjectHeapLimit limits[OBJECT_HEAP_TYPES];
} ObjectHeap;

_Static_assert(sizeof(ObjectHeap) == 0xc4, "ObjectHeap size");

/* the size classes of every object heap: shared ones live in the process heap */
typedef struct ObjectHeapSizeClassDef {
	SceSize size;
	SceUInt32 ownHeap;			/* 1: a fixed heap of the object heap's own */
} ObjectHeapSizeClassDef;

/* oheap.c */
void *ObjectHeapGetHeap(ObjectHeap *heap, SceSize size);
int ObjectHeapAllocLimit(ObjectHeap *heap, unsigned int type, void **object);
int ObjectHeapFreeLimit(ObjectHeap *heap, unsigned int type, void *object);
void *ObjectHeapGetDefaultHeap(ObjectHeap *heap);
void ObjectHeapSetKernelHeap(const struct UIDManager *manager);
int ObjectHeapInitShared(void);

/* fixed_heap.c */
int FixedHeapInitForProcess(FixedHeap *heap, SceUID pid, const char *name, SceUInt32 attr,
			    SceSize itemSize, void *partition, SceSize pageSize,
			    SceUInt16 maxPages);
FixedHeapClass *FixedHeapGetClass(void);
int FixedHeapInitClass(FixedHeapClass *cls, const char *name);
int FixedHeapRelease(FixedHeap *heap);
int FixedHeapFinish(FixedHeap *heap);
void FixedHeapInitLock(FixedHeap *heap);
int FixedHeapWriteLock(FixedHeap *heap, const char *func, int line);
void FixedHeapWriteUnlock(FixedHeap *heap, int state);
int FixedHeapReadLock(FixedHeap *heap, const char *func, int line);
void FixedHeapReadUnlock(FixedHeap *heap, int state);
int FixedHeapFreeObject(FixedHeap *heap, void *object);
int FixedHeapFreeObjectRaw(FixedHeap *heap, void *object);
int KernelFixedHeapFreeObject(SceSize size, void *object);
int FixedHeapClassAlloc(void *uidHeap, SceSize size, void **object);
int KernelFixedHeapAllocObject(SceSize size, void **object);
int FixedHeapAllocObject(FixedHeap *heap, void **object);
int FixedHeapAllocObjectExt(FixedHeap *heap, void **object);
int FixedHeapAllocObjectWithAttr(FixedHeap *heap, struct Partition *partition, SceUInt32 opt,
				 void **object);
int FixedHeapAllocObjectRaw(FixedHeap *heap, int raw, void **object);
int FixedHeapGetInfo(FixedHeap *heap, SceKernelFixedHeapInfo *info);
#if FW_DEBUG
int FixedHeapGetIndexRaw(FixedHeap *heap, const void *object);
#endif
int FixedHeapGetObjectByIndexRaw(FixedHeap *heap, SceUInt32 index, void **object);
int FixedHeapWalk(FixedHeap *heap, int (*func)(void *object, SceUInt32 index, void *arg),
		  void *arg);
int FixedHeapApplyRangeWithIndexArgRaw(FixedHeap *heap, SceUInt32 start, SceUInt32 count,
				       int (*func)(void *object, SceUInt32 index, void *arg),
				       void *arg, SceUInt32 *next);
int FixedHeapSetPage(FixedHeap *heap, void *page);
int FixedHeapSetUID(struct UIDManager *manager);
FixedHeap *SceSysmemForKernel_3717C322(SceSize size);		/* sceKernelGetKernelFixedHeap */
void *SceSysmemForKernel_ABA5313C(FixedHeap *heap);		/* sceKernelFixedHeapAllocObject */
int SceSysmemForKernel_93344755(FixedHeap *heap, void *object);	/* sceKernelFixedHeapFreeObject */
int SceSysmemForKernel_811C452B(FixedHeap *heap, FixedHeapObjectHook *hook);
int sceKernelGetFixedHeapInfo(SceUID uid, SceKernelFixedHeapInfo *info);
int sceKernelFixedHeapSetHook(SceUID uid, FixedHeapObjectHook *hook);

/* nameheap2.c: kept names, counted, in a hash table */
#define NAME_HEAP_BUCKETS		0x200
#define NAME_HEAP_LOCKS			16
#define NAME_ENTRY_SHORT_NAME		0x20	/* names of a fixed heap entry */

#define NAME_HEAP_FLAG_SECOND		0x00000002	/* the first table, not the second */
#define NAME_HEAP_FLAG_LONG		0x00010000	/* a name of up to 255 characters */
#define NAME_HEAP_FLAG_BY_LINK		0x01000000	/* name is one the name heap handed out */

#define NAME_HEAP_CONTROL_ATTR_UNIQUE	0x00000001	/* a name may be inserted once */

#define NAME_ENTRY_ATTR_KIND_MASK	0x0000000f	/* what owner is */
#define NAME_ENTRY_KIND_UID_ENTRY	1
#define NAME_ENTRY_ATTR_KERNEL_HEAP	0x00001000	/* from the kernel heap, not the fixed heap */
#define NAME_ENTRY_ATTR_BUCKET_SHIFT	16
#define NAME_ENTRY_ATTR_BUCKET_MASK	0x2fff0000	/* as in Sony's code, with bit 29 */
#define NAME_ENTRY_ATTR_VALID		0x40000000

typedef struct NameEntry {
	struct NameEntry *next;
	SceUInt32 refCount;			/* 0x04 */
	SceUInt32 attr;				/* 0x08: NAME_ENTRY_ATTR_*, bucket */
	void *owner;				/* 0x0c: NAME_ENTRY_KIND_UID_ENTRY: its UID entry */
	char name[NAME_ENTRY_SHORT_NAME];	/* 0x10: longer with NAME_ENTRY_ATTR_KERNEL_HEAP */
} NameEntry;

typedef struct NameHeapControl {
	SceUInt32 attr;				/* NAME_HEAP_CONTROL_ATTR_* */
	SceUInt32 seed;				/* 0x04: of the hash */
	NameEntry **buckets;			/* 0x08 */
	FixedHeap *entryHeap;			/* 0x0c: of 0x30-byte entries */
	SceKernelRWSpinlock locks[NAME_HEAP_LOCKS];	/* 0x10: by hash */
	SceUInt32 count;			/* 0x50: high-water counter, peak << 16 | count */
} NameHeapControl;

_Static_assert(sizeof(NameHeapControl) == 0x54, "NameHeapControl size");

/* sceKernelNameHeapGetInfo */
typedef struct SceKernelNameHeapInfo {
	SceSize size;				/* 0x14 */
	struct {
		SceUInt32 count;
		SceUInt32 peak;
	} heaps[2];
} SceKernelNameHeapInfo;

int NameHeapInsert(const char *name, SceUInt32 flags, NameEntry **out);
int NameHeapLink(const char *name, SceUInt32 flags, NameEntry **out);
int NameHeapDelete(const char *name, SceUInt32 flags);
int NameHeapFind(const char *name, SceUInt32 flags, NameEntry **out);
int NameHeapInit(struct UIDManager *manager);
int NameEntrySetUIDEntry(NameEntry *entry, void *uidEntry);
SceUInt32 SceSysmemForKernel_EB48682E(const char *key, SceSize length, SceUInt32 seed);	/* sceKernelNameHeapGetKey */

/* uid_class.c */
int UIDHeapInit(UIDHeap *heap);
int UIDHeapClassInit(UIDHeapClass *cls, const char *name, SceClassCallback create,
		     SceClassCallback destroy, int (*alloc)(void *heap, SceSize size, void **object),
		     int (*free)(void *heap, void *object));
int UIDHeapObjectCreate(void *object);
int UIDHeapObjectDestroy(void *object);
int UIDHeapCallAlloc(void *heap, SceSize size, void **object);
int UIDHeapCallFree(void *heap, void *object);

/* sysmem.c: the kernel heap under its lock */
void *HeapAllocLocked(void *heap, SceSize size);
int HeapFreeLocked(void *heap, void *ptr);
void *UIDManagerGetKernelHeap(void);

/* smb.c: simple memblocks */
struct MemBlockObject;
int AllocSimpleMemBlock(const char *name, SceUInt32 smbType, void *phyMemPart, SceUInt32 flags,
			struct MemBlockObject **smb);
int SceSysmemForKernel_39D5A3B7(struct MemBlockObject *smb, void **base);	/* sceKernelGetSimpleMemBlockBase */
int ksceKernelFreeSimpleMemBlock(void *smb);	/* vitasdk: void * */

/* 0x81012970 (partition.c): the kernel partition (UIDManager +0x40) */
struct Partition *PartitionGetKernel(void);

/* address_space.c */
int AddressSpaceGetDefaultPhyMemPart(struct AddressSpaceObject *as, int kernel, void **part);

/* uid.c */
int GUIDUnregisterChecked(SceUID guid);
int GUIDSetCNOA(SceUID guid, void *cls, const char *name, void *object, SceUInt32 attr);
int UIDDLinkClassInit(struct UIDManager *manager);
int UIDEntryHeapClassInit(struct UIDManager *manager);
void UIDClassSetCallbacks(SceClass *cls, const char *name, SceClassCallback create,
			  SceClassCallback destroy);

#define ADDRESS_SPACE_TYPE_USER		0x10000001

/* address_space.c: an address space object (SceUIDAddressSpaceClass) */
typedef struct AddressSpaceObject {
	SceObjectBase base;
	SceInt32 refCount;			/* 0x08 */
	SceKernelSpinlock lock;			/* 0x0c: the partition tables */
	SceUInt32 type;				/* 0x10 */
	SceUID pid;				/* 0x14: the owning process */
	AddressSpaceContext *context;		/* 0x18 */
	struct Partition *partition[ADDRESS_SPACE_PARTITIONS];	/* 0x1c */
	SceUID partitionUid[ADDRESS_SPACE_PARTITIONS];	/* 0x9c: -1 none */
	void *phyMemPart[ADDRESS_SPACE_PHYMEMPARTS];	/* 0x11c: by code bits 20..23 */
	SceUID userIOPA;			/* 0x15c: the "SceUserIOPA" block */
	SceUID userIOTimer;			/* 0x160: the "SceUserIOTimer" block */
	uintptr_t userIOTimerBase;		/* 0x164 */
	const AddressSpaceHeapOps *heap;	/* 0x168 */
	SceUInt32 magic;			/* 0x16c */
} AddressSpaceObject;

/* a subbudget's size and free size (sceKernelGetSubbudgetInfo) */
typedef struct AddressSpaceSubbudgetInfo {
	SceSize size;
	SceSize psize;
	SceSize freeSize;
} AddressSpaceSubbudgetInfo;

/* the free memory of the user, CDRAM and contiguous partitions (sceKernelGetFreeMemorySize) */
typedef struct AddressSpaceFreeMemorySize {
	SceSize size;
	SceSize user;
	SceSize cdram;
	SceSize phycont;
} AddressSpaceFreeMemorySize;

_Static_assert(sizeof(AddressSpaceObject) == 0x170, "AddressSpaceObject size");

int AddressSpaceObjectCreate(void *object);
int AddressSpaceObjectDestroy(void *object);
int UIDHeapClassInitManager(struct UIDManager *manager);
int UIDClassInit(struct UIDManager *manager);

/* sceKernelGetClassInfoAll */
typedef struct SceKernelClassInfo {
	SceSize size;				/* 0x10 */
	SceClass *cls;
	SceClass *parent;
	const char *name;
} SceKernelClassInfo;

/* sceUIDEntryHeapGetInfo */
typedef struct SceKernelUIDEntryHeapInfo {
	SceSize size;				/* 0x18 */
	SceUInt32 itemSize;
	SceSize totalSize;
	SceUInt32 perPage;
	SceUInt32 freeCount;
	SceUInt32 peakCount;
} SceKernelUIDEntryHeapInfo;

/* scePUIDGetInfo */
typedef struct SceKernelPUIDInfo {
	SceSize size;				/* 0x14 */
	SceUID uid;
	SceUID pid;
	SceUInt32 attr;
	SceUInt8 classAttr;			/* 0x10 */
	SceUInt8 linkCount;
	SceUInt16 useCount;
} SceKernelPUIDInfo;

/* memblock.c */
int FreeMemBlockByGUID(SceUID guid);

/* sysroot.c (sysroot_time.c) */
SceUID SceSysrootForKernel_7385CADE(void);	/* the current process */
SceUID SceSysrootForKernel_D441DC34(void);	/* the current thread */
uint32_t SceSysrootForKernel_085C2BCB(void);	/* SceUIDSysrootClass */
int ksceKernelSysrootGetPUIDEntryHeap(SceUID processId, void **entryHeap);
int SysrootPIDtoAddressSpaceCB(SceUID processId, void **addressSpaceCB);
void *SysrootGetProcessObjectHeap(SceUID pid, int kind);

/* the atomics of atomics_small.c */
SceInt16 ksceKernelAtomicAddAndGet16(SceInt16 *address, SceInt16 value);
SceInt16 ksceKernelAtomicDecIfPositive16(SceInt16 *address);
int snprintf(char *dst, SceSize len, const char *fmt, ...);

/* exports used inside the module */
SceUID SceSysmemForKernel_6DB44A42(SceClass *cls, const char *name, void *object);	/* sceGUIDRegister */
int SceSysmemForKernel_7F7D5C4A(SceClass *cls, SceClass *base);	/* wiki: sceUIDClassIsSubclassForKernel (guessed) */
void *SceSysmemForKernel_0164D817(void);				/* sceKernelGetPhyPartKernel */
void *SceSysmemForKernel_FA6C098C(SceUID uid);				/* sceUIDtoObject */
int ksceGUIDGetObject(SceUID guid, void *object);
void *ksceKernelAlloc(SceSize size);
int ksceKernelFree(void *ptr);

/* sysclib fill.c: the fill core memset wraps */
void *SetMemoryCore(void *destination, int value, SceSize size);
/* memory_set32.c: the same fill with a 32-bit pattern */
void *SetMemory32Core(void *destination, SceUInt32 pattern, SceSize size);


/*
 * uid.c: a UID entry, the kernel's record of an object it hands out a UID for.
 * A GUID entry owns the object; a link (an opened GUID) and a PUID (a
 * process's handle) point at a GUID entry.
 */
#define UID_ENTRY_ATTR_LINK_LIMIT	0x000000ff	/* 0: no limit */
#define UID_ENTRY_ATTR_TYPE_SHIFT	8		/* object type of the limits, 4 bits */
#define UID_ENTRY_ATTR_TYPE_MASK	0x00000f00
#define UID_ENTRY_ATTR_PERMANENT	0x00001000	/* never freed by closing */
#define UID_ENTRY_ATTR_SINGLE_LINK	0x00002000	/* freed with one link left */
#define UID_ENTRY_ATTR_NAME_PUBLIC	0x00004000	/* found by name */
#define UID_ENTRY_ATTR_NAME_KIND	0x00008000	/* which name heap the name is in */
#define UID_ENTRY_ATTR_LEVEL_SHIFT	16		/* visibility level, 3 bits */
#define UID_ENTRY_ATTR_LEVEL_MASK	0x00070000
#define UID_ENTRY_ATTR_LEVEL_DEFAULT	0x00030000	/* level 3, when the caller gives none */
#define UID_ENTRY_ATTR_GUID		0x00100000	/* owns the object */
#define UID_ENTRY_ATTR_LINK		0x00200000	/* points at a GUID entry */
#define UID_ENTRY_ATTR_ACTIVE		0x00300000
#define UID_ENTRY_ATTR_GUID_CLOSED	0x00400000	/* closed, still referred to */
#define UID_ENTRY_ATTR_LINK_CLOSED	0x00800000
#define UID_ENTRY_ATTR_GUID_ANY		0x00500000
#define UID_ENTRY_ATTR_LINK_ANY		0x00a00000
#define UID_ENTRY_ATTR_CLOSED		0x00c00000
#define UID_ENTRY_ATTR_ALL		0x00f00000
#define UID_ENTRY_ATTR_DESTRUCT		0x08000000	/* destruct the object even without a heap */
#define UID_ENTRY_ATTR_WEAK		0x10000000	/* PUID: closing it does not close the GUID */
#define UID_ENTRY_ATTR_PUID		0x40000000	/* a process's entry */
#define UID_ENTRY_MAX_LCOUNT		0xf0
#define UID_ENTRY_MAX_UCOUNT		0x400

/* a UID: bit 0 set, bits 1..15 the entry index, bits 16..29 a generation, bit 30 PUID */
#define UID_GENERATION_MASK		0x3fff0000
#define UID_GENERATION_ONE		0x00010000
#define UID_PUID			0x40000000

static inline int UIDIsGUID(SceUID uid)
{
	return uid >= 0 && (SceInt32)((SceUInt32)uid << 31) < 0 && !(uid & UID_PUID);
}

static inline int UIDIsPUID(SceUID uid)
{
	return uid >= 0 && (SceInt32)((SceUInt32)uid << 31) < 0 &&
	       (SceInt32)((SceUInt32)uid << 1) < 0;
}

static inline unsigned int UIDGetIndex(SceUID uid)
{
	return (uid & 0xffff) >> 1;
}

typedef struct UIDEntry {
	SceObjectBase *object;			/* 0x00 */
	SceUID pid;				/* 0x04: the owner */
	SceUInt32 attr;				/* 0x08: UID_ENTRY_ATTR_* */
	SceUInt8 classAttr;			/* 0x0c: the class's attributes */
	SceUInt8 linkCount;			/* 0x0d: "lcount" */
	SceUInt16 useCount;			/* 0x0e: "ucount" */
	SceUID uid;				/* 0x10 */
	SceUInt32 reserved;			/* 0x14: zeroed, never read */
	union {					/* 0x18 */
		void *heap;			/* GUID: the heap of the object */
		struct UIDEntry *base;		/* link: the GUID entry */
		SceUID guid;			/* PUID: the GUID */
	};
	const char *name;			/* 0x1c */
} UIDEntry;

static inline unsigned int UIDEntryGetLevel(const UIDEntry *entry)
{
	return (entry->attr & UID_ENTRY_ATTR_LEVEL_MASK) >> UID_ENTRY_ATTR_LEVEL_SHIFT;
}

/* a heap of UID entries: the GUID one, and one per process for its PUIDs */
typedef struct UIDEntryHeap {
	FixedHeap fixedHeap;
	SceUID reserved;			/* 0xa4: -1, never read */
	SceUID pid;				/* 0xa8 */
	void *heap;				/* 0xac: the kernel heap it is in */
} UIDEntryHeap;

_Static_assert(sizeof(UIDEntryHeap) == 0xb0, "UIDEntryHeap size");

/* an entry's info, in the entry info walks */
typedef struct SceKernelUIDEntryInfo {
	SceSize size;				/* 0x24 */
	SceUID uid;
	SceObjectBase *object;
	SceClass *cls;
	SceUID guid;				/* 0x10 */
	SceUInt16 linkCount;
	SceUInt16 useCount;
	SceUInt32 attr;
	const char *name;
	SceUID pid;				/* 0x20 */
} SceKernelUIDEntryInfo;

/* objects a close leaves to free, after the lock is dropped */
#define DELETE_PARAM_OBJECTS		2

typedef struct DeleteParam {
	UIDEntry *next;				/* a base entry still to close */
	int nObject;
	struct {
		SceObjectBase *object;
		void *heap;
		SceUInt32 type;
	} objects[DELETE_PARAM_OBJECTS];
} DeleteParam;

/* walks collecting UIDs: match(entry) -> UID_WALK_*, then add() on a match */
typedef struct UIDFuncVector {
	UIDEntryHeap *heap;
	SceUInt32 (*match)(UIDEntry *entry, struct UIDFuncVector *vector);
	void (*add)(struct UIDFuncVector *vector, UIDEntry *entry);
	SceUInt32 level;			/* highest visibility level */
	SceSize size;				/* 0x10: of buffer, in bytes */
	void *buffer;
	SceSize count;				/* 0x18: written */
	SceUInt32 total;			/* matched */
	SceUInt32 key;				/* 0x20 */
} UIDFuncVector;

/* the UID manager */
typedef struct UIDManager {
	SceUInt32 reserved0;
	SceClass *classes;			/* 0x04: the ring of every class */
	FixedHeap *guidEntryHeap;		/* 0x08: fixed heap of the GUID entries */
	struct PhyPageTableMgr *phyPageTableMgr;	/* 0x0c */
	struct PhyMemPart *phyPartKernel;	/* 0x10 */
	struct PhyMemPart *phyPartTool;		/* 0x14 */
	FixedHeap *fixedHeap16B;		/* 0x18 */
	FixedHeap *fixedHeap32B;		/* 0x1c: kernel fixed heaps by object size */
	FixedHeap *fixedHeap48B;		/* 0x20 */
	FixedHeap *fixedHeap64B;		/* 0x24 */
	FixedHeap *fixedHeap128B;		/* 0x28 */
	FixedHeap *fixedHeap256B;		/* 0x2c */
	FixedHeap *fixedHeap512B;		/* 0x30 */
	FixedHeap *fixedHeapL2Object;		/* 0x34 */
	NameHeapControl *nameHeapControls;	/* 0x38: two of them */
	struct SmbFifo *smbFifo;		/* 0x3c: the simple memblock FIFOs */
	struct Partition *kernelPartition;	/* 0x40: simple memblocks and L2 tables live here */
	void *heapDefault;			/* 0x44 */
	ObjectHeap *kernelObjectHeap;		/* 0x48: the kernel's object heap */
	SceUInt8 reserved1[0x84 - 0x4c];
	struct PhyMemLow *phyMemLowMain;	/* 0x84 */
	struct PhyMemLow *phyMemLowSub;		/* 0x88 */
	void *phyPageReset;			/* 0x8c */
	const struct PhyMemPageRange *phyPageVenezia;	/* 0x90 */
	const struct PhyMemPageRange *phyPageVip;	/* 0x94 */
	SceClass *uidClass;			/* 0x98: "SceUIDClass", the root */
	SceClass *dlinkClass;			/* 0x9c: "SceUIDDLinkClass" */
	UIDHeapClass *uidHeapClass;		/* 0xa0: "SceUIDHeapClass" */
	FixedHeapClass *fixedHeapClass;		/* 0xa4: "SceUIDFixedHeapClass" */
	FixedHeapClass *entryHeapClass;		/* 0xa8: "SceUIDEntryHeapClass" */
	SceClass *smbClass;			/* 0xac: "SceUIDSimpleMemBlockClass" */
	SceClass *memBlockClass;		/* 0xb0: "SceUIDMemBlockClass" */
	SceClass *tinyPartitionClass;		/* 0xb4: "SceUIDTinyPartitionClass" */
	SceClass *partitionClass;		/* 0xb8: "SceUIDPartitionClass" */
	SceClass *heapClass;			/* 0xbc: "SceUIDKernelHeapClass" */
	void *registerHeap;			/* 0xc0: "heap" of objects registered, not created */
	SceUInt32 partitionReserved[2];		/* 0xc4: kept by PartitionModuleStart, unused */
	SceUID uncacheHeap;			/* 0xcc: the uncached kernel heap */
	SceUInt32 reserved2;
	struct AddressSpaceGlobals *addressSpaceGlobals;	/* 0xd4 */
	SceUInt32 processStorageKey;		/* 0xd8: sysmem's process-local storage */
} UIDManager;

extern UIDManager *g_uidManager;

/* heap.c: the kernel heaps */
struct KernelHeap;
int HeapLock(struct KernelHeap *heap);
void HeapUnlock(struct KernelHeap *heap, int intr);
void *HeapAllocPrologue(struct KernelHeap *heap, SceSize size, SceSize alignment);
int HeapFree(struct KernelHeap *heap, void *ptr);
int HeapModuleStart(const UIDManager *manager);

/* a VM block of a process (sceKernelAllocMemBlockForVMForUser) */
typedef struct SysmemVMBlock {
	SceUID puid;				/* -1: free */
	uintptr_t base;
	SceSize size;
} SysmemVMBlock;

#define SYSMEM_VM_BLOCKS		32

/* sysmem's process-local storage (key UIDManager.processStorageKey) */
typedef struct SysmemProcessStorage {
	SceKernelSpinlock lock;
	SceSize vmSize;				/* 0x04: of all VM blocks, at most 16 MiB */
	SysmemVMBlock *vmBlocks;		/* 0x08: SYSMEM_VM_BLOCKS, freed when the process ends */
	SceUInt32 managerThread;		/* 0x0c */
} SysmemProcessStorage;

/* callbacks of a fixed-heap walk: low nibble 1 = match, high nibble 0x20 = stop */
#define UID_WALK_MATCH		0x01
#define UID_WALK_SKIP		0x02
#define UID_WALK_CONTINUE	0x10
#define UID_WALK_STOP		0x20
