/*
 * scejpeg_decmain.c: ARM software baseline JPEG / MJPEG decoder for ScePspemu
 * ME JPEG emulation (0x8101b850..0x8101d5f7).
 */
#include "host_common.h"

#define PSPEMU_JPEG_ERR_IMAGE_EMPTY       ((int)0x80650003)
#define PSPEMU_JPEG_ERR_BAD_MARKER_LEN    ((int)0x80650004)
#define PSPEMU_JPEG_ERR_BAD_DHT_COUNTS    ((int)0x80650005)
#define PSPEMU_JPEG_ERR_BAD_DHT_INDEX     ((int)0x80650006)
#define PSPEMU_JPEG_ERR_BAD_DQT_INDEX     ((int)0x80650007)
#define PSPEMU_JPEG_ERR_DECODE_ERROR      ((int)0x80650009)
#define PSPEMU_JPEG_ERR_INVALID_POINTER   ((int)0x80650010)
#define PSPEMU_JPEG_ERR_BAD_COMP_ID       ((int)0x80650011)
#define PSPEMU_JPEG_ERR_UNSUP_COLORSPACE  ((int)0x80650013)
#define PSPEMU_JPEG_ERR_BAD_MCU_SIZE      ((int)0x80650014)
#define PSPEMU_JPEG_ERR_BAD_PRECISION     ((int)0x80650015)
#define PSPEMU_JPEG_ERR_UNSUP_SAMPLING    ((int)0x80650016)
#define PSPEMU_JPEG_ERR_COMP_COUNT        ((int)0x80650017)
#define PSPEMU_JPEG_ERR_EOI_EXPECTED      ((int)0x80650019)
#define PSPEMU_JPEG_ERR_UNSUP_IMAGE_SIZE  ((int)0x80650020)
#define PSPEMU_JPEG_ERR_NO_HUFF_TABLE     ((int)0x80650021)
#define PSPEMU_JPEG_ERR_NO_QUANT_TABLE    ((int)0x80650022)
#define PSPEMU_JPEG_ERR_NO_SOI            ((int)0x80650023)
#define PSPEMU_JPEG_ERR_BAD_DQT_MARKER    ((int)0x80650024)
#define PSPEMU_JPEG_ERR_BAD_DHT_MARKER    ((int)0x80650025)
#define PSPEMU_JPEG_ERR_BAD_DRI_MARKER    ((int)0x80650026)
#define PSPEMU_JPEG_ERR_BAD_SOF_MARKER    ((int)0x80650027)
#define PSPEMU_JPEG_ERR_BAD_SOS_MARKER    ((int)0x80650028)
#define PSPEMU_JPEG_ERR_SOF_DUPLICATE     ((int)0x80650029)
#define PSPEMU_JPEG_ERR_NO_LOSSLESS       ((int)0x80650031)
#define PSPEMU_JPEG_ERR_NO_ARITH          ((int)0x80650032)
#define PSPEMU_JPEG_ERR_UNKNOWN_MARKER    ((int)0x80650035)
#define PSPEMU_JPEG_ERR_BAD_RESTART       ((int)0x80650036)
#define PSPEMU_JPEG_ERR_UNEXPECTED_MARKER ((int)0x80650037)
#define PSPEMU_JPEG_ERR_INVALID_REGION    ((int)0x80650038)
#define PSPEMU_JPEG_ERR_INVALID_STATE     ((int)0x80650039)
#define PSPEMU_JPEG_ERR_CANNOT_CONTINUE   ((int)0x80650040)
#define PSPEMU_JPEG_ERR_MEMORY_SIZE       ((int)0x80650041)

#define JPEG_NOINLINE __attribute__((noinline, noclone))

typedef struct PspJpegHuffTable {
	int8_t   fastBits[256];
	int8_t   fastVal[256];
	uint8_t  slowSym[247];
	uint8_t  valid;
	uint16_t maxCode[7];
	uint16_t valOffset[8];
} PspJpegHuffTable;

typedef struct PspJpegComponent {
	uint8_t           id;
	uint8_t           blocksPerMcu;
	int16_t           dcPred;
	int16_t          *quantTable;
	PspJpegHuffTable *dcTable;
	PspJpegHuffTable *acTable;
	uint8_t          *outPtr;
	int32_t           outStride;
	uint8_t           downscale;
	uint8_t           colStep;
	uint8_t           shiftH;
	uint8_t           shiftV;
	int32_t           outRowStep;
} PspJpegComponent;

typedef struct PspJpegDecoder {
	int16_t           quantTables[4][64];
	PspJpegHuffTable  huffTables[4][2];
	PspJpegComponent *mcuCompPtrs[25];
	int32_t           mcuBlockSteps[24];
	PspJpegComponent  components[4];
	const uint8_t    *streamCur;
	const uint8_t    *streamEnd;
	uint32_t          bitBuf;
	int8_t            bitCount;
	uint8_t           flags;
	int16_t           curMcuRow;
	uint16_t          regionStartX;
	uint16_t          regionStartY;
	uint16_t          regionWidthMcu;
	uint16_t          regionEndY;
	uint16_t          mcuCols;
	uint16_t          mcuRows;
	uint16_t          restartInterval;
	uint16_t          restartCountdown;
	uint8_t           nextRestartMarker;
	uint8_t           colorSpaceCode;
	uint8_t           scanColorSpaceCode;
	uint8_t           isProgressive;
	uint8_t           scanStart;
	uint8_t           scanEnd;
	uint16_t          padTail;
	uint16_t          imageWidth;
	uint16_t          imageHeight;
} PspJpegDecoder;

_Static_assert(sizeof(PspJpegHuffTable) == 790, "PspJpegHuffTable size");
_Static_assert(sizeof(PspJpegComponent) == 32, "PspJpegComponent size");
_Static_assert(offsetof(PspJpegDecoder, mcuCompPtrs) == 6832, "mcuCompPtrs offset");
_Static_assert(offsetof(PspJpegDecoder, mcuBlockSteps) == 6932, "mcuBlockSteps offset");
_Static_assert(offsetof(PspJpegDecoder, components) == 7028, "components offset");
_Static_assert(offsetof(PspJpegDecoder, streamCur) == 7156, "streamCur offset");
_Static_assert(offsetof(PspJpegDecoder, bitCount) == 7168, "bitCount offset");
_Static_assert(offsetof(PspJpegDecoder, colorSpaceCode) == 7189, "colorSpaceCode offset");
_Static_assert(offsetof(PspJpegDecoder, imageWidth) == 7196, "imageWidth offset");
_Static_assert(sizeof(PspJpegDecoder) == 7200, "PspJpegDecoder size");

