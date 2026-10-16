/* Public AVC decoder records from avcdec.h, SDK Release 03.570.011. */
#pragma once

#include <stddef.h>
#include <sce/videodec.h>

#define SCE_AVCDEC_ERROR_INVALID_TYPE (-2141061119)
#define SCE_AVCDEC_ERROR_INVALID_PARAM (-2141061118)
#define SCE_AVCDEC_ERROR_OUT_OF_MEMORY (-2141061117)
#define SCE_AVCDEC_ERROR_INVALID_STATE (-2141061116)
#define SCE_AVCDEC_ERROR_UNSUPPORT_IMAGE_SIZE (-2141061115)
#define SCE_AVCDEC_ERROR_INVALID_COLOR_FORMAT (-2141061114)
#define SCE_AVCDEC_ERROR_NOT_PHY_CONTINUOUS_MEMORY (-2141061113)
#define SCE_AVCDEC_ERROR_ALREADY_USED (-2141061112)
#define SCE_AVCDEC_ERROR_INVALID_POINTER (-2141061111)
#define SCE_AVCDEC_ERROR_ES_BUFFER_FULL (-2141061110)
#define SCE_AVCDEC_ERROR_INITIALIZE (-2141061109)
#define SCE_AVCDEC_ERROR_NOT_INITIALIZE (-2141061108)
#define SCE_AVCDEC_ERROR_INVALID_STREAM (-2141061107)
#define SCE_AVCDEC_ERROR_INVALID_ARGUMENT_SIZE (-2141061106)
#define SCE_AVCDEC_ERROR_GREATER_THAN_1200_AT_LV30 (-2141061105)
#define SCE_AVCDEC_ERROR_INTERLACED (-2141061104)
#define SCE_AVCDEC_ERROR_INVALID_PICTURE (-2141061055)

typedef struct SceAvcdecQueryDecoderInfo {
	SceUInt32 horizontal;
	SceUInt32 vertical;
	SceUInt32 numOfRefFrames;
} SceAvcdecQueryDecoderInfo;

typedef struct SceAvcdecDecoderInfo {
	SceUInt32 frameMemSize;
} SceAvcdecDecoderInfo;

typedef struct SceAvcdecBuf {
	void *pBuf;
	SceUInt32 size;
} SceAvcdecBuf;

typedef struct SceAvcdecCtrl {
	SceUInt32 handle;
	SceAvcdecBuf frameBuf;
} SceAvcdecCtrl;

typedef struct SceAvcdecAu {
	SceVideodecTimeStamp pts;
	SceVideodecTimeStamp dts;
	SceAvcdecBuf es;
} SceAvcdecAu;

typedef struct SceAvcdecInfo {
	SceUInt32 numUnitsInTick;
	SceUInt32 timeScale;
	SceUChar8 fixedFrameRateFlag;

	SceUChar8 aspectRatioIdc;
	SceUShort16 sarWidth;
	SceUShort16 sarHeight;

	SceUChar8 colourPrimaries;
	SceUChar8 transferCharacteristics;
	SceUChar8 matrixCoefficients;

	SceUChar8 videoFullRangeFlag;

	SceUChar8 reserved[3];

	SceUChar8 flag;
	SceUChar8 padding[2];
	SceVideodecTimeStamp outputPts;
} SceAvcdecInfo;

typedef struct SceAvcdecFrameOptionRGBA {
	SceUInt8 alpha;
	SceUInt8 cscCoefficient;
	SceUInt8 reserved[14];
} SceAvcdecFrameOptionRGBA;

typedef union SceAvcdecFrameOption {
	SceUInt8 reserved[16];
	SceAvcdecFrameOptionRGBA rgba;
} SceAvcdecFrameOption;

typedef struct SceAvcdecFrame {
	SceUInt32 pixelType;
	SceUInt32 framePitch;
	SceUInt32 frameWidth;
	SceUInt32 frameHeight;

	SceUInt32 horizontalSize;
	SceUInt32 verticalSize;

	SceUInt32 frameCropLeftOffset;
	SceUInt32 frameCropRightOffset;
	SceUInt32 frameCropTopOffset;
	SceUInt32 frameCropBottomOffset;

	SceAvcdecFrameOption opt;

	void *pPicture[2];
} SceAvcdecFrame;

typedef struct SceAvcdecPicture {
	SceSize size;
	SceAvcdecFrame frame;
	SceAvcdecInfo info;
} SceAvcdecPicture;

