/*
 * SceSysmem: the structures and internal functions its source files share.
 *
 * Sony's file names (from the debug records) are kept: oheap.c, uid.c, ...
 * Internal functions without a debug-record name have descriptive names and
 * are marked "name unknown" where they are defined.
 */

#pragma once

#include <scetypes.h>
#include <sysmem/spinlock.h>
#include <fw.h>
#include "mmu.h"
#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include <sysmem/heap_info.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <threadmgr/threadcb.h>
#include <sysmem/sysclib.h>
#include <sysmem/address_space.h>
#include <kernel_boot_loader/kernel_boot_param.h>

/*
 * A function of one file that Sony's debug build leaves out of line and its release builds
 * fold into the callers (nothing is left of it there).
 */
#if FW_DEBUG
#define SYSMEM_DEBUG_FUNC
#define SYSMEM_DEBUG_STATIC		static __attribute__((noinline))
#define SYSMEM_DEBUG_LOCAL		static
#else
#define SYSMEM_DEBUG_FUNC		static inline
#define SYSMEM_DEBUG_STATIC		static inline
#define SYSMEM_DEBUG_LOCAL		static inline
#endif

#if FW_DEBUG
/*
 * The assertion-level call as an ordinary call: debug.h declares it pure (consecutive checks
 * share one call), which lets the compiler fold a condition over a store made just before it.
 * Where Sony's code reads the memory again after the call, the check goes through this name
 * for the same function.
 */
int SceDebugForDriver_374B7868_Impure(void) __asm__("SceDebugForDriver_374B7868");
#define SCE_DBG_ASSERT_CHECK_FUNC_RELOAD(_cond, _line, _func) do {		\
	if (SceDebugForDriver_374B7868_Impure() > 0)				\
		SceDebugForDriver_FD753E7A(1, (_cond), SCE_DBGINFO_FUNC(_line, _func),	\
				      __builtin_return_address(0));		\
} while (0)
#define SCE_DBG_ASSERT_CHECK_RELOAD(_cond, _line) do {				\
	if (SceDebugForDriver_374B7868_Impure() > 0)				\
		SceDebugForDriver_FD753E7A(1, (_cond), SCE_DBGINFO(_line),	\
				      __builtin_return_address(0));		\
} while (0)
#define SCE_DBG_ASSERT_FAIL_RELOAD(_line)	SCE_DBG_ASSERT_CHECK_RELOAD(0, _line)
#else
#define SCE_DBG_ASSERT_CHECK_FUNC_RELOAD(_cond, _line, _func)	do { } while (0)
#define SCE_DBG_ASSERT_CHECK_RELOAD(_cond, _line)		do { } while (0)
#define SCE_DBG_ASSERT_FAIL_RELOAD(_line)			do { } while (0)
#endif

/* memblock_obj_module.h */
typedef struct MemBlockObject MemBlockObject;
typedef struct TinyPartitionClientInfoList TinyPartitionClientInfoList;

/* sysmem_boot.h */
struct SysmemBootContext;

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

/* the name the converted files first used */
typedef Partition PartitionObject;

struct UIDManager;

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
int ObjectHeapSetKernelSizeClass(SceSize size, void *fixedHeap, SceUID uid);
void ObjectHeapStart(struct UIDManager *manager);

/* fixed_heap.c */
int FixedHeapInitForProcess(FixedHeap *heap, SceUID pid, const char *name, SceUInt32 attr,
			    SceSize itemSize, void *partition, SceSize pageSize,
			    SceUInt16 maxPages);
FixedHeapClass *FixedHeapGetClass(void);
#if SYSMEM_NSKBL
int FixedHeapInitClass(FixedHeapClass *cls, const char *name, SceSize itemsize, SceClass *parent);
struct SceKernelBootParam;
int FixedHeapBootInitialize(FixedHeap *heap, const char *name, SceUInt32 attr, SceUID partitionId,
			    SceSize itemSize, SceSize pageSize, SceUInt16 maxPages, void **pages,
			    void **blocks);
int FixedHeapStart(struct SceKernelBootParam *param, struct UIDManager *manager);
int FixedHeapStartPhase2(void);
int FixedHeapInitForUID(FixedHeap *heap, char *name, SceUInt32 attr, SceSize itemSize,
			SceSize pageSize, void *base, struct Partition *partition, SceUInt16 maxPages,
			void **pages, void **blocks);
