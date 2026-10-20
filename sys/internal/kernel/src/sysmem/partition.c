/*
 * SceSysmem: memory partitions. A tiny partition is an address range with a
 * sorted list of the memblocks placed in it; a partition adds the L2 page
 * tables of its address space's range (one per MiB, 1 KiB each, carved from
 * simple memblocks four at a time) and a domain.
 */

#include "sysmem_internal.h"
#include <sysmem/uid_guid.h>
#include "memblock_obj_module.h"
#include "memblock/status_module.h"
#include <sysmem/dipsw.h>
#include <sysmem/utils.h>

/* MemBlockObject flags bits 20..22 */

/* an L2 table slot of the address space context: an L2PageTable | 1, or a section owner | 2 */
#define AS_L2_SLOT_TYPE(_slot)		((_slot) & 3)
#define AS_L2_SLOT_PTR(_slot)		((_slot) & ~3u)
#define AS_L2_SLOT_TABLE		1
#define AS_L2_SLOT_BLOCK		2

/* SceUIDPartitionClass */
static SceClass *s_partitionClass;

/* l1pagetable.c, l2pagetable.c, smb.c, fixed_heap.c, virpage.c */
/* memblock_obj.c */
/* memblock_vpl.c, memblock_partial.c, phymem.c */
/* memblock.c */

/*
 * An L2 page table released: unhooked from the L1 table and its quarter of
 * the holding simple memblock marked free; the block itself goes once all
 * four quarters are free.
 */
int PartitionAsCommonFreeL2PageTable(Partition *partition, AddressSpaceContext *context,
				     L2PageTable *l2pt, SceUInt32 flags)
{
	MemBlockObject *block, *prev, *cur;
	int intr;

	L1PageTableUnsetL2PageTable(context->l1pt, l2pt->vbase);
	block = l2pt->block;
	switch (l2pt->flags & 0xf) {
	case 1:
		MemBlockL2PageTableSetFreeRaw(block, 0x1000000);
		break;
	case 2:
		MemBlockL2PageTableSetFreeRaw(block, 0x2000000);
		break;
	case 4:
		MemBlockL2PageTableSetFreeRaw(block, 0x4000000);
		break;
	case 8:
		MemBlockL2PageTableSetFreeRaw(block, 0x8000000);
		break;
	default:
		SCE_DBG_ASSERT_FAIL(6064);
		break;
	}
	if (MemBlockL2PageTableIsAllFree(block) == 1) {
		/* off the partition's list of L2 table blocks (linked through smbNext) */
		intr = SceCpuForDriver_4C38CE4D(&partition->l2BlockLock);
		for (prev = NULL, cur = partition->l2Blocks; cur != NULL;
		     prev = cur, cur = cur->smbNext) {
			if (block != cur)
				continue;
			if (prev != NULL)
				prev->smbNext = block->smbNext;
			else
				partition->l2Blocks = block->smbNext;
		}
		sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&partition->l2BlockLock, intr);
		if (partition != context->partition) {
			FreeSimpleMemBlock(block, 0);
			return L2PageTableFree(context->l2ptHeap, l2pt);
		}
		FreeSimpleMemBlock(block, flags);
	}
	return L2PageTableFree(context->l2ptHeap, l2pt);
}

/* name unknown: a new virtual page from the address space's heap */
int PartitionAllocVirPageRaw(Partition *partition, VirPage **vp)
{
	void *object;
	int ret;

	ret = FixedHeapAllocObjectExt(partition->tiny.context->virPageHeap, &object);
	if (ret >= 0) {
		VirPageInit(object);
		SceCpuForKernel_9DA58A9E((uint32_t *)&partition->virPageCount);
		*vp = object;
	}
	return ret;
}

/* name unknown: the used block holding va (or exactly [va, va + size)), from block on */
int TinyPartitionFindBlockFrom(TinyPartition *tiny, uintptr_t va, SceSize size,
			       MemBlockObject *block, MemBlockObject **found)
{
	MemBlockObject *head = tiny->blocks;

	for (; block != head; block = block->next) {
		uintptr_t base = block->vbase;

		if (size == 0) {
			if (va < base)
				break;
			if (va - base < block->size)
				goto check;
		} else {
			if (va == base) {
				if (size == block->size)
					goto check;
				MemBlockGetName(block);
				base = block->vbase;
			}
			if (va < base)
				break;
		}
	}
notFound:
	*found = NULL;
	return SCE_KERNEL_ERROR_ERROR;
check:
	if (block->code == 0)
		goto notFound;
	*found = block;
	return 0;
}

int MemBlockUnmapRaw(MemBlockObject *block, Partition *partition)
{
	int ret;

	if (!(block->attr & MEMBLOCK_ATTR_MAPPED)) {
		SCE_DBG_PRINTF(6, 1666, "pBlock=%p/attr=0x%08x already unmapped\n", block,
			       block->attr);
		return 0;
	}
	if ((block->flags & MEMBLOCK_KIND_MASK) == MEMBLOCK_KIND_VPL) {
		ret = MemBlockVirPageListUnmapRaw(block, partition);
	} else if (block->attr & MEMBLOCK_ATTR_PARTIAL) {
		ret = MemBlockPartialUnmapRaw(block, partition);
	} else {
		SCE_DBG_PRINTF(0xb, 1680, "Warning\n");
		SCE_DBG_ASSERT_FAIL(1681);
		ret = 0;
	}
	block->attr &= ~MEMBLOCK_ATTR_MAPPED;
	return ret;
}

/*
 * name unknown: the info of the used blocks based in [lo, hi) (with flags
 * bit 0, only those within [pa, pa + psize)); *count of them filled, the
 * number of used blocks returned.
 */
int TinyPartitionGetMemBlockInfoList(TinyPartition *tiny, SceUInt32 flags, uintptr_t lo,
				     uintptr_t hi, uintptr_t pa, SceSize psize, SceUInt32 arg,
				     MemBlockInfoEx *info, SceSize infoSize, SceUInt32 *count)
{
	MemBlockObject *block, *head;
	SceUInt32 filled, byPhy;
	int intr, used;

	byPhy = flags & 1;
	filled = 0;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&tiny->lock);
	head = tiny->blocks;
	used = 0;
	block = head;
	while (block != NULL) {
		if (block->code != 0) {
			if (filled < infoSize / sizeof(*info)) {
				uintptr_t vbase = block->vbase;

				if (lo <= vbase && hi > vbase &&
				    (byPhy == 0 || pa == 0 ||
				     MemBlockWithinPhyRange(block, pa, psize) == 1)) {
					if (MemBlockGetInfoRaw(block, arg, info) < 0)
						goto next;
					filled++;
					info++;
				}
			}
			used++;
		}
next:
		block = block->next;
		if (head == block)
			break;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&tiny->lock, intr);
	*count = filled;
	return used;
}

/* the CDRAM codes, freed through the CDRAM partitions */
static inline int MemBlockCodeIsCdram(SceUInt32 code)
{
	return code == MEMBLOCK_CODE_KERNEL_CDRAM_RW || code == MEMBLOCK_CODE_KERNEL_CDRAM_L1WBWA_RW ||
	       code == MEMBLOCK_CODE_USER_CDRAM_L1WBWA_RW || code == MEMBLOCK_CODE_USER_CDRAM_R ||
	       code == MEMBLOCK_CODE_USER_CDRAM_RW;
}

/* the physical address behind a CDRAM block's address */
static inline uintptr_t MemBlockCdramPbase(SceUInt32 code, uintptr_t vbase)
{
	switch (code) {
	case MEMBLOCK_CODE_USER_CDRAM_L1WBWA_RW:
	case MEMBLOCK_CODE_USER_CDRAM_R:
	case MEMBLOCK_CODE_USER_CDRAM_RW:
		return vbase < 0x61000000 ? vbase + 0xc7000000 : vbase + 0xbf000000;
	case MEMBLOCK_CODE_KERNEL_CDRAM_L1WBWA_RW:
	case MEMBLOCK_CODE_KERNEL_CDRAM_RW:
		return vbase < 0x21000000 ? vbase + 0x07000000 : vbase - 0x01000000;
	default:
		return vbase;
	}
}

/* a block unmapped and its memory freed, back to virtually allocated */
int PartitionFreeMemBlockMappedToVirAllocRaw(Partition *partition, MemBlockObject *block)
{
	void *phyPart;
	uintptr_t pa;
	SceUInt32 code;
	int ret;

	if (block->attr & MEMBLOCK_ATTR_MAPPED) {
		MemBlockUnmapRaw(block, partition);
		if (partition == NULL) {
			SCE_DBG_ASSERT_FAIL(5732);
			return SCE_KERNEL_ERROR_PARTITION_ERROR;
		}
	}
	MemBlockStatusStateChangePhyAllocFromMA(block);
	code = block->code;
	if (MemBlockCodeIsCdram(code)) {
		if (block->attr & MEMBLOCK_ATTR_KEEP_PHY) {
			SCE_DBG_PRINTF(0xb, 5742, "SKIP free physical block [0x%08x]:vbase=0x%08x\n",
				       block->attr, block->vbase);
		} else {
			pa = MemBlockCdramPbase(code, block->vbase);
			phyPart = MemBlockGetPhyMemPartRaw(block);
			if (phyPart == NULL)
				PhyMemCdramFree(pa, block->size);
			else if (PhyMemPartCdramFree(phyPart, pa, block->size) < 0)
				SCE_DBG_PRINTF(0xb, 5798, "PhyMemPartCdramFree failed\n");
		}
	} else if (((block->flags & MEMBLOCK_OPT_MASK) != MEMBLOCK_OPT_BASE_BLOCK &&
		    (block->flags & MEMBLOCK_OPT_MASK) != MEMBLOCK_OPT_BASE_INFO) ||
		   !(block->attr & MEMBLOCK_ATTR_KEEP_PHY)) {
		phyPart = MemBlockGetPhyMemPartRaw(block);
		if (block->attr & MEMBLOCK_ATTR_PARTIAL) {
			MemBlockPartialFreeRaw(block, partition);
		} else {
			ret = MemBlockFreePhyPages(block, phyPart);
			if (ret < 0) {
				SCE_DBG_ASSERT_FAIL(5824);
				return ret;
			}
		}
	}
	MemBlockStatusStateChangeLisAllocFromPARaw(block);
	if ((block->flags & MEMBLOCK_KIND_MASK) == MEMBLOCK_KIND_VPL) {
		ret = MemBlockVirPageListFreeVirPageCore(block, partition);
		if (ret < 0) {
			SCE_DBG_ASSERT_FAIL(5835);
			return ret;
		}
	} else {
		ret = 0;
	}
	MemBlockStatusStateChangeVirAllocFromLARaw(block);
	return ret;
}

/*
 * name unknown: a block leaving the list: the MiB slots naming it as their
 * first block (L2 tables) or section owner now name the next used block in
 * that MiB, or nothing.
 */
void PartitionUnlinkBlockSlots(Partition *partition, MemBlockObject *block)
{
	MemBlockObject *next, *self = (MemBlockObject *)((uintptr_t)block & ~3u);
	L2PageTable *l2pt;
	SceUInt32 *slot, value, mib, last;
	int intr;

	intr = SceCpuForDriver_4C38CE4D(&partition->l2Lock);
	mib = block->vbase >> 20;
	last = (block->size + block->vbase - 1) >> 20;
	for (slot = &partition->tiny.context->l2Tables[mib]; mib <= last; mib++, slot++) {
		value = *slot;
		if (AS_L2_SLOT_TYPE(value) == AS_L2_SLOT_TABLE) {
			l2pt = (L2PageTable *)AS_L2_SLOT_PTR(value);
			if (l2pt->firstBlock != block)
				continue;
			for (next = block; ; ) {
				next = next->next;
				if (next == block || next->vbase >> 20 != mib) {
					l2pt->firstBlock = NULL;
					break;
				}
				if (next->code != 0) {
					l2pt->firstBlock = next;
					break;
				}
			}
		} else if (AS_L2_SLOT_TYPE(value) == AS_L2_SLOT_BLOCK &&
			   ((uintptr_t)self | AS_L2_SLOT_BLOCK) == value) {
			value = 0;
			for (next = self; ; ) {
				next = next->next;
				if (next == self || next->vbase >> 20 != mib)
					break;
				if (next->code != 0) {
					value = ((uintptr_t)next & ~3u) | AS_L2_SLOT_BLOCK;
					break;
				}
			}
			*slot = value;
		}
	}
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&partition->l2Lock, intr);
}

static inline void TinyPartitionSetFreeSizeR1(TinyPartition *tiny, SceSize size)
{
	tiny->freeSize = size;
	SCE_DBG_ASSERT_CHECK_FUNC_RELOAD(size >= tiny->freeSize, 2780, "TinyPartitionSetFreeSizeR1");
}