typedef struct SceAvcdecArrayPicture {
	SceUInt32 numOfOutput;
	SceUInt32 numOfElm;
	SceAvcdecPicture **pPicture;
} SceAvcdecArrayPicture;

typedef struct SceAvcdecInfoForInterlaced {
	SceUInt32 numUnitsInTick;
	SceUInt32 timeScale;
	SceUChar8 fixedFrameRateFlag;

	SceUChar8 aspectRatioIdc;
	SceUShort16 sarWidth;
	SceUShort16 sarHeight;

	SceUChar8 colourPrimaries;
	SceUChar8 transferCharacteristics;
	SceUChar8 matrixCoefficients;

	SceUChar8 videoFullRangeFlag;

	SceUChar8 picStruct[2];
	SceUChar8 ctType;

	SceUChar8 flag;
	SceUChar8 padding[2];
	SceVideodecTimeStamp outputPts;
} SceAvcdecInfoForInterlaced;

typedef struct SceAvcdecPictureForInterlaced {
	SceSize size;
	SceAvcdecFrame frame;
	SceAvcdecInfoForInterlaced info;
} SceAvcdecPictureForInterlaced;

VITASDK_BUILD_ASSERT_EQ(12, SceAvcdecQueryDecoderInfo);
VITASDK_BUILD_ASSERT_EQ(4, SceAvcdecDecoderInfo);
VITASDK_BUILD_ASSERT_EQ(8, SceAvcdecBuf);
VITASDK_BUILD_ASSERT_EQ(12, SceAvcdecCtrl);
VITASDK_BUILD_ASSERT_EQ(24, SceAvcdecAu);
VITASDK_BUILD_ASSERT_EQ(32, SceAvcdecInfo);
VITASDK_BUILD_ASSERT_EQ(16, SceAvcdecFrameOptionRGBA);
VITASDK_BUILD_ASSERT_EQ(16, SceAvcdecFrameOption);
VITASDK_BUILD_ASSERT_EQ(64, SceAvcdecFrame);
VITASDK_BUILD_ASSERT_EQ(100, SceAvcdecPicture);
VITASDK_BUILD_ASSERT_EQ(12, SceAvcdecArrayPicture);
VITASDK_BUILD_ASSERT_EQ(32, SceAvcdecInfoForInterlaced);
VITASDK_BUILD_ASSERT_EQ(100, SceAvcdecPictureForInterlaced);
_Static_assert(offsetof(SceAvcdecInfo, reserved) == 18, "SceAvcdecInfo reserved");
_Static_assert(offsetof(SceAvcdecInfo, flag) == 21, "SceAvcdecInfo flag");
_Static_assert(offsetof(SceAvcdecInfo, padding) == 22, "SceAvcdecInfo padding");
_Static_assert(offsetof(SceAvcdecInfo, outputPts) == 24, "SceAvcdecInfo outputPts");
_Static_assert(offsetof(SceAvcdecInfoForInterlaced, picStruct) == 18, "SceAvcdecInfoForInterlaced picStruct");
_Static_assert(offsetof(SceAvcdecInfoForInterlaced, ctType) == 20, "SceAvcdecInfoForInterlaced ctType");
_Static_assert(offsetof(SceAvcdecInfoForInterlaced, flag) == 21, "SceAvcdecInfoForInterlaced flag");
_Static_assert(offsetof(SceAvcdecInfoForInterlaced, padding) == 22, "SceAvcdecInfoForInterlaced padding");
_Static_assert(offsetof(SceAvcdecInfoForInterlaced, outputPts) == 24, "SceAvcdecInfoForInterlaced outputPts");
_Static_assert(offsetof(SceAvcdecFrame, opt) == 40, "SceAvcdecFrame opt");
_Static_assert(offsetof(SceAvcdecFrame, pPicture) == 56, "SceAvcdecFrame pPicture");
_Static_assert(offsetof(SceAvcdecPicture, frame) == 4, "SceAvcdecPicture frame");
_Static_assert(offsetof(SceAvcdecPicture, info) == 68, "SceAvcdecPicture info");
_Static_assert(offsetof(SceAvcdecPictureForInterlaced, frame) == 4, "SceAvcdecPictureForInterlaced frame");
_Static_assert(offsetof(SceAvcdecPictureForInterlaced, info) == 68, "SceAvcdecPictureForInterlaced info");
_Static_assert(offsetof(SceAvcdecArrayPicture, numOfElm) == 4, "SceAvcdecArrayPicture numOfElm");
_Static_assert(offsetof(SceAvcdecArrayPicture, pPicture) == 8, "SceAvcdecArrayPicture pPicture");