int UIDStart(struct SceKernelBootParam *param, struct UIDManager *manager);
int UIDStartPhase2(void);
void *SysrootGetUIDPlaceholderObject(void);
void SysrootSetUIDPlaceholderObject(void *object);
void SysrootSetFallbackAddressSpaceCB(void *addressSpace);
int HeaplibStart(struct SceKernelBootParam *param, struct UIDManager *manager);
int HeaplibStartPhase2(struct SceKernelBootParam *param, struct UIDManager *manager);
#else
int FixedHeapInitClass(FixedHeapClass *cls, const char *name);
#endif
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
int SceSysmemForKernel_811C452B(FixedHeap *heap, FixedHeapObjectHook *hook);
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
	volatile SceUInt32 count;		/* 0x50: high-water counter, peak << 16 | count; atomics */
} NameHeapControl;

_Static_assert(sizeof(NameHeapControl) == 0x54, "NameHeapControl size");

int NameHeapInsert(const char *name, SceUInt32 flags, NameEntry **out);
int NameHeapLink(const char *name, SceUInt32 flags, NameEntry **out);
int NameHeapDelete(const char *name, SceUInt32 flags);
int NameHeapFind(const char *name, SceUInt32 flags, NameEntry **out);
int NameHeapStart(struct UIDManager *manager);
int AddressSpaceStart(struct SceKernelBootParam *pBoot, struct UIDManager *manager);
int NameHeapInit(struct UIDManager *manager);
int NameEntrySetUIDEntry(NameEntry *entry, void *uidEntry);

/* uid_heap.c */
int UIDHeapInit(UIDHeap *heap);
#if SYSMEM_NSKBL
int sceUIDHeapClassInit(UIDHeapClass *cls, const char *name, SceClass *parent,
			SceSize itemsize, SceClassCallback create, SceClassCallback destroy,
			int (*alloc)(void *heap, SceSize size, void **object),
			int (*free)(void *heap, void *object));
#else
int sceUIDHeapClassInit(UIDHeapClass *cls, const char *name, SceClassCallback create,
			SceClassCallback destroy,
			int (*alloc)(void *heap, SceSize size, void **object),
			int (*free)(void *heap, void *object));
#endif
int UIDHeapObjectCreate(void *object);
int UIDHeapObjectDestroy(void *object);
int UIDHeapCallAlloc(void *heap, SceSize size, void **object);
int UIDHeapCallFree(void *heap, void *object);

/* 0x81012970 (partition.c): the kernel partition (UIDManager +0x40) */
struct Partition *PartitionGetKernel(void);

#define ADDRESS_SPACE_TYPE_USER		0x10000001

/* a subbudget's size and free size (sceKernelGetSubbudgetInfo) */
typedef struct AddressSpaceSubbudgetInfo {
	SceSize size;
	SceSize psize;
	SceSize freeSize;
} AddressSpaceSubbudgetInfo;

int AddressSpaceObjectCreate(void *object);
int AddressSpaceObjectDestroy(void *object);
int UIDHeapClassInitManager(struct UIDManager *manager);
#if SYSMEM_NSKBL
int UIDClassStart(struct UIDManager *manager);
#else
int UIDClassModuleStart(struct UIDManager *manager);
#endif

/* sceKernelGetClassInfoAll */
typedef struct SceKernelClassInfo {
	SceSize size;				/* 0x10 */
	SceClass *cls;
	SceClass *parent;
	const char *name;
} SceKernelClassInfo;

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

/* exports used inside the module */

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
#define UID_ENTRY_ATTR_BOOT		0x01000000	/* registered at boot (UIDStart) */
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
	SceSize size;				/* 0x00: sizeof(UIDManager), set by the boot loader */
	SceClass *classes;			/* 0x04: the ring of every class */
	FixedHeap *guidEntryHeap;		/* 0x08: fixed heap of the GUID entries */
	struct PhyPageTableMgr *phyPageTableMgr;	/* 0x0c */
	struct PhyMemPart *pPhyMemPartKD;	/* 0x10 */
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
	SceUInt32 numMemory;			/* 0x4c: the boot param's memory ranges */
	SceKernelBootMemory memory[SCE_KERNEL_BOOT_MEMORY_MAX];	/* 0x50 */
	L1PageTable kernelL1PageTable;		/* 0x70: the boot loader's TTBR0/TTBR1 tables */
	struct PhyMemLow *phyMemLowMain;	/* 0x84 */
	struct PhyMemLow *phyMemLowSub;		/* 0x88 */
	struct PhyPage *phyPageReset;		/* 0x8c */
	struct PhyPage *phyPageVenezia;		/* 0x90 */
	struct PhyPage *phyPageVip;		/* 0x94 */
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
	void *rootPartitionObject;		/* 0xc4: the root partition's PartitionInit object */
	struct AddressSpaceContext *rootContext;	/* 0xc8: the root partition's context */
	SceUID uncacheHeap;			/* 0xcc: the uncached kernel heap */
	struct AddressSpaceContext *kernelContext;	/* 0xd0: the kernel address space's */
	struct AddressSpaceGlobals *addressSpaceGlobals;	/* 0xd4 */
	SceUInt32 processStorageKey;		/* 0xd8: sysmem's process-local storage */
} UIDManager;

