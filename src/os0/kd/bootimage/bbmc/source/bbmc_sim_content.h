#ifndef BBMC_SIM_CONTENT_H
#define BBMC_SIM_CONTENT_H

#include <stddef.h>
#include <psp2kern/types.h>

typedef struct BbmcSimFile {
	SceUInt16 fileId;
	SceUInt8 pathLength;
	SceUInt8 reserved;
	SceUInt8 path[8];
} BbmcSimFile;

typedef struct BbmcSimContentInfo {
	SceUInt32 validFields;
	SceUInt8 sw1;
	SceUInt8 sw2;
	SceUInt8 reservedStatus[2];
	SceUInt16 dataLength;
	SceUInt8 reservedLength[2];
	SceUInt8 data[32];
	BbmcSimFile file;
} BbmcSimContentInfo;

_Static_assert(sizeof(BbmcSimFile) == 12, "SIM file size");
_Static_assert(offsetof(BbmcSimFile, path) == 4, "SIM path");
_Static_assert(sizeof(BbmcSimContentInfo) == 56, "SIM content size");
_Static_assert(offsetof(BbmcSimContentInfo, data) == 12, "SIM data");
_Static_assert(offsetof(BbmcSimContentInfo, file) == 44, "SIM file echo");

#endif
