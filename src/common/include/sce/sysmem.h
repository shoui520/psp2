/*
 * SceSysmem (os0:kd/sysmem.skprx) driver interface additions to vitasdk.
 */

#pragma once

/* vitasdk's SceKernelAllocMemBlockKernelOpt names few fields: Sony's layout below */
#if defined(_PSP2KERN_KERNEL_SYSMEM_H_) || defined(_PSP2KERN_KERNEL_SYSMEM_HEAP_H_)
#error "include sce/sysmem.h before <psp2kern/kernel/sysmem.h>"
#endif
#define SceKernelAllocMemBlockKernelOpt	SceKernelAllocMemBlockKernelOpt_vitasdk
#define SceKernelAllocMemBlockOptKernel	SceKernelAllocMemBlockOptKernel_vitasdk
#define ksceKernelAllocMemBlock		ksceKernelAllocMemBlock_vitasdk
#define ksceKernelAllocPartitionMemBlock	ksceKernelAllocPartitionMemBlock_vitasdk
#define SceKernelHeapCreateOpt		SceKernelHeapCreateOpt_vitasdk
#define ksceKernelCreateHeap		ksceKernelCreateHeap_vitasdk
#define SceAllocOpt			SceAllocOpt_vitasdk
#define ksceKernelAllocHeapMemoryWithOption	ksceKernelAllocHeapMemoryWithOption_vitasdk
#include <psp2kern/kernel/sysmem.h>
#include <stddef.h>
#undef SceKernelAllocMemBlockKernelOpt
#undef SceKernelAllocMemBlockOptKernel
#undef ksceKernelAllocMemBlock
#undef ksceKernelAllocPartitionMemBlock
#undef SceKernelHeapCreateOpt
#undef ksceKernelCreateHeap
#undef SceAllocOpt
#undef ksceKernelAllocHeapMemoryWithOption

/*
 * ksceKernelAllocMemBlock's options (Sony: SceKernelAllocMemBlockOptKernel);
 * attr says which fields are given (SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_*, and
 * sysmem's ALLOC_ATTR_* for the rest)
 */
typedef struct SceKernelAllocMemBlockKernelOpt {
	SceSize size;				/* sizeof(SceKernelAllocMemBlockKernelOpt) */
	SceUInt32 reserved;
	SceUInt32 attr;				/* 0x08 */
	SceUInt32 vbase;			/* 0x0c: HAS_VBASE */
	SceUInt32 paddr;			/* 0x10: HAS_PADDR */
	SceSize alignment;			/* 0x14: HAS_ALIGNMENT */
	SceUInt32 extraLow;			/* 0x18: HAS_EXTRA_LOW */
	SceUInt32 extraHigh;			/* 0x1c: HAS_EXTRA_HIGH */
	SceUInt32 mirror_blockid;		/* 0x20: HAS_MIRROR_BLOCKID, the base block */
	SceUID pid;				/* 0x24: HAS_PID */
	SceKernelPaddrList *paddr_list;		/* 0x28: HAS_PADDR_LIST */
	SceSize unitSize;			/* 0x2c: HAS_UNIT_SIZE */
	SceUInt8 domain;			/* 0x30: HAS_DOMAIN, the MMU domain */
	SceUInt8 padding[3];
	SceUInt32 openFlags;			/* 0x34: CLIENT_INFO, the open flags allowed */
	SceUInt32 capabilities[8];		/* 0x38: CLIENT_INFO, those an opener needs */
} SceKernelAllocMemBlockKernelOpt;

_Static_assert(sizeof(SceKernelAllocMemBlockKernelOpt) == 0x58, "SceKernelAllocMemBlockKernelOpt size");

typedef SceKernelAllocMemBlockKernelOpt SceKernelAllocMemBlockOptKernel;

SceUID ksceKernelAllocMemBlock(const char *name, SceKernelMemBlockType type, SceSize size,
			       SceKernelAllocMemBlockKernelOpt *opt);
SceUID ksceKernelAllocPartitionMemBlock(SceUID part, const char *name, SceKernelMemBlockType type,
					SceSize size, const SceKernelAllocMemBlockOptKernel *opt);

/*
 * SceKernelAllocMemBlockKernelOpt attr bits beyond vitasdk's
 * SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_* names (HAS_PADDR, HAS_ALIGNMENT,
 * HAS_MIRROR_BLOCKID, HAS_PID, HAS_PADDR_LIST, PHYCONT, ALLOW_PARTIAL_OP).
 * NOPHYPAGE, SHARE_VBASE and SHARE_PHYPAGE are Sony's (0.990 messages).
 */
