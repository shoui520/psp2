/*
 * SceSysmem: memblocks made of a list of virtual pages (kind 4). The list is
 * backed by physical pages, mapped piece by piece in the largest sections the
 * alignment allows (L1 sections from 1 MiB, L2 pages below), remapped and
 * unmapped as a whole.
 */

#include "sce/cp15.h"
#include "sysmem_internal.h"
#define MEMBLOCK_OBJ_MODULE_CALL
#include "memblock_obj_module.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "memblock_vpl.c"

/* the memblock code a page is invalidated through when unmapped */
#define MEMBLOCK_CODE_INVALIDATE	0x40c04006

struct L2PageTable;

/* phymem.c, virpage.c */
int PhyMemPartFreePhyPage(void *part, struct PhyPage *page);
SceSize PhyPageGetSizeRaw(const struct PhyPage *page);
int VirPageAllocPhyPage(VirPage *vp, void *partition, void *phyPart, VirPage **out);
int VirPageFree(void *partition, FixedHeap *heap, VirPage *vp);
/* partition.c */
int PartitionMapPages(void *partition, SceUInt32 code, uintptr_t va, SceSize size,
		      uintptr_t pa, SceUInt32 flags);
int PartitionUnmapL2PageTable(void *partition, uintptr_t va, SceSize size, SceUInt32 mode,
			      SceUInt32 flags);
int PartitionRemapPages(PartitionObject *partition, MemBlockObject *block, SceUInt32 type,
			uintptr_t va, SceSize size, uintptr_t pa, SceUInt32 flags);
int PartitionFindL2PageTable(PartitionObject *partition, uintptr_t va, SceSize size,
			     SceUInt32 flags, struct L2PageTable **l2pt);
void PartitionFindAndTryRemoveL2PageTable(PartitionObject *partition, uintptr_t va, SceSize size);
/* l1pagetable.c, l2pagetable.c (their memblock structs are still placeholders there) */
int L1PageTableMap(L1PageTable *l1pt, MemBlockObject *block, SceUInt32 type, SceUInt32 domain,
		   SceSize unit, uintptr_t va, SceSize size, uintptr_t pa);
int L1PageTableRemap(L1PageTable *l1pt, MemBlockObject *block, SceUInt32 type, SceUInt32 domain,
		     SceSize unit, uintptr_t va, SceSize size, uintptr_t pa, SceUInt32 flags);
int L1PageTableUnmap(L1PageTable *l1pt, MemBlockObject *block, SceSize unit, SceUInt32 asid,
		     SceUInt8 domain, uintptr_t va, SceSize size, SceUInt32 flags);
void L2PageTableUnreserve(struct L2PageTable *l2pt, SceSize size);
/* cache.c, mmu.c, address_space.c */
void CacheInvalidateRangeWithPA(const void *start, uintptr_t pa, SceSize size);
int AddressSpaceSwitchContext(AddressSpaceObject *as);

/* the bit of a type in the type masks below */
#define VIRPAGE_TYPE_BIT(_type)		(1u << ((_type) - 1))
#define VIRPAGE_TYPES_UNMAPPED		(VIRPAGE_TYPE_BIT(1) | VIRPAGE_TYPE_BIT(2) | \
					 VIRPAGE_TYPE_BIT(8))

/* the first physical page's address (PhyPage info: pindex in bits 0..19) */
static inline uintptr_t VplPagePbase(const struct PhyPage *page)
{
	return *(const SceUInt32 *)page << 12;
}

/* whether the PhyPage is an allocated block (link bits 28..29 both set) */
static inline int VplPageAllocated(const struct PhyPage *page)
{
	return (*(const SceUInt32 *)page & 0x30000000) == 0x30000000;
}

/* the largest piece at va (and pa): a power of two no larger than size, aligned at both */
static inline SceSize VplChunk(uintptr_t va, uintptr_t pa, SceSize size)
{
	SceUInt32 align = va | pa | 0x80000000;
	SceSize chunk = 1u << (31 - __builtin_clz(size));

	align &= -align;
	return align <= chunk ? align : chunk;
}

/* the ASID of the current address space */
static inline SceUInt32 VplCurrentAsid(void)
{
	return readContextId() & 0xff;
}

