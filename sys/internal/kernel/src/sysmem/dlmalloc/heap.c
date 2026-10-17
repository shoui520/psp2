/*
 * SceSysmem: kernel heaps (Sony's dlmalloc/heap.c). A heap is a UID object
 * around a dlmalloc mspace (dlmalloc_sce.patch) that grows by memblocks:
 * sceKernelDlMmap/DlMunmap are the mspace's system memory.
 */

#include "../sysmem_internal.h"
#define MEMBLOCK_OBJ_MODULE_FILE	"dlmalloc/../memblock_obj_module.h"
#include "../memblock_obj_module.h"
#include <sysmem/heap_info.h>

#undef __FILE_NAME__
#define __FILE_NAME__ "dlmalloc/heap.c"

typedef void *mspace;

mspace create_mspace(SceSize capacity, int locked, void *heap);
mspace create_mspace_with_base(void *base, SceSize capacity, int locked, void *heap);
SceSize destroy_mspace(mspace msp);
void *mspace_malloc(mspace msp, SceSize bytes);
void mspace_free(mspace msp, void *mem);
void *mspace_realloc(mspace msp, void *mem, SceSize newsize);
void *mspace_memalign(mspace msp, SceSize alignment, SceSize bytes);
SceSize mspace_release_unused_segment(mspace msp);
int mspace_malloc_stats(mspace msp);
SceSize mspace_footprint(mspace msp);
SceSize mspace_usable_size(void *mem);
int mspace_verify_all_chunk(mspace msp, const char *name, void *mem);

/* dlmalloc's numbers (mallinfo, malloc_stats) */
struct mallinfo {
	SceSize arena;
	SceSize ordblks;
	SceSize smblks;
	SceSize hblks;
	SceSize hblkhd;
	SceSize usmblks;
	SceSize fsmblks;
	SceSize uordblks;
	SceSize fordblks;
	SceSize keepcost;
};

struct malloc_stats {
	SceSize max_system_bytes;
	SceSize system_bytes;
	SceSize in_use_bytes;
};

struct mallinfo mspace_mallinfo(mspace msp);
void mspace_get_malloc_stats(mspace msp, struct malloc_stats *stats);
SceSize mspace_get_mmapped_segments(mspace msp, SceSize max, struct malloc_segment_range *list);


/* SceKernelHeapCreateOpt flags kept in the heap (HEAP_HAS_*: sysmem/sysmem.h) */
#define HEAP_VERIFY			0x00000010	/* every chunk checked around a free */
#define HEAP_ACTIVE			0x00000020
#define HEAP_OPT_INVALID		0x0000e0ec

/* the memory types whose new pages are invalidated first */
#define HEAP_CODE_UNCACHED_1		0x30608006	/* PLACEHOLDER: name */
#define HEAP_CODE_UNCACHED_2		0x30600806	/* PLACEHOLDER: name */

/* the memblocks of a heap that is not the kernel's (slots from index 2) */
typedef struct HeapBlockVector {
	SceUInt32 capacity;
	SceUInt32 count;
	MemBlockObject *block[];
} HeapBlockVector;

/* what the last DlMmap/DlMunmap of an AllocHeapMemoryWithOption did */
typedef struct HeapMmapResult {
	SceUInt32 kind;				/* 1 mapped, 2 unmapped */
	void *base;
	SceSize size;
} HeapMmapResult;

/* SceUIDKernelHeapClass (0x80, from the 0x80 fixed heap) */
typedef struct KernelHeap {
	mspace msp;				/* 0x00 */
	SceClass *cls;				/* 0x04 */
	SceUInt32 reserved0;
	SceKernelSpinlock lock;			/* 0x0c */
	SceUInt32 reserved1;
	SceUInt32 reserved2;			/* 0x14: zeroed at creation, never read */
	SceUInt32 flags;			/* 0x18: HEAP_* */
	SceUID pid;				/* 0x1c: -1 the kernel's */
	SceUInt32 code;				/* 0x20: memblock code of its blocks */
	SceSize maxSize;			/* 0x24 */
	SceSize size;				/* 0x28: of its blocks */
	SceSize peakSize;			/* 0x2c */
	SceUInt32 chunks;			/* 0x30 */
	SceUInt32 peakChunks;			/* 0x34 */
	SceSize used;				/* 0x38 */
	SceSize peakUsed;			/* 0x3c */
	SceSize maxChunk;			/* 0x40 */
	HeapMmapResult *mmapResult;		/* 0x44 */
	char *name;				/* 0x48: 0x20, from the 0x20 fixed heap */
	HeapBlockVector *blocks;		/* 0x4c */
	SceUInt8 reserved3[0x30];
} KernelHeap;

_Static_assert(sizeof(KernelHeap) == 0x80, "KernelHeap size");

/* name unknown: called on every allocation and free (SceSysmemForKernel_42834422; debug
   builds only); an allocation reports its caller and the allocating function by name hash */
typedef struct HeapHooks {
	void (*alloc)(SceUID uid, void *ptr, SceSize size, const void *caller, SceUInt32 func);
	void (*free)(SceUID uid, void *ptr);
} HeapHooks;

/* the function hashes the allocation hook gets (sha1(name)[0..3], as in the debug records) */
#define HEAP_FUNC_ALLOC_HEAP_MEMORY		0x3b956c63	/* sceKernelAllocHeapMemory */
#define HEAP_FUNC_ALLOC_HEAP_MEMORY_WITH_OPTION	0xdb747aeb	/* sceKernelAllocHeapMemoryWithOption */
#define HEAP_FUNC_ALLOC_HEAP_MEMORY_WITH_OPT	0x16fdddfd	/* PLACEHOLDER: name */
#define HEAP_FUNC_ALLOC				0x52e5216a	/* sceKernelAlloc */
#define HEAP_FUNC_ALLOC_WITH_OPT		0x65ff9138	/* sceKernelAllocWithOpt */
#define HEAP_FUNC_ALLOC_UNCACHE			0x010b9fc3	/* sceKernelAllocUncacheHeapMemory */
#define HEAP_FUNC_ALLOC_UNCACHE_WITH_OPTION	0xaa952bc9	/* sceKernelAllocUncacheHeapMemoryWithOption */
#define HEAP_FUNC_OBJECT_ALLOC			0xb2e40fe1	/* PLACEHOLDER: name */

SceUInt32 s_uncacheHeap = (SceUInt32)-1;	/* the uncached heap's UID */
static KernelHeap *s_kernelHeap;		/* UIDManager +0x44 */
static SceClass *s_heapClass;			/* "SceUIDKernelHeapClass" */
#if FW_DEBUG
static HeapHooks s_hooks;
#endif