SYSMEM_DEBUG_FUNC int TinyPartitionInitRaw(TinyPartition *tiny, const char *name, uintptr_t vbase,
					   SceSize size, AddressSpaceContext *context)
{
	if (vbase & 0xffffff) {
		SCE_DBG_ASSERT_FAIL(3051);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (size >= 0x1000000 && (size << 8) != 0) {
		SCE_DBG_ASSERT_FAIL(3055);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	tiny->vbase = vbase;
	tiny->size = size;
	tiny->name = name;
	tiny->context = context;
	tiny->freeSize = size;
	tiny->magic = TINY_PARTITION_MAGIC;
	tiny->lock = 0;
	tiny->reserved0 = 0;
	tiny->blockCount = 0;
	tiny->attr = 0xffffffff;
	tiny->reserved1 = 0xffffffff;
	TinyPartitionSetFreeSizeR1(tiny, size);
	return 0;
}

/* SceKernelAllocMemBlockCommand cmdFlags (+0x90) */
#define ALLOC_CMD_MIRROR		0x01
#define ALLOC_CMD_PHY_ANY		0x04
#define ALLOC_CMD_PADDR_LIST		0x08
#define ALLOC_CMD_PADDR			0x10
#define ALLOC_CMD_PARTIAL		0x20

/* the codes that must be physically contiguous */
static inline void AllocMemBlockCommandCheck2(SceKernelAllocMemBlockCommand *command)
{
	switch (command->code) {
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_R:
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_RW:
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_R:
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW:
		if (!(command->opt.attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT))
			SceDebugForDriver_FD753E7A(1, 0,
					      SCE_DBGINFO_FUNC(530, "AllocMemBlockCommandCheck2"),
					      __builtin_return_address(0));
		break;
	}
}

/* an allocation's parameters checked and completed */
int AllocMemBlockCommandCheckParameter(SceKernelAllocMemBlockCommand *command)
{
	MemBlockObject *mirror;
	SceUInt32 attr, align;
	SceSize size;

	if (command->partition == NULL) {
		SCE_DBG_ASSERT_FAIL(551);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (command->opt.size != sizeof(command->opt))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	if (command->code == 0) {
		SCE_DBG_ASSERT_FAIL(562);
		return SCE_KERNEL_ERROR_ILLEGAL_MEMBLOCK_TYPE;
	}
	if (command->typeInfo->attr & 0x100)
		command->opt.attr |= SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT;
#if FW_DEBUG
	if (SceDebugForDriver_374B7868() >= 2) {
		AllocMemBlockCommandCheck2(command);
		SceDebugForDriver_FD753E7A(2, 1, SCE_DBGINFO(579), __builtin_return_address(0));
	}
#endif
	attr = command->opt.attr;
	if (attr & ALLOC_ATTR_SHARE_PHYPAGE) {
		if (!(attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_MIRROR_BLOCKID))
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (attr & ALLOC_ATTR_SHARE_VBASE) {
		if (!(attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_MIRROR_BLOCKID))
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	mirror = NULL;
	if (attr & ALLOC_ATTR_SHARE_PHYPAGE) {
		if (!(attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_MIRROR_BLOCKID))
			return SCE_KERNEL_ERROR_PARTITION_ERROR;
		command->cmdFlags |= ALLOC_CMD_MIRROR;
		goto lookup;
	}
	if ((command->code & 0xf00000) == 0x100000) {
		if (attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR) {
			command->cmdFlags |= ALLOC_CMD_PADDR;
		} else {
			SCE_DBG_ASSERT_FAIL(621);
			attr = command->opt.attr;
		}
	} else if (attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR) {
		command->cmdFlags |= ALLOC_CMD_PADDR;
	} else if (attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR_LIST) {
		command->cmdFlags |= ALLOC_CMD_PADDR_LIST;
	} else if (attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_ALLOW_PARTIAL_OP) {
		command->cmdFlags |= ALLOC_CMD_PARTIAL;
	} else {
		command->cmdFlags |= ALLOC_CMD_PHY_ANY;
	}
	if (attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_MIRROR_BLOCKID) {
lookup:
		mirror = SceSysmemForKernel_FA6C098C(command->opt.mirror_blockid);
		attr = command->opt.attr;
	}
	size = command->size;
	if (size == 0) {
		if (!(attr & ALLOC_ATTR_SHARE_PHYPAGE)) {
			SCE_DBG_PRINTF(0xb, 667, "Error:vsize==0\n");
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		}
		if (mirror == NULL)
			goto no_mirror;
		command->size = mirror->size;
		goto map_ref;
	}
	if (attr & ALLOC_ATTR_SHARE_PHYPAGE) {
		if (mirror == NULL)
			goto no_mirror;
map_ref:
		MemBlockIncMapRef(mirror);
		attr = command->opt.attr;
		if (!(attr & ALLOC_ATTR_SHARE_VBASE)) {
			size = command->size;
			goto check_size;
		}
	} else if (!(attr & ALLOC_ATTR_SHARE_VBASE) || mirror == NULL) {
		if (!(attr & ALLOC_ATTR_SHARE_VBASE))
			goto check_size;
		goto no_mirror;
	}
	size = mirror->size;
	command->size = size;
	if (size & 0xfff)
		goto bad_size;
	goto set_vbase;
no_mirror:
	if (size & 0xfff)
		goto bad_size;
	if (!(attr & ALLOC_ATTR_SHARE_VBASE))
		goto check_attr;
set_vbase:
	attr |= ALLOC_ATTR_HAS_VBASE;
	command->opt.attr = attr;
	if (mirror != NULL)
		command->opt.vbase = mirror->vbase;
	goto check_attr;
check_size:
	if (size & 0xfff) {
bad_size:
		SCE_PRINTF_WITH_INFO(0, 696, "Invalid argument:size=0x%08x\n", size);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
check_attr:
	if (attr & ALLOC_ATTR_INVALID) {
		SCE_PRINTF_WITH_INFO(0, 712, "Invalid argument:attr=0x%08x\n", attr);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (attr & ALLOC_ATTR_NOPHYPAGE) {
		if (attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		if (command->opt.alignment != 0) {
			SCE_PRINTF_WITH_INFO(0, 727, "Invalid alignment:0x%08x\n",
					     command->opt.alignment);
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		}
	}
	if ((attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT) && command->opt.alignment != 0) {
		align = command->opt.alignment;
		if (align & (align - 1))
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		if (align & 0xfff) {
			SCE_PRINTF_WITH_INFO(0, 741, "Invalid alignment:0x%08x\n", align);
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		}
		if (align < 0x1000) {
			SCE_PRINTF_WITH_INFO(0, 747, "Invalid alignment:0x%08x\n", align);
			command->opt.alignment = 0;
			attr = command->opt.attr & ~SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
			command->opt.attr = attr;
		} else if (command->typeInfo->maxAlignment < align) {
			SCE_PRINTF_WITH_INFO(0, 754, "Invalid alignment:0x%08x > 0x%08x\n", align,
					     command->typeInfo->maxAlignment);
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		}
	}
	if (attr & ALLOC_ATTR_HAS_EXTRA_LOW) {
		if (command->opt.extraLow & 0xfff) {
			SCE_PRINTF_WITH_INFO(0, 763, "Invalid alignment:extraLow=0x%08x\n",
					     command->opt.extraLow);
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		}
		if (command->opt.extraLow < 0x1000) {
			SCE_PRINTF_WITH_INFO(0, 769, "size is small:extraLow=0x%08x\n",
					     command->opt.extraLow);
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		}
		if (command->opt.extraLow > 0xffffff) {
			SCE_PRINTF_WITH_INFO(0, 774, "Huge size:extraLow=0x%08x\n", command->opt.extraLow);
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		}
	}
	if (attr & ALLOC_ATTR_HAS_EXTRA_HIGH) {
		if (command->opt.extraHigh & 0xfff) {
			SCE_PRINTF_WITH_INFO(0, 782, "Invalid alignment:extraHigh=0x%08x\n",
					     command->opt.extraHigh);
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		}
		if (command->opt.extraHigh < 0x1000) {
			SCE_PRINTF_WITH_INFO(0, 788, "Smaller size:extraHigh=0x%08x\n",
					     command->opt.extraHigh);
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		}
		if (command->opt.extraHigh > 0xffffff) {
			SCE_PRINTF_WITH_INFO(0, 795, "Huge size:extraLow=0x%08x\n", command->opt.extraLow);
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		}
	}
	if ((command->opt.attr & (ALLOC_ATTR_HAS_EXTRA_LOW | ALLOC_ATTR_HAS_EXTRA_HIGH)) ==
	    (ALLOC_ATTR_HAS_EXTRA_LOW | ALLOC_ATTR_HAS_EXTRA_HIGH) &&
	    command->size < command->opt.extraLow + command->opt.extraHigh)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (attr & ALLOC_ATTR_HAS_CLIENT_INFO) {
		if (!(attr & ALLOC_ATTR_CLIENT_INFO))
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	} else if (attr & ALLOC_ATTR_CLIENT_INFO) {
		command->opt.openFlags = 0x30;
	}
	if ((attr & (ALLOC_ATTR_SHARE_PHYPAGE | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT)) ==
	    (ALLOC_ATTR_SHARE_PHYPAGE | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT)) {
		command->opt.attr = attr | ALLOC_ATTR_SHARE_PHYPAGE;
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (attr & ALLOC_ATTR_HAS_UNIT_SIZE) {
		switch (command->opt.unitSize) {
		case 0x1000:
		case 0x10000:
		case 0x100000:
		case 0x1000000:
			return 0;
		default:
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		}
	}
	return 0;
}

/* the block a MiB of the address space names: its first block, or its section owner */
MemBlockObject *AsCommonGetBlockLink(const SceUInt32 *l2Tables, uintptr_t va)
{
	SceUInt32 slot;

	if (l2Tables == NULL) {
		SCE_DBG_PRINTF(0xb, 2354, "There is no ppL2PageTable.\n");
		return NULL;
	}
	slot = l2Tables[va >> 20];
	if (AS_L2_SLOT_TYPE(slot) == AS_L2_SLOT_TABLE) {
		if (AS_L2_SLOT_PTR(slot) != 0)
			return ((L2PageTable *)AS_L2_SLOT_PTR(slot))->firstBlock;
	} else if (AS_L2_SLOT_TYPE(slot) == AS_L2_SLOT_BLOCK) {
		return (MemBlockObject *)AS_L2_SLOT_PTR(slot);
	}
	return NULL;
}

int MemBlockMapWithOpt(MemBlockObject *block, const SceKernelAllocMemBlockCommand *command,
		       Partition *partition)
{
	if ((block->flags & MEMBLOCK_KIND_MASK) != MEMBLOCK_KIND_VPL) {
		SCE_DBG_PRINTF(0xb, 1634, "WARNING:check:0x%08x+0x%08x\n", block->vbase, block->size);
		return 0;
	}
	return MemBlockVirPageListMapRaw(block, command, partition, 0);
}

/* whether the code is a CDRAM one exactly when res2 says it is */
static void AllocMemBlockCommandAssertCdramCode(SceUInt32 code, int res2)
{
	int isCdram = MemBlockCodeIsCdram(code);

	if ((isCdram && res2 == 0) || (!isCdram && res2 != 0)) {
		SCE_DBG_PRINTF(6, 314, "mismatch res=%d, res2=%d %s]\n", res2, isCdram, "SceNull");
		SCE_DBG_ASSERT_FAIL(315);
	}
}

/* name unknown: the block's options: the type bits, partial ops, the mirrored block */
int AllocMemBlockCommandSetOpt(SceKernelAllocMemBlockCommand *command)
{
	MemBlockObject *block = command->block, *base;
	SceUInt32 attr = command->opt.attr;
	int ret;

	if (attr >> 28)
		block->attr = (block->attr & ~0xf0000) | ((attr >> 28) << 16);
	if (attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_ALLOW_PARTIAL_OP)
		block->attr |= MEMBLOCK_ATTR_PARTIAL;
	ret = 0;
	if (attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_MIRROR_BLOCKID) {
		ret = MemBlockGetMirrorBlock(&command->opt, &base);
		if (ret >= 0) {
			MemBlockOptSetBaseBlockRaw(block, base);
			AllocMemBlockCommandSetExtraLow(command, MemBlockGetHeadFreeSize(base));
			AllocMemBlockCommandSetExtraHigh(command, MemBlockGetTailFreeSize(base));
			ret = 0;
		}
	}
	return ret;
}

/* name unknown: the last virtual page of a list block */
SYSMEM_DEBUG_STATIC VirPage *MemBlockGetLastVirPage(MemBlockObject *block)
{
	VirPage *vp = MemBlockGetVirPageRaw(block), *next;

	if (vp == NULL)
		return vp;
	for (next = vp->next; next != NULL; next = vp->next)
		vp = next;
	return vp;
}

/* next (the block after block, before nextnext) folded into block and freed */
static void MemBlockMergeNext(MemBlockObject *block, MemBlockObject *next, MemBlockObject *nextnext,
			      FixedHeap *heap, int isFree)
{
	SCE_DBG_ASSERT_CHECK(next == block->next, 1143);
	SCE_DBG_ASSERT_CHECK(nextnext == next->next, 1144);
	block->next = nextnext;
	block->size += next->size;
	nextnext->prev = block;
	if (isFree) {
		MemBlockSetFreeNextRaw(block, MemBlockGetFreeNextRaw(next));
		MemBlockSetFreePrevRaw(MemBlockGetFreeNextRaw(next), block);
	}
	next->next = next;
	next->prev = next;
	FreeMemBlockObject(heap, next);
}

/* block linked behind prev, in the block list and in the free list */
static inline void TinyPartitionLinkFreeAfter(MemBlockObject *prev, MemBlockObject *block)
{
	MemBlockObject *next;

	block->next = prev->next;
	block->prev = prev;
	prev->next = block;
	block->next->prev = block;
	MemBlockSetFreePrevRaw(block, prev);
	next = MemBlockGetFreeNextRaw(prev);
	MemBlockSetFreePrevRaw(next, block);
	next = MemBlockGetFreeNextRaw(prev);
	MemBlockSetFreeNextRaw(block, next);
	MemBlockSetFreeNextRaw(prev, block);
}

/* a new free block on its own free list */
static inline void MemBlockInitFreeRaw(MemBlockObject *block)
{
	MemBlockInitRaw(block);
	MemBlockSetFreePrevRaw(block, block);
	MemBlockSetFreeNextRaw(block, block);
}

/* a free block cut at offset: the rest becomes a new free block behind it */
int TinyPartitionSplitFreeMemBlockRaw(TinyPartition *tiny, MemBlockObject *block, SceSize offset,
				      MemBlockObject **rest)
{
	MemBlockObject *split;
	int ret;

	ret = AllocMemBlockObject(tiny->context->memBlockHeap, &split);
	if (ret < 0)
		return ret;
	MemBlockInitFreeRaw(split);
	split->vbase = block->vbase + offset;
	split->size = block->size - offset;
	block->size = offset;
	TinyPartitionLinkFreeAfter(block, split);
	SCE_DBG_ASSERT_CHECK(block->size != 0, 3190);
	SCE_DBG_ASSERT_CHECK(split->size != 0, 3191);
	if (rest != NULL)
		*rest = split;
	return 0;
}

/* the list sentinel, and one free block over the whole range */
int TinyPartitionInitList(TinyPartition *tiny)
{
	AddressSpaceContext *context = tiny->context;
	MemBlockObject *block;
	int intr, ret;

	ret = AllocMemBlockObject(context->memBlockHeap, &block);
	if (ret < 0)
		return ret;
	MemBlockInitFreeRaw(block);
	block->vbase = 0;
	block->size = 0;
	tiny->blocks = block;
	ret = AllocMemBlockObject(context->memBlockHeap, &block);
	if (ret < 0)
		return ret;
	MemBlockInitFreeRaw(block);
	MemBlockSetNameRaw(block, "SceFree");
	block->vbase = tiny->vbase;
	block->size = tiny->size;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&tiny->lock);
	TinyPartitionLinkFreeAfter(tiny->blocks, block);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&tiny->lock, intr);
	return ret;
}

/* a free block off the free list (it links to itself) */
static inline void MemBlockUnlinkFreeRaw(MemBlockObject *block)
{
	MemBlockObject *prev, *next;

	prev = MemBlockGetFreePrevRaw(block);
	MemBlockSetFreeNextRaw(prev, MemBlockGetFreeNextRaw(block));
	next = MemBlockGetFreeNextRaw(block);
	MemBlockSetFreePrevRaw(next, MemBlockGetFreePrevRaw(block));
	MemBlockSetFreePrevRaw(block, block);
	MemBlockSetFreeNextRaw(block, block);
}

static inline void TinyPartitionDecFreeSizeR1(TinyPartition *tiny, SceSize size)
{
	SCE_DBG_ASSERT_CHECK_FUNC(size <= tiny->freeSize, 2827, "TinyPartitionDecFreeSizeR1");
	SceCpuForDriver_A4884C4E((SceInt32 *)&tiny->freeSize, size);
}

/* the MiB slots of a new block: it is the first block of a table, or owns the section */
static inline void PartitionLinkBlockSlots(Partition *partition, MemBlockObject *block)
{
	SceUInt32 *slot, value, mib, last;
	int intr;

	intr = SceCpuForDriver_4C38CE4D(&partition->l2Lock);
	mib = block->vbase >> 20;
	last = (block->size + block->vbase - 1) >> 20;
	for (slot = &partition->tiny.context->l2Tables[mib]; mib <= last; mib++, slot++) {
		value = *slot;
		if (value == 0) {
			*slot = ((uintptr_t)block & ~3u) | AS_L2_SLOT_BLOCK;
		} else if (AS_L2_SLOT_TYPE(value) == AS_L2_SLOT_TABLE) {
			L2PageTable *l2pt = (L2PageTable *)AS_L2_SLOT_PTR(value);

			if (l2pt->firstBlock == NULL || block->vbase < l2pt->firstBlock->vbase)
				l2pt->firstBlock = block;
		} else if (AS_L2_SLOT_TYPE(value) == AS_L2_SLOT_BLOCK &&
			   block->vbase < ((MemBlockObject *)AS_L2_SLOT_PTR(value))->vbase) {
			*slot = ((uintptr_t)block & ~3u) | AS_L2_SLOT_BLOCK;
		}
	}
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&partition->l2Lock, intr);
}

/* the next block to look at: free blocks are walked along the free list */
static inline MemBlockObject *TinyPartitionNextCandidate(MemBlockObject *block)
{
	if (block->code == 0)
		return MemBlockGetFreeNextRaw(block);
	return block->next;
}

/*
 * The free block holding [va, va + size) cut down to exactly that range and
 * taken off the free list: it becomes the new block (param->block), and the
 * MiB slots it covers learn about it.
 */
static inline int TinyPartitionAllocMemBlockByAddrRaw(Partition *partition,
						       SceKernelAllocMemBlockCommand *command, uintptr_t va)
{
	TinyPartition *tiny = &partition->tiny;
	MemBlockObject *head = tiny->blocks, *block;
	int ret;

	for (block = TinyPartitionNextCandidate(head); block != head;
	     block = TinyPartitionNextCandidate(block)) {
		if (va < block->vbase || command->size + va - block->vbase > block->size)
			continue;
		if (block->code != 0) {
			SCE_DBG_PRINTF(6, 3388,
				       "INLCUDE not Free:pPart=%p[%s]/0x%08x, base=0x%08x, size=0x%08x\n",
				       tiny, tiny->name, tiny->freeSize, va, command->size);
			ret = SCE_KERNEL_ERROR_PARTITION_ERROR;
			SCE_DBG_PRINTF(6, 3390, "pBlock->name=[%s]\n", MemBlockGetName(block));
			goto failed;
		}
		if (va != block->vbase) {
			if (va > block->vbase) {
				ret = TinyPartitionSplitFreeMemBlockRaw(tiny, block, va - block->vbase,
									&block);
				if (ret < 0) {
					SCE_DBG_PRINTF(0xb, 3403,
						       "TinyPartitionSplitFreeMemBlockRaw failed 0x%08x\n",
						       ret);
					goto failed;
				}
			} else {
				SCE_DBG_ASSERT_FAIL(3407);
			}
		}
		if (block->size != command->size) {
			if (block->size > command->size) {
				ret = TinyPartitionSplitFreeMemBlockRaw(tiny, block, command->size, NULL);
				if (ret < 0) {
					SCE_DBG_PRINTF(0xb, 3417,
						       "TinyPartitionSplitFreeMemBlockRaw failed 0x%08x\n",
						       ret);
					goto failed;
				}
			} else {
				SCE_DBG_ASSERT_FAIL(3421);
			}
		}
		SceCpuForKernel_9DA58A9E((uint32_t *)&tiny->blockCount);
		TinyPartitionDecFreeSizeR1(tiny, block->size);
		MemBlockUnlinkFreeRaw(block);
		command->block = block;
		block->code = command->code;
		block->attr = 0;
		PartitionLinkBlockSlots(partition, block);
		return 0;
	}
	SCE_DBG_PRINTF(6, 3437, "Error:pPart=%p, name=[%s], base=0x%08x, size=0x%08x\n", tiny,
		       tiny->name, tiny->vbase, tiny->size);
	ret = SCE_KERNEL_ERROR_PARTITION_NO_VIRTUAL_ADDRESS;
	SCE_DBG_PRINTF(6, 3439, "Error:REQ: base=0x%08x, vsize=0x%08x\n", va, command->size);
failed:
	command->block = NULL;
	return ret;
}

int PartitionAllocMemBlockInListByAddrByCommandRaw(Partition *partition,
						   SceKernelAllocMemBlockCommand *command,
						   uintptr_t va)
{
	int intr, ret;

	SCE_DBG_ASSERT_CHECK(command->partition == partition, 5103);
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&partition->tiny.lock);
	ret = TinyPartitionAllocMemBlockByAddrRaw(partition, command, va);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&partition->tiny.lock, intr);
	return ret;
}

typedef void (*TinyPartitionFreeCallback)(TinyPartition *tiny, MemBlockObject *block);

/*
 * A block given back: it becomes free (unless flags bit 1, the callback sees
 * it first) and merges with free neighbours; the free list stays sorted.
 */
int TinyPartitionFreeMemBlock(TinyPartition *tiny, MemBlockObject *block, SceUInt32 flags,
			      TinyPartitionFreeCallback callback)
{
	AddressSpaceContext *context;
	MemBlockObject *head, *cur, *next, *prevFree, *prev;
	SceSize size;
	SceUInt32 merged;
	int intr, ret;

	/* Sony lays this error path out cold, after the walk */
	if (__builtin_expect(block->vbase < tiny->vbase ||
			     block->vbase + block->size - tiny->vbase > tiny->size, 0)) {
		SCE_DBG_PRINTF(0xb, 3576, "pBlockTarget=%p failed, pPart=%p[%s],nBlock=%d\n", block,
			       tiny, tiny->name, tiny->blockCount & 0xffff);
		SCE_DBG_ASSERT_FAIL(3577);
		return SCE_KERNEL_ERROR_PARTITION_ERROR;
	}
	context = tiny->context;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&tiny->lock);
	head = tiny->blocks;
#if FW_DEBUG
	if (head == NULL) {
		/* the dump of a partition whose list is gone (debug builds) */
		SCE_PRINTF_WITH_INFO(0xb, 3592, "***** pBlockAnchor is SCE_NULL *****\n");
		SCE_PRINTF_WITH_INFO(0xb, 3595, "pPart=%p[%s],nBlock=%d\n", tiny, tiny->name,
				     tiny->blockCount & 0xffff);
		SCE_PRINTF_WITH_INFO(0xb, 3598, ".base=0x%08x, .size=0x%08x\n", tiny->vbase,
				     tiny->size);
		SCE_PRINTF_WITH_INFO(0xb, 3601, ".nBlockUsed=0x%08x, .magic=0x%08x\n",
				     tiny->blockCount & 0xffff, tiny->magic);
		SCE_PRINTF_WITH_INFO(0xb, 3604, ".freeSize=0x%08x, .magic=0x%08x\n", tiny->freeSize,
				     tiny->magic);
		SCE_PRINTF_WITH_INFO(0xb, 3605, ".pAC=%p\n", tiny->context);
		SCE_PRINTF_WITH_INFO(0xb, 3608, "pBlockTarget %p[0x%08x+0x%08x]\n", block,
				     block->vbase, block->size);
		SCE_PRINTF_WITH_INFO(0xb, 3609, "guid=0x%08x\n", block->guid);
		if (block->flags & MEMBLOCK_FLAG_NAMED)
			SCE_PRINTF_WITH_INFO(0xb, 3611, ".name=[%s]\n", MemBlockGetNameRaw(block));
		if (tiny->context != NULL)
			SCE_PRINTF_WITH_INFO(0xb, 3615,
					     "PartitionGetAC(pPart)->=context.contextid=0x%08x\n",
					     tiny->context->mmu.contextId);
	}
#endif

	/* to the block, along the free list while it stays below it */
	prevFree = head;
	cur = MemBlockGetFreeNextRaw(head);
	if (cur->vbase >= block->vbase || cur == head) {
		cur = head->next;
		if (cur == head)
			goto not_found;
	}
	for (;;) {
		while (cur->code == 0) {
			next = MemBlockGetFreeNextRaw(cur);
			prevFree = cur;
			if (block->vbase <= next->vbase)
				goto step;
			if (next == head) {
				cur = cur->next;
				if (cur == head)
					goto not_found;
			} else {
				cur = next;
			}
		}
		if (cur == block)
			goto found;
step:
		cur = cur->next;
		if (cur == head)
			goto not_found;
	}
not_found:
	ret = SCE_KERNEL_ERROR_PARTITION_ERROR;
	goto out;

found:
	if (!(flags & 2))
		callback(tiny, cur);
	merged = 0;
	MemBlockInitRaw(cur);
	prev = block->prev;
	block->attr = 0;
	size = block->size;
	next = block->next;
	if (prev != head && prev->code == 0) {
		merged = 1;
		MemBlockMergeNext(prev, block, next, context->memBlockHeap, 0);
		block = prev;
	}
	if (next != head && next->code == 0) {
		merged |= 2;
		MemBlockMergeNext(block, next, next->next, context->memBlockHeap, 1);
	}
	if (!(merged & 2)) {
		MemBlockSetFreePrevRaw(MemBlockGetFreeNextRaw(prevFree), block);
		MemBlockSetFreeNextRaw(block, MemBlockGetFreeNextRaw(prevFree));
	}
	if (!(merged & 1)) {
		MemBlockSetFreeNextRaw(prevFree, block);
		MemBlockSetFreePrevRaw(block, prevFree);
	}
	SceCpuForDriver_5F6A8743((SceInt32 *)&tiny->freeSize, size);
	ret = 0;
	SceCpuForKernel_0E04BD2E((uint32_t *)&tiny->blockCount);
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&tiny->lock, intr);
	return ret;
}

/* the kernel/user shared tiny partition (CreateTinyPartitionRaw) */
static TinyPartition *s_tinyPartitionUserShared;

/* a block freed with what it carried: its base block (out) and base info (out) */
int PartitionFreeMemBlockWithBaseRaw(Partition *partition, MemBlockObject *block,
				     MemBlockObject **baseBlock, void **baseInfo)
{
	MemBlockObject *base;
	TinyPartition *shared;
	SceUInt32 opt;
	int ret;

	if (partition == NULL) {
		SCE_DBG_ASSERT_FAIL(5856);
		return SCE_KERNEL_ERROR_PARTITION_ERROR;
	}
	*baseBlock = NULL;
	ret = PartitionFreeMemBlockMappedToVirAllocRaw(partition, block);
	if (ret < 0)
		return ret;
	opt = block->flags & MEMBLOCK_OPT_MASK;
	if (opt == MEMBLOCK_OPT_BASE_INFO) {
		void *info = MemBlockOptGetBaseInfoRaw(block);

		if (baseInfo != NULL)
			*baseInfo = info;
	} else if (opt == MEMBLOCK_OPT_BASE_BLOCK) {
		base = MemBlockOptGetBaseBlockRaw(block);
		if (base != NULL)
			*baseBlock = base;
	}
	if ((block->flags & MEMBLOCK_OPT_MASK) == MEMBLOCK_OPT_VIR_BASE_BLOCK) {
		shared = s_tinyPartitionUserShared;
		base = MemBlockOptGetVirBaseBlockRaw(block);
		if (base->code != 0)
			TinyPartitionFreeMemBlock(shared, base, 2, NULL);
	}
	if (block->code == 0)
		return 0;
	return TinyPartitionFreeMemBlock(&partition->tiny, block, 0,
					 (TinyPartitionFreeCallback)PartitionUnlinkBlockSlots);
}

/*
 * name unknown: size bytes cut from a free block at offset (or, with attr bit
 * 27, at the highest size-aligned offset) and taken off the free list.
 */
int TinyPartitionCutFreeMemBlockRaw(TinyPartition *tiny, const SceSize *size, SceUInt32 attr,
				    MemBlockObject *block, SceSize offset, MemBlockObject **out)
{
	int ret;

	if (attr & 0x8000000) {
		/* from the top: the last whole unit of the free block */
		offset += *size * (((block->size - offset) >> (31 - __builtin_clz(*size))) - 1);
		if (offset != 0)
			goto split;
	} else if (offset != 0) {
split:
		ret = TinyPartitionSplitFreeMemBlockRaw(tiny, block, offset, &block);
		if (ret < 0)
			return ret;
	}
	if (*size < block->size) {
		ret = TinyPartitionSplitFreeMemBlockRaw(tiny, block, *size, NULL);
		if (ret < 0)
			return ret;
	} else if (*size != block->size) {
		*out = block;
		return 0;
	}
	MemBlockUnlinkFreeRaw(block);
	*out = block;
	return 0;
}

/* the bytes from va up to its alignment */
static inline SceSize GetAlignmentOffset(uintptr_t va, SceSize align)
{
	SCE_DBG_ASSERT_FUNC((align & (align - 1)) == 0, 268, "GetAlignmentOffset");
	return ((va + align - 1) & -align) - va;
}

/* the next block a search looks at: free blocks go along the free list */
static inline MemBlockObject *TinyPartitionStep(MemBlockObject *block, SceUInt32 attr)
{
	if (attr & 0x8000000) {
		if (block->code == 0)
			return MemBlockGetFreePrevRaw(block);
		return block->prev;
	}
	if (block->code == 0)
		return MemBlockGetFreeNextRaw(block);
	return block->next;
}

/*
 * A free range of req->size aligned to align, at least req->offset into the
 * partition: the first fit from the bottom, or from the top with attr bit 27.
 */
int TinyPartitionAllocMemBlockRaw(TinyPartition *tiny, SceKernelAllocMemBlockCommand *command,
				  SceSize align, MemBlockObject **out)
{
	MemBlockObject *head = tiny->blocks, *cur, *start;
	uintptr_t minVa;
	SceSize pad;
	int ret;

	if (tiny->size < command->offset) {
		SCE_DBG_PRINTF(0xb, 3250, "Illegal offset 0x%08x\n", command->offset);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	cur = TinyPartitionStep(head, command->opt.attr);
	start = head;
	if (command->offset != 0) {
		minVa = command->offset + tiny->vbase;
		for (;;) {
			if (head == cur) {
				/* nothing at or above minVa: from the start again */
				cur = TinyPartitionStep(head, command->opt.attr);
				start = head;
				goto search;
			}
			if (cur->vbase >= minVa)
				break;
			/* a free block across minVa may still hold an aligned range above it */
			if (cur->code == 0 && command->size - cur->vbase + minVa < cur->size) {
				pad = GetAlignmentOffset(minVa, align) + minVa - cur->vbase;
				if (command->size + pad < cur->size) {
					ret = TinyPartitionCutFreeMemBlockRaw(tiny, &command->size,
									      command->opt.attr, cur,
									      pad, &cur);
					if (ret < 0)
						return ret;
					goto taken;
				}
			}
			start = cur;
			cur = TinyPartitionStep(cur, command->opt.attr);
		}
		SCE_DBG_ASSERT_CHECK(minVa <= cur->vbase, 3299);
	}
search:
	for (;;) {
		if (cur->code == 0) {
			pad = GetAlignmentOffset(cur->vbase, align);
			if (command->size == cur->size && pad == 0) {
				MemBlockUnlinkFreeRaw(cur);
				goto taken;
			}
			if (command->size + pad <= cur->size) {
				ret = TinyPartitionCutFreeMemBlockRaw(tiny, &command->size, command->opt.attr, cur,
								      pad, &cur);
				if (ret < 0)
					return ret;
				goto taken;
			}
		}
		cur = TinyPartitionStep(cur, command->opt.attr);
		if (start == cur)
			break;
		if (head == cur) {
restart:
			cur = TinyPartitionStep(head, command->opt.attr);
			start = head;
		}
	}
	*out = NULL;
	return SCE_KERNEL_ERROR_PARTITION_NO_VIRTUAL_ADDRESS;
taken:
	SceCpuForKernel_9DA58A9E((uint32_t *)&tiny->blockCount);
	TinyPartitionDecFreeSizeR1(tiny, cur->size);
	*out = cur;
	return 0;
}

/* a block anywhere in the partition (aligned to align) */
int PartitionAllocMemBlockInListByCommandRaw(Partition *partition,
					     SceKernelAllocMemBlockCommand *command,
					     SceSize align)
{
	MemBlockObject *block;
	int intr, ret;

	SCE_DBG_ASSERT_CHECK(command->partition == partition, 5078);
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&partition->tiny.lock);
	ret = TinyPartitionAllocMemBlockRaw(&partition->tiny, command, align, &block);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 5082, "TinyPartitionAllocMemBlockRaw failed 0x%08x\n", ret);
		command->block = NULL;
	} else {
		block->code = command->code;
		block->attr = 0;
		PartitionLinkBlockSlots(partition, block);
		command->block = block;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&partition->tiny.lock, intr);
	return ret;
}

