/*
 * SceSysmem (os0:kd/sysmem.skprx): its driver and kernel interface.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <psp2kern/types.h>
#include <sysmem/memblock.h>
#include <sysmem/mmu.h>

#ifdef __cplusplus
extern "C" {
#endif

/* sysmem's objects other modules hold by pointer (laid out in sysmem_internal.h, phymem.c) */
typedef struct PhyMemPart PhyMemPart;
typedef struct ObjectHeap ObjectHeap;
typedef struct UIDEntry UIDEntry;
typedef struct FixedHeap FixedHeap;
typedef struct TinyPartition TinyPartition;
typedef struct SceKernelHeapHook SceKernelHeapHook;

/*
 * SceSysmemForDriver_C94850C9's options (Sony: SceKernelAllocMemBlockOptKernel);
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

#ifndef __cplusplus	/* C only; g++ 4.8 (C++98) has no static_assert */
_Static_assert(sizeof(SceKernelAllocMemBlockKernelOpt) == 0x58, "SceKernelAllocMemBlockKernelOpt size");
#endif

typedef SceKernelAllocMemBlockKernelOpt SceKernelAllocMemBlockOptKernel;

/* vitasdk: ksceKernelAllocMemBlock */
SceUID SceSysmemForDriver_C94850C9(const char *name, SceKernelMemBlockType type, SceSize size,
			       SceKernelAllocMemBlockKernelOpt *opt);
SceUID sceKernelAllocPartitionMemBlock(SceUID part, const char *name, SceKernelMemBlockType type,
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
 * SceSysmemForDriver_9328E0E8's options; flags is Sony's name ("pOpt->flags"),
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
	SceUInt32 memtype;			/* 0x10: HEAP_HAS_MEMTYPE */
	SceUID pid;				/* 0x14 */
	struct KernelHeap *heap;		/* 0x18 */
} SceKernelHeapCreateOpt;

#ifndef __cplusplus
_Static_assert(sizeof(SceKernelHeapCreateOpt) == 0x1c, "SceKernelHeapCreateOpt size");
#endif

/* vitasdk: ksceKernelCreateHeap */
SceUID SceSysmemForDriver_9328E0E8(const char *name, SceSize size, SceKernelHeapCreateOpt *opt);

/* SceSysmemForDriver_49D4DD9B's options */
typedef struct SceAllocOpt {
	SceSize size;				/* sizeof(SceAllocOpt) */
	SceUInt32 mmapKind;			/* 0x04: out, HeapMmapResult.kind */
	SceSize align;
	void *mmapBase;				/* 0x0c: out */
	SceSize mmapSize;			/* 0x10: out */
} SceAllocOpt;

#ifndef __cplusplus
_Static_assert(sizeof(SceAllocOpt) == 0x14, "SceAllocOpt size");
#endif

/* vitasdk: ksceKernelAllocHeapMemoryWithOption */
void *SceSysmemForDriver_49D4DD9B(SceUID uid, SceSize len, SceAllocOpt *opt);

#include <sysmem/uid_class.h>

/* vitasdk: ksceGUIDKernelCreate */
SceUID SceSysmemForDriver_56A13E90(SceClass *cls, const char *name, SceObjectBase **obj);

/*
 * sceGUIDKernelCreateWithOpt's options (Sony: UIDCreateOpt, after
 * UIDCreateOptCheckForPUID); each flag makes sysmem read its field.
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
/* vitasdk: ksceKernelIsAccessibleRange */
int SceSysmemForDriver_9C78064C(SceUInt32 type, const void *addr, SceSize size);

/* the memblock's virtual base address */
/* vitasdk: ksceKernelGetMemBlockVBase */
int SceSysmemForDriver_B81CF0A3(SceUID uid, void **vbase);

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

#ifndef __cplusplus
_Static_assert(offsetof(SceKernelPhyMemPartInfoForDebugger, freeBlocks) == 0xac,
	       "debugger phymem info");
#endif

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

#ifndef __cplusplus
_Static_assert(sizeof(AddressSpaceInfo) == 0x654, "AddressSpaceInfo size");
#endif

