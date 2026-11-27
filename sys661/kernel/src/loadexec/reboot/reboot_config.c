/*
 * Boot configuration parser for ePSP reboot.bin (pspbtcnf.bin type2 loader).
 */

#include "boot_config.h"
#include "ipl_data.h"
#include "loadelf_boot.h"

extern int openBootModuleFile(SceCompatBootFile *file, int optional,
			      u32 bootFlags, u32 bootArg,
			      SceLoadCoreBootInfo *bootInfo);

int type2(void *image, SceSize imageSize, SceLoadCoreBootInfo *bootInfo,
	  char *configPath, PspBootConfigHeader *header,
	  u32 bootFlags, u32 bootArg)
{
	PspBootConfigMode *mode = (PspBootConfigMode *)
		((u8 *)image + header->modeStart);
	PspBootConfigModule *configModule = (PspBootConfigModule *)
		((u8 *)image + header->moduleStart);
	const char *names = (const char *)image + header->moduleNameStart;
	SceLoadCoreBootModuleInfo *module;
	SceCompatBootFile file;
	u32 modeCount = header->numModes;
	u32 modeFlags = 0;
	u32 maxSearch;
	u32 index = 0;

	if (modeCount != 0) {
		do {
			mode->configName = (const char *)
				((u32)names + mode->configNameOffset);
			if (bootInfo->bootMode == mode->bootMode) {
				modeFlags = mode->flags;
				modeCount = header->numModes;
				break;
			}
			modeCount = header->numModes;
			index++;
			mode++;
		} while (index < modeCount);
	}
	if (index == modeCount)
		Panic(g_strType2, 349);

	if (mode->configName != NULL) {
		strncpy(configPath, mode->configName, 256);
		bootInfo->configFile = configPath;
	} else {
		bootInfo->configFile = NULL;
	}

	maxSearch = mode->maxSearch;
	configModule += mode->searchStart;
	bootInfo->numModules = 0;
	module = bootInfo->modules;
	for (index = 0; index < maxSearch; index++, configModule++) {
		u32 loadMode;
		int result;
		SceSize size;

		if ((modeFlags & configModule->flags & 0xFFFU) == 0)
			continue;
		configModule->name = (const char *)
			((u32)names + configModule->nameOffset);
		file.name = configModule->name;
		result = openBootModuleFile(&file,
			(configModule->flags & 0xFF0000U) == 0x40000U,
			bootFlags, bootArg, bootInfo->self);
		size = file.size;
		if (result != 0) {
			module->modSize = 0;
			module->attr |= 0x80000000U;
			Panic(g_strType2, 542);
		}
		module->attr = 0;
		module->modSize = size;
		module->bootData = 0;
		module->argSize = 0;
		module->argPartId = 0;
		module->modPath = (char *)configModule->name;
		loadMode = configModule->flags & 0xFF0000U;
		module->modBuf = file.buffer;
		if (loadMode == 0x20000U) {
			module->attr = 1;
		} else if (loadMode <= 0x20000U) {
			module->attr = 0;
			if (loadMode != 0x10000U)
				Panic(g_strType2, 508);
		} else {
			if (loadMode != 0x40000U) {
				module->attr = 0;
				Panic(g_strType2, 508);
			}
			if ((bootInfo->moduleApiDefaultMode & 0xFFU) == 0)
				module->bootData = 0x210;
			module->attr |= 3;
		}
		if ((configModule->flags & 0x80000000U) != 0)
			module->attr |= 0x100U;
		maxSearch = mode->maxSearch;
		module++;
		bootInfo->numModules++;
	}
	return 0;
}

int _loadprx_by_pspbtcnf_bin(SceLoadCoreBootInfo *bootInfo, char *configPath,
			     u32 bootFlags, u32 bootArg)
{
	SceCompatBootFile file;
	int newSize;

	file.name = g_strPspbtcnf;
	if (openBootModuleFile(&file, 0, bootFlags, bootArg,
			       bootInfo->self) < 0)
		return -1;
	if (file.size == 0)
		return -1;
	newSize = getBootModuleLength(file.buffer, file.size, 1);
	if (newSize < 0)
		Panic(g_strLoadprxByPspbtcnf, 702);
	file.size = (SceSize)newSize;
	if (((PspBootConfigHeader *)file.buffer)->signature != 0x0F803001U)
		Panic(g_strLoadprxByPspbtcnf, 726);
	if (type2(file.buffer, file.size, bootInfo, configPath,
		  file.buffer, bootFlags, bootArg) < 0)
		Panic(g_strLoadprxByPspbtcnf, 731);
	memset(file.buffer, 0, file.size);
	return 0;
}