int ksceKernelSpinlockLowLockCpuSuspendIntr(SceKernelSpinlock *lock);
void ksceKernelSpinlockLowUnlockCpuResumeIntr(SceKernelSpinlock *lock, int intr);
int ksceGUIDReferObjectWithClass(SceUID guid, SceClass *cls, SceObjectBase **object);
int ksceGUIDReferObjectWithLevel(SceUID guid, SceUInt32 level, SceObjectBase **object);
int ksceGUIDReleaseObject(SceUID guid);
int ksceGUIDClose(SceUID guid);
int ksceGUIDGetName(SceUID guid, const char **name);
int ksceGUIDGetUIDVectorByClass(SceClass *cls, int level, SceUID *vector, SceSize num,
				SceSize *count);
int sceUIDCallConstructor(SceClass *cls, SceObjectBase *object);
SceUID SceSysmemForKernel_6DB44A42(SceClass *cls, const char *name, void *object);
int SceSysmemForKernel_BF275C5A(SceUID guid, SceClass *cls, const char *name,
				SceObjectBase *object);
SceUID SceSysmemForKernel_EC90AA11(SceObjectBase *object);
int UIDHeapClassInit(UIDHeapClass *cls, const char *name, SceClassCallback create,
		     SceClassCallback destroy, int (*alloc)(void *heap, SceSize size, void **object),
		     int (*free)(void *heap, void *object));
int ksceKernelGetMemBlockAllocMapSize(SceUID uid, SceSize *size);
int MemBlockType2Code(SceUInt32 type, SceUInt32 *code);
SceSize MemBlockGetAllocMapSizeRaw(MemBlockObject *block);
uintptr_t MemBlockGetVBase(MemBlockObject *block);
SceSize MemBlockGetHeadFreeSize(MemBlockObject *block);
int MemBlockFreeFromPartition(MemBlockObject *block);
struct Partition *PartitionGetKernel(void);
int PartitionGetPhyMemPart(struct Partition *partition, SceUInt32 code, void **phyMemPart);
int PartitionAllocMemBlockWithPhyMemPartExt(struct Partition *partition, void *phyMemPart,
					    const char *name, SceUInt32 code, SceSize size,
					    const SceKernelAllocMemBlockKernelOpt *opt,
					    MemBlockObject **block);
int TinyPartitionFindMemBlock(TinyPartition *tiny, uintptr_t va, SceSize size,
			      MemBlockObject **block);
AddressSpaceObject *SceSysmemForKernel_2E5CC243(void);
struct Partition *AddressSpaceGetPartitionByIndex(AddressSpaceObject *as, SceUInt32 index);
int AddressSpaceGetPhyMemPartByIndex(AddressSpaceObject *as, SceUInt32 index, void **part);
void *SceSysmemForKernel_0164D817(void);
uint32_t ksceKernelSysrootGetStatus(void);
void *sceKernelSysrootAlloc(SceSize size);
char *strncpy(char *dst, const char *src, SceSize n);

/* name unknown: SceUIDKernelHeapClass's constructor */
static int HeapObjectCreate(SceObjectBase *object)
{
	KernelHeap *heap = (KernelHeap *)object;

	heap->lock = 0;
	heap->reserved2 = 0;
	heap->msp = NULL;
	heap->flags = 0;
	heap->size = 0;
	heap->peakSize = 0;
	heap->chunks = 0;
	heap->peakChunks = 0;
	heap->used = 0;
	heap->peakUsed = 0;
	heap->maxChunk = 0;
	heap->mmapResult = NULL;
	heap->name = NULL;
	heap->blocks = NULL;
	return 0;
}

/* name unknown: ksceKernelCreateHeap's funcUID, a new GUID for the heap */
static SceUID HeapCreateGUID(KernelHeap *heap, const char *name)
{
	return SceSysmemForKernel_6DB44A42(s_heapClass, name, heap);
}

/* name unknown: a block of the heap given back; returns its size */
SceSize HeapFreeBlock(KernelHeap *heap, MemBlockObject *block)
{
	SceSize size;
	int intr;

	intr = ksceKernelRWSpinlockLowReadLockCpuSuspendIntr(&block->lock);
	size = MemBlockGetAllocMapSizeRaw(block);
	heap->size -= size;
	ksceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&block->lock, intr);
	MemBlockFreeFromPartition(block);
	return size;
}

/* name unknown: the heap's memory ranges (its mmapped segments, then its other
   blocks), up to max of them; returns how many there are */
SceUInt32 HeapGetBlockList(KernelHeap *heap, SceUInt32 max,
				  struct malloc_segment_range *list)
{
	HeapBlockVector *blocks;
	struct malloc_segment_range *next;
	SceUInt32 segments, count, i, j;
	MemBlockObject *block;
	uintptr_t base;

	if (list == NULL) {
		blocks = heap->blocks;
		if (blocks == NULL)
			return 0;
		count = 0;
		for (i = 2; i < blocks->capacity; i++) {
			if (blocks->block[i - 2] != NULL)
				count++;
		}
		return count;
	}
	segments = mspace_get_mmapped_segments(heap->msp, max, list);
	blocks = heap->blocks;
	next = list + segments;
	if (blocks == NULL || blocks->capacity < 3)
		return segments;
	count = segments;
	for (i = 2; i < blocks->capacity; i++) {
		block = blocks->block[i - 2];
		if (block == NULL)
			continue;
		base = block->vbase;
		for (j = 0; j < segments; j++) {
			if (list[j].base == (void *)base)
				break;
		}
		if (j < segments)
			continue;
		if (count < max) {
			next->base = (void *)base;
			next->size = block->size;
			next++;
		}
		count++;
	}
	return count;
}

/* name unknown: the heap object cleared and given back, unless it was the caller's */
int FreeHeapObject(KernelHeap *heap)
{
	SceUInt32 flags = heap->flags;

	heap->lock = 0;
	heap->reserved2 = 0;
	heap->msp = NULL;
	heap->flags = 0;
	heap->size = 0;
	heap->peakSize = 0;
	heap->chunks = 0;
	heap->peakChunks = 0;
	heap->used = 0;
	heap->peakUsed = 0;
	heap->maxChunk = 0;
	heap->mmapResult = NULL;
	heap->name = NULL;
	heap->blocks = NULL;
	if (flags & HEAP_HAS_HEAPCB)
		return 0;
	return KernelFixedHeapFreeObject(sizeof(KernelHeap), heap);
}

static inline int AllocHeapObject(KernelHeap **heap)
{
	int ret = KernelFixedHeapAllocObject(sizeof(KernelHeap), (void **)heap);

	if (ret < 0)
		SCE_DBG_PRINTF_FUNC(0xb, 356, "AllocHeapObject",
				    "KernelFixedHeapAllocObject failed 0x%08x\n", ret);
	return ret;
}

