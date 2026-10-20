/*
 * SceSysmem: memblock objects. Only the class accessor is converted so far;
 * the rest of the file is still in sysmem.c's stubs.
 */

#undef __FILE_NAME__
#define __FILE_NAME__ "memblock_obj_module.h"
#define MEMBLOCK_OBJ_MODULE_EXTERN
#include <sysmem/cp15.h>
#include "memblock_obj_module.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "memblock_obj.c"

/* SceUIDMemBlockClass */
static SceClass *s_memBlockClass;

SceClass *SceSysmemForKernel_86681B64(void)
{
	return s_memBlockClass;
}

/* memblock_db.c */

/* name unknown: the block's base address */
__attribute__((noinline))
uintptr_t MemBlockGetVBase(MemBlockObject *block)
{
	int flags = SceCpuForDriver_EC53D007(&block->lock);
	uintptr_t vbase = block->vbase;

	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&block->lock, flags);
	return vbase;
}

/* name unknown: GUIDApplyWithClass callback, the base into *argument */
int MemBlockGetVBaseCB(SceObjectBase *object, void *argument)
{
	*(uintptr_t *)argument = MemBlockGetVBase((MemBlockObject *)object);
	return 0;
}

/* name unknown: the block's code */
__attribute__((noinline))
SceUInt32 MemBlockGetCode(MemBlockObject *block)
{
	int flags = SceCpuForDriver_EC53D007(&block->lock);
	SceUInt32 code = block->code;

	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&block->lock, flags);
	return code;
}

/* name unknown: GUIDApplyWithClass callback, the block's type into *argument */
int MemBlockGetTypeCB(SceObjectBase *object, void *argument)
{
	return MemBlockCode2Type(MemBlockGetCode((MemBlockObject *)object),
				(SceUInt32 *)argument);
}

/* name unknown: the block's partition */
struct Partition *GetMemBlockPartition(MemBlockObject *block)
{
	int flags = SceCpuForDriver_EC53D007(&block->lock);
	void *partition = block->partition;

	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&block->lock, flags);
	return partition;
}

