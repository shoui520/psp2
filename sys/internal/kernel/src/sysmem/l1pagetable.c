/*
 * SceSysmem: L1 page tables. An address space has two: one below the
 * TTBR0/TTBR1 split and one above it. Ranges are mapped as supersections
 * (16 MiB units in domain 0) or sections, under the table's lock.
 */

#include "sysmem_internal.h"
#include "memblock_obj_module.h"

int sceKernelMMUSetL2PageTable(uint32_t *l1, uint32_t domain, uintptr_t va, uintptr_t l2pa);
int sceKernelMMUUnsetL2PageTable(uint32_t *l1, uintptr_t va);
int sceKernelMMUMapSupersections(uint32_t *l1, uint32_t type, uint32_t domain, uintptr_t va,
				 SceSize size, uintptr_t pa);
int sceKernelMMUMapSections(uint32_t *l1, uint32_t type, uint32_t domain, uintptr_t va,
			    SceSize size, uintptr_t pa);
int sceKernelMMUUnmapSectionsWithFlags(uint32_t *l1, uint32_t asid, uintptr_t va, SceSize size,
				       uint32_t flags);
int MMUUnmapSupersectionsWithFlags(uint32_t *l1, uint32_t asid, uintptr_t va, SceSize size,
				   uint32_t flags);
int sceKernelMMUL1VAtoPABySW(const uint32_t *l1, uintptr_t va, uintptr_t *pa);
int sceKernelMMUL1IsEqualAccessibleBySW(const uint32_t *l1, uint32_t perm, uintptr_t va,
					uintptr_t *pa);
int sceKernelMMUL1GetMemoryType(const uint32_t *l1, uintptr_t va);
int MMUIsValidMap(const MMUContext *context, uintptr_t va, SceSize size, uintptr_t pa);
int SceSysrootForKernel_5C426B19(void);

/* memblock type bit: a shared (S) mapping */
#define L1PT_TYPE_SHARED		0x10000

#define L1PT_SUPERSECTION		0x1000000
#define L1PT_SECTION			0x100000

/* address_space.c */
MMUContext *AddressSpaceGetMMUContext(AddressSpaceObject *as);

static inline uint32_t *L1PageTableFor(const L1PageTable *l1pt, uintptr_t va)
{
	return va < l1pt->split ? l1pt->low : l1pt->high;
}

/* supersections: 16 MiB units, domain 0 only */
static inline int L1PageTableUseSupersections(SceSize unit, SceUInt32 domain)
{
	return unit >= L1PT_SUPERSECTION && domain == 0;
}

/* name unknown: whether [va, va + size) maps to pa in the partition's address space */
int L1PageTableCheckMap(Partition *partition, uintptr_t va, SceSize size, uintptr_t pa)
{
	MMUContext *context = NULL;

	if (partition == NULL) {
		if ((SceUInt32)SceSysrootForKernel_5C426B19() > 3)
			return 0;
	} else if (partition->addressSpace != NULL) {
		context = AddressSpaceGetMMUContext(partition->addressSpace);
	}
	return MMUIsValidMap(context, va, size, pa);
}

/* name unknown: a new L1 page table object */
void L1PageTableInit(L1PageTable *l1pt, uint32_t *low, uint32_t *high, uintptr_t split,
		     SceUInt32 reserved)
{
	l1pt->lock = 0;
	l1pt->reserved = reserved;
	l1pt->low = low;
	l1pt->high = high;
	l1pt->split = split;
}

int L1PageTableMapDirect(L1PageTable *l1pt, SceUInt32 type, SceUInt32 domain, uintptr_t va,
			 SceSize size, uintptr_t pa)
{
	int flags, ret;

	SCE_DBG_ASSERT_CHECK(domain < 16, 175);
	flags = SceCpuForDriver_4C38CE4D(&l1pt->lock);
	if (L1PageTableUseSupersections(size, domain)) {
		if ((va & (L1PT_SUPERSECTION - 1)) == 0 && (size & (L1PT_SUPERSECTION - 1)) == 0 &&
		    (pa & (L1PT_SUPERSECTION - 1)) == 0)
			ret = sceKernelMMUMapSupersections(L1PageTableFor(l1pt, va), type, 0, va,
							   size, pa);
		else
			ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	} else {
		ret = sceKernelMMUMapSections(L1PageTableFor(l1pt, va), type, domain, va, size, pa);
	}
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&l1pt->lock, flags);
	return ret;
}

