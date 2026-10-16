/* IPL boot initialization; the original identifies this function as boot_main. */
#include "ipl.h"
#include "ipl_data.h"
#include "memlmd.h"

extern int loadprx(SceLoadCoreBootInfo *bootInfo, u32 bootFlags, u32 bootArg);
extern int activateKernelModules(SceLoadCoreBootInfo *bootInfo);

#define SYSREG_MEMORY_CONFIG	0xBC100040
#define BOOT_INFO_ADDRESS	0x89F80000
#define BOOT_SCRAMBLE_SEED	0xBFC00200

_Static_assert(sizeof(SceLoadCoreBootInfo) == 128, "boot information size");

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