static const uint16_t g_jpegQuantScaleTable[64] = {
	32768, 45451, 45451, 42813, 63042, 42813, 38531, 59384,
	59384, 38531, 32768, 53444, 55938, 53444, 32768, 25746,
	45451, 50343, 50343, 45451, 25746, 17734, 35710, 42813,
	45308, 42813, 35710, 17734,  9041, 24598, 33638, 38531,
	38531, 33638, 24598,  9041, 12540, 23170, 30274, 32768,
	30274, 23170, 12540, 11812, 20853, 25746, 25746, 20853,
	11812, 10631, 17734, 20228, 17734, 10631,  9041, 13933,
	13933,  9041,  7103,  9598,  7103,  4893,  4893,  2494,
};

static const uint8_t g_jpegZigzagByteOffset[64] = {
	 16,   2,   4,  18,  32,  48,  34,  20,
	  6,   8,  22,  36,  50,  64,  80,  66,
	 52,  38,  24,  10,  12,  26,  40,  54,
	 68,  82,  96, 112,  98,  84,  70,  56,
	 42,  28,  14,  30,  44,  58,  72,  86,
	100, 114, 116, 102,  88,  74,  60,  46,
	 62,  76,  90, 104, 118, 120, 106,  92,
	 78,  94, 108, 122, 124, 110, 126, 126,
};

static const uint8_t g_jpegDefaultMjpegDht[416] = {
	0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x01, 0x00, 0x03,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x10, 0x00, 0x02, 0x01, 0x03, 0x03,
	0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7D, 0x01, 0x02, 0x03, 0x00, 0x04,
	0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81,
	0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0, 0x24, 0x33, 0x62, 0x72, 0x82,
	0x09, 0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x34, 0x35, 0x36,
	0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56,
	0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75, 0x76,
	0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95,
	0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3,
	0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA,
	0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
	0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0x11, 0x00, 0x02,
	0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77, 0x00, 0x01,
	0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22,
	0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0, 0x15, 0x62,
	0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34, 0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26, 0x27, 0x28,
	0x29, 0x2A, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A,
	0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A,
	0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
	0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
	0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5,
	0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE2, 0xE3,
	0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA,
};

static int JPEG_NOINLINE sceJpegDecSetupOutput(PspJpegDecoder *dec, uint8_t *outBuf);
static int JPEG_NOINLINE sceJpegDecParseDht(PspJpegDecoder *dec, int markerLen);
static int JPEG_NOINLINE sceJpegDecInitBitStream(PspJpegDecoder *dec, const uint8_t *ptr);
static int JPEG_NOINLINE sceJpegDecDecodeMcuRow(PspJpegDecoder *dec);
static int JPEG_NOINLINE sceJpegDecParseSos(PspJpegDecoder *dec);
static int JPEG_NOINLINE sceJpegDecParseMarkers(PspJpegDecoder *dec);

static inline PspJpegDecoder *alignDecoderPtr(void *rawCtx)
{
	return (PspJpegDecoder *)(((uintptr_t)rawCtx + 127U) & ~(uintptr_t)127U);
}

static inline uint32_t jpegRor32(uint32_t val, int shift)
{
	unsigned int s = (unsigned int)shift & 31U;
	return (s == 0U) ? val : ((val >> s) | (val << (32U - s)));
}

static inline uint32_t jpegClampSample(int val, int minVal)
{
	if (val < minVal)
		val = minVal;
	else if (val >= 0x4000)
		return 255U;
	return (uint32_t)val >> 6;
}

static inline uint32_t jpegRefillBytes(const uint8_t **streamCur, const uint8_t *streamEnd, uint32_t curBits)
{
	const uint8_t *ptr = *streamCur;
	uint32_t bits = curBits;

	if (ptr < streamEnd) {
		uint8_t firstByte = ptr[0];
		uint8_t nextByte = ptr[1];

		bits = (bits & 0xFFFF00FFU) | ((uint32_t)firstByte << 8);
		if (firstByte == 0xFFU) {
			if (nextByte != 0U)
				return bits;
			nextByte = ptr[2];
			ptr++;
		}

		bits = (bits & 0xFFFFFF00U) | (uint32_t)nextByte;
		{
			uint8_t thirdByte = ptr[2];
			if (nextByte == 0xFFU) {
				if (thirdByte != 0U) {
					*streamCur = ptr + 1;
					return bits;
				}
				thirdByte = ptr[3];
				ptr++;
			}

			bits = (bits & 0x00FFFFFFU) | ((uint32_t)thirdByte << 24);
			if (thirdByte == 0xFFU) {
				if (ptr[3] == 0U)
					ptr++;
				else
					ptr--;
			}
			*streamCur = ptr + 3;
		}
	}
	return bits;
}

static int sceJpegDecSetupOutput(PspJpegDecoder *dec, uint8_t *outBuf)
{
	uint32_t regionWidth = dec->regionWidthMcu;
	uint8_t csCode;
	unsigned int csIdx;
	int32_t yStride;
	uint32_t yBlocks;
	uint32_t i;

	if (((uintptr_t)outBuf & 0x3FU) != 0U)
		return PSPEMU_JPEG_ERR_INVALID_POINTER;

	csCode = dec->colorSpaceCode;
	csIdx = (unsigned int)(uint8_t)((csCode & 0x0FU) - 5U);
	yStride = (int32_t)(regionWidth << (((csCode >> 2) & 3U) + 2U));
	dec->components[0].outStride = yStride;

	if (csIdx > 8U)
		return PSPEMU_JPEG_ERR_UNSUP_COLORSPACE;

	yBlocks = dec->components[0].blocksPerMcu;
	if (((1U << csIdx) & 0x111U) != 0U) {
		for (i = 0U; i < yBlocks; i++)
			dec->mcuBlockSteps[i] = 8;
	} else if (((1U << csIdx) & 0x006U) != 0U) {
		if (yBlocks > 1U) {
			for (i = 0U; i < yBlocks - 1U; i++)
				dec->mcuBlockSteps[i] = 8 * yStride;
			dec->mcuBlockSteps[yBlocks - 1U] = 8 - 8 * yStride * (int32_t)(yBlocks - 1U);
		} else {
			dec->mcuBlockSteps[0] = 8;
		}
	} else if (((1U << csIdx) & 0x020U) != 0U) {
		dec->mcuBlockSteps[0] = 8;
		dec->mcuBlockSteps[1] = 8 * (yStride - 1);
		dec->mcuBlockSteps[2] = 8;
		dec->mcuBlockSteps[3] = 8 - 8 * yStride;
	} else {
		return PSPEMU_JPEG_ERR_UNSUP_COLORSPACE;
	}

	dec->components[0].outRowStep = (yStride << ((csCode & 3U) + 2U)) - yStride;
	for (i = 1U; i < 4U; i++) {
		int32_t cStride = (int32_t)(8U * regionWidth);
		dec->mcuBlockSteps[yBlocks + i - 1U] = 8;
		dec->components[i].outStride = cStride;
		dec->components[i].outRowStep = cStride * (16 >> (dec->components[i].colStep >> 1)) - cStride;
	}

	if (outBuf != NULL) {
		uint8_t *planePtr = outBuf;
		uint32_t regionRows = (uint32_t)(dec->regionEndY - dec->regionStartY);

		for (i = 0U; i < 4U; i++) {
			uint32_t denom = (uint32_t)dec->components[i].downscale * (uint32_t)dec->components[i].colStep;
			if (denom == 0U)
				break;
			dec->components[i].outPtr = planePtr;
			planePtr += (regionWidth * ((uint32_t)dec->components[i].blocksPerMcu << 7) * regionRows) / denom;
		}
	}
	return 0;
}