static inline void InitHeapObject(KernelHeap *heap)
{
	int ret = sceUIDCallConstructor(s_heapClass, (SceObjectBase *)heap);

	if (ret < 0)
		SCE_DBG_PRINTF_FUNC(0xb, 339, "InitHeapObject",
				    "sceUIDCallConstructor failed 0x%08x\n", ret);
}

SceUID CreateHeap(const char *name, SceSize size, SceUID uid, const SceKernelHeapCreateOpt *opt,
		  SceUID (*funcUID)(KernelHeap *heap, const char *name))
{
	KernelHeap *heap = NULL;
	SceUInt32 code = 0x1060d006;
	SceUInt32 flags;
	SceSize maxSize;
	SceUID pid;
	void *base;
	SceSize baseSize;
	int ret;

	if (opt == NULL) {
		maxSize = (SceSize)-1;
		pid = -1;
		flags = 0;
	} else {
		if (opt->size != sizeof(*opt)) {
			SCE_DBG_PRINTF(0xb, 645, "pOpt->size=0x%08x\n", opt->size);
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
		}
		flags = opt->flags;
		if (flags & HEAP_OPT_INVALID) {
			SCE_DBG_PRINTF(0xb, 652, "pOpt->flags=0x%08x\n", flags);
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		}
		if (flags & HEAP_HAS_MAX_SIZE) {
			maxSize = opt->maxSize;
			if (maxSize < size) {
				SCE_DBG_PRINTF(0xb, 659, "pOpt->maxSize=0x%08x\n", maxSize);
				return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
			}
		} else {
			maxSize = (SceSize)-1;
		}
		if ((flags & (HEAP_HAS_MEMBLOCK | HEAP_HAS_MEMTYPE)) ==
		    (HEAP_HAS_MEMBLOCK | HEAP_HAS_MEMTYPE))
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		if (flags & HEAP_HAS_MEMTYPE) {
			ret = MemBlockType2Code(opt->memtype, &code);
			if (ret < 0) {
				SCE_DBG_PRINTF(0xb, 672, "MemBlockType2Code(0x%08x) failed 0x%08x\n",
					       opt->memtype, ret);
				return ret;
			}
		}
		pid = (flags & HEAP_HAS_PID) ? opt->pid : -1;
		if (flags & HEAP_HAS_HEAPCB) {
			heap = opt->heap;
			if (heap == NULL) {
				SCE_DBG_PRINTF(3, 686,
					       "WARNING: SCE_KERNEL_HEAP_HAS_HEAPCB with NULL heap\n");
				flags &= ~HEAP_HAS_HEAPCB;
			}
		}
	}
	if (heap == NULL) {
		ret = AllocHeapObject(&heap);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 695, "AllocHeapObject failed 0x%08x\n", ret);
			return ret;
		}
	}
	InitHeapObject(heap);
	heap->maxSize = maxSize;
	heap->pid = pid;
	heap->flags = flags;
	heap->code = code;
	ret = KernelFixedHeapAllocObject(0x20, (void **)&heap->name);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 713, "KernelFixedHeapAllocObject failed 0x%08x\n", ret);
		SCE_DBG_ASSERT_FAIL(714);
		FreeHeapObject(heap);
		return ret;
	}
	strncpy(heap->name, name, 0x20);
	if (heap->flags & HEAP_HAS_MEMBLOCK) {
		ret = ksceKernelGetMemBlockBase(opt->memBlock, &base);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 727, "sceKernelGetMemBlockBase failed 0x%08x\n", ret);
			SCE_DBG_ASSERT_FAIL(728);
			goto fail;
		}
		ret = ksceKernelGetMemBlockAllocMapSize(opt->memBlock, &baseSize);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 737, "sceKernelGetMemBlockAllocMapSize failed 0x%08x\n",
				       ret);
			SCE_DBG_ASSERT_FAIL(738);
			goto fail;
		}
		heap->msp = create_mspace_with_base(base, baseSize, 1, heap);
		if (heap->msp == NULL)
			SCE_DBG_PRINTF(0xb, 747, "create_mspace_with_base failed\n");
		heap->size = baseSize;
		if (heap->peakSize < baseSize)
			heap->peakSize = baseSize;
	} else {
		heap->msp = create_mspace(size, 1, heap);
		if (heap->msp == NULL)
			SCE_DBG_PRINTF(0xb, 753, "create_mspace failed\n");
	}
	if (heap->msp == NULL) {
		KernelFixedHeapFreeObject(0x20, heap->name);
		FreeHeapObject(heap);
		return SCE_KERNEL_ERROR_HEAPLIB_ERROR;
	}
	if (uid == -1) {
		uid = funcUID(heap, name);
		if (uid < 0) {
			SCE_DBG_PRINTF(0xb, 769, "funcUID failed 0x%08x\n", uid);
			KernelFixedHeapFreeObject(0x20, heap->name);
			FreeHeapObject(heap);
			return uid;
		}
	} else {
		ret = SceSysmemForKernel_BF275C5A(uid, s_heapClass, NULL, (SceObjectBase *)heap);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 782, "sceGUIDSet failed 0x%08x\n", ret);
			KernelFixedHeapFreeObject(0x20, heap->name);
			FreeHeapObject(heap);
			return ret;
		}
	}
	heap->flags |= HEAP_ACTIVE;
	return uid;

fail:
	KernelFixedHeapFreeObject(0x20, heap->name);
	FreeHeapObject(heap);
	return ret;
}

/* the heap behind a UID, with a reference */
static inline KernelHeap *GUIDReferHeap(SceUID uid)
{
	KernelHeap *heap = NULL;
	int ret;

	ret = ksceGUIDReferObjectWithClass(uid, s_heapClass, (SceObjectBase **)&heap);
	if (ret < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 281, "GUIDReferHeap",
				    "sceGUIDReferObjectWithClass failed 0x%08x\n", ret);
		return NULL;
	}
	if (heap == NULL && uid == SCE_KERNEL_HEAP_TOOL_ID)
		SCE_DBG_ASSERT_CHECK_FUNC(0, 285, "GUIDReferHeap");
	return heap;
}

/* name kept from the records */
static int HeapEnableDiableMode(SceUID uid, SceUInt32 mode, int enable)
{
	KernelHeap *heap;
	int intr;

	if (mode & ~HEAP_VERIFY)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	heap = GUIDReferHeap(uid);
	if (heap == NULL) {
		SCE_DBG_PRINTF(0xb, 995, "UIDtoHeap(0x%08x) failed\n", uid);
		return SCE_KERNEL_ERROR_ILLEGAL_HEAP_ID;
	}
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&heap->lock);
	if (enable)
		heap->flags |= mode;
	else
		heap->flags &= ~mode;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&heap->lock, intr);
	ksceGUIDReleaseObject(uid);
	return 0;
}

