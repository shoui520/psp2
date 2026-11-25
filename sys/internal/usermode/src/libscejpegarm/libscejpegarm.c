/*
 * SceJpegArm (ARM/NEON software JPEG decoder): public API, header/marker
 * parsing, output layout, and split-decoding state machine.
 */

#include "jpegarm_internal.h"

const uint8_t g_jpegArmZigzagByteOffset[63] = {
	  2,  16,  32,  18,   4,   6,  20,  34,
	 48,  64,  50,  36,  22,   8,  10,  24,
	 38,  52,  66,  80,  96,  82,  68,  54,
	 40,  26,  12,  14,  28,  42,  56,  70,
	 84,  98, 112, 114, 100,  86,  72,  58,
	 44,  30,  46,  60,  74,  88, 102, 116,
	118, 104,  90,  76,  62,  78,  92, 106,
	120, 122, 108,  94, 110, 124, 126
};

const uint8_t g_jpegArmDefaultDht[416] = {
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
	0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA
};

static inline JpegArmDecoder *alignDecoderCtx(void *ctxBuf)
{
	uintptr_t addr = ((uintptr_t)ctxBuf + 127U) & ~(uintptr_t)127U;
	return (JpegArmDecoder *)addr;
}

static inline int decodeColorSpacePublic(uint32_t code)
{
	uint32_t v = code & 3U;
	if (v == 0U)
		return 0;
	if ((code & 0x0CU) == 0U || (code & 0xF0U) == 0U)
		return (int)(code & 0xF0U);
	uint32_t h = (code >> 2) & 3U;
	return (int)(((code >> 4) << 16) | ((1U << (h - 1U)) << 8) | (1U << (v - 1U)));
}

static inline uint8_t makeDecodeFlags(int decodeMode, int isRgba)
{
	uint8_t flags = 0;
	if ((uint32_t)(decodeMode & 3) <= 1U)
		flags |= JPEGARM_FLAG_JFIF_ONLY;
	if ((uint32_t)((decodeMode & 3) - 1) <= 1U)
		flags |= JPEGARM_FLAG_DEFAULT_DHT;
	if (isRgba) {
		flags |= JPEGARM_FLAG_OUTPUT_RGBA;
		if ((decodeMode & 4) != 0)
			flags |= JPEGARM_FLAG_EXACT_SIZE;
	}
	return flags;
}

static inline int extractDownscale(int decodeMode, int *outDownscale)
{
	int bits = decodeMode & 0x70;
	if (bits == 0) {
		*outDownscale = 1;
		return 0;
	}
	int scale = bits >> 3;
	if (((scale - 1) & scale) != 0 || (uint32_t)(scale - 1) > 7U)
		return SCE_JPEGARM_ERROR_INVALID_DECODE_MODE;
	*outDownscale = scale;
	return 0;
}

static inline void resetDecoderTables(JpegArmDecoder *dec)
{
	dec->isProgressive = 0;
	dec->restartInterval = 0;
	dec->restartCountdown = 0;
	dec->coefBufBase = NULL;
	for (int i = 0; i < 4; i++) {
		dec->huffTables[i][0].valid = 0;
		dec->huffTables[i][1].valid = 0;
		dec->components[i].blocksPerMcu = 0;
	}
}

static inline int findSoiMarker(const uint8_t *buf, SceSize size, SceSize *offsetOut)
{
	if (size <= 1U)
		return SCE_JPEGARM_ERROR_NO_SOI;

	SceSize pos = 0;
	while (buf[pos + 1] != 0xD8) {
		pos += (buf[pos + 1] == 0xFF) ? 2U : 1U;
		if (pos >= size - 1U)
			return SCE_JPEGARM_ERROR_NO_SOI;
	}
	if (buf[pos] != 0xFF || pos + 2U == size)
		return SCE_JPEGARM_ERROR_NO_SOI;

	*offsetOut = pos + 2U;
	return 0;
}

static inline void setupMcuBlockSteps(JpegArmDecoder *dec, uint32_t yStride, int blockSize, uint32_t *numYBlocksOut)
{
	uint32_t numY = dec->components[0].blocksPerMcu;
	switch (dec->colorSpaceCode & 0x0F) {
	case 5:
	case 9:
	case 13:
		for (uint32_t i = 0; i < numY; i++)
			dec->mcuBlockSteps[i] = blockSize;
		break;
	case 10:
		dec->mcuBlockSteps[0] = blockSize;
		dec->mcuBlockSteps[1] = (int32_t)(blockSize * yStride) - blockSize;
		dec->mcuBlockSteps[2] = blockSize;
		dec->mcuBlockSteps[3] = blockSize - (int32_t)(blockSize * yStride);
		break;
	default:
		if (numY > 1U) {
			int32_t rowBytes = (int32_t)(blockSize * yStride);
			for (uint32_t i = 0; i < numY - 1U; i++)
				dec->mcuBlockSteps[i] = rowBytes;
			dec->mcuBlockSteps[numY - 1U] = blockSize - rowBytes * (int32_t)(numY - 1U);
		} else {
			dec->mcuBlockSteps[0] = blockSize;
		}
		break;
	}
	*numYBlocksOut = numY;
}

/* =========================================================================
 * Public exports (0x81000000 .. 0x8100028c)
 * ========================================================================= */

int sceJpegArmDecodeMJpegYCbCr(const SceUInt8 *pJpeg, SceSize isize, SceUInt8 *pYCbCr, SceSize osize,
	int decodeMode, void *pCoefBuffer, SceSize coefBufferSize)
{
	if (pYCbCr == NULL || pJpeg == NULL || (((uintptr_t)pYCbCr | (uintptr_t)pCoefBuffer) & 7U) != 0U)
		return SCE_JPEGARM_ERROR_INVALID_POINTER;
	if (osize == 0U || isize == 0U)
		return SCE_JPEGARM_ERROR_MEMORY_SIZE;
	if ((decodeMode & 4) != 0)
		return SCE_JPEGARM_ERROR_INVALID_COLOR_FORMAT;

	int downscale;
	int err = extractDownscale(decodeMode, &downscale);
	if (err < 0)
		return err;

	uint8_t flags = makeDecodeFlags(decodeMode, 0);
	return jpegArmDecodeFullFrame(pJpeg, isize, pYCbCr, osize, flags, downscale,
		NULL, pCoefBuffer, coefBufferSize);
}

int sceJpegArmDecodeMJpeg(const SceUInt8 *pJpeg, SceSize isize, void *pRGBA, SceSize osize,
	int decodeMode, void *pCoefBuffer, SceSize coefBufferSize)
{
	if (pRGBA == NULL || pJpeg == NULL ||
		(((uintptr_t)pRGBA & 3U) != 0U) || (((uintptr_t)pCoefBuffer & 7U) != 0U))
		return SCE_JPEGARM_ERROR_INVALID_POINTER;
	if (osize == 0U || isize == 0U)
		return SCE_JPEGARM_ERROR_MEMORY_SIZE;

	int downscale;
	int err = extractDownscale(decodeMode, &downscale);
	if (err < 0)
		return err;

	uint8_t flags = makeDecodeFlags(decodeMode, 1);
	return jpegArmDecodeFullFrame(pJpeg, isize, (uint8_t *)pRGBA, osize, flags, downscale,
		NULL, pCoefBuffer, coefBufferSize);
}

int sceJpegArmGetOutputInfo(const SceUInt8 *pJpeg, SceSize isize, int outputFormat, int decodeMode,
	SceJpegOutputInfo *pOutputInfo)
{
	if (pOutputInfo == NULL || pJpeg == NULL)
		return SCE_JPEGARM_ERROR_INVALID_POINTER;
	if (isize == 0U)
		return SCE_JPEGARM_ERROR_MEMORY_SIZE;
	if ((uint32_t)(outputFormat + 1) > 1U)
		return SCE_JPEGARM_ERROR_INVALID_COLOR_FORMAT;
	if ((decodeMode & 4) != 0 && outputFormat != 0)
		return SCE_JPEGARM_ERROR_INVALID_COLOR_FORMAT;

	int downscale;
	int err = extractDownscale(decodeMode, &downscale);
	if (err < 0)
		return err;

	uint8_t flags = makeDecodeFlags(decodeMode, outputFormat == 0);
	int ret = jpegArmGetOutputInfoInternal(pJpeg, isize, flags, downscale, outputFormat, pOutputInfo);
	pOutputInfo->colorSpace = decodeColorSpacePublic((uint32_t)pOutputInfo->colorSpace);
	return ret;
}

int sceJpegArmCreateSplitDecoder(SceJpegArmSplitDecodeCtrl *pCtrl)
{
	if (pCtrl == NULL || ((uintptr_t)pCtrl & 3U) != 0U)
		return SCE_JPEGARM_ERROR_INVALID_POINTER;

	sceClibMemset(pCtrl, 0, sizeof(*pCtrl));
	pCtrl->state = 0;
	pCtrl->decoderCtx = (void *)(((uintptr_t)pCtrl + 255U) & ~(uintptr_t)127U);
	pCtrl->selfPtr = pCtrl;
	return 0;
}

int sceJpegArmDeleteSplitDecoder(SceJpegArmSplitDecodeCtrl *pCtrl)
{
	if (pCtrl == NULL || ((uintptr_t)pCtrl & 3U) != 0U || pCtrl->selfPtr != pCtrl)
		return SCE_JPEGARM_ERROR_INVALID_POINTER;

	pCtrl->state = 0;
	pCtrl->decoderCtx = NULL;
	pCtrl->selfPtr = NULL;
	return 0;
}