int sceJpegDecDecode(void *ctx)
{
	PspJpegDecoder *dec = alignDecoderPtr(ctx);
	int ret;

	if (dec->curMcuRow >= (int16_t)dec->regionEndY)
		return PSPEMU_JPEG_ERR_CANNOT_CONTINUE;
	if (dec->streamCur >= dec->streamEnd)
		return PSPEMU_JPEG_ERR_CANNOT_CONTINUE;

	ret = sceJpegDecDecodeMcuRow(dec);
	if (ret < 0)
		return ret;

	dec->curMcuRow++;
	if (dec->curMcuRow == (int16_t)dec->mcuRows) {
		if (dec->isProgressive != 0U)
			return 0;
		ret = sceJpegDecParseMarkers(dec);
		if (ret == 0xD9)
			return 0;
		if (ret >= 0) {
			dec->curMcuRow = -1;
			return PSPEMU_JPEG_ERR_EOI_EXPECTED;
		}
		return ret;
	}

	dec->components[0].outPtr += dec->components[0].outRowStep;
	dec->components[1].outPtr += dec->components[1].outRowStep;
	dec->components[2].outPtr += dec->components[2].outRowStep;
	dec->components[3].outPtr += dec->components[3].outRowStep;
	return 1;
}

int sceJpegDecInit(void *ctx, const void *srcBuf, uint32_t srcSize, uint32_t downscale, int opt)
{
	PspJpegDecoder *dec = alignDecoderPtr(ctx);
	const uint8_t *buf = (const uint8_t *)srcBuf;
	uint32_t pos = 0U;
	int ret;
	uint32_t mcuBytes;
	uint32_t i;

	dec->isProgressive = 0U;
	dec->restartCountdown = 0U;
	for (i = 0U; i < 4U; i++) {
		dec->huffTables[i][0].valid = 0U;
		dec->huffTables[i][1].valid = 0U;
		dec->components[i].blocksPerMcu = 0U;
	}

	if (srcSize == 1U)
		return PSPEMU_JPEG_ERR_NO_SOI;

	while (buf[pos + 1U] != 0xD8U) {
		pos += (buf[pos + 1U] == 0xFFU) ? 2U : 1U;
		if (pos >= srcSize - 1U)
			return PSPEMU_JPEG_ERR_NO_SOI;
	}
	if (buf[pos] != 0xFFU)
		return PSPEMU_JPEG_ERR_NO_SOI;
	pos += 2U;
	if (pos == srcSize)
		return PSPEMU_JPEG_ERR_NO_SOI;

	dec->flags = (uint8_t)opt;
	dec->streamCur = &buf[pos];
	dec->streamEnd = &buf[srcSize];
	dec->colorSpaceCode = 0U;

	ret = sceJpegDecParseMarkers(dec);
	if (ret < 0)
		return ret;
	if (ret != 0xDA) {
		dec->curMcuRow = -1;
		return PSPEMU_JPEG_ERR_UNEXPECTED_MARKER;
	}

	dec->regionStartX = 0U;
	dec->regionStartY = 0U;
	dec->curMcuRow = 0;
	dec->regionWidthMcu = dec->mcuCols;
	dec->regionEndY = dec->mcuRows;

	ret = sceJpegDecParseSos(dec);
	if (ret < 0) {
		dec->curMcuRow = -1;
		return ret;
	}

	dec->scanColorSpaceCode = dec->colorSpaceCode;
	if (opt != 0) {
		if ((dec->colorSpaceCode & 0x0FU) != 0x0AU)
			return PSPEMU_JPEG_ERR_UNSUP_SAMPLING;
		if ((dec->colorSpaceCode & 0xF0U) != 0x20U)
			return PSPEMU_JPEG_ERR_UNSUP_COLORSPACE;
	}

	mcuBytes = 0U;
	for (i = 0U; i < 4U; i++) {
		dec->components[i].downscale = 1U;
		dec->components[i].colStep = 2U;
		dec->components[i].shiftH = 1U;
		dec->components[i].shiftV = 1U;
		mcuBytes += ((uint32_t)dec->components[i].blocksPerMcu << 6) / downscale;
	}

	ret = sceJpegDecSetupOutput(dec, NULL);
	if (ret < 0)
		return ret;

	{
		uint32_t padCol = (dec->flags != 0U && (dec->mcuCols & 7U) == 7U) ? 1U : 0U;
		uint64_t total = (uint64_t)mcuBytes * (uint64_t)dec->mcuRows * (uint64_t)(dec->mcuCols + padCol);

		if (total >= 0x80000000ULL)
			return PSPEMU_JPEG_ERR_UNSUP_IMAGE_SIZE;
		return (int)(dec->mcuCols * dec->mcuRows * mcuBytes);
	}
}

int sceJpegDecTerm(void *ctx)
{
	(void)ctx;
	return 0;
}

int sceJpegDecClose(void *ctx, void *outBuf, int startX, int startY, int endX, int endY)
{
	PspJpegDecoder *dec = alignDecoderPtr(ctx);
	int ret;

	if (outBuf == NULL)
		return PSPEMU_JPEG_ERR_INVALID_POINTER;
	if (dec->curMcuRow != 0)
		return PSPEMU_JPEG_ERR_INVALID_STATE;

	if ((startX | startY | endX | endY) != 0) {
		if ((startX | startY) < 0 ||
		    endX > (int)dec->mcuCols ||
		    endY > (int)dec->mcuRows ||
		    startY >= endY ||
		    startX >= endX)
			return PSPEMU_JPEG_ERR_INVALID_REGION;

		dec->regionStartX = (uint16_t)startX;
		dec->regionStartY = (uint16_t)startY;
		dec->regionWidthMcu = (uint16_t)(endX - startX);
		dec->regionEndY = (uint16_t)endY;
	}

	ret = sceJpegDecSetupOutput(dec, (uint8_t *)outBuf);
	return (ret < 0) ? ret : 0;
}

int sceJpegDecFinish(void)
{
	return 0x2000;
}

