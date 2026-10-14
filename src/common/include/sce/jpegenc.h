/* Public JPEG encoder record from scejpegenc.h, SDK Release 03.570.011. */
#pragma once

#include <stddef.h>
#include <vitasdk/build_utils.h>
#include <psp2/types.h>

typedef struct {
	SceSize size;
	int iFrameWidth;
	int iFrameHeight;
	int pixelFormat;
	void *pJpeg;
	SceSize oJpegbufSize;
	int option;
} SceJpegEncoderInitParam;

VITASDK_BUILD_ASSERT_EQ(28, SceJpegEncoderInitParam);
_Static_assert(offsetof(SceJpegEncoderInitParam, size) == 0, "JPEG encoder size");
_Static_assert(offsetof(SceJpegEncoderInitParam, iFrameWidth) == 4, "JPEG encoder width");
_Static_assert(offsetof(SceJpegEncoderInitParam, iFrameHeight) == 8, "JPEG encoder height");
_Static_assert(offsetof(SceJpegEncoderInitParam, pixelFormat) == 12, "JPEG encoder format");
_Static_assert(offsetof(SceJpegEncoderInitParam, pJpeg) == 16, "JPEG encoder output");
_Static_assert(offsetof(SceJpegEncoderInitParam, oJpegbufSize) == 20, "JPEG encoder output size");
_Static_assert(offsetof(SceJpegEncoderInitParam, option) == 24, "JPEG encoder option");