_Static_assert(sizeof(UIDManager) == 0xdc, "UIDManager");

extern UIDManager *g_uidManager;

/* heap.c: the kernel heaps */
struct KernelHeap;
int HeapLock(struct KernelHeap *heap);
void HeapUnlock(struct KernelHeap *heap, int intr);
void *HeapAllocPrologue(struct KernelHeap *heap, SceSize size, SceSize alignment);
int HeapFree(struct KernelHeap *heap, void *ptr);
int HeapModuleStart(const UIDManager *manager);
int HeaplibImplementationStart(void);
int HeaplibImplementationStartCore0(UIDManager *manager);
int HeaplibImplementationStartPhase2(void);
int HeaplibImplementationStartPhase2Core0(UIDManager *manager);

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

/* address_space.c */
int AddressSpaceGetDefaultPhyMemPart(struct AddressSpaceObject *as, int kernel, void **part);
int AddressSpaceVARangeToPAVector(AddressSpaceObject *as, const SceKernelPARange *range,
				  SceKernelPAVector *vector, int *memoryType);
struct Partition *AddressSpaceGetPartitionByIndex(AddressSpaceObject *as, SceUInt32 index);
int AddressSpaceGetPhyMemPartByIndex(AddressSpaceObject *as, SceUInt32 index, void **part);
int AddressSpaceGetPhyMemPart(AddressSpaceObject *as, SceUInt32 code, void **part);
int AddressSpaceAllocMemBlockByCommand(AddressSpaceObject *unused,
				       SceKernelAllocMemBlockCommand *command);
int AddressSpaceSwitchContext(AddressSpaceObject *as);
int AddressSpaceMemset(AddressSpaceObject *as, void *dst, int c, SceSize size);
int AddressSpaceCheckAccessibleRange(AddressSpaceObject *as, uint32_t mode, uintptr_t va,
				     SceSize size);
int AddressSpaceVAtoPAWithMode(AddressSpaceObject *as, uint32_t mode, uintptr_t va, uintptr_t *pa);
int AddressSpaceVAtoPABySW(AddressSpaceObject *as, uintptr_t va, uintptr_t *pa);
int AddressSpaceGetMemoryType(AddressSpaceObject *as, uintptr_t va);
int AddressSpaceFindMemBlockCtlByAddr(AddressSpaceObject *as, uintptr_t va, SceSize size,
				      MemBlockObject **block);
SceUID AddressSpaceFindMemBlockCB(AddressSpaceObject *as, uintptr_t va, SceSize size,
				  MemBlockObject **out);
int AddressSpaceGetMemBlockInfoByAddr(AddressSpaceObject *as, uintptr_t va, SceInt32 level,
				      MemBlockPhyInfo *info);
int AddressSpaceGetSubbudgetInfo(AddressSpaceObject *as, SceUInt32 subbudget,
				 AddressSpaceSubbudgetInfo *out);
int AddressSpaceGetFreeMemorySize(AddressSpaceObject *as, SceKernelFreeMemorySizeInfo *out);
int AddressSpaceIdFreeMemBlock(SceUID guid, MemBlockBaseInfo *info, MemBlockObject **baseBlock,
			       void **baseInfo);
MMUContext *AddressSpaceGetMMUContext(AddressSpaceObject *as);
int AddressSpaceIsEqualAccessibleBySW(SceUID guid, SceUInt32 perm, uintptr_t va, uintptr_t *pa);
int AddressSpaceKernelReady(void);
int AsidUserStart(const UIDManager *manager);
void AddressSpaceKernelGameStart(void);

/* aimgr.c */
int AimgrInit(void);

typedef struct Bitheap {
	SceUInt32 magic;
	SceUInt32 count;			/* indices */
	SceUInt32 words;			/* per map */
	SceUInt32 used;
	SceUInt32 free;				/* set bits in freeMap */
	SceUInt32 freed;			/* set bits in freedMap */
	SceUInt32 *freeMap;
	SceUInt32 *freedMap;
} Bitheap;

/* bitheap.c */
int BitheapInit(Bitheap *heap, SceUInt32 count, SceUInt32 *buffer);
int BitheapAlloc(Bitheap *heap);
int sceKernelBitheapUse(Bitheap *heap, int index);
int sceKernelBitheapFree(Bitheap *heap, int index);
int BitheapModuleStart(void);

/* cache.c */
void CacheInvalidateRangeWithPA(const void *start, uintptr_t pa, SceSize size);