int sceJpegArmSplitDecodeMJpeg(SceJpegArmSplitDecodeCtrl *pCtrl)
{
	if (pCtrl == NULL || ((uintptr_t)pCtrl & 3U) != 0U || pCtrl->selfPtr != pCtrl)
		return SCE_JPEGARM_ERROR_INVALID_POINTER;

	uint32_t state = pCtrl->state;
	if (state > 4U)
		return SCE_JPEGARM_ERROR_INVALID_STATE;

	int outputFormat = pCtrl->outputFormat;
	uint8_t *streamBuf = pCtrl->pStreamBuffer;
	JpegArmDecoder *dec = (JpegArmDecoder *)pCtrl->decoderCtx;
	uintptr_t outAlignMask = (outputFormat < 0) ? 0U : 3U;

	if (streamBuf == NULL ||
		(((uintptr_t)pCtrl->pOutputBuffer & outAlignMask) != 0U) ||
		(((uintptr_t)pCtrl->pCoefBuffer & 7U) != 0U))
		return SCE_JPEGARM_ERROR_INVALID_POINTER;

	if (pCtrl->streamBufferSize <= 255U)
		return SCE_JPEGARM_ERROR_MEMORY_SIZE;

	if (pCtrl->pWriteBuffer != NULL) {
		uintptr_t base = (uintptr_t)streamBuf;
		uintptr_t end = base + pCtrl->streamBufferSize;
		uintptr_t wr = (uintptr_t)pCtrl->pWriteBuffer;
		if (wr < base || wr > end || wr + pCtrl->writeBufferSize > end)
			return SCE_JPEGARM_ERROR_INVALID_POINTER;
	}

	int ret;
	SceSize coefSize = 0;

	if (state <= 1U) {
		int hdrRet;
		if (state == 0U) {
			if ((uint32_t)pCtrl->splitMode > 1U)
				return SCE_JPEGARM_ERROR_INVALID_INIT_PARAM;
			if ((pCtrl->decodeMode & 4) != 0 && outputFormat != 0)
				return SCE_JPEGARM_ERROR_INVALID_COLOR_FORMAT;

			int downscale;
			ret = extractDownscale(pCtrl->decodeMode, &downscale);
			if (ret < 0)
				return ret;

			uint8_t flags = makeDecodeFlags(pCtrl->decodeMode, outputFormat == 0);
			hdrRet = jpegArmInitAndParseHeader(dec, streamBuf, pCtrl->streamBufferSize,
				downscale, flags, 1, &coefSize);
		} else {
			if (streamBuf != dec->streamBase) {
				pCtrl->state = 5;
				return SCE_JPEGARM_ERROR_INVALID_POINTER;
			}
			SceSize curSpan = (SceSize)(dec->streamEnd - dec->streamBase);
			if (dec->splitState > 7U ? (pCtrl->streamBufferSize != curSpan) : (pCtrl->streamBufferSize < curSpan)) {
				pCtrl->state = 5;
				return SCE_JPEGARM_ERROR_INVALID_POINTER;
			}

			ret = jpegArmCommitSplitInput(dec, pCtrl->writeBufferSize, pCtrl->isEndOfStream);
			if (ret < 0) {
				pCtrl->state = 5;
				return ret;
			}
			hdrRet = jpegArmParseHeaderAndSos(dec, &coefSize);
		}

		if (hdrRet < 0) {
			if (hdrRet == SCE_JPEGARM_ERROR_INPUT_SUSPENDED) {
				ret = jpegArmGetSplitWriteBuffer(dec, &pCtrl->pWriteBuffer, &pCtrl->writeBufferSize);
				if (ret < 0) {
					pCtrl->state = 5;
					return ret;
				}
				pCtrl->state = 1;
				return SCE_JPEGARM_ERROR_INPUT_SUSPENDED;
			}
			pCtrl->outputInfo.colorSpace = dec->colorSpaceCode;
			pCtrl->state = 5;
			if (hdrRet == SCE_JPEGARM_ERROR_UNSUPPORT_SAMPLING || hdrRet == SCE_JPEGARM_ERROR_UNSUPPORT_COLORSPACE)
				pCtrl->outputInfo.colorSpace = decodeColorSpacePublic((uint32_t)pCtrl->outputInfo.colorSpace);
			return hdrRet;
		}

		jpegArmFillOutputInfo(dec, outputFormat, (SceSize)hdrRet, coefSize, &pCtrl->outputInfo);
		pCtrl->writeBufferSize = 0;
		pCtrl->regionX = 0;
		pCtrl->regionY = 0;

		if (outputFormat != 0) {
			uint16_t w = (uint16_t)pCtrl->outputInfo.pitch[0].x;
			uint16_t h = (pCtrl->splitMode != 0)
				? (uint16_t)(1U << ((dec->colorSpaceCode & 3U) + 2U - dec->components[0].downscaleShift))
				: (uint16_t)pCtrl->outputInfo.pitch[0].y;
			pCtrl->regionWidth = w;
			pCtrl->regionHeight = h;
			pCtrl->planeSize[0] = (SceSize)w * h;
			uint32_t cw = w >> (((dec->colorSpaceCode >> 2) & 3U) - 1U);
			uint32_t ch = h >> ((dec->colorSpaceCode & 3U) - 1U);
			pCtrl->planeSize[1] = cw * ch;
			pCtrl->planeSize[2] = cw * ch;
			pCtrl->planeSize[3] = 0;
		} else {
			uint16_t w = (uint16_t)(pCtrl->outputInfo.pitch[0].x >> 2);
			uint16_t h = (pCtrl->splitMode != 0)
				? (uint16_t)dec->components[0].outHeight
				: (uint16_t)pCtrl->outputInfo.pitch[0].y;
			pCtrl->regionWidth = w;
			pCtrl->regionHeight = h;
			pCtrl->planeSize[0] = (SceSize)w * h * 4U;
			pCtrl->planeSize[1] = 0;
			pCtrl->planeSize[2] = 0;
			pCtrl->planeSize[3] = 0;
		}

		pCtrl->state = 2;
		pCtrl->outputInfo.colorSpace = decodeColorSpacePublic((uint32_t)pCtrl->outputInfo.colorSpace);
		return SCE_JPEGARM_ERROR_HEADER_DONE;
	}

	/* States 2, 3, 4: decoding rows */
	if (streamBuf != dec->streamBase)
		return SCE_JPEGARM_ERROR_INVALID_POINTER;

	SceSize curSpan = (SceSize)(dec->streamEnd - dec->streamBase);
	if (dec->splitState > 7U ? (pCtrl->streamBufferSize != curSpan) : (pCtrl->streamBufferSize < curSpan))
		return SCE_JPEGARM_ERROR_INVALID_POINTER;

	uint8_t *outPlanes[4];
	outPlanes[0] = (uint8_t *)pCtrl->pOutputBuffer;
	outPlanes[1] = outPlanes[0] + pCtrl->planeSize[0];
	outPlanes[2] = outPlanes[1] + pCtrl->planeSize[1];
	outPlanes[3] = NULL;

	if (state == 2U) {
		ret = jpegArmBeginDecodeOutput(dec, outPlanes[0], 0, 0, 0, 0, pCtrl->pCoefBuffer);
		if (ret < 0) {
			pCtrl->state = 5;
			return ret;
		}
		if (pCtrl->splitMode != 0) {
			ret = jpegArmSetOutputPointers(dec, outPlanes);
			if (ret < 0) {
				pCtrl->state = 5;
				return ret;
			}
		}
	} else if (state == 4U) {
		ret = jpegArmSetOutputPointers(dec, outPlanes);
		if (ret < 0) {
			pCtrl->state = 5;
			return ret;
		}
		pCtrl->regionY = (uint16_t)(pCtrl->regionY + pCtrl->regionHeight);
	} else {
		ret = jpegArmCommitSplitInput(dec, pCtrl->writeBufferSize, pCtrl->isEndOfStream);
		if (ret < 0) {
			pCtrl->state = 5;
			return ret;
		}
	}

	int rowRet;
	if (pCtrl->splitMode == 0) {
		do {
			rowRet = jpegArmDecodeMcuRow(dec);
		} while (rowRet > 0);
	} else if (pCtrl->splitMode == 1) {
		if ((uint32_t)(dec->progPhase - 1U) <= 1U) {
			do {
				rowRet = jpegArmDecodeMcuRow(dec);
			} while (rowRet > 0 && dec->progPhase <= 2U);
		} else {
			rowRet = jpegArmDecodeMcuRow(dec);
		}
		if (rowRet > 0) {
			pCtrl->writeBufferSize = 0;
			pCtrl->state = 4;
			return 0;
		}
	} else {
		pCtrl->state = 5;
		return SCE_JPEGARM_ERROR_INVALID_INIT_PARAM;
	}

	if (rowRet == 0) {
		pCtrl->writeBufferSize = 0;
		if (pCtrl->splitMode == 1 && (dec->flags & 0x0C) == 0x0C)
			pCtrl->regionHeight = (uint16_t)dec->rgbaRowHeight;
		int dims = jpegArmGetDecodedDimensions(dec);
		pCtrl->state = 5;
		return dims;
	}

	if (rowRet == SCE_JPEGARM_ERROR_INPUT_SUSPENDED) {
		ret = jpegArmGetSplitWriteBuffer(dec, &pCtrl->pWriteBuffer, &pCtrl->writeBufferSize);
		if (ret < 0) {
			pCtrl->state = 5;
			return ret;
		}
		pCtrl->state = 3;
		return SCE_JPEGARM_ERROR_INPUT_SUSPENDED;
	}

	pCtrl->state = 5;
	return rowRet;
}

