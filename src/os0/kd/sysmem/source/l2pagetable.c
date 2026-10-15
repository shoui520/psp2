/*
 * SceSysmem: L2 page tables. Each covers one MiB of an address space and
 * counts the pages mapped through it, under its own lock.
 */

#include "sysmem_internal.h"
#include "memblock_obj_module.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "l2pagetable.c"

int sceKernelMMUL2Init(uint32_t *l2);
int sceKernelMMUMapLargePages(uint32_t *l2, uint32_t type, uint32_t domain, uintptr_t offset,
			      SceSize size, uintptr_t pa);
int sceKernelMMUUnmapLargePagesWithFlags(uint32_t *l2, uint32_t asid, uintptr_t va,
					 uintptr_t offset, SceSize size, uint32_t flags);
int sceKernelMMUMapSmallPages(uint32_t *l2, uint32_t type, uint32_t domain, uintptr_t offset,
			      SceSize size, uintptr_t pa);
int sceKernelMMUUnmapSmallPagesWithFlags(uint32_t *l2, uint32_t asid, uintptr_t va,
					 uintptr_t offset, SceSize size, uint32_t flags);

/* memblock type bit: a shared (S) mapping */
#define L2PT_TYPE_SHARED		0x10000

#define L2PT_LARGE_PAGE			0x10000

/* L2PageTableUnmapPages flags: the pages stay reserved */
#define L2PT_UNMAP_KEEP_RESERVED	0x01
/* L2PageTableRemapPages flags: write the caches back first */
#define L2PT_REMAP_SYNC_CACHES		0x01

/* L2PageTable: sysmem_internal.h */

/* name unknown: a cleared table object from the heap */
int AllocL2PageTableObject(Partition *partition, FixedHeap *heap, SceUInt32 opt, L2PageTable **out)
{
	L2PageTable *l2pt;
	int ret;

	ret = FixedHeapAllocObjectWithAttr(heap, partition, opt, (void **)&l2pt);
	if (ret < 0)
		return ret;
	l2pt->magic = L2PAGETABLE_MAGIC;
	l2pt->lock = 0;
	l2pt->vbase = 0;
	l2pt->flags = 0;
	l2pt->mappedPages = 0;
	l2pt->reservedPages = 0;
	l2pt->table = NULL;
	l2pt->reserved2 = 0;
	l2pt->tablePa = 0;
	l2pt->block = NULL;
	l2pt->partition = NULL;
	l2pt->firstBlock = NULL;
	*out = l2pt;
	return ret;
}

/* name unknown: clears the table's bookkeeping and frees it */
int L2PageTableFree(FixedHeap *heap, L2PageTable *l2pt)
{
	l2pt->lock = 0;
	l2pt->vbase = 0;
	l2pt->flags = 0;
	l2pt->mappedPages = 0;
	l2pt->reservedPages = 0;
	l2pt->table = NULL;
	l2pt->reserved2 = 0;
	l2pt->tablePa = 0;
	l2pt->block = NULL;
	l2pt->partition = NULL;
	l2pt->firstBlock = NULL;
	l2pt->magic = 0;
	return FixedHeapFreeObject(heap, l2pt);
}

/* name unknown */
int L2PageTableInit(L2PageTable *l2pt)
{
	return sceKernelMMUL2Init(l2pt->table);
}

/* name unknown: the pages mapped and reserved through the table */
int L2PageTableGetPageCount(L2PageTable *l2pt)
{
	int flags = ksceKernelRWSpinlockLowReadLockCpuSuspendIntr(&l2pt->lock);
	SceInt16 mapped = l2pt->mappedPages, reserved = l2pt->reservedPages;

	ksceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&l2pt->lock, flags);
	return mapped + reserved;
}

/* name unknown: releases reserved pages */
void L2PageTableUnreserve(L2PageTable *l2pt, SceSize size)
{
	int flags = ksceKernelRWSpinlockLowWriteLockCpuSuspendIntr(&l2pt->lock);

	l2pt->reservedPages -= size >> 12;
	ksceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&l2pt->lock, flags);
}