int sceKernelGetMemBlockVBase(SceUID uid, void **base)
{
	GUIDApplyArgs args;

	if (base == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	args.result = 0;
	args.argument = base;
	return GUIDApplyWithClass(uid, s_memBlockClass, MemBlockGetVBaseCB, &args);
}

int SceSysmemForKernel_D44FE44B(SceUID uid, unsigned int *type)
{
	GUIDApplyArgs args;

	if (type == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	args.result = 0;
	args.argument = type;
	return GUIDApplyWithClass(uid, s_memBlockClass, MemBlockGetTypeCB, &args);
}

#undef __FILE_NAME__
#define __FILE_NAME__ "memblock_obj_module.h"

/* name unknown */
const char *MemBlockGetName(MemBlockObject *block)
{
	if (block->guid != 0)
		return SceSysmemForDriver_E655852F(block->guid);
	if (!(block->flags & MEMBLOCK_FLAG_NAMED))
		return "SceNULL";
	SCE_DBG_ASSERT_CHECK_FUNC(1, 316, "MemBlockGetNameRaw");
	return block->name;
}

#undef __FILE_NAME__
#define __FILE_NAME__ "memblock_obj.c"

/* name unknown: clears the block's fields after the object header */
void MemBlockClearRaw(MemBlockObject *block)
{
	block->prev = NULL;
	block->flags = 0;
	block->code = 0;
	block->attr = 0;
	block->vbase = 0;
	block->size = 0;
	block->lock = 0;
	block->partition = NULL;
	block->phyMemPart = NULL;
	block->name = NULL;
}

/* name unknown: a fresh block (state 1 = free) */
void MemBlockInitRaw(MemBlockObject *block)
{
	block->code = 0;
	block->guid = -1;
	block->attr = 0;
	block->partition = NULL;
	block->virPages = NULL;
	block->flags = (block->flags & 0xff80ff60) | 1;
	block->mapRef = 0;
	block->lockRef = 0;
	block->opt = 0;
	block->name = NULL;
}

/* the block's physical memory partition; the kernel's when none is set */
void *MemBlockGetPhyMemPartRaw(MemBlockObject *block)
{
	if (block->phyMemPart == NULL && ((block->code << 8) >> 28) != 1) {
		SCE_DBG_ASSERT_FAIL(358);
		return SceSysmemForKernel_0164D817();
	}
	return block->phyMemPart;
}

/* name unknown */
void MemBlockSetPhyMemPartRaw(MemBlockObject *block, void *phyMemPart)
{
	block->phyMemPart = phyMemPart;
}

/* name unknown: SceUIDMemBlockClass's constructor */
int MemBlockObjectCreate(void *object)
{
	MemBlockObject *block = object;

	block->next = block;
	block->prev = block;
	block->guid = -1;
	block->code = 0;
	block->attr = 0;
	block->vbase = 0;
	block->size = 0;
	block->lock = 0;
	block->partition = NULL;
	block->phyMemPart = NULL;
	block->name = NULL;
	block->virPages = NULL;
	block->flags = 1;
	MemBlockSetFreeCodeRaw(block);
	block->code = 0;
	block->attr = 0;
	block->vbase = 0;
	block->size = 0;
	block->partition = NULL;
	block->virPages = NULL;
	block->mapRef = 0;
	block->lockRef = 0;
	block->opt = 0;
	block->name = NULL;
	block->guid = -1;
	return 0;
}

/* name unknown: SceUIDMemBlockClass's destructor */
int MemBlockObjectDestroy(void *object)
{
	MemBlockObject *block = object;
	void *info = MemBlockOptGetUserMapInfoRaw(block);

	if (info != NULL)
		SceSysmemForKernel_4233C16D(info);
	block->prev = NULL;
	block->flags = 0;
	block->code = 0;
	block->attr = 0;
	block->vbase = 0;
	block->size = 0;
	block->lock = 0;
	block->partition = NULL;
	block->phyMemPart = NULL;
	block->name = NULL;
	block->virPages = NULL;
	block->guid = -1;
	return 0;
}

/* memblock_partial.c, memblock_vpl.c, virpage.c */
/* phymem.c, mmu.c, address_space.c, smb.c, fixed_heap.c */
extern struct PhyMemPart *s_phyPartGame;

/* bits 20..22: 4 = carries base info in opt */

/* the bytes with memory behind them */
SceSize MemBlockGetAllocMapSizeRaw(MemBlockObject *block)
{
	if (block->code == 0 || (block->attr & MEMBLOCK_ATTR_WHOLE))
		return block->size;
	switch (block->flags & MEMBLOCK_KIND_MASK) {
	case MEMBLOCK_KIND_PARTIAL:
		SCE_DBG_PRINTF(0xb, 78, "pBlock=%p\n", block);
		SCE_DBG_PRINTF(0xb, 79, "status=0x%08x\n", block->flags);
		return MemBlockPartialGetAllocatedSize(block);
	case MEMBLOCK_KIND_VPL:
		return MemBlockVirPageListGetPhySize(block);
	default:
		return 0;
	}
}

/* name unknown */
SceSize MemBlockGetAllocMapSize(MemBlockObject *block)
{
	int flags = SceCpuForDriver_EC53D007(&block->lock);
	SceSize size = MemBlockGetAllocMapSizeRaw(block);

	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&block->lock, flags);
	return size;
}

/* name unknown: the one physical range behind the block (an error when there are more) */
int MemBlockGetPhyRange(MemBlockObject *block, uintptr_t range[2])
{
	VirPage *vp;
	uintptr_t pbase;
	int flags, found = 0, ret = 0;

	flags = SceCpuForDriver_EC53D007(&block->lock);
	for (vp = MemBlockGetVirPageRaw(block); vp != NULL; vp = vp->next) {
		ret = VirPageGetPhyBase(vp, &pbase);
		if (ret >= 0) {
			if (found) {
				ret = SCE_KERNEL_ERROR_VARANGE_IS_NOT_PHYSICAL_CONTINUOUS;
				break;
			}
			{
				SceSize size = vp->size;

				found = 1;
				range[0] = pbase;
				range[1] = size;
			}
		}
	}
	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&block->lock, flags);
	return ret;
}

