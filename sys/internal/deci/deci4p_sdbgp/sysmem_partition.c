/*
 * SceDeci4pSDbgp partition and L1 page table queries (sysmem_partition.c).
 */

#include <scetypes.h>
#include <kernel/error.h>
#include <sysmem/address_space.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include "sdbgp_internal.h"

int PartitionGetMMUL2AllInfo(Partition *part, uintptr_t va,
			     SceKernelMMUL2AllInfo *info)
{
	L2PageTable *l2;
	int i;

	if (va < part->tiny.vbase || va - part->tiny.vbase > part->tiny.size)
		return 1;
	l2 = SceSysmemForKernel_456ECB54(part, va);
	if (!l2) {
		SCE_DBG_PRINTF_FUNC(0xb, 82, "PartitionGetMMUL2Info",
				    "not found l2pagetable for %p\n", (void *)va);
		return SCE_KERNEL_ERROR_PARTITION_ERROR;
	}
	for (i = 0; i < 256; i++)
		info->table[i] = l2->table[i];
	return 0;
}

int L1PageTableGetInfo(L1PageTable *l1pt, uintptr_t va, SceKernelMMUL1Info *info)
{
	SceKernelIntrStatus intr;
	const uint32_t *table;
	int ret;

	intr = SceCpuForDriver_EC53D007(&l1pt->lock);
	table = va < l1pt->split ? l1pt->low : l1pt->high;
	ret = sceKernelMMUL1GetInfo(table, va, info);
	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&l1pt->lock, intr);
	return ret;
}

int L1PageTableGetAllInfo(L1PageTable *l1pt, SceKernelMMUL1AllInfo *info)
{
	SceKernelIntrStatus intr;
	uint32_t entry;
	unsigned int i;

	intr = SceCpuForDriver_EC53D007(&l1pt->lock);
	info->split = l1pt->split;
	for (i = 0; i < 4096; i++) {
		entry = (i << 20) < l1pt->split ? l1pt->low[i] : l1pt->high[i];
		info->table[i] = entry;
		if ((entry & 3) == 1)
			info->l2Count++;
	}
	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&l1pt->lock, intr);
	return 0;
}
