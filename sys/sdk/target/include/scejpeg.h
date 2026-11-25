/* Sony's SDK scejpeg.h (Release 03.570.011): the JPEG decoder's records and constants */
#pragma once

#include <stddef.h>
#include <scetypes.h>

#define SCE_JPEG_MJPEG_INIT_OPTION_NONE			0
#define SCE_JPEG_MJPEG_INIT_OPTION_LPDDR2_MEMORY	1

#define SCE_JPEG_MAX_HEIGHT	65535
#define SCE_JPEG_MAX_WIDTH	65535

#define SCE_JPEG_MJPEG_WITH_DHT				0
#define SCE_JPEG_MJPEG_WITHOUT_DHT			1
#define SCE_JPEG_MJPEG_ANY_SAMPLING_WITHOUT_DHT		2
#define SCE_JPEG_MJPEG_ANY_SAMPLING			3

#define SCE_JPEG_MJPEG_DOWNSCALE_1_2	(1 << 4)
#define SCE_JPEG_MJPEG_DOWNSCALE_1_4	(1 << 5)
#define SCE_JPEG_MJPEG_DOWNSCALE_1_8	(1 << 6)

#define SCE_JPEG_PIXEL_RGBA8888		0
#define SCE_JPEG_PIXEL_BGRA8888		4

#define SCE_JPEG_COLORSPACE_JFIF	0x00
#define SCE_JPEG_COLORSPACE_BT601	0x10

#define SCE_JPEG_NO_CSC_OUTPUT		(-1)

#define SCE_JPEG_OK					0

#define SCE_JPEG_ERROR_IMAGE_EMPTY			-2140864509	/* 0x80650003 */
#define SCE_JPEG_ERROR_BAD_MARKER_LENGTH		-2140864508	/* 0x80650004 */
#define SCE_JPEG_ERROR_BAD_DHT_COUNTS			-2140864507	/* 0x80650005 */
#define SCE_JPEG_ERROR_BAD_DHT_INDEX			-2140864506	/* 0x80650006 */
#define SCE_JPEG_ERROR_BAD_DQT_INDEX			-2140864505	/* 0x80650007 */
#define SCE_JPEG_ERROR_DECODE_ERROR			-2140864503	/* 0x80650009 */
#define SCE_JPEG_ERROR_INVALID_POINTER			-2140864496	/* 0x80650010 */
#define SCE_JPEG_ERROR_BAD_COMPONENT_ID			-2140864495	/* 0x80650011 */
#define SCE_JPEG_ERROR_UNSUPPORT_COLORSPACE		-2140864493	/* 0x80650013 */
#define SCE_JPEG_ERROR_BAD_MCU_SIZE			-2140864492	/* 0x80650014 */
#define SCE_JPEG_ERROR_BAD_PRECISION			-2140864491	/* 0x80650015 */
#define SCE_JPEG_ERROR_UNSUPPORT_SAMPLING		-2140864490	/* 0x80650016 */
#define SCE_JPEG_ERROR_COMPONENT_COUNT			-2140864489	/* 0x80650017 */
#define SCE_JPEG_ERROR_CONVERSION_NOTIMPL		-2140864488	/* 0x80650018 */
#define SCE_JPEG_ERROR_EOI_EXPECTED			-2140864487	/* 0x80650019 */
#define SCE_JPEG_ERROR_UNSUPPORT_IMAGE_SIZE		-2140864480	/* 0x80650020 */
#define SCE_JPEG_ERROR_NO_HUFF_TABLE			-2140864479	/* 0x80650021 */
#define SCE_JPEG_ERROR_NO_QUANT_TABLE			-2140864478	/* 0x80650022 */
#define SCE_JPEG_ERROR_NO_SOI				-2140864477	/* 0x80650023 */
#define SCE_JPEG_ERROR_BAD_DQT_MARKER			-2140864476	/* 0x80650024 */
#define SCE_JPEG_ERROR_BAD_DHT_MARKER			-2140864475	/* 0x80650025 */
#define SCE_JPEG_ERROR_BAD_DRI_MARKER			-2140864474	/* 0x80650026 */
#define SCE_JPEG_ERROR_BAD_SOF_MARKER			-2140864473	/* 0x80650027 */
#define SCE_JPEG_ERROR_BAD_SOS_MARKER			-2140864472	/* 0x80650028 */
#define SCE_JPEG_ERROR_SOF_DUPLICATE			-2140864471	/* 0x80650029 */
#define SCE_JPEG_ERROR_NO_LOSSLESS_SUPPORT		-2140864463	/* 0x80650031 */
#define SCE_JPEG_ERROR_NO_ARITH_SUPPORT			-2140864462	/* 0x80650032 */
#define SCE_JPEG_ERROR_UNKNOWN_MARKER			-2140864459	/* 0x80650035 */
#define SCE_JPEG_ERROR_BAD_RESTART_MARKER		-2140864458	/* 0x80650036 */
#define SCE_JPEG_ERROR_UNEXPECTED_MARKER		-2140864457	/* 0x80650037 */
#define SCE_JPEG_ERROR_INVALID_REGION			-2140864456	/* 0x80650038 */
#define SCE_JPEG_ERROR_INVALID_STATE			-2140864455	/* 0x80650039 */
#define SCE_JPEG_ERROR_CANNOT_CONTINUE			-2140864448	/* 0x80650040 */
#define SCE_JPEG_ERROR_MEMORY_SIZE			-2140864447	/* 0x80650041 */
#define SCE_JPEG_ERROR_CANNOT_INIT			-2140864446	/* 0x80650042 */
#define SCE_JPEG_ERROR_CANNOT_FINISH			-2140864445	/* 0x80650043 */
#define SCE_JPEG_ERROR_INVALID_COLOR_FORMAT		-2140864432	/* 0x80650050 */
#define SCE_JPEG_ERROR_NOT_PHY_CONTINUOUS_MEMORY	-2140864429	/* 0x80650053 */
#define SCE_JPEG_ERROR_INVALID_DECODE_MODE		-2140864416	/* 0x80650060 */
#define SCE_JPEG_ERROR_BAD_PROGRESSIVE_PARAM		-2140864415	/* 0x80650061 */
#define SCE_JPEG_ERROR_EXCEED_MAX_SPLIT_DECODER		-2140864414	/* 0x80650062 */
#define SCE_JPEG_ERROR_INIT_DONE			-2140864413	/* 0x80650063 */
#define SCE_JPEG_ERROR_INPUT_SUSPENDED			-2140864412	/* 0x80650064 */
#define SCE_JPEG_ERROR_INPUT_DATA_TOO_BIG		-2140864411	/* 0x80650065 */
#define SCE_JPEG_ERROR_INVALID_DATA_SIZE		-2140864410	/* 0x80650066 */
#define SCE_JPEG_ERROR_INVALID_INIT_PARAM		-2140864409	/* 0x80650067 */

