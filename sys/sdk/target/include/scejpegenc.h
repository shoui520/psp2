/* Sony's SDK scejpegenc.h (Release 03.570.011): the JPEG encoder's records and constants */
#pragma once

#include <stddef.h>
#include <scetypes.h>

#define SCE_JPEGENC_ERROR_IMAGE_SIZE			((int)0x80650200)
#define SCE_JPEGENC_ERROR_INSUFFICIENT_BUFFER		((int)0x80650201)
#define SCE_JPEGENC_ERROR_INVALID_COMPRATIO		((int)0x80650202)
#define SCE_JPEGENC_ERROR_INVALID_PIXELFORMAT		((int)0x80650203)
#define SCE_JPEGENC_ERROR_INVALID_HEADER_MODE		((int)0x80650204)
#define SCE_JPEGENC_ERROR_INVALID_POINTER		((int)0x80650205)
#define SCE_JPEGENC_ERROR_NOT_PHY_CONTINUOUS_MEMORY	((int)0x80650206)
#define SCE_JPEGENC_ERROR_INVALID_INIT_PARAM		((int)0x80650207)

#define SCE_JPEGENC_COMPRATIO_NONE	0	/* the highest quality */
#define SCE_JPEGENC_COMPRATIO_DEFAULT	64
#define SCE_JPEGENC_COMPRATIO_HIGH	255	/* the lowest quality */

#define SCE_JPEGENC_PIXEL_YCBCR420	8
#define SCE_JPEGENC_PIXEL_YCBCR422	9
#define SCE_JPEGENC_PIXEL_RGBA8888	0
#define SCE_JPEGENC_PIXEL_BGRA8888	4

#define SCE_JPEGENC_PITCH_HW_CSC	16	/* or'ed into the format: convert RGBA to YCbCr */

#define SCE_JPEGENC_MODE_JPEG		0
#define SCE_JPEGENC_MODE_MJPEG		1

typedef void *SceJpegEncoderContext;

typedef struct {
	SceSize size;
	int iFrameWidth;
	int iFrameHeight;
	int pixelFormat;
	void *pJpeg;
	SceSize oJpegbufSize;
	int option;
} SceJpegEncoderInitParam;

_Static_assert(sizeof(SceJpegEncoderInitParam) == 28, "SceJpegEncoderInitParam");
_Static_assert(offsetof(SceJpegEncoderInitParam, pixelFormat) == 12, "JPEG encoder format");
_Static_assert(offsetof(SceJpegEncoderInitParam, oJpegbufSize) == 20, "JPEG encoder output size");