/* debug_state.c */
int InitializeDebugPrintLevel(const struct SysmemBootContext *context);
#if defined(SYSMEM_SKBL) || defined(SYSMEM_NSKBL)
int InitializeAssertLevel(void);	/* the boot loaders' own */
#else
int InitializeAssertLevel(const struct SysmemBootContext *context);
#endif
int InitializeDebugNop(void);
int InitializeDebugCorelock(void);

/* dipsw.c */
union DipswState;
int DipswSetState(union DipswState *dipsw);
#if defined(SYSMEM_SKBL) || defined(SYSMEM_NSKBL)
struct SceDipsw;
int InitializeDipsw(struct SceDipsw *dipsw);	/* the boot loaders' own */
#else
int InitializeDipsw(const struct SysmemBootContext *context);
#endif

/* l1pagetable.c */
void L1PageTableInit(L1PageTable *l1pt, uint32_t *low, uint32_t *high, uintptr_t split,
		     SceUInt32 reserved);
int L1PageTableMapDirect(L1PageTable *l1pt, SceUInt32 type, SceUInt32 domain, uintptr_t va,
			 SceSize size, uintptr_t pa);
int L1PageTableUnmapDirect(L1PageTable *l1pt, SceUInt32 asid, SceUInt32 domain, uintptr_t va,
			   SceSize size);
int L1PageTableMap(L1PageTable *l1pt, MemBlockObject *block, SceUInt32 type,
		   SceUInt32 domain, SceSize unit, uintptr_t va, SceSize size, uintptr_t pa);
int L1PageTableRemap(L1PageTable *l1pt, MemBlockObject *block, SceUInt32 type,
		     SceUInt32 domain, SceSize unit, uintptr_t va, SceSize size, uintptr_t pa,
		     SceUInt32 flags);
int L1PageTableSetL2PageTable(L1PageTable *l1pt, SceUInt32 domain, uintptr_t va, uintptr_t l2pa);
int L1PageTableUnsetL2PageTable(L1PageTable *l1pt, uintptr_t va);
int L1PageTableUnmap(L1PageTable *l1pt, MemBlockObject *block, SceSize unit,
		     SceUInt32 asid, SceUInt8 domain, uintptr_t va, SceSize size, SceUInt32 flags);
int L1PageTableVAtoPABySW(L1PageTable *l1pt, uintptr_t va, uintptr_t *pa);
int L1PageTableIsEqualAccessibleBySW(L1PageTable *l1pt, SceUInt32 perm, uintptr_t va,
				     uintptr_t *pa);
int L1PageTableGetMemoryType(L1PageTable *l1pt, uintptr_t va);
int L1PageTableModuleStart(void);
int L1PageTableStart(struct SceKernelBootParam *param, struct UIDManager *manager);
int L1PageTableStartFirst(void);
int PhyMemLowStart(struct UIDManager *manager);
int L2PageTableStart(struct SceKernelBootParam *param, struct UIDManager *manager);
int BlockStart(void);
int SimpleMemBlockStartPhase2(void);

/* l2cc.c */
void sceKernelL2CacheRemap(void);

/* l2pagetable.c */
int AllocL2PageTableObject(Partition *partition, FixedHeap *heap, SceUInt32 opt, L2PageTable **out);
int L2PageTableFree(FixedHeap *heap, L2PageTable *l2pt);
int L2PageTableInit(L2PageTable *l2pt);
int L2PageTableGetPageCount(L2PageTable *l2pt);
void L2PageTableUnreserve(L2PageTable *l2pt, SceSize size);
#if SYSMEM_NSKBL
int L2PageTableAddPageCount(L2PageTable *l2pt, SceSize size, SceSize extra);
int MemBlockAssocPhyPage(struct MemBlockObject *block, struct PhyPage *page);
int PartitionAllocMemBlockByAddrByCommand(struct Partition *partition,
					  SceKernelAllocMemBlockCommand *command, uintptr_t va);
int PartitionMapL2PageTable(struct Partition *partition, L2PageTable *l2pt, SceUInt32 flags);
int PartitionAllocSimpleMemBlockByAddr(struct Partition *partition, SceUInt32 smbType,
				       uintptr_t va, SceSize size, struct MemBlockObject **block);
int AllocPremappedBootSimpleMemBlock(const char *name, SceUInt32 smbType, SceUInt32 flags,
				     uintptr_t va, struct PhyPage *page,
				     struct MemBlockObject **pBlock);
int PartitionStart(struct SceKernelBootParam *pBoot, struct UIDManager *manager);
#endif
int L2PageTableMapPages(L2PageTable *l2pt, SceUInt32 type, uintptr_t va, SceSize size,
			uintptr_t pa);
