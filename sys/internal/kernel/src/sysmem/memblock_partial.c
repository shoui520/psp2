/*
 * SceSysmem: partial memblocks. A partial block's range is cut into units
 * that are allocated, mapped read-only or mapped read-write one by one. An
 * operation on a range works on a copy of the unit table and puts back the
 * units it already changed when one fails.
 */

#include "sysmem_internal.h"
#define MEMBLOCK_OBJ_MODULE_CALL
#include "memblock_obj_module.h"

/* a unit: its PhyPage (word aligned) and the mapping in bits 0..1 */
#define PARTIAL_UNIT_PAGE(_unit)	((_unit) & ~3u)
#define PARTIAL_UNIT_MAP(_unit)		((_unit) & 3)
#define PARTIAL_MAP_NONE		0
#define PARTIAL_MAP_RO			1
#define PARTIAL_MAP_RW			3

struct PhyPage;
/* l1pagetable.c (its memblock struct is still a placeholder there) */

/* the first physical page's address (PhyPage info: pindex in bits 0..19) */
static inline uintptr_t PartialPagePbase(const struct PhyPage *page)
{
	return *(const SceUInt32 *)page << 12;
}

/* units from start cleared over size bytes; the number cleared */
static int PartialInfoNullRange(PartialInfo *info, SceUInt32 start, uint32_t *units, SceSize size)
{
	SceUInt32 i = start;
	int count = 0;

	if (size != 0 && start < info->count) {
		do {
			size -= info->unitSize;
			i++;
			*units++ = 0;
			count = i - start;
		} while (size != 0 && i < info->count);
	}
	SCE_DBG_ASSERT_CHECK(i <= info->count, 167);
	return count;
}

/* name unknown: one unit's physical pages */
int PartialAllocUnit(MemBlockObject *block, SceSize unitSize, SceUInt32 *page)
{
	return PhyMemPartAllocPhyPage(MemBlockGetPhyMemPartRaw(block), unitSize, 0x10,
				      (struct PhyPage **)page);
}

/* name unknown: one unit's physical pages freed */
SYSMEM_DEBUG_STATIC int PartialFreeUnit(MemBlockObject *block, uint32_t *unit)
{
	int ret = PhyMemPartFreePhyPage(MemBlockGetPhyMemPartRaw(block),
					(struct PhyPage *)PARTIAL_UNIT_PAGE(*unit));

	if (ret >= 0)
		*unit = 0;
	return ret;
}

/* name unknown: a unit's mapping removed, from the L2 tables or the L1 table */
int PartialUnmapUnit(PartitionObject *partition, MemBlockObject *block, SceSize unitSize,
		     uintptr_t va)
{
	AddressSpaceContext *context;

	if (unitSize < 0x100000)
		return PartitionUnmapL2PageTable(partition, va, unitSize, 0, 0);
	context = partition->tiny.context;
	PartitionFindAndTryRemoveL2PageTable(partition, va, unitSize);
	return L1PageTableUnmap(context->l1pt, block, unitSize, (SceUInt8)context->mmu.contextId,
				partition->domain, va, unitSize, 0);
}

/* name unknown: a unit mapped with code's attributes */
int PartialMapUnit(PartitionObject *partition, MemBlockObject *block, SceUInt32 code,
		   SceSize unitSize, uintptr_t va, const struct PhyPage *page)
{
	AddressSpaceContext *context;
	SceUInt8 domain;

	if (unitSize < 0x100000)
		return PartitionMapPages(partition, code, va, unitSize, PartialPagePbase(page), 0);
	domain = partition->domain;
	context = partition->tiny.context;
	PartitionFindAndTryRemoveL2PageTable(partition, va, unitSize);
	return L1PageTableMap(context->l1pt, block, code, domain, unitSize, va, unitSize,
			      PartialPagePbase(page));
}

/* name unknown: mapped read-only (the block's code without its write bit) */
int PartialMapUnitRO(PartitionObject *partition, MemBlockObject *block, SceSize unitSize,
		     uint32_t *unit, uintptr_t va)
{
	SceUInt32 code = block->code;
	int ret;

	switch (code & 0xff) {
	case 0x06:
		code &= ~0x02;
		break;
	case 0x60:
		code &= ~0x20;
		break;
	default:
		return SCE_KERNEL_ERROR_PARTITION_ERROR;
	}
	if (code == 0)
		return SCE_KERNEL_ERROR_PARTITION_ERROR;
	ret = PartialMapUnit(partition, block, code, unitSize, va,
			     (const struct PhyPage *)PARTIAL_UNIT_PAGE(*unit));
	if (ret < 0)
		return ret;
	*unit = PARTIAL_UNIT_PAGE(*unit) | PARTIAL_MAP_RO;
	return ret;
}

