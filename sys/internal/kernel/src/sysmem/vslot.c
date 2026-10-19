/*
 * SceSysmem: virtual slots. Kernel windows onto a few pages of a process: a
 * slot is a 16 KiB kernel memblock made at boot; mapping a process range into
 * it locks the range, looks up its physical pages and maps them in the slot.
 * Free slots wait in a FIFO per slot type.
 */

#include "sysmem_internal.h"
#define MEMBLOCK_OBJ_MODULE_CALL
#include "memblock_obj_module.h"
#include "memblock/status_module.h"
#pragma push_macro("__FILE_NAME__")
#undef __FILE_NAME__
#define __FILE_NAME__ "vslot_module.h"
#include "vslot_module.h"
#pragma pop_macro("__FILE_NAME__")

#undef __FILE_NAME__
#define __FILE_NAME__ "vslot.c"

#define VSLOT_TYPE_16K_MXUU		0x12201000	/* FIFO 0 */
#define VSLOT_MAX_SIZE			0x2000
#define VSLOT_BOOT_COUNT		16

/* the free slots of one type */
typedef struct VSlotFIFO {
	SceKernelRWSpinlock lock;
	SceUInt16 count;		/* 0x04 */
	SceUInt16 low;			/* 0x06: PLACEHOLDER: a count kept beside it */
	MemBlockObject *head;		/* 0x08 */
	MemBlockObject *tail;		/* 0x0c */
} VSlotFIFO;

_Static_assert(sizeof(VSlotFIFO) == 0x10, "VSlotFIFO size");

static VSlotFIFO s_vslotFifo[2];
static SceClass *s_vslotClass;
static SceClass s_vslotClassBody;

static inline VSlotFIFO *GetFIFO(SceUInt32 type)
{
	switch (type & 0xf) {
	case 0:
		return &s_vslotFifo[0];
	case 1:
		return &s_vslotFifo[1];
	default:
		SCE_DBG_ASSERT_FAIL(84);
		return NULL;
	}
}

/* name unknown: a slot onto the tail of its FIFO (a new one also counted in low) */
static int VSlotFIFOPush(SceUInt32 type, MemBlockObject *slot, int isNew)
{
	VSlotFIFO *fifo = GetFIFO(type);
	int intr = SceCpuForDriver_4C38CE4D(&fifo->lock);

	if (fifo->count == 0) {
		fifo->head = slot;
		fifo->tail = slot;
	} else {
		MemBlockObject *tail;
#if FW_DEBUG
		int assertLevel = SceDebugForDriver_374B7868();

		tail = fifo->tail;
		if (assertLevel > 0) {
#pragma push_macro("__FILE_NAME__")
#undef __FILE_NAME__
#define __FILE_NAME__ "vslot_module.h"
			SceDebugForDriver_FD753E7A(1, VSlotIsVSlot(tail),
					      SCE_DBGINFO_FUNC(196, "VSlotSetNextRaw"),
					      __builtin_return_address(0));
#pragma pop_macro("__FILE_NAME__")
		}
#else
		tail = fifo->tail;
#endif
		tail->vslotNext = slot;
		fifo->tail = slot;
	}
	fifo->count++;
	if (isNew)
		fifo->low++;
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&fifo->lock, intr);
	return 0;
}

/* name unknown: the slot fields cleared */
static void VSlotClear(MemBlockObject *slot)
{
	MemBlockClearRaw(slot);
	VSlotSetInfoRaw(slot, NULL);
	VSlotSetAllocPhySizeRaw(slot, 0);
	VSlotSetNextRaw(slot, NULL);
}

/* name unknown: the class destructor */
int VSlotDestroy(MemBlockObject *slot)
{
	VSlotClear(slot);
	return 0;
}

/* name unknown: the class constructor */
int VSlotCreate(MemBlockObject *slot)
{
	VSlotClear(slot);
	return 0;
}