#pragma push_macro("__FILE_NAME__")
#undef __FILE_NAME__
#define __FILE_NAME__ "dlmalloc/heaplib_dlmalloc.h"

/* a chunk of size freed from the heap's accounting */
static inline void KernelHeapDecChunk(KernelHeap *heap, SceSize size)
{
	if (heap->chunks == 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0xb, 232, "nChunk=%d\n", heap->chunks);
		return;
	}
	heap->chunks--;
	heap->used -= size;
}

#pragma pop_macro("__FILE_NAME__")

/* a chunk freed; the heap is locked */
int HeapFree(KernelHeap *heap, void *ptr)
{
	SceSize size = 0;
	int ret;

	if (ptr != NULL)
		size = mspace_usable_size(ptr);
	if (heap->lock == 0) {
		SCE_DBG_PRINTF(0xb, 1148, "heap must been locked.(heap=%p)\n***\n", heap);
		SCE_DBG_ASSERT_FAIL(1149);
	}
	if (!(heap->flags & HEAP_ACTIVE)) {
		SCE_DBG_PRINTF(0xb, 1154, "heap is not active\n");
		ret = SCE_KERNEL_ERROR_HEAP_NOT_ACTIVE;
	} else {
		ret = 0;
		if (heap->flags & HEAP_VERIFY) {
			ret = mspace_verify_all_chunk(heap->msp, heap->name, ptr);
			SCE_DBG_ASSERT_CHECK(ret >= 0, 1159);
		}
		mspace_free(heap->msp, ptr);
		if (heap->flags & HEAP_VERIFY) {
			ret = mspace_verify_all_chunk(heap->msp, heap->name, NULL);
			SCE_DBG_ASSERT_CHECK(ret >= 0, 1166);
		}
	}
	if (ptr != NULL)
		KernelHeapDecChunk(heap, size);
	return ret;
}

/* name unknown: SceUIDKernelHeapClass's free (HeapFree inlined) */
__attribute__((flatten)) static int HeapObjectFree(void *object, void *ptr)
{
	KernelHeap *heap = object;
	int intr, ret;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&heap->lock);
	ret = HeapFree(heap, ptr);
#if FW_DEBUG
	if (s_hooks.free != NULL)
		s_hooks.free((SceUID)heap, ptr);
#endif
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&heap->lock, intr);
	return ret;
}

/* name unknown: hooks on every allocation and free; NULL removes them */
int SceSysmemForKernel_42834422(const HeapHooks *hooks)
{
#if FW_DEBUG
	if (hooks == NULL) {
		s_hooks.alloc = NULL;
		s_hooks.free = NULL;
		return 0;
	}
	s_hooks.alloc = hooks->alloc;
	s_hooks.free = hooks->free;
#endif
	return 0;
}

SceUID ksceKernelCreateHeap(const char *name, SceSize size, SceKernelHeapCreateOpt *opt)
{
	return CreateHeap(name, size, -1, opt, HeapCreateGUID);
}

int ksceKernelDeleteHeap(SceUID uid)
{
	int ret;

	do {
		ret = ksceGUIDClose(uid);
		if (ret < 0)
			SCE_DBG_PRINTF_FUNC(0xb, 867, "sceKernelDeleteHeap",
					    "sceGUIDClose failed 0x%08x\n", ret);
	} while (ret < 0);
	return ret;
}

int sceKernelPrintHeapSegmentList(SceUID uid)
{
	KernelHeap *heap;
	int ret;
#if FW_DEBUG
	HeapBlockVector *blocks;
	MemBlockObject **ppBlock;
	SceUInt32 i, capacity, count;
	uintptr_t base;
	SceSize size, headFreeSize;
	int intr;
#endif

	heap = GUIDReferHeap(uid);
	if (heap == NULL) {
		SCE_DBG_PRINTF(0xb, 923, "UIDtoHeap(0x%08x) failed\n", uid);
		return SCE_KERNEL_ERROR_ILLEGAL_HEAP_ID;
	}
	ret = mspace_malloc_stats(heap->msp);
#if FW_DEBUG
	blocks = heap->blocks;
	capacity = blocks->capacity;
	count = blocks->count;
	for (i = 2, ppBlock = blocks->block; i < capacity; i++, ppBlock++) {
		if (*ppBlock == NULL)
			continue;
		intr = ksceKernelRWSpinlockLowReadLockCpuSuspendIntr(&(*ppBlock)->lock);
		base = (*ppBlock)->vbase;
		headFreeSize = MemBlockGetHeadFreeSize(*ppBlock);
		size = MemBlockGetAllocMapSizeRaw(*ppBlock);
		SCE_DBG_PRINTF(0xb, 947, "ui=%02d/%2d/%02d,*ppBlock=%p/%p+0x%08x\n", i, count,
			       capacity, *ppBlock, base + headFreeSize, size);
		ksceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&(*ppBlock)->lock, intr);
	}
#endif
	ksceGUIDReleaseObject(uid);
	return ret;
}

int sceKernelShrinkHeap(SceUID uid)
{
	KernelHeap *heap;
	int intr, ret;

	heap = GUIDReferHeap(uid);
	if (heap == NULL) {
		SCE_DBG_PRINTF(0xb, 966, "UIDtoHeap(0x%08x) failed\n", uid);
		return SCE_KERNEL_ERROR_ILLEGAL_HEAP_ID;
	}
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&heap->lock);
	ret = mspace_release_unused_segment(heap->msp);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&heap->lock, intr);
	ksceGUIDReleaseObject(uid);
	return ret;
}

/* name unknown: sets heap mode bits (HEAP_VERIFY) */
int SceSysmemForKernel_0B36F7F9(SceUID uid, SceUInt32 mode)
{
	return HeapEnableDiableMode(uid, mode, 1);
}

/* name unknown: clears them */
int SceSysmemForKernel_6E82D5A0(SceUID uid, SceUInt32 mode)
{
	return HeapEnableDiableMode(uid, mode, 0);
}

/* name unknown: the heap's lock taken; returns the interrupt state */
int HeapLock(KernelHeap *heap)
{
	return ksceKernelSpinlockLowLockCpuSuspendIntr(&heap->lock);
}

/* name unknown */
void HeapUnlock(KernelHeap *heap, int intr)
{
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&heap->lock, intr);
}