/* name unknown: the block's physical ranges into [cur, end), adjacent pieces merged */
int MemBlockGetPhyRangeList(MemBlockObject *block, SceKernelPAVector *vector,
			    SceKernelPARange *cur, SceKernelPARange *end)
{
	VirPage *vp;
	SceKernelPARange *last = NULL;
	uintptr_t pbase;

	for (vp = MemBlockGetVirPageRaw(block); vp != NULL; vp = vp->next) {
		if (VirPageGetPhyBase(vp, &pbase) < 0)
			continue;
		if (vector->capacity != 0 && cur != end) {
			if (last != NULL && last->addr + last->size == pbase) {
				last->size += vp->size;
				continue;
			}
			cur->addr = pbase;
			cur->size = vp->size;
			last = cur;
			cur++;
		}
		vector->total++;
	}
	return 0;
}

/* name unknown: the block an allocation mirrors */
int MemBlockGetMirrorBlock(const SceKernelAllocMemBlockKernelOpt *opt, MemBlockObject **block)
{
	return sceGUIDGetObjectWithClass(opt->mirror_blockid, s_memBlockClass,
					  (SceObjectBase **)block);
}

/* the unbacked bytes in front of the first backed ones */
__attribute__((noinline))
SceSize MemBlockGetHeadFreeSize(MemBlockObject *block)
{
	if (block->code != 0 && !(block->attr & MEMBLOCK_ATTR_WHOLE)) {
		switch (block->flags & MEMBLOCK_KIND_MASK) {
		case MEMBLOCK_KIND_PARTIAL:
			return MemBlockPartialGetHeadFreeSize(block);
		case MEMBLOCK_KIND_VPL:
			return MemBlockVirPageListGetHeadFreeSize(block);
		}
	}
	return 0;
}

/* name unknown: the first backed address */
uintptr_t MemBlockGetMappedBase(MemBlockObject *block)
{
	int flags = SceCpuForDriver_EC53D007(&block->lock);
	uintptr_t vbase = block->vbase;
	SceSize head = MemBlockGetHeadFreeSize(block);

	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&block->lock, flags);
	return head + vbase;
}

/* name unknown: GUIDApplyWithClass callback, the mapped base into *argument */
int MemBlockGetMappedBaseCB(SceObjectBase *object, void *argument)
{
	*(uintptr_t *)argument = MemBlockGetMappedBase((MemBlockObject *)object);
	return 0;
}

/* name unknown: the unbacked bytes at the end */
SceSize MemBlockGetTailFreeSize(MemBlockObject *block)
{
	if (block->code != 0 && !(block->attr & MEMBLOCK_ATTR_WHOLE)) {
		switch (block->flags & MEMBLOCK_KIND_MASK) {
		case MEMBLOCK_KIND_PARTIAL:
			return MemBlockPartialGetTailSize(block);
		case MEMBLOCK_KIND_VPL:
			return MemBlockVirPageListGetTailFreeSize(block);
		}
	}
	return 0;
}

#if FW_DEBUG
/* debug builds only (their list helpers are debug-only) */

/* name unknown */
SceSize MemBlockGetHeadType1Size(MemBlockObject *block)
{
	if (block->code != 0 && !(block->attr & MEMBLOCK_ATTR_WHOLE)) {
		switch (block->flags & MEMBLOCK_KIND_MASK) {
		case MEMBLOCK_KIND_PARTIAL:
			return MemBlockPartialGetHeadFreeSize(block);
		case MEMBLOCK_KIND_VPL:
			return MemBlockVirPageListGetHeadType1Size(block);
		}
	}
	return 0;
}

/* name unknown */
SceSize MemBlockGetTailType1Size(MemBlockObject *block)
{
	if (block->code != 0 && !(block->attr & MEMBLOCK_ATTR_WHOLE)) {
		switch (block->flags & MEMBLOCK_KIND_MASK) {
		case MEMBLOCK_KIND_PARTIAL:
			return MemBlockPartialGetTailSize(block);
		case MEMBLOCK_KIND_VPL:
			return MemBlockVirPageListGetTailType1Size(block);
		}
	}
	return 0;
}
#endif