#define SCE_JPEG_ERROR_UNSUPPORT_DOWNSCALE	(SCE_JPEG_ERROR_INVALID_DECODE_MODE)
#define SCE_JPEG_ERROR_OUT_OF_MEMORY		(SCE_JPEG_ERROR_MEMORY_SIZE)

#define SCE_JPEG_CS_UNKNOWN		(0 << 16)
#define SCE_JPEG_CS_GRAYSCALE		(1 << 16)
#define SCE_JPEG_CS_YCBCR		(2 << 16)

#define SCE_JPEG_CS_H1V1		(0x101)
#define SCE_JPEG_CS_H2V1		(0x201)
#define SCE_JPEG_CS_H4V1		(0x401)
#define SCE_JPEG_CS_H1V2		(0x102)
#define SCE_JPEG_CS_H2V2		(0x202)
#define SCE_JPEG_CS_H1V4		(0x104)

typedef struct {
	SceSize size;
	int maxSplitDecoder;
	int option;
} SceJpegMJpegInitParam;

typedef struct {
	int colorSpace;
	unsigned short imageWidth;
	unsigned short imageHeight;
	SceSize outputBufferSize;
	SceSize tempBufferSize;
	SceSize coefBufferSize;
	struct {
		unsigned int x;
		unsigned int y;
	} pitch[4];
} SceJpegOutputInfo;

typedef struct {
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

_Static_assert(sizeof(SceJpegOutputInfo) == 52, "SceJpegOutputInfo");
_Static_assert(sizeof(SceJpegSplitDecodeCtrl) == 96, "SceJpegSplitDecodeCtrl");

int sceJpegInitMJpeg(int maxSplitDecoder);
int sceJpegFinishMJpeg(void);
int sceJpegInitMJpegWithParam(const SceJpegMJpegInitParam *pInitParam);

int sceJpegDecodeMJpeg(
	const unsigned char *pJpeg,
	SceSize isize,
	void *pRGBA,
	SceSize osize,
	int decodeMode,
	void *pTempBuffer,
	SceSize tempBufferSize,
	void *pCoefBuffer,
	SceSize coefBufferSize);

int sceJpegDecodeMJpegYCbCr(
	const unsigned char *pJpeg,
	SceSize isize,
	unsigned char *pYCbCr,
	SceSize osize,
	int decodeMode,
	void *pCoefBuffer,
	SceSize coefBufferSize);

int sceJpegMJpegCsc(
	void *pRGBA,
	const unsigned char *pYCbCr,
	int xysize,
	int iFrameWidth,
	int colorOption,
	int sampling);

int sceJpegCsc(
	void *pRGBA,
	const unsigned char *pYCbCr,
	int xysize,
	int iFrameWidth,
	int colorOption,
	int sampling);

int sceJpegGetOutputInfo(
	const unsigned char *pJpeg,
	SceSize isize,
	int outputFormat,
	int decodeMode,
	SceJpegOutputInfo *pOutputInfo);

int sceJpegCreateSplitDecoder(SceJpegSplitDecodeCtrl *pCtrl);
int sceJpegDeleteSplitDecoder(SceJpegSplitDecodeCtrl *pCtrl);
int sceJpegSplitDecodeMJpeg(SceJpegSplitDecodeCtrl *pCtrl);
