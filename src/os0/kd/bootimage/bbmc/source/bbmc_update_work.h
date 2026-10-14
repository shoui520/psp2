#ifndef BBMC_UPDATE_WORK_H
#define BBMC_UPDATE_WORK_H

#include <stddef.h>
#include <psp2kern/types.h>

typedef struct BbmcUpdateFilePair {
	SceUID firstFile;
	SceUID secondFile;
	SceUInt32 firstSize;
	SceUInt32 secondSize;
	SceUInt32 forwardedValue;
	SceUInt32 state;
} BbmcUpdateFilePair;

typedef struct BbmcPairedUpdateWork {
	SceUInt32 operationMode;
	BbmcUpdateFilePair pairs[5];
	SceInt32 requestedIndex;
	SceUInt32 activeIndex;
	SceUInt32 totalBytes;
	SceUInt32 completedBytes;
} BbmcPairedUpdateWork;

typedef struct BbmcFileSetUpdateWork {
	SceUInt32 forwardedOptions[3];
	SceUID files[5];
	SceUInt32 sizes[5];
	SceUInt32 totalBytes;
	SceUInt32 completedBytes;
} BbmcFileSetUpdateWork;

typedef union BbmcUpdateWork {
	SceUID files[2];
	BbmcFileSetUpdateWork fileSet;
	BbmcPairedUpdateWork paired;
} BbmcUpdateWork;

_Static_assert(sizeof(BbmcUpdateFilePair) == 24, "update file pair size");
_Static_assert(offsetof(BbmcUpdateFilePair, firstFile) == 0,
	"update first file");
_Static_assert(offsetof(BbmcUpdateFilePair, secondFile) == 4,
	"update second file");
_Static_assert(offsetof(BbmcUpdateFilePair, firstSize) == 8,
	"update first size");
_Static_assert(offsetof(BbmcUpdateFilePair, secondSize) == 12,
	"update second size");
_Static_assert(offsetof(BbmcUpdateFilePair, forwardedValue) == 16,
	"update forwarded value");
_Static_assert(offsetof(BbmcUpdateFilePair, state) == 20,
	"update pair state");
_Static_assert(sizeof(BbmcPairedUpdateWork) == 140, "paired update size");
_Static_assert(offsetof(BbmcPairedUpdateWork, operationMode) == 0,
	"paired update mode");
_Static_assert(offsetof(BbmcPairedUpdateWork, pairs) == 4,
	"paired update files");
_Static_assert(offsetof(BbmcPairedUpdateWork, requestedIndex) == 124,
	"paired update requested index");
_Static_assert(offsetof(BbmcPairedUpdateWork, activeIndex) == 128,
	"paired update active index");
_Static_assert(offsetof(BbmcPairedUpdateWork, totalBytes) == 132,
	"paired update total bytes");
_Static_assert(offsetof(BbmcPairedUpdateWork, completedBytes) == 136,
	"paired update completed bytes");
_Static_assert(sizeof(BbmcFileSetUpdateWork) == 60, "file set update size");
_Static_assert(offsetof(BbmcFileSetUpdateWork, forwardedOptions) == 0,
	"file set forwarded options");
_Static_assert(offsetof(BbmcFileSetUpdateWork, files) == 12,
	"file set handles");
_Static_assert(offsetof(BbmcFileSetUpdateWork, sizes) == 32,
	"file set sizes");
_Static_assert(offsetof(BbmcFileSetUpdateWork, totalBytes) == 52,
	"file set total bytes");
_Static_assert(offsetof(BbmcFileSetUpdateWork, completedBytes) == 56,
	"file set completed bytes");
_Static_assert(sizeof(BbmcUpdateWork) == 140, "update working storage");

#endif
