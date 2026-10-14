/*
 * SceSysmem: memblock state changes (Sony's memblock/status_module.h).
 * The state is the memblock flags' bits 0..4: 1 free, 2 virtually allocated.
 */

#pragma once

#include "../memblock_obj_module.h"

#pragma push_macro("__FILE_NAME__")
#undef __FILE_NAME__
#define __FILE_NAME__ "memblock/status_module.h"

#define MEMBLOCK_STATUS_MASK		0x1f
#define MEMBLOCK_STATE_FREE		0x01
#define MEMBLOCK_STATE_VIR_ALLOC	0x02	/* VA: an address range */
#define MEMBLOCK_STATE_LIST_ALLOC	0x04	/* LA: in the partition's list */
#define MEMBLOCK_STATE_PHY_ALLOC	0x08	/* PA: physical pages behind it */
#define MEMBLOCK_STATE_MAPPED		0x10	/* MA: mapped */

static inline void MemBlockStatusSet(MemBlockObject *block, SceUInt32 state)
{
	block->flags = (block->flags & ~MEMBLOCK_STATUS_MASK) | state;
}

static inline void MemBlockStatusStateChangeVirAllocFromFreeRaw(MemBlockObject *block)
{
	SceUInt32 flags = block->flags;

	if ((flags & 0x1f) != MEMBLOCK_STATE_FREE) {
		SCE_DBG_PRINTF_FUNC(0xb, 210, "MemBlockStatusStateChangeVirAllocFromFreeRaw",
				    "Illegal state: 0x%08x\n", flags & 0x1f);
		flags = block->flags;
	}
	block->flags = (flags & ~0x1f) | MEMBLOCK_STATE_VIR_ALLOC;
}

static inline void MemBlockStatusStateChangeVirAllocFromLARaw(MemBlockObject *block)
{
	SceUInt32 flags = block->flags;

	if ((flags & 0x1f) != MEMBLOCK_STATE_LIST_ALLOC) {
		SCE_DBG_PRINTF_FUNC(0xb, 222, "MemBlockStatusStateChangeVirAllocFromLARaw",
				    "Illegal state: 0x%08x\n", flags & 0x1f);
		flags = block->flags;
	}
	block->flags = (flags & ~0x1f) | MEMBLOCK_STATE_VIR_ALLOC;
}

static inline void MemBlockStatusStateChangeLisAllocFromPARaw(MemBlockObject *block)
{
	SceUInt32 flags = block->flags;

	if ((flags & 0x1f) != MEMBLOCK_STATE_PHY_ALLOC) {
		SCE_DBG_PRINTF_FUNC(0xb, 246, "MemBlockStatusStateChangeLisAllocFromPARaw",
				    "Illegal state: 0x%08x\n", flags & 0x1f);
		flags = block->flags;
	}
	block->flags = (flags & ~0x1f) | MEMBLOCK_STATE_LIST_ALLOC;
}

static inline void MemBlockStatusStateChangeLisAllocFromVA(MemBlockObject *block)
{
	SceUInt32 flags = block->flags;

	if ((flags & 0x1f) != MEMBLOCK_STATE_VIR_ALLOC) {
		SCE_DBG_PRINTF_FUNC(0xb, 234, "MemBlockStatusStateChangeLisAllocFromVA",
				    "Illegal state: 0x%08x\n", flags & 0x1f);
		flags = block->flags;
	}
	block->flags = (flags & ~0x1f) | MEMBLOCK_STATE_LIST_ALLOC;
}

static inline void MemBlockStatusStateChangePhyAllocFromLARaw(MemBlockObject *block)
{
	SceUInt32 flags = block->flags;

	if ((flags & 0x1f) != MEMBLOCK_STATE_LIST_ALLOC) {
		SCE_DBG_PRINTF_FUNC(0xb, 258, "MemBlockStatusStateChangePhyAllocFromLARaw",
				    "Illegal state: 0x%08x\n", flags & 0x1f);
		flags = block->flags;
	}
	block->flags = (flags & ~0x1f) | MEMBLOCK_STATE_PHY_ALLOC;
}

static inline void MemBlockStatusStateChangeMappedFromPARaw(MemBlockObject *block)
{
	SceUInt32 flags = block->flags;

	if ((flags & 0x1f) != MEMBLOCK_STATE_PHY_ALLOC) {
		SCE_DBG_PRINTF_FUNC(0xb, 280, "MemBlockStatusStateChangeMappedFromPARaw",
				    "Illegal state: 0x%08x\n", flags & 0x1f);
		flags = block->flags;
	}
	block->flags = (flags & ~0x1f) | MEMBLOCK_STATE_MAPPED;
}

static inline void MemBlockStatusStateChangePhyAllocFromMA(MemBlockObject *block)
{
	SceUInt32 flags = block->flags;

	if ((flags & 0x1f) != MEMBLOCK_STATE_MAPPED) {
		SCE_DBG_PRINTF_FUNC(0xb, 269, "MemBlockStatusStateChangePhyAllocFromMA",
				    "TMP:Illegal state: 0x%08x\n", flags & 0x1f);
		flags = block->flags;
	}
	block->flags = (flags & ~0x1f) | MEMBLOCK_STATE_PHY_ALLOC;
}

#define MEMBLOCK_STATUS_LOCKED		0x200	/* locked by sceKernelAllocUnmapMemBlockForUser */

static inline int MemBlockCheckLockStatusRaw(MemBlockObject *block)
{
	/* the original's check here is constant */
	SCE_DBG_ASSERT_CHECK_FUNC(1, 322, "MemBlockCheckLockStatusRaw");
	return block->flags & MEMBLOCK_STATUS_LOCKED;
}

#define MEMBLOCK_STATUS_UNMAP		0x400	/* PLACEHOLDER: name; allocated unmapped */

static inline void MemBlockSetLockStatusRaw(MemBlockObject *block, SceUInt32 status)
{
	/* the original's check here is constant */
	SCE_DBG_ASSERT_CHECK_FUNC(1, 331, "MemBlockSetLockStatusRaw");
	block->flags |= status;
}

/* one quarter (bits 24..27) of a block of L2 page tables free again */
static inline void MemBlockL2PageTableSetFreeRaw(MemBlockObject *block, SceUInt32 quarter)
{
	SCE_DBG_ASSERT_FUNC(quarter, 522, "MemBlockL2PageTableSetFreeRaw");
	block->flags |= quarter;
}

#pragma pop_macro("__FILE_NAME__")