/* names unknown (os0:kd/deci4p_pamp.skprx) */
int SceSysmemForDriver_C9928F5E(uintptr_t *base, SceSize *size);	/* the user I/O physical window */
int SceSysmemForKernel_C3EF4055(SceUID uid, AddressSpaceInfo *info);

/* names unknown (os0:kd/intrmgr.skprx): name string copy into / release from kernel heap */
int SceSysmemForKernel_B628B46B(const char *name, unsigned int flags, char **out);
int SceSysmemForKernel_634D0D0C(char *name, unsigned int flags);
const char *SceSysmemForDriver_E655852F(SceUID uid);	/* vitasdk: ksceGUIDName */

/* Global uncached heap allocator; NID 0x7750CEA7, size is the sole argument. */
/* vitasdk: ksceKernelAllocUncacheHeapMemory */
void *SceSysmemForDriver_7750CEA7(SceSize size);

/* NID 0xF3BBE2E1: memory type for the block containing this address. */
/* vitasdk: ksceKernelGetMemBlockMemtypeByAddr */
int SceSysmemForDriver_F3BBE2E1(const void *address);


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

#ifndef __cplusplus
_Static_assert(sizeof(MemBlockInfoEx) == 0x30, "MemBlockInfoEx size");
#endif

/* MemBlockInfoEx with the physical ranges (sceKernelGetMemBlockInfo) */
typedef struct MemBlockPhyInfo {
	MemBlockInfoEx ex;
	SceUInt32 count;		/* 0x30: the backed pieces */
	SceUInt32 rangeCount;		/* 0x34: the ranges below, at most 16 */
	uintptr_t pbase[16];		/* 0x38 */
	SceSize size[16];		/* 0x78 */
} MemBlockPhyInfo;

#ifndef __cplusplus
_Static_assert(sizeof(MemBlockPhyInfo) == 0xb8, "MemBlockPhyInfo size");
#endif

/* vitasdk: ksceKernelMemBlockGetInfoEx */
int SceSysmemForDriver_24A99FFF(SceUID uid, MemBlockPhyInfo *info);
/* vitasdk: ksceKernelMemBlockType2Memtype */
int SceSysmemForDriver_20C811FA(SceUInt32 type);

int SceSysmemForDriver_A7C0D1FC(const void *va);
SceUInt32 SceSysmemForDriver_CB0F3A33(SceUInt32 type);
int GUIDGetClass(SceUID guid, SceClass **cls);
/* vitasdk: ksceGUIDGetName */
int SceSysmemForDriver_A78755EB(SceUID guid, const char **name);
int SceSysmemForDriver_0FC24464(SceUID guid, void *object);	/* vitasdk: ksceGUIDGetObject */
/* vitasdk: ksceGUIDReferObjectWithSubclass */
int SceSysmemForDriver_72A98D17(SceUID guid, SceClass *cls, SceObjectBase **object);
int SceSysmemForDriver_4CFA4100(SceUID guid, const char *name);	/* vitasdk: ksceGUIDSetName */
/* vitasdk: ksceKernelAllocUncacheHeapMemoryWithOption */
void *SceSysmemForDriver_0B4ED16A(SceSize size, SceAllocOpt *opt);
SceSSize sceKernelCountFillValueFromUserProc(SceUID pid, const void *src, SceUInt32 value, SceSize size);
int SceSysmemForDriver_F50BDC0C(SceUID guid);	/* vitasdk: ksceKernelDecRefCountMemBlock */
/* vitasdk: ksceKernelGetMemBlockInfo */
int SceSysmemForDriver_A73CFFEF(SceUID uid, SceInt32 level, MemBlockPhyInfo *info);
/* vitasdk: ksceKernelGetMemBlockMappedBase */
int SceSysmemForDriver_0B1FD5C3(SceUID uid, void **base);
/* name from the NID database: the kind of memory an address maps (3: mapped at its physical address) */
/* vitasdk: ksceKernelGetPhysicalMemoryType */
int SceSysmemForDriver_0AAA4FDD(const void *va);
int SceSysmemForDriver_EAF3849B(SceUID guid);	/* vitasdk: ksceKernelIncRefCountMemBlock */
/* vitasdk: ksceKernelIsAccessibleRangeProc */
int SceSysmemForDriver_9F6E45E3(SceUID pid, SceUInt32 mode, const void *addr, SceSize size);
/* vitasdk: ksceKernelMemBlockTypeGetPrivileges */
SceUInt32 SceSysmemForDriver_6A0792A3(SceUInt32 type);
/* vitasdk: ksceKernelProcModeVAtoPA */
int SceSysmemForDriver_61A67D32(SceUID pid, SceUInt32 mode, const void *va, uintptr_t *pa);
/* vitasdk: kscePUIDGetClass */
int SceSysmemForDriver_E9728A12(SceUID pid, SceUID puid, SceClass **out);
/* vitasdk: kscePUIDGetObject */
int SceSysmemForDriver_FE6D7FAE(SceUID pid, SceUID puid, SceObjectBase **out);
int sceKernelShrinkHeap(SceUID uid);