/*
 * Virtual pages of a type for [va, va + size), each as large as the alignment
 * of va allows (at most 16 MiB), appended to the block's list.
 */
int MemBlockAssocVirPageWithType(MemBlockObject *block, Partition *partition, SceUInt32 type,
				 uintptr_t va, SceSize size, uintptr_t pbase)
{
	VirPage *last, *vp = NULL;
	SceSize chunk, rest;
	int ret;

	if ((block->flags & MEMBLOCK_KIND_MASK) == MEMBLOCK_KIND_VPL)
		last = MemBlockGetLastVirPage(block);
	else
		last = NULL;
	while (size != 0) {
		ret = PartitionAllocVirPageRaw(partition, &vp);
		if (ret < 0) {
			SCE_DBG_ASSERT_FAIL_RELOAD(1265);
			return ret;
		}
		vp->vbase = va;
		vp->pbase = pbase;
		vp->flags = (vp->flags & ~0x3f) | (type & 0x3f);
		if (type == VIRPAGE_TYPE_DIVIDED && (block->attr & MEMBLOCK_ATTR_PHYCONT)) {
			vp->size = size;
			vp->flags |= VIRPAGE_ROUND_UP;
			if (last == NULL) {
				MemBlockSetVirPageRaw(block, vp);
				return 0;
			}
			last->next = vp;
			return 0;
		}
		if (va == 0) {
			vp->size = size;
			chunk = size;
			rest = 0;
		} else if ((1u << __builtin_ctz(va)) <= size) {
			chunk = 1u << __builtin_ctz(va);
			vp->size = chunk;
			if (chunk < 0x1000) {
				SCE_DBG_ASSERT_FAIL_RELOAD(1312);
				chunk = vp->size;
			}
			if (chunk > 0x1000000) {
				chunk = 0x1000000;
				vp->size = chunk;
			}
			rest = size - chunk;
		} else {
			chunk = 1u << (31 - __builtin_clz(size));
			vp->size = chunk;
			if (chunk < 0x1000) {
				SCE_DBG_ASSERT_FAIL_RELOAD(1301);
				chunk = vp->size;
			}
			if (chunk > 0x1000000) {
				chunk = 0x1000000;
				vp->size = chunk;
			}
			rest = size - chunk;
		}
		va += chunk;
		pbase += chunk;
		size = rest;
		if (last == NULL)
			MemBlockSetVirPageRaw(block, vp);
		else
			last->next = vp;
		last = vp;
	}
	return 0;
}

