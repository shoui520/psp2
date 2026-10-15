/*
 * SceSysmem: the memblock object helpers Sony's memblock files share.
 * PLACEHOLDER: only what the converted files use is named so far.
 */

#pragma once

#include "sysmem_internal.h"

/* Sony's records name this header; heap.c includes it from dlmalloc/ */
#ifndef MEMBLOCK_OBJ_MODULE_FILE
#define MEMBLOCK_OBJ_MODULE_FILE	"memblock_obj_module.h"
#endif

#pragma push_macro("__FILE_NAME__")
#undef __FILE_NAME__
#define __FILE_NAME__ MEMBLOCK_OBJ_MODULE_FILE

#define MEMBLOCK_FLAG_NAMED		0x80

/* MemBlockObject.attr */
#define MEMBLOCK_ATTR_KEEP_PHY		0x00000020	/* the physical pages are another block's */
#define MEMBLOCK_ATTR_MAPPED		0x00000040
#define MEMBLOCK_ATTR_NEEDS_CLEAR	0x00000080
#define MEMBLOCK_ATTR_PHYCONT		0x00000100	/* the backed part is one virtual page */
#define MEMBLOCK_ATTR_PARTIAL		0x00002000	/* mapped in whole units, never supersections */
#define MEMBLOCK_ATTR_CLIENT		0x00004000	/* PLACEHOLDER: name; no base block */
#define MEMBLOCK_ATTR_USER_CLIENT	0x00008000	/* ALLOC_ATTR_USER_CLIENT */
#define MEMBLOCK_ATTR_WHOLE		0x80000000	/* a simple memblock: no free head or tail */

/* MemBlockObject.attr: mapped in whole units (never supersections) */

typedef struct MemBlockObject {
	union {
		SceObjectBase base;
		struct {
			struct MemBlockObject *next;	/* 0x00: in its partition's list */
			SceClass *cls;
		};
	};
	struct MemBlockObject *prev;		/* 0x08 */
	SceUInt32 flags;			/* 0x0c */
	SceUInt32 code;				/* 0x10: the memblock code (memblock_db.c) */
	SceUInt32 attr;				/* 0x14: MEMBLOCK_ATTR_* */
	uintptr_t vbase;			/* 0x18 */
	SceSize size;				/* 0x1c */
	SceKernelRWSpinlock lock;		/* 0x20 */
	union {
		struct {
			void *partition;		/* 0x24 */
			void *phyMemPart;		/* 0x28 */
		};
		struct {			/* a free block: the partition's free list */
			struct MemBlockObject *freePrev;	/* 0x24 */
			struct MemBlockObject *freeNext;	/* 0x28 */
		};
	};
	const char *name;			/* 0x2c */
	union {
		struct {
			void *virPages;		/* 0x30: list of virtual pages */
			SceUInt16 mapRef;	/* 0x34: MemBlockGetMapRef */
			SceUInt16 lockRef;	/* 0x36: MemBlockGetLockRef */
			SceUInt32 opt;		/* 0x38: what MEMBLOCK_OPT_* says (MemBlockOpt*) */
			SceUID guid;		/* 0x3c: MemBlockGetGUID, -1 when none */
		};
		struct {			/* a virtual slot (vslot.c) */
			SceUInt32 vslotType;	/* 0x30: bit 28 set */
			struct VSlotInfo *vslotInfo;	/* 0x34 */
			SceSize vslotAllocPhySize;	/* 0x38 */
			struct MemBlockObject *vslotNext;	/* 0x3c: in its FIFO */
		};
		struct {			/* a simple memblock (smb.c) */
			SceUInt32 smbType;	/* 0x30 */
			struct PhyPage *smbPage;	/* 0x34: its physical pages */
			SceSize smbSize;	/* 0x38: of the pages */
			struct MemBlockObject *smbNext;	/* 0x3c: in its type's FIFO */
		};
	};
} MemBlockObject;

/*
 * memblock_obj.c keeps out-of-line copies (MEMBLOCK_OBJ_MODULE_EXTERN); files
 * that call them rather than inline them define MEMBLOCK_OBJ_MODULE_CALL.
 * Elsewhere the body is GNU extern inline: inlined where it folds (a constant
 * name), otherwise a call to memblock_obj.c's copy, as in Sony's partition.c.
 */
/* the name of a named block */
static inline const char *MemBlockGetNameRaw(MemBlockObject *block)
{
	SCE_DBG_ASSERT_CHECK_FUNC(block->flags & MEMBLOCK_FLAG_NAMED, 316, "MemBlockGetNameRaw");
	return block->name;
}