#define ALLOC_ATTR_HAS_VBASE		0x00000001	/* opt.vbase is the address */
#define ALLOC_ATTR_HAS_EXTRA_LOW	0x00000008
#define ALLOC_ATTR_HAS_EXTRA_HIGH	0x00000010
#define ALLOC_ATTR_HAS_CLIENT_INFO	0x00000020	/* opt.openFlags..capabilities[7] are given */
#define ALLOC_ATTR_INVALID		0x00000d00
#define ALLOC_ATTR_HAS_UNIT_SIZE	0x00002000	/* opt.unitSize */
#define ALLOC_ATTR_CLIENT_INFO		0x00004000	/* the block carries a MemBlockClientInfo */
#define ALLOC_ATTR_USER_CLIENT		0x00008000	/* allocated for a user process's client */
#define ALLOC_ATTR_HAS_DOMAIN		0x00010000	/* opt.domain is the MMU domain */
#define ALLOC_ATTR_VBASE_ANY		0x00020000	/* address 0 allowed */
#define ALLOC_ATTR_NOPHYPAGE		0x00040000	/* no physical pages allocated */
#define ALLOC_ATTR_NO_MAP		0x00080000	/* mapped by the caller */
#define ALLOC_ATTR_NO_ALIGN		0x00100000	/* opt.alignment ignored */
#define ALLOC_ATTR_NO_CLEAR		0x00400000	/* the block is not cleared */
#define ALLOC_ATTR_SHARE_VBASE		0x00800000	/* the base block's size */
#define ALLOC_ATTR_SHARE_PHYPAGE	0x01000000	/* the base block's physical pages */
#define ALLOC_ATTR_KEEP_CLEAR		0x02000000	/* cleared whatever the dipsw says */
#define ALLOC_ATTR_BLOCK_SHIFT		28		/* bits 28..31: the block's allocationAttributes */

/* Complete result of Sysmem's physical range query. */
typedef struct SceKernelPhyMemRangeInfo {
	SceSize size;
	uintptr_t base;
	SceSize rangeSize;
} SceKernelPhyMemRangeInfo;

int sceKernelGetPhyMemInfo(SceUInt32 type, SceKernelPhyMemRangeInfo *info);

/*
 * ksceKernelCreateHeap's options; flags is Sony's name ("pOpt->flags"),
 * attr and uselock vitasdk's, kept for its callers
 */
/* SceKernelHeapCreateOpt flags: which fields are given */
#define HEAP_AUTO_EXTEND		0x00000001
#define HEAP_HAS_MAX_SIZE		0x00000100
#define HEAP_HAS_MEMBLOCK		0x00000200	/* built in the memblock given */
#define HEAP_HAS_MEMTYPE		0x00000400
#define HEAP_HAS_PID			0x00000800
#define HEAP_HAS_HEAPCB			0x00001000	/* the heap object comes from the caller */

typedef struct SceKernelHeapCreateOpt {
	SceSize size;				/* sizeof(SceKernelHeapCreateOpt) */
	union {
		SceUInt32 flags;
		SceUInt32 attr;
		SceUInt32 uselock;
	};
	SceSize maxSize;			/* 0x08: "pOpt->maxSize" */
	SceUID memBlock;			/* 0x0c: the memblock to build the heap in */
	SceUInt32 memtype;			/* 0x10: SCE_KERNEL_HEAP_ATTR_HAS_MEMORY_TYPE */
	SceUID pid;				/* 0x14 */
	struct KernelHeap *heap;		/* 0x18 */
} SceKernelHeapCreateOpt;

_Static_assert(sizeof(SceKernelHeapCreateOpt) == 0x1c, "SceKernelHeapCreateOpt size");

SceUID ksceKernelCreateHeap(const char *name, SceSize size, SceKernelHeapCreateOpt *opt);

/* ksceKernelAllocHeapMemoryWithOption's options */
typedef struct SceAllocOpt {
	SceSize size;				/* sizeof(SceAllocOpt) */
	SceUInt32 mmapKind;			/* 0x04: out, HeapMmapResult.kind */
	SceSize align;
	void *mmapBase;				/* 0x0c: out */
	SceSize mmapSize;			/* 0x10: out */
} SceAllocOpt;

_Static_assert(sizeof(SceAllocOpt) == 0x14, "SceAllocOpt size");

void *ksceKernelAllocHeapMemoryWithOption(SceUID uid, SceSize len, SceAllocOpt *opt);