/* =========================================================================
 * Output plane & coefficient buffer setup (0x81000710 .. 0x81000ea4)
 * ========================================================================= */

int jpegArmSetupYCbCrPlanes(JpegArmDecoder *dec, uint8_t *outBuf)
{
	if (((uintptr_t)outBuf & 7U) != 0U)
		return SCE_JPEGARM_ERROR_INVALID_POINTER;

	uint32_t regionW = dec->regionWidthMcu;
	uint32_t regionH = (uint32_t)dec->regionEndY - (uint32_t)dec->regionStartY;
	uint32_t cs = dec->colorSpaceCode;
	uint8_t dsShift = dec->components[0].downscaleShift;
	uint32_t hCode = (cs >> 2) & 3U;
	uint32_t vCode = cs & 3U;
	int blockSize = 8 >> dsShift;

	uint32_t rawW = (regionW << (hCode + 2U)) >> dsShift;
	uint32_t outH = (regionH << (vCode + 2U)) >> dsShift;
	uint32_t yStride;

	if ((dec->flags & JPEGARM_FLAG_JFIF_ONLY) != 0) {
		outH = (outH + 7U) & ~7U;
		yStride = (rawW + 15U) & ~15U;
	} else {
		uint32_t align = 4U << hCode;
		yStride = (rawW + align - 1U) & ~(align - 1U);
	}

	dec->components[0].outStride = yStride;
	dec->components[0].outHeight = outH;

	uint32_t numYBlocks;
	if (dec->isProgressive != 0) {
		numYBlocks = 1;
		dec->mcuBlockSteps[0] = blockSize;
		dec->components[0].outRowStep = (int32_t)(blockSize * yStride - rawW);
	} else {
		setupMcuBlockSteps(dec, yStride, blockSize, &numYBlocks);
		dec->components[0].outRowStep = (int32_t)(yStride * (uint32_t)(blockSize << (vCode - 1U)) - rawW);
	}

	uint32_t cRawW = (uint32_t)blockSize * regionW;
	uint32_t cStride;
	uint32_t cHeight;
	if ((dec->flags & JPEGARM_FLAG_JFIF_ONLY) != 0) {
		cHeight = outH >> (vCode - 1U);
		cStride = (cRawW + 15U) & ~15U;
	} else {
		cStride = (cRawW + 7U) & ~7U;
		cHeight = (uint32_t)blockSize * regionH;
	}

	for (int i = 1; i < 4; i++) {
		dec->mcuBlockSteps[numYBlocks + (uint32_t)(i - 1)] = blockSize;
		dec->components[i].outStride = cStride;
		dec->components[i].outHeight = cHeight;
		dec->components[i].outRowStep = (int32_t)(cStride * (uint32_t)(8 >> dec->components[i].downscaleShift) - cRawW);
	}

	if (outBuf != NULL) {
		dec->components[0].outPtr = outBuf;
		uint8_t *partB = outBuf + dec->components[0].outStride * dec->components[0].outHeight;
		uint8_t *partC = partB + dec->components[1].outStride * dec->components[1].outHeight;
		uint8_t *partD = partC + dec->components[2].outStride * dec->components[2].outHeight;
		dec->components[1].outPtr = partB;
		dec->components[2].outPtr = partC;
		dec->components[3].outPtr = partD;
	}

	return 0;
}

int jpegArmSetupRgbaOutput(JpegArmDecoder *dec, uint8_t *outBuf)
{
	if (((uintptr_t)outBuf & 3U) != 0U)
		return SCE_JPEGARM_ERROR_INVALID_POINTER;

	uint32_t cs = dec->colorSpaceCode;
	uint8_t dsShift = dec->components[0].downscaleShift;
	uint32_t hCode = (cs >> 2) & 3U;
	uint32_t vCode = cs & 3U;
	int blockSize = 8 >> dsShift;
	uint32_t mcuW = (1U << (hCode + 2U)) >> dsShift;
	uint32_t mcuH = (1U << (vCode + 2U)) >> dsShift;
	uint32_t yStride = (mcuW + 7U) & ~7U;

	dec->rgbaOutPtr = outBuf;
	dec->mcuPixelWidth = mcuW;
	dec->components[0].outStride = yStride;
	dec->components[0].outHeight = mcuH;
	dec->components[0].outRowStep = 0;
	dec->hSampMask = (uint8_t)((1U << (hCode - 1U)) - 1U);
	dec->vSampMask = (uint8_t)((1U << (vCode - 1U)) - 1U);

	if ((dec->flags & JPEGARM_FLAG_EXACT_SIZE) != 0) {
		uint32_t ds = dec->components[0].downscale;
		dec->rgbaWidth = ((uint32_t)dec->imageWidth + ds - 1U) >> dsShift;
		dec->rgbaHeight = ((uint32_t)dec->imageHeight + ds - 1U) >> dsShift;
	} else {
		dec->rgbaWidth = (uint32_t)dec->regionWidthMcu * mcuW;
		dec->rgbaHeight = ((uint32_t)dec->regionEndY - (uint32_t)dec->regionStartY) * mcuH;
	}
	dec->rgbaCurY = 0;

	uint32_t numYBlocks;
	setupMcuBlockSteps(dec, yStride, blockSize, &numYBlocks);

	uint32_t cStride = ((uint32_t)blockSize + 7U) & ~7U;
	for (int i = 1; i < 4; i++) {
		dec->mcuBlockSteps[numYBlocks + (uint32_t)(i - 1)] = blockSize;
		dec->components[i].outStride = cStride;
		dec->components[i].outHeight = (uint32_t)blockSize;
		dec->components[i].outRowStep = 0;
	}

	return 0;
}

int jpegArmSetupCoefBuffer(JpegArmDecoder *dec, void *coefBuf, SceSize *outSize)
{
	uint64_t totalBytes;

	if (coefBuf != NULL) {
		uint32_t mcuCols = dec->mcuCols;
		uint32_t mcuRows = dec->mcuRows;
		uint32_t hCode = (dec->colorSpaceCode >> 2) & 3U;
		uint32_t yCols = mcuCols << (hCode - 1U);

		if (dec->mcuCompPtrs[1] != NULL) {
			dec->scanMcuCols = (uint16_t)mcuCols;
			dec->scanMcuRows = (uint16_t)mcuRows;
			int32_t rowBytes = (int32_t)(yCols << 7);

			int idx = 0;
			while (dec->mcuCompPtrs[idx] != NULL) {
				JpegArmComponent *comp = dec->mcuCompPtrs[idx];
				if (comp != &dec->components[0]) {
					dec->coefBlockSteps[idx] = 128;
					comp->coefRowStep = 0;
					idx++;
					continue;
				}

				uint32_t numY = dec->components[0].blocksPerMcu;
				switch (dec->colorSpaceCode & 0x0F) {
				case 5:
				case 9:
				case 13:
					for (uint32_t i = 0; i < numY; i++)
						dec->coefBlockSteps[idx + (int)i] = 128;
					dec->components[0].coefRowStep = 0;
					break;
				case 6:
				case 7:
					if (numY > 1U) {
						for (uint32_t i = 0; i < numY - 1U; i++)
							dec->coefBlockSteps[idx + (int)i] = rowBytes;
						dec->coefBlockSteps[idx + (int)numY - 1] = 128 - rowBytes * (int32_t)(numY - 1U);
						dec->components[0].coefRowStep = rowBytes * (int32_t)(numY - 1U);
					} else {
						dec->coefBlockSteps[idx] = 128;
						dec->components[0].coefRowStep = 0;
					}
					break;
				case 10:
					dec->coefBlockSteps[idx + 0] = 128;
					dec->coefBlockSteps[idx + 1] = (int32_t)((yCols - 1U) << 7);
					dec->coefBlockSteps[idx + 2] = 128;
					dec->coefBlockSteps[idx + 3] = (int32_t)((1 - (int32_t)yCols) << 7);
					dec->components[0].coefRowStep = rowBytes;
					break;
				default:
					break;
				}
				idx += (int)numY;
			}
		} else {
			JpegArmComponent *comp = dec->mcuCompPtrs[0];
			if (comp->blocksPerMcu == 1) {
				dec->scanMcuCols = (uint16_t)mcuCols;
				dec->scanMcuRows = (uint16_t)mcuRows;
				comp->coefRowStep = 0;
			} else {
				uint32_t blockRows = ((uint32_t)dec->imageHeight + 7U) >> 3;
				dec->scanMcuRows = (uint16_t)blockRows;
				if (dec->progPhase == 3U) {
					dec->scanMcuCols = (uint16_t)yCols;
					comp->coefRowStep = 0;
				} else {
					uint32_t blockCols = ((uint32_t)dec->imageWidth + 7U) >> 3;
					dec->scanMcuCols = (uint16_t)blockCols;
					comp->coefRowStep = (int32_t)((yCols - blockCols) << 7);
				}
			}
			dec->coefBlockSteps[0] = 128;
		}

		uint64_t mcuBlockBytes = ((uint64_t)mcuCols * mcuRows) << 7;
		if ((mcuBlockBytes >> 31) != 0)
			return SCE_JPEGARM_ERROR_UNSUPPORT_IMAGE_SIZE;

		uint8_t *ptr = (uint8_t *)coefBuf;
		totalBytes = 0;
		for (int i = 0; i < 4; i++) {
			dec->components[i].coefPtr = (int16_t *)ptr;
			uint64_t compBytes = mcuBlockBytes * dec->components[i].blocksPerMcu;
			if ((compBytes >> 31) != 0)
				return SCE_JPEGARM_ERROR_UNSUPPORT_IMAGE_SIZE;
			ptr += compBytes;
			totalBytes += compBytes;
		}
	} else {
		uint64_t mcuBlockBytes = ((uint64_t)dec->mcuCols * dec->mcuRows) << 7;
		if ((mcuBlockBytes >> 31) != 0)
			return SCE_JPEGARM_ERROR_UNSUPPORT_IMAGE_SIZE;

		totalBytes = 0;
		for (int i = 0; i < 4; i++) {
			uint64_t compBytes = mcuBlockBytes * dec->components[i].blocksPerMcu;
			if ((compBytes >> 31) != 0)
				return SCE_JPEGARM_ERROR_UNSUPPORT_IMAGE_SIZE;
			totalBytes += compBytes;
		}
	}

	uint64_t finalSize = totalBytes + 32ULL;
	if ((finalSize >> 31) != 0)
		return SCE_JPEGARM_ERROR_UNSUPPORT_IMAGE_SIZE;

	if (outSize != NULL)
		*outSize = (SceSize)finalSize;
	return 0;
}