int sceJpegDecGetOutputInfo(const void *srcBuf, uint32_t srcSize, void *outYuv, uint32_t outSize, int mode)
{
	uint8_t decStorage[sizeof(PspJpegDecoder) + 128];
	PspJpegDecoder *dec = alignDecoderPtr(decStorage);
	int ret;

	ret = sceJpegDecInit(dec, srcBuf, srcSize, 1U, mode);
	if (ret < 0)
		return ret;
	if ((uint32_t)ret > outSize)
		return PSPEMU_JPEG_ERR_MEMORY_SIZE;

	ret = sceJpegDecSetupOutput(dec, (uint8_t *)outYuv);
	if (ret != 0)
		return ret;

	return (int)(((uint32_t)dec->regionWidthMcu << (((dec->colorSpaceCode >> 2) & 3U) + 18U)) +
	             ((uint32_t)(dec->regionEndY - dec->regionStartY) << ((dec->colorSpaceCode & 3U) + 2U)));
}

int sceJpegDecReset(void *ctx)
{
	PspJpegDecoder *dec = alignDecoderPtr(ctx);
	return (int)dec->colorSpaceCode;
}

int sceJpegDecDelete(void *ctx)
{
	const PspJpegDecoder *dec = (const PspJpegDecoder *)ctx;
	return (int)(((uint32_t)dec->regionWidthMcu << (((dec->colorSpaceCode >> 2) & 3U) + 18U)) +
	             ((uint32_t)(dec->regionEndY - dec->regionStartY) << ((dec->colorSpaceCode & 3U) + 2U)));
}

static int sceJpegDecParseDht(PspJpegDecoder *dec, int markerLen)
{
	const uint8_t *ptr = dec->streamCur;
	int rem = markerLen;

	while (rem > 0) {
		uint8_t tableInfo = *ptr++;
		unsigned int tableId = tableInfo & 0x0FU;
		int isAc = (tableInfo & 0x10U) != 0U;
		PspJpegHuffTable *tbl;
		uint8_t cumCounts[17];
		int totalSyms = 0;
		int fastFilled = 0;
		int i;

		if (tableId > 3U)
			return PSPEMU_JPEG_ERR_BAD_DHT_INDEX;

		tbl = &dec->huffTables[tableId][isAc ? 1 : 0];
		cumCounts[0] = 0U;
		for (i = 1; i <= 16; i++) {
			totalSyms += *ptr++;
			cumCounts[i] = (uint8_t)totalSyms;
		}

		if (totalSyms > 255)
			return PSPEMU_JPEG_ERR_BAD_DHT_COUNTS;

		if (totalSyms != 0) {
			int curCode = 0;
			int codeLen = 1;
			int slowLen = 8;
			int baseFastCount = cumCounts[8];
			int slowPos = baseFastCount + 246;
			int symIdx;

			for (symIdx = 0; symIdx < totalSyms; symIdx++) {
				uint8_t sym = *ptr++;
				int cat = sym & 0x0F;
				int storedSym = sym;

				while (cumCounts[codeLen] <= symIdx)
					codeLen++;

				if (cat == 0) {
					storedSym = (uint8_t)(-(int)sym);
					if ((unsigned int)storedSym >= 32U)
						storedSym = 32;
				}

				if (codeLen <= 8) {
					int endCode = curCode + (256 >> codeLen);
					if (endCode > 256)
						return PSPEMU_JPEG_ERR_BAD_DHT_COUNTS;

					if (cat != 0 && codeLen + cat <= 8) {
						int8_t bitsEntry = (int8_t)(codeLen + cat);
						unsigned int shift = (unsigned int)(-cat) & 31U;

						if (isAc)
							bitsEntry = (int8_t)((storedSym >> 4) - 16 * (codeLen + cat));
						while (curCode < endCode) {
							uint32_t codeShifted = (uint32_t)curCode << (codeLen + 24);
							uint32_t signAdj = (uint32_t)((int32_t)(~codeShifted) >> 31) >> shift;
							tbl->fastVal[curCode] = (int8_t)((codeShifted >> shift) - signAdj);
							tbl->fastBits[curCode] = bitsEntry;
							curCode++;
						}
					} else {
						int8_t bitsEntry = isAc ? (int8_t)codeLen : (int8_t)(-codeLen);
						int8_t valEntry = isAc ? (int8_t)storedSym : (int8_t)(-cat);

						while (curCode < endCode) {
							tbl->fastVal[curCode] = valEntry;
							tbl->fastBits[curCode] = bitsEntry;
							curCode++;
						}
					}
					fastFilled = curCode;
				} else {
					if (!isAc)
						storedSym = -cat;
					tbl->slowSym[slowPos] = (uint8_t)storedSym;

					if (codeLen > slowLen) {
						if (slowLen == 8) {
							int slot;
							for (slot = 6; slot > 15 - codeLen; slot--)
								tbl->maxCode[slot] = 0U;
						} else {
							int slot;
							uint16_t maxVal = (uint16_t)(curCode << (16 - slowLen));
							for (slot = 15 - slowLen; slot > 15 - codeLen; slot--)
								tbl->maxCode[slot] = maxVal;
							tbl->valOffset[16 - slowLen] = (uint16_t)(baseFastCount - 1 + curCode - symIdx);
						}
						curCode <<= (codeLen - slowLen);
						slowLen = codeLen;
					}
					curCode++;
				}
				slowPos--;
			}

			if (slowLen != 8) {
				int8_t slowSentinel = isAc ? 40 : 0;
				uint16_t maxVal;
				int slot;

				while (fastFilled <= 255)
					tbl->fastBits[fastFilled++] = slowSentinel;
				if (curCode - totalSyms < 0)
					return PSPEMU_JPEG_ERR_BAD_DHT_COUNTS;
				tbl->valOffset[16 - codeLen] = (uint16_t)(baseFastCount - 1 + curCode - totalSyms);
				maxVal = (uint16_t)(curCode << (16 - slowLen));
				for (slot = 15 - slowLen; slot >= 0; slot--)
					tbl->maxCode[slot] = maxVal;
			} else {
				while (fastFilled <= 255) {
					tbl->fastBits[fastFilled] = tbl->fastBits[fastFilled - 1];
					tbl->fastVal[fastFilled] = tbl->fastVal[fastFilled - 1];
					fastFilled++;
				}
			}
		} else {
			while (fastFilled <= 255) {
				tbl->fastBits[fastFilled] = tbl->fastBits[fastFilled - 1];
				tbl->fastVal[fastFilled] = tbl->fastVal[fastFilled - 1];
				fastFilled++;
			}
		}

		tbl->valid = 1U;
		rem -= 17 + totalSyms;
	}

	return (rem == 0) ? 0 : PSPEMU_JPEG_ERR_BAD_DHT_MARKER;
}