int L2PageTableRemapPages(L2PageTable *l2pt, MemBlockObject *block, SceUInt32 type,
			  uintptr_t va, SceSize size, uintptr_t pa, SceUInt32 flags);
int L2PageTableUnmapPages(L2PageTable *l2pt, SceUInt32 asid, uintptr_t va, SceSize size,
			  SceUInt32 flags);
int L2PageTableModuleStart(void);

/* l2cc.c */
#if defined(SYSMEM_SKBL)
struct ScePl310Registers;
void CpuL2CacheInit(struct ScePl310Registers *registerBase);
#endif

/* led.c */
#if defined(SYSMEM_SKBL) || defined(SYSMEM_NSKBL)
int LedModuleStart(void);
#else
int LedModuleStart(const struct SysmemBootContext *context);
#endif

/* memblock.c */
int FreeMemBlockByGUID(SceUID guid);
int MemBlockGetInfoRaw(MemBlockObject *block, SceInt32 level, MemBlockInfoEx *info);
int MemBlockGetInfo(MemBlockObject *block, SceInt32 level, MemBlockPhyInfo *info);
int MemBlockWithinPhyRange(MemBlockObject *block, uintptr_t pa, SceSize size);
int MemBlockClearBaseInfo(MemBlockObject *block);
int MemBlockModuleStartAfterProcessmgr(void);
int MemBlockModuleStart(void);
int MemBlockOptSetClientInfoRaw(MemBlockObject *block, const SceKernelAllocMemBlockKernelOpt *opt);
SceUID MemBlockCreateGUID(const char *name, MemBlockObject *block, SceUInt32 attr);

/* memblock_db.c */
SceUInt32 MemBlockCode2PartitionIndex(SceUInt32 code);
const MemBlockCodeInfo *MemBlockCode2CodeInfo(SceUInt32 code);
int MemBlockType2TypeInfo(SceUInt32 type, const MemBlockTypeInfo **info);
int MemBlockType2Code(SceUInt32 type, SceUInt32 *code);
int MemBlockCode2TypeInfo(SceUInt32 code, const MemBlockTypeInfo **info);
int MemBlockCode2Type(SceUInt32 code, SceUInt32 *type);

/* memblock_partial.c */
int MemBlockPartialInfoCreate(MemBlockObject *block, SceKernelAllocMemBlockCommand *command);
int MemBlockPartialAllocPhyPage(MemBlockObject *block, SceKernelAllocMemBlockCommand *command);
int MemBlockPartialMapRaw(MemBlockObject *block, SceKernelAllocMemBlockCommand *command);
int MemBlockPartialUnmapRaw(MemBlockObject *block, PartitionObject *partition);
int MemBlockPartialFreeRaw(MemBlockObject *block, PartitionObject *partition);
int MemBlockPartialOpRange(PartitionObject *partition, MemBlockObject *block, SceUInt32 op,
			   uintptr_t va, SceSize size);
SceSize MemBlockPartialGetHeadFreeSize(MemBlockObject *block);
SceSize MemBlockPartialGetTailSize(MemBlockObject *block);
SceSize MemBlockPartialGetMappedSize(MemBlockObject *block);
SceSize MemBlockPartialGetAllocatedSize(MemBlockObject *block);

/* memblock_vpl.c */
SceSize MemBlockVirPageListGetHeadFreeSize(MemBlockObject *block);
SceSize MemBlockVirPageListGetTailFreeSize(MemBlockObject *block);
SceSize MemBlockVirPageListGetHeadType1Size(MemBlockObject *block);
SceSize MemBlockVirPageListGetTailType1Size(MemBlockObject *block);
SceSize MemBlockVirPageListGetMappedSize(MemBlockObject *block);
SceSize MemBlockVirPageListGetPhySize(MemBlockObject *block);
int MemBlockVirPageListCheckWithinPhyRange(MemBlockObject *block, uintptr_t pa, SceSize size);
int MemBlockVirPageListGetInfo(MemBlockObject *block, MemBlockPhyInfo *info);
int MemBlockVirPageListAllocPhyPage(MemBlockObject *block, PartitionObject *partition,
				    void *phyPart);
int MemBlockVirPageListMapRaw(MemBlockObject *block, const SceKernelAllocMemBlockCommand *command,
			      PartitionObject *partition, SceUInt32 flags);
int MemBlockVirPageListUnmapRaw(MemBlockObject *block, PartitionObject *partition);
int MemBlockVirPageListRemapRaw(MemBlockObject *block, PartitionObject *partition,
				SceUInt32 type, SceUInt32 flags);
int MemBlockVirPageListFreeVirPageCore(MemBlockObject *block, PartitionObject *partition);

/* partition.c */
int AddressSpaceContextAllocL2Tables(AddressSpaceContext *context);
int PartitionFindL2PageTable(Partition *partition, uintptr_t va, SceSize size, SceUInt32 flags,
			     L2PageTable **out);