/* the mapped bytes */
SceSize MemBlockGetMapSize(MemBlockObject *block)
{
	SceUInt32 whole;

	if (block->code == 0)
		return block->size;
	whole = block->attr & MEMBLOCK_ATTR_WHOLE;
	if (whole)
		return block->size;
	switch (block->flags & MEMBLOCK_KIND_MASK) {
	case MEMBLOCK_KIND_PARTIAL:
		SCE_DBG_PRINTF(0xb, 566, "pBlock=%p\n", block);
		SCE_DBG_PRINTF(0xb, 567, "status=0x%08x\n", block->flags);
		return MemBlockPartialGetMappedSize(block);
	case MEMBLOCK_KIND_VPL:
		return MemBlockVirPageListGetMappedSize(block);
	default:
		SCE_DBG_ASSERT_FAIL(573);
		return whole;
	}
}

/* the given physical pages behind the block's first virtual page, marked mapped */
SYSMEM_DEBUG_FUNC int MemBlockAssocPhyPage(MemBlockObject *block, struct PhyPage *page)
{
	VirPage *vp;

	if ((block->flags & MEMBLOCK_KIND_MASK) != MEMBLOCK_KIND_VPL)
		return 0;
	vp = MemBlockGetVirPageRaw(block);
	if (vp->page != NULL) {
		SCE_DBG_ASSERT_FAIL(647);
		return SCE_KERNEL_ERROR_BLOCK_ERROR;
	}
	vp->page = page;
	vp->pbase = *(const SceUInt32 *)page << 12;
	block->attr |= MEMBLOCK_ATTR_MAPPED;
	return 0;
}

/* the ASID of the current address space */
static inline SceUInt32 MemBlockCurrentAsid(void)
{
	return readContextId() & 0xff;
}

#pragma push_macro("__FILE_NAME__")
#undef __FILE_NAME__
#define __FILE_NAME__ "memblock_obj.c"

/* a new block's memory zeroed once, in its own address space */
int MemBlockFill(MemBlockObject *block)
{
	Partition *partition;
	uintptr_t vbase, start, pa;
	SceUInt32 dacr;
	int ret = 0;

	if (!(block->attr & MEMBLOCK_ATTR_NEEDS_CLEAR))
		return 0;
	vbase = block->vbase;
	partition = block->partition;
	if (vbase >= 0x40000000 && partition != NULL && partition->addressSpace != NULL) {
		SceUInt32 contextId = partition->addressSpace->context->mmu.contextId;

		if (contextId != MemBlockCurrentAsid()) {
			AddressSpaceObject *addressSpace = partition->addressSpace;
			SceSize size;

			start = vbase + MemBlockGetHeadFreeSize(block);
			size = MemBlockGetAllocMapSizeRaw(block);
			ret = AddressSpaceMemset(addressSpace, (void *)start, 0, size);
			if (ret < 0) {
				SCE_DBG_ASSERT_FAIL(750);
				return ret;
			}
			goto cleared;
		}
	}
	start = vbase + MemBlockGetHeadFreeSize(block);
	if (MMUVAtoPAWithFlags(2, 1, start, &pa) < 0) {
		/* not writable as it is: with every domain open as client */
		dacr = readDacr();
		writeDacr(DACR_ALL_CLIENT);
		memset((void *)start, 0, MemBlockGetAllocMapSizeRaw(block));
		writeDacr(dacr);
	} else {
		memset((void *)start, 0, MemBlockGetAllocMapSizeRaw(block));
	}
cleared:
	block->attr &= ~MEMBLOCK_ATTR_NEEDS_CLEAR;
	return 0;
}

#pragma pop_macro("__FILE_NAME__")

/* name unknown: whether every quarter of an L2 page table block is free */
int MemBlockL2PageTableIsAllFree(const MemBlockObject *block)
{
	SceUInt32 flags = block->flags;
	SceUInt32 l2pt = flags & MEMBLOCK_L2PT;

	if (!l2pt)
		return l2pt;
	return (flags & MEMBLOCK_L2PT_FREE_MASK) == MEMBLOCK_L2PT_FREE_MASK;
}

#pragma push_macro("__FILE_NAME__")
#undef __FILE_NAME__
#define __FILE_NAME__ "memblock/status_module.h"

static inline int MemBlockL2PageTableCheckFreeRaw(const MemBlockObject *block, SceUInt32 quarter)
{
	SCE_DBG_ASSERT_FUNC(quarter, 632, "MemBlockL2PageTableCheckFreeRaw");
	return (block->flags & quarter) != 0;
}

#pragma pop_macro("__FILE_NAME__")

#pragma push_macro("__FILE_NAME__")
#undef __FILE_NAME__
#define __FILE_NAME__ "memblock/status_module.h"

