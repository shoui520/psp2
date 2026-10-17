/* Module records in pspbtcnf.bin; the path is relocated in place. */
#ifndef IPL_BOOT_CONFIG_H
#define IPL_BOOT_CONFIG_H

#include "ipl.h"
#include "loadcore.h"
#include "psp_ipl.h"

/* The header and both record tables use offsets into one configuration image. */
typedef struct PspBootConfigHeader {
	u32 signature;
	u32 devkit;
	u32 reservedSignature[2];
	u32 modeStart;
	u32 numModes;
	u32 reservedModes[2];
	u32 moduleStart;
	u32 numModules;
	u32 reservedModules[2];
	u32 moduleNameStart;
	u32 moduleNameEnd;
	u32 reservedNames[2];
} PspBootConfigHeader;

typedef struct PspBootConfigMode {
	u16 maxSearch;
	u16 searchStart;
	u32 flags;
	u32 bootMode;
	union {
		u32 configNameOffset;
		const char *configName;
	};
	u32 bootArg0;
	u32 bootArg1;
	u32 buildVersion;
	SceSize memClearSize;
} PspBootConfigMode;

typedef struct PspBootConfigModule {
	union {
		u32 nameOffset;
		const char *name;
	};
	u32 reservedName;
	u32 flags;
	u32 reservedFlags;
	u8 hash[16];
} PspBootConfigModule;

_Static_assert(sizeof(PspBootConfigModule) == 32, "boot configuration module");
_Static_assert(sizeof(PspBootConfigHeader) == 64, "boot configuration header");
_Static_assert(sizeof(PspBootConfigMode) == 32, "boot configuration mode");

int openBootModuleFile(SceCompatBootFile *file, int optional,
		       u32 bootFlags, u32 bootArg, SceLoadCoreBootInfo *bootInfo);
int type2(void *image, SceSize imageSize, SceLoadCoreBootInfo *bootInfo,
	  char *configPath, PspBootConfigHeader *header,
	  u32 bootFlags, u32 bootArg);

#endif