/* a virtual page's mappings removed (flags bit 0: the pages stay reserved) */
int UnmapVirPageWithOpt(PartitionObject *partition, MemBlockObject *block, VirPage *vp,
			SceUInt32 flags)
{
	AddressSpaceContext *context;
	uintptr_t va, pa = 0;
	SceSize size, chunk;
	int byPa = 0, ret;

	if (flags & ~1) {
		SCE_DBG_ASSERT_FAIL(729);
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	}
	context = partition->tiny.context;
	va = vp->vbase;
	size = vp->size;
	if (vp->page == NULL) {
		pa = vp->pbase;
		byPa = pa != 0;
	}
	if (block->code == MEMBLOCK_CODE_INVALIDATE)
		CacheInvalidateRangeWithPA((const void *)va, pa, size);
	ret = 0;
	while (size != 0) {
		if (byPa) {
			chunk = VplChunk(va, pa, size);
		} else {
			chunk = 1u << (31 - __builtin_clz(size));
			if ((1u << __builtin_ctz(va)) <= chunk)
				chunk = 1u << __builtin_ctz(va);
		}
		if (chunk >= 0x100000) {
			PartitionFindAndTryRemoveL2PageTable(partition, va, chunk);
			ret = L1PageTableUnmap(context->l1pt, block, chunk,
					       (SceUInt8)context->mmu.contextId, partition->domain,
					       va, chunk, flags);
		} else if (chunk < 0x1000) {
			SCE_DBG_ASSERT_FAIL(814);
		} else {
			ret = PartitionUnmapL2PageTable(partition, va, chunk, flags, 0);
		}
		va += chunk;
		size -= chunk;
		if (byPa)
			pa += chunk;
	}
	return ret;
}

/* [va, va + size) mapped to pa in the largest pieces the alignment allows */
int MapVirPageCore(const SceKernelAllocMemBlockCommand *command, PartitionObject *partition,
		   L1PageTable **l1pt, MemBlockObject *block, uintptr_t va, SceSize size,
		   uintptr_t pa)
{
	SceUInt32 domain;
	SceSize chunk;
	int ret;

	if (command != NULL && (command->opt.attr & ALLOC_ATTR_HAS_DOMAIN))
		domain = command->opt.domain;
	else
		domain = partition->domain;
	while (size != 0) {
		chunk = VplChunk(va, pa, size);
		if (chunk >= 0x100000) {
			PartitionFindAndTryRemoveL2PageTable(partition, va, chunk);
			L1PageTableMap(*l1pt, block, block->code, domain, chunk, va, chunk, pa);
		} else if (chunk < 0x1000) {
			SCE_DBG_ASSERT_FAIL(490);
		} else {
			ret = PartitionMapPages(partition, block->code, va, chunk, pa, 0);
			if (ret < 0)
				return ret;
		}
		size -= chunk;
		va += chunk;
		pa += chunk;
	}
	return 0;
}

/* name unknown: a reserved range released (Sony's loop runs once: it steps by the whole size) */
void UnmapReservedVirPage(PartitionObject *partition, MemBlockObject *block,
			  const uintptr_t *vbase, const SceSize *size)
{
	AddressSpaceContext *context = partition->tiny.context;
	struct L2PageTable *l2pt;
	uintptr_t va = *vbase;
	SceSize rest;

	for (rest = *size; rest != 0; rest -= *size) {
		if (rest < 0x100000) {
			if (PartitionFindL2PageTable(partition, va, rest, 0, &l2pt) >= 0)
				L2PageTableUnreserve(l2pt, rest);
		} else {
			PartitionFindAndTryRemoveL2PageTable(partition, va, rest);
			L1PageTableUnmap(context->l1pt, block, rest, (SceUInt8)context->mmu.contextId,
					 partition->domain, va, rest, 1);
		}
		va += *vbase;
	}
}