/* name unknown: size bytes of one type at *va, *va moved past them */
int MemBlockAllocVirPageForType(MemBlockObject *block, SceUInt32 type, SceSize size,
				Partition *partition, uintptr_t *va)
{
	int ret = 0;

	if (size != 0) {
		ret = MemBlockAssocVirPageWithType(block, partition, type, *va, size, 0);
		if (ret >= 0) {
			*va += size;
			return ret;
		}
		SCE_DBG_PRINTF(0xb, 1352, "MemBlockAssocVirPageWithType failed 0x%08x\n", ret);
		SCE_DBG_ASSERT_FAIL(1353);
	}
	return ret;
}

/*
 * A block's virtual pages: the extra low part (type 1), the low pad (type 2),
 * the backed middle (type 4, or 16 at pbase with mode 1), the high pad and the
 * extra high part.
 */
int MemBlockAllocVirPage(MemBlockObject *block, SceKernelAllocMemBlockCommand *command,
			 Partition *partition, int mode, uintptr_t pbase)
{
	uintptr_t va = block->vbase;
	SceSize low, high, middle;
	int ret;

	low = (command->opt.attr & ALLOC_ATTR_HAS_EXTRA_LOW) ? command->opt.extraLow : 0;
	ret = MemBlockAllocVirPageForType(block, VIRPAGE_TYPE_1, low, partition, &va);
	if (ret < 0)
		return ret;
	ret = MemBlockAllocVirPageForType(block, VIRPAGE_TYPE_PHY, command->padLow, partition, &va);
	if (ret < 0)
		return ret;
	low = (command->opt.attr & ALLOC_ATTR_HAS_EXTRA_LOW) ? command->opt.extraLow : 0;
	high = (command->opt.attr & ALLOC_ATTR_HAS_EXTRA_HIGH) ? command->opt.extraHigh : 0;
	middle = command->size - low - high - command->padLow - command->padHigh;
	if (middle != 0) {
		if (mode == 0) {
			ret = MemBlockAssocVirPageWithType(block, partition, VIRPAGE_TYPE_DIVIDED, va,
							   middle, 0);
		} else if (mode == 1) {
			ret = MemBlockAssocVirPageWithType(block, partition, VIRPAGE_TYPE_16, va,
							   middle, pbase);
		} else {
			SCE_DBG_ASSERT_FAIL(1416);
			va += middle;
			goto tail;
		}
		if (ret < 0) {
			SCE_DBG_ASSERT_FAIL(1420);
			return ret;
		}
		va += middle;
	}
tail:
	ret = MemBlockAllocVirPageForType(block, VIRPAGE_TYPE_PHY, command->padHigh, partition, &va);
	if (ret < 0)
		return ret;
	high = (command->opt.attr & ALLOC_ATTR_HAS_EXTRA_HIGH) ? command->opt.extraHigh : 0;
	ret = MemBlockAllocVirPageForType(block, VIRPAGE_TYPE_1, high, partition, &va);
	if (ret < 0)
		return ret;
	SCE_DBG_ASSERT_CHECK(((command->opt.attr & ALLOC_ATTR_HAS_EXTRA_LOW) ? command->opt.extraLow : 0) ==
			     MemBlockGetHeadType1Size(block), 1442);
	SCE_DBG_ASSERT_CHECK(((command->opt.attr & ALLOC_ATTR_HAS_EXTRA_HIGH) ? command->opt.extraHigh : 0) ==
			     MemBlockGetTailType1Size(block), 1444);
	return 0;
}

/* MemBlockTypeInfo attr: never cleared */
#define MEMBLOCK_TYPE_NO_CLEAR		0x20000000
#define MEMBLOCK_TYPE_CDRAM		0x80

/* the largest power of two both are a multiple of */
static inline SceSize VirPageCommonAlign(uintptr_t va, SceSize size)
{
	SceSize vaAlign = 1u << __builtin_ctz(va), sizeAlign = 1u << __builtin_ctz(size);

	return sizeAlign < vaAlign ? sizeAlign : vaAlign;
}

/* a mirror's virtual pages: the base block's physical pages, page by page */
static inline int MemBlockSharePhyPageInMemBlockVirPageList(MemBlockObject *block,
							    MemBlockObject *base,
							    Partition *partition)
{
	uintptr_t va = block->vbase, pbase;
	VirPage *src, *vp = NULL, *last;
	SceUInt32 type;
	SceSize size, chunk;
	int ret;

	src = MemBlockGetVirPageRaw(base);
	last = (block->flags & MEMBLOCK_KIND_MASK) == MEMBLOCK_KIND_VPL ?
	       MemBlockGetLastVirPage(block) : NULL;
	for (; src != NULL; src = src->next) {
		type = src->flags;
		size = src->size;
		pbase = src->pbase;
		while (size != 0) {
			ret = PartitionAllocVirPageRaw(partition, &vp);
			if (ret < 0)
				return ret;
			if (vp == NULL) {
				SCE_DBG_ASSERT_FAIL(1792);
				return ret;
			}
			chunk = VirPageCommonAlign(va, size);
			vp->vbase = va;
			vp->page = NULL;
			vp->pbase = pbase;
			vp->flags = (type & 0x3f) | (vp->flags & ~0x3f) | VIRPAGE_HAS_PBASE;
			vp->size = chunk;
			size -= chunk;
			va += chunk;
			pbase += chunk;
			if (last == NULL)
				MemBlockSetVirPageRaw(block, vp);
			else
				last->next = vp;
			last = vp;
		}
	}
	return 0;
}

/* a mirror shares its base block's physical pages */
static inline int MemBlockSharePhyPage(MemBlockObject *block,
				       SceKernelAllocMemBlockCommand *command,
				       Partition *partition)
{
	MemBlockObject *base;
	SceUInt32 attr = command->opt.attr;

	command->block->attr |= MEMBLOCK_ATTR_KEEP_PHY;
	if (!(attr & ALLOC_ATTR_SHARE_PHYPAGE) ||
	    ((attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_MIRROR_BLOCKID) && block->vbase == 0)) {
		if ((block->flags & MEMBLOCK_KIND_MASK) == MEMBLOCK_KIND_VPL)
			MemBlockVirPageListFreeVirPageCore(block, partition);
		return SCE_KERNEL_ERROR_PARTITION_ERROR;
	}
	base = MemBlockOptGetBaseBlockRaw(block);
	if ((base->flags & MEMBLOCK_KIND_MASK) == MEMBLOCK_KIND_VPL)
		MemBlockSharePhyPageInMemBlockVirPageList(block, base, partition);
	else
		SCE_DBG_PRINTF(0xb, 1851, "Warning: unsupported case\n");
	return 0;
}

/* one virtual page per range of opt.paddr_list */
static inline int MemBlockAllocVirPageFromPAVector(MemBlockObject *block,
						   SceKernelAllocMemBlockCommand *command,
						   Partition *partition)
{
	const SceKernelPAVector *list = command->opt.paddr_list;
	const SceKernelPARange *range = list->pRanges;
	uintptr_t va = block->vbase;
	VirPage *vp = NULL, *last = NULL;
	SceUInt32 i;
	SceSize size;
	uintptr_t pbase;
	int ret;

	if ((block->flags & MEMBLOCK_KIND_MASK) == MEMBLOCK_KIND_VPL)
		last = MemBlockGetLastVirPage(block);
	for (i = 0; i < list->nDataInVector; i++, range++) {
		size = range->size;
		pbase = range->addr;
		ret = PartitionAllocVirPageRaw(partition, &vp);
		if (ret < 0) {
			SCE_DBG_ASSERT_FAIL(1490);
			return ret;
		}
		vp->vbase = va;
		vp->size = size;
		vp->pbase = pbase;
		vp->page = NULL;
		vp->flags = (vp->flags & ~0x3f) | VIRPAGE_HAS_PBASE | VIRPAGE_TYPE_DIVIDED;
		if (last == NULL)
			MemBlockSetVirPageRaw(block, vp);
		else
			last->next = vp;
		va += size;
		last = vp;
	}
	return 0;
}

/* VA -> LA: the block's virtual pages, by the command */
static inline int MemBlockVirAlloc2LisAllocByCommand(MemBlockObject *block,
						     SceKernelAllocMemBlockCommand *command,
						     Partition *partition)
{
	SceUInt32 cmd;
	int ret;

	SCE_DBG_ASSERT_CHECK(block == command->block, 1863);
	SCE_DBG_ASSERT_CHECK(partition == command->partition, 1864);
	cmd = command->cmdFlags;
	if (cmd & ALLOC_CMD_MIRROR) {
		ret = MemBlockSharePhyPage(block, command, partition);
		if (ret < 0)
			return ret;
	} else {
		if (cmd & ALLOC_CMD_PADDR) {
			ret = MemBlockAllocVirPage(block, command, partition, 1, command->opt.paddr);
		} else if (cmd & ALLOC_CMD_PADDR_LIST) {
			ret = MemBlockAllocVirPageFromPAVector(block, command, partition);
		} else if (cmd & ALLOC_CMD_PARTIAL) {
			ret = MemBlockPartialInfoCreate(block, command);
		} else if (cmd & ALLOC_CMD_PHY_ANY) {
			ret = MemBlockAllocVirPage(block, command, partition, 0, 0);
		} else {
			SCE_DBG_ASSERT_FAIL(1897);
			ret = 0;
		}
		if (ret < 0) {
			if ((block->flags & MEMBLOCK_KIND_MASK) == MEMBLOCK_KIND_VPL)
				MemBlockVirPageListFreeVirPageCore(block, partition);
			return ret;
		}
	}
	MemBlockStatusStateChangeLisAllocFromVA(block);
	block->partition = partition;
	MemBlockSetPhyMemPartRaw(block, command->phyMemPart);
	return 0;
}

/* LA -> PA: physical pages behind it, unless the caller gives them */
static inline int MemBlockLisAlloc2PhyAllocByCommand(MemBlockObject *block,
						     SceKernelAllocMemBlockCommand *command,
						     Partition *partition)
{
	SceUInt32 attr;
	int ret;

	SCE_DBG_ASSERT_CHECK(block == command->block, 1918);
	SCE_DBG_ASSERT_CHECK(partition == command->partition, 1919);
	attr = command->opt.attr;
	if (!(attr & ALLOC_ATTR_NOPHYPAGE) &&
	    !(attr & (ALLOC_ATTR_SHARE_PHYPAGE | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR_LIST))) {
		if ((block->flags & MEMBLOCK_KIND_MASK) == MEMBLOCK_KIND_VPL) {
			ret = MemBlockVirPageListAllocPhyPage(block, partition, command->phyMemPart);
			if (ret < 0) {
				SCE_DBG_PRINTF(0xb, 1937,
					       "MemBlockVirPageListAllocPhyPage failed 0x%08x\n", ret);
				if ((block->flags & MEMBLOCK_KIND_MASK) == MEMBLOCK_KIND_VPL)
					MemBlockVirPageListFreeVirPageCore(block, partition);
				return ret;
			}
		} else if (block->attr & MEMBLOCK_ATTR_PARTIAL) {
			ret = MemBlockPartialAllocPhyPage(block, command);
			if (ret < 0) {
				SCE_DBG_PRINTF(0xb, 1943, "MemBlockPartialAllocPhyPage failed 0x%08x\n",
					       ret);
				return ret;
			}
		}
	}
	MemBlockStatusStateChangePhyAllocFromLARaw(block);
	return 0;
}

/* PA -> MA: mapped, unless the caller maps it */
static inline int MemBlockPhyAlloc2MappedByCommand(MemBlockObject *block,
						   SceKernelAllocMemBlockCommand *command,
						   Partition *partition)
{
	SceUInt32 attr;
	int ret;

	SCE_DBG_ASSERT_CHECK(block == command->block, 1964);
	SCE_DBG_ASSERT_CHECK(partition == command->partition, 1965);
	attr = command->opt.attr;
	if (attr & ALLOC_ATTR_NOPHYPAGE) {
		block->attr = (block->attr & ~MEMBLOCK_ATTR_NEEDS_CLEAR) | MEMBLOCK_ATTR_MAPPED;
	} else if ((attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR_LIST) ||
		   (attr & ALLOC_ATTR_SHARE_PHYPAGE)) {
		/* somebody else's pages: nothing to free on failure */
		if (!(attr & ALLOC_ATTR_NO_MAP)) {
			ret = MemBlockMapWithOpt(block, command, partition);
			if (ret < 0)
				return ret;
		}
	} else if (block->attr & MEMBLOCK_ATTR_PARTIAL) {
		MemBlockPartialMapRaw(block, command);
	} else if (!(attr & ALLOC_ATTR_NO_MAP)) {
		ret = MemBlockMapWithOpt(block, command, partition);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 1996, "MemBlockMapWithOpt failed 0x%08x\n", ret);
			MemBlockFreePhyPages(block, MemBlockGetPhyMemPartRaw(block));
			return ret;
		}
	}
	MemBlockStatusStateChangeMappedFromPARaw(block);
	return 0;
}

/*
 * A virtually allocated block taken through the remaining states: virtual
 * pages (LA), physical pages (PA) and the mapping (MA).
 */