#define SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR	0x00000002

#include <sce/uid_class.h>
#include <psp2kern/kernel/sysmem/uid_guid.h>

SceUID ksceGUIDKernelCreate(SceClass *cls, const char *name, SceObjectBase **obj);

/*
 * ksceGUIDKernelCreateWithOpt's options (Sony: UIDCreateOpt, after
 * UIDCreateOptCheckForPUID); each flag makes sysmem read its field.
 * (vfs_subr.c's use of vitasdk's SceGUIDKernelCreateOpt is still to move
 * onto this one.)
 */
#define UID_CREATE_OPT_OBJECT		0x00000001	/* the caller's memory holds the object */
#define UID_CREATE_OPT_HEAP_GUID	0x00000002	/* from the heap of a GUID */
#define UID_CREATE_OPT_PROCESS_HEAP	0x00000008	/* from an object heap of a process */
#define UID_CREATE_OPT_TYPE		0x00000010
#define UID_CREATE_OPT_ATTR		0x00000100
#define UID_CREATE_OPT_OWNER		0x00000200
#define UID_CREATE_OPT_CLASS		0x00000400	/* refused: a duplicate class */

typedef struct UIDCreateOpt {
	SceUInt32 flags;			/* UID_CREATE_OPT_* */
	SceObjectBase *object;			/* UID_CREATE_OPT_OBJECT */
	SceUID heapGuid;			/* UID_CREATE_OPT_HEAP_GUID */
	SceUID pid;				/* UID_CREATE_OPT_PROCESS_HEAP */
	SceUInt32 heapKind;			/* which of the process's object heaps */
	SceUInt32 type;				/* UID_CREATE_OPT_TYPE: the object's type */
	SceUInt32 attr;				/* UID_CREATE_OPT_ATTR */
	SceUID ownerPid;			/* UID_CREATE_OPT_OWNER */
	SceClass *cls;				/* UID_CREATE_OPT_CLASS */
} UIDCreateOpt;

/* scePUIDOpenByGUIDWithOptForKernel, scePUIDOpenByNameWithOptForKernel */
SceUID SceSysmemForKernel_1FF2749A(SceUID pid, SceUID guid, const UIDCreateOpt *opt);
SceUID SceSysmemForKernel_201E970B(SceUID pid, const char *name, const UIDCreateOpt *opt);
int ksceKernelIsAccessibleRange(SceUInt32 type, const void *addr, SceSize size);

/* the memblock's virtual base address */
int ksceKernelGetMemBlockVBase(SceUID uid, void **vbase);

/* a physical memory partition's sizes (phymem.c) */
typedef struct SceKernelPhyMemPartInfoCore {
	uintptr_t pbase;		/* the lowest segment's */
	SceSize psize;			/* every segment's */
	SceSize freeSize;
	SceUInt32 freeUnitCount;	/* 1 MiB units on the free list */
} SceKernelPhyMemPartInfoCore;

/* sceKernelGetPhyMemPartInfoForDebugger */
typedef struct SceKernelPhyMemPartInfoForDebugger {
	SceSize size;
	SceUID uid;
	const char *name;		/* 0x08 */
	SceUInt32 type;			/* 0x0c */
	SceSize freeSize;		/* 0x10 */
	SceSize freeSizeMin;		/* 0x14 */
	SceSize cacheSize;		/* 0x18: added to */
	SceUInt32 freeUnitCount;	/* 0x1c: 1 MiB units on the free list */
	SceUInt32 segmentCount;		/* 0x20 */
	struct {
		uintptr_t pbase;
		SceSize psize;
	} segments[16];			/* 0x24 */
	SceUInt32 freeBlockCount;	/* 0xa4 */
	SceUInt32 freeBlockMax;		/* 0xa8 */
	struct SceKernelPhyMemPartFreeBlock {
		uintptr_t pbase;
		SceSize size;
	} *freeBlocks;			/* 0xac: may be NULL */
} SceKernelPhyMemPartInfoForDebugger;

_Static_assert(offsetof(SceKernelPhyMemPartInfoForDebugger, freeBlocks) == 0xac,
	       "debugger phymem info");

/* the allocator an address space keeps its own objects with (sceKernelCreateAddressSpace) */
typedef struct AddressSpaceHeapOps {
	void *heap;
	void *(*alloc)(void *heap, SceSize size);
	void (*free)(void *heap, void *object);
} AddressSpaceHeapOps;