/* the low part of an address space is mapped shared */
static inline SceUInt32 L2PageTableSharedType(const Partition *partition,
					      SceUInt32 type, uintptr_t va)
{
	const L1PageTable *l1pt = partition->tiny.context->l1pt;

	/* below the L1 split (TTBR0, user space) mappings are shared */
	if (!(type & L2PT_TYPE_SHARED) && va < l1pt->split)
		type |= L2PT_TYPE_SHARED;
	return type;
}

int L2PageTableMapPages(L2PageTable *l2pt, SceUInt32 type, uintptr_t va, SceSize size,
			uintptr_t pa)
{
	int flags, ret;

	flags = ksceKernelRWSpinlockLowWriteLockCpuSuspendIntr(&l2pt->lock);
	type = L2PageTableSharedType(l2pt->partition, type, va);
	if (size < L2PT_LARGE_PAGE)
		ret = sceKernelMMUMapSmallPages(l2pt->table, type, va, va - l2pt->vbase, size, pa);
	else
		ret = sceKernelMMUMapLargePages(l2pt->table, type, va, va - l2pt->vbase, size, pa);
	if (ret < 0)
		SCE_DBG_ASSERT_FAIL(512);
	else
		l2pt->mappedPages += size >> 12;
	ksceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&l2pt->lock, flags);
	return ret;
}

int L2PageTableRemapPages(L2PageTable *l2pt, MemBlockObject *block, SceUInt32 type,
			  uintptr_t va, SceSize size, uintptr_t pa, SceUInt32 flags)
{
	const AddressSpaceContext *context;
	int intr, ret;

	type = L2PageTableSharedType(l2pt->partition, type, va);
	context = ((Partition *)block->partition)->tiny.context;
	if (flags & L2PT_REMAP_SYNC_CACHES) {
		intr = ksceKernelRWSpinlockLowReadLockCpuSuspendIntr(&l2pt->lock);
		if (block->code & 0x42)
			ksceKernelL1DcacheCleanInvalidateRange((void *)va, size);
		if (type & 0x11)
			ksceKernelL1IcacheInvalidateRange((void *)va, size);
		ksceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&l2pt->lock, intr);
	}
	intr = ksceKernelRWSpinlockLowWriteLockCpuSuspendIntr(&l2pt->lock);
	if (size < L2PT_LARGE_PAGE) {
		sceKernelMMUUnmapSmallPagesWithFlags(l2pt->table, (SceUInt8)context->mmu.contextId,
						     va, va - l2pt->vbase, size, 0);
		ret = sceKernelMMUMapSmallPages(l2pt->table, type, va, va - l2pt->vbase, size, pa);
	} else {
		sceKernelMMUUnmapLargePagesWithFlags(l2pt->table, (SceUInt8)context->mmu.contextId,
						     va, va - l2pt->vbase, size, 0);
		ret = sceKernelMMUMapLargePages(l2pt->table, type, va, va - l2pt->vbase, size, pa);
	}
	if (ret < 0)
		SCE_DBG_ASSERT_FAIL(593);
	ksceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&l2pt->lock, intr);
	return ret;
}

int L2PageTableUnmapPages(L2PageTable *l2pt, SceUInt32 asid, uintptr_t va, SceSize size,
			  SceUInt32 flags)
{
	int intr, ret;

	intr = ksceKernelRWSpinlockLowWriteLockCpuSuspendIntr(&l2pt->lock);
	if (size < L2PT_LARGE_PAGE)
		ret = sceKernelMMUUnmapSmallPagesWithFlags(l2pt->table, asid, va, va - l2pt->vbase,
							   size, flags);
	else
		ret = sceKernelMMUUnmapLargePagesWithFlags(l2pt->table, asid, va, va - l2pt->vbase,
							   size, flags);
	if (ret < 0)
		SCE_DBG_ASSERT_FAIL(639);
	else if (!(flags & L2PT_UNMAP_KEEP_RESERVED))
		l2pt->mappedPages -= size >> 12;
	else
		l2pt->reservedPages += size >> 12;
	ksceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&l2pt->lock, intr);
	return ret;
}

/* name unknown: nothing to set up */
int L2PageTableModuleStart(void)
{
	return 0;
}