/* sysmem's own, opaque to the modules that pass them */
struct PhyPage;
struct malloc_segment_range;

int CreatePhyMemPartByPbase(const char *name, SceUInt32 type, uintptr_t pbase, SceSize psize, PhyMemPart **out);
SceUID GUIDOpenByName(const char *name);
int GetMemBlockInfoAll(TinyPartition *tiny, uintptr_t lo, uintptr_t hi, uintptr_t pa, SceSize psize, SceUInt32 level, MemBlockInfoEx *info, SceSize infoSize, SceUInt32 *count);
/* vitasdk: ksceKernelProcUserMap */
SceUID SceSysmemForDriver_0091D74D(SceUID pid, const char *name, int permission, const void *user_buf, SceSize size, void **kernel_page, SceSize *kernel_size, SceUInt32 *kernel_offset);
int SceSysmemForDriver_009E1C61(SceUID guid);	/* vitasdk: ksceKernelFreeMemBlock */
/* vitasdk: ksceGUIDReferObjectWithClass */
int SceSysmemForDriver_00ED6C14(SceUID guid, SceClass *sce_class, SceObjectBase **object);
/* vitasdk: ksceKernelMapMemBlockWithFlag */
int SceSysmemForDriver_04059C4B(SceUID guid, int flags);
/* vitasdk: kscePUIDGetName */
int SceSysmemForDriver_09896EB7(SceUID pid, SceUID puid, const char **name);
/* vitasdk: ksceGUIDReferObject */
int SceSysmemForDriver_0F5C84B7(SceUID guid, SceObjectBase **object);
/* vitasdk: kscePUIDSetName */
int SceSysmemForDriver_12624884(SceUID pid, SceUID puid, const char *name);
int SceSysmemForDriver_13805CA8(SceUID guid, uintptr_t va, SceSize size, SceUInt32 flags);
int SceSysmemForDriver_16713BE8(SceUID guid, uintptr_t va, SceSize size, SceUInt32 flags);
/* vitasdk: kscePUIDtoGUIDWithClass */
SceUID SceSysmemForDriver_184172B1(SceUID pid, SceUID puid, SceClass *cls);
/* vitasdk: ksceKernelCopyFromToUser */
int SceSysmemForDriver_1BD44DD5(void *dst, const void *src, SceSize size);
/* vitasdk: ksceKernelUnlockRangeWithMode */
int SceSysmemForDriver_22CBE925(SceKernelMemoryRefPerm mode, void *addr, SceSize size);
/* vitasdk: ksceKernelUserMap */
SceUID SceSysmemForDriver_278BC201(const char *name, int permission, const void *user_buf, SceSize size, void **kernel_page, SceSize *kernel_size, SceUInt32 *kernel_offset);
/* vitasdk: ksceKernelProcessSwitchContext */
int SceSysmemForDriver_2D711589(const MMUContext *context, MMUContext *saved);
/* vitasdk: ksceKernelProcessGetContext */
int SceSysmemForDriver_2ECF7944(SceUID pid, MMUContext **context);
/* vitasdk: ksceKernelVARangeToPARangeBySW */
int SceSysmemForDriver_32257A24(const SceKernelVARange *range, SceKernelPARange *out);
SceUID SceSysmemForDriver_45D22597(SceUID pid, SceUID puid);	/* vitasdk: kscePUIDtoGUID */
int SceSysmemForDriver_4C584B29(SceUID guid, uintptr_t va, SceSize size, SceUInt32 op);
/* vitasdk: kscePUIDOpenByName */
SceUID SceSysmemForDriver_513B9DDD(SceUID pid, const char *name);
int SceSysmemForDriver_58D21746(SceUID guid);	/* vitasdk: ksceKernelMapMemBlock */
int SceSysmemForDriver_59A4402F(void *addr, SceSize size);	/* vitasdk: ksceKernelLockRange */
int SceSysmemForDriver_64DBE472(SceUID guid, uintptr_t range[2]);
/* vitasdk: ksceKernelLockRangeProc */
int SceSysmemForDriver_659586BF(SceUID pid, void *addr, SceSize size);
int SceSysmemForDriver_6C76AD89(SceUID guid, uintptr_t va, SceSize size, SceUInt32 flags);
/* vitasdk: ksceKernelCopyToUser */
int SceSysmemForDriver_6D88EF8A(void *dst, const void *src, SceSize size);
int SceSysmemForDriver_75C70DE0(void *addr, SceSize size);	/* vitasdk: ksceKernelUnlockRange */
/* vitasdk: ksceGUIDReferObjectWithClassLevel */
int SceSysmemForDriver_77066FD1(SceUID guid, SceClass *pClass, SceUInt32 level, SceObjectBase **entry);
/* vitasdk: ksceKernelAllocHeapMemory */
void *SceSysmemForDriver_7B4CB60A(SceUID uid, SceSize size);
/* vitasdk: ksceKernelUserMapWithFlags */
SceUID SceSysmemForDriver_7D4F8B5F(const char *name, int permission, int flags, const void *user_buf, SceSize size, void **kernel_page, SceSize *kernel_size, unsigned int *kernel_offset);
/* vitasdk: ksceKernelStrncpyToUser */
SceSSize SceSysmemForDriver_80BD6FEB(char *dst, const char *src, SceSize n);
int SceSysmemForDriver_84A4AF5E(SceUID pid, SceUID puid);	/* vitasdk: kscePUIDClose */
/* vitasdk: ksceKernelFindProcMemBlockByAddr */
SceUID SceSysmemForDriver_857F1D5A(SceUID pid, const void *addr, SceSize size);
/* vitasdk: ksceGUIDCreate */
SceUID SceSysmemForDriver_89A44858(SceClass *cls, const char *name, SceUID heapGuid, SceObjectBase **obj);
/* vitasdk: ksceKernelFindMemBlockByAddr */
SceUID SceSysmemForDriver_8A1742F6(const void *addr, SceSize size);
int SceSysmemForDriver_8C43B052(SceUID guid, uintptr_t va, SceSize size, SceUInt32 flags);
int SceSysmemForDriver_8D160E65(const void *va, uintptr_t *pa);	/* vitasdk: ksceKernelVAtoPA */
/* vitasdk: kscePUIDOpenByNameWithClass */
SceUID SceSysmemForDriver_8DA0BCA5(SceUID pid, const char *name, SceClass *cls);
/* vitasdk: ksceKernelGetMemBlockPARange */
int SceSysmemForDriver_98C15666(SceUID guid, SceKernelPARange *range);
/* vitasdk: ksceKernelGetMemBlockBase */
int SceSysmemForDriver_A841EDDA(SceUID uid, void **base);
/* vitasdk: ksceKernelUnlockRangeProc */
int SceSysmemForDriver_A8525B06(SceUID pid, void *addr, SceSize size);
/* vitasdk: ksceKernelVARangeToPARange */
int SceSysmemForDriver_AE36C775(const SceKernelVARange *range, SceKernelPARange *out);
/* vitasdk: ksceKernelVARangeToPARangeByHW */
int SceSysmemForDriver_B3575090(const SceKernelVARange *range, SceKernelPARange *out);
/* vitasdk: ksceKernelAllocHeapMemoryWithOpt */
void *SceSysmemForDriver_B415B5A8(SceUID uid, SceSize size, void *opt, const void *caller);
/* vitasdk: ksceKernelStrnlenFromUser */
SceSize SceSysmemForDriver_B429D419(const char *str, SceSize max);
/* vitasdk: ksceKernelCountFillValue64FromUser */
SceSSize SceSysmemForDriver_BB3B02C2(const void *src, SceUInt64 value, SceSize size);
/* vitasdk: ksceKernelCopyFromUser */
int SceSysmemForDriver_BC996A7A(void *dst, const void *src, SceSize size);
/* vitasdk: ksceKernelCountFillValueFromUser */
SceSSize SceSysmemForDriver_BDA6E42B(const void *src, SceUInt32 value, SceSize size);
SceUID SceSysmemForDriver_BF209859(SceUID pid, SceUID guid);	/* vitasdk: kscePUIDOpenByGUID */
/* vitasdk: kscePUIDOpenByGUIDWithFlags */
SceUID SceSysmemForDriver_CED1547B(SceUID pid, SceUID guid, SceUInt32 flags);
/* vitasdk: ksceKernelStrncpyFromUser */
SceSSize SceSysmemForDriver_DB3EC244(char *dst, const char *src, SceSize n);
/* vitasdk: ksceKernelRemapMemBlock */
int SceSysmemForDriver_DFE2C8CB(SceUID guid, SceKernelMemBlockType type);
/* vitasdk: ksceKernelIsEqualAccessibleRangeProcBySW */
int SceSysmemForDriver_F4AD89D8(SceUID pid, SceUInt32 perm, const void *addr, SceSize size);
/* vitasdk: ksceGUIDReferObjectWithLevel */
int SceSysmemForDriver_F6DB54BA(SceUID guid, SceUInt32 level, SceObjectBase **object);
int SceSysmemForDriver_FB817A59(void *ptr);	/* vitasdk: ksceKernelFreeUncacheHeapMemory */
int SceSysmemForDriver_FFCD9B60(SceUID guid);	/* vitasdk: ksceKernelUnmapMemBlock */
int SceSysmemForKernel_01194C2E(void *pInfo);	/* vitasdk: ksceKernelNameHeapGetInfo */
void *SceSysmemForKernel_0164D817(void);
int SceSysmemForKernel_021053DD(SceUInt32 visibility, SceUID *vector, SceSize vectorSize, SceUInt32 *count);
SceUID SceSysmemForKernel_03A2CAF0(UIDEntry *entry);
int SceSysmemForKernel_05822B82(SceUInt32 type, SceSize *free);
PhyMemPart *SceSysmemForKernel_0B1B4ACA(void);
int SceSysmemForKernel_0B36F7F9(SceUID uid, SceUInt32 mode);
int SceSysmemForKernel_0B5A3E93(const void *va);
/* vitasdk: ksceGUIDGetObjectWithClass */
int SceSysmemForKernel_0E43E113(SceUID uid, SceClass *pClass, SceObjectBase **obj);
int SceSysmemForKernel_133024A5(SceUID guid);
int SceSysmemForKernel_17D3D4BD(ObjectHeap *heap, SceSize size, void *object);
SceUInt32 SceSysmemForKernel_17F12DCE(UIDEntry *entry);
/* vitasdk: ksceKernelAddressSpaceVAtoPA */
int SceSysmemForKernel_1BAF8C0B(void *pAS, SceUInt32 mode, void *pVA, void **pPA);
int SceSysmemForKernel_1DE4953F(SceUInt32 index, SceClass **cls);
int SceSysmemForKernel_20D01AFA(SceUID guid);
/* vitasdk: ksceKernelCopyToUserProcTextDomain */
int SceSysmemForKernel_2995558D(SceUID pid, void *dst, const void *src, SceSize size);
int SceSysmemForKernel_2C15615F(SceUID pid, SceUID puid, SceObjectBase **out);
int SceSysmemForKernel_2CFD8C9A(SceUInt32 type, uintptr_t pa, uintptr_t *va);
int SceSysmemForKernel_2F526AB8(ObjectHeap *heap);
int SceSysmemForKernel_320E907F(void);
FixedHeap *SceSysmemForKernel_3717C322(SceSize size);
void SceSysmemForKernel_3753C584(PhyMemPart *part);
int SceSysmemForKernel_40965FD1(const char *name, SceUInt32 flags, char **out);
int SceSysmemForKernel_4233C16D(void *ptr);	/* vitasdk: ksceKernelFree */
int SceSysmemForKernel_441E6BE9(SceUID uid, struct PhyPage *page);
int SceSysmemForKernel_47C55DCC(SceUInt32 index, void **part);
SceUInt32 SceSysmemForKernel_48D2E408(SceUInt32 type);
/* vitasdk: ksceKernelCopyToUserDomain */
int SceSysmemForKernel_499A9FFF(void *dst, const void *src, SceSize size);
int SceSysmemForKernel_4A2E421D(SceUID guid);	/* vitasdk: ksceKernelDeleteAddressSpace */
int SceSysmemForKernel_4E923101(SceUID guid);
/* vitasdk: ksceGUIDGetUIDVectorByClass */
int SceSysmemForKernel_52137FA3(SceClass *cls, int vis_level, SceUID *vector, SceSize num, SceSize *ret_num);
int SceSysmemForKernel_5B434019(PhyMemPart *part, SceSize psize);
SceClass *SceSysmemForKernel_5C0FC6BE(void);
int SceSysmemForKernel_5EDDEEB5(SceUID guid, SceClass *cls, const char *name, SceObjectBase *object, SceUInt32 attr);
/* vitasdk: ksceKernelCopyToUserTextDomain */
int SceSysmemForKernel_5EF1DAAF(void *dst, const void *src, SceSize size);
const char *SceSysmemForKernel_5FD15C6E(UIDEntry *entry);
PhyMemPart *SceSysmemForKernel_62800A6B(void);
int SceSysmemForKernel_660DC18A(ObjectHeap *heap, SceUID pid, SceUID heapUid);
int SceSysmemForKernel_66489B4B(SceUID pid, SceUInt32 level, SceUID *vector, SceSize size, SceSize *ret_num);
/* vitasdk: ksceGUIDKernelCreateWithAttr */
int SceSysmemForKernel_6D26B066(SceClass *pClass, const char *name, SceUInt32 attr, SceObjectBase **ppEntry);
SceInt32 SceSysmemForKernel_6D563733(PhyMemPart *part);
SceUID SceSysmemForKernel_6DB44A42(SceClass *cls, const char *name, void *object);
int SceSysmemForKernel_6DBDA03B(void *hook);
int SceSysmemForKernel_6E82D5A0(SceUID uid, SceUInt32 mode);
/* vitasdk: ksceKernelAddressSpaceUnmap */
int SceSysmemForKernel_73C38695(SceUID uid, int asid, int domain, void *addr, SceSize size);
ObjectHeap *SceSysmemForKernel_791D6205(void);
SceUInt16 SceSysmemForKernel_797C858B(UIDEntry *entry);
int SceSysmemForKernel_7CDB7655(SceClass *cls, const char *name, void *uidclass, SceSize itemsize, SceClassCallback create, SceClassCallback destroy);
/* vitasdk: ksceKernelAddressSpaceSetPhyMemPart */
int SceSysmemForKernel_7DF07D60(SceUID guid, SceUInt32 index, void *pPhyMemPart);
int SceSysmemForKernel_844DF3DA(SceUInt32 type, uintptr_t pa, SceSize size);
int SceSysmemForKernel_8450BFF7(void);
void *SceSysmemForKernel_85571907(SceSize size);	/* vitasdk: ksceKernelAlloc */
ObjectHeap *SceSysmemForKernel_857408DA(void);
/* vitasdk: ksceKernelReallocHeapMemory */
void *SceSysmemForKernel_8EE8B917(SceUID uid, void *ptr, SceSize size);
void *SceSysmemForKernel_8FD11E02(void);
int SceSysmemForKernel_93344755(FixedHeap *heap, void *object);
SceClass *SceSysmemForKernel_99D3BF76(void);
int SceSysmemForKernel_9F1A9AA6(const char *name, SceUInt32 flags, char **out);
int SceSysmemForKernel_A2DFA383(const char *name, SceUInt32 type, SceSize psize, PhyMemPart **out);
int SceSysmemForKernel_A4DB88E1(void *block);	/* vitasdk: ksceKernelFreeSimpleMemBlock */
void *SceSysmemForKernel_ABA5313C(FixedHeap *heap);
int SceSysmemForKernel_AD19285A(TinyPartition *tiny, uintptr_t lo, uintptr_t hi, SceUInt32 level, MemBlockInfoEx *info, SceSize infoSize, SceUInt32 *count);
int SceSysmemForKernel_BA441FD0(SceUID uid, MemBlockInfoEx *info);
int SceSysmemForKernel_BF275C5A(SceUID guid, SceClass *cls, const char *name, SceObjectBase *object);
int SceSysmemForKernel_BF70207B(SceUID uid, SceUInt32 level, SceUInt32 max, struct malloc_segment_range *list);
int SceSysmemForKernel_C3F9607E(SceUID pid, uintptr_t va, SceInt32 level, MemBlockPhyInfo *info);
void SceSysmemForKernel_CAB15F74(SceUID pid);
/* vitasdk: ksceKernelUIDEntryHeapGetInfo */
SceInt32 SceSysmemForKernel_CCD47B97(SceUID entryHeapId, void *pInfo);
void *SceSysmemForKernel_CF3C3F7A(SceSize size, SceAllocOpt *opt);
SceSize SceSysmemForKernel_D124075A(void *ptr);
/* vitasdk: ksceKernelGetMemBlockType */
int SceSysmemForKernel_D44FE44B(SceUID uid, unsigned int *type);
void SceSysmemForKernel_DCEF10B2(PhyMemPart *part);
int SceSysmemForKernel_E8CC2166(SceUID guid, SceUInt32 level);
int SceSysmemForKernel_E9957329(SceClass *cls, int level, SceUID *vector, SceSize num, SceSize *ret_num);
int SceSysmemForKernel_EA98CCE1(SceUInt32 type, SceSize size, SceSize alignment, uintptr_t *pa);
SceUInt32 SceSysmemForKernel_EB48682E(const char *key, SceSize len, SceUInt32 seed);
SceUID SceSysmemForKernel_EC90AA11(SceObjectBase *object);
SceUInt8 SceSysmemForKernel_F3DB2D71(UIDEntry *entry);
SceUID SceSysmemForKernel_F49C0BE8(const char *name);
int SceSysmemForKernel_FA1A73DD(SceUID guid, uintptr_t *base);
int SceSysmemForKernel_FCB5745A(SceUInt32 *dst, SceUInt32 value, SceSize count);
int SceSysmemForKernel_FF464A49(SceUID guid, UIDEntry **out);
SceUID SceSysmemForTZS_402EB970(const char *name, SceKernelMemBlockType type, SceSize size, SceKernelAllocMemBlockKernelOpt *opt);
int SceSysmemForTZS_E1DEDFF4(SceUID uid, void **base);
SceUID SceSysmemForKernel_89D33DF0(SceUID guid);	/* vitasdk: ksceGUIDOpenByGUID */
int sceGUIDClose(SceUID guid);
SceUID sceGUIDGetPID(SceUID guid);
SceUID sceGUIDKernelCreateWithOpt(SceClass *sce_class, const char *name, const UIDCreateOpt *opt,
				   SceObjectBase **obj);