/* name unknown: the unmapped pages at the start, in bytes */
SceSize MemBlockVirPageListGetHeadFreeSize(MemBlockObject *block)
{
	VirPage *vp;
	SceSize size = 0;

	for (vp = MemBlockGetVirPageRaw(block); vp != NULL; vp = vp->next) {
		switch (VIRPAGE_TYPE(vp)) {
		case VIRPAGE_TYPE_1:
		case VIRPAGE_TYPE_PHY:
		case VIRPAGE_TYPE_8:
			size += vp->size;
			break;
		default:
			return size;
		}
	}
	return size;
}

/* name unknown: the unmapped pages at the end, in bytes */
SceSize MemBlockVirPageListGetTailFreeSize(MemBlockObject *block)
{
	VirPage *vp;
	SceSize size = 0;

	for (vp = MemBlockGetVirPageRaw(block); vp != NULL; vp = vp->next) {
		switch (VIRPAGE_TYPE(vp)) {
		case VIRPAGE_TYPE_1:
		case VIRPAGE_TYPE_PHY:
		case VIRPAGE_TYPE_8:
			size += vp->size;
			break;
		default:
			size = 0;
			break;
		}
	}
	return size;
}

#if FW_DEBUG
/* debug builds only (absent from the releases) */

/* name unknown: the type 1 pages at the start (types 2 and 8 skipped), in bytes */
SceSize MemBlockVirPageListGetHeadType1Size(MemBlockObject *block)
{
	VirPage *vp;
	SceSize size = 0;

	for (vp = MemBlockGetVirPageRaw(block); vp != NULL; vp = vp->next) {
		switch (VIRPAGE_TYPE(vp)) {
		case VIRPAGE_TYPE_PHY:
		case VIRPAGE_TYPE_8:
			break;
		case VIRPAGE_TYPE_1:
			size += vp->size;
			break;
		default:
			return size;
		}
	}
	return size;
}

/* name unknown: the type 1 pages at the end (types 2 and 8 skipped), in bytes */
SceSize MemBlockVirPageListGetTailType1Size(MemBlockObject *block)
{
	VirPage *vp;
	SceSize size = 0;

	for (vp = MemBlockGetVirPageRaw(block); vp != NULL; vp = vp->next) {
		switch (VIRPAGE_TYPE(vp)) {
		case VIRPAGE_TYPE_PHY:
		case VIRPAGE_TYPE_8:
			break;
		case VIRPAGE_TYPE_1:
			size += vp->size;
			break;
		default:
			size = 0;
			break;
		}
	}
	return size;
}

#endif

/* name unknown: the mapped pages, in bytes */
SceSize MemBlockVirPageListGetMappedSize(MemBlockObject *block)
{
	VirPage *vp;
	SceSize size = 0;

	if ((block->flags & MEMBLOCK_KIND_MASK) != MEMBLOCK_KIND_VPL)
		return 0;
	vp = MemBlockGetVirPageRaw(block);
	if (vp == NULL)
		return 0;
	for (; vp != NULL; vp = vp->next)
		if (VIRPAGE_TYPE(vp) == VIRPAGE_TYPE_DIVIDED || VIRPAGE_TYPE(vp) == VIRPAGE_TYPE_16)
			size += vp->size;
	return size;
}

/* name unknown: the pages with physical memory behind them, in bytes */
SceSize MemBlockVirPageListGetPhySize(MemBlockObject *block)
{
	VirPage *vp;
	SceSize size = 0;

	if ((block->flags & MEMBLOCK_KIND_MASK) != MEMBLOCK_KIND_VPL)
		return 0;
	vp = MemBlockGetVirPageRaw(block);
	if (vp == NULL)
		return 0;
	for (; vp != NULL; vp = vp->next) {
		switch (VIRPAGE_TYPE(vp)) {
		case VIRPAGE_TYPE_DIVIDED:
			if (vp->page == NULL)
				break;
			/* fall through */
		case VIRPAGE_TYPE_16:
			size += vp->size;
			break;
		}
	}
	return size;
}

