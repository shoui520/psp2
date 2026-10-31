/*
 * SceDeci4pSDbgp MMU L1 entry decoding (cpu_mmu.c).
 */

#include <scetypes.h>
#include <kernel/error.h>
#include <sysmem/debug.h>
#include "sdbgp_internal.h"

int sceKernelMMUL1GetInfo(const uint32_t *table, uintptr_t va, SceKernelMMUL1Info *info)
{
	uintptr_t index;
	uint32_t type;

	if (!info)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	index = va >> 20;
	type = table[index] & 3;
	if (type == 2) {
		info->vbase = index << 20;
		info->pbase = (table[index] >> 20) << 20;
		info->type = 1;
		return 0;
	}
	if (type == 3) {
		SCE_DBG_ASSERT_FAIL(40);
		return SCE_KERNEL_ERROR_MMU_ILLEGAL_L1_TYPE;
	}
	if (type == 1) {
		info->vbase = index << 20;
		info->pbase = (table[index] >> 10) << 10;
		info->type = 2;
		return 0;
	}
	info->vbase = va;
	info->pbase = 0;
	info->type = 0;
	return 0;
}