int jpegArmSuspendSplitInput(JpegArmDecoder *dec, int err)
{
	const uint8_t *cur = dec->streamCur;
	uint8_t *base = dec->streamBase;
	const uint8_t *end = dec->streamEnd;

	if (err == SCE_JPEGARM_ERROR_BAD_MARKER_LENGTH) {
		if ((uint32_t)(dec->splitState - 2U) > 2U)
			return err;

		if (cur > base && cur + 1 < end) {
			uint32_t segLen = ((uint32_t)cur[0] << 8) | cur[1];
			if ((uint32_t)(cur[-1] - 0xE0U) <= 0x1EU && segLen != 0U) {
				dec->skipBytes = (uint32_t)((cur + segLen) - end);
				dec->streamCur = base;
				dec->splitState |= 8U;
				return SCE_JPEGARM_ERROR_INPUT_SUSPENDED;
			}
		}

		const uint8_t *src = cur - 2;
		if (src <= base)
			return SCE_JPEGARM_ERROR_INPUT_DATA_TOO_BIG;
		if (src > end)
			return err;

		SceSize rem = (SceSize)(end - src);
		for (SceSize i = 0; i < rem; i++)
			base[i] = src[i];
		dec->streamCur = base + rem;
		dec->splitState |= 8U;
		return SCE_JPEGARM_ERROR_INPUT_SUSPENDED;
	}

	if (err == SCE_JPEGARM_ERROR_INPUT_SUSPENDED) {
		if (cur <= base)
			return SCE_JPEGARM_ERROR_INPUT_DATA_TOO_BIG;
		if (cur <= end) {
			SceSize rem = (SceSize)(end - cur);
			for (SceSize i = 0; i < rem; i++)
				base[i] = cur[i];
			dec->streamCur = base + rem;
			dec->splitState |= 8U;
		}
	}

	return err;
}

/* =========================================================================
 * Progressive scan progression & frame/row drivers (0x810013f8 .. 0x810029d4)
 * ========================================================================= */

int jpegArmAdvanceProgressiveScan(JpegArmDecoder *dec)
{
	for (;;) {
		if (dec->progCoeffDone >= dec->totalCoeffCount) {
			int total = dec->totalCoeffCount;
			int done = dec->progCoeffDone;

			if ((dec->flags & JPEGARM_FLAG_OUTPUT_RGBA) != 0) {
				if (total <= 0) {
					if (dec->progErrorFlag == -1)
						return SCE_JPEGARM_ERROR_DECODE_ERROR;
					int m = jpegArmParseMarkers(dec);
					if (m < 0) {
						if (dec->splitState != 0) {
							dec->splitState = 4;
							return jpegArmSuspendSplitInput(dec, m);
						}
						return m;
					}
					if (m == 0xD9)
						return 0;
					dec->curMcuRow = -1;
					return SCE_JPEGARM_ERROR_EOI_EXPECTED;
				}
			}

			if (done == total) {
				dec->progCoeffDone = 0;
				dec->scanCoeffCount = -1;
				dec->totalCoeffCount = (int16_t)(-(total / 64));
				dec->progPhase = 3;
				if (dec->totalCoeffCount >= 0) {
					if (dec->progErrorFlag == -1)
						return SCE_JPEGARM_ERROR_DECODE_ERROR;
					int m = jpegArmParseMarkers(dec);
					if (m < 0) {
						if (dec->splitState != 0) {
							dec->splitState = 4;
							return jpegArmSuspendSplitInput(dec, m);
						}
						return m;
					}
					if (m == 0xD9)
						return 0;
					dec->curMcuRow = -1;
					return SCE_JPEGARM_ERROR_EOI_EXPECTED;
				}
			} else if (total >= done) {
				if (dec->progErrorFlag == -1)
					return SCE_JPEGARM_ERROR_DECODE_ERROR;
				int m = jpegArmParseMarkers(dec);
				if (m < 0) {
					if (dec->splitState != 0) {
						dec->splitState = 4;
						return jpegArmSuspendSplitInput(dec, m);
					}
					return m;
				}
				if (m == 0xD9)
					return 0;
				dec->curMcuRow = -1;
				return SCE_JPEGARM_ERROR_EOI_EXPECTED;
			}

			jpegArmSetupProgressiveIdctPass(dec);
			dec->curMcuRow = 0;
			dec->mcuColOffset = (int16_t)(-(int)dec->regionStartX);
			jpegArmSetupCoefBuffer(dec, dec->coefBufBase, NULL);
			return 1;
		}

		dec->curMcuRow = 0;
		dec->mcuColOffset = 0;
		dec->progPhase = 1;
		int m = jpegArmParseMarkers(dec);
		if (m < 0) {
			if (dec->splitState != 0)
				m = jpegArmSuspendSplitInput(dec, m);
			if (m != SCE_JPEGARM_ERROR_UNEXPECTED_MARKER)
				return m;
		} else if (m == 0xDA) {
			int sos = jpegArmParseSos(dec);
			if (sos >= 0) {
				jpegArmSetupCoefBuffer(dec, dec->coefBufBase, NULL);
				dec->progPhase = 2;
				return 1;
			}
			if (dec->splitState != 0) {
				sos = jpegArmSuspendSplitInput(dec, sos);
				if (sos == SCE_JPEGARM_ERROR_INPUT_SUSPENDED)
					return sos;
			}
			dec->curMcuRow = -1;
			if (sos != SCE_JPEGARM_ERROR_UNEXPECTED_MARKER)
				return sos;
		}

		dec->streamCur -= 2;
		dec->progCoeffDone = dec->totalCoeffCount;
	}
}

int jpegArmDecodeMcuRow(void *ctxBuf)
{
	JpegArmDecoder *dec = alignDecoderCtx(ctxBuf);
	uint8_t splitState = dec->splitState;
	if (splitState > 7U)
		return SCE_JPEGARM_ERROR_INVALID_STATE;

	if (splitState > 3U) {
		if (splitState == 5U)
			dec->splitState = 0;
		int m = jpegArmParseMarkers(dec);
		if (m < 0) {
			if (dec->splitState != 0) {
				dec->splitState = 4;
				return jpegArmSuspendSplitInput(dec, m);
			}
			return m;
		}
		if (m == 0xD9)
			return 0;
		dec->curMcuRow = -1;
		return SCE_JPEGARM_ERROR_EOI_EXPECTED;
	}

	if (dec->isProgressive == 0) {
		if (dec->curMcuRow >= (int)dec->regionEndY || dec->streamCur >= dec->streamEnd)
			return SCE_JPEGARM_ERROR_CANNOT_CONTINUE;

		int ret = dec->decodeRowFn(dec);
		if (ret < 0) {
			if (dec->splitState != 0)
				return jpegArmSuspendSplitInput(dec, ret);
			return ret;
		}

		dec->curMcuRow++;
		if (dec->curMcuRow == (int)dec->mcuRows) {
			int m = jpegArmParseMarkers(dec);
			if (m < 0) {
				if (dec->splitState != 0) {
					dec->splitState = 4;
					return jpegArmSuspendSplitInput(dec, m);
				}
				return m;
			}
			if (m == 0xD9)
				return 0;
			dec->curMcuRow = -1;
			return SCE_JPEGARM_ERROR_EOI_EXPECTED;
		}

		for (int i = 0; i < 4; i++)
			dec->components[i].outPtr += dec->components[i].outRowStep;
		return 1;
	}

	/* Progressive path */
	if (dec->curMcuRow >= (int)dec->scanMcuRows || dec->streamCur >= dec->streamEnd)
		return SCE_JPEGARM_ERROR_CANNOT_CONTINUE;

	if (dec->skipBytes != 0U) {
		SceSize avail = (SceSize)(dec->streamEnd - dec->streamCur);
		if (dec->skipBytes >= avail) {
			dec->streamCur = dec->streamBase;
			dec->skipBytes -= avail;
			dec->splitState = splitState | 8U;
			return SCE_JPEGARM_ERROR_INPUT_SUSPENDED;
		}
		dec->streamCur += dec->skipBytes;
		dec->skipBytes = 0;
	}

	if (dec->progPhase == 1U)
		return jpegArmAdvanceProgressiveScan(dec);

	int ret = dec->decodeRowFn(dec);
	if (ret < 0) {
		if (ret != SCE_JPEGARM_ERROR_DECODE_ERROR) {
			if (dec->splitState != 0)
				return jpegArmSuspendSplitInput(dec, ret);
			return ret;
		}
		dec->progErrorFlag = -1;
		dec->progCoeffDone = (int16_t)(dec->totalCoeffCount - dec->scanCoeffCount);
		dec->curMcuRow = (int16_t)dec->scanMcuRows;
	} else {
		dec->curMcuRow++;
	}

	if (dec->curMcuRow == (int)dec->scanMcuRows) {
		dec->progCoeffDone = (int16_t)(dec->progCoeffDone + dec->scanCoeffCount);
		return jpegArmAdvanceProgressiveScan(dec);
	}

	for (int i = 0; i < 4; i++)
		dec->components[i].coefPtr = (int16_t *)((uint8_t *)dec->components[i].coefPtr + dec->components[i].coefRowStep);

	if (dec->progPhase == 3U) {
		int compIdx = -dec->progCoeffDone;
		dec->components[compIdx].outPtr += dec->components[compIdx].outRowStep;
	}
	return 1;
}