/* a chunk allocated (aligned when alignment is not 0); the heap is locked */
void *HeapAllocPrologue(KernelHeap *heap, SceSize size, SceSize alignment)
{
	SceSize usable;
	void *ptr;
	int ret;

	if (heap->lock == 0) {
		SCE_DBG_PRINTF(0xb, 1050, "heap must been locked.(heap=%p)\n***\n", heap);
		SCE_DBG_ASSERT_FAIL(1051);
	}
	if (!(heap->flags & HEAP_ACTIVE)) {
		SCE_DBG_PRINTF(0xb, 1058, "heap is not active.\n");
		return NULL;
	}
	if (heap->flags & HEAP_VERIFY) {
		ret = mspace_verify_all_chunk(heap->msp, heap->name, NULL);
		SCE_DBG_ASSERT_CHECK(ret >= 0, 1063);
	}
	if (alignment == 0)
		ptr = mspace_malloc(heap->msp, size);
	else
		ptr = mspace_memalign(heap->msp, alignment, size);
	if (ptr != NULL) {
		usable = mspace_usable_size(ptr);
		heap->chunks++;
		if (heap->peakChunks < heap->chunks)
			heap->peakChunks = heap->chunks;
		heap->used += usable;
		if (heap->peakUsed < heap->used)
			heap->peakUsed = heap->used;
		if (heap->maxChunk < usable)
			heap->maxChunk = usable;
	}
	return ptr;
}

/* name unknown: SceUIDKernelHeapClass's allocation */
static int HeapObjectAlloc(void *object, SceSize size, void **ptr)
{
	KernelHeap *heap = object;
	void *mem;
	int intr;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&heap->lock);
	mem = HeapAllocPrologue(heap, size, 0);
#if FW_DEBUG
	if (mem != NULL && s_hooks.alloc != NULL)
		s_hooks.alloc((SceUID)heap, mem, size, __builtin_return_address(0),
			      HEAP_FUNC_OBJECT_ALLOC);
#endif
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&heap->lock, intr);
	*ptr = mem;
	return mem != NULL ? 0 : SCE_KERNEL_ERROR_HEAPLIB_ERROR;
}

/* name kept from the records: an allocation, reporting the memblock it mapped */
static void *HeapAllocHeapMemoryWithOption(KernelHeap *heap, SceSize size,
					   SceAllocOpt *opt)
{
	HeapMmapResult result;
	HeapMmapResult *last;
	SceSize align;
	void *ptr;
	int intr;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&heap->lock);
	if (opt == NULL) {
		ptr = HeapAllocPrologue(heap, size, 0);
	} else {
		align = opt->align;
		heap->mmapResult = &result;
		result.kind = 0;
		result.base = NULL;
		result.size = 0;
		ptr = HeapAllocPrologue(heap, size, align);
		last = heap->mmapResult;
		opt->mmapKind = last->kind;
		switch (last->kind) {
		case 0:
			opt->mmapBase = NULL;
			opt->mmapSize = 0;
			break;
		case 1:
		case 2:
			if (last->size == 0) {
				SCE_DBG_ASSERT_FAIL(1283);
			} else {
				opt->mmapBase = last->base;
				opt->mmapSize = last->size;
			}
			break;
		default:
			SCE_DBG_ASSERT_FAIL(1287);
			break;
		}
		heap->mmapResult = NULL;
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&heap->lock, intr);
	return ptr;
}

/* name kept from the records */
static void *sceKernelAllocHeapMemoryWithOptionCore(SceUID uid, SceSize size, SceAllocOpt *opt)
{
	KernelHeap *heap;
	void *ptr;

	if (opt != NULL && opt->size != sizeof(*opt)) {
		SCE_DBG_PRINTF(0xb, 1310, "0x%08x != 0x%08x(expect)\n", opt->size, sizeof(*opt));
		SCE_DBG_ASSERT_FAIL(1311);
		return NULL;
	}
	heap = GUIDReferHeap(uid);
	if (heap == NULL)
		return NULL;
	ptr = HeapAllocHeapMemoryWithOption(heap, size, opt);
	ksceGUIDReleaseObject(uid);
	return ptr;
}

/* the caller passes its own return address for the hook */
void *ksceKernelAllocHeapMemoryWithOpt(SceUID uid, SceSize size, void *opt, const void *caller)
{
	void *ptr = sceKernelAllocHeapMemoryWithOptionCore(uid, size, opt);

#if FW_DEBUG
	if (ptr != NULL && s_hooks.alloc != NULL)
		s_hooks.alloc(uid, ptr, size, caller, HEAP_FUNC_ALLOC_HEAP_MEMORY_WITH_OPT);
#endif
	return ptr;
}

void *ksceKernelAllocHeapMemoryWithOption(SceUID uid, SceSize size, SceAllocOpt *opt)
{
	void *ptr = sceKernelAllocHeapMemoryWithOptionCore(uid, size, opt);

#if FW_DEBUG
	if (ptr != NULL && s_hooks.alloc != NULL)
		s_hooks.alloc(uid, ptr, size, __builtin_return_address(0),
			      HEAP_FUNC_ALLOC_HEAP_MEMORY_WITH_OPTION);
#endif
	return ptr;
}

void *ksceKernelAllocHeapMemory(SceUID uid, SceSize size)
{
	void *ptr = sceKernelAllocHeapMemoryWithOptionCore(uid, size, NULL);

#if FW_DEBUG
	if (ptr != NULL && s_hooks.alloc != NULL)
		s_hooks.alloc(uid, ptr, size, __builtin_return_address(0),
			      HEAP_FUNC_ALLOC_HEAP_MEMORY);
#endif
	return ptr;
}

/* name kept from the records: a new block vector of capacity slots */
int AllocVectorBlock(KernelHeap *heap, SceUInt32 capacity)
{
	HeapBlockVector *blocks;
	MemBlockObject **ppBlock;
	SceUInt32 i;

	blocks = ksceKernelAllocHeapMemory(SCE_KERNEL_HEAP_ID, capacity * sizeof(void *));
	heap->blocks = blocks;
	if (blocks == NULL) {
		SCE_DBG_PRINTF(0xb, 421, "sceKernelAllocHeapMemory failed\n");
		return SCE_KERNEL_ERROR_ERROR;
	}
	blocks->capacity = capacity;
	ppBlock = blocks->block;
	for (i = 2; i < capacity; i++)
		*ppBlock++ = NULL;
	return 0;
}

/* sceKernelReallocHeapMemory (without the hooks or the heap lock) */
void *SceSysmemForKernel_8EE8B917(SceUID uid, void *ptr, SceSize size)
{
	KernelHeap *heap = GUIDReferHeap(uid);

	if (heap == NULL)
		return NULL;
	return mspace_realloc(heap->msp, ptr, size);
}

int ksceKernelFreeHeapMemory(SceUID uid, void *ptr)
{
	KernelHeap *heap;
	int intr, ret;

	heap = GUIDReferHeap(uid);
	if (heap == NULL) {
		SCE_DBG_PRINTF_FUNC(0xb, 1405, "sceKernelFreeHeapMemory",
				    "UIDtoHeap(0x%08x) failed\n", uid);
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1406, "sceKernelFreeHeapMemory");
		return SCE_KERNEL_ERROR_ILLEGAL_HEAP_ID;
	}
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&heap->lock);
	ret = HeapFree(heap, ptr);
