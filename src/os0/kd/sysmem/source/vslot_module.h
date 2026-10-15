/*
 * SceSysmem: the virtual slot accessors (Sony's vslot_module.h). Each checks
 * that the memblock is a virtual slot (type bit 28).
 */

#pragma once

#include "memblock_obj_module.h"

#pragma push_macro("__FILE_NAME__")
#undef __FILE_NAME__
#define __FILE_NAME__ "vslot_module.h"

#define VSLOT_TYPE_FLAG			0x10000000

/* what a slot maps: a process's pages */
typedef struct VSlotInfo {
	SceUID pid;
	uintptr_t va;
	SceSize size;
	uintptr_t *pa;			/* 0x0c: one per page */
} VSlotInfo;

static inline int VSlotIsVSlot(const MemBlockObject *block)
{
	return (block->vslotType & VSLOT_TYPE_FLAG) != 0;
}

static inline void VSlotSetTypeRaw(MemBlockObject *block, SceUInt32 type)
{
	SCE_DBG_ASSERT_CHECK_FUNC((type & VSLOT_TYPE_FLAG) != 0, 141, "VSlotSetTypeRaw");
	block->vslotType = type;
}

static inline VSlotInfo *VSlotGetInfoRaw(MemBlockObject *block)
{
	SCE_DBG_ASSERT_CHECK_FUNC(VSlotIsVSlot(block), 148, "VSlotGetInfoRaw");
	return block->vslotInfo;
}

static inline void VSlotSetInfoRaw(MemBlockObject *block, VSlotInfo *info)
{
	SCE_DBG_ASSERT_CHECK_FUNC(VSlotIsVSlot(block), 157, "VSlotSetInfoRaw");
	block->vslotInfo = info;
}

static inline SceSize VSlotGetAllocPhySizeRaw(MemBlockObject *block)
{
	SCE_DBG_ASSERT_CHECK_FUNC(VSlotIsVSlot(block), 173, "VSlotGetAllocPhySizeRaw");
	return block->vslotAllocPhySize;
}

static inline void VSlotSetAllocPhySizeRaw(MemBlockObject *block, SceSize size)
{
	SCE_DBG_ASSERT_CHECK_FUNC(VSlotIsVSlot(block), 181, "VSlotSetAllocPhySizeRaw");
	block->vslotAllocPhySize = size;
}

static inline MemBlockObject *VSlotGetNextRaw(MemBlockObject *block)
{
	SCE_DBG_ASSERT_CHECK_FUNC(VSlotIsVSlot(block), 188, "VSlotGetNextRaw");
	return block->vslotNext;
}

static inline void VSlotSetNextRaw(MemBlockObject *block, MemBlockObject *next)
{
	SCE_DBG_ASSERT_CHECK_FUNC(VSlotIsVSlot(block), 196, "VSlotSetNextRaw");
	block->vslotNext = next;
}

#pragma pop_macro("__FILE_NAME__")