int sceGUIDReleaseObject(SceUID guid);
int sceGUIDSetPID(SceUID guid, SceUID pid);
int sceKernelAddressSpaceFreeAllMemBlock(SceUID guid);
SceUID sceKernelAllocMemBlockForVM(const char *userName, SceSize size);
SceUID sceKernelAllocUnmapMemBlock(const char *userName, SceSize size);
int sceKernelCheckModelCapability(unsigned int capability);
int sceKernelCloseMemBlock(SceUID puid);
int sceKernelCloseVMDomain(void);
int sceKernelCopyFromToUserProc(SceUID pid, void *dst, const void *src, SceSize size);
int sceKernelCopyFromUserProc(SceUID pid, void *dst, const void *src, SceSize size);
int sceKernelCopyToUserProc(SceUID pid, void *dst, const void *src, SceSize size);
int sceKernelCopyToUserProcTextDomainForDBGP(SceUID pid, void *dst, const void *src, SceSize size);
SceSSize sceKernelCountFillValue64FromUserProc(SceUID pid, const void *src, SceUInt64 value, SceSize size);
SceUID sceKernelCreateAddressSpace(SceUID pid, const char *name, SceUInt32 type, void *pOpt);
int sceKernelCreateObjectHeap(SceUID pid, int kernel, SceKernelHeapHook *hook, ObjectHeap **out);
int sceKernelDeleteHeap(SceUID uid);
int sceKernelDeletePhyMemPart(PhyMemPart *part);
SceUID sceKernelFindMemBlockByAddr(const void *addr, SceSize size);
int sceKernelFreeHeapMemory(SceUID uid, void *ptr);
int sceKernelFreeMemBlock(SceUID puid);
int sceKernelFreeMemBlockForVM(SceUID puid);
int sceKernelGetFixedHeapInfoByPointer(void *fixedHeap, void *fixedHeapInfo);
int sceKernelGetHeapInfo(SceUID uid, SceUInt32 level, void *pInfo);
int sceKernelGetHeapInfoByPtr(void *object, void *pInfo);
int sceKernelGetMemBlockAllocMapSize(SceUID uid, SceSize *size);
int sceKernelGetMemBlockBase(SceUID puid, void **userBase);
SceUID sceKernelGetMemBlockProcess(SceUID guid);
int sceKernelGetModel(void);
int sceKernelGetModelForCDialog(void);
int sceKernelGetPhyMemPartInfoByID(SceUID uid, SceKernelPhyMemPartInfoForDebugger *info);
int sceKernelGetPhyMemPartInfoCore(PhyMemPart *part, SceKernelPhyMemPartInfoCore *info);
int sceKernelGetPhyMemPartInfoForDebugger(PhyMemPart *part, SceKernelPhyMemPartInfoForDebugger *info);
int sceKernelGrowPhyMemPartByPbase(PhyMemPart *part, uintptr_t pbase, SceSize psize);
int sceKernelIsPSVitaTV(void);
int sceKernelLockRangeWithMode(SceKernelMemoryRefPerm mode, void *addr, SceSize size);
void *sceKernelObjectHeapAlloc(ObjectHeap *heap, SceSize size);
int sceKernelObjectHeapSetResourceLimitAll(ObjectHeap *heap, unsigned int count, const SceUInt32 (*limits)[2]);
int sceKernelOpenVMDomain(void);
int sceKernelPrintHeapSegmentList(SceUID uid);
int sceKernelResetPhyMemPart(PhyMemPart *part);
int sceKernelShowPhyMemPart(PhyMemPart *part);
SceSSize sceKernelStrncpyFromUserProc(SceUID pid, char *dst, const char *src, SceSize n);
SceSSize sceKernelStrncpyToUserProc(SceUID pid, char *dst, const char *src, SceSize n);
SceSSize sceKernelStrnlenFromUserProc(SceUID pid, const char *str, SceSize max);
int sceKernelSysmemCleaner(void);
int sceKernelUserUnmap(SceUID guid, const void *userBuffer, SceSize userSize);
int sceKernelVARangeToPAVector(const SceKernelVARange *range, SceKernelPAVector *vector);
int sceKernelVARangeToPAVectorByHW(const SceKernelVARange *range, SceKernelPAVector *vector);
int sceKernelVARangeToPAVectorBySW(const SceKernelVARange *range, SceKernelPAVector *vector);
int sceKernelVAtoPABySW(const void *va, uintptr_t *pa);
int sceKernelVerifyHeap(SceUID uid, void *ptr);

#ifdef __cplusplus
}
#endif
