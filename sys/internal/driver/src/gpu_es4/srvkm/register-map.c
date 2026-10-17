/* Native GPU register mapping and register-dump storage helpers. */
#include <sysmem/sysmem.h>
#include "gpu-dump.h"
#include <gpu/sgx_regs.h>
#include <sysmem/sysclib.h>

GpuDumpRegion gpuDumpRegion;

unsigned GetRegisterDumpAllocationSize(void)
{
	return GPU_DUMP_SPAN_SIZE;
}

unsigned StoreRegisterDump(const void *data)
{
	if (data)
		memcpy(gpuDumpRegion.bytes, data, GPU_DUMP_DATA_SIZE);
	else
		memset(gpuDumpRegion.bytes, 0, GPU_DUMP_DATA_SIZE);
	return 0;
}

SceGpuRegisterDump *GetRegisterDumpBuffer(void)
{
	return &gpuDumpRegion.state.dump;
}

/* Native internal name unavailable. Failed allocation/base lookup still
 * stores the allocation result, sets interrupt 0x34 and returns zero.
 */
int MapGpuRegisters(void)
{
	SceKernelAllocMemBlockKernelOpt options;
	SGX_DEVICE_MAP *platform = &gpuDumpRegion.state.platform;
	int uid;

	platform->ui32Flags = 0;
	platform->sRegsSysPBase.uiAddr = SGX_REG_BASE;
	platform->sRegsCpuPBase.uiAddr = SGX_REG_BASE;
	platform->ui32RegsSize = SGX_REG_SIZE;
	memset(&options, 0, sizeof(options));
	options.size = sizeof(options);
	options.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	options.paddr = SGX_REG_BASE;
	uid = SceSysmemForDriver_C94850C9("SceSGX543Reg",
				    SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_SO_RW,
				    SGX_REG_SIZE, &options);
	gpuDumpRegion.state.registerMemblockUid = uid;
	SceSysmemForDriver_A841EDDA(uid, &platform->pvRegsCpuVBase);
	platform->ui32IRQ = 0x34;
	return 0;
}