/* the head of the FIFO taken off; an error when it is empty */
static inline int VSlotFIFOPop(VSlotFIFO *fifo, MemBlockObject **pSlot)
{
	MemBlockObject *slot;
	SceUInt16 count = fifo->count;

	if (count == 0) {
		SCE_DBG_ASSERT_CHECK(count, 193);
		return SCE_KERNEL_ERROR_ERROR;
	}
	count--;
	slot = fifo->head;
	fifo->count = count;
	if (count < fifo->low)
		fifo->low = count;
	if (count == 0) {
		fifo->head = NULL;
		fifo->tail = NULL;
	} else {
		fifo->head = VSlotGetNextRaw(slot);
	}
	*pSlot = slot;
	return 0;
}

/* the process range's pages mapped into the slot */
static inline int VSlotMap(MemBlockObject *slot, SceUID pid, uintptr_t va, SceSize size,
			   MemBlockObject **pSlot, uintptr_t *pVbase)
{
	VSlotInfo *info;
	SceUInt32 pages = size >> 12, i, j;
	uintptr_t vbase, pa;
	int intr, ret = 0;

	intr = SceCpuForDriver_4C38CE4D(&slot->lock);
	info = VSlotGetInfoRaw(slot);
	info->pid = pid;
	info->va = va;
	info->size = size;
	for (i = 0; i < pages; i++, va += 0x1000) {
		ret = sceKernelProcModeVAtoPA(pid, 0x10, (const void *)va, &pa);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 423,
				       "%d/%d:sceKernelProcModeVAtoPA(0x%08x, 0x%08x) failed 0x%08x\n",
				       i, pages, pid, va, ret);
			goto out;
		}
		info->pa[i] = pa;
	}
	VSlotSetAllocPhySizeRaw(slot, size);
	vbase = slot->vbase;
	for (i = 0; i < pages; i++, vbase += 0x1000) {
		pa = info->pa[i];
		ret = PartitionMapPages(slot->partition, slot->code, vbase, 0x1000, pa, 0);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 446, "PartitionMapL2PageTable failed 0x%08x\n", ret);
			for (j = 0, vbase = slot->vbase; j < i; j++, vbase += 0x1000)
				ret = PartitionUnmapL2PageTable(slot->partition, vbase, 0x1000, 0, 0);
			goto out;
		}
	}
	vbase = slot->vbase;
	*pSlot = slot;
	*pVbase = vbase;
out:
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&slot->lock, intr);
	return ret;
}

/* up to 8 KiB of a process mapped into a free slot of the type */
int sceKernelVSlotMapProcMemory(SceUID pid, uintptr_t va, SceSize size, SceUInt32 type,
				MemBlockObject **pSlot, uintptr_t *pVbase)
{
	VSlotFIFO *fifo;
	MemBlockObject *slot;
	int intr, ret;

	if ((va & 0xfff) || size - 1 >= VSLOT_MAX_SIZE || (size & 0xfff))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = sceKernelLockRangeProc(pid, (void *)va, size);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 506, "sceKernelLockRangeProc failed 0x%08x\n", ret);
		return ret;
	}
	fifo = GetFIFO(type);
	intr = SceCpuForDriver_4C38CE4D(&fifo->lock);
	ret = VSlotFIFOPop(fifo, &slot);
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&fifo->lock, intr);
	if (ret < 0) {
		sceKernelUnlockRangeProc(pid, (void *)va, size);
		return ret;
	}
	return VSlotMap(slot, pid, va, size, pSlot, pVbase);
}

/* the slot's pages unmapped */
static inline void VSlotUnmap(MemBlockObject *slot)
{
	SceUInt32 pages = VSlotGetAllocPhySizeRaw(slot) >> 12, i;
	uintptr_t vbase = slot->vbase;
	int ret;

	for (i = 0; i < pages; i++, vbase += 0x1000) {
		ret = PartitionUnmapL2PageTable(slot->partition, vbase, 0x1000, 0, 0);
		if (ret < 0)
			SCE_DBG_PRINTF(0xb, 608, "PartitionUnmapL2PageTable failed 0x%08x\n", ret);
	}
}