static inline void MemBlockL2PageTableClearFreeRaw(MemBlockObject *block, SceUInt32 quarter)
{
	SCE_DBG_ASSERT_FUNC(quarter, 597, "MemBlockL2PageTableClearFreeRaw");
	block->flags &= ~quarter;
}

#pragma pop_macro("__FILE_NAME__")

#pragma push_macro("__FILE_NAME__")
#undef __FILE_NAME__
#define __FILE_NAME__ "memblock_obj.c"

/* the free quarters of an L2 page table block */
int MemBlockL2PageTableFreeCount(const MemBlockObject *block)
{
	int count;

	if (block == NULL)
		return 0;
	if (!(block->flags & MEMBLOCK_L2PT)) {
		SCE_DBG_PRINTF(0xb, 800, "pBlock %p .status=0x%08x\n", block, block->flags);
		SCE_DBG_ASSERT_FAIL(808);
		return 0;
	}
	count = MemBlockL2PageTableCheckFreeRaw(block, MEMBLOCK_L2PT_FREE(0));
	if (MemBlockL2PageTableCheckFreeRaw(block, MEMBLOCK_L2PT_FREE(1)))
		count++;
	if (MemBlockL2PageTableCheckFreeRaw(block, MEMBLOCK_L2PT_FREE(2)))
		count++;
	if (MemBlockL2PageTableCheckFreeRaw(block, MEMBLOCK_L2PT_FREE(3)))
		count++;
	return count;
}

#pragma pop_macro("__FILE_NAME__")

#pragma push_macro("__FILE_NAME__")
#undef __FILE_NAME__
#define __FILE_NAME__ "memblock_obj.c"

/* the lowest free quarter taken: its flag, virtual and physical address */
int MemBlockL2PageTableAllocFreeArea(MemBlockObject *block, SceUInt32 *quarter, uintptr_t *va,
				     uintptr_t *pa)
{
	VirPage *vp;
	uintptr_t vbase, pbase, lowest;
	SceUInt32 taken;
	int ret = 0;

	if (block == NULL)
		return SCE_KERNEL_ERROR_PARTITION_ERROR;
	if (!(block->flags & MEMBLOCK_L2PT_FREE_MASK)) {
		SCE_DBG_ASSERT_CHECK(block->flags & MEMBLOCK_L2PT_FREE_MASK, 851);
		return SCE_KERNEL_ERROR_PARTITION_ERROR;
	}
	vbase = block->vbase;
	if (block->code != 0 && (block->attr & MEMBLOCK_ATTR_WHOLE)) {
		ret = SimpleMemBlockGetPbase(block, &pbase);
		if (ret < 0)
			return ret;
	} else if ((block->flags & MEMBLOCK_KIND_MASK) == MEMBLOCK_KIND_VPL) {
		lowest = 0xffffffff;
		for (vp = MemBlockGetVirPageRaw(block); vp != NULL; vp = vp->next) {
			uintptr_t candidate;

			if (VIRPAGE_TYPE(vp) != VIRPAGE_TYPE_DIVIDED)
				continue;
			ret = VirPageGetPhyBase(vp, &candidate);
			if (ret < 0)
				return ret;
			if (candidate <= lowest)
				lowest = candidate;
		}
		pbase = lowest;
		ret = 0;
	}
	if (MemBlockL2PageTableCheckFreeRaw(block, MEMBLOCK_L2PT_FREE(0))) {
		MemBlockL2PageTableClearFreeRaw(block, MEMBLOCK_L2PT_FREE(0));
		taken = MEMBLOCK_L2PT_FREE(0);
	} else if (MemBlockL2PageTableCheckFreeRaw(block, MEMBLOCK_L2PT_FREE(1))) {
		MemBlockL2PageTableClearFreeRaw(block, MEMBLOCK_L2PT_FREE(1));
		vbase += 0x400;
		pbase += 0x400;
		taken = MEMBLOCK_L2PT_FREE(1);
	} else if (MemBlockL2PageTableCheckFreeRaw(block, MEMBLOCK_L2PT_FREE(2))) {
		MemBlockL2PageTableClearFreeRaw(block, MEMBLOCK_L2PT_FREE(2));
		vbase += 0x800;
		pbase += 0x800;
		taken = MEMBLOCK_L2PT_FREE(2);
	} else if (MemBlockL2PageTableCheckFreeRaw(block, MEMBLOCK_L2PT_FREE(3))) {
		MemBlockL2PageTableClearFreeRaw(block, MEMBLOCK_L2PT_FREE(3));
		vbase += 0xc00;
		pbase += 0xc00;
		taken = MEMBLOCK_L2PT_FREE(3);
	} else {
		taken = 0;
	}
	*quarter = taken;
	*va = vbase;
	*pa = pbase;
	return ret;
}

