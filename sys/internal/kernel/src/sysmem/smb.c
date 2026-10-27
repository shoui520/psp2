/*
 * SceSysmem: simple memblocks. Small kernel blocks of fixed types made in
 * advance at boot: each type keeps a FIFO of free blocks, already placed in
 * the kernel partition, and an allocation takes one and backs it with
 * physical pages (with guard pages around it for some types).
 */

#include "sysmem_internal.h"
#define MEMBLOCK_OBJ_MODULE_CALL
#include "memblock_obj_module.h"
#include "memblock/status_module.h"

/*
 * The simple memblock type: bits 0..3 the FIFO, 4..11 the layout, 12..23 the
 * size with any guard pages, 24..27 the memory.
 */
#define SMB_TYPE_FIFO(_type)		((_type) & 0xf)
#define SMB_TYPE_LAYOUT(_type)		((_type) & 0xff0)
#define SMB_TYPE_SIZE(_type)		((_type) & 0xfff000)
#define SMB_TYPE_MEMORY(_type)		((_type) & 0xf000000)

#define SMB_LAYOUT_GUARD1		0x10	/* a guard page in front */
#define SMB_LAYOUT_GUARD2		0x20	/* two guard pages in front */
#define SMB_LAYOUT_ONE_PAGE		0x800	/* one page mapped */

#define SMB_MEMORY_NORMAL		0x0000000
#define SMB_MEMORY_1			0x1000000	/* PLACEHOLDER: meaning unknown */

#define SMB_FIFO_COUNT			9

/* AllocSimpleMemBlock flags */
#define SMB_ALLOC_NO_CLEAR		0x01
#define SMB_ALLOC_NO_MAP		0x02
#define SMB_ALLOC_MAP_FLAGS(_flags)	(((_flags) >> 4) & 3)

/* the free blocks of one type */
typedef struct SmbFifo {
	SceKernelRWSpinlock lock;
	SceUInt32 count;		/* 0x04 */
	SceUInt32 reserved;
	SceUInt32 lowCount;		/* 0x0c: raised by each push, lowered to count by a pop below it */
	MemBlockObject *head;		/* 0x10 */
	MemBlockObject *tail;		/* 0x14 */
} SmbFifo;

_Static_assert(sizeof(SmbFifo) == 0x18, "SmbFifo size");

/* the types, by FIFO */
static const SceUInt32 s_smbTypes[SMB_FIFO_COUNT] = {
	0x1000, 0x4002, 0x4022, 0x10004, 0x100808,
	0x1001000, 0x1004002, 0x1004022, 0x1010004,
};

static SceClass *s_smbClass;
static SmbFifo *s_smbFifo;

/* the bytes of guard pages in front of the block */
static inline SceSize SmbGuardSize(SceUInt32 smbType)
{
	switch (SMB_TYPE_LAYOUT(smbType)) {
	case SMB_LAYOUT_GUARD1:
		return 0x1000;
	case SMB_LAYOUT_GUARD2:
		return 0x2000;
	default:
		return 0;
	}
}

/* the first physical page's address (PhyPage info: pindex in bits 0..19) */
static inline uintptr_t SmbPagePbase(const MemBlockObject *block)
{
	return *(const SceUInt32 *)block->smbPage << 12;
}

/* name unknown: the block's pages mapped behind its guard pages */
int SimpleMemBlockMap(MemBlockObject *block, SceUInt32 flags)
{
	SceUInt32 smbType = block->smbType;
	uintptr_t va = block->vbase;
	SceSize size;

	switch (SMB_TYPE_LAYOUT(smbType)) {
	case SMB_LAYOUT_GUARD1:
		va += 0x1000;
		size = SMB_TYPE_SIZE(smbType) - 0x1000;
		break;
	case SMB_LAYOUT_GUARD2:
		va += 0x2000;
		size = SMB_TYPE_SIZE(smbType) - 0x2000;
		break;
	case SMB_LAYOUT_ONE_PAGE:
		size = 0x1000;
		break;
	default:
		size = SMB_TYPE_SIZE(smbType);
		break;
	}
	return PartitionMapPages(PartitionGetKernel(), block->code, va, size, SmbPagePbase(block),
				 SMB_ALLOC_MAP_FLAGS(flags));
}

/* name unknown: the block's memory zeroed */
int SimpleMemBlockClear(MemBlockObject *block)
{
	memset((void *)(block->vbase + SmbGuardSize(block->smbType)), 0,
	       SMB_TYPE_SIZE(block->smbType));
	return 0;
}

int SimpleMemBlockSetDefaultName(MemBlockObject *block)
{
	const char *name;

	switch (block->smbType) {
	case 0x1000:
	case 0x1001000:
		name = SysrootGetMemBlockName(5);
		break;
	case 0x4022:
	case 0x1004022:
		name = SysrootGetMemBlockName(6);
		break;
	case 0x4002:
	case 0x1004002:
		name = SysrootGetMemBlockName(7);
		break;
	case 0x10004:
	case 0x100808:
	case 0x1010004:
	case 0x1100808:
		name = SysrootGetMemBlockName(8);
		break;
	default:
		SCE_DBG_ASSERT_FAIL(645);
		name = NULL;
		break;
	}
	MemBlockSetNameRaw(block, name);
	return 0;
}