int jpegArmParseHeaderAndSos(void *ctxBuf, SceSize *coefSizeOut)
{
	JpegArmDecoder *dec = alignDecoderCtx(ctxBuf);
	if (dec->splitState > 7U)
		return SCE_JPEGARM_ERROR_INVALID_STATE;

	if (dec->skipBytes != 0U) {
		SceSize avail = (SceSize)(dec->streamEnd - dec->streamCur);
		if (dec->skipBytes >= avail) {
			dec->streamCur = dec->streamBase;
			dec->skipBytes -= avail;
			dec->splitState |= 8U;
			return SCE_JPEGARM_ERROR_INPUT_SUSPENDED;
		}
		dec->streamCur += dec->skipBytes;
		dec->skipBytes = 0;
	}

	int m = jpegArmParseMarkers(dec);
	if (m < 0) {
		if (dec->splitState != 0)
			return jpegArmSuspendSplitInput(dec, m);
		return m;
	}
	if (m != 0xDA) {
		dec->curMcuRow = -1;
		return SCE_JPEGARM_ERROR_UNEXPECTED_MARKER;
	}

	dec->regionStartX = 0;
	dec->regionStartY = 0;
	dec->regionWidthMcu = dec->mcuCols;
	dec->regionEndY = dec->mcuRows;
	dec->curMcuRow = 0;
	dec->mcuColOffset = 0;

	int sos = jpegArmParseSos(dec);
	if (sos < 0) {
		if (dec->splitState != 0) {
			sos = jpegArmSuspendSplitInput(dec, sos);
			if (sos == SCE_JPEGARM_ERROR_INPUT_SUSPENDED)
				return sos;
		}
		dec->curMcuRow = -1;
		return sos;
	}

	uint8_t cs = dec->colorSpaceCode;
	dec->scanColorSpaceCode = cs;
	uint8_t samp = cs & 0x0F;

	if ((dec->flags & JPEGARM_FLAG_JFIF_ONLY) != 0) {
		if ((cs & 0xF0) != 0x20)
			return SCE_JPEGARM_ERROR_UNSUPPORT_COLORSPACE;
		if (samp != 9 && samp != 10)
			return SCE_JPEGARM_ERROR_UNSUPPORT_SAMPLING;
	}

	if ((cs & 3) == 0 || (cs & 0x0C) == 0 || samp == 11 || samp == 14 || samp == 15)
		return SCE_JPEGARM_ERROR_UNSUPPORT_SAMPLING;

	uint8_t ds = dec->downscaleFactor;
	uint8_t dsShift = (uint8_t)(31 - __builtin_clz((unsigned int)ds));
	for (int i = 0; i < 4; i++) {
		dec->components[i].downscale = ds;
		dec->components[i].downscaleShift = dsShift;
	}

	if ((dec->flags & JPEGARM_FLAG_OUTPUT_RGBA) != 0) {
		if ((cs & 0xF0) != 0x20)
			return SCE_JPEGARM_ERROR_UNSUPPORT_COLORSPACE;
		jpegArmSetupRgbaOutput(dec, NULL);
	} else {
		jpegArmSetupYCbCrPlanes(dec, NULL);
	}

	if (dec->isProgressive != 0) {
		int err = jpegArmSetupCoefBuffer(dec, NULL, coefSizeOut);
		if (err < 0)
			return err;
	} else if (coefSizeOut != NULL) {
		*coefSizeOut = 0;
	}

	uint64_t ycbcrBytes = 0;
	for (int i = 0; i < 4; i++) {
		if (dec->components[i].blocksPerMcu == 0)
			break;
		ycbcrBytes += (uint64_t)dec->components[i].outStride * dec->components[i].outHeight;
	}
	if ((ycbcrBytes >> 31) != 0)
		return SCE_JPEGARM_ERROR_UNSUPPORT_IMAGE_SIZE;

	if (dec->splitState != 0)
		dec->splitState = 3;
	if (dec->isProgressive != 0)
		dec->progPhase = 2;

	return (int)ycbcrBytes;
}

int jpegArmInitAndParseHeader(void *ctxBuf, const uint8_t *jpegBuf, SceSize jpegSize,
	int downscale, uint8_t flags, int isSplit, SceSize *coefSizeOut)
{
	JpegArmDecoder *dec = alignDecoderCtx(ctxBuf);
	resetDecoderTables(dec);
	dec->splitState = (isSplit == 1) ? 1 : 0;

	SceSize soiEnd;
	int err = findSoiMarker(jpegBuf, jpegSize, &soiEnd);
	if (err < 0)
		return err;

	dec->flags = flags;
	if (((downscale - 1) & downscale) != 0 || (uint32_t)(downscale - 1) > 7U)
		return SCE_JPEGARM_ERROR_INVALID_DECODE_MODE;

	dec->streamCur = jpegBuf + soiEnd;
	dec->streamEnd = jpegBuf + jpegSize;
	dec->streamBase = (uint8_t *)jpegBuf;
	dec->skipBytes = 0;
	dec->colorSpaceCode = 0;
	dec->downscaleFactor = (uint8_t)downscale;
	if (isSplit == 1)
		dec->splitState = 2;
	dec->scanCoeffCount = 0;
	dec->progCoeffDone = 0;
	dec->totalCoeffCount = 0;
	dec->progErrorFlag = 0;
	dec->progPhase = 0;

	return jpegArmParseHeaderAndSos(dec, coefSizeOut);
}

int jpegArmBeginDecodeOutput(void *ctxBuf, uint8_t *outBuf, int valA, int posYA, int valB, int posYB, void *coefBuf)
{
	JpegArmDecoder *dec = alignDecoderCtx(ctxBuf);
	if (outBuf == NULL)
		return SCE_JPEGARM_ERROR_INVALID_POINTER;

	if (dec->isProgressive != 0) {
		if (coefBuf == NULL || ((uintptr_t)coefBuf & 7U) != 0U)
			return SCE_JPEGARM_ERROR_INVALID_POINTER;
		jpegArmSetupCoefBuffer(dec, coefBuf, NULL);
		dec->coefBufBase = coefBuf;
	}

	if (dec->curMcuRow != 0 || (dec->splitState != 0 && dec->splitState != 3))
		return SCE_JPEGARM_ERROR_INVALID_STATE;

	if ((valA | posYA | valB | posYB) != 0) {
		if ((valA | posYA) < 0 || valB > (int)dec->mcuCols || posYB > (int)dec->mcuRows || valA >= valB || posYA >= posYB)
			return SCE_JPEGARM_ERROR_INVALID_REGION;
		dec->regionStartX = (uint16_t)valA;
		dec->regionStartY = (uint16_t)posYA;
		dec->regionWidthMcu = (uint16_t)(valB - valA);
		dec->regionEndY = (uint16_t)posYB;
		dec->mcuColOffset = (dec->isProgressive != 0) ? 0 : (int16_t)(-valA);
	}

	if ((dec->flags & JPEGARM_FLAG_OUTPUT_RGBA) != 0)
		return jpegArmSetupRgbaOutput(dec, outBuf);

	int ret = jpegArmSetupYCbCrPlanes(dec, outBuf);
	return (ret < 0) ? ret : 0;
}

