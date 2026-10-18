/* Complete private mapping-file configuration. */
#pragma once

#include <stddef.h>
#include <scetypes.h>

typedef struct AppmgrMapFileOverlay {
	unsigned char type;
	unsigned char order;
	char src[292];
	char dst[292];
} AppmgrMapFileOverlay;

typedef struct AppmgrMapFile {
	char mapFilePath[1024];
	SceUID file;
	int line;
	int version;
	char app0[292];
	char savedata0[292];
	char addcont0[292];
	char otherApps[292];
	char otherAppsPatches[292];
	char otherAppsSavedata[292];
	char otherAppsContents[292];
	AppmgrMapFileOverlay overlays[8];
} AppmgrMapFile;

_Static_assert(sizeof(AppmgrMapFileOverlay) == 586, "map-file overlay size");
_Static_assert(offsetof(AppmgrMapFile, file) == 1024, "map-file descriptor");
_Static_assert(offsetof(AppmgrMapFile, app0) == 1036, "map-file paths");
_Static_assert(offsetof(AppmgrMapFile, overlays) == 3080, "map-file overlays");
_Static_assert(sizeof(AppmgrMapFile) == 7768, "complete map-file configuration");
