/* Complete PARAMS wire record: original allocation and SFO capacity are 1024. */
#pragma once

#include <stddef.h>
#include <ss_mgr/ss_mgr.h>

typedef struct AppmgrSavedataParams {
	SceUInt32 creationMarker;
	SceKernelOpenPsId openPsId;
	SceUInt32 accountIndex;
	SceUInt32 consoleType;
	SceUInt32 cexUseCount;
	SceUInt32 dexUseCount;
	SceUInt32 otherUseCount;
	char createdTitleId[12];
	char lastTitleId[12];
	SceUInt32 reserved;
	SceUInt32 maxSize;
	SceUInt32 flags;
	unsigned char reservedData[948];
} AppmgrSavedataParams;

_Static_assert(sizeof(AppmgrSavedataParams) == 1024,
	       "savedata PARAMS record size");
_Static_assert(offsetof(AppmgrSavedataParams, creationMarker) == 0 &&
	       offsetof(AppmgrSavedataParams, openPsId) == 4 &&
	       offsetof(AppmgrSavedataParams, accountIndex) == 20 &&
	       offsetof(AppmgrSavedataParams, consoleType) == 24 &&
	       offsetof(AppmgrSavedataParams, cexUseCount) == 28 &&
	       offsetof(AppmgrSavedataParams, dexUseCount) == 32 &&
	       offsetof(AppmgrSavedataParams, otherUseCount) == 36 &&
	       offsetof(AppmgrSavedataParams, createdTitleId) == 40 &&
	       offsetof(AppmgrSavedataParams, lastTitleId) == 52 &&
	       offsetof(AppmgrSavedataParams, reserved) == 64 &&
	       offsetof(AppmgrSavedataParams, maxSize) == 68 &&
	       offsetof(AppmgrSavedataParams, flags) == 72 &&
	       offsetof(AppmgrSavedataParams, reservedData) == 76,
	       "savedata PARAMS member offsets");

