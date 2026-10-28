/* IPL boot initialization; the original identifies this function as boot_main. */
#include "ipl.h"
#include "ipl_data.h"
#include "loadelf_boot.h"
#include "memlmd.h"

extern int loadprx(SceLoadCoreBootInfo *bootInfo, u32 bootFlags, u32 bootArg);

#define SYSREG_MEMORY_CONFIG	0xBC100040
#define BOOT_INFO_ADDRESS	0x89F80000
#define BOOT_SCRAMBLE_SEED	0xBFC00200

_Static_assert(sizeof(SceLoadCoreBootInfo) == 128, "boot information size");

int kactivate(SceLoadCoreBootInfo *bootInfo, int index,
	      SceLoadCoreBootServices *services)
{
	SceLoadCoreExecFileInfo *execInfo = &g_execInfo[index];
	SceLoadCoreBootModuleInfo *module = &bootInfo->modules[index];
	u8 *file = module->modBuf;
	void *bootArgs[2];

	memset(execInfo, 0, sizeof(*execInfo));
	execInfo->topAddr = (u8 *)module->reserved;
	execInfo->apiType = 0;
	execInfo->modeAttribute = 0;
	execInfo->isSignChecked = 1;
	if (loadBootExecutable(file, execInfo, index, services) < 0)
		Panic(g_strKactivate, 293);
	memset(module->modBuf, 0, module->modSize);
	if (index == 1) {
		SceSize importsSize = execInfo->importsSize;

		services->importsInfo = execInfo->importsInfo;
		services->importsSize = importsSize;
		services->getLengthFunc = sceUtilsGetLoadModuleABLengthByPolling;
		services->prepareGetLengthFunc = memlmdPrepareGetLength;
		services->setMaskFunc = memlmdInitializeScrambleKey;
		services->loadcoreExecInfo = &g_execInfo[1];
		services->compareSubType = memlmdCompareSubType;
		services->compareLatestSubType = memlmdCompareLatestSubType;
		services->sysmemExecInfo = &g_execInfo[0];
	}
	sceKernelIcacheInvalidateAll();
	sceKernelDcacheWritebackAll();
	bootInfo->loadedModules = 2;
	if (index != 0) {
		if (index != 1)
			Panic(g_strKactivate, 376);
		bootArgs[0] = bootInfo;
		bootArgs[1] = services;
		callWithBootStack((int (*)(u32, void *))execInfo->entryAddr,
				  8, bootArgs, services->stackTop);
	} else {
		((int (*)(u32, SceLoadCoreBootServices *))execInfo->entryAddr)(
			4, services);
	}
	return 0;
}

int activateKernelModules(SceLoadCoreBootInfo *bootInfo)
{
	SceLoadCoreBootServices sysmemServices;
	SceLoadCoreBootServices loadcoreServices;
	SceLoadCoreBootModuleInfo *modules;
	SceSize clearSize = g_memClearSize;
	int intrFlags;

	if (clearSize == 0)
		clearSize = 0x88600000 - (u32)bootInfo->memBase;
	memset(bootInfo->memBase, 0, clearSize);
	memset(&sysmemServices, 0, sizeof(sysmemServices));
	memset(&loadcoreServices, 0, sizeof(loadcoreServices));
	bootInfo->buildVersion = g_buildVersion;
	modules = bootInfo->modules;
	sysmemServices.memSize = bootInfo->memSize;
	sysmemServices.model = bootInfo->model;
	sysmemServices.memClearSize = g_memClearSize;
	sysmemServices.bootMode = g_bootArgs[2];
	sysmemServices.numProtects = bootInfo->numProtects;
	sysmemServices.configMode = bootInfo->bootMode;
	sysmemServices.unknown0C = 3;
	sysmemServices.bootArg0 = bootInfo->bootArg0 | g_bootArgs[0];
	sysmemServices.bootArg1 = bootInfo->bootArg1 | g_bootArgs[1];
	sysmemServices.unknown10 = 6;
	sysmemServices.protects = bootInfo->protects;
	sysmemServices.initialMaxAllocSize = modules[1].modSize;
	sysmemServices.memBase = bootInfo->memBase;
	sysmemServices.handoffCallback = bootHandoffCallback;
	modules[0].reserved = (s32)bootInfo->memBase;
	sysmemServices.loadcoreServices = &loadcoreServices;
	sysmemServices.resumeArg = g_bootArgs[3];
	intrFlags = sceKernelCpuSuspendIntr();
	kactivate(bootInfo, 0, &sysmemServices);
	modules[1].reserved = (s32)loadcoreServices.loadCoreAddr;
	loadcoreServices.syscallTableSeed = g_bootArgs[3];
	kactivate(bootInfo, 1, &loadcoreServices);
	sceKernelCpuResumeIntr(intrFlags);
	return 0;
}

