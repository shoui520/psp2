/*
 * SceDeci4pSDbgp MMU L1/L2 address space queries (sysmem_address_space.c).
 */

#include <scetypes.h>
#include <kernel/error.h>
#include <sysmem/address_space.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include "sdbgp_internal.h"

int sceKernelAddressSpaceGetMMUL1Info(SceUID asid, uintptr_t va, SceKernelMMUL1Info *info)
{
	AddressSpaceObject *as;
	int ret;

	ret = sceGUIDGetObject(asid, &as);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 40, "sceKernelAddressSpaceGetAS failed 0x%08x\n", ret);
		return ret;
	}
	return L1PageTableGetInfo(as->context->l1pt, va, info);
}

int sceKernelAddressSpaceGetMMUL1AllInfo(SceUID asid, SceKernelMMUL1AllInfo *info)
{
	AddressSpaceObject *as;
	int ret;

	if (!info)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (info->size != sizeof(*info))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	ret = sceGUIDGetObject(asid, &as);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 119, "sceKernelAddressSpaceGetAS failed 0x%08x\n", ret);
		return ret;
	}
	return L1PageTableGetAllInfo(as->context->l1pt, info);
}

int sceKernelAddressSpaceGetMMUL2AllInfo(SceUID asid, uintptr_t va, SceKernelMMUL2AllInfo *info)
{
	AddressSpaceObject *as;
	Partition *part;
	int ret;
	int i;

	if (!info)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (info->size != sizeof(*info))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	ret = sceGUIDGetObject(asid, &as);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 154, "sceKernelAddressSpaceGetAS failed 0x%08x\n", ret);
		return ret;
	}
	for (i = 0; i < ADDRESS_SPACE_PARTITIONS; i++) {
		part = SceSysmemForKernel_1F84FE44(as, i);
		if (!part)
			continue;
		ret = PartitionGetMMUL2AllInfo(part, va, info);
		if (ret != 1)
			break;
	}
	return ret;
}