/* name unknown: the class destructor */
int SimpleMemBlockDestroy(MemBlockObject *block)
{
	MemBlockClearRaw(block);
	block->smbPage = NULL;
	block->smbSize = 0;
	block->smbNext = NULL;
	return 0;
}

/* name unknown: the class constructor */
int SimpleMemBlockCreate(MemBlockObject *block)
{
	MemBlockClearRaw(block);
	block->smbPage = NULL;
	block->smbSize = 0;
	block->smbNext = NULL;
	return 0;
}

static SmbFifo *GetFIFO(SceUInt32 smbType)
{
	if (SMB_TYPE_FIFO(smbType) < SMB_FIFO_COUNT)
		return &s_smbFifo[SMB_TYPE_FIFO(smbType)];
	SCE_DBG_ASSERT_FAIL(315);
	return NULL;
}

/* name unknown: a free block onto the tail of its FIFO */
int SimpleMemBlockPush(SceUInt32 smbType, MemBlockObject *block)
{
	SmbFifo *fifo = GetFIFO(smbType);
	int intr = SceCpuForDriver_4C38CE4D(&fifo->lock);

	if (fifo->count != 0) {
		fifo->count++;
		fifo->lowCount++;
		fifo->tail->smbNext = block;
		fifo->tail = block;
	} else {
		fifo->head = block;
		fifo->tail = block;
		fifo->count = 1;
		fifo->lowCount++;
	}
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&fifo->lock, intr);
	return 0;
}

__attribute__((noreturn))
void SMBType2MemBlockCode(void)
{
	SCE_PANIC(160);
}

/* the memblock code of a type's memory */
static inline SceUInt32 SmbMemBlockCode(SceUInt32 smbType)
{
	switch (SMB_TYPE_MEMORY(smbType)) {
	case SMB_MEMORY_NORMAL:
		return MEMBLOCK_CODE_KERNEL_ROOT_RW;
	case SMB_MEMORY_1:
		return 0x1060d004;
	default:
		SMBType2MemBlockCode();
	}
}

/* name unknown: one more free block of a type, placed in the kernel partition */
int SimpleMemBlockNew(SceUInt32 smbType)
{
	MemBlockObject *block, *newBlock;
	SceUInt32 code = SmbMemBlockCode(smbType);
	int ret;

	ret = PartitionAllocSimpleMemBlock(PartitionGetKernel(), smbType, SMB_TYPE_SIZE(smbType),
					   &block);
	if (ret < 0)
		return ret;
	block->smbPage = NULL;
	block->base.sce_class = s_smbClass;
	block->attr |= 0x80000000;
	MemBlockStatusStateChangeVirAllocFromFreeRaw(block);
	block->smbType = smbType;
	SimpleMemBlockSetDefaultName(block);
	newBlock = block;
	newBlock->code = code;
	newBlock->partition = PartitionGetKernel();
	newBlock->smbSize = 0;
	newBlock->smbNext = NULL;
	SimpleMemBlockPush(smbType, newBlock);
	return 0;
}

int AllocSimpleMemBlock(const char *name, SceUInt32 smbType, void *phyMemPart, SceUInt32 flags,
			MemBlockObject **pBlock)
{
	SmbFifo *fifo;
	MemBlockObject *block;
	struct PhyPage *page;
	SceSize size;
	int intr, ret;

	size = SMB_TYPE_SIZE(smbType);
	switch (SMB_TYPE_LAYOUT(smbType)) {
	case SMB_LAYOUT_GUARD1:
		size -= 0x1000;
		break;
	case SMB_LAYOUT_GUARD2:
		size -= 0x2000;
		break;
	case SMB_LAYOUT_ONE_PAGE:
		size = 0x1000;
		break;
	}
	SCE_DBG_ASSERT_PRINTF(size != 0, 515, "sceKernelGetAssertLevel=%d, smbType=0x%08x\n",
			      SceDebugForDriver_374B7868(), smbType);
	if (phyMemPart == NULL)
		phyMemPart = SceSysmemForKernel_0164D817();
	if ((flags & ~0x33) || pBlock == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

	fifo = GetFIFO(smbType);
	intr = SceCpuForDriver_4C38CE4D(&fifo->lock);
	if (fifo->count == 0) {
		*pBlock = NULL;
		sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&fifo->lock, intr);
		SCE_PRINTF_WITH_INFO(0, 529, "There is no SMB for name=[%s],type=0x%08x\n", name,
				     smbType);
		return SCE_KERNEL_ERROR_NO_SMB;
	}
	block = fifo->head;
	*pBlock = block;
	fifo->count--;
	if (fifo->count < fifo->lowCount)
		fifo->lowCount = fifo->count;
	if (fifo->count != 0)
		fifo->head = block->smbNext;
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&fifo->lock, intr);

	(*pBlock)->smbType = smbType;
	MemBlockSetPhyMemPartRaw(*pBlock, phyMemPart);
	(*pBlock)->code = SmbMemBlockCode(smbType);
	ret = PhyMemPartAllocPhyPage(phyMemPart, size, 0x10, &page);
	if (ret < 0)
		return ret;
	(*pBlock)->smbPage = page;
	block = *pBlock;
	if (!(flags & SMB_ALLOC_NO_MAP)) {
		ret = SimpleMemBlockMap(block, flags);
		if (ret < 0)
			return ret;
		block = *pBlock;
	}
	MemBlockSetNameRaw(block, name);
	block = *pBlock;
	if (!(flags & SMB_ALLOC_NO_CLEAR)) {
		ret = SimpleMemBlockClear(block);
		block = *pBlock;
	} else {
		ret = 0;
	}
	block->smbSize = size;
	return ret;
}