/* sceKernelCreateAddressSpace's options */
#define ADDRESS_SPACE_OPT_HEAP		0x00000002	/* heap is given */
#define ADDRESS_SPACE_OPT_L1_TABLE	0x00000004	/* l1Table is given */

typedef struct AddressSpaceCreateOpt {
	SceSize size;
	SceUInt32 flags;			/* ADDRESS_SPACE_OPT_* */
	const AddressSpaceHeapOps *heap;	/* 0x08: for the object and its context */
	uint32_t *l1Table;			/* 0x0c: the new L1 table (16 KiB) */
} AddressSpaceCreateOpt;

#define ADDRESS_SPACE_PARTITIONS	32
#define ADDRESS_SPACE_PHYMEMPARTS	16

/* an address space's partitions and physical memory partitions (SceSysmemForKernel_C3EF4055) */
typedef struct AddressSpaceInfo {
	SceSize size;				/* sizeof(AddressSpaceInfo) */
	SceUID uid;
	SceUInt8 asid;
	SceUInt32 partitionCount;		/* 0x0c */
	struct {
		SceSize size;
		SceUInt32 data[11];
	} partition[ADDRESS_SPACE_PARTITIONS];	/* 0x10: PartitionInfo */
	SceUInt32 phyMemPartCount;		/* 0x610 */
	void *phyMemPart[ADDRESS_SPACE_PHYMEMPARTS];	/* 0x614 */
} AddressSpaceInfo;

_Static_assert(sizeof(AddressSpaceInfo) == 0x654, "AddressSpaceInfo size");

/* names unknown (os0:kd/deci4p_pamp.skprx) */
int SceSysmemForDriver_C9928F5E(uintptr_t *base, SceSize *size);	/* the user I/O physical window */
int SceSysmemForKernel_C3EF4055(SceUID uid, AddressSpaceInfo *info);

/* names unknown (os0:kd/intrmgr.skprx): name string copy into / release from kernel heap */
int SceSysmemForKernel_B628B46B(const char *name, unsigned int flags, char **out);
int SceSysmemForKernel_634D0D0C(char *name, unsigned int flags);
const char *ksceGUIDName(SceUID uid);

/* Global uncached heap allocator; NID 0x7750CEA7, size is the sole argument. */
void *ksceKernelAllocUncacheHeapMemory(SceSize size);

/* NID 0xF3BBE2E1: memory type for the block containing this address. */
int ksceKernelGetMemBlockMemtypeByAddr(const void *address);

/* SceKernelUtilsForDriver: the SDK's libmt19937 (libmt19937.h) */
typedef struct SceMt19937Context {
	SceUInt32 count;
	SceUInt32 state[624];
} SceMt19937Context;

int ksceMt19937Init(SceMt19937Context *ctx, SceUInt32 seed);
SceUInt32 ksceMt19937UInt(SceMt19937Context *ctx);

/* a memblock as the kernel sees it (memblock.c) */
typedef struct MemBlockInfoDetails {
	SceUInt32 type;
	SceUID guid;			/* 0x04 */
	const char *name;		/* 0x08 */
	uintptr_t vbase;		/* 0x0c */
	SceSize size;			/* 0x10 */
	SceSize mapSize;		/* 0x14 */
	SceSize headFree;		/* 0x18 */
	SceSize tailFree;		/* 0x1c */
	SceUInt32 allocationAttributes;	/* allocation option attribute bits 28..31 */
	SceUID partitionUid;		/* 0x24 */
	void *phyMemPart;		/* 0x28 */
} MemBlockInfoDetails;

typedef struct MemBlockInfoEx {
	SceSize size;			/* sizeof(MemBlockInfoEx) */
	MemBlockInfoDetails details;
} MemBlockInfoEx;

_Static_assert(sizeof(MemBlockInfoEx) == 0x30, "MemBlockInfoEx size");

/* MemBlockInfoEx with the physical ranges (sceKernelGetMemBlockInfo) */
typedef struct MemBlockPhyInfo {
	MemBlockInfoEx ex;
	SceUInt32 count;		/* 0x30: the backed pieces */
	SceUInt32 rangeCount;		/* 0x34: the ranges below, at most 16 */
	uintptr_t pbase[16];		/* 0x38 */
	SceSize size[16];		/* 0x78 */
} MemBlockPhyInfo;

_Static_assert(sizeof(MemBlockPhyInfo) == 0xb8, "MemBlockPhyInfo size");

int ksceKernelMemBlockGetInfoEx(SceUID uid, MemBlockPhyInfo *info);
int ksceKernelMemBlockType2Memtype(SceUInt32 type);