#pragma pop_macro("__FILE_NAME__")

/* name unknown: the map reference count */
SceUInt16 MemBlockGetMapRef(MemBlockObject *block)
{
	int flags = SceCpuForDriver_EC53D007(&block->lock);
	SceUInt16 count = block->mapRef;

	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&block->lock, flags);
	return count;
}

/* name unknown */
void MemBlockClearMapRef(MemBlockObject *block)
{
	int flags = SceCpuForDriver_4C38CE4D(&block->lock);

	block->mapRef = 0;
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&block->lock, flags);
}

/* name unknown */
void MemBlockIncMapRef(MemBlockObject *block)
{
	int flags = SceCpuForDriver_4C38CE4D(&block->lock);

	block->mapRef++;
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&block->lock, flags);
}

/* name unknown: one map reference dropped; busy while any were left (sic) */
int MemBlockDecMapRef(MemBlockObject *block)
{
	int flags = SceCpuForDriver_4C38CE4D(&block->lock);
	int ret = 0;

	if (block->mapRef != 0) {
		block->mapRef--;
		ret = SCE_KERNEL_ERROR_BLOCK_ERROR;
	}
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&block->lock, flags);
	return ret;
}

/* name unknown: the lock reference count */
SceUInt16 MemBlockGetLockRef(MemBlockObject *block)
{
	int flags = SceCpuForDriver_EC53D007(&block->lock);
	SceUInt16 count = block->lockRef;

	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&block->lock, flags);
	return count;
}

/* name unknown */
void MemBlockIncLockRef(MemBlockObject *block)
{
	int flags = SceCpuForDriver_4C38CE4D(&block->lock);

	block->lockRef++;
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&block->lock, flags);
}

/* name unknown: one lock reference dropped; busy when there was none */
int MemBlockDecLockRef(MemBlockObject *block)
{
	int flags = SceCpuForDriver_4C38CE4D(&block->lock);
	int ret;

	if (block->lockRef == 0) {
		ret = SCE_KERNEL_ERROR_BLOCK_ERROR;
	} else {
		ret = 0;
		block->lockRef--;
	}
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&block->lock, flags);
	return ret;
}

/* name unknown: the block's GUID */
SceUID MemBlockGetGUID(MemBlockObject *block)
{
	int flags = SceCpuForDriver_EC53D007(&block->lock);
	SceUID uid = block->guid;

	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&block->lock, flags);
	return uid;
}

/* name unknown: a list block's physical pages freed (game pages from the game partition) */
int MemBlockFreePhyPages(MemBlockObject *block, void *phyPart)
{
	VirPage *vp;
	int flags;

	flags = SceCpuForDriver_4C38CE4D(&block->lock);
	if ((block->flags & MEMBLOCK_KIND_MASK) == MEMBLOCK_KIND_VPL) {
		for (vp = MemBlockGetVirPageRaw(block); vp != NULL; vp = vp->next) {
			if (vp->page == NULL)
				continue;
			if ((vp->flags & 0xf000000) == 0x1000000 && s_phyPartGame != NULL)
				phyPart = s_phyPartGame;
			PhyMemPartFreePhyPage(phyPart, vp->page);
			vp->page = NULL;
		}
	}
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&block->lock, flags);
	return 0;
}

/* a memblock object from the heap, constructed (0.990: AllocMemBlockObject) */
int AllocMemBlockObject(FixedHeap *heap, MemBlockObject **block)
{
	void *object;
	int ret = FixedHeapAllocObjectExt(heap, &object);

	if (ret >= 0) {
		MemBlockObjectCreate(object);
		*block = object;
	}
	return ret;
}