int PartitionGetPhyMemPart(Partition *partition, SceUInt32 code, void **phyMemPart);
void PartitionVirPageRelease(Partition *partition);
int DeletePartition(Partition *partition);
int PartitionAllocVirPage(Partition *partition, VirPage **vp);
int PartitionReserveSection(Partition *partition, uintptr_t va);
int PartitionMapPages(Partition *partition, SceUInt32 code, uintptr_t va, SceSize size,
		      uintptr_t pa, SceUInt32 flags);
int PartitionRemapPages(Partition *partition, MemBlockObject *block, SceUInt32 type,
			uintptr_t va, SceSize size, uintptr_t pa, SceUInt32 flags);
int PartitionUnmapL2PageTable(Partition *partition, uintptr_t va, SceSize size, SceUInt32 mode,
			      SceUInt32 flags);
void PartitionFindAndTryRemoveL2PageTable(Partition *partition, uintptr_t va, SceSize size);
int PartitionGetMemoryType(Partition *partition, uintptr_t va);
/* the partitions' answer for an address outside them; otherwise they answer as the walks do */
#define PARTITION_WALK_NOT_IN		7	/* name unknown */
int PartitionVAtoPABySW(Partition *partition, uintptr_t va, uintptr_t *pa);
int PartitionIsEqualAccessibleBySW(Partition *partition, SceUInt32 perm, uintptr_t va,
				   uintptr_t *pa);
int PartitionGetMemBlockPARange(Partition *partition, MemBlockObject *block,
				SceKernelPARange *range);
int PartitionGetMemBlockPAVector(Partition *partition, MemBlockObject *block,
				 SceKernelPAVector *vector);
int PartitionAllocMemBlockByCommand(Partition *partition, SceKernelAllocMemBlockCommand *command);
int PartitionAllocMemBlockWithPhyMemPartExt(Partition *partition, void *phyMemPart,
					    const char *name, SceUInt32 code, SceSize size,
					    const SceKernelAllocMemBlockKernelOpt *opt,
					    MemBlockObject **block);
int PartitionAllocSimpleMemBlock(Partition *partition, SceUInt32 smbType, SceSize size,
				 MemBlockObject **block);
int PartitionAllocVSlot(Partition *partition, SceUInt32 code, SceSize size, MemBlockObject **slot);
int PartitionFreeMemBlockWithBase(Partition *partition, MemBlockObject *block,
				  MemBlockObject **baseBlock, void **baseInfo);
int PartitionFreeMemBlock(Partition *partition, MemBlockObject *block);
int AddressSpaceContextFreeL2Tables(AddressSpaceContext *context);
int TinyPartitionFreeAllMemBlock(Partition *partition, TinyPartitionClientInfoList *list);
int PartitionMapMemBlock(Partition *partition, MemBlockObject *block,
			 const SceKernelAllocMemBlockCommand *command, SceUInt32 flags);
int PartitionUnmapMemBlock(Partition *partition, MemBlockObject *block);
int PartitionRemapMemBlock(Partition *partition, MemBlockObject *block, SceUInt32 type,
			   SceUInt32 flags);
int TinyPartitionFindMemBlock(TinyPartition *tiny, uintptr_t va, SceSize size,
			      MemBlockObject **block);
int PartitionGetMemBlockInfoByAddr(TinyPartition *tiny, uintptr_t va, SceUInt32 arg, void *info);
int TinyPartitionGetMemBlockInfoListByPA(TinyPartition *tiny, uintptr_t lo, uintptr_t hi,
					 uintptr_t pa, SceSize psize, SceUInt32 arg,
					 MemBlockInfoEx *info, SceSize infoSize, SceUInt32 *count);
int TinyPartitionGetMemBlockInfoListByVA(TinyPartition *tiny, uintptr_t lo, uintptr_t hi,
					 SceUInt32 arg, MemBlockInfoEx *info, SceSize infoSize,
					 SceUInt32 *count);
int PartitionGetInfo(Partition *partition, PartitionInfo *info);
SceUID sceKernelCreatePartition(const char *name, SceUInt8 domain, uintptr_t vbase, SceSize size,
				Partition **out, const PartitionInitParam *param);
int MemBlockFreeFromPartition(MemBlockObject *block);
int PartitionModuleStart(const UIDManager *manager);
int CreateTinyPartitionRaw(void);

/* a page index (PhyPage.info's low bits) to its physical address */
#define PHYMEM_PAGE_SHIFT		12

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

/* the physical address of the page's first byte */
#define PhyPageGetAddress(_page)	((_page)->info << 12)