/* name unknown: mapped read-write */
int PartialMapUnitRW(PartitionObject *partition, MemBlockObject *block, SceSize unitSize,
		     uint32_t *unit, uintptr_t va)
{
	SceUInt32 code = block->code;
	int ret;

	switch (code & 0xff) {
	case 0x06:
	case 0x60:
		break;
	default:
		return SCE_KERNEL_ERROR_MEMBLOCK_NOT_ALLOW_REMAP_OP;
	}
	ret = PartialMapUnit(partition, block, code, unitSize, va,
			     (const struct PhyPage *)PARTIAL_UNIT_PAGE(*unit));
	if (ret < 0)
		return ret;
	*unit |= PARTIAL_MAP_RW;
	return ret;
}

/* one unit through the operation */
static inline int PartialOpChangeState(PartitionObject *partition, MemBlockObject *block,
				       SceSize unitSize, uint32_t *unit, uintptr_t va,
				       SceUInt32 op)
{
	SceUInt32 page;
	int ret;

	switch (op) {
	case PARTIAL_OP_ALLOC:
		if (*unit != 0)
			return 0;
		ret = PartialAllocUnit(block, unitSize, &page);
		if (ret < 0)
			return ret;
		*unit = PARTIAL_UNIT_PAGE(page);
		return ret;
	case PARTIAL_OP_FREE:
		if (*unit == 0)
			return 0;
		if (PARTIAL_UNIT_MAP(*unit) != PARTIAL_MAP_NONE) {
			ret = PartialUnmapUnit(partition, block, unitSize, va);
			if (ret < 0)
				return ret;
			*unit = PARTIAL_UNIT_PAGE(*unit);
		}
		return PartialFreeUnit(block, unit);
	case PARTIAL_OP_ALLOC_MAP:
		if (*unit == 0) {
			ret = PartialAllocUnit(block, unitSize, &page);
			if (ret < 0)
				return ret;
			*unit = PARTIAL_UNIT_PAGE(page);
			if (*unit == 0)
				return ret;
		}
		switch (block->code & 0xff) {
		case 0x06:
		case 0x60:
			return PartialMapUnitRW(partition, block, unitSize, unit, va);
		case 0x05:
		case 0x50:
			return PartialMapUnitRO(partition, block, unitSize, unit, va);
		default:
			return SCE_KERNEL_ERROR_ERROR;
		}
	case PARTIAL_OP_UNMAP:
		if (PARTIAL_UNIT_MAP(*unit) != PARTIAL_MAP_NONE) {
			ret = PartialUnmapUnit(partition, block, unitSize, va);
			if (ret < 0)
				return ret;
			*unit = PARTIAL_UNIT_PAGE(*unit);
		}
		return 0;
	case PARTIAL_OP_RW_TO_RO:
		if (PARTIAL_UNIT_MAP(*unit) != PARTIAL_MAP_RW)
			return 0;
		ret = PartialUnmapUnit(partition, block, unitSize, va);
		if (ret < 0)
			return ret;
		*unit = PARTIAL_UNIT_PAGE(*unit);
		return PartialMapUnitRO(partition, block, unitSize, unit, va);
	case PARTIAL_OP_RO_TO_RW:
		if (PARTIAL_UNIT_MAP(*unit) != PARTIAL_MAP_RO)
			return 0;
		ret = PartialUnmapUnit(partition, block, unitSize, va);
		if (ret < 0)
			return ret;
		*unit = PARTIAL_UNIT_PAGE(*unit);
		return PartialMapUnitRW(partition, block, unitSize, unit, va);
	default:
		SCE_PANIC_FUNC(527, "PartialOpChangeState");
	}
}

/*
 * A unit changed by a failed operation back to its old state. Note Sony's
 * fallbacks: a read-write unit that cannot be mapped read-write again is
 * mapped read-only, and a read-only unit that cannot be unmapped from
 * read-write is mapped read-write once more.
 */