int MemBlockVirAlloc2MappedByCommand(MemBlockObject *block, SceKernelAllocMemBlockCommand *command,
				     Partition *partition)
{
	MemBlockAllocResult *result;
	SceUInt32 typeAttr, attr;
	int ret;

	SCE_DBG_ASSERT_CHECK(command->block == block, 2019);
	SCE_DBG_ASSERT_CHECK(command->partition == partition, 2020);
	if (command->opt.size != sizeof(command->opt)) {
		SCE_DBG_ASSERT_FAIL(2028);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	}
	block->partition = NULL;
	block->virPages = NULL;
	block->flags &= ~MEMBLOCK_KIND_MASK;
	block->code = command->code;
	if (command->opt.attr & ALLOC_ATTR_CLIENT_INFO)
		MemBlockOptSetClientInfoRaw(block, &command->opt);
	MemBlockSetNameRaw(block, command->name);

	typeAttr = command->typeInfo->attr;
	if ((typeAttr & MEMBLOCK_TYPE_NO_CLEAR) || (command->code & 0xf00000) == 0x100000 ||
	    !(command->code & 0x22)) {
		if (!(command->opt.attr & ALLOC_ATTR_NO_CLEAR))
			command->opt.attr |= ALLOC_ATTR_NO_CLEAR;
	}
	attr = command->opt.attr;
	if (!(attr & ALLOC_ATTR_NO_CLEAR)) {
		if (!(attr & (SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR |
			      SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR_LIST))) {
			block->attr |= MEMBLOCK_ATTR_NEEDS_CLEAR;
		} else {
			typeAttr &= MEMBLOCK_TYPE_CDRAM;
			AllocMemBlockCommandAssertCdramCode(command->code, typeAttr);
			if (typeAttr != 0)
				block->attr |= MEMBLOCK_ATTR_NEEDS_CLEAR;
		}
		attr = command->opt.attr;
	}
	if (attr & ALLOC_ATTR_SHARE_PHYPAGE)
		block->attr &= ~MEMBLOCK_ATTR_NEEDS_CLEAR;
	if (attr & ALLOC_ATTR_NO_MAP)
		block->attr &= ~MEMBLOCK_ATTR_NEEDS_CLEAR;
	if (attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT)
		block->attr |= MEMBLOCK_ATTR_PHYCONT;
	if (attr & ALLOC_ATTR_USER_CLIENT)
		block->attr |= MEMBLOCK_ATTR_USER_CLIENT;

	ret = MemBlockVirAlloc2LisAllocByCommand(block, command, partition);
	if (ret < 0)
		return ret;
	ret = MemBlockLisAlloc2PhyAllocByCommand(block, command, partition);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 2156, "MemBlockLisAlloc2PhyAllocByCommand failed. res=0x%08x\n",
			       ret);
		return ret;
	}
	ret = MemBlockPhyAlloc2MappedByCommand(block, command, partition);
	if (ret < 0) {
		if ((block->flags & MEMBLOCK_KIND_MASK) == MEMBLOCK_KIND_VPL)
			MemBlockVirPageListFreeVirPageCore(block, partition);
		return ret;
	}
	result = command->result;
	if (result != NULL) {
		result->mappedBase = block->vbase + MemBlockGetHeadFreeSize(block);
		result->mappedSize = MemBlockGetAllocMapSizeRaw(block);
		result->memoryType = (SceUInt8)(command->code >> 8) & 0xd0;
		result->access = (SceUInt8)command->code >> 4 & 6;
	}
	return 0;
}

/* the allocation unit: opt.unitSize when given, else a page */
static inline SceSize AllocMemBlockCommandGetUnitSize(const SceKernelAllocMemBlockCommand *command)
{
	if (command->opt.attr & ALLOC_ATTR_HAS_UNIT_SIZE)
		return command->opt.unitSize;
	return 0x1000;
}

static inline void AllocMemBlockCommandRoundupUnitSize(SceKernelAllocMemBlockCommand *command,
						       SceSize unit)
{
	SCE_DBG_ASSERT_CHECK_FUNC(unit == 0x1000 || unit == 0x10000 || unit == 0x100000 ||
				  unit == 0x1000000, 289, "AllocMemBlockCommandRoundupUnitSize");
	command->size = (command->size + unit - 1) & -unit;
}

/* a block allocated at va, taken through to mapped */
int PartitionAllocMemBlockCoreByAddrByCommandRaw(Partition *partition,
						 SceKernelAllocMemBlockCommand *command,
						 uintptr_t va)
{
	MemBlockObject *block;
	SceSize unit;
	int ret;

	SCE_DBG_ASSERT_CHECK(command->partition == partition, 5212);
	ret = AllocMemBlockCommandCheckParameter(command);
	if (ret < 0)
		goto fail;
	unit = AllocMemBlockCommandGetUnitSize(command);
	if (va & (unit - 1)) {
		SCE_DBG_ASSERT_FAIL(5227);
		command->block = NULL;
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	AllocMemBlockCommandRoundupUnitSize(command, unit);
	if (va == 0 && !(command->opt.attr & ALLOC_ATTR_VBASE_ANY)) {
		SCE_DBG_ASSERT_FAIL(5238);
		command->block = NULL;
		return SCE_KERNEL_ERROR_ERROR;
	}
	ret = PartitionAllocMemBlockInListByAddrByCommandRaw(partition, command, va);
	if (ret < 0)
		goto fail;
	block = command->block;
	AllocMemBlockCommandSetOpt(command);
	MemBlockStatusStateChangeVirAllocFromFreeRaw(block);
	ret = MemBlockVirAlloc2MappedByCommand(block, command, partition);
	if (ret >= 0)
		return ret;
	if (block->code != 0)
		TinyPartitionFreeMemBlock(&partition->tiny, block, 0,
					  (TinyPartitionFreeCallback)PartitionUnlinkBlockSlots);
fail:
	command->block = NULL;
	return ret;
}

/* the largest power of two not above size */
static inline SceSize AllocMemBlockSizeAlign(SceSize size)
{
	return 1u << (31 - __builtin_clz(size));
}

/* a block placed by the partition (or the user shared one), taken through to mapped */
static inline int PartitionAllocMemBlockCoreByCommandRaw(Partition *partition,
							 SceKernelAllocMemBlockCommand *command)
{
	MemBlockObject *block, *sharedBlock = NULL;
	TinyPartition *shared;
	SceSize unit, align, sizeAlign;
	SceUInt32 attr;
	int intr, ret;

	SCE_DBG_ASSERT_CHECK(partition == command->partition, 5308);
	unit = AllocMemBlockCommandGetUnitSize(command);
	ret = AllocMemBlockCommandCheckParameter(command);
	if (ret < 0)
		goto fail;
	AllocMemBlockCommandRoundupUnitSize(command, unit);
	attr = command->opt.attr;
	if (attr & ALLOC_ATTR_NO_ALIGN)
		align = 0;
	else if (attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT)
		align = command->opt.alignment;
	else
		align = 0;
	if (attr & ALLOC_ATTR_HAS_VBASE) {
		ret = PartitionAllocMemBlockInListByAddrByCommandRaw(partition, command,
								     command->opt.vbase);
		if (ret < 0)
			goto fail;
		goto vir_alloc;
	}
	sizeAlign = AllocMemBlockSizeAlign(command->size);
	if (align < unit)
		align = unit;
	if (sizeAlign >= 0x100000)
		sizeAlign = 0x100000;
	if (align < sizeAlign)
		align = sizeAlign;
	if (align >= 0x1000000)
		align = 0x1000000;
	if (((command->code >> 24) & 0xf) == 10) {
		/* user shared: the range comes from the shared partition */
		shared = s_tinyPartitionUserShared;
		if (shared == NULL)
			return SCE_KERNEL_ERROR_PARTITION_ERROR;
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&shared->lock);
		ret = TinyPartitionAllocMemBlockRaw(shared, command, align, &sharedBlock);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&shared->lock, intr);
		if (ret < 0)
			return ret;
		ret = PartitionAllocMemBlockInListByAddrByCommandRaw(partition, command,
								     sharedBlock->vbase);
		if (ret < 0) {
			if (sharedBlock->code != 0)
				TinyPartitionFreeMemBlock(s_tinyPartitionUserShared, sharedBlock, 2,
							  NULL);
			goto fail;
		}
		block = command->block;
		sharedBlock->code = command->code;
		MemBlockOptSetVirBaseBlockRaw(block, sharedBlock);
	} else {
		ret = PartitionAllocMemBlockInListByCommandRaw(partition, command, align);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 5410,
				       "PartitionAllocMemBlockInListByCommandRaw([%s]) failed 0x%08x\n",
				       partition->tiny.name, ret);
			return ret;
		}
	}
vir_alloc:
	AllocMemBlockCommandSetOpt(command);
	block = command->block;
	MemBlockStatusStateChangeVirAllocFromFreeRaw(block);
	ret = MemBlockVirAlloc2MappedByCommand(command->block, command, partition);
	if (ret < 0) {
		block = command->block;
		SCE_DBG_PRINTF(0xb, 5428,
			       "MemBlockVirAlloc2MappedByCommand(0x%08x) failed 0x%08x, %p[0x%08x+0x%08x]\n",
			       command->size, ret, block, block->vbase, block->size);
		block = command->block;
		if (block != NULL) {
			SCE_DBG_PRINTF(0xb, 5432, "->FreeMemBlockFromList(,%p[0x%08x])\n", block,
				       block->size);
			if (command->block->code != 0)
				TinyPartitionFreeMemBlock(&command->partition->tiny, command->block, 0,
							  (TinyPartitionFreeCallback)
							  PartitionUnlinkBlockSlots);
			command->block = NULL;
		}
		if (sharedBlock != NULL) {
			SCE_DBG_PRINTF(0xb, 5437, "->FreeMemBlockFromListForShared()\n");
			if (sharedBlock->code != 0)
				TinyPartitionFreeMemBlock(s_tinyPartitionUserShared, sharedBlock, 2,
							  NULL);
		}
	}
	return ret;
fail:
	command->block = NULL;
	return ret;
}

/* the CDRAM codes are mapped at fixed offsets from their physical pages */
static inline uintptr_t MemBlockCdramPhyToVir(SceUInt32 code, uintptr_t pa)
{
	switch (code) {
	case MEMBLOCK_CODE_USER_CDRAM_L1WBWA_RW:
	case MEMBLOCK_CODE_USER_CDRAM_R:
	case MEMBLOCK_CODE_USER_CDRAM_RW:
		return pa < 0x27000000 ? pa + 0x41000000 : pa + 0x39000000;
	case MEMBLOCK_CODE_KERNEL_CDRAM_L1WBWA_RW:
	case MEMBLOCK_CODE_KERNEL_CDRAM_RW:
		return pa < 0x27000000 ? pa + 0x01000000 : pa - 0x07000000;
	default:
		return pa;
	}
}

static inline uintptr_t MemBlockCdramVirToPhy(SceUInt32 code, uintptr_t va)
{
	switch (code) {
	case MEMBLOCK_CODE_USER_CDRAM_L1WBWA_RW:
	case MEMBLOCK_CODE_USER_CDRAM_R:
	case MEMBLOCK_CODE_USER_CDRAM_RW:
		return va < 0x61000000 ? va - 0x39000000 : va - 0x41000000;
	case MEMBLOCK_CODE_KERNEL_CDRAM_L1WBWA_RW:
	case MEMBLOCK_CODE_KERNEL_CDRAM_RW:
		return va < 0x21000000 ? va + 0x07000000 : va - 0x01000000;
	default:
		return va;
	}
}

/*
 * A block allocated by the command. CDRAM blocks get their physical pages
 * first (given, a mirror's, or from the CDRAM partition) and sit at the
 * matching fixed address.
 */
int PartitionAllocMemBlockByCommandRaw(Partition *partition, SceKernelAllocMemBlockCommand *command)
{
	MemBlockObject *base = NULL, *block;
	SceUInt32 cdram, attr, extra = 0;
	SceSize align;
	uintptr_t pa;
	int ret;

	cdram = command->typeInfo->attr & MEMBLOCK_TYPE_CDRAM;
	AllocMemBlockCommandAssertCdramCode(command->code, cdram);
	if (cdram == 0) {
		if (command->opt.attr & ALLOC_ATTR_HAS_VBASE)
			ret = PartitionAllocMemBlockCoreByAddrByCommandRaw(partition, command,
									   command->opt.vbase);
		else
			ret = PartitionAllocMemBlockCoreByCommandRaw(partition, command);
		goto name;
	}
	attr = command->opt.attr;
	if ((attr & (ALLOC_ATTR_NOPHYPAGE | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR)) ==
	    (ALLOC_ATTR_NOPHYPAGE | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR)) {
		pa = command->opt.paddr;
		attr &= ~(ALLOC_ATTR_NOPHYPAGE | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR);
	} else if ((attr & (ALLOC_ATTR_SHARE_PHYPAGE | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_MIRROR_BLOCKID)) ==
		   (ALLOC_ATTR_SHARE_PHYPAGE | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_MIRROR_BLOCKID)) {
		base = SceSysmemForKernel_FA6C098C(command->opt.mirror_blockid);
		if (base == NULL) {
			command->block = NULL;
			return SCE_KERNEL_ERROR_PARTITION_ERROR;
		}
		if (command->size > base->size)
			command->size = base->size;
		pa = MemBlockCdramVirToPhy(base->code, base->vbase);
		attr = command->opt.attr;
		extra = MEMBLOCK_ATTR_KEEP_PHY;
	} else {
		align = (attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT) ?
			command->opt.alignment : 0;
		if (command->phyMemPart != NULL)
			ret = PhyMemPartCdramAlloc(command->phyMemPart, command->size, align, &pa);
		else
			ret = PhyMemCdramAlloc(command->size, align, &pa);
		if (ret < 0) {
			command->block = NULL;
			return ret;
		}
		attr = command->opt.attr;
	}
	command->opt.paddr = pa;
	command->opt.attr = attr | ALLOC_ATTR_HAS_VBASE | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	ret = PartitionAllocMemBlockCoreByAddrByCommandRaw(partition, command,
							   MemBlockCdramPhyToVir(command->code, pa));
	if (ret < 0)
		return ret;
	block = command->block;
	block->attr |= extra;
	if (!(command->opt.attr & ALLOC_ATTR_CLIENT_INFO) && base != NULL) {
		if ((block->flags & MEMBLOCK_OPT_MASK) == MEMBLOCK_OPT_BASE_BLOCK) {
			/* AllocMemBlockCommandSetOpt has set it */
			(void)MemBlockOptGetBaseBlockRaw(block);
		} else {
			SCE_DBG_PRINTF(0xb, 5614, "->MemBlockOptSetBaseBlock(), status=0x%08x\n",
				       block->flags);
			MemBlockOptSetBaseBlockRaw(command->block, base);
		}
	}
name:
	if (command->block != NULL)
		MemBlockSetNameRaw(command->block, command->name);
	if (ret < 0)
		return ret;
	MemBlockFill(command->block);
	return ret;
}

/* name unknown: a block of the code in the partition, backed from phyMemPart */
int PartitionAllocMemBlockWithPhyMemPart(Partition *partition, void *phyMemPart, const char *name,
					 SceUInt32 code, SceSize size,
					 const SceKernelAllocMemBlockKernelOpt *opt,
					 MemBlockObject **block)
{
	SceKernelAllocMemBlockCommand command;
	int ret;

	AllocMemBlockCommandInit(&command, name, size);
	command.phyMemPart = phyMemPart;
	command.partition = partition;
	command.code = code;
	/* the type is looked up for the kernel code whatever the block's */
	ret = MemBlockCode2TypeInfo(MEMBLOCK_CODE_KERNEL_ROOT_RW, &command.typeInfo);
	if (ret < 0)
		return ret;
	if (opt != NULL)
		memcpy(&command.opt, opt, sizeof(command.opt));
	ret = PartitionAllocMemBlockByCommandRaw(partition, &command);
	*block = command.block;
	return ret;
}