static int sceJpegDecInitBitStream(PspJpegDecoder *dec, const uint8_t *ptr)
{
	const uint8_t *end = dec->streamEnd;
	uint32_t bits = 0U;
	int shift = 24;
	int lastVal = 0;

	dec->components[0].dcPred = 0;
	dec->components[1].dcPred = 0;
	dec->components[2].dcPred = 0;
	dec->components[3].dcPred = 0;

	do {
		uint8_t byteVal;
		if (ptr >= end)
			break;
		byteVal = *ptr;
		if (byteVal == 0xFFU) {
			lastVal = ptr[1];
			if (lastVal != 0)
				ptr -= 2;
			ptr++;
		}
		ptr++;
		bits |= (uint32_t)byteVal << shift;
		shift -= 8;
	} while (shift != -8);

	dec->streamCur = ptr;
	dec->bitBuf = bits;
	return lastVal;
}

static inline int jpegDecodeSlowSymbol(const PspJpegHuffTable *tbl, uint32_t firstByte,
                                       uint32_t *bitBuf, int *bitCount,
                                       const uint8_t **streamCur, const uint8_t *streamEnd,
                                       int *outRemBits)
{
	uint32_t code16;
	int shift;
	int symIdx;

	*bitBuf <<= 8;
	*bitCount += 8;
	if (*bitCount >= 0) {
		*bitCount -= 24;
		*bitBuf = jpegRor32(jpegRefillBytes(streamCur, streamEnd, jpegRor32(*bitBuf, *bitCount)),
		                    -*bitCount);
	}

	code16 = (firstByte << 8) | (*bitBuf >> 24);
	shift = (code16 < tbl->maxCode[6]) ? 7 : 6;
	if (code16 >= tbl->maxCode[5]) {
		shift--;
		if (code16 >= tbl->maxCode[4]) {
			shift--;
			if (code16 >= tbl->maxCode[3]) {
				shift--;
				if (code16 >= tbl->maxCode[2]) {
					shift--;
					if (code16 >= tbl->maxCode[1])
						shift += (code16 < tbl->maxCode[0]) ? -1 : -2;
				}
			}
		}
	}

	symIdx = (int)tbl->valOffset[shift] + 247 - (int)(code16 >> (shift & 31));
	*outRemBits = 8 - shift;
	return (int)(int8_t)tbl->slowSym[symIdx];
}