#if FW_DEBUG
	if (ret == 0 && s_hooks.free != NULL)
		s_hooks.free(uid, ptr);
#endif
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&heap->lock, intr);
	ksceGUIDReleaseObject(uid);
	return ret;
}

/* reports the blocks still held, then frees them */
static inline void CheckBlock(KernelHeap *heap)
{
	HeapBlockVector *blocks = heap->blocks;
	MemBlockObject **ppBlock = blocks->block;
	SceUInt32 i, capacity;

	capacity = blocks->capacity;
	for (i = 2; i < capacity; i++) {
		if (*ppBlock++ != NULL) {
			SCE_DBG_PRINTF_FUNC(0xb, 600, "CheckBlock", "%02d/%02d: leak pBlock=%p\n", i,
					    capacity, ppBlock[-1]);
			HeapFreeBlock(heap, ppBlock[-1]);
		}
	}
}

/* name unknown: SceUIDKernelHeapClass's destructor */
static int HeapObjectDestroy(SceObjectBase *object)
{
	KernelHeap *heap = (KernelHeap *)object;
	int intr;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&heap->lock);
	destroy_mspace(heap->msp);
	if (heap->blocks != NULL) {
		CheckBlock(heap);
		ksceKernelFreeHeapMemory(SCE_KERNEL_HEAP_ID, heap->blocks);
		heap->blocks = NULL;
	}
	heap->flags &= ~HEAP_ACTIVE;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&heap->lock, intr);
	KernelFixedHeapFreeObject(0x20, heap->name);
	return FreeHeapObject(heap);
}

int sceKernelVerifyHeap(SceUID uid, void *ptr)
{
	KernelHeap *heap;
	int intr, ret;

	heap = GUIDReferHeap(uid);
	if (heap == NULL) {
		SCE_DBG_ASSERT_FAIL(1425);
		return SCE_KERNEL_ERROR_ILLEGAL_HEAP_ID;
	}
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&heap->lock);
	ret = mspace_verify_all_chunk(heap->msp, heap->name, ptr);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&heap->lock, intr);
	ksceGUIDReleaseObject(uid);
	return ret;
}

void *ksceKernelAlloc(SceSize size)
{
	void *ptr = HeapAllocHeapMemoryWithOption(s_kernelHeap, size, NULL);

#if FW_DEBUG
	if (ptr != NULL && s_hooks.alloc != NULL)
		s_hooks.alloc(SCE_KERNEL_HEAP_ID, ptr, size, __builtin_return_address(0),
			      HEAP_FUNC_ALLOC);
#endif
	return ptr;
}

/* sceKernelAllocWithOpt */
void *SceSysmemForKernel_CF3C3F7A(SceSize size, SceAllocOpt *opt)
{
	void *ptr = HeapAllocHeapMemoryWithOption(s_kernelHeap, size, opt);

#if FW_DEBUG
	if (ptr != NULL && s_hooks.alloc != NULL)
		s_hooks.alloc(SCE_KERNEL_HEAP_ID, ptr, size, __builtin_return_address(0),
			      HEAP_FUNC_ALLOC_WITH_OPT);
#endif
	return ptr;
}

int ksceKernelFree(void *ptr)
{
	KernelHeap *heap = s_kernelHeap;
	int intr, ret;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&heap->lock);
	ret = HeapFree(heap, ptr);
#if FW_DEBUG
	if (ret == 0 && s_hooks.free != NULL) {
		s_hooks.free(SCE_KERNEL_HEAP_ID, ptr);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&heap->lock, intr);
		return 0;
	}
#endif
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&heap->lock, intr);
	return ret;
}

/* a new block in the heap's vector (not for the kernel heap) */
static inline void AddBlock(KernelHeap *heap, MemBlockObject *block)
{
	HeapBlockVector *blocks = heap->blocks, *old;
	MemBlockObject **ppBlock;
	SceUInt32 capacity, i;
	int ret;

	if (blocks == NULL) {
		AllocVectorBlock(heap, 8);
		heap->blocks->count = 0;
		blocks = heap->blocks;
		capacity = blocks->capacity;
	} else {
		capacity = blocks->capacity;
		if (capacity <= blocks->count + 2) {
			old = blocks;
			ret = AllocVectorBlock(heap, capacity * 2);
			if (ret < 0) {
				SCE_DBG_PRINTF_FUNC(0xb, 482, "AddBlock",
						    "AllocVectorBlock(%p, 0x%08x) failed 0x%08x\n",
						    heap, capacity * 2, ret);
				return;
			}
			heap->blocks->count = old->count;
			for (i = 2; i < capacity; i++)
				heap->blocks->block[i - 2] = old->block[i - 2];
			ksceKernelFreeHeapMemory(SCE_KERNEL_HEAP_ID, old);
			blocks = heap->blocks;
			capacity = blocks->capacity;
		}
	}
	ppBlock = blocks->block;
	for (i = 2; i < capacity; i++, ppBlock++) {
		if (*ppBlock == NULL) {
			*ppBlock = block;
			goto added;
		}
	}
	SCE_DBG_PRINTF_FUNC(0xb, 507, "AddBlock", "Cannot add:size=%02d, count=%02d\n", capacity,
			    blocks->count);
added:
	heap->blocks->count++;
}