#define AS_L2_TABLES_SIZE		0x4000	/* a slot per MiB of the 4 GiB */

/* name unknown: the address space's L2 table slots, zeroed */
int AddressSpaceContextAllocL2Tables(AddressSpaceContext *context)
{
	MemBlockObject *block;
	void *phyMemPart;
	int ret;

	context->l2Tables = NULL;
	context->l2TablesBlock = NULL;
	phyMemPart = SceSysmemForKernel_0164D817();
	ret = PartitionAllocMemBlockWithPhyMemPart(context->partition, phyMemPart, "SceKernelASL2PTV",
						   MEMBLOCK_CODE_KERNEL_ROOT_RW, AS_L2_TABLES_SIZE, NULL,
						   &block);
	if (ret < 0)
		return ret;
	MemBlockSetNameRaw(block, "SceKernelASL2PTV");
	context->l2Tables = (SceUInt32 *)block->vbase;
	context->l2TablesBlock = block;
	memset(context->l2Tables, 0, AS_L2_TABLES_SIZE);
	return 0;
}

/* PartitionL2CAllocL2PageTableRaw flags: the caller holds l2Lock */
#define PARTITION_L2C_LOCKED		0x02
#define PARTITION_L2C_CACHE_MIN		3

/* a block with a free quarter: one on the partition's list, else a new one */
static inline MemBlockObject *PartitionL2CAllocMemBlockForL2PageTableRaw(Partition *partition,
									 SceUInt32 flags)
{
	MemBlockObject *block;
	const char *name;
	SceUInt32 smbType;
	int intr, free, ret;

	intr = SceCpuForDriver_4C38CE4D(&partition->l2BlockLock);
	block = partition->l2Blocks;
	if (block != NULL) {
		for (;;) {
			free = MemBlockL2PageTableFreeCount(block);
			if (free == 0) {
				partition->l2Blocks = block->smbNext;
				block->smbNext = NULL;
				break;
			}
			if (free > 0) {
				partition->l2Blocks = block->smbNext;
				block->smbNext = NULL;
				sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&partition->l2BlockLock,
										intr);
				return block;
			}
		}
	}
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&partition->l2BlockLock, intr);
	name = SysrootGetMemBlockName(2);
	if (partition == g_uidManager->kernelPartition)
		smbType = ((flags & 1) << 4) | 0x21;
	else
		smbType = 1;
	ret = AllocSimpleMemBlock(name, 0x1001000, NULL, smbType, &block);
	if (ret < 0) {
		if (ret == SCE_KERNEL_ERROR_NO_SMB)
			SceDebugForDriver_391B5B74(SCE_DBGINFO_FUNC(4717, "PartitionL2CAllocMemBlockForL2PageTableRaw"),
					__builtin_return_address(0));
		SceDebugForDriver_391B5B74(SCE_DBGINFO_FUNC(4719, "PartitionL2CAllocMemBlockForL2PageTableRaw"),
				__builtin_return_address(0));
	}
	block->flags |= MEMBLOCK_L2PT;
	MemBlockL2PageTableSetFreeRaw(block, MEMBLOCK_L2PT_FREE_MASK);
	block->lock = 0;
	return block;
}

/* the address space's L2 table slots, allocated on first use */
static inline SceUInt32 *PartitionL2CAllocL2PageTableVectorR1(Partition *partition)
{
	AddressSpaceContext *context = partition->tiny.context;
	SceUInt32 *tables = context->l2Tables;

	if (tables == NULL) {
		if (partition->attr & 1) {
			SCE_DBG_ASSERT_CHECK(tables != NULL, 4525);
			return NULL;
		}
		if (AddressSpaceContextAllocL2Tables(context) < 0)
			return NULL;
		tables = context->l2Tables;
	}
	return tables;
}

/*
 * An L2 page table for the MiB at va: the object from the partition's cache,
 * the table itself a quarter of a simple memblock, hooked into the L1 table
 * and the address space's slots.
 */
int PartitionL2CAllocL2PageTableRaw(Partition *partition, uintptr_t va, SceUInt32 flags,
				    L2PageTable **out)
{
	AddressSpaceContext *context;
	MemBlockObject *block;
	L2PageTable *l2pt;
	SceUInt32 *tables, *slot, quarter;
	uintptr_t tableVa, tablePa;
	int intr, l2Intr = 0, ret;

	if (!(flags & PARTITION_L2C_LOCKED)) {
		/* keep a few in the cache: no allocation under l2Lock */
		intr = SceCpuForDriver_4C38CE4D(&partition->l2CacheLock);
		while (partition->l2CacheCount < PARTITION_L2C_CACHE_MIN) {
			AllocL2PageTableObject(partition, partition->tiny.context->l2ptHeap, flags, &l2pt);
			l2pt->next = partition->l2Cache;
			partition->l2Cache = l2pt;
			partition->l2CacheCount++;
		}
		sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&partition->l2CacheLock, intr);
		l2Intr = SceCpuForDriver_4C38CE4D(&partition->l2Lock);
	}
	context = partition->tiny.context;
	intr = SceCpuForDriver_4C38CE4D(&partition->l2CacheLock);
	if (partition->l2CacheCount == 0) {
		ret = AllocL2PageTableObject(partition, partition->tiny.context->l2ptHeap, flags, &l2pt);
		if (ret < 0) {
			sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&partition->l2CacheLock, intr);
			goto out;
		}
	} else {
		l2pt = partition->l2Cache;
		partition->l2Cache = l2pt->next;
		l2pt->next = NULL;
		partition->l2CacheCount--;
	}
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&partition->l2CacheLock, intr);

	block = PartitionL2CAllocMemBlockForL2PageTableRaw(partition, flags);
	l2pt->block = block;
	ret = MemBlockL2PageTableAllocFreeArea(block, &quarter, &tableVa, &tablePa);
	if (ret < 0)
		goto out;
	switch (quarter) {
	case 0x1000000:
		l2pt->flags |= 1;
		break;
	case 0x2000000:
		l2pt->flags |= 2;
		break;
	case 0x4000000:
		l2pt->flags |= 4;
		break;
	case 0x8000000:
		l2pt->flags |= 8;
		break;
	default:
		SCE_DBG_ASSERT_FAIL(4802);
		ret = SCE_KERNEL_ERROR_PARTITION_ERROR;
		goto out;
	}
	l2pt->tablePa = tablePa;
	l2pt->vbase = va & ~0xfffff;
	l2pt->table = (uint32_t *)tableVa;
	ret = L2PageTableInit(l2pt);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(4821);
		goto out;
	}
	ret = L1PageTableSetL2PageTable(context->l1pt, partition->domain, l2pt->vbase, l2pt->tablePa);
	if (ret < 0)
		goto out;
	tables = PartitionL2CAllocL2PageTableVectorR1(partition);
	if (tables != NULL) {
		slot = &tables[l2pt->vbase >> 20];
		if (AS_L2_SLOT_TYPE(*slot) == AS_L2_SLOT_BLOCK)
			l2pt->firstBlock = (MemBlockObject *)AS_L2_SLOT_PTR(*slot);
		*slot = ((uintptr_t)l2pt & ~3u) | AS_L2_SLOT_TABLE;
		l2pt->partition = partition;
		SceCpuForKernel_9DA58A9E((uint32_t *)&partition->l2Count);
	}
	if (MemBlockL2PageTableFreeCount(block) > 0) {
		intr = SceCpuForDriver_4C38CE4D(&partition->l2BlockLock);
		block->smbNext = partition->l2Blocks;
		partition->l2Blocks = block;
		sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&partition->l2BlockLock, intr);
	}
	*out = l2pt;
out:
	if (!(flags & PARTITION_L2C_LOCKED))
		sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&partition->l2Lock, l2Intr);
	return ret;
}

/* PartitionFindL2PageTable, PartitionMapPages flags */
#define PARTITION_MAP_LOCKED		0x01	/* the caller holds mapLock */

/* the L2 page table covering [va, va + size), which must sit in one MiB */
int PartitionFindL2PageTable(Partition *partition, uintptr_t va, SceSize size, SceUInt32 flags,
			     L2PageTable **out)
{
	L2PageTable *l2pt;
	SceUInt32 slot;
	int intr;

	if ((va >> 20) != ((va + size - 1) >> 20)) {
		*out = NULL;
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	intr = (flags & PARTITION_L2C_LOCKED) ? 0 :
	       SceCpuForDriver_4C38CE4D(&partition->l2Lock);
	slot = partition->tiny.context->l2Tables[va >> 20];
	l2pt = AS_L2_SLOT_TYPE(slot) == AS_L2_SLOT_TABLE ? (L2PageTable *)AS_L2_SLOT_PTR(slot) : NULL;
	if (!(flags & PARTITION_L2C_LOCKED))
		sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&partition->l2Lock, intr);
	*out = l2pt;
	return l2pt != NULL ? 0 : SCE_KERNEL_ERROR_NO_L2PAGETABLE;
}

/* name unknown: where the partition's blocks of the code get their pages */
int PartitionGetPhyMemPart(Partition *partition, SceUInt32 code, void **phyMemPart)
{
	if (partition->addressSpace != NULL)
		return AddressSpaceGetPhyMemPart(partition->addressSpace, code, phyMemPart);
	*phyMemPart = SceSysmemForKernel_0164D817();
	return 0;
}

/* name unknown: a virtual page of the partition given back */
void PartitionVirPageRelease(Partition *partition)
{
	SceCpuForKernel_0E04BD2E((uint32_t *)&partition->virPageCount);
}

#define PARTITION_ATTR_1		0x01	/* PLACEHOLDER: not supported here */
#define PARTITION_ATTR_KEEP		0x08	/* PLACEHOLDER: name; never deleted */

/*
 * A partition of [vbase, vbase + size) in the context's address space. It is
 * returned even when the parameters turn out wrong.
 */
Partition *PartitionInit(const char *name, SceUInt8 domain, uintptr_t vbase, SceSize size,
			 const PartitionInitParam *param)
{
	AddressSpaceContext *context;
	Partition *partition;
	SceUInt32 attr;
	int ret;

	partition = SceSysmemForKernel_85571907(sizeof(*partition));
	if (partition == NULL)
		return NULL;
	SetMemory32Core(partition, 0, sizeof(*partition));
	context = param->context;
	partition->refCount = 0;
	partition->tiny.base.sce_class = s_partitionClass;
	ret = TinyPartitionInitRaw(&partition->tiny, name, vbase, size, context);
	if (ret < 0 || param->size != sizeof(*param))
		return partition;
	attr = param->attr;
	if (attr & ~0xf)
		return partition;
	partition->entryAttr = param->entryAttr;
	partition->entryAttrMask = param->entryAttrMask;
	if (attr & PARTITION_ATTR_1) {
		SCE_DBG_ASSERT_FAIL(5981);
		return partition;
	}
	partition->l2Lock = 0;
	partition->attr = attr;
	partition->domain = domain;
	partition->l2Count = 0;
	partition->virPageCount = 0;
	partition->addressSpace = NULL;
	partition->reserved = 0;
	ret = TinyPartitionInitList(&partition->tiny);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 6004, "TinyPartitionInitList failed 0x%08x\n", ret);
		return partition;
	}
	partition->l2BlockLock = 0;
	partition->l2Blocks = NULL;
	partition->l2CacheLock = 0;
	partition->l2Cache = NULL;
	partition->l2CacheCount = 0;
	partition->mapLock = 0;
	partition->pid = -1;
	partition->uid = -1;
	return partition;
}

/* waits for the last reference, frees the cached L2 tables and the partition */
int DeletePartition(Partition *partition)
{
	L2PageTable *l2pt;
	int intr, ret;

	if (partition->attr & PARTITION_ATTR_KEEP)
		return 0;
	while ((ret = SceCpuForDriver_CDA96E81(&partition->refCount, 0, -1)) != 0)
		SCE_DBG_PRINTF(3, 6240, "PartitionInvalidate failed 0x%08x, 0x%08x\n", ret,
			       partition->refCount);
	intr = SceCpuForDriver_4C38CE4D(&partition->l2CacheLock);
	while (partition->l2CacheCount != 0) {
		l2pt = partition->l2Cache;
		partition->l2Cache = l2pt->next;
		l2pt->next = NULL;
		partition->l2CacheCount--;
		L2PageTableFree(partition->tiny.context->l2ptHeap, l2pt);
	}
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&partition->l2CacheLock, intr);
	SetMemory32Core(partition, TINY_PARTITION_DEAD, sizeof(*partition));
	SceSysmemForKernel_4233C16D(partition);
	return 0;
}

/* a new virtual page, for virpage.c */
int PartitionAllocVirPage(Partition *partition, VirPage **vp)
{
	return PartitionAllocVirPageRaw(partition, vp);
}

/* the L2 page table for the MiB at va, kept (0x8000) even when it empties */
int PartitionReserveSection(Partition *partition, uintptr_t va)
{
	L2PageTable *l2pt;
	int intr, ret;

	intr = SceCpuForDriver_4C38CE4D(&partition->l2Lock);
	ret = PartitionFindL2PageTable(partition, va, 0x1000, PARTITION_L2C_LOCKED, &l2pt);
	if (ret < 0) {
		ret = PartitionL2CAllocL2PageTableRaw(partition, va, PARTITION_L2C_LOCKED, &l2pt);
		if (ret >= 0)
			l2pt->flags |= 0x8000;
	}
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&partition->l2Lock, intr);
	return ret;
}

/* [va, va + size) in one MiB mapped to pa, its L2 page table made if needed */
int PartitionMapPages(Partition *partition, SceUInt32 code, uintptr_t va, SceSize size,
		      uintptr_t pa, SceUInt32 flags)
{
	L2PageTable *l2pt;
	int mapIntr = 0, l2Intr, ret;

	if (!(flags & PARTITION_MAP_LOCKED))
		mapIntr = SceCpuForDriver_4C38CE4D(&partition->mapLock);
	if (!(flags & PARTITION_L2C_LOCKED))
		l2Intr = SceCpuForDriver_4C38CE4D(&partition->l2Lock);
	else
		l2Intr = 0;
	ret = PartitionFindL2PageTable(partition, va, size, flags | 3, &l2pt);
	if (ret < 0)
		ret = PartitionL2CAllocL2PageTableRaw(partition, va, flags | 3, &l2pt);
	if (!(flags & PARTITION_L2C_LOCKED))
		sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&partition->l2Lock, l2Intr);
	if (ret >= 0)
		ret = L2PageTableMapPages(l2pt, code, va, size, pa);
	if (!(flags & PARTITION_MAP_LOCKED))
		sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&partition->mapLock, mapIntr);
	return ret;
}

int PartitionRemapPages(Partition *partition, MemBlockObject *block, SceUInt32 type,
			uintptr_t va, SceSize size, uintptr_t pa, SceUInt32 flags)
{
	L2PageTable *l2pt;
	int intr, ret;

	intr = SceCpuForDriver_4C38CE4D(&partition->mapLock);
	ret = PartitionFindL2PageTable(partition, va, size, PARTITION_MAP_LOCKED, &l2pt);
	if (ret >= 0)
		ret = L2PageTableRemapPages(l2pt, block, type, va, size, pa, flags);
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&partition->mapLock, intr);
	return ret;
}