/* a memblock object destructed and back to the heap (0.990: FreeMemBlockObject) */
void FreeMemBlockObject(FixedHeap *heap, MemBlockObject *block)
{
	MemBlockObjectDestroy(block);
	FixedHeapFreeObject(heap, block);
}

/* name unknown: base info attached to the block of uid */
int MemBlockSetBaseInfo(SceUID uid, void *info)
{
	MemBlockObject *block;
	MemBlockObject *baseBlock;
	SceUInt32 status;
	int flags, ret;

	if (info == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = sceGUIDReferObjectWithClass(uid, s_memBlockClass, (SceObjectBase **)&block);
	if (ret < 0)
		return ret;
	flags = SceCpuForDriver_4C38CE4D(&block->lock);
	baseBlock = block;
	status = baseBlock->flags;
#if FW_DEBUG
	if ((status & MEMBLOCK_OPT_MASK) != MEMBLOCK_OPT_BASE_BLOCK &&
	    SceDebugForDriver_374B7868() > 0) {
		SceDebugForDriver_FD753E7A(1, 0, SCE_DBGINFO_FUNC(1120, "MemBlockSetBaseInfoRaw"),
				      __builtin_return_address(0));
		status = baseBlock->flags;
	}
#endif
	if ((status & MEMBLOCK_OPT_MASK) == MEMBLOCK_OPT_BASE_BLOCK) {
		status &= ~MEMBLOCK_OPT_MASK;
		baseBlock->flags = status;
		baseBlock->opt = 0;
	} else if ((status & MEMBLOCK_OPT_MASK) != 0) {
#pragma push_macro("__FILE_NAME__")
#undef __FILE_NAME__
#define __FILE_NAME__ "memblock_obj_module.h"
		SCE_DBG_PRINTF_FUNC(0xb, 1025, "MemBlockOptSetBaseInfoRaw", "status=0x%08x\n",
				    status);
#pragma pop_macro("__FILE_NAME__")
		status = baseBlock->flags;
	}
	baseBlock->flags = (status & ~MEMBLOCK_OPT_MASK) | MEMBLOCK_OPT_BASE_INFO;
	baseBlock->opt = (SceUInt32)info;
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&block->lock, flags);
	sceGUIDReleaseObject(uid);
	return 0;
}

#pragma push_macro("__FILE_NAME__")
#undef __FILE_NAME__
#define __FILE_NAME__ "memblock_obj.c"

int sceKernelGetMemBlockAllocMapSize(SceUID uid, SceSize *size)
{
	MemBlockObject *block;

	if (size == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	block = SceSysmemForKernel_FA6C098C(uid);
	if (block == NULL) {
		SCE_DBG_PRINTF_FUNC(0xf, 1266, "sceKernelGetMemBlockAllocMapSize",
			"Invalid block id\n");
		return SCE_KERNEL_ERROR_ILLEGAL_BLOCK_ID;
	}
	*size = MemBlockGetAllocMapSize(block);
	return 0;
}

#pragma pop_macro("__FILE_NAME__")

/* name unknown: physical pages for the block of uid */
int SceSysmemForKernel_441E6BE9(SceUID uid, struct PhyPage *page)
{
	MemBlockObject *block = SceSysmemForKernel_FA6C098C(uid);

	if (block != NULL)
		return MemBlockAssocPhyPage(block, page);
	return SCE_KERNEL_ERROR_ILLEGAL_BLOCK_ID;
}

int sceKernelGetMemBlockMappedBase(SceUID uid, void **base)
{
	GUIDApplyArgs args;

	if (base == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	*base = NULL;
	args.result = 0;
	args.argument = base;
	return GUIDApplyWithClass(uid, s_memBlockClass, MemBlockGetMappedBaseCB, &args);
}

int sceKernelGetMemBlockBase(SceUID uid, void **base)
{
	return sceKernelGetMemBlockMappedBase(uid, base);
}

/* name unknown: SceUIDMemBlockClass gets its callbacks, then the simple memblocks start */
void MemBlockObjModuleStart(const UIDManager *manager)
{
	s_memBlockClass = manager->memBlockClass;
	UIDClassSetCallbacks(s_memBlockClass, "SceUIDMemBlockClass",
			     (SceClassCallback)MemBlockObjectCreate,
			     (SceClassCallback)MemBlockObjectDestroy);
	SimpleMemBlockModuleStart(manager);
}