#define PHYMEM_BUDDY_ORDERS		9	/* 4 KiB .. 1 MiB */

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

/* a physical memory partition (phymem.c; uid.c gives the boot ones their GUIDs) */
struct PhyMemPart {
	SceObjectBase base;
	volatile SceInt32 refCount;	/* 0x08: atomics; PhyMemPartGetARCount reads the word */
	SceUInt32 flags;		/* 0x0c: bit0 = active */
	SceUID uid;			/* 0x10 */
	SceUInt32 type;			/* 0x14 */
	SceKernelSpinlock lock;		/* 0x18 */
	PhyMemPartCpuCache *cpuCache;	/* 0x1c */
	PhyPage *freeList;		/* 0x20: whole 1 MiB units, sorted */
	PhyPage freeListHead;		/* 0x24 */
	PhyMemPartBuddy buddy[PHYMEM_BUDDY_ORDERS];	/* 0x34 */
	struct PhyMemSegment *segments;	/* 0x7c */
	SceInt32 useCount;		/* 0x80: creators and openers; the last delete frees */
	volatile SceInt32 freeSize;	/* 0x84: Sony rereads it on every use */
	SceInt32 freeSizeMin;		/* 0x88 */
	char name[0x20];		/* 0x8c */
};

_Static_assert(sizeof(PhyMemPart) == 0xac, "PhyMemPart size");

typedef struct PhyMemLow {
	SceKernelSpinlock lock;
	uintptr_t base;
	SceSize size;
	SceSize unitSize;
	SceUInt32 unitShift;
	SceUInt32 unitMask;
	SceUInt32 wordCount;		/* bitmap words */
	SceUInt32 *bitmap;
} PhyMemLow;

/* phymem.c */
SceSize PhyPageGetSizeRaw(const PhyPage *page);
int PhyMemPartAllocPhyPage(PhyMemPart *part, SceSize size, SceUInt32 flags, PhyPage **page);
int PhyMemPartFreePhyPage(PhyMemPart *part, PhyPage *page);
SceSize PhyMemPartGetFreeSizeInternal(PhyMemPart *part);
int PhyMemPartCdramAlloc(PhyMemPart *part, SceSize size, SceSize alignment, uintptr_t *pa);
int PhyMemPartCdramFree(PhyMemPart *part, uintptr_t pa, SceSize size);
int PhyMemPartGetInfoCore(PhyMemPart *part, SceKernelPhyMemPartInfoCore *info);
const char *PhyMemPartGetName(PhyMemPart *part);
SceUInt32 PhyMemPartTestAttr(PhyMemPart *part, SceUInt32 mask);
int PhyMemModuleStart(const UIDManager *manager);
int PhyMemStart(struct SceKernelBootParam *param, UIDManager *manager);
int PhyMemModuleStartPhase2(const UIDManager *manager);
int PhyMemPartInitCdram(void);
int PhyMemCdramAlloc(SceSize size, SceSize alignment, uintptr_t *pa);
int PhyMemCdramFree(uintptr_t pa, SceSize size);

/* phymem_low.c */
int PhyMemLowAlloc(PhyMemLow *pml, SceSize size, SceSize alignment, SceUInt32 flags,
		   uintptr_t *pa, SceSize *allocated);
int PhyMemLowFree(PhyMemLow *pml, uintptr_t pa, SceSize size);
int PhyMemLowGetFreeSize(PhyMemLow *pml, SceSize *free);
int PhyMemLowMainAlloc(SceSize size, SceSize alignment, SceUInt32 flags, uintptr_t *pa,
		       SceSize *allocated);
int PhyMemLowMainMarkUsed(uintptr_t pa, SceSize size);
int PhyMemLowMainFree(uintptr_t pa, SceSize size);
int PhyMemLowCreate(uintptr_t base, SceSize size, PhyMemLow **out);
int PhyMemLowDestroy(PhyMemLow *pml);
int PhyMemLowStaticAlloc(SceSize size, SceSize alignment, uintptr_t *pa);
int PhyMemLowStaticMarkUsed(uintptr_t pa, SceSize size);
int PhyMemLowStaticFree(uintptr_t pa, SceSize size);
int PhyMemLowModuleStart(const UIDManager *manager);
int PhyMemLowModuleStartPhase2(const UIDManager *manager);
int PhyMemLowStaticInit(void);

/* procevent.c */
int sceKernelProcessEventInit(void);

/* serial.c */
int SerialDebugPutchar(void *args, int c);
#if defined(SYSMEM_SKBL) || defined(SYSMEM_NSKBL)
int SerialInit(int uart0Enabled);
#else
int SerialInit(const struct SysmemBootContext *context);
#endif