/* the heap grows by a new memblock (dlmalloc's mmap) */
void *sceKernelDlMmap(KernelHeap *heap, SceSize size)
{
	SceKernelAllocMemBlockKernelOpt opt;
	void *phyMemPart, *base;
	MemBlockObject *block;
	AddressSpaceObject *as;
	struct Partition *partition;
	SceSize mapSize, total;
	SceUInt32 code;
	char *name;
	int intr, ret;

	if (!(heap->flags & HEAP_AUTO_EXTEND) && heap->size != 0) {
		SCE_PRINTF_WITH_INFO(0, 1520, "Warning:\n");
		SCE_PRINTF_WITH_INFO(0, 1522,
				     "\tGrow request for pHeap %p[%32s] without AUTO_EXTEND option\n",
				     heap, heap->name);
		SCE_PRINTF_WITH_INFO(0, 1524, "\tCurrent page size: 0x%08x -> +0x%08x [request]\n",
				     heap->size, size);
		return (void *)-1;
	}
	if ((g_uidManager->heapDefault == NULL || heap == g_uidManager->heapDefault) &&
	    heap->size >= 0xff000)
		SCE_DBG_PRINTF(0xb, 1534, "Grow request for pHeap %p[%s], 0x%08x += 0x%08x\n", heap,
			       heap->name, heap->size, size);
	if (heap->size + size > heap->maxSize) {
		SCE_DBG_PRINTF(0xb, 1542,
			       "Warning:Heap[%s] size overflow 0x%08x+0x%08x > 0x%08x[max]\n",
			       heap->name, heap->size, size, heap->maxSize);
		return (void *)-1;
	}
	code = heap->code;
	if (heap->pid == -1) {
		as = SceSysmemForKernel_2E5CC243();
	} else {
		ret = SysrootPIDtoAddressSpaceCB(heap->pid, (void **)&as);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 1554, "GetAddressSpace failed 0x%08x\n", ret);
			return (void *)-1;
		}
	}
	if (as == NULL) {
		partition = PartitionGetKernel();
		phyMemPart = SceSysmemForKernel_0164D817();
	} else {
		partition = AddressSpaceGetPartitionByIndex(as, code >> 28);
		if (partition == NULL)
			partition = PartitionGetKernel();
		if (AddressSpaceGetPhyMemPartByIndex(as, (code >> 20) & 0xf, &phyMemPart) < 0) {
			ret = PartitionGetPhyMemPart(partition, heap->code, &phyMemPart);
			if (ret < 0) {
				SCE_DBG_PRINTF(0xb, 1570,
					       "PartitionGetPhyMemPart(%p[%s]) failed 0x%08x\n",
					       partition, partition->tiny.name, ret);
				return (void *)-1;
			}
		}
	}
	if (ksceKernelSysrootGetStatus() < 3) {
		name = sceKernelSysrootAlloc(0x20);
		strncpy(name, heap->name, 0x1f);
	} else {
		name = heap->name;
	}
	/* the rest of the options stays unset (as in the original) */
	opt.size = sizeof(opt);
	opt.attr = 0x80000000;
	ret = PartitionAllocMemBlockWithPhyMemPartExt(partition, phyMemPart, name, heap->code, size,
						      &opt, &block);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 1595, "PartitionAllocMemBlock(size 0x%08x) failed 0x%08x\n",
			       size, ret);
		SCE_DBG_PRINTF(0xb, 1596, "\tpHeap->name=[%s]\n", heap->name);
		return (void *)-1;
	}
	if (g_uidManager->heapDefault != NULL && heap != g_uidManager->heapDefault)
		AddBlock(heap, block);
	intr = ksceKernelRWSpinlockLowWriteLockCpuSuspendIntr(&block->lock);
#pragma push_macro("__FILE_NAME__")
#undef __FILE_NAME__
#define __FILE_NAME__ "dlmalloc/../memblock_obj_module.h"
	SCE_DBG_ASSERT_CHECK_FUNC(name != NULL, 653, "MemBlockSetNameRaw");
#pragma pop_macro("__FILE_NAME__")
	block->name = name;
	block->flags |= MEMBLOCK_FLAG_NAMED;
	mapSize = MemBlockGetAllocMapSizeRaw(block);
	base = (void *)block->vbase;
	ksceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&block->lock, intr);
	total = heap->size + mapSize;
	if (heap->peakSize < total)
		heap->peakSize = total;
	heap->size = total;
	if (heap->code == HEAP_CODE_UNCACHED_1 || heap->code == HEAP_CODE_UNCACHED_2)
		ksceKernelDcacheInvalidateRange(base, mapSize);
	if (heap->mmapResult != NULL) {
		heap->mmapResult->base = base;
		heap->mmapResult->size = mapSize;
		heap->mmapResult->kind = 1;
	}
	return base;
}

/* a block leaves the heap's vector */
static inline void RemoveBlock(KernelHeap *heap, MemBlockObject *block)
{
	HeapBlockVector *blocks = heap->blocks;
	MemBlockObject **slot = blocks != NULL ? &blocks->block[0] : NULL;
	SceUInt32 i, capacity;

	capacity = blocks->capacity;
	for (i = 2; i < capacity; i++, slot++) {
		if (*slot == block) {
			*slot = NULL;
			heap->blocks->count--;
			return;
		}
	}
	SCE_DBG_PRINTF_FUNC(0xb, 555, "RemoveBlock", "No such block %p\n", block);
}

/* the heap shrinks: its blocks over [base, base + size) are freed (dlmalloc's munmap) */
int sceKernelDlMunmap(KernelHeap *heap, void *base, SceSize size)
{
	MemBlockObject *block;
	uintptr_t va = (uintptr_t)base;
	SceSize rest = size, done;
	int ret;

	while (rest != 0) {
		ret = TinyPartitionFindMemBlock(&PartitionGetKernel()->tiny, va, 0, &block);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 1653, "Heap->name=[%s]\n", heap->name);
			SCE_DBG_PRINTF(0xb, 1655,
				       "PartitionFindMemBlockByAddr(0x%08x) failed, res=0x%08x\n", va,
				       ret);
			SCE_DBG_PRINTF(0xb, 1656, "sizeRest=0x%08x, size=0x%08x\n", rest, size);
			SCE_PRINTF_WITH_INFO(0, 1659, "unmap 0x%08x failed\n", va);
			SCE_DBG_ASSERT_FAIL(1665);
			done = 0x1000;
		} else {
			if ((uintptr_t)base != MemBlockGetVBase(block)) {
				SCE_PRINTF_WITH_INFO(0, 1669, "ptr %p!=%p failed->DUMMY\n", base,
						     (void *)block->vbase);
				return -1;
			}
			done = HeapFreeBlock(heap, block);
			if (g_uidManager->heapDefault != NULL && heap != g_uidManager->heapDefault)
				RemoveBlock(heap, block);
			if (done >= rest)
				done = rest;
		}
		rest -= done;
		va += done;
	}
	if (heap->mmapResult != NULL) {
		heap->mmapResult->kind = 2;
		heap->mmapResult->base = base;
		heap->mmapResult->size = size;
	}
	return 0;
}

/* name unknown: dlmalloc's direct mmap, the same as its mmap */
void *sceKernelDlDirectMmap(KernelHeap *heap, SceSize size)
{
	return sceKernelDlMmap(heap, size);
}