static inline void PartialUnitRestore(PartitionObject *partition, MemBlockObject *block,
				      SceSize unitSize, SceUInt32 old, uint32_t *unit,
				      uintptr_t va)
{
	SceUInt32 page;

	if (old == 0) {
		if (*unit != 0)
			PartialFreeUnit(block, unit);
		return;
	}
	switch (PARTIAL_UNIT_MAP(old)) {
	case PARTIAL_MAP_NONE:
		if (*unit == 0) {
			if (PartialAllocUnit(block, unitSize, &page) >= 0)
				*unit = PARTIAL_UNIT_PAGE(page);
		} else if (PARTIAL_UNIT_MAP(*unit) != PARTIAL_MAP_NONE) {
			if (PartialUnmapUnit(partition, block, unitSize, va) >= 0)
				*unit = PARTIAL_UNIT_PAGE(*unit);
		}
		break;
	case PARTIAL_MAP_RW:
		if (*unit == 0) {
			if (PartialAllocUnit(block, unitSize, &page) < 0)
				break;
			*unit = PARTIAL_UNIT_PAGE(page);
		}
		if (PARTIAL_UNIT_MAP(*unit) == PARTIAL_MAP_NONE) {
			PartialMapUnitRW(partition, block, unitSize, unit, va);
		} else if (PARTIAL_UNIT_MAP(*unit) != PARTIAL_MAP_RW &&
			   PartialUnmapUnit(partition, block, unitSize, va) >= 0) {
			*unit = PARTIAL_UNIT_PAGE(*unit);
			if (PartialMapUnitRW(partition, block, unitSize, unit, va) < 0)
				PartialMapUnitRO(partition, block, unitSize, unit, va);
		}
		break;
	default:
		if (*unit == 0) {
			if (PartialAllocUnit(block, unitSize, &page) < 0)
				break;
			*unit = PARTIAL_UNIT_PAGE(page);
		}
		if (PARTIAL_UNIT_MAP(*unit) == PARTIAL_MAP_NONE) {
			PartialMapUnitRO(partition, block, unitSize, unit, va);
		} else if (PARTIAL_UNIT_MAP(*unit) == PARTIAL_MAP_RW) {
			if (PartialUnmapUnit(partition, block, unitSize, va) < 0) {
				PartialMapUnitRW(partition, block, unitSize, unit, va);
			} else {
				*unit = PARTIAL_UNIT_PAGE(*unit);
				PartialMapUnitRO(partition, block, unitSize, unit, va);
			}
		}
		break;
	}
}