int jpegArmDecodeFullFrame(const uint8_t *jpegBuf, SceSize jpegSize, uint8_t *outBuf, SceSize outSize,
	uint8_t flags, int downscale, int *outColorSpace, void *coefBuf, SceSize coefSize)
{
	uint8_t ctxRaw[sizeof(JpegArmDecoder) + 128];
	JpegArmDecoder *dec = alignDecoderCtx(ctxRaw);
	SceSize reqCoefSize = 0;

	int ret = jpegArmInitAndParseHeader(dec, jpegBuf, jpegSize, downscale, flags, 0, &reqCoefSize);
	if (ret < 0)
		return ret;

	if ((flags & JPEGARM_FLAG_OUTPUT_RGBA) != 0) {
		ret = jpegArmSetupRgbaOutput(dec, outBuf);
		if (ret < 0)
			return ret;
		int dims = jpegArmGetDecodedDimensions(dec);
		SceSize reqRgba = (SceSize)((dims >> 16) & 0xFFFF) * (SceSize)(dims & 0xFFFF) * 4U;
		if (outSize < reqRgba)
			return SCE_JPEGARM_ERROR_MEMORY_SIZE;
	} else {
		if (outSize < (SceSize)ret)
			return SCE_JPEGARM_ERROR_MEMORY_SIZE;
		ret = jpegArmSetupYCbCrPlanes(dec, outBuf);
		if (ret < 0)
			return ret;
	}

	if (dec->isProgressive != 0) {
		if (coefBuf == NULL || ((uintptr_t)coefBuf & 7U) != 0U)
			return SCE_JPEGARM_ERROR_INVALID_POINTER;
		if (coefSize < reqCoefSize)
			return SCE_JPEGARM_ERROR_MEMORY_SIZE;
		jpegArmSetupCoefBuffer(dec, coefBuf, NULL);
		dec->coefBufBase = coefBuf;
	}

	int rowRet;
	do {
		rowRet = jpegArmDecodeMcuRow(dec);
	} while (rowRet > 0);

	if (rowRet < 0)
		return rowRet;

	if (outColorSpace != NULL)
		*outColorSpace = dec->colorSpaceCode;
	return jpegArmGetDecodedDimensions(dec);
}

int jpegArmGetSplitWriteBuffer(void *ctxBuf, uint8_t **writePtrOut, SceSize *writeSizeOut)
{
	JpegArmDecoder *dec = alignDecoderCtx(ctxBuf);
	if (writePtrOut == NULL || writeSizeOut == NULL)
		return SCE_JPEGARM_ERROR_INVALID_POINTER;
	if (dec->splitState <= 7U)
		return SCE_JPEGARM_ERROR_INVALID_STATE;

	*writePtrOut = (uint8_t *)dec->streamCur;
	*writeSizeOut = (SceSize)(dec->streamEnd - dec->streamCur);
	return 0;
}

int jpegArmCommitSplitInput(void *ctxBuf, SceSize writtenSize, int isEndOfStream)
{
	JpegArmDecoder *dec = alignDecoderCtx(ctxBuf);
	if ((writtenSize | (SceSize)isEndOfStream) == 0U)
		return SCE_JPEGARM_ERROR_INVALID_DATA_SIZE;
	if (writtenSize > (SceSize)(dec->streamEnd - dec->streamCur))
		return SCE_JPEGARM_ERROR_INVALID_DATA_SIZE;
	if (dec->splitState <= 7U)
		return SCE_JPEGARM_ERROR_INVALID_STATE;

	uint8_t nextState = dec->splitState & 0xF7U;
	const uint8_t *newEnd = dec->streamCur + writtenSize;
	dec->streamCur = dec->streamBase;
	dec->streamEnd = newEnd;

	if (isEndOfStream == 1)
		dec->splitState = (nextState == 4U) ? 5U : 0U;
	else
		dec->splitState = nextState;
	return 0;
}

int jpegArmGetOutputInfoInternal(const uint8_t *jpegBuf, SceSize jpegSize, uint8_t flags,
	int downscale, int outputFormat, SceJpegOutputInfo *info)
{
	uint8_t ctxRaw[sizeof(JpegArmDecoder) + 128];
	JpegArmDecoder *dec = alignDecoderCtx(ctxRaw);
	SceSize coefSize = 0;

	resetDecoderTables(dec);
	dec->splitState = 0;

	SceSize soiEnd;
	int err = findSoiMarker(jpegBuf, jpegSize, &soiEnd);
	if (err < 0) {
		info->colorSpace = dec->colorSpaceCode;
		return err;
	}

	dec->flags = flags;
	if (((downscale - 1) & downscale) != 0 || (uint32_t)(downscale - 1) > 7U) {
		info->colorSpace = dec->colorSpaceCode;
		return SCE_JPEGARM_ERROR_INVALID_DECODE_MODE;
	}

	dec->streamCur = jpegBuf + soiEnd;
	dec->streamEnd = jpegBuf + jpegSize;
	dec->streamBase = (uint8_t *)jpegBuf;
	dec->skipBytes = 0;
	dec->colorSpaceCode = 0;
	dec->downscaleFactor = (uint8_t)downscale;
	dec->scanCoeffCount = 0;
	dec->progCoeffDone = 0;
	dec->totalCoeffCount = 0;
	dec->progErrorFlag = 0;
	dec->progPhase = 0;

	int ycbcrSize = jpegArmParseHeaderAndSos(dec, &coefSize);
	if (ycbcrSize < 0) {
		info->colorSpace = dec->colorSpaceCode;
		return ycbcrSize;
	}

	return jpegArmFillOutputInfo(dec, outputFormat, (SceSize)ycbcrSize, coefSize, info);
}

int jpegArmFillOutputInfo(void *ctxBuf, int outputFormat, SceSize ycbcrSize, SceSize coefSize,
	SceJpegOutputInfo *info)
{
	JpegArmDecoder *dec = alignDecoderCtx(ctxBuf);

	info->colorSpace = dec->colorSpaceCode;
	info->imageWidth = dec->imageWidth;
	info->imageHeight = dec->imageHeight;
	info->coefBufferSize = coefSize;

	if (outputFormat < 0) {
		info->outputBufferSize = ycbcrSize;
		info->tempBufferSize = 0;
		for (int i = 0; i < 4; i++) {
			if (dec->components[i].blocksPerMcu != 0) {
				info->pitch[i].x = dec->components[i].outStride;
				info->pitch[i].y = dec->components[i].outHeight;
			} else {
				info->pitch[i].x = 0;
				info->pitch[i].y = 0;
			}
		}
		return 0;
	}

	uint32_t pitchX;
	uint32_t pitchY;
	SceSize tempSize;

	if ((dec->flags & JPEGARM_FLAG_OUTPUT_RGBA) != 0) {
		jpegArmSetupRgbaOutput(dec, NULL);
		pitchX = dec->rgbaWidth * 4U;
		pitchY = dec->rgbaHeight;
		tempSize = 0;
	} else {
		pitchX = dec->components[0].outStride * 4U;
		pitchY = dec->components[0].outHeight;
		tempSize = ycbcrSize;
	}

	uint64_t total = (uint64_t)pitchX * pitchY;
	info->outputBufferSize = (SceSize)total;
	info->tempBufferSize = tempSize;
	info->pitch[0].x = pitchX;
	info->pitch[0].y = pitchY;
	for (int i = 1; i < 4; i++) {
		info->pitch[i].x = 0;
		info->pitch[i].y = 0;
	}

	if ((total >> 32) != 0)
		return SCE_JPEGARM_ERROR_UNSUPPORT_IMAGE_SIZE;
	return 0;
}

int jpegArmGetDecodedDimensions(void *ctxBuf)
{
	JpegArmDecoder *dec = alignDecoderCtx(ctxBuf);
	if ((dec->flags & JPEGARM_FLAG_OUTPUT_RGBA) != 0)
		return (int)((dec->rgbaWidth << 16) | dec->rgbaHeight);
	return (int)((dec->components[0].outStride << 16) + dec->components[0].outHeight);
}

int jpegArmSetOutputPointers(void *ctxBuf, uint8_t *planes[4])
{
	JpegArmDecoder *dec = alignDecoderCtx(ctxBuf);
	if (planes == NULL)
		return SCE_JPEGARM_ERROR_INVALID_POINTER;

	if ((dec->flags & JPEGARM_FLAG_OUTPUT_RGBA) != 0) {
		if (((uintptr_t)planes[0] & 3U) != 0U)
			return SCE_JPEGARM_ERROR_INVALID_POINTER;
		dec->rgbaOutPtr = planes[0];
		return 0;
	}

	uintptr_t mask = ((uintptr_t)8U >> dec->components[0].downscaleShift) - 1U;
	for (int i = 0; i < 4; i++) {
		if (((uintptr_t)planes[i] & mask) != 0U)
			return SCE_JPEGARM_ERROR_INVALID_POINTER;
		dec->components[i].outPtr = planes[i];
	}
	return 0;
}

