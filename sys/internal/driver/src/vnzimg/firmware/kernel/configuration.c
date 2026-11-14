/*
 * The kernel's header ("VK0", at +0x200) and the image's configuration (at
 * +0x300), which the kernel reads at VNZ_KERNEL_CONFIGURATION: the cores it
 * starts, the memory map, each core's kind, the drivers it resumes and
 * suspends, and the threads it starts. The pointers are physical, as the
 * kernel reads them.
 */
#include "kernel.h"

#define PHYSICAL(object)	((const void *)VNZ_PHYSICAL_OF((uint32_t)(object)))

/* the link's (image.ld) */
extern const uint8_t vnzKernelKernelSize[];

extern void vnzKernelReset(void);
extern const VnzKernelModule g_vnzKernelReportModule;
extern const VnzKernelModule g_vnzKernelApplicationModule;

const VnzKernelModule g_vnzKernelKernelModule VNZ_KERNEL_SECTION(".kernel.module") = {
	.magic = VNZ_KERNEL_MODULE_KERNEL,
	.self = VNZ_PHYSICAL_OF(0x800200),
	.size = (uint32_t)vnzKernelKernelSize,
	.entry = vnzKernelReset,
	.attributes = VNZ_KERNEL_MODULE_ATTRIBUTES,
};

/* the memory regions, by id */
static const struct {
	uint32_t count;
	VnzKernelMemoryRegion regions[7];
} g_vnzKernelMemoryMap VNZ_KERNEL_SECTION(".kernel.configuration") = {
	7, {
		{ VNZ_KERNEL_MEMORY_HEAP,	0x40a00000, 0x200000 },
		{ VNZ_KERNEL_MEMORY_WORK,	0x20000000, 0x8000000 },
		{ VNZ_KERNEL_MEMORY_POOL0,	0x40960000, 0xa0000 },
		{ VNZ_KERNEL_MEMORY_POOL1,	0x40958000, 0x8000 },
		{ VNZ_KERNEL_MEMORY_EXCEPTION,	0x40810800, 0x800 },
		{ VNZ_KERNEL_MEMORY_CRASH,	0x40957000, 0x1000 },
		{ VNZ_KERNEL_MEMORY_LOCAL_RAM,	VNZ_LOCAL_RAM, 0x1000 },
	},
};

/* each core's kind (vnzKernelCoreType) and the interrupts a crash records: cores 48-54
 * of kind 2, core 55, the application's, of none */
static const VnzKernelCoreTable g_vnzKernelCoreTable VNZ_KERNEL_SECTION(".kernel.configuration") = {
	VNZ_CORES, {
		{ 2, 0 }, { 2, 0 }, { 2, 0 }, { 2, 0 },
		{ 2, 0 }, { 2, 0 }, { 2, 0 }, { 0, 0 },
	},
};

/* the drivers the kernel resumes and suspends */
static const struct {
	uint32_t count;
	const VnzKernelModule *modules[1];
} g_vnzKernelModuleTable VNZ_KERNEL_SECTION(".kernel.configuration") = {
	1, { PHYSICAL(&g_vnzKernelReportModule) },
};

/* the application's thread, on the last core */
static const struct {
	uint32_t count;
	VnzKernelStartThread threads[1];
} g_vnzKernelStartTable VNZ_KERNEL_SECTION(".kernel.configuration") = {
	1, {
		{ VNZ_FIRST_CORE_ID + VNZ_CORES - 1, 4, PHYSICAL(&g_vnzKernelApplicationModule),
			0x400, 0, 0 },
	},
};

VnzKernelConfiguration g_vnzKernelConfiguration VNZ_KERNEL_SECTION(".kernel.configuration.header") = {
	.cores = (1u << VNZ_CORES) - 1,
	.memory = PHYSICAL(&g_vnzKernelMemoryMap),
	.coreTable = PHYSICAL(&g_vnzKernelCoreTable),
	.modules = PHYSICAL(&g_vnzKernelModuleTable),
	.startThreads = PHYSICAL(&g_vnzKernelStartTable),
};