/* the GUID's name, the block's name or "SceNULL" (memblock_obj.c) */
const char *MemBlockGetName(MemBlockObject *block);

#if defined(MEMBLOCK_OBJ_MODULE_CALL)
void MemBlockSetNameRaw(MemBlockObject *block, const char *name);
#else
#ifdef MEMBLOCK_OBJ_MODULE_EXTERN
#define MEMBLOCK_OBJ_MODULE_INLINE
#else
#define MEMBLOCK_OBJ_MODULE_INLINE	extern inline __attribute__((gnu_inline))
#endif

MEMBLOCK_OBJ_MODULE_INLINE void MemBlockSetNameRaw(MemBlockObject *block, const char *name)
{
	if (name == NULL) {
		block->name = NULL;
		block->flags &= ~MEMBLOCK_FLAG_NAMED;
		return;
	}
	SCE_DBG_ASSERT_CHECK(name != NULL, 653);
	block->name = name;
	block->flags |= MEMBLOCK_FLAG_NAMED;
}

#endif

/* GUIDApplyWithClass: result = func(object, argument), under the GUID lock */
typedef struct GUIDApplyArgs {
	int result;
	void *argument;
} GUIDApplyArgs;

int GUIDApplyWithClass(SceUID guid, SceClass *cls, int (*func)(SceObjectBase *object, void *argument),
		       GUIDApplyArgs *args);

/* the block's state field (flags bits 20..22): 3 = mapped with user map info */
#define MEMBLOCK_OPT_MASK		0x700000
#define MEMBLOCK_OPT_USER_MAP		0x300000

static inline __attribute__((always_inline)) void MemBlockSetFreeCodeRaw(MemBlockObject *block)
{
	SCE_DBG_ASSERT_CHECK_FUNC(1, 309, "MemBlockSetFreeCodeRaw");
	block->flags &= 0xff80ff7f;
}

/* the user map info a block in the user-map state carries */
static inline __attribute__((always_inline)) void *MemBlockOptGetUserMapInfoRaw(MemBlockObject *block)
{
	if ((block->flags & MEMBLOCK_OPT_MASK) != MEMBLOCK_OPT_USER_MAP)
		return NULL;
	SCE_DBG_ASSERT_CHECK_FUNC(1, 997, "MemBlockOptGetUserMapInfoRaw");
	return (void *)block->opt;
}

/* the block kind (flags bits 16..19): 8 = partial (memblock_partial.c) */
#define MEMBLOCK_KIND_MASK		0xf0000
#define MEMBLOCK_KIND_PARTIAL		0x80000

/* a partial block's units (memblock_partial.c) */
typedef struct PartialInfo {
	SceUInt32 count;		/* units */
	SceSize unitSize;		/* 0x04: a power of two */
	SceKernelRWSpinlock lock;	/* 0x08 */
	SceUInt32 reserved;
	SceUInt32 *units;		/* 0x10 */
	SceUInt32 *shadow;		/* 0x14: the copy an operation works on */
} PartialInfo;

_Static_assert(sizeof(PartialInfo) == 0x18, "PartialInfo size");


static inline struct PartialInfo *MemBlockGetPartialInfoRaw(MemBlockObject *block)
{
	SCE_DBG_ASSERT_CHECK_FUNC((block->flags & MEMBLOCK_KIND_MASK) == MEMBLOCK_KIND_PARTIAL, 917,
				  "MemBlockGetPartialInfoRaw");
	return block->virPages;
}

static inline void MemBlockSetPartialInfoRaw(MemBlockObject *block, struct PartialInfo *info)
{
	SceUInt32 flags = block->flags;

	if ((flags & MEMBLOCK_KIND_MASK) != MEMBLOCK_KIND_PARTIAL) {
		if ((flags & MEMBLOCK_KIND_MASK) == 0) {
			SCE_DBG_PRINTF_FUNC(0xb, 905, "MemBlockSetPartialInfoRaw",
					    "0x%08x -> Force PartialInfo\n", flags);
			block->flags = (block->flags & ~MEMBLOCK_KIND_MASK) | MEMBLOCK_KIND_PARTIAL;
		} else {
			SCE_DBG_ASSERT_CHECK_FUNC(0, 908, "MemBlockSetPartialInfoRaw");
			SCE_DBG_PRINTF_FUNC(0xb, 909, "MemBlockSetPartialInfoRaw", "%p:0x%08x\n", block,
					    block->flags);
		}
	}
	block->virPages = info;
}

