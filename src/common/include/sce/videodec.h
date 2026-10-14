/* Video decoder types from videodec.h, SDK Release 03.570.011. */
#pragma once

#include <psp2/types.h>
#include <vitasdk/build_utils.h>
#include <stddef.h>

#define SCE_VIDEODEC_ERROR_INVALID_STATE (-2141059068)
#define SCE_VIDEODEC_ERROR_INVALID_STREAM (-2141059059)

typedef struct SceVideodecQueryInitInfoHwAvcdec {
	SceSize size;
	SceUInt32 horizontal;
	SceUInt32 vertical;
	SceUInt32 numOfRefFrames;
	SceUInt32 numOfStreams;
} SceVideodecQueryInitInfoHwAvcdec;

typedef union SceVideodecQueryInitInfo {
	SceUInt8 reserved[32];
	SceVideodecQueryInitInfoHwAvcdec hwAvc;
} SceVideodecQueryInitInfo;

typedef struct SceVideodecMemInfo {
	SceUInt32 memSize;
} SceVideodecMemInfo;

typedef struct SceVideodecBuf {
	void *pBuf;
	SceUInt32 size;
} SceVideodecBuf;

typedef struct SceVideodecCtrl {
	SceVideodecBuf memBuf;
	SceUID memBufUid;
	SceUIntVAddr vaContext;
	SceUInt32 contextSize;
} SceVideodecCtrl;

typedef struct SceVideodecTimeStamp {
	SceUInt32 upper;
	SceUInt32 lower;
} SceVideodecTimeStamp;

VITASDK_BUILD_ASSERT_EQ(8, SceVideodecTimeStamp);
VITASDK_BUILD_ASSERT_EQ(20, SceVideodecQueryInitInfoHwAvcdec);
VITASDK_BUILD_ASSERT_EQ(32, SceVideodecQueryInitInfo);
VITASDK_BUILD_ASSERT_EQ(4, SceVideodecMemInfo);
VITASDK_BUILD_ASSERT_EQ(8, SceVideodecBuf);
VITASDK_BUILD_ASSERT_EQ(20, SceVideodecCtrl);
_Static_assert(offsetof(SceVideodecQueryInitInfoHwAvcdec, size) == 0,
	"SceVideodecQueryInitInfoHwAvcdec.size");
_Static_assert(offsetof(SceVideodecQueryInitInfoHwAvcdec, horizontal) == 4,
	"SceVideodecQueryInitInfoHwAvcdec.horizontal");
_Static_assert(offsetof(SceVideodecQueryInitInfoHwAvcdec, vertical) == 8,
	"SceVideodecQueryInitInfoHwAvcdec.vertical");
_Static_assert(offsetof(SceVideodecQueryInitInfoHwAvcdec, numOfRefFrames) == 12,
	"SceVideodecQueryInitInfoHwAvcdec.numOfRefFrames");
_Static_assert(offsetof(SceVideodecQueryInitInfoHwAvcdec, numOfStreams) == 16,
	"SceVideodecQueryInitInfoHwAvcdec.numOfStreams");
_Static_assert(offsetof(SceVideodecQueryInitInfo, hwAvc) == 0,
	"SceVideodecQueryInitInfo.hwAvc");
_Static_assert(offsetof(SceVideodecMemInfo, memSize) == 0,
	"SceVideodecMemInfo.memSize");
_Static_assert(offsetof(SceVideodecBuf, pBuf) == 0,
	"SceVideodecBuf.pBuf");
_Static_assert(offsetof(SceVideodecBuf, size) == 4,
	"SceVideodecBuf.size");
_Static_assert(offsetof(SceVideodecCtrl, memBuf) == 0,
	"SceVideodecCtrl.memBuf");
_Static_assert(offsetof(SceVideodecCtrl, memBufUid) == 8,
	"SceVideodecCtrl.memBufUid");
_Static_assert(offsetof(SceVideodecCtrl, vaContext) == 12,
	"SceVideodecCtrl.vaContext");
_Static_assert(offsetof(SceVideodecCtrl, contextSize) == 16,
	"SceVideodecCtrl.contextSize");
