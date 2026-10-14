/* Native GPU register mapping and register-dump storage helpers. */
#include "sce/sysmem.h"
#include "gpu-dump.h"

extern void *memset(void *destination, int value, unsigned size);
extern void *memcpy(void *destination, const void *source, unsigned size);

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
	platform->sRegsSysPBase.uiAddr = 0xe8400000;
	platform->sRegsCpuPBase.uiAddr = 0xe8400000;
	platform->ui32RegsSize = 0x20000;
	memset(&options, 0, sizeof(options));
	options.size = sizeof(options);
	options.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	options.paddr = 0xe8400000;
	uid = ksceKernelAllocMemBlock("SceSGX543Reg",
				    SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_SO_RW,
				    0x20000, &options);
	gpuDumpRegion.state.registerMemblockUid = uid;
	ksceKernelGetMemBlockBase(uid, &platform->pvRegsCpuVBase);
	platform->ui32IRQ = 0x34;
	return 0;
}