int ksceKernelGetHeapInfoByPtr(void *object, void *pInfo)
{
	KernelHeap *heap = object;
	HeapInfo *info = pInfo;
	struct malloc_stats stats;
	struct mallinfo mi;
	SceUID uid;
	int intr;
	SceUInt32 ret;

	if (info == NULL) {
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		goto out;
	}
	if (info->size != sizeof(*info)) {
		SCE_DBG_PRINTF_FUNC(0xb, 1877, "sceKernelGetHeapInfoByPtr",
				    "pInfo size mismatch: 0x%08x!=0x%08x[expect]\n",
				    info->size, sizeof(*info));
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
		goto out;
	}
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&heap->lock);
	uid = SceSysmemForKernel_EC90AA11((SceObjectBase *)heap);
	if (uid < 0)
		info->uid = 0;
	else
		info->uid = uid;
	info->name = heap->name;
	info->totalSize = heap->size;
	info->peakTotalSize = heap->peakSize;
	info->maxSize = heap->maxSize;
	info->footprint = mspace_footprint(heap->msp);
	mi = mspace_mallinfo(heap->msp);
	info->arena = mi.arena;
	info->ordblks = mi.ordblks;
	info->hblkhd = mi.hblkhd;
	info->usmblks = mi.usmblks;
	info->uordblks = mi.uordblks;
	info->fordblks = mi.fordblks;
	info->keepcost = mi.keepcost;
	info->stateSize = mspace_usable_size(heap->msp);
	mspace_get_malloc_stats(heap->msp, &stats);
	info->maxSystemBytes = stats.max_system_bytes;
	info->systemBytes = stats.system_bytes;
	info->inUseBytes = stats.in_use_bytes;
	info->chunks = heap->chunks;
	info->peakChunks = heap->peakChunks;
	info->used = heap->used;
	info->peakUsed = heap->peakUsed;
	info->maxChunk = heap->maxChunk;
	if (heap->flags & HEAP_HAS_MEMBLOCK) {
		if (info->maxBlocks == 0 || info->blockList == NULL) {
			info->blockCount = 0;
		} else {
			info->blockCount = 1;
			info->blockList->base = (char *)heap->msp - 8;
			info->blockList->size = heap->size;
		}
	} else {
		info->blockCount = HeapGetBlockList(heap, info->maxBlocks, info->blockList);
	}
	info->flags = heap->flags;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&heap->lock, intr);
	ret = 0;
out:
	return ret;
}

int ksceKernelGetHeapInfo(SceUID uid, SceUInt32 level, void *pInfo)
{
	SceObjectBase *heap;
	const char *name;
	SceUInt32 ret;

	ret = ksceGUIDReferObjectWithLevel(uid, level, &heap);
	if ((SceInt32)ret < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 1895, "sceKernelGetHeapInfo",
				    "sceGUIDReferObjectWithLevel failed 0x%08x\n", ret);
		return ret;
	}
	ret = ksceKernelGetHeapInfoByPtr(heap, pInfo);
	if ((SceInt32)ret < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 1900, "sceKernelGetHeapInfo",
				    "sceKernelGetHeapInfoByPtr failed 0x%08x\n", ret);
	} else {
		ret = ksceGUIDGetName(uid, &name);
		if ((SceInt32)ret < 0)
			SCE_DBG_PRINTF_FUNC(0xb, 1905, "sceKernelGetHeapInfo",
					    "GUIDGetName failed 0x%08x\n", ret);
		else
			((HeapInfo *)pInfo)->name = name;
	}
	ksceGUIDReleaseObject(uid);
	return ret;
}

/* name unknown: the heap's memory ranges, up to max of them; returns how many */
int SceSysmemForKernel_BF70207B(SceUID uid, SceUInt32 level, SceUInt32 max,
				struct malloc_segment_range *list)
{
	KernelHeap *heap;
	int intr, ret;

	if (max != 0 && list == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = ksceGUIDReferObjectWithLevel(uid, level, (SceObjectBase **)&heap);
	if (ret < 0)
		return ret;
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&heap->lock);
	if (heap->flags & HEAP_HAS_MEMBLOCK) {
		ret = 1;
		if (list != NULL) {
			list->base = (char *)heap->msp - 8;
			list->size = heap->size;
		}
	} else {
		ret = HeapGetBlockList(heap, max, list);
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&heap->lock, intr);
	ksceGUIDReleaseObject(uid);
	return ret;
}

/* name unknown: the info of every heap visible at level, as many as fit in size bytes */
int SceSysmemForKernel_1380E144(SceUInt32 level, HeapInfo *infos, SceSize size, SceSize *count)
{
	SceUID *uids;
	SceSize n = 0;
	int i, ret, max;

	if (count == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	*count = 0;
	ret = ksceGUIDGetUIDVectorByClass(s_heapClass, level, NULL, 0, &n);
	if (ret < 0)
		return ret;
	max = size / sizeof(*infos);
	if (ret < max)
		max = ret;
	if (max != 0) {
		uids = ksceKernelAllocHeapMemory(SCE_KERNEL_HEAP_ID, max * sizeof(SceUID));
		if (uids == NULL)
			return SCE_ERROR_ERRNO_ENOMEM;
		ksceGUIDGetUIDVectorByClass(s_heapClass, level, uids, max, &n);
		for (i = 0; i < (int)n; i++) {
			infos->size = sizeof(*infos);
			ret = ksceKernelGetHeapInfo(uids[i], level, infos);
			if (ret < 0) {
				ksceKernelFreeHeapMemory(SCE_KERNEL_HEAP_ID, uids);
				return ret;
			}
			infos++;
		}
		ksceKernelFreeHeapMemory(SCE_KERNEL_HEAP_ID, uids);
	}
	*count = n;
	return max;
}

void *ksceKernelAllocUncacheHeapMemory(SceSize size)
{
	void *ptr = sceKernelAllocHeapMemoryWithOptionCore(s_uncacheHeap, size, NULL);

#if FW_DEBUG
	if (ptr != NULL && s_hooks.alloc != NULL)
		s_hooks.alloc(s_uncacheHeap, ptr, size, __builtin_return_address(0),
			      HEAP_FUNC_ALLOC_UNCACHE);
#endif
	return ptr;
}

void *ksceKernelAllocUncacheHeapMemoryWithOption(SceSize size, SceAllocOpt *opt)
{
	void *ptr = sceKernelAllocHeapMemoryWithOptionCore(s_uncacheHeap, size, opt);

#if FW_DEBUG
	if (ptr != NULL && s_hooks.alloc != NULL)
		s_hooks.alloc(s_uncacheHeap, ptr, size, __builtin_return_address(0),
			      HEAP_FUNC_ALLOC_UNCACHE_WITH_OPTION);
#endif
	return ptr;
}

int ksceKernelFreeUncacheHeapMemory(void *ptr)
{
	return ksceKernelFreeHeapMemory(s_uncacheHeap, ptr);
}

/* name unknown: the usable size of a heap chunk */
SceSize SceSysmemForKernel_D124075A(void *ptr)
{
	return mspace_usable_size(ptr);
}

/* name unknown: SceUIDKernelHeapClass and the kernel's two heaps */
int HeapModuleStart(const UIDManager *manager)
{
	s_heapClass = manager->heapClass;
	UIDHeapClassInit((UIDHeapClass *)s_heapClass, "SceUIDKernelHeapClass",
			 (SceClassCallback)HeapObjectCreate, (SceClassCallback)HeapObjectDestroy,
			 HeapObjectAlloc, HeapObjectFree);
	s_kernelHeap = manager->heapDefault;
	s_uncacheHeap = manager->uncacheHeap;
	return 0;
}