/* whether every mapped page lies in [pa, pa + size): 1 yes, 0 no */
int MemBlockVirPageListCheckWithinPhyRange(MemBlockObject *block, uintptr_t pa, SceSize size)
{
	VirPage *vp = MemBlockGetVirPageRaw(block);
	uintptr_t end = size + pa, pbase;
	SceUInt32 bit;

	if (vp == NULL)
		return 1;
	for (; vp != NULL; vp = vp->next) {
		if (VIRPAGE_TYPE(vp) - 1 >= 32)
			return SCE_KERNEL_ERROR_SYSMEM_MEMBLOCK_ERROR;
		bit = VIRPAGE_TYPE_BIT(VIRPAGE_TYPE(vp));
		if (bit & (VIRPAGE_TYPES_UNMAPPED | VIRPAGE_TYPE_BIT(32)))
			continue;
		if (bit & VIRPAGE_TYPE_BIT(16)) {
			if (vp->pbase < pa || end < vp->pbase + vp->size)
				return 0;
			continue;
		}
		if (!(bit & VIRPAGE_TYPE_BIT(4)))
			return SCE_KERNEL_ERROR_SYSMEM_MEMBLOCK_ERROR;
		if (vp->page != NULL) {
			if (!VplPageAllocated(vp->page)) {
				SCE_DBG_ASSERT_FAIL(267);
				return 0;
			}
			pbase = VplPagePbase(vp->page);
			if (pbase < pa)
				return 0;
			if (end < PhyPageGetSizeRaw(vp->page) + pbase)
				return 0;
		} else {
			if (vp->pbase < pa)
				return 0;
			if (end < vp->pbase + vp->size)
				return 0;
		}
	}
	return 1;
}

/* the physical ranges, the first 16 listed */
int MemBlockVirPageListGetInfo(MemBlockObject *block, MemBlockPhyInfo *info)
{
	VirPage *vp = MemBlockGetVirPageRaw(block);
	SceUInt32 bit, n;

	for (; vp != NULL; vp = vp->next) {
		if (VIRPAGE_TYPE(vp) - 1 >= 32)
			return SCE_KERNEL_ERROR_SYSMEM_MEMBLOCK_ERROR;
		bit = VIRPAGE_TYPE_BIT(VIRPAGE_TYPE(vp));
		if (bit & (VIRPAGE_TYPES_UNMAPPED | VIRPAGE_TYPE_BIT(32)))
			continue;
		if (!(bit & VIRPAGE_TYPE_BIT(16))) {
			if (!(bit & VIRPAGE_TYPE_BIT(4)))
				return SCE_KERNEL_ERROR_SYSMEM_MEMBLOCK_ERROR;
			if (vp->page != NULL) {
				if (!VplPageAllocated(vp->page)) {
					SCE_DBG_ASSERT_FAIL(333);
					return SCE_KERNEL_ERROR_BLOCK_ERROR;
				}
				n = info->rangeCount;
				if (n < 16) {
					info->pbase[n] = VplPagePbase(vp->page);
					info->size[n] = PhyPageGetSizeRaw(vp->page);
					info->rangeCount = n + 1;
				}
				info->count++;
				continue;
			}
		}
		n = info->rangeCount;
		if (n < 16) {
			info->pbase[n] = vp->pbase;
			info->size[n] = vp->size;
			info->rangeCount = n + 1;
		}
		info->count++;
	}
	return 0;
}

/* every page backed; on failure the pages backed so far are freed */
int MemBlockVirPageListAllocPhyPage(MemBlockObject *block, PartitionObject *partition,
				    void *phyPart)
{
	VirPage *vp, *next, *last;
	int ret;

	SCE_DBG_ASSERT((block->flags & MEMBLOCK_KIND_MASK) == MEMBLOCK_KIND_VPL, 919);
	for (vp = MemBlockGetVirPageRaw(block); vp != NULL; vp = next) {
		next = vp->next;
		ret = VirPageAllocPhyPage(vp, partition, phyPart, &last);
		if (ret < 0) {
			for (vp = MemBlockGetVirPageRaw(block); vp != NULL; vp = vp->next) {
				if (vp->page != NULL)
					PhyMemPartFreePhyPage(phyPart, vp->page);
				vp->page = NULL;
			}
			return ret;
		}
		/* a divided page: the pieces end before the next page */
		if (vp != last)
			last->next = next;
	}
	return 0;
}

