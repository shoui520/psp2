/* Sony's SDK kernel/openpsid.h: the Open PSID, the console's public identifier */
#pragma once
#include <scetypes.h>

typedef struct SceKernelOpenPsId {
	SceUChar8 id[16];
} SceKernelOpenPsId;