static int sceJpegDecDecodeMcuRow(PspJpegDecoder *dec)
{
	int16_t coef[64];
	int bitCount = dec->bitCount;
	uint32_t bitBuf = dec->bitBuf;
	int mcuCol = -(int)dec->regionStartX;
	uint32_t activeCols = (dec->curMcuRow < (int16_t)dec->regionStartY) ? 0U : (uint32_t)dec->regionWidthMcu;
	const uint8_t *streamEnd = dec->streamEnd;
	int i;

	for (i = 0; i < 64; i++)
		coef[i] = 0;

	for (;;) {
		int blockIdx = 0;
		PspJpegComponent *comp = dec->mcuCompPtrs[0];

		while (comp != NULL) {
			const uint8_t *streamCur = dec->streamCur;
			const PspJpegHuffTable *dcTbl = comp->dcTable;
			const int16_t *quant = comp->quantTable;
			uint32_t top8 = bitBuf >> 24;
			int fastBits = dcTbl->fastBits[top8];
			int fastVal = dcTbl->fastVal[top8];
			int16_t dcVal = comp->dcPred;
			int maxOffset = 0;

			/* Decode DC coefficient */
			if (fastBits > 0) {
				dcVal = (int16_t)(dcVal + quant[1] * fastVal);
				bitCount += fastBits;
				bitBuf <<= fastBits;
				comp->dcPred = dcVal;
				if (bitCount >= 0) {
					bitCount -= 24;
					bitBuf = jpegRor32(jpegRefillBytes(&streamCur, streamEnd, jpegRor32(bitBuf, bitCount)),
					                   -bitCount);
				}
			} else {
				int cat;
				if (fastBits != 0) {
					bitCount -= fastBits;
					bitBuf = jpegRor32(bitBuf, fastBits);
					cat = fastVal;
				} else {
					int remBits;
					cat = jpegDecodeSlowSymbol(dcTbl, top8, &bitBuf, &bitCount,
					                           &streamCur, streamEnd, &remBits);
					bitCount += remBits;
					bitBuf = jpegRor32(bitBuf, -remBits);
				}
				if (bitCount >= 0) {
					bitCount -= 24;
					bitBuf = jpegRor32(jpegRefillBytes(&streamCur, streamEnd, jpegRor32(bitBuf, bitCount)),
					                   -bitCount);
				}
				if (cat != 0) {
					unsigned int shift = (unsigned int)cat & 31U;
					uint32_t signMask = (bitBuf >> 31) - 1U;
					uint32_t raw = bitBuf >> shift;

					bitCount -= cat;
					bitBuf = jpegRor32(bitBuf, (int)shift);
					if (bitCount > 0) {
						bitCount -= 24;
						bitBuf = jpegRor32(jpegRefillBytes(&streamCur, streamEnd, jpegRor32(bitBuf, bitCount)),
						                   -bitCount);
						if (bitCount >= -15)
							raw = (raw & 0xFFFFFF00U) | (bitBuf & 0xFFU);
					}
					dcVal = (int16_t)(dcVal + quant[1] * (int)(raw - (signMask >> shift)));
					comp->dcPred = dcVal;
				}
			}

			/* Decode AC coefficients */
			{
				const PspJpegHuffTable *acTbl = comp->acTable;
				int kNeg = -63;

				coef[0] = dcVal;
				coef[1] = 0;

				do {
					uint32_t acTop = bitBuf >> 24;
					int8_t entry = acTbl->fastBits[acTop];
					int val = acTbl->fastVal[acTop];
					int nextPos = kNeg + 1;

					if (entry < 0) {
						int shift = entry >> 4;
						int zzOff;

						bitCount -= shift;
						kNeg = (entry & 0x0F) + nextPos;
						bitBuf = jpegRor32(bitBuf, shift);
						zzOff = g_jpegZigzagByteOffset[kNeg + 62];
						if (maxOffset < zzOff)
							maxOffset = zzOff;
						coef[zzOff >> 1] = (int16_t)(val * quant[kNeg & 0x3F]);
						if (bitCount > 0) {
							bitCount -= 24;
							bitBuf = jpegRor32(jpegRefillBytes(&streamCur, streamEnd, jpegRor32(bitBuf, bitCount)),
							                   -bitCount);
						}
						continue;
					}

					{
						int remBits;
						if ((entry >> 4) != 0) {
							bitBuf <<= (entry & 31);
							val = jpegDecodeSlowSymbol(acTbl, acTop, &bitBuf, &bitCount,
							                           &streamCur, streamEnd, &remBits);
							bitBuf <<= (remBits & 31);
							bitCount += remBits;
						} else {
							bitBuf <<= (entry & 31);
							bitCount += entry;
						}
					}

					if (bitCount >= 0) {
						bitCount -= 24;
						bitBuf = jpegRor32(jpegRefillBytes(&streamCur, streamEnd, jpegRor32(bitBuf, bitCount)),
						                   -bitCount);
					}

					if ((val & 0x0F) != 0) {
						int cat = val & 0x0F;
						unsigned int shift = (unsigned int)(-cat) & 31U;
						uint32_t signMask = (bitBuf >> 31) - 1U;
						uint32_t raw = bitBuf >> shift;
						int zzOff;

						bitCount += cat;
						bitBuf = jpegRor32(bitBuf, (int)shift);
						if (bitCount > 0) {
							bitCount -= 24;
							bitBuf = jpegRor32(jpegRefillBytes(&streamCur, streamEnd, jpegRor32(bitBuf, bitCount)),
							                   -bitCount);
							if (bitCount >= -15)
								raw = (raw & 0xFFFFFF00U) | (bitBuf & 0xFFU);
						}
						kNeg = ((uint8_t)val >> 4) + nextPos;
						zzOff = g_jpegZigzagByteOffset[kNeg + 62];
						if (maxOffset < zzOff)
							maxOffset = zzOff;
						coef[zzOff >> 1] = (int16_t)((int)(raw - (signMask >> shift)) * quant[kNeg & 0x3F]);
					} else {
						kNeg = (val == 16) ? (kNeg + 16) : val;
					}
				} while (kNeg < 0);

				if (kNeg != 0) {
					dec->curMcuRow = -1;
					return PSPEMU_JPEG_ERR_DECODE_ERROR;
				}
			}

			dec->streamCur = streamCur;
			if ((uint32_t)mcuCol < activeCols) {
				int minClip = (dec->flags != 0U) ? 1 : 0;
				uint8_t *dst = comp->outPtr;
				int stride = comp->outStride;
				int rowsDone;

				/* Row pass of 8x8 integer IDCT */
				if (maxOffset == 0) {
					int16_t dcOnly = coef[0];
					for (i = 0; i < 8; i++)
						coef[i] = dcOnly;
					rowsDone = 1;
				} else {
					int rowIdx;
					int maxRow = (maxOffset >> 4) + 1;
					for (rowIdx = 0; rowIdx < maxRow; rowIdx++) {
						int16_t *row = &coef[rowIdx * 8];
						if ((row[1] | row[2] | row[3] | row[4] | row[5] | row[6] | row[7]) != 0) {
							int inA = row[0];
							int inB = row[1];
							int inC = row[2];
							int inD = row[3];
							int inE = row[4];
							int inF = row[5];
							int inG = row[6];
							int inH = row[7];
							int oddDiffA = inF - inD;
							int oddSumA = inD + inF;
							int oddDiffB = inB - inH;
							int oddSumB = inH + inB;
							int rotCommon = 30274 * (oddDiffA + oddDiffB);
							int oddDelta = oddSumB - oddSumA;
							int oddTotal = oddSumB + oddSumA;
							int evenDiff = inC - inG;
							int evenSum = inG + inC;
							int outOddC = oddTotal + ((42813 * oddDiffA - rotCommon) >> 14);
							int outOddB = outOddC + ((23170 * oddDelta) >> 14);
							int outOddA = outOddB + ((17734 * oddDiffB - rotCommon) >> 14);
							int evenRot = ((23170 * evenDiff) >> 14) - evenSum;
							int dcSum = inA + inE;
							int dcDiff = inA - inE;
							int stageA = dcSum + evenSum;
							int stageB = dcSum - evenSum;
							int stageC = dcDiff + evenRot;
							int stageD = dcDiff - evenRot;

							row[0] = (int16_t)(stageA - oddTotal);
							row[1] = (int16_t)(stageC + outOddC);
							row[2] = (int16_t)(stageD - outOddB);
							row[3] = (int16_t)(stageB + outOddA);
							row[4] = (int16_t)(stageB - outOddA);
							row[5] = (int16_t)(stageD + outOddB);
							row[6] = (int16_t)(stageC - outOddC);
							row[7] = (int16_t)(stageA + oddTotal);
						} else if (row[0] != 0) {
							int16_t dc = row[0];
							row[1] = dc;
							row[2] = dc;
							row[3] = dc;
							row[4] = dc;
							row[5] = dc;
							row[6] = dc;
							row[7] = dc;
						}
					}
					rowsDone = maxRow;
				}

				/* Column pass + pixel clamp + store (reversed column order) */
				{
					int colIdx;
					for (colIdx = 7; colIdx >= 0; colIdx--) {
						const int16_t *col = &coef[colIdx];
						uint32_t wordLo;
						uint32_t wordHi;

						if (maxOffset < 16) {
							uint32_t px = jpegClampSample(col[0] + 8224, minClip);
							uint32_t quad = px | (px << 8) | (px << 16) | (px << 24);
							wordLo = quad;
							wordHi = quad;
						} else if (maxOffset < 32) {
							int dc = col[0] + 8224;
							int acFirst = col[8];
							int mulA = (3259 * acFirst) >> 14;
							int mulB = (13890 * acFirst) >> 14;
							int mulC = (9281 * acFirst) >> 14;

							wordLo = jpegClampSample(dc + acFirst, minClip) |
							         (jpegClampSample(dc + mulB, minClip) << 8) |
							         (jpegClampSample(dc + mulC, minClip) << 16) |
							         (jpegClampSample(dc + mulA, minClip) << 24);
							wordHi = jpegClampSample(dc - mulA, minClip) |
							         (jpegClampSample(dc - mulC, minClip) << 8) |
							         (jpegClampSample(dc - mulB, minClip) << 16) |
							         (jpegClampSample(dc - acFirst, minClip) << 24);
						} else if (maxOffset < 48) {
							int dc = col[0] + 8224;
							int acFirst = col[8];
							int acNext = col[16];
							int scaledNext = (6786 * acNext) >> 14;
							int mulA = (3259 * acFirst) >> 14;
							int mulB = (13890 * acFirst) >> 14;
							int mulC = (9281 * acFirst) >> 14;
							int evenPlus = dc + acNext;
							int evenMinus = dc - acNext;
							int oddPlus = dc + scaledNext;
							int oddMinus = dc - scaledNext;

							wordLo = jpegClampSample(oddPlus + acFirst, minClip) |
							         (jpegClampSample(evenPlus + mulB, minClip) << 8) |
							         (jpegClampSample(evenMinus + mulC, minClip) << 16) |
							         (jpegClampSample(oddMinus + mulA, minClip) << 24);
							wordHi = jpegClampSample(oddMinus - mulA, minClip) |
							         (jpegClampSample(evenMinus - mulC, minClip) << 8) |
							         (jpegClampSample(evenPlus - mulB, minClip) << 16) |
							         (jpegClampSample(oddPlus - acFirst, minClip) << 24);
						} else {
							int dc = col[0] + 8224;
							int inB = col[8];
							int inC = col[16];
							int inD = col[24];
							int inE = col[32];
							int inF = col[40];
							int inG = col[48];
							int inH = col[56];
							int oddDiffA = inF - inD;
							int oddSumA = inD + inF;
							int oddDiffB = inB - inH;
							int oddSumB = inH + inB;
							int rotCommon = 30274 * (oddDiffA + oddDiffB);
							int oddDelta = oddSumB - oddSumA;
							int oddTotal = oddSumB + oddSumA;
							int evenDiff = inC - inG;
							int evenSum = inG + inC;
							int outOddC = oddTotal + ((42813 * oddDiffA - rotCommon) >> 14);
							int outOddB = outOddC + ((23170 * oddDelta) >> 14);
							int outOddA = outOddB + ((17734 * oddDiffB - rotCommon) >> 14);
							int evenRot = ((23170 * evenDiff) >> 14) - evenSum;
							int dcSum = dc + inE;
							int dcDiff = dc - inE;
							int stageA = dcSum + evenSum;
							int stageB = dcSum - evenSum;
							int stageC = dcDiff + evenRot;
							int stageD = dcDiff - evenRot;

							wordLo = jpegClampSample(stageA + oddTotal, minClip) |
							         (jpegClampSample(stageC - outOddC, minClip) << 8) |
							         (jpegClampSample(stageD + outOddB, minClip) << 16) |
							         (jpegClampSample(stageB - outOddA, minClip) << 24);
							wordHi = jpegClampSample(stageB + outOddA, minClip) |
							         (jpegClampSample(stageD - outOddB, minClip) << 8) |
							         (jpegClampSample(stageC + outOddC, minClip) << 16) |
							         (jpegClampSample(stageA - oddTotal, minClip) << 24);
						}

						{
							uint32_t *dstWords = (uint32_t *)dst;
							dstWords[0] = wordLo;
							dstWords[1] = wordHi;
						}
						dst += stride;
					}
				}

				for (i = 0; i < rowsDone * 8; i++)
					coef[i] = 0;
				comp->outPtr += dec->mcuBlockSteps[blockIdx];
			}

			blockIdx++;
			comp = dec->mcuCompPtrs[blockIdx];
		}

		if (dec->restartCountdown != 0U) {
			dec->restartCountdown--;
			if (dec->restartCountdown == 0U) {
				const uint8_t *scanPtr = dec->streamCur;
				const uint8_t *limit = scanPtr + 1536;
				int foundRestart = 0;

				while (scanPtr + 1 < streamEnd) {
					if (*scanPtr == 0xFFU) {
						uint8_t marker = scanPtr[1];
						if (marker == dec->nextRestartMarker) {
							dec->nextRestartMarker = (uint8_t)((marker + 1U) & 0xF7U);
							bitCount = -24;
							dec->restartCountdown = dec->restartInterval;
							sceJpegDecInitBitStream(dec, scanPtr + 2);
							bitBuf = dec->bitBuf;
							foundRestart = 1;
							break;
						}
						if (marker == 0xD9U) {
							dec->streamCur = scanPtr;
							return 0xD9;
						}
						break;
					}
					scanPtr++;
					if (scanPtr == limit)
						break;
				}
				if (!foundRestart) {
					dec->curMcuRow = -1;
					return PSPEMU_JPEG_ERR_BAD_RESTART;
				}
			}
		}

		mcuCol++;
		if (mcuCol >= (int)dec->mcuCols - (int)dec->regionStartX) {
			dec->bitBuf = bitBuf;
			dec->bitCount = (int8_t)bitCount;
			return 0;
		}
	}
}