/* a page with a preset physical range mapped */
static inline void AddMapVirPagePbase(PartitionObject *partition, MemBlockObject *block,
				      VirPage *vp)
{
	AddressSpaceContext *context = partition->tiny.context;
	SceSize size = vp->size;

	if (size >= 0x100000) {
		PartitionFindAndTryRemoveL2PageTable(partition, vp->vbase, size);
		L1PageTableMap(context->l1pt, block, block->code, partition->domain, size, vp->vbase,
			       vp->size, vp->pbase);
	} else if (size < 0x1000) {
		SCE_DBG_ASSERT_CHECK_FUNC(0, 705, "AddMapVirPagePbase");
	} else {
		PartitionMapPages(partition, block->code, vp->vbase, size, vp->pbase, 0);
	}
}

/*
 * Every page mapped. Cacheable pages are first mapped uncached and invalidated
 * (in the block's address space) so no stale lines stay behind.
 */
int MemBlockVirPageListMapRaw(MemBlockObject *block, const SceKernelAllocMemBlockCommand *command,
			      PartitionObject *partition, SceUInt32 flags)
{
	AddressSpaceContext *context;
	Partition *blockPartition;
	AddressSpaceObject *as;
	MMUContext saved;
	VirPage *vp;
	uintptr_t vbase, va, pa;
	SceSize size;
	SceUInt32 code;
	int invalidate, intr, ret = 0;

	if (block->attr & MEMBLOCK_ATTR_MAPPED) {
		SCE_DBG_PRINTF(6, 976, "PID=0x%08x:pBlock %p/[attr 0x%08x]/[%s] already mapped\n",
			       SceSysrootForKernel_7385CADE(), block, block->attr,
			       MemBlockGetName(block));
		return 0;
	}
	switch (MEMBLOCK_CODE_MEMORY_TYPE(block->code)) {
	case MEMBLOCK_CODE_SO:
	case MEMBLOCK_CODE_DEVICE:
	case MEMBLOCK_CODE_NORMAL_NC:
		invalidate = 3;
		break;
	case MEMBLOCK_CODE_L1WBWA:
		invalidate = 2;
		break;
	default:
		invalidate = 0;
		break;
	}
	if (flags & 1)
		invalidate = 0;
	for (vp = MemBlockGetVirPageRaw(block); vp != NULL; vp = vp->next) {
		switch (VIRPAGE_TYPE(vp)) {
		case VIRPAGE_TYPE_1:
		case VIRPAGE_TYPE_PHY:
		case VIRPAGE_TYPE_8:
			UnmapVirPageWithOpt(partition, block, vp, 1);
			break;
		case VIRPAGE_TYPE_DIVIDED:
			context = partition->tiny.context;
			size = vp->size;
			if (vp->page == NULL) {
				if (!(vp->flags & VIRPAGE_HAS_PBASE))
					return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
				pa = vp->pbase;
				va = vp->vbase;
			} else {
				va = vp->vbase;
				pa = VplPagePbase(vp->page);
				vp->pbase = pa;
				if (invalidate != 0) {
					code = block->code;
					block->code = (code & ~MEMBLOCK_CODE_MEMORY_TYPE_MASK) |
						      MEMBLOCK_CODE_L1WBWA;
					ret = MapVirPageCore(command, partition, &context->l1pt,
							     block, va, size, pa);
					if (ret < 0)
						return ret;
					vbase = block->vbase;
					blockPartition = block->partition;
					if (vbase < 0x40000000 || blockPartition == NULL ||
					    (as = blockPartition->addressSpace) == NULL ||
					    as->context->mmu.contextId == VplCurrentAsid()) {
						CacheInvalidateRangeWithPA((const void *)va, pa, size);
					} else {
						intr = ksceKernelCpuSuspendIntr();
						MMUContextSave(&saved);
						AddressSpaceSwitchContext(((Partition *)block->partition)->addressSpace);
						CacheInvalidateRangeWithPA((const void *)va, pa, size);
						MMUContextRestore(&saved);
						ksceKernelCpuResumeIntr(intr);
					}
					UnmapVirPageWithOpt(partition, block, vp, 0);
					block->code = code;
				}
			}
			ret = MapVirPageCore(command, partition, &context->l1pt, block, va,
					     size, pa);
			if (ret < 0)
				return ret;
			break;
		case VIRPAGE_TYPE_16:
			AddMapVirPagePbase(partition, block, vp);
			break;
		default:
			SCE_DBG_ASSERT_FAIL(1041);
			break;
		}
	}
	block->attr |= MEMBLOCK_ATTR_MAPPED;
	return ret;
}