/* the slot unmapped, the process range unlocked and the slot back on its FIFO */
int sceKernelVSlotUnmapProcMemory(MemBlockObject *slot)
{
	VSlotInfo *info;
	int ret;

	VSlotUnmap(slot);
	info = VSlotGetInfoRaw(slot);
	ret = sceKernelUnlockRangeProc(info->pid, (void *)info->va, info->size);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 626, "sceKernelUnlockRangeProc failed 0x%08x\n", ret);
		return ret;
	}
	ret = VSlotFIFOPush(slot->vslotType, slot, 0);
	return ret < 0 ? ret : 0;
}

/* a new slot's object, info and page vector */
static inline void MakeVSlot(MemBlockObject *slot, SceUInt32 type)
{
	VSlotInfo *info;
	uintptr_t *pa;

	slot->base.sce_class = s_vslotClass;
	slot->attr |= 0x80000000;
	VSlotSetTypeRaw(slot, type);
	info = SceSysmemForKernel_85571907(sizeof(*info));
	if (info == NULL) {
		SCE_DBG_PRINTF(0xb, 308, "%p, 0x%08x: cannot allocate VSlotInfo\n", slot, type);
		return;
	}
	info->pid = -1;
	info->va = 0;
	info->size = 0;
	info->pa = NULL;
	pa = SceSysmemForKernel_85571907(sizeof(*pa) * (VSLOT_MAX_SIZE >> 12));
	if (pa == NULL) {
		SCE_DBG_PRINTF(0xb, 316, "%p, 0x%08x: cannot allocate pPhyPageVector\n", slot, type);
		SceSysmemForKernel_4233C16D(info);
		return;
	}
	pa[0] = 0;
	pa[1] = 0;
	info->pa = pa;
	VSlotSetInfoRaw(slot, info);
}

static inline void VSlotSetDefaultName(MemBlockObject *slot)
{
	const char *name;

	if (slot->vslotType == VSLOT_TYPE_16K_MXUU) {
		name = "SceVSlot16KMXUU";
	} else {
		SCE_DBG_ASSERT_FAIL(284);
		name = NULL;
	}
	MemBlockSetNameRaw(slot, name);
}

/* a slot of 16 KiB in the kernel address space's partition 6, onto its FIFO */
static inline void CreateVSlot(SceUInt32 type)
{
	MemBlockObject *slot = NULL;
	int ret;

	ret = PartitionAllocVSlot(AddressSpaceGetPartitionByIndex(SceSysmemForKernel_2E5CC243(), 6),
				  0x6060d006, 0x4000, &slot);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 379, "PartitionAllocVSlot failed 0x%08x\n", ret);
		return;
	}
	MakeVSlot(slot, type);
	MemBlockStatusStateChangeVirAllocFromFreeRaw(slot);
	VSlotSetDefaultName(slot);
	VSlotSetAllocPhySizeRaw(slot, 0);
	VSlotFIFOPush(type, slot, 1);
}

int VSlotModuleStart(void)
{
	SceClass *cls;
	SceUInt32 attributes;
	int ret, i;

	s_vslotClass = &s_vslotClassBody;
	ret = SceSysmemForKernel_7CDB7655(&s_vslotClassBody, "SceUIDVSlotClass",
					  SceSysmemForKernel_E4193EB6(), 0x40,
					  (SceClassCallback)VSlotCreate,
					  (SceClassCallback)VSlotDestroy);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(651);
		return ret;
	}
	cls = s_vslotClass;
	s_vslotFifo[0].lock = 0;
	attributes = cls->attributes;
	s_vslotFifo[0].count = 0;
	s_vslotFifo[0].low = 0;
	s_vslotFifo[0].head = NULL;
	cls->attributes = attributes | 0x1000;
	s_vslotFifo[0].tail = NULL;
	s_vslotFifo[1].lock = 0;
	s_vslotFifo[1].count = 0;
	s_vslotFifo[1].low = 0;
	s_vslotFifo[1].head = NULL;
	s_vslotFifo[1].tail = NULL;
	for (i = VSLOT_BOOT_COUNT; i != 0; i--)
		CreateVSlot(VSLOT_TYPE_16K_MXUU);
	return 0;
}