/* [va, va + size) in one MiB unmapped (mode 1: kept reserved) */
int PartitionUnmapL2PageTable(Partition *partition, uintptr_t va, SceSize size, SceUInt32 mode,
			      SceUInt32 flags)
{
	AddressSpaceContext *context;
	L2PageTable *l2pt;
	int intr = 0, ret;

	if (!(flags & PARTITION_MAP_LOCKED))
		intr = SceCpuForDriver_4C38CE4D(&partition->mapLock);
	context = partition->tiny.context;
	if (mode >> 1) {
		SCE_DBG_ASSERT_FAIL(6398);
		ret = SCE_KERNEL_ERROR_INVALID_FLAGS;
	} else {
		ret = PartitionFindL2PageTable(partition, va, size, flags | PARTITION_MAP_LOCKED,
					       &l2pt);
		if (ret >= 0 || (ret = PartitionL2CAllocL2PageTableRaw(partition, va,
					flags | PARTITION_MAP_LOCKED, &l2pt)) >= 0)
			ret = L2PageTableUnmapPages(l2pt, (SceUInt8)context->mmu.contextId, va, size,
						    mode);
	}
	if (!(flags & PARTITION_MAP_LOCKED))
		sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&partition->mapLock, intr);
	return ret;
}

/* the MiBs of [va, va + size) whose L2 page tables map nothing freed */
void PartitionFindAndTryRemoveL2PageTable(Partition *partition, uintptr_t va, SceSize size)
{
	AddressSpaceContext *context;
	L2PageTable *found, *l2pt;
	int intr, l2Intr, ret;

	intr = SceCpuForDriver_4C38CE4D(&partition->mapLock);
	context = partition->tiny.context;
	for (; size != 0; size -= 0x100000, va += 0x100000) {
		if (PartitionFindL2PageTable(partition, va, 0x100000, 0, &found) < 0)
			continue;
		l2pt = found;
		if (L2PageTableGetPageCount(l2pt) != 0 || (l2pt->flags & 0x8000))
			continue;
		l2Intr = SceCpuForDriver_4C38CE4D(&partition->l2Lock);
		partition->tiny.context->l2Tables[l2pt->vbase >> 20] = 0;
		sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&partition->l2Lock, l2Intr);
		SceCpuForKernel_0E04BD2E((uint32_t *)&partition->l2Count);
		ret = PartitionAsCommonFreeL2PageTable(partition, context, l2pt, 1);
		if (ret < 0)
			SCE_DBG_PRINTF(0xb, 6469, "PartitionCheckFreeL2PageTable failed 0x%08x\n",
				       ret);
	}
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&partition->mapLock, intr);
}

static inline int TinyPartitionHasAddr(const TinyPartition *tiny, uintptr_t va)
{
	return va >= tiny->vbase && va - tiny->vbase <= tiny->size;
}

/* the L2 page table holding va, if any */
static inline L2PageTable *PartitionGetL2PageTable(Partition *partition, uintptr_t va)
{
	SceUInt32 slot;
	int intr;

	intr = SceCpuForDriver_4C38CE4D(&partition->l2Lock);
	slot = partition->tiny.context->l2Tables[va >> 20];
	if (AS_L2_SLOT_TYPE(slot) != AS_L2_SLOT_TABLE) {
		sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&partition->l2Lock, intr);
		return NULL;
	}
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&partition->l2Lock, intr);
	return (L2PageTable *)AS_L2_SLOT_PTR(slot);
}

/* the memory type va is mapped with, 0 outside the partition */
int PartitionGetMemoryType(Partition *partition, uintptr_t va)
{
	L2PageTable *l2pt;
	int type;

	if (!TinyPartitionHasAddr(&partition->tiny, va))
		return 0;
	type = L1PageTableGetMemoryType(partition->tiny.context->l1pt, va);
	switch (type) {
	case 0:
		l2pt = PartitionGetL2PageTable(partition, va);
		if (l2pt == NULL)
			return SCE_KERNEL_ERROR_PARTITION_ERROR;
		return sceKernelMMUL2GetMemoryType(l2pt->table, va);
	case 2:
	case 8:
	case 0x40:
	case 0x80:
	case 0xd0:
		return type;
	default:
		SCE_DBG_ASSERT_FAIL(6575);
		return type;
	}
}

/* name unknown: va's physical address from the page tables */
int PartitionVAtoPABySW(Partition *partition, uintptr_t va, uintptr_t *pa)
{
	L2PageTable *l2pt;
	int ret;

	if (!TinyPartitionHasAddr(&partition->tiny, va))
		return PARTITION_WALK_NOT_IN;
	ret = L1PageTableVAtoPABySW(partition->tiny.context->l1pt, va, pa);
	if (ret != MMU_WALK_PAGE_TABLE)
		return ret;
	l2pt = PartitionGetL2PageTable(partition, va);
	if (l2pt == NULL)
		return SCE_KERNEL_ERROR_PARTITION_ERROR;
	return MMUL2VAtoPABySW(l2pt->table, va, pa);
}

/* whether va is accessible with perm, from the page tables */
int PartitionIsEqualAccessibleBySW(Partition *partition, SceUInt32 perm, uintptr_t va,
				   uintptr_t *pa)
{
	L2PageTable *l2pt;
	int ret;

	if (!TinyPartitionHasAddr(&partition->tiny, va))
		return PARTITION_WALK_NOT_IN;
	if (partition->tiny.context == NULL) {
		SCE_DBG_PRINTF(0xb, 6670, "PartitionGetAC(%p) is NULL\n", partition);
		SCE_DBG_ASSERT_FAIL(6671);
		return SCE_KERNEL_ERROR_PARTITION_ERROR;
	}
	ret = L1PageTableIsEqualAccessibleBySW(partition->tiny.context->l1pt, perm, va, pa);
	if (ret != MMU_WALK_PAGE_TABLE)
		return ret;
	l2pt = PartitionGetL2PageTable(partition, va);
	if (l2pt == NULL)
		return SCE_KERNEL_ERROR_PARTITION_ERROR;
	return MMUL2IsEqualAccessibleBySW(l2pt->table, perm, va, pa);
}

/* name unknown: the one physical range a list block maps */
int PartitionGetMemBlockPARange(Partition *partition, MemBlockObject *block,
				SceKernelPARange *range)
{
	VirPage *vp;
	uintptr_t pbase;
	SceSize size;
	int found = 0, ret = 0;

	for (vp = MemBlockGetVirPageRaw(block); vp != NULL; vp = vp->next) {
		ret = VirPageGetMapRange(vp, &pbase, &size);
		if (ret < 0)
			break;
		if (size == 0)
			continue;
		if (found)
			return SCE_KERNEL_ERROR_NOT_PHY_CONT_MEMBLOCK;
		found = 1;
		range->addr = pbase;
		range->size = size;
	}
	return ret;
}

/* name unknown: the physical ranges a list block maps, as many as fit */
int PartitionGetMemBlockPAVector(Partition *partition, MemBlockObject *block,
				 SceKernelPAVector *vector)
{
	SceKernelPARange *range;
	VirPage *vp;
	uintptr_t pbase;
	SceSize size;

	vector->nDataInVector = 0;
	vector->total = 0;
	range = vector->pRanges;
	for (vp = MemBlockGetVirPageRaw(block); vp != NULL; vp = vp->next) {
		if (VirPageGetMapRange(vp, &pbase, &size) < 0)
			break;
		if (size == 0)
			continue;
		if (vector->total <= vector->capacity) {
			range->addr = pbase;
			range->size = size;
			range++;
			vector->nDataInVector++;
		}
		vector->total++;
	}
	return 0;
}

/* name unknown: by command, for the other files */
int PartitionAllocMemBlockByCommand(Partition *partition, SceKernelAllocMemBlockCommand *command)
{
	return PartitionAllocMemBlockByCommandRaw(partition, command);
}

/* name unknown */
int PartitionAllocMemBlockWithPhyMemPartExt(Partition *partition, void *phyMemPart,
					    const char *name, SceUInt32 code, SceSize size,
					    const SceKernelAllocMemBlockKernelOpt *opt,
					    MemBlockObject **block)
{
	return PartitionAllocMemBlockWithPhyMemPart(partition, phyMemPart, name, code, size, opt,
						    block);
}

/* name unknown: a block of size for simple memblocks, in the kernel partition */
int PartitionAllocSimpleMemBlock(Partition *partition, SceUInt32 smbType, SceSize size,
				 MemBlockObject **block)
{
	SceKernelAllocMemBlockCommand command;
	int ret;

	SCE_DBG_ASSERT_CHECK(partition == g_uidManager->kernelPartition, 6854);
	AllocMemBlockCommandInit(&command, NULL, size);
	command.code = MEMBLOCK_CODE_KERNEL_ROOT_RW;
	ret = MemBlockCode2TypeInfo(MEMBLOCK_CODE_KERNEL_ROOT_RW, &command.typeInfo);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(6863);
		return ret;
	}
	command.block = NULL;
	command.partition = partition;
	ret = PartitionAllocMemBlockInListByCommandRaw(partition, &command, size);
	if (ret >= 0)
		*block = command.block;
	return ret;
}

/* name unknown: an address range for a virtual slot of the code */
int PartitionAllocVSlot(Partition *partition, SceUInt32 code, SceSize size, MemBlockObject **slot)
{
	SceKernelAllocMemBlockCommand command;
	int ret;

	AllocMemBlockCommandInit(&command, NULL, size);
	command.code = code;
	ret = MemBlockCode2TypeInfo(code, &command.typeInfo);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(6939);
		return ret;
	}
	command.block = NULL;
	command.partition = partition;
	ret = PartitionAllocMemBlockInListByCommandRaw(partition, &command, size);
	if (ret >= 0) {
		*slot = command.block;
		(*slot)->partition = partition;
		(*slot)->code = code;
	}
	return ret;
}

/* name unknown: a block and its base freed, for the other files */
int PartitionFreeMemBlockWithBase(Partition *partition, MemBlockObject *block,
				  MemBlockObject **baseBlock, void **baseInfo)
{
	return PartitionFreeMemBlockWithBaseRaw(partition, block, baseBlock, baseInfo);
}

#define MEMBLOCK_FLAG_ZOMBIE		0x20	/* freed while still mapped elsewhere */

/*
 * A block freed, then each base block it mirrored once its last mirror is
 * gone and it was freed already (a zombie).
 */
int PartitionFreeMemBlock(Partition *partition, MemBlockObject *block)
{
	MemBlockObject *base = NULL;
	MemBlockBaseInfo *info = NULL, *cur;
	Partition *part2;
	int ret;

	if (MemBlockGetLockRef(block) != 0)
		return SCE_KERNEL_ERROR_BLOCK_IN_USE;
	if (MemBlockGetMapRef(block) != 0) {
		SCE_DBG_PRINTF(0xb, 7009, "Make ZOMBIE Status, %p[%s]\n", block,
			       MemBlockGetNameRaw(block) != NULL ? MemBlockGetNameRaw(block) : "NULL");
		block->flags |= MEMBLOCK_FLAG_ZOMBIE;
		return 0;
	}
	SCE_DBG_ASSERT_CHECK(partition == block->partition, 7015);
	ret = PartitionFreeMemBlockWithBaseRaw(partition, block, &base, (void **)&info);
	/* the blocks freeing this one released, while they are zombies */
	for (block = base, cur = info; block != NULL || cur != NULL;
	     block = base, cur = info) {
		base = NULL;
		info = NULL;
		if (cur != NULL)
			block = cur->block;
		MemBlockDecMapRef(block);
		if (MemBlockGetMapRef(block) != 0 || !(block->flags & MEMBLOCK_FLAG_ZOMBIE))
			goto done;
		part2 = block->partition;
		if (part2->tiny.magic != TINY_PARTITION_MAGIC) {
			SCE_PRINTF_WITH_INFO(0xb, 7057, "pPart2=%p[%s] is deleted\n", part2,
					     part2->tiny.name);
			goto done;
		}
		if (cur == NULL) {
			ret = PartitionFreeMemBlockWithBaseRaw(partition == part2 ? partition : part2,
							       block, &base, (void **)&info);
		} else {
			ret = PIDFreeMemBlock(cur, &base, (void **)&info);
			if (ret < 0)
				SCE_DBG_PRINTF(0xb, 7065, "PIDFreeMemBlock(%p) failed 0x%08x\n", cur,
					       ret);
			SceSysmemForKernel_4233C16D(cur);
		}
	}
	return ret;
done:
	if (cur != NULL)
		SceSysmemForKernel_4233C16D(cur);
	return ret;
}

/* name unknown: an address space's L2 page tables and their slots freed */
int AddressSpaceContextFreeL2Tables(AddressSpaceContext *context)
{
	SceUInt32 *slot, *end;
	int ret;

	if (context == NULL)
		return 0;
	if (context->l2TablesBlock == NULL)
		return 0;
	slot = context->l2Tables;
	if (slot != NULL) {
		for (end = slot + AS_L2_TABLES_SIZE / 4; slot != end; slot++) {
			if (*slot == 0 || AS_L2_SLOT_TYPE(*slot) != AS_L2_SLOT_TABLE ||
			    AS_L2_SLOT_PTR(*slot) == 0)
				continue;
			PartitionAsCommonFreeL2PageTable(context->partition, context,
							 (L2PageTable *)AS_L2_SLOT_PTR(*slot), 0);
			*slot = 0;
		}
	}
	ret = PartitionFreeMemBlock(context->l2TablesBlock->partition, context->l2TablesBlock);
	context->l2Tables = NULL;
	context->l2TablesBlock = NULL;
	return ret;
}

/* every block of a dying process's partition freed, then the list itself */
int TinyPartitionFreeAllMemBlock(Partition *partition, TinyPartitionClientInfoList *list)
{
	TinyPartition *tiny = &partition->tiny;
	AddressSpaceContext *context;
	MemBlockObject *head, *cur, *next, *prev;
	struct FixedHeap *heap;
	SceUInt32 count, i;
	SceSize size;
	int intr;

	if (partition->attr & PARTITION_ATTR_KEEP)
		return 0;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&tiny->lock);
	head = tiny->blocks;
	context = tiny->context;
	if (head == NULL)
		goto out;
	cur = head->next;
	if (cur == head)
		goto empty;
	if (cur != NULL) {
		for (;; cur = next) {
			next = cur->next;
			if (cur->code != 0) {
				SCE_DBG_PRINTF(0, 3785,
					       "[DeleteProcess] Force free memblock: %p:0x%08x+0x%08x [%s]\n",
					       cur, cur->vbase, cur->size, MemBlockGetName(cur));
				if ((cur->flags & MEMBLOCK_OPT_MASK) == MEMBLOCK_OPT_CLIENT_INFO &&
				    cur->opt != 0 && list != NULL) {
					count = list->count;
					if (count < list->capacity && list->capacity != 0) {
						for (i = 0; list->entries[i] != NULL; )
							if (++i == list->capacity)
								goto full;
						list->entries[i] = (void *)cur->opt;
						list->count = count + 1;
					}
				}
full:
				MemBlockClearBaseInfo(cur);
				PartitionFreeMemBlockMappedToVirAllocRaw(partition, cur);
			}
			if (next == head)
				break;
			if (next == NULL)
				break;
		}
		cur = head->next;
		if (cur == head)
			goto empty;
	}
	do {
		prev = cur->prev;
		next = cur->next;
		size = cur->size;
		heap = context->memBlockHeap;
		prev->next = next;
		next->prev = prev;
		cur->next = cur;
		cur->prev = cur;
		FreeMemBlockObject(heap, cur);
		SceCpuForDriver_5F6A8743((SceInt32 *)&tiny->freeSize, size);
		SceCpuForKernel_0E04BD2E((uint32_t *)&tiny->blockCount);
		cur = next;
	} while (cur != head);