int jpegArmParseDht(JpegArmDecoder *dec, int markerLen)
{
	const uint8_t *ptr = dec->streamCur;
	int remaining = markerLen;

	while (remaining > 0) {
		uint8_t header = ptr[0];
		uint32_t tblIdx = header & 0x0FU;
		int isAc = (header & 0x10U) != 0U;
		if (tblIdx > 3U)
			return SCE_JPEGARM_ERROR_BAD_DHT_INDEX;

		JpegArmHuffTable *tbl = &dec->huffTables[tblIdx][isAc];
		uint8_t cumCount[16];
		int totalSyms = 0;
		for (int i = 0; i < 16; i++) {
			totalSyms += ptr[1 + i];
			cumCount[i] = (uint8_t)totalSyms;
		}
		if (totalSyms > 255)
			return SCE_JPEGARM_ERROR_BAD_DHT_COUNTS;

		const uint8_t *symPtr = ptr + 17;
		int fastEnd = 0;
		int maxBits = 8;
		int code = 0;
		int slowPos = cumCount[7] - 1;
		int bitLen = 1;

		if (totalSyms != 0) {
			for (int i = 0; i < totalSyms; i++) {
				uint8_t sym = *symPtr++;
				while (bitLen <= 16 && i >= cumCount[bitLen - 1])
					bitLen++;

				int cat = sym & 0x0F;
				if (bitLen <= 8) {
					int spanEnd = code + (256 >> bitLen);
					if (spanEnd > 256)
						return SCE_JPEGARM_ERROR_BAD_DHT_COUNTS;

					if (cat != 0 && bitLen + cat <= 8) {
						int8_t tag = isAc
							? (int8_t)((sym >> 4) - 16 * (bitLen + cat))
							: (int8_t)(bitLen + cat);
						int shift = 32 - cat;
						for (int c = code; c < spanEnd; c++) {
							uint32_t bits = (uint32_t)c << (bitLen + 24);
							uint32_t signMask = (uint32_t)((int32_t)(~bits) >> 31);
							tbl->fastVal[c] = (int8_t)((bits >> shift) - (signMask >> shift));
							tbl->fastBits[c] = tag;
						}
					} else {
						int8_t tag = isAc ? (int8_t)bitLen : (int8_t)(-bitLen);
						int8_t val = isAc ? (int8_t)sym : (int8_t)(-cat);
						for (int c = code; c < spanEnd; c++) {
							tbl->fastVal[c] = val;
							tbl->fastBits[c] = tag;
						}
					}
					code = spanEnd;
					fastEnd = code;
				} else {
					tbl->slowSym[slowPos - i - 1] = isAc ? sym : (uint8_t)(-cat);
					if (bitLen > maxBits) {
						if (maxBits == 8) {
							for (int b = 9; b < bitLen; b++)
								tbl->maxCode[16 - b] = 0;
						} else {
							uint16_t limit = (uint16_t)(code << (16 - maxBits));
							for (int b = maxBits; b < bitLen; b++)
								tbl->maxCode[16 - b] = limit;
							tbl->valOffset[16 - maxBits] = (uint16_t)(slowPos + code - i);
						}
						code <<= (bitLen - maxBits);
						maxBits = bitLen;
					}
					code++;
				}
			}
		}

		if (maxBits == 8) {
			uint8_t fillBits = (fastEnd > 0) ? (uint8_t)tbl->fastBits[fastEnd - 1] : 0;
			uint8_t fillVal = (fastEnd > 0) ? (uint8_t)tbl->fastVal[fastEnd - 1] : 0;
			for (int c = fastEnd; c < 256; c++) {
				tbl->fastBits[c] = (int8_t)fillBits;
				tbl->fastVal[c] = (int8_t)fillVal;
			}
		} else {
			int fillLen = (fastEnd < 256) ? (256 - fastEnd) : 1;
			jpegArmMemset(&tbl->fastBits[fastEnd], isAc ? 40 : 0, (SceSize)fillLen);
			if (code < totalSyms)
				return SCE_JPEGARM_ERROR_BAD_DHT_COUNTS;
			tbl->valOffset[16 - bitLen] = (uint16_t)(slowPos + code - totalSyms);
			uint16_t limit = (uint16_t)(code << (16 - maxBits));
			for (int b = maxBits; b <= 15; b++)
				tbl->maxCode[16 - b] = limit;
			tbl->maxCode[0] = 0;
		}

		tbl->valid = 1;
		ptr = symPtr;
		remaining -= 17 + totalSyms;
	}

	return (remaining == 0) ? 0 : SCE_JPEGARM_ERROR_BAD_DHT_MARKER;
}

/* =========================================================================
 * Progressive IDCT pass setup, SOS parser, marker parser, memset
 * ========================================================================= */

int jpegArmSetupProgressiveIdctPass(JpegArmDecoder *dec)
{
	if ((dec->flags & JPEGARM_FLAG_OUTPUT_RGBA) != 0) {
		int numY = dec->components[0].blocksPerMcu;
		int count = (numY < 1) ? 1 : numY;
		for (int i = 0; i < count; i++)
			dec->mcuCompPtrs[i] = &dec->components[0];
		dec->mcuCompPtrs[count] = &dec->components[1];
		dec->mcuCompPtrs[count + 1] = &dec->components[2];
		dec->mcuCompPtrs[count + 2] = NULL;
		dec->decodeRowFn = jpegArmIdctProgressiveRowRgba;
		return 0;
	}

	int compIdx = -dec->progCoeffDone;
	uint32_t cs = dec->colorSpaceCode;
	int hShift = (int)((cs >> 2) & 3U) - 1;
	int vShift = (int)(cs & 3U) - 1;

	dec->mcuCompPtrs[0] = &dec->components[compIdx];
	dec->mcuCompPtrs[1] = NULL;

	if (compIdx == 0) {
		dec->compMcuCols = (uint16_t)(dec->mcuCols << hShift);
		dec->regionStartX = (uint16_t)(dec->regionStartX << hShift);
		dec->regionWidthMcu = (uint16_t)(dec->regionWidthMcu << hShift);
		dec->regionStartY = (uint16_t)(dec->regionStartY << vShift);
		dec->regionEndY = (uint16_t)(dec->regionEndY << vShift);
	} else if (compIdx == 1) {
		dec->compMcuCols = dec->mcuCols;
		dec->regionStartX = (uint16_t)(dec->regionStartX >> hShift);
		dec->regionWidthMcu = (uint16_t)(dec->regionWidthMcu >> hShift);
		dec->regionStartY = (uint16_t)(dec->regionStartY >> vShift);
		dec->regionEndY = (uint16_t)(dec->regionEndY >> vShift);
	}

	dec->decodeRowFn = jpegArmIdctProgressiveRowYCbCr;
	return 0;
}

int jpegArmParseSos(JpegArmDecoder *dec)
{
	const uint8_t *cur = dec->streamCur;
	const uint8_t *end = dec->streamEnd;
	int segLen = ((int)cur[0] << 8) | cur[1];
	int numComps = cur[2];
	const uint8_t *segEnd = cur + segLen;

	if (end <= segEnd || segLen - 3 != 2 * numComps + 3 || (uint32_t)(numComps - 1) > 2U)
		return SCE_JPEGARM_ERROR_BAD_SOS_MARKER;

	if (dec->isProgressive == 0) {
		dec->scanStart = 0;
		dec->scanEnd = 63;
		dec->decodeRowFn = ((dec->flags & JPEGARM_FLAG_OUTPUT_RGBA) != 0)
			? jpegArmDecodeRowBaselineRgba
			: jpegArmDecodeRowBaselineYCbCr;
	} else {
		uint8_t ss = segEnd[-3];
		uint8_t se = segEnd[-2];
		uint8_t ahAl = segEnd[-1];
		uint8_t al = ahAl & 0x0F;
		uint8_t ah = ahAl >> 4;

		dec->scanStart = ss;
		dec->scanEnd = se;
		dec->approxLow = al;
		dec->approxHigh = ah;

		if (dec->progPhase == 0) {
			if (ss != 0 || se != 0)
				return SCE_JPEGARM_ERROR_BAD_PROGRESSIVE_PARAM;
		} else if (ss != 0) {
			if (ss > se || se > 63 || numComps != 1)
				return SCE_JPEGARM_ERROR_BAD_PROGRESSIVE_PARAM;
		} else if (se != 0) {
			return SCE_JPEGARM_ERROR_BAD_PROGRESSIVE_PARAM;
		}

		if ((ah != 0 && ah != al + 1) || al > 13)
			return SCE_JPEGARM_ERROR_BAD_PROGRESSIVE_PARAM;

		dec->scanCoeffCount = (al != 0) ? 0 : (int16_t)(numComps * (se - ss + 1));

		if (ah == 0)
			dec->decodeRowFn = (ss != 0) ? jpegArmDecodeRowProgressiveAcFirst : jpegArmDecodeRowProgressiveDcFirst;
		else
			dec->decodeRowFn = (ss != 0) ? jpegArmDecodeRowProgressiveAcRefine : jpegArmDecodeRowProgressiveDcRefine;
	}

	const uint8_t *compSpec = cur + 3;
	int mcuBlocks = 0;

	for (int i = 0; i < numComps; i++, compSpec += 2) {
		uint8_t compId = compSpec[0];
		int compIdx = -1;
		for (int c = 0; c < 4; c++) {
			if (dec->components[c].id == compId) {
				compIdx = c;
				break;
			}
		}
		if (compIdx < 0)
			return SCE_JPEGARM_ERROR_BAD_COMPONENT_ID;

		uint8_t tdTa = compSpec[1];
		JpegArmHuffTable *dcTbl = &dec->huffTables[tdTa >> 4][0];
		JpegArmHuffTable *acTbl = &dec->huffTables[tdTa & 0x0F][1];
		dec->components[compIdx].dcTable = dcTbl;
		dec->components[compIdx].acTable = acTbl;

		if ((dec->scanStart == 0 && dcTbl->valid == 0) ||
			(dec->scanEnd != 0 && acTbl->valid == 0))
			return SCE_JPEGARM_ERROR_NO_HUFF_TABLE;

		if (numComps == 1) {
			if (mcuBlocks >= 24)
				return SCE_JPEGARM_ERROR_BAD_MCU_SIZE;
			dec->mcuCompPtrs[mcuBlocks++] = &dec->components[compIdx];
		} else {
			int count = dec->components[compIdx].blocksPerMcu;
			if (mcuBlocks + count > 24)
				return SCE_JPEGARM_ERROR_BAD_MCU_SIZE;
			for (int b = 0; b < count; b++)
				dec->mcuCompPtrs[mcuBlocks++] = &dec->components[compIdx];
		}
	}

	dec->mcuCompPtrs[mcuBlocks] = NULL;
	for (int c = 0; c < 4; c++)
		dec->components[c].dcPred = 0;
	dec->progEobRun = 0;

	/* Prime the 32-bit bit buffer from entropy-coded data */
	const uint8_t *dataPtr = segEnd;
	uint32_t bits = 0;
	int loaded = 0;

	for (int i = 0; i < 4; i++) {
		if (dataPtr >= end)
			break;
		uint8_t byte = *dataPtr;
		if (byte == 0xFF) {
			if (dataPtr + 1 >= end || dataPtr[1] != 0x00)
				break;
			dataPtr++;
		}
		dataPtr++;
		bits |= (uint32_t)byte << (24 - i * 8);
		loaded++;
	}

	dec->streamCur = dataPtr;
	dec->bitBuf = bits;
	dec->bitCount = (int8_t)(8 - loaded * 8);

	if (dec->splitState != 0 && (dec->bitCount > 0 || dataPtr >= end)) {
		dec->streamCur = cur;
		return SCE_JPEGARM_ERROR_BAD_MARKER_LENGTH;
	}

	if (dec->restartInterval != 0) {
		dec->restartCountdown = dec->restartInterval;
		dec->nextRestartMarker = 0xD0;
	}
	return 0;
}

