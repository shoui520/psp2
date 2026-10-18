#pragma once

#include <stddef.h>
#include <scetypes.h>

typedef struct BbmcSubscribeFeatureIdParam {
	SceInt32 subscriberId;
	SceUID callbackId;
	SceUInt32 featureId;
	SceUInt32 eventMask;
} BbmcSubscribeFeatureIdParam;

typedef struct BbmcUpdatePackage {
	SceUInt32 chunkCount;
	SceUInt32 chunkIndex;
	void *buffer;
	SceSize size;
} BbmcUpdatePackage;

typedef struct BbmcDriverInfo {
	SceInt32 currentState;
	SceInt32 targetState;
	SceUInt32 firstBootReply;
	SceUInt32 secondBootReply;
} BbmcDriverInfo;

_Static_assert(sizeof(BbmcSubscribeFeatureIdParam) == 16,
	"BBMC subscription record size");
_Static_assert(offsetof(BbmcSubscribeFeatureIdParam, callbackId) == 4 &&
	offsetof(BbmcSubscribeFeatureIdParam, featureId) == 8 &&
	offsetof(BbmcSubscribeFeatureIdParam, eventMask) == 12,
	"BBMC subscription record layout");
_Static_assert(sizeof(BbmcUpdatePackage) == 16,
	"BBMC update package size");
_Static_assert(offsetof(BbmcUpdatePackage, chunkIndex) == 4 &&
	offsetof(BbmcUpdatePackage, buffer) == 8 &&
	offsetof(BbmcUpdatePackage, size) == 12,
	"BBMC update package layout");
_Static_assert(sizeof(BbmcDriverInfo) == 16, "BBMC driver information size");
_Static_assert(offsetof(BbmcDriverInfo, targetState) == 4 &&
	offsetof(BbmcDriverInfo, firstBootReply) == 8 &&
	offsetof(BbmcDriverInfo, secondBootReply) == 12,
	"BBMC driver information layout");