/* every unit of [va, va + size) checked, then put through the operation */
int PartialInfoOpRange(PartitionObject *partition, MemBlockObject *block, PartialInfo *info,
		       SceUInt32 op, uintptr_t va, SceSize size)
{
	SceSize unitSize = info->unitSize, rest;
	uint32_t *units, *shadow;
	SceUInt32 first, count, i, j;
	uintptr_t vbase, unitVa;
	int intr, ret = 0;

	intr = SceCpuForDriver_4C38CE4D(&info->lock);
	vbase = block->vbase;
	SCE_DBG_ASSERT_CHECK(va >= vbase, 594);
	units = info->units;
	first = (va - vbase) >> (31 - __builtin_clz(unitSize));
	shadow = info->shadow;
	count = info->count;

	if (op & PARTIAL_CHECK_MASK) {
		for (i = first, rest = size; i < count; i++) {
			switch (op & PARTIAL_CHECK_MASK) {
			case SCE_KERNEL_MEMBLOCK_PARTIAL_CHECK_ALLOCATED:
				if (units[i] == 0) {
					SCE_DBG_PRINTF(0xb, 612,
						       "SCE_KERNEL_MEMBLOCK_PARTIAL_CHECK_ALLOCATED failed\n");
					ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
					goto out;
				}
				break;
			case SCE_KERNEL_MEMBLOCK_PARTIAL_CHECK_FREE:
				if (units[i] != 0) {
					SCE_DBG_PRINTF(0xb, 619,
						       "SCE_KERNEL_MEMBLOCK_PARTIAL_CHECK_FREE:ui=%d failed\n",
						       i);
					ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
					goto out;
				}
				break;
			case SCE_KERNEL_MEMBLOCK_PARTIAL_CHECK_MAPPED:
				if (PARTIAL_UNIT_MAP(units[i]) == PARTIAL_MAP_NONE) {
					SCE_DBG_PRINTF(0xb, 626,
						       "SCE_KERNEL_MEMBLOCK_PARTIAL_CHECK_MAPPED failed\n");
					ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
					goto out;
				}
				break;
			case SCE_KERNEL_MEMBLOCK_PARTIAL_CHECK_ALLOCNOMAP:
				if (PARTIAL_UNIT_MAP(units[i]) != PARTIAL_MAP_NONE) {
					SCE_DBG_PRINTF(0xb, 633,
						       "SCE_KERNEL_MEMBLOCK_PARTIAL_CHECK_ALLOCNOMAP failed\n");
					ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
					goto out;
				}
				if (units[i] == 0) {
					SCE_DBG_PRINTF(0xb, 638,
						       "SCE_KERNEL_MEMBLOCK_PARTIAL_CHECK_ALLOCATED failed\n");
					ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
					goto out;
				}
				break;
			case SCE_KERNEL_MEMBLOCK_PARTIAL_CHECK_MAPPED_RW:
				if (PARTIAL_UNIT_MAP(units[i]) != PARTIAL_MAP_RW) {
					SCE_DBG_PRINTF(0xb, 645,
						       "SCE_KERNEL_MEMBLOCK_PARTIAL_CHECK_MAPPED_RW failed\n");
					ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
					goto out;
				}
				break;
			case SCE_KERNEL_MEMBLOCK_PARTIAL_CHECK_MAPPED_RO:
				if (PARTIAL_UNIT_MAP(units[i]) != PARTIAL_MAP_RO) {
					SCE_DBG_PRINTF(0xb, 652,
						       "SCE_KERNEL_MEMBLOCK_PARTIAL_CHECK_MAPPED_RO failed\n");
					ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
					goto out;
				}
				break;
			default:
				SCE_PANIC(658);
			}
			rest -= unitSize;
			if (rest == 0)
				break;
		}
	}

	/* the operation works on a copy, which becomes the table when it all succeeds */
	for (i = 0; i < count; i++)
		shadow[i] = units[i];
	unitVa = unitSize * first + vbase;
	for (i = first, rest = size; i < info->count; i++, unitVa += unitSize) {
		ret = PartialOpChangeState(partition, block, unitSize, &shadow[i], unitVa,
					   op & PARTIAL_OP_MASK);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 684, "Operation failed:res=0x%08x\n", ret);
			unitVa = unitSize * first + vbase;
			for (j = first; j < i; j++, unitVa += unitSize)
				PartialUnitRestore(partition, block, unitSize, units[j], &shadow[j],
						   unitVa);
			goto out;
		}
		rest -= unitSize;
		if (rest == 0)
			break;
	}
	info->units = shadow;
	info->shadow = units;
out:
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&info->lock, intr);
	return ret;
}

/* name unknown: a block's unit table, the unit size from the options or 4 KiB */
int MemBlockPartialInfoCreate(MemBlockObject *block, SceKernelAllocMemBlockCommand *command)
{
	PartialInfo *info;
	SceSize unitSize;

	if (command->opt.attr & ALLOC_ATTR_HAS_UNIT_SIZE)
		unitSize = command->opt.unitSize;
	else
		unitSize = 0x1000;
	info = SceSysmemForKernel_85571907(sizeof(*info));
	if (info == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	info->unitSize = unitSize;
	info->lock = 0;
	info->count = command->size >> (31 - __builtin_clz(unitSize));
	info->units = SceSysmemForKernel_85571907(info->count * sizeof(*info->units));
	if (info->units != NULL) {
		info->shadow = SceSysmemForKernel_85571907(info->count * sizeof(*info->shadow));
		if (info->shadow != NULL) {
			MemBlockSetPartialInfoRaw(block, info);
			return 0;
		}
		if (info->units != NULL)
			SceSysmemForKernel_4233C16D(info->units);
	}
	SceSysmemForKernel_4233C16D(info);
	return SCE_ERROR_ERRNO_ENOMEM;
}

/* the units' physical pages, the extra low and high parts left without */
int MemBlockPartialAllocPhyPage(MemBlockObject *block, SceKernelAllocMemBlockCommand *command)
{
	PartialInfo *info = MemBlockGetPartialInfoRaw(block);
	uint32_t *units = info->units;
	SceUInt32 page;
	SceUInt32 done = 0;
	SceSize low = 0, high = 0, rest;
	int ret;

	if ((command->opt.attr & ALLOC_ATTR_HAS_EXTRA_LOW) && command->opt.extraLow != 0) {
		done = PartialInfoNullRange(info, 0, units, command->opt.extraLow);
		units += done;
	}
	if (command->opt.attr & ALLOC_ATTR_HAS_EXTRA_LOW)
		low = command->opt.extraLow;
	if (command->opt.attr & ALLOC_ATTR_HAS_EXTRA_HIGH)
		high = command->opt.extraHigh;
	for (rest = command->size - low - high; rest != 0; rest -= info->unitSize) {
		ret = PhyMemPartAllocPhyPage(command->phyMemPart, info->unitSize, 0x10,
					     (struct PhyPage **)&page);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 852, "PhyMemPartAllocPhyPage failed 0x%08x\n", ret);
			return ret;
		}
		done++;
		*units++ = PARTIAL_UNIT_PAGE(page);
	}
	if ((command->opt.attr & ALLOC_ATTR_HAS_EXTRA_HIGH) && command->opt.extraHigh != 0)
		PartialInfoNullRange(info, done, units, command->opt.extraHigh);
	return 0;
}

