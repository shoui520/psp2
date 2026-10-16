/* Boot configuration helpers; the stripped IPL has no source-file records. */
#include "boot_config.h"
#include "ipl_data.h"

extern int openBootFile(SceCompatBootFile *file, int optional);
extern int compareBootModuleHash(const SceLoadCoreBootModuleInfo *module,
				 const PspBootConfigModule *configModule);

int openBootModuleFile(SceCompatBootFile *file, int optional,
		       u32 bootFlags, u32 bootArg, SceLoadCoreBootInfo *bootInfo)
{
	/* The original forwards only the file and optional flag. */
	return openBootFile(file, optional);
}

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
			/* Even a zero offset is relocated in the original. */
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
	if (mode->bootArg0 != 0)
		g_bootArgs[0] = mode->bootArg0;
	if (mode->bootArg1 != 0)
		g_bootArgs[1] = mode->bootArg1;
	if (mode->buildVersion != 0)
		g_buildVersion = mode->buildVersion;
	if (mode->memClearSize != 0)
		g_memClearSize = mode->memClearSize;
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

		if ((modeFlags & configModule->flags & 0xFFF) == 0)
			continue;
		configModule->name = (const char *)
			((u32)names + configModule->nameOffset);
		file.name = configModule->name;
		/* Sony leaves buffer and size untouched before this call. */
		result = openBootModuleFile(&file,
			(configModule->flags & 0xFF0000) == 0x40000,
			bootFlags, bootArg, bootInfo->self);
		size = file.size;
		if (result != 0) {
			module->modSize = 0;
			module->attr |= 0x80000000;
			Panic(g_strType2, 542);
		}
		module->attr = 0;
		module->modSize = size;
		module->bootData = 0;
		module->argSize = 0;
		module->argPartId = 0;
		module->modPath = (char *)configModule->name;
		loadMode = configModule->flags & 0xFF0000;
		module->modBuf = file.buffer;
		if (loadMode == 0x20000) {
			module->attr = 1;
		} else if (loadMode <= 0x20000) {
			module->attr = 0;
			if (loadMode != 0x10000)
				Panic(g_strType2, 508);
		} else {
			if (loadMode != 0x40000) {
				module->attr = 0;
				Panic(g_strType2, 508);
			}
			if ((bootInfo->moduleApiDefaultMode & 0xFF) == 0)
				module->bootData = 0x210;
			module->attr |= 3;
		}
		if ((configModule->flags & 0x80000000) != 0)
			module->attr |= 0x100;
		if (module->modSize != 0 ||
		    (configModule->flags & 0xFF0000) != 0x40000) {
			if (compareBootModuleHash(module, configModule) < 0)
				Panic(g_strType2, 527);
		}
		maxSearch = mode->maxSearch;
		module->attr |= 0x100;
		module++;
		bootInfo->numModules++;
	}
	return 0;
}
