/* JPEG decoder control records shared by the driver and its callers. */
#pragma once

#include <scetypes.h>

typedef struct SceJpegMJpegInitParam {
	SceSize size;
	int maxSplitDecoder;
	int option;
} SceJpegMJpegInitParam;

typedef struct SceJpegPitch {
	unsigned int x;
	unsigned int y;
} SceJpegPitch;

typedef struct SceJpegOutputInfo {
	int colorSpace;
	unsigned short imageWidth;
	unsigned short imageHeight;
	SceSize outputBufferSize;
	SceSize tempBufferSize;
	SceSize coefBufferSize;
	SceJpegPitch pitch[4];
} SceJpegOutputInfo;

typedef struct SceJpegSplitDecodeCtrl {
	unsigned char *pStreamBuffer;
	SceSize streamBufferSize;
	unsigned char *pWriteBuffer;
	SceSize writeBufferSize;
	int isEndOfStream;
	int decodeMode;
	SceJpegOutputInfo outputInfo;
	void *pOutputBuffer;
	void *pCoefBuffer;
	unsigned int internalData[3];
} SceJpegSplitDecodeCtrl;

_Static_assert(sizeof(SceJpegMJpegInitParam) == 12, "SceJpegMJpegInitParam");
_Static_assert(sizeof(SceJpegPitch) == 8, "SceJpegPitch");
_Static_assert(sizeof(SceJpegOutputInfo) == 52, "SceJpegOutputInfo");
_Static_assert(sizeof(SceJpegSplitDecodeCtrl) == 96, "SceJpegSplitDecodeCtrl");