/* smb.c: simple memblocks */
int AllocSimpleMemBlock(const char *name, SceUInt32 smbType, void *phyMemPart, SceUInt32 flags,
			struct MemBlockObject **smb);
int SceSysmemForKernel_39D5A3B7(struct MemBlockObject *smb, void **base);	/* sceKernelGetSimpleMemBlockBase */
int FreeSimpleMemBlock(MemBlockObject *block, SceUInt32 flags);
int SimpleMemBlockGetPbase(MemBlockObject *block, uintptr_t *pbase);
int SimpleMemBlockModuleStart(const UIDManager *manager);
void SimpleMemBlockStart(UIDManager *manager);

/* sysmem.c: the kernel heap under its lock */
void *HeapAllocLocked(void *heap, SceSize size);
int HeapFreeLocked(void *heap, void *ptr);
void *UIDManagerGetKernelHeap(void) __attribute__((pure));
int PIDFreeMemBlock(MemBlockBaseInfo *info, MemBlockObject **baseBlock, void **baseInfo);

/* sysmem_heap.c */
char *SysmemAllocName(const char *name);
void SysmemFreeName(char *name);
int SysmemHeapModuleStart(const UIDManager *manager);

/* sysroot.c */
int SysrootPIDtoAddressSpaceCB(SceUID processId, void **addressSpaceCB);
void *SysrootGetProcessObjectHeap(SceUID pid, int kind);
void *sceKernelSysrootAlloc(SceSize size);
int SysrootModuleStart(const struct SysmemBootContext *context);
int SysrootIncProcessResource(SceUInt32 type);
int SysrootDecProcessResource(SceUInt32 type);
uint32_t SysrootGetBootTypeIndicator2(void);
int SysrootCreateProcessLocalStorage(const char *name, SceSize size);
void *SysrootGetProcessLocalStorageAddr(SceUInt32 key);
int SysrootGetProcessLocalStorageAddrForPid(SceUID pid, SceUInt32 key, void **addr, int create);
const char *SysrootGetMemBlockName(unsigned int index);
#if SYSMEM_NSKBL
struct SceKernelBootParam;
int SetupCore0(struct SceKernelBootParam *param);
#endif

/* sysroot_capability.c */
int CheckProcessCapabilityGranted(SceUID processId, unsigned int capability);
int CheckProcessDebugCapability(SceUID processId);

/* sysroot_time_low.c */
uint32_t SysrootGetTimeLow(void);

/* uid.c */
int GUIDUnregisterChecked(SceUID guid);
int GUIDSetCNOA(SceUID guid, void *cls, const char *name, void *object, SceUInt32 attr);
int UIDDLinkClassInit(struct UIDManager *manager);
int UIDDLinkStart(struct UIDManager *manager);
int UIDEntryHeapClassInit(struct UIDManager *manager);
void UIDClassSetCallbacks(SceClass *cls, const char *name, SceClassCallback create,
			  SceClassCallback destroy);
int sceUIDCallConstructor(SceClass *cls, SceObjectBase *object);
int GUIDRegister(SceClass *cls, const char *name, SceObjectBase *object, SceUInt32 attr);
int GUIDFindByName(const char *name);
int GUIDReleaseObject(SceUID guid);
int GUIDReferObjectWithClassExclusive(SceUID guid, SceClass *cls, SceObjectBase **object);
SceUID PUIDFindByGUID(SceUID pid, SceUID guid);
SceUID PUID2GUIDWithRefer(SceUID pid, SceUID puid, SceClass *cls);
int PUIDCloseWithFlags(SceUID pid, SceUID puid, SceUInt32 flags);
int PUIDReferObjectWithLevel(SceUID pid, SceUID puid, SceUInt32 level, SceObjectBase **out);

/* virpage.c */
int VirPageInit(VirPage *vp);
int VirPageAllocPhyPage(VirPage *vp, void *partition, void *phyPart, VirPage **out);
int VirPageGetMapRange(VirPage *vp, uintptr_t *pbase, SceSize *size);
int VirPageGetPhyBase(VirPage *vp, uintptr_t *pbase);
int VirPageFree(void *partition, FixedHeap *heap, VirPage *vp);
int VirPageCreate(void);
int VirPageStart(void);
int VirPageDestroy(void);
void AllocMemBlockCommandInit(SceKernelAllocMemBlockCommand *command, const char *name,
			      SceSize size);
void AllocMemBlockCommandInitWithType(SceKernelAllocMemBlockCommand *command, const char *name,
				      SceUInt32 type, SceSize size);
void AllocMemBlockCommandSetExtraLow(SceKernelAllocMemBlockCommand *command, SceUInt32 extraLow);
void AllocMemBlockCommandSetExtraHigh(SceKernelAllocMemBlockCommand *command, SceUInt32 extraHigh);