/* every page unmapped; type 8 pages are released and also assert */
int MemBlockVirPageListUnmapRaw(MemBlockObject *block, PartitionObject *partition)
{
	VirPage *vp;

	for (vp = MemBlockGetVirPageRaw(block); vp != NULL; vp = vp->next) {
		switch (VIRPAGE_TYPE(vp)) {
		case VIRPAGE_TYPE_1:
		case VIRPAGE_TYPE_PHY:
			UnmapReservedVirPage(partition, block, &vp->vbase, &vp->size);
			break;
		case VIRPAGE_TYPE_DIVIDED:
		case VIRPAGE_TYPE_16:
			UnmapVirPageWithOpt(partition, block, vp, 0);
			break;
		case VIRPAGE_TYPE_8:
			UnmapReservedVirPage(partition, block, &vp->vbase, &vp->size);
			/* fall through */
		default:
			SCE_DBG_ASSERT_FAIL(1157);
			break;
		}
	}
	return 0;
}

/* a page's mappings changed to type */
static inline void RemapVirPage(PartitionObject *partition, MemBlockObject *block,
				SceUInt32 type, VirPage *vp, SceUInt32 flags)
{
	SceSize size = vp->size;
	AddressSpaceContext *context = partition->tiny.context;
	uintptr_t va, pa;
	SceSize chunk;

	if (vp->page == NULL) {
		if (!(vp->flags & VIRPAGE_HAS_PBASE))
			return;
		pa = vp->pbase;
	} else {
		pa = VplPagePbase(vp->page);
		vp->pbase = pa;
	}
	va = vp->vbase;
	while (size != 0) {
		chunk = VplChunk(va, pa, size);
		if (chunk >= 0x100000) {
			PartitionFindAndTryRemoveL2PageTable(partition, va, chunk);
			L1PageTableRemap(context->l1pt, block, type, partition->domain, chunk, va,
					 chunk, pa, flags);
		} else if (chunk < 0x1000) {
			SCE_DBG_ASSERT_CHECK_FUNC(0, 1120, "RemapVirPage");
		} else {
			PartitionRemapPages(partition, block, type, va, chunk, pa, flags);
		}
		size -= chunk;
		va += chunk;
		pa += chunk;
	}
}

int MemBlockVirPageListRemapRaw(MemBlockObject *block, PartitionObject *partition,
				SceUInt32 type, SceUInt32 flags)
{
	VirPage *vp;

	if (!(block->attr & MEMBLOCK_ATTR_MAPPED)) {
		SCE_DBG_PRINTF(6, 1178, "pBlock=%p/attr=0x%08x already unmapped\n", block,
			       block->attr);
		return 0;
	}
	for (vp = MemBlockGetVirPageRaw(block); vp != NULL; vp = vp->next) {
		switch (VIRPAGE_TYPE(vp)) {
		case VIRPAGE_TYPE_1:
		case VIRPAGE_TYPE_PHY:
		case VIRPAGE_TYPE_8:
		case VIRPAGE_TYPE_16:
			break;
		case VIRPAGE_TYPE_DIVIDED:
			RemapVirPage(partition, block, type, vp, flags);
			break;
		default:
			SCE_DBG_ASSERT_FAIL(1197);
			break;
		}
	}
	return 0;
}

/* the list's virtual pages back to the partition's heap */
int MemBlockVirPageListFreeVirPageCore(MemBlockObject *block, PartitionObject *partition)
{
	AddressSpaceContext *context = partition->tiny.context;
	VirPage *vp, *next;

	for (vp = MemBlockGetVirPageRaw(block); vp != NULL; vp = next) {
		next = vp->next;
		if (VirPageFree(partition, context->virPageHeap, vp) < 0)
			SCE_DBG_ASSERT_FAIL(1229);
	}
	MemBlockClearVirPageRaw(block);
	return 0;
}