empty:
	head->next = head;
	head->prev = head;
	heap = context->memBlockHeap;
	tiny->magic = TINY_PARTITION_DEAD;
	FreeMemBlockObject(heap, head);
	tiny->blocks = NULL;
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&tiny->lock, intr);
	return 0;
}

/* name unknown: a list block mapped */
int PartitionMapMemBlock(Partition *partition, MemBlockObject *block,
			 const SceKernelAllocMemBlockCommand *command, SceUInt32 flags)
{
	if ((block->flags & MEMBLOCK_KIND_MASK) != MEMBLOCK_KIND_VPL) {
		SCE_DBG_PRINTF(0xb, 7147, "WARNING\n");
		return 0;
	}
	return MemBlockVirPageListMapRaw(block, command, partition, flags);
}

int PartitionUnmapMemBlock(Partition *partition, MemBlockObject *block)
{
	SCE_DBG_ASSERT_CHECK(partition == block->partition, 7161);
	return MemBlockUnmapRaw(block, block->partition);
}

int PartitionRemapMemBlock(Partition *partition, MemBlockObject *block, SceUInt32 type,
			   SceUInt32 flags)
{
	if ((block->flags & MEMBLOCK_KIND_MASK) == MEMBLOCK_KIND_VPL)
		return MemBlockVirPageListRemapRaw(block, block->partition, type, flags);
	SCE_DBG_PRINTF(0xb, 7187, "Cannot remap\n");
	SCE_DBG_ASSERT_FAIL(7188);
	return 0;
}

/* name unknown: the block holding va (or exactly [va, va + size)) */
int TinyPartitionFindMemBlock(TinyPartition *tiny, uintptr_t va, SceSize size,
			      MemBlockObject **block)
{
	MemBlockObject *link;
	int intr, ret;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&tiny->lock);
	link = AsCommonGetBlockLink(tiny->context->l2Tables, va);
	if (link != NULL) {
		ret = TinyPartitionFindBlockFrom(tiny, va, size, link, block);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&tiny->lock, intr);
		if (ret == 0)
			return 0;
	} else {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&tiny->lock, intr);
	}
	/* the hint was no good: search from the start */
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&tiny->lock);
	ret = TinyPartitionFindBlockFrom(tiny, va, size, tiny->blocks->next, block);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&tiny->lock, intr);
	return ret;
}

int PartitionGetMemBlockInfoByAddr(TinyPartition *tiny, uintptr_t va, SceUInt32 arg, void *info)
{
	MemBlockObject *block = NULL, *link;
	int intr, ret;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&tiny->lock);
	link = AsCommonGetBlockLink(tiny->context->l2Tables, va);
	if ((link != NULL && (ret = TinyPartitionFindBlockFrom(tiny, va, 0, link, &block)) >= 0) ||
	    (ret = TinyPartitionFindBlockFrom(tiny, va, 0, tiny->blocks->next, &block)) >= 0) {
		ret = MemBlockGetInfo(block, arg, info);
		if (ret < 0)
			SCE_DBG_PRINTF(0xb, 7262, "MemBlockGetInfo failed 0x%08x\n", ret);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&tiny->lock, intr);
	return ret;
}

/* name unknown: the blocks of [lo, hi) backed in [pa, pa + psize) */
int TinyPartitionGetMemBlockInfoListByPA(TinyPartition *tiny, uintptr_t lo, uintptr_t hi,
					 uintptr_t pa, SceSize psize, SceUInt32 arg,
					 MemBlockInfoEx *info, SceSize infoSize, SceUInt32 *count)
{
	return TinyPartitionGetMemBlockInfoList(tiny, 1, lo, hi, pa, psize, arg, info, infoSize,
						count);
}

/* name unknown: the blocks of [lo, hi) */
int TinyPartitionGetMemBlockInfoListByVA(TinyPartition *tiny, uintptr_t lo, uintptr_t hi,
					 SceUInt32 arg, MemBlockInfoEx *info, SceSize infoSize,
					 SceUInt32 *count)
{
	return TinyPartitionGetMemBlockInfoList(tiny, 0, lo, hi, 0, 0, arg, info, infoSize, count);
}

/* name unknown */
int PartitionGetInfo(Partition *partition, PartitionInfo *info)
{
	int intr;

	if (info->size != sizeof(*info))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&partition->tiny.lock);
	info->vbase = partition->tiny.vbase;
	info->vsize = partition->tiny.size;
	info->name = partition->tiny.name;
	info->blockCount = partition->tiny.blockCount & 0xffff;
	info->freeSize = partition->tiny.freeSize;
	if (info->size >= sizeof(*info))
		info->blockCountMax = partition->tiny.blockCount >> 16;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&partition->tiny.lock, intr);
	info->partition = partition;
	if (info->size >= sizeof(*info)) {
		info->l2Count = partition->l2Count & 0xffff;
		info->l2CountMax = partition->l2Count >> 16;
		info->virPageCount = partition->virPageCount & 0xffff;
		info->virPageCountMax = partition->virPageCount >> 16;
	}
	return 0;
}

/* the L2 page table for va, if there is one */
L2PageTable *SceSysmemForKernel_456ECB54(Partition *partition, uintptr_t va)
{
	SceUInt32 slot;
	L2PageTable *l2pt;
	int intr;

	intr = SceCpuForDriver_4C38CE4D(&partition->l2Lock);
	slot = partition->tiny.context->l2Tables[va >> 20];
	l2pt = AS_L2_SLOT_TYPE(slot) == AS_L2_SLOT_TABLE ? (L2PageTable *)AS_L2_SLOT_PTR(slot) : NULL;
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&partition->l2Lock, intr);
	return l2pt;
}

/* a partition with a GUID (param->uid, or a new one) */
SceUID sceKernelCreatePartition(const char *name, SceUInt8 domain, uintptr_t vbase, SceSize size,
				Partition **out, const PartitionInitParam *param)
{
	Partition *partition;
	SceUID uid;
	int ret;

	if (name == NULL) {
		SCE_DBG_ASSERT_FAIL(7430);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (vbase & 0xfffff) {
		SCE_DBG_ASSERT_FAIL(7434);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (size >= 0x100000 && (size & 0xfffff)) {
		SCE_DBG_ASSERT_FAIL(7438);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	partition = PartitionInit(name, domain, vbase, size, param);
	if (partition == NULL)
		return SCE_KERNEL_ERROR_PARTITION_ERROR;
	uid = param->uid;
	if (uid == -1) {
		uid = SceSysmemForKernel_6DB44A42(s_partitionClass, name, partition);
	} else {
		ret = sceGUIDSet(uid, s_partitionClass, NULL, (SceObjectBase *)partition);
		if (ret < 0)
			SCE_DBG_PRINTF(0xb, 7456, "sceGUIDSet failed 0x%08x\n", ret);
	}
	partition->pid = param->pid;
	partition->uid = uid;
	*out = partition;
	return uid;
}

/* name unknown: a kernel block with extraLow bytes below it */
SceUID SceSysmemForKernel_40870E74(const char *name, SceSize size, SceSize extraLow)
{
	SceKernelAllocMemBlockKernelOpt opt;
	Partition *partition = g_uidManager->kernelPartition;
	void *phyMemPart;
	MemBlockObject *block;
	int ret;

	opt.attr = 0x40000000 | ALLOC_ATTR_HAS_EXTRA_LOW;
	opt.size = sizeof(opt);
	if (sceKernelCheckDipsw(DIPSW_NO_CLEAR))
		opt.attr |= ALLOC_ATTR_NO_CLEAR;
	opt.extraLow = extraLow;
	opt.extraHigh = 0;
	opt.alignment = 0;
	ret = PartitionGetPhyMemPart(partition, MEMBLOCK_CODE_KERNEL_ROOT_RW, &phyMemPart);
	if (ret < 0)
		return ret;
	ret = PartitionAllocMemBlockWithPhyMemPart(partition, phyMemPart, name,
						   MEMBLOCK_CODE_KERNEL_ROOT_RW, size, &opt, &block);
	if (ret < 0)
		return ret;
	return MemBlockCreateGUID(name, block, opt.attr);
}

SceUID sceKernelAllocPartitionMemBlock(SceUID partUid, const char *name, SceUInt32 type,
					SceSize size, const SceKernelAllocMemBlockKernelOpt *opt)
{
	SceKernelAllocMemBlockCommand command;
	Partition *partition;
	void *phyMemPart;
	SceUInt32 code, attr;
	int ret;

	if (size & 0xfff) {
		SCE_PRINTF_WITH_INFO_FUNC(0, 7513, "sceKernelAllocPartitionMemBlock",
					  "Invalid argument:name=[%s],size=0x%08x\n", name, size);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	ret = sceGUIDGetObject(partUid, &partition);
	if (ret < 0)
		return ret;
	ret = MemBlockType2Code(type, &code);
	if (ret < 0)
		return ret;
	attr = opt != NULL ? opt->attr : 0;
	ret = PartitionGetPhyMemPart(partition, code, &phyMemPart);
	if (ret < 0)
		return ret;
	AllocMemBlockCommandInit(&command, name, size);
	command.code = code;
	command.phyMemPart = phyMemPart;
	command.partition = partition;
	ret = MemBlockCode2TypeInfo(code, &command.typeInfo);
	if (ret < 0)
		return ret;
	if (opt != NULL) {
		memcpy(&command.opt, opt, sizeof(command.opt));
		if (opt->attr & ALLOC_ATTR_KEEP_CLEAR)
			goto alloc;
	}
	if (sceKernelCheckDipsw(DIPSW_NO_CLEAR))
		command.opt.attr |= ALLOC_ATTR_NO_CLEAR;
alloc:
	ret = PartitionAllocMemBlockByCommandRaw(partition, &command);
	if (ret < 0)
		return ret;
	return MemBlockCreateGUID(name, command.block, attr);
}

/* sceKernelGetUIDPartitionClass */
SceClass *SceSysmemForKernel_5C0FC6BE(void)
{
	return s_partitionClass;
}

/* name unknown (SceSysmemForKernel_7DC46969 on the wiki) */
int SceSysmemForKernel_8450BFF7(void)
{
	return 1;
}

/* name unknown (SceSysmemForKernel_B339A865 on the wiki) */
int SceSysmemForKernel_320E907F(void)
{
	return 0;
}

/* name unknown: a partition's state by its GUID */
int SceSysmemForKernel_FB101B86(SceUID uid, PartitionInfo *info)
{
	Partition *partition = SceSysmemForKernel_FA6C098C(uid);

	if (partition == NULL)
		return SCE_KERNEL_ERROR_ERROR;
	return PartitionGetInfo(partition, info);
}

/* name unknown: the kernel's partition */
Partition *PartitionGetKernel(void)
{
	return g_uidManager->kernelPartition;
}

/* name unknown: a block freed from its own partition */
int MemBlockFreeFromPartition(MemBlockObject *block)
{
	return PartitionFreeMemBlock(block->partition, block);
}

/* SceUIDTinyPartitionClass */
static SceClass *s_tinyPartitionClass;
static Partition *s_kernelPartition;
static SceUInt32 s_partitionReserved0;
static SceUInt32 s_partitionReserved1;

#define PARTITION_ASLR_CODE		0x1060d004
#define PARTITION_MAGIC_VA		0x07f00000

/*
 * The classes; then the kernel partition's layout randomised by up to three
 * pad blocks ("SceASLR") and a MiB taken at a fixed address ("SceMagic").
 */
int PartitionModuleStart(const UIDManager *manager)
{
	SceSize sizes[3] = { 0x1000, 0x10000, 0x100000 };
	SceUInt32 masks[3] = { 0xf, 0x7, 0x3 };
	Partition *kernel = g_uidManager->kernelPartition;
	SceKernelAllocMemBlockCommand command;
	MemBlockObject *block;
	SceUInt32 random;
	SceSize size;
	int i, ret;

	s_partitionClass = manager->partitionClass;
	s_kernelPartition = manager->kernelPartition;
	s_tinyPartitionClass = manager->tinyPartitionClass;
	s_partitionReserved0 = manager->partitionReserved[0];
	s_partitionReserved1 = manager->partitionReserved[1];
	s_tinyPartitionClass->name = "SceUIDTinyPartitionClass";
	s_tinyPartitionClass->create_cb = NULL;
	s_tinyPartitionClass->destroy_cb = NULL;
	s_partitionClass->create_cb = NULL;
	s_partitionClass->destroy_cb = NULL;
	s_partitionClass->name = "SceUIDPartitionClass";
	for (i = 0; i < 3; i++) {
		ret = SceKernelUtilsForDriver_875B2A1C(&random, sizeof(random));
		if (ret < 0) {
			SCE_DBG_ASSERT_FAIL(7790);
			return ret;
		}
		size = (masks[i] & random) * sizes[i];
		if (size == 0)
			continue;
		AllocMemBlockCommandInit(&command, NULL, size);
		command.code = PARTITION_ASLR_CODE;
		MemBlockCode2TypeInfo(PARTITION_ASLR_CODE, &command.typeInfo);
		command.partition = kernel;
		command.block = NULL;
		ret = PartitionAllocMemBlockInListByCommandRaw(kernel, &command, sizes[i]);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 7801,
				       "PartitionAllocMemBlockInListByCommandRaw failed 0x%08x\n", ret);
			return ret;
		}
		block = command.block;
		block->partition = kernel;
		MemBlockSetPhyMemPartRaw(block, SceSysmemForKernel_0164D817());
		block->code = PARTITION_ASLR_CODE;
		MemBlockSetNameRaw(block, "SceASLR");
	}
	AllocMemBlockCommandInit(&command, NULL, 0x100000);
	command.code = PARTITION_ASLR_CODE;
	MemBlockCode2TypeInfo(PARTITION_ASLR_CODE, &command.typeInfo);
	command.partition = kernel;
	command.block = NULL;
	ret = PartitionAllocMemBlockInListByAddrByCommandRaw(kernel, &command, PARTITION_MAGIC_VA);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 7817,
			       "PartitionAllocMemBlockInListByAddrByCommandRaw failed 0x%08x\n", ret);
		return ret;
	}
	block = command.block;
	block->partition = kernel;
	MemBlockSetPhyMemPartRaw(block, SceSysmemForKernel_0164D817());
	block->code = PARTITION_ASLR_CODE;
	MemBlockSetNameRaw(block, "SceMagic");
	return ret;
}

/* the user shared tiny partition's context: only its memblock heap */
static AddressSpaceContext s_userSharedContext;

/* the tiny partition [0xe0000000, 0xf0000000) that user shared blocks come from */
int CreateTinyPartitionRaw(void)
{
	AddressSpaceObject *as;
	TinyPartition *tiny;
	int ret;

	if (s_tinyPartitionUserShared != NULL)
		return 0;
	as = SceSysmemForKernel_FA6C098C(SCE_KERNEL_ADDRESS_SPACE_ID);
	s_userSharedContext.memBlockHeap = as->context->memBlockHeap;
	tiny = SceSysmemForKernel_85571907(sizeof(*tiny));
	if (tiny == NULL)
		return SCE_KERNEL_ERROR_HEAPLIB_NOMEM;
	memset(tiny, 0, sizeof(*tiny));
	tiny->base.sce_class = s_tinyPartitionClass;
	ret = TinyPartitionInitRaw(tiny, "SceKernelUserShared", 0xe0000000, 0x10000000,
				   &s_userSharedContext);
	if (ret < 0)
		return ret;
	ret = TinyPartitionInitList(tiny);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 3140, "TinyPartitionInitList failed 0x%08x\n", ret);
		return ret;
	}
	s_tinyPartitionUserShared = tiny;
	sceGUIDSet(SCE_KERNEL_PARTITION_USER_SHARED_ID, s_partitionClass, NULL, (SceObjectBase *)tiny);
	return 0;
}