/* kind 4: a list of virtual pages (memblock_vpl.c) */
#define MEMBLOCK_KIND_VPL		0x40000

static inline VirPage *MemBlockGetVirPageRaw(MemBlockObject *block)
{
	if ((block->flags & MEMBLOCK_KIND_MASK) != MEMBLOCK_KIND_VPL)
		SCE_DBG_PRINTF_FUNC(0xb, 861, "MemBlockGetVirPageRaw", "pBlock=%p:0x%08x\n", block,
				    block->flags);
	SCE_DBG_ASSERT_CHECK_FUNC((block->flags & MEMBLOCK_KIND_MASK) == MEMBLOCK_KIND_VPL, 864,
				  "MemBlockGetVirPageRaw");
	return block->virPages;
}

static inline void MemBlockClearVirPageRaw(MemBlockObject *block)
{
	SCE_DBG_ASSERT_CHECK_FUNC((block->flags & MEMBLOCK_KIND_MASK) == MEMBLOCK_KIND_VPL, 878,
				  "MemBlockClearVirPageRaw");
	block->virPages = NULL;
	block->flags &= ~MEMBLOCK_KIND_MASK;
}

/* the free list links (only free blocks, code 0, are on it) */
static inline MemBlockObject *MemBlockGetFreePrevRaw(MemBlockObject *block)
{
	SCE_DBG_ASSERT_CHECK_FUNC(block->code == 0, 577, "MemBlockGetFreePrevRaw");
	SCE_DBG_ASSERT_CHECK_FUNC(block->freePrev == NULL || block->freePrev->code == 0, 579,
				  "MemBlockGetFreePrevRaw");
	return block->freePrev;
}

static inline MemBlockObject *MemBlockGetFreeNextRaw(MemBlockObject *block)
{
	SCE_DBG_ASSERT_CHECK_FUNC(block->code == 0, 595, "MemBlockGetFreeNextRaw");
	SCE_DBG_ASSERT_CHECK_FUNC(block->freeNext == NULL || block->freeNext->code == 0, 597,
				  "MemBlockGetFreeNextRaw");
	return block->freeNext;
}

static inline void MemBlockSetFreeNextRaw(MemBlockObject *block, MemBlockObject *next)
{
	SCE_DBG_ASSERT_CHECK_FUNC(block->code == 0, 605, "MemBlockSetFreeNextRaw");
	SCE_DBG_ASSERT_CHECK_FUNC(next == NULL || next->code == 0, 606, "MemBlockSetFreeNextRaw");
	block->freeNext = next;
}

static inline void MemBlockSetFreePrevRaw(MemBlockObject *block, MemBlockObject *prev)
{
	SCE_DBG_ASSERT_CHECK_FUNC(block->code == 0, 587, "MemBlockSetFreePrevRaw");
	SCE_DBG_ASSERT_CHECK_FUNC(prev == NULL || prev->code == 0, 588, "MemBlockSetFreePrevRaw");
	block->freePrev = prev;
}

/* bits 20..22: 1 = mirrors a base block in opt, 4 = base info in opt */
#define MEMBLOCK_OPT_BASE_BLOCK		0x100000
#define MEMBLOCK_OPT_BASE_INFO		0x400000

static inline void MemBlockOptSetBaseBlockRaw(MemBlockObject *block, MemBlockObject *base)
{
	SCE_DBG_ASSERT_CHECK_FUNC((block->flags & MEMBLOCK_OPT_MASK) == 0, 949,
				  "MemBlockOptSetBaseBlockRaw");
	block->opt = (SceUInt32)base;
	block->flags = (block->flags & ~MEMBLOCK_OPT_MASK) | MEMBLOCK_OPT_BASE_BLOCK;
}

/* the opt state (bits 20..22) 2: the virtual base block (another partition's) in opt */
#define MEMBLOCK_OPT_VIR_BASE_BLOCK	0x200000

static inline MemBlockObject *MemBlockOptGetBaseBlockRaw(MemBlockObject *block)
{
	SceUInt32 state = block->flags & MEMBLOCK_OPT_MASK;

	if (state != MEMBLOCK_OPT_BASE_BLOCK)
		SCE_DBG_PRINTF_FUNC(0xb, 938, "MemBlockOptGetBaseBlockRaw", "status=0x%08x\n",
				    block->flags);
	SCE_DBG_ASSERT_CHECK_FUNC(state == MEMBLOCK_OPT_BASE_BLOCK, 941, "MemBlockOptGetBaseBlockRaw");
	return (MemBlockObject *)block->opt;
}

