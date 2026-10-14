#ifndef BBMC_SIM_REFRESH_H
#define BBMC_SIM_REFRESH_H

#include <stddef.h>
#include <psp2kern/types.h>

typedef struct BbmcRefreshedFile {
	SceUInt16 fileId;
	SceUInt8 pathLength;
	SceUInt8 reserved;
	SceUInt16 path[2];
	SceUInt32 fileKind;
} BbmcRefreshedFile;

typedef struct BbmcFileRefreshDetail {
	SceUInt32 stage;
	SceUInt32 mode;
	SceUInt32 sessionType;
	SceUInt8 aidLength;
	SceUInt8 reserved;
	SceUInt16 fileCount;
	SceUInt8 aid[16];
	SceUInt8 reservedAid[4];
	BbmcRefreshedFile files[15];
} BbmcFileRefreshDetail;

_Static_assert(sizeof(BbmcRefreshedFile) == 12, "refreshed file size");
_Static_assert(offsetof(BbmcRefreshedFile, path) == 4, "refreshed file path");
_Static_assert(offsetof(BbmcRefreshedFile, fileKind) == 8,
	"refreshed file kind");
_Static_assert(sizeof(BbmcFileRefreshDetail) == 216, "file refresh detail size");
_Static_assert(offsetof(BbmcFileRefreshDetail, aidLength) == 12,
	"file refresh AID length");
_Static_assert(offsetof(BbmcFileRefreshDetail, fileCount) == 14,
	"file refresh count");
_Static_assert(offsetof(BbmcFileRefreshDetail, aid) == 16, "file refresh AID");
_Static_assert(offsetof(BbmcFileRefreshDetail, files) == 36,
	"file refresh files");

#endif