/* every unit allocated and mapped */
int MemBlockPartialMapRaw(MemBlockObject *block, SceKernelAllocMemBlockCommand *command)
{
	PartitionObject *partition = block->partition;
	int ret;

	ret = PartialInfoOpRange(partition, block, MemBlockGetPartialInfoRaw(block),
				 PARTIAL_OP_ALLOC_MAP, block->vbase, block->size);
	if (ret >= 0) {
		command->block->attr |= 0x40;
		return ret;
	}
	SCE_DBG_PRINTF(0xb, 882, "PartialInfoOpRange failed 0x%08x\n", ret);
	return ret;
}

/* name unknown: every unit unmapped */
int MemBlockPartialUnmapRaw(MemBlockObject *block, PartitionObject *partition)
{
	return PartialInfoOpRange(partition, block, MemBlockGetPartialInfoRaw(block),
				  PARTIAL_OP_UNMAP, block->vbase, block->size);
}

/* name unknown: every unit freed */
int MemBlockPartialFreeRaw(MemBlockObject *block, PartitionObject *partition)
{
	return PartialInfoOpRange(partition, block, MemBlockGetPartialInfoRaw(block),
				  PARTIAL_OP_FREE, block->vbase, block->size);
}

/* name unknown */
int MemBlockPartialOpRange(PartitionObject *partition, MemBlockObject *block, SceUInt32 op,
			   uintptr_t va, SceSize size)
{
	return PartialInfoOpRange(partition, block, MemBlockGetPartialInfoRaw(block), op, va,
				  size);
}

/* name unknown: the free units at the start, in bytes */
SceSize MemBlockPartialGetHeadFreeSize(MemBlockObject *block)
{
	PartialInfo *info = MemBlockGetPartialInfoRaw(block);
	SceUInt32 count = info->count, *units = info->units;
	SceSize unitSize = info->unitSize, size = 0;
	SceUInt32 i;

	for (i = 0; i < count; i++) {
		if (units[i] != 0)
			break;
		size += unitSize;
	}
	return size;
}

/* name unknown: from the last allocated unit to the end (all of it when none is), in bytes */
SceSize MemBlockPartialGetTailSize(MemBlockObject *block)
{
	PartialInfo *info = MemBlockGetPartialInfoRaw(block);
	SceUInt32 count = info->count, *units = info->units;
	SceSize unitSize = info->unitSize, size = 0;
	int i;

	for (i = 0; i < count; i++) {
		if (*units++ != 0)
			size = 0;
		size += unitSize;
	}
	return size;
}

/* name unknown: the mapped units, in bytes */
SceSize MemBlockPartialGetMappedSize(MemBlockObject *block)
{
	PartialInfo *info = MemBlockGetPartialInfoRaw(block);
	SceUInt32 count = info->count, *units = info->units;
	SceSize unitSize = info->unitSize, size = 0;
	int i;

	for (i = 0; i < count; i++)
		if (PARTIAL_UNIT_MAP(*units++) != PARTIAL_MAP_NONE)
			size += unitSize;
	return size;
}

/* name unknown: the allocated units, in bytes */
SceSize MemBlockPartialGetAllocatedSize(MemBlockObject *block)
{
	PartialInfo *info = MemBlockGetPartialInfoRaw(block);
	SceUInt32 count = info->count, *units = info->units;
	SceSize unitSize = info->unitSize, size = 0;
	int i;

	for (i = 0; i < count; i++)
		if (*units++ != 0)
			size += unitSize;
	return size;
}