/* the client info of opt state 5 */
typedef struct MemBlockClientInfo {
	SceUInt32 capabilities[8];	/* 0x00: opt.capabilities, each bit one the opener needs */
	SceUInt32 openFlags;		/* 0x20: opt.openFlags, the open flags allowed */
	SceKernelSpinlock lock;		/* 0x24 */
	SceUInt32 capacity;		/* 0x28 */
	SceUID *guids;			/* 0x2c: the mapping blocks, -1 when free */
	SceUInt32 count;		/* 0x30 */
	SceUInt32 reserved[3];
} MemBlockClientInfo;

_Static_assert(sizeof(MemBlockClientInfo) == 0x40, "MemBlockClientInfo size");

/* the client infos of a dying process's blocks, collected while they are freed (partition.c) */
typedef struct TinyPartitionClientInfoList {
	SceUInt32 capacity;
	void **entries;
	SceUInt32 count;
} TinyPartitionClientInfoList;

/* opt state 4 or 5: the base info / client info pointer in opt */
static inline void MemBlockOptClearBaseInfoRaw(MemBlockObject *block)
{
	SceUInt32 flags = block->flags;

	if ((flags & MEMBLOCK_OPT_MASK) != MEMBLOCK_OPT_BASE_INFO) {
		SCE_DBG_PRINTF_FUNC(0xb, 1038, "MemBlockOptClearBaseInfoRaw", "status=0x%08x\n",
				    flags);
		flags = block->flags;
	}
	block->flags = flags & ~MEMBLOCK_OPT_MASK;
	block->opt = 0;
}

static inline void MemBlockOptClearClientInfoRaw(MemBlockObject *block)
{
	SceUInt32 flags = block->flags;

	SCE_DBG_ASSERT_CHECK_FUNC((flags & MEMBLOCK_OPT_MASK) == 0x500000, 1071,
				  "MemBlockOptClearClientInfoRaw");
	block->opt = 0;
	block->flags = block->flags & ~MEMBLOCK_OPT_MASK;
}

static inline void MemBlockOptSetVirBaseBlockRaw(MemBlockObject *block, MemBlockObject *base)
{
	SceUInt32 flags = block->flags;

	if (flags & MEMBLOCK_OPT_MASK)
		SCE_DBG_PRINTF_FUNC(0xb, 969, "MemBlockOptSetVirBaseBlockRaw", "status=0x%08x\n",
				    flags);
	SCE_DBG_ASSERT_CHECK_FUNC((block->flags & MEMBLOCK_OPT_MASK) == 0, 972,
				  "MemBlockOptSetVirBaseBlockRaw");
	block->opt = (SceUInt32)base;
	block->flags = (block->flags & ~MEMBLOCK_OPT_MASK) | MEMBLOCK_OPT_VIR_BASE_BLOCK;
}

static inline MemBlockObject *MemBlockOptGetVirBaseBlockRaw(MemBlockObject *block)
{
	SceUInt32 state = block->flags & MEMBLOCK_OPT_MASK;

	SCE_DBG_ASSERT_CHECK_FUNC(state == MEMBLOCK_OPT_VIR_BASE_BLOCK, 959, "MemBlockOptGetVirBaseBlockRaw");
	return (MemBlockObject *)block->opt;
}

static inline MemBlockObject *MemBlockOptGetBaseInfoRaw(MemBlockObject *block)
{
	SceUInt32 state = block->flags & MEMBLOCK_OPT_MASK;

	SCE_DBG_ASSERT_CHECK_FUNC(state == MEMBLOCK_OPT_BASE_INFO, 1015, "MemBlockOptGetBaseInfoRaw");
	return (MemBlockObject *)block->opt;
}

static inline void MemBlockSetVirPageRaw(MemBlockObject *block, VirPage *vp)
{
	SceUInt32 flags = block->flags;

	if ((flags & MEMBLOCK_KIND_MASK) != MEMBLOCK_KIND_VPL) {
		if ((flags & MEMBLOCK_KIND_MASK) == 0)
			block->flags = (flags & ~MEMBLOCK_KIND_MASK) | MEMBLOCK_KIND_VPL;
		else
			SCE_DBG_PRINTF_FUNC(0xb, 893, "MemBlockSetVirPageRaw", "%p:0x%08x\n", block,
					    flags);
	}
	block->virPages = vp;
}

#pragma pop_macro("__FILE_NAME__")