/* name unknown */
int L1PageTableUnmapDirect(L1PageTable *l1pt, SceUInt32 asid, SceUInt32 domain, uintptr_t va,
			   SceSize size)
{
	int flags, ret;

	flags = SceCpuForDriver_4C38CE4D(&l1pt->lock);
	if (L1PageTableUseSupersections(size, domain)) {
		if ((va & (L1PT_SUPERSECTION - 1)) == 0 && (size & (L1PT_SUPERSECTION - 1)) == 0)
			ret = MMUUnmapSupersectionsWithFlags(L1PageTableFor(l1pt, va), asid, va,
							     size, 0);
		else
			ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	} else {
		ret = sceKernelMMUUnmapSectionsWithFlags(L1PageTableFor(l1pt, va), asid, va, size,
							 0);
	}
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&l1pt->lock, flags);
	return ret;
}

/* unit is the block's mapping granularity */
int L1PageTableMap(L1PageTable *l1pt, MemBlockObject *block, SceUInt32 type,
		   SceUInt32 domain, SceSize unit, uintptr_t va, SceSize size, uintptr_t pa)
{
	int flags, ret;

	flags = SceCpuForDriver_4C38CE4D(&l1pt->lock);
	if (!(type & L1PT_TYPE_SHARED) && va < l1pt->split)
		type |= L1PT_TYPE_SHARED;
	if (L1PageTableUseSupersections(unit, domain)) {
		if (block == NULL || !(block->attr & MEMBLOCK_ATTR_PARTIAL)) {
			ret = sceKernelMMUMapSupersections(L1PageTableFor(l1pt, va), type, 0, va,
							   size, pa);
			goto out;
		}
	} else if (unit < L1PT_SECTION) {
		SCE_DBG_ASSERT_FAIL(304);
		ret = SCE_KERNEL_ERROR_ERROR;
		goto out;
	}
	ret = sceKernelMMUMapSections(L1PageTableFor(l1pt, va), type, domain, va, size, pa);
out:
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&l1pt->lock, flags);
	return ret;
}

/* name unknown: maps a block's range again with its current type */
int L1PageTableRemap(L1PageTable *l1pt, MemBlockObject *block, SceUInt32 type,
		     SceUInt32 domain, SceSize unit, uintptr_t va, SceSize size, uintptr_t pa,
		     SceUInt32 flags)
{
	SceUInt32 mapType = block->code;
	AddressSpaceContext *context = ((Partition *)block->partition)->tiny.context;
	int intr, ret;

	if (!(mapType & L1PT_TYPE_SHARED) && va < l1pt->split)
		mapType |= L1PT_TYPE_SHARED;
	if (flags & 1) {
		intr = SceCpuForDriver_EC53D007(&l1pt->lock);
		if (block->code & 0x42)
			SceCpuForKernel_4F442396((void *)va, size);
		if (mapType & 0x11)
			SceCpuForKernel_2E637B1D((void *)va, size);
		sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&l1pt->lock, intr);
	}
	intr = SceCpuForDriver_4C38CE4D(&l1pt->lock);
	if (L1PageTableUseSupersections(unit, domain) &&
	    !(block->attr & MEMBLOCK_ATTR_PARTIAL)) {
		MMUUnmapSupersectionsWithFlags(L1PageTableFor(l1pt, va),
					       (SceUInt8)context->mmu.contextId, va, size, 0);
		ret = sceKernelMMUMapSupersections(L1PageTableFor(l1pt, va), mapType, 0, va, size,
						   pa);
	} else {
		if (!L1PageTableUseSupersections(unit, domain) && unit < L1PT_SECTION) {
			ret = SCE_KERNEL_ERROR_ERROR;
			goto out;
		}
		sceKernelMMUUnmapSectionsWithFlags(L1PageTableFor(l1pt, va),
						   (SceUInt8)context->mmu.contextId, va, size, 0);
		ret = sceKernelMMUMapSections(L1PageTableFor(l1pt, va), type, domain, va, size, pa);
	}
	L1PageTableCheckMap(block->partition, va, size, pa);