static int sceJpegDecParseSos(PspJpegDecoder *dec)
{
	const uint8_t *ptr = dec->streamCur;
	const uint8_t *end = dec->streamEnd;
	uint32_t segLen = ((uint32_t)ptr[0] << 8) | (uint32_t)ptr[1];
	uint32_t compCount = ptr[2];
	const uint8_t *compSpec = &ptr[3];
	int totalBlocks = 0;
	uint32_t i;

	if (ptr + segLen >= end || 2U * compCount + 3U != segLen - 3U || (compCount - 1U) > 2U)
		return PSPEMU_JPEG_ERR_BAD_SOS_MARKER;

	dec->scanStart = 0U;
	dec->scanEnd = 63U;

	for (i = 0U; i < compCount; i++) {
		uint8_t compId = compSpec[0];
		uint8_t huffSel = compSpec[1];
		int found = -1;
		int c;

		for (c = 0; c < 4; c++) {
			if (dec->components[c].id == compId) {
				found = c;
				break;
			}
		}
		if (found < 0)
			return PSPEMU_JPEG_ERR_BAD_COMP_ID;

		dec->components[found].dcTable = &dec->huffTables[huffSel >> 4][0];
		dec->components[found].acTable = &dec->huffTables[huffSel & 0x0FU][1];
		if (dec->components[found].dcTable->valid == 0U ||
		    dec->components[found].acTable->valid == 0U)
			return PSPEMU_JPEG_ERR_NO_HUFF_TABLE;

		{
			uint32_t blk;
			for (blk = 0U; blk < dec->components[found].blocksPerMcu; blk++)
				dec->mcuCompPtrs[totalBlocks++] = &dec->components[found];
		}
		compSpec += 2;
	}

	if (totalBlocks > 24)
		return PSPEMU_JPEG_ERR_BAD_MCU_SIZE;

	dec->mcuCompPtrs[totalBlocks] = NULL;
	sceJpegDecInitBitStream(dec, compSpec + 3);
	dec->bitCount = -24;
	return 0;
}