int jpegArmParseMarkers(JpegArmDecoder *dec)
{
	if ((dec->flags & JPEGARM_FLAG_DEFAULT_DHT) != 0) {
		const uint8_t *saved = dec->streamCur;
		dec->streamCur = g_jpegArmDefaultDht;
		int err = jpegArmParseDht(dec, (int)sizeof(g_jpegArmDefaultDht));
		dec->streamCur = saved;
		if (err < 0)
			return err;
	}

	const uint8_t *cur = dec->streamCur;
	const uint8_t *end = dec->streamEnd;

	for (;;) {
		while (cur + 1 < end) {
			if (cur[0] == 0xFF && cur[1] != 0xFF)
				break;
			cur++;
		}
		if (cur + 1 >= end) {
			dec->streamCur = end + 1;
			return SCE_JPEGARM_ERROR_BAD_MARKER_LENGTH;
		}

		uint8_t marker = cur[1];
		dec->streamCur = cur + 2;
		if (marker == 0xD9)
			return 0xD9;

		if (cur + 3 >= end)
			return SCE_JPEGARM_ERROR_BAD_MARKER_LENGTH;

		if ((marker == 0xE0 || marker == 0xE1) && (dec->colorSpaceCode & 0xF0) == 0)
			dec->colorSpaceCode = 0x20;

		const uint8_t *payload = cur + 4;
		int payloadLen = (((int)cur[2] << 8) | cur[3]) - 2;
		if (payloadLen < 0 || payload + payloadLen >= end)
			return SCE_JPEGARM_ERROR_BAD_MARKER_LENGTH;

		const uint8_t *nextMarker = payload + payloadLen;
		if (marker == 0xDA)
			return 0xDA;

		dec->streamCur = payload;
		int err = 0;

		if (marker == 0xC4) {
			err = jpegArmParseDht(dec, payloadLen);
		} else if (marker == 0xC0 || marker == 0xC1 || marker == 0xC2) {
			if (marker == 0xC2)
				dec->isProgressive = 1;

			if (payload[0] != 8) {
				err = SCE_JPEGARM_ERROR_BAD_PRECISION;
			} else if ((dec->colorSpaceCode & 0x0F) != 0) {
				err = SCE_JPEGARM_ERROR_SOF_DUPLICATE;
			} else {
				uint16_t height = (uint16_t)(((uint16_t)payload[1] << 8) | payload[2]);
				uint16_t width = (uint16_t)(((uint16_t)payload[3] << 8) | payload[4]);
				int numComps = payload[5];
				dec->imageHeight = height;
				dec->imageWidth = width;

				if (height == 0 || width == 0) {
					err = SCE_JPEGARM_ERROR_IMAGE_EMPTY;
				} else if ((uint32_t)(numComps - 1) > 2U) {
					err = SCE_JPEGARM_ERROR_COMPONENT_COUNT;
				} else if (payloadLen != 3 * numComps + 6) {
					err = SCE_JPEGARM_ERROR_BAD_SOF_MARKER;
				} else {
					uint8_t cs = dec->colorSpaceCode;
					const uint8_t *compData = payload + 6;
					for (int c = 0; c < numComps; c++, compData += 3) {
						dec->components[c].id = compData[0];
						uint8_t hv = compData[1];
						uint32_t vSamp = hv & 0x0FU;
						uint32_t hSamp = hv >> 4;
						uint8_t blocks;

						if (c == 0) {
							if (hSamp == 0 || vSamp == 0 || hSamp == 3 || vSamp == 3 || hSamp > 4 || vSamp > 4) {
								err = SCE_JPEGARM_ERROR_UNSUPPORT_SAMPLING;
								break;
							}
							uint32_t hShift = hSamp >> 1;
							uint32_t vShift = vSamp >> 1;
							dec->mcuCols = (uint16_t)((width + 8U * hSamp - 1U) >> (hShift + 3U));
							dec->mcuRows = (uint16_t)((height + 8U * vSamp - 1U) >> (vShift + 3U));
							blocks = (uint8_t)(hSamp * vSamp);
							cs = (uint8_t)(cs + (vShift + 4U * hShift) + 5U);
						} else {
							if (hv != 0x11) {
								err = SCE_JPEGARM_ERROR_UNSUPPORT_SAMPLING;
								break;
							}
							blocks = 1;
						}

						uint32_t qIdx = compData[2];
						if (qIdx > 3U) {
							err = SCE_JPEGARM_ERROR_NO_QUANT_TABLE;
							break;
						}
						dec->components[c].quantTable = dec->quantTables[qIdx];
						dec->components[c].blocksPerMcu = blocks;
					}

					if (err == 0) {
						uint8_t lastId = dec->components[numComps - 1].id;
						for (int c = numComps; c < 4; c++)
							dec->components[c].id = lastId;
						dec->totalCoeffCount = (int16_t)(numComps << 6);

						if (numComps == 1) {
							if ((cs & 0x0F) == 5)
								dec->colorSpaceCode = 0x15;
							else
								err = SCE_JPEGARM_ERROR_UNSUPPORT_SAMPLING;
						} else if (numComps == 3) {
							if ((cs & 0xF0) != 0) {
								dec->colorSpaceCode = cs;
							} else if (dec->components[0].id == 'R' &&
								dec->components[1].id == 'G' &&
								dec->components[2].id == 'B') {
								dec->colorSpaceCode = cs | 0x50;
							} else {
								dec->colorSpaceCode = cs | 0x20;
							}
						} else {
							dec->colorSpaceCode = cs & 0x0F;
						}
					}
				}
			}
		} else if (marker == 0xC3) {
			return SCE_JPEGARM_ERROR_NO_LOSSLESS_SUPPORT;
		} else if (marker == 0xC9 || marker == 0xCC) {
			return SCE_JPEGARM_ERROR_NO_ARITH_SUPPORT;
		} else if (marker == 0x01 || (marker >= 0xC5 && marker <= 0xC8) ||
			(marker >= 0xCA && marker <= 0xCB) || (marker >= 0xCD && marker <= 0xD7)) {
			return SCE_JPEGARM_ERROR_UNKNOWN_MARKER;
		} else if (marker == 0xDB) {
			const uint8_t *qPtr = payload;
			int qRem = payloadLen;
			while (qRem > 0) {
				uint8_t pqTq = *qPtr++;
				uint32_t qIdx = pqTq & 0x0FU;
				int is16Bit = (pqTq & 0x10U) != 0U;
				if (qIdx > 3U) {
					err = SCE_JPEGARM_ERROR_BAD_DQT_INDEX;
					break;
				}
				int16_t *dst = dec->quantTables[qIdx];
				if (is16Bit) {
					dst[0] = (int16_t)(((uint16_t)qPtr[0] << 8) | qPtr[1]);
					qPtr += 2;
					for (int i = 0; i < 63; i++) {
						int16_t val = (int16_t)(((uint16_t)qPtr[0] << 8) | qPtr[1]);
						qPtr += 2;
						*(int16_t *)((uint8_t *)dst + g_jpegArmZigzagByteOffset[i]) = val;
					}
					qRem -= 129;
				} else {
					dst[0] = (int16_t)(*qPtr++);
					for (int i = 0; i < 63; i++)
						*(int16_t *)((uint8_t *)dst + g_jpegArmZigzagByteOffset[i]) = (int16_t)(*qPtr++);
					qRem -= 65;
				}
				if (qRem < 0) {
					err = SCE_JPEGARM_ERROR_BAD_DQT_MARKER;
					break;
				}
			}
		} else if (marker == 0xDD) {
			if (payloadLen != 2) {
				dec->streamCur = nextMarker;
				return SCE_JPEGARM_ERROR_BAD_DRI_MARKER;
			}
			dec->restartInterval = (uint16_t)(((uint16_t)payload[0] << 8) | payload[1]);
		}

		dec->streamCur = nextMarker;
		if (err < 0)
			return err;
		cur = nextMarker;
	}
}

void *jpegArmMemset(void *dst, int c, SceSize len)
{
	return sceClibMemset(dst, c, len);
}