#if !SYSMEM_NSKBL
/* name unknown: a block anywhere in the kernel's physical partition */
int SceSysmemForKernel_814CDCC5(const char *name, SceUInt32 smbType, SceUInt32 flags,
				MemBlockObject **pBlock)
{
	return AllocSimpleMemBlock(name, smbType, NULL, flags, pBlock);
}
#endif

/* name unknown: unmapped, its pages freed and back on its FIFO */
int FreeSimpleMemBlock(MemBlockObject *block, SceUInt32 flags)
{
	SceUInt32 smbType;
	int ret;

	ret = PartitionUnmapL2PageTable(PartitionGetKernel(), block->vbase, block->size, 0, flags);
	if (ret < 0)
		return ret;
	ret = PhyMemPartFreePhyPage(MemBlockGetPhyMemPartRaw(block), block->smbPage);
	if (ret < 0)
		return ret;
	smbType = block->smbType;
	block->smbPage = NULL;
	SimpleMemBlockSetDefaultName(block);
	block->smbType = 0;
	return SimpleMemBlockPush(smbType, block);
}

int SceSysmemForKernel_A4DB88E1(void *block)
{
	return FreeSimpleMemBlock(block, 0);
}

/* name unknown: the block's address past its guard pages */
int SceSysmemForKernel_39D5A3B7(MemBlockObject *block, void **base)
{
	*base = (void *)(block->vbase + SmbGuardSize(block->smbType));
	return 0;
}

/* name unknown: every FIFO's counts */
typedef struct SceKernelSimpleMemBlockInfo {
	SceSize size;			/* 0x70 */
	struct {
		SceUInt32 count;
		SceUInt32 smbType;
		SceUInt32 lowCount;
	} fifo[SMB_FIFO_COUNT];
} SceKernelSimpleMemBlockInfo;

#if !SYSMEM_NSKBL
int SceSysmemForKernel_BD625136(SceKernelSimpleMemBlockInfo *info)
{
	SmbFifo *fifo;
	int i;

	if (info->size != sizeof(*info))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	for (i = 0; i < SMB_FIFO_COUNT; i++) {
		fifo = GetFIFO(i);
		info->fifo[i].count = fifo->count;
		info->fifo[i].smbType = s_smbTypes[i];
		info->fifo[i].lowCount = fifo->lowCount;
	}
	return 0;
}
#endif

/* name unknown: the physical address of the block's pages */
int SimpleMemBlockGetPbase(MemBlockObject *block, uintptr_t *pbase)
{
	*pbase = SmbPagePbase(block);
	return 0;
}

#if !SYSMEM_NSKBL
/* name unknown: the class, the boot stock of blocks, and their sections reserved */
int SimpleMemBlockModuleStart(const UIDManager *manager)
{
	SceUInt32 sectionUsed[128];	/* the kernel's 1 MiB sections holding a block */
	MemBlockObject *block;
	SceUInt32 index;
	int i;

	s_smbClass = manager->smbClass;
	UIDClassSetCallbacks(s_smbClass, "SceUIDSimpleMemBlockClass",
			     (SceClassCallback)SimpleMemBlockCreate,
			     (SceClassCallback)SimpleMemBlockDestroy);
	s_smbFifo = manager->smbFifo;
#ifdef SYSMEM_TZS
	for (i = 504; i != 0; i--)
		SimpleMemBlockNew(0x1000);
	for (i = 60; i != 0; i--)
		SimpleMemBlockNew(0x4002);
	for (i = 6; i != 0; i--)
		SimpleMemBlockNew(0x10004);
#else
	for (i = 0x400; i != 0; i--)
		SimpleMemBlockNew(0x1000);
	for (i = 0x60; i != 0; i--)
		SimpleMemBlockNew(0x4002);
#endif
	for (i = 0; i < 128; i++)
		sectionUsed[i] = 0;
	for (i = 0; i < SMB_FIFO_COUNT; i++) {
		for (block = s_smbFifo[i].head; block != NULL; block = block->smbNext) {
			index = (SmbGuardSize(block->smbType) + block->vbase) >> 20;
			if (sectionUsed[index] == 0)
				sectionUsed[index] = 1;
		}
	}
	for (i = 0; i < 128; i++)
		if (sectionUsed[i] == 1)
			PartitionReserveSection(PartitionGetKernel(), i << 20);
	return 0;
}
#endif