static int sceJpegDecParseMarkers(PspJpegDecoder *dec)
{
	const uint8_t *ptr;
	const uint8_t *end = dec->streamEnd;

	if (dec->flags == 2U) {
		const uint8_t *savedCur = dec->streamCur;
		int dhtRet;

		dec->streamCur = g_jpegDefaultMjpegDht;
		dhtRet = sceJpegDecParseDht(dec, (int)sizeof(g_jpegDefaultMjpegDht));
		dec->streamCur = savedCur;
		if (dhtRet < 0)
			return dhtRet;
	}

	ptr = dec->streamCur;
	for (;;) {
		uint8_t marker;
		const uint8_t *segStart;
		const uint8_t *segNext;
		int segBodyLen;
		int ret;

		for (;;) {
			uint8_t byteVal = *ptr++;
			if (byteVal == 0xFFU) {
				marker = *ptr;
				if (marker != 0xFFU)
					break;
			}
			if (ptr >= end)
				return PSPEMU_JPEG_ERR_BAD_MARKER_LEN;
		}

		if (marker == 0xD9U || marker == 0xDAU) {
			dec->streamCur = ptr + 1;
			return (int)marker;
		}

		segStart = ptr + 3;
		segBodyLen = (int)(((uint32_t)ptr[1] << 8) | (uint32_t)ptr[2]) - 2;
		if (segBodyLen < 0)
			return PSPEMU_JPEG_ERR_BAD_MARKER_LEN;
		segNext = segStart + segBodyLen;
		if (segNext >= end)
			return PSPEMU_JPEG_ERR_BAD_MARKER_LEN;

		dec->streamCur = segStart;

		if (marker == 0xC9U || marker == 0xCCU)
			return PSPEMU_JPEG_ERR_NO_ARITH;
		if (marker == 0xC2U) {
			dec->isProgressive = 1U;
			return PSPEMU_JPEG_ERR_UNKNOWN_MARKER;
		}
		if (marker == 0xC3U)
			return PSPEMU_JPEG_ERR_NO_LOSSLESS;
		if (marker == 0x01U || (marker >= 0xC5U && marker <= 0xD7U))
			return PSPEMU_JPEG_ERR_UNKNOWN_MARKER;

		if (marker == 0xC0U || marker == 0xC1U) {
			if (segStart[0] != 8U) {
				ret = PSPEMU_JPEG_ERR_BAD_PRECISION;
			} else if ((dec->colorSpaceCode & 0x0FU) != 0U) {
				ret = PSPEMU_JPEG_ERR_SOF_DUPLICATE;
			} else {
				uint16_t imgH = (uint16_t)(((uint32_t)segStart[1] << 8) | (uint32_t)segStart[2]);
				uint16_t imgW = (uint16_t)(((uint32_t)segStart[3] << 8) | (uint32_t)segStart[4]);
				uint32_t numComp = segStart[5];

				dec->imageHeight = imgH;
				dec->imageWidth = imgW;
				if (imgH == 0U || imgW == 0U) {
					ret = PSPEMU_JPEG_ERR_IMAGE_EMPTY;
				} else if (numComp - 1U > 2U) {
					ret = PSPEMU_JPEG_ERR_COMP_COUNT;
				} else if (segBodyLen != (int)(3U * numComp + 6U)) {
					ret = PSPEMU_JPEG_ERR_BAD_SOF_MARKER;
				} else {
					const uint8_t *cspec = &segStart[6];
					uint8_t csCode = dec->colorSpaceCode;
					uint32_t c;

					ret = 0;
					for (c = 0U; c < numComp; c++) {
						uint8_t samp = cspec[1];
						uint8_t qIdx = cspec[2];
						uint32_t blocks;

						dec->components[c].id = cspec[0];
						if (c == 0U) {
							uint32_t hSamp = samp >> 4;
							uint32_t vSamp = samp & 0x0FU;

							if (hSamp == 0U || vSamp == 0U ||
							    hSamp == 3U || vSamp == 3U ||
							    hSamp > 4U || vSamp > 4U) {
								ret = PSPEMU_JPEG_ERR_UNSUP_SAMPLING;
								break;
							}
							dec->mcuCols = (uint16_t)((imgW + 8U * hSamp - 1U) >> ((hSamp >> 1) + 3U));
							dec->mcuRows = (uint16_t)((imgH + 8U * vSamp - 1U) >> ((vSamp >> 1) + 3U));
							csCode = (uint8_t)(csCode + ((vSamp >> 1) + 4U * (hSamp >> 1) + 5U));
							blocks = hSamp * vSamp;
						} else {
							if (samp != 0x11U) {
								ret = PSPEMU_JPEG_ERR_UNSUP_SAMPLING;
								break;
							}
							blocks = 1U;
						}

						if (qIdx > 3U) {
							ret = PSPEMU_JPEG_ERR_NO_QUANT_TABLE;
							break;
						}
						dec->components[c].quantTable = dec->quantTables[qIdx];
						dec->components[c].blocksPerMcu = (uint8_t)blocks;
						cspec += 3;
					}

					if (ret == 0) {
						for (; c < 4U; c++)
							dec->components[c].id = dec->components[c - 1U].id;

						if (numComp == 3U) {
							if ((csCode & 0xF0U) == 0U) {
								uint32_t rgbSig = ((uint32_t)dec->components[0].id << 16) |
								                  ((uint32_t)dec->components[1].id << 8) |
								                  (uint32_t)dec->components[2].id;
								csCode |= (rgbSig == 0x524742U) ? 0x50U : 0x20U;
							}
						} else if (numComp == 1U) {
							if ((csCode & 0x0FU) != 5U)
								ret = PSPEMU_JPEG_ERR_UNSUP_SAMPLING;
							else
								csCode = 0x15U;
						} else {
							csCode &= 0x0FU;
						}

						if (ret == 0)
							dec->colorSpaceCode = csCode;
					}
				}
			}
			dec->streamCur = segNext;
			if (ret < 0)
				return ret;
		} else if (marker == 0xC4U) {
			ret = sceJpegDecParseDht(dec, segBodyLen);
			dec->streamCur = segNext;
			if (ret < 0)
				return ret;
		} else if (marker == 0xDBU) {
			int rem = segBodyLen;
			const uint8_t *qPtr = segStart;

			ret = 0;
			while (rem > 0) {
				uint8_t qInfo = *qPtr++;
				int is16Bit = (qInfo & 0x10U) != 0U;
				uint32_t qId = qInfo & 0x0FU;
				int elemBytes = is16Bit ? 64 : 0;
				int j;

				if (qId > 3U) {
					ret = PSPEMU_JPEG_ERR_BAD_DQT_INDEX;
					break;
				}
				for (j = 0; j < 64; j++) {
					uint32_t qVal = *qPtr++;
					if (is16Bit)
						qVal = (qVal << 8) | (uint32_t)(*qPtr++);
					dec->quantTables[qId][(j + 1) & 0x3F] =
						(int16_t)(((uint32_t)g_jpegQuantScaleTable[j] * qVal + 2048U) >> 12);
				}
				rem -= 65 + elemBytes;
			}
			if (ret == 0 && rem != 0)
				ret = PSPEMU_JPEG_ERR_BAD_DQT_MARKER;
			dec->streamCur = segNext;
			if (ret < 0)
				return ret;
		} else if (marker == 0xDDU) {
			if (segBodyLen != 2) {
				dec->streamCur = segNext;
				return PSPEMU_JPEG_ERR_BAD_DRI_MARKER;
			}
			dec->nextRestartMarker = 0xD0U;
			dec->restartInterval = (uint16_t)(((uint32_t)segStart[0] << 8) | (uint32_t)segStart[1]);
			dec->restartCountdown = dec->restartInterval;
			dec->streamCur = segNext;
		} else if (marker == 0xE0U || marker == 0xE1U) {
			if ((dec->colorSpaceCode & 0xF0U) == 0U)
				dec->colorSpaceCode = 0x20U;
			dec->streamCur = segNext;
		} else {
			dec->streamCur = segNext;
		}

		ptr = segNext;
	}
}