out:
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&l1pt->lock, intr);
	return ret;
}

/* name unknown */
int L1PageTableSetL2PageTable(L1PageTable *l1pt, SceUInt32 domain, uintptr_t va,
			      uintptr_t l2pa)
{
	int flags, ret;

	flags = SceCpuForDriver_4C38CE4D(&l1pt->lock);
	ret = sceKernelMMUSetL2PageTable(L1PageTableFor(l1pt, va), domain, va, l2pa);
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&l1pt->lock, flags);
	return ret;
}

/* name unknown */
int L1PageTableUnsetL2PageTable(L1PageTable *l1pt, uintptr_t va)
{
	int flags, ret;

	flags = SceCpuForDriver_4C38CE4D(&l1pt->lock);
	ret = sceKernelMMUUnsetL2PageTable(L1PageTableFor(l1pt, va), va);
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&l1pt->lock, flags);
	return ret;
}

int L1PageTableUnmap(L1PageTable *l1pt, MemBlockObject *block, SceSize unit,
		     SceUInt32 asid, SceUInt8 domain, uintptr_t va, SceSize size, SceUInt32 flags)
{
	int intr, ret;

	if (flags >> 1) {
		SCE_DBG_ASSERT_FAIL(441);
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	}
	intr = SceCpuForDriver_4C38CE4D(&l1pt->lock);
	if (L1PageTableUseSupersections(unit, domain)) {
		if (block == NULL || !(block->attr & MEMBLOCK_ATTR_PARTIAL)) {
			ret = MMUUnmapSupersectionsWithFlags(L1PageTableFor(l1pt, va), asid, va,
							     size, flags);
			goto out;
		}
	} else if (unit < L1PT_SECTION) {
		SCE_DBG_ASSERT_FAIL(460);
		ret = SCE_KERNEL_ERROR_ERROR;
		goto out;
	}
	ret = sceKernelMMUUnmapSectionsWithFlags(L1PageTableFor(l1pt, va), asid, va, size, flags);
out:
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&l1pt->lock, intr);
	return ret;
}

/* name unknown */
int L1PageTableVAtoPABySW(L1PageTable *l1pt, uintptr_t va, uintptr_t *pa)
{
	int flags, ret;

	flags = SceCpuForDriver_EC53D007(&l1pt->lock);
	ret = sceKernelMMUL1VAtoPABySW(L1PageTableFor(l1pt, va), va, pa);
	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&l1pt->lock, flags);
	return ret;
}

/* name unknown */
int L1PageTableIsEqualAccessibleBySW(L1PageTable *l1pt, SceUInt32 perm, uintptr_t va,
				     uintptr_t *pa)
{
	int flags, ret;

	flags = SceCpuForDriver_EC53D007(&l1pt->lock);
	ret = sceKernelMMUL1IsEqualAccessibleBySW(L1PageTableFor(l1pt, va), perm, va, pa);
	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&l1pt->lock, flags);
	return ret;
}

/* name unknown */
int L1PageTableGetMemoryType(L1PageTable *l1pt, uintptr_t va)
{
	int flags, ret;

	flags = SceCpuForDriver_EC53D007(&l1pt->lock);
	ret = sceKernelMMUL1GetMemoryType(L1PageTableFor(l1pt, va), va);
	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&l1pt->lock, flags);
	return ret;
}

/* name unknown: nothing to set up */
int L1PageTableModuleStart(void)
{
	return 0;
}