/* The host acknowledges each phase before either side reuses the key buffer. */
void *transferBootKey(void)
{
	const u8 *source = g_iplXorKey;
	u8 *destination = (u8 *)0xBFC00FC0;
	const u8 *end = source + sizeof(g_iplXorKey);

	do {
		u32 block[4];

		__builtin_memcpy(block, source, sizeof(block));
		__builtin_memcpy(destination, block, sizeof(block));
		source += sizeof(block);
		destination += sizeof(block);
	} while (source != end);
	memset(g_iplXorKey, 0, sizeof(g_iplXorKey));
	HW(0xABD78000) = 1;
	while (HW(0xABD78000) != 2)
		;
	HW(0xABD78000) = 3;
	while (HW(0xABD78000) != 0)
		;
	return memset((void *)0xBFC00FC0, 0, sizeof(g_iplXorKey));
}

int boot_main(void *memBase, SceSize memSize, u32 bootArg, u32 bootMode,
	      u32 bootFlags, u32 resumeArg, u32 model)
{
	SceLoadCoreBootInfo *bootInfo;
	u8 *seed = (u8 *)BOOT_SCRAMBLE_SEED;
	u32 block[3];
	u32 memoryConfig;

	memset((void *)0xBFC00000, 0, 512);
	memset((void *)0xBFC00220, 0, 3552);
	transferBootKey();
	/* The original reads each group before writing it to the scratchpad. */
	__builtin_memcpy(block, g_scrambleSeed, 12);
	__builtin_memcpy(seed, block, 12);
	__builtin_memcpy(block, g_scrambleSeed + 3, 12);
	__builtin_memcpy(seed + 12, block, 12);
	__builtin_memcpy(block, g_scrambleSeed + 6, 8);
	__builtin_memcpy(seed + 24, block, 8);
	memlmdInitializeScrambleKey(0, seed);
	g_bootArgs[2] = bootMode;
	g_bootArgs[3] = resumeArg;
	g_bootArgs[1] = bootFlags;
	g_bootArgs[0] = bootArg;
	g_bootInfo = (SceLoadCoreBootInfo *)BOOT_INFO_ADDRESS;
	memset((void *)BOOT_INFO_ADDRESS, 0, sizeof(*bootInfo));
	bootInfo = g_bootInfo;
	bootInfo->moduleApiDefaultMode = 2;
	bootInfo->memSize = memSize;
	bootInfo->modArgProtId = -1;
	bootInfo->self = (void *)BOOT_INFO_ADDRESS;
	bootInfo->model = model;
	bootInfo->reserved0 = 0;
	bootInfo->numProtects = 0;
	bootInfo->modProtId = -1;
	bootInfo->bootArg0 = bootArg;
	bootInfo->bootArg1 = bootFlags;
	bootInfo->buildVersion = 0;
	bootInfo->reserved1 = 0;
	memoryConfig = HW(SYSREG_MEMORY_CONFIG);
	bootInfo->modules = (SceLoadCoreBootModuleInfo *)
		(((u32)bootInfo + sizeof(*bootInfo) + 63) & ~63U);
	memoryConfig &= 0xFF000000;
	bootInfo->memBase = memBase;
	bootInfo->protects = NULL;
	bootInfo->configFile = NULL;
	if (memoryConfig <= 0x4FFFFFFF || memoryConfig == 0xF0000000) {
		if (model != 0)
			Panic(g_strBootMain, 568);
	} else if (memoryConfig <= 0x5FFFFFFF) {
		if (model != 1)
			Panic(g_strBootMain, 574);
	} else if (memoryConfig <= 0x6FFFFFFF) {
		if (model - 1 >= 2)
			Panic(g_strBootMain, 581);
	} else if ((memoryConfig & 0x80000000) == 0) {
		if (model != 2 && model != 4)
			Panic(g_strBootMain, 588);
	} else if (model != 3 && model != 5 && model != 6 && model != 7 &&
		   model != 8 && model != 9) {
		Panic(g_strBootMain, 599);
	}
	loadprx(g_bootInfo, bootFlags, bootArg);
	activateKernelModules(g_bootInfo);
	return 0;
}
