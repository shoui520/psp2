/*
 * SceSysmem's address spaces: an address space object, its translation context
 * and L1 tables, its partitions and their L2 page tables. Sysmem owns them; the
 * debugger (deci4p_sdbgp) walks them.
 */
#pragma once

#include <stdint.h>
#include <scetypes.h>
#include <sysmem/mmu.h>
#include <sysmem/spinlock.h>
#include <sysmem/sysmem.h>
#include <sysmem/uid_class.h>

struct MemBlockObject;
struct FixedHeap;
struct Partition;

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
/*
 * name unknown: the object a partition with attr bit 0 is made with (only
 * the root, in NSKBL: 24 bytes, zeroed, never read there).
 */
typedef struct PartitionRootObject {
	SceUInt32 reserved[6];
} PartitionRootObject;

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
	struct PartitionRootObject *rootObject;	/* 0x58: attr bit 0's object, from PartitionInit */
	SceUInt32 entryAttr;			/* 0x5c: the creation table entry's attr ... */
	SceUInt32 entryAttrMask;		/* 0x60: ... and mask; stored, never read */
	SceUID pid;				/* 0x64: the owning process */
	SceUID uid;				/* 0x68 */
	SceKernelRWSpinlock l2CacheLock;	/* 0x6c */
	struct L2PageTable *l2Cache;		/* 0x70: free L2 page tables kept for reuse */
	SceUInt32 l2CacheCount;			/* 0x74 */
	volatile SceUInt32 l2Count;		/* 0x78: the low half in use, the high half the peak */
	volatile SceUInt32 virPageCount;	/* 0x7c: the low half in use, the high half the peak */
} Partition;

_Static_assert(sizeof(Partition) == 0x80, "Partition size");

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
	const SceKernelHeapHook *heap;		/* 0x168 */
	SceUInt32 magic;			/* 0x16c: ADDRESS_SPACE_MAGIC */
} AddressSpaceObject;

/* the address space object's magic, and the fill it is destroyed with */
#define ADDRESS_SPACE_MAGIC		0x4d95aeec
#define ADDRESS_SPACE_DEAD_FILL		0x07fd9f97

_Static_assert(sizeof(AddressSpaceObject) == 0x170, "AddressSpaceObject size");

struct Partition *SceSysmemForKernel_1F84FE44(AddressSpaceObject *as, SceUInt32 index);
L2PageTable *SceSysmemForKernel_456ECB54(Partition *partition, uintptr_t va);
