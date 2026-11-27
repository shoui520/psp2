/*
 * Sony scePng lightweight PNG decoder / info query (linked from libScePng).
 */
#include "mp4rec_internal.h"
#include <libdeflt.h>

extern unsigned int sceGzipCrc32(unsigned int uiCrc, const unsigned char *pSrc, unsigned int uiSize);
extern unsigned int sceZlibAdler32(unsigned int uiAdler, const unsigned char *pSrc, unsigned int uiSize);
extern int sceZlibIsValid(const void *pSrc);
extern int sceDeflateDecompress(void *dst, unsigned int dstSize, const void *src,
	const void **nextSrc);
extern int sceDeflateDecompressPartial(void *dst, unsigned int dstSize, const void *src,
	const void **nextSrc, SceDeflatePartialInputParam *param);

#define SCE_PNG_ERROR_INVALID_FORMAT   (-2140602367)
#define SCE_PNG_ERROR_INVALID_SIZE     (-2140602366)
#define SCE_PNG_ERROR_NOT_SUPPORTED    (-2140602365)
#define SCE_PNG_ERROR_INVALID_POINTER  (-2140602364)

static const uint8_t s_adam7ShiftY[8] = { 3, 3, 3, 2, 2, 1, 1, 0 };
static const uint8_t s_adam7ShiftX[8] = { 3, 3, 2, 2, 1, 1, 0, 0 };
static const uint8_t s_adam7BiasY[8]  = { 7, 7, 3, 3, 1, 1, 0, 0 };
static const uint8_t s_adam7BiasX[8]  = { 7, 3, 3, 1, 1, 0, 0, 0 };
static const uint8_t s_adam7StepY[8]  = { 8, 8, 8, 4, 4, 2, 2, 1 };
static const uint8_t s_adam7StepX[8]  = { 8, 8, 4, 4, 2, 2, 1, 1 };
static const uint8_t s_adam7StartY[8] = { 0, 0, 4, 0, 2, 0, 1, 0 };
static const uint8_t s_adam7StartX[8] = { 0, 4, 0, 2, 0, 1, 0, 0 };

static uint32_t ReadBe32(const void *p)
{
	const uint8_t *b = (const uint8_t *)p;
	return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
		((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

int32_t _scePngConvert24to32(void *buf, int32_t height, int32_t width, uint32_t transColor)
{
	uint32_t totalPixels = (uint32_t)(width * height);
	uint8_t *src = (uint8_t *)buf + totalPixels * 3U;
	uint32_t *dst = (uint32_t *)((uint8_t *)buf + totalPixels * 4U);

	while ((uint8_t *)dst != src) {
		src -= 3;
		dst--;
		uint32_t px = ((uint32_t)src[0]) | ((uint32_t)src[1] << 8) |
			((uint32_t)src[2] << 16) | 0xFF000000U;
		if (px == transColor)
			px &= 0x00FFFFFFU;
		*dst = px;
	}
	return (int32_t)(totalPixels * 4U);
}

static int32_t readIhdr(const uint8_t *ihdr, int32_t *outWidth, int32_t *outHeight,
	int32_t *outFormat, int32_t *outColorType, int32_t *outBitsPerPixel)
{
	uint32_t interlace = ihdr[12];
	if ((ihdr[10] | ihdr[11]) != 0)
		return SCE_PNG_ERROR_NOT_SUPPORTED;
	if (interlace > 1U)
		return SCE_PNG_ERROR_INVALID_FORMAT;

	uint32_t w = ReadBe32(ihdr);
	uint32_t h = ReadBe32(ihdr + 4);
	*outWidth = (int32_t)w;
	*outHeight = (int32_t)h;
	if (w == 0 || h == 0)
		return SCE_PNG_ERROR_INVALID_FORMAT;
	if (w > 65534U || h > 65534U || (w * h) >= 0x10000000U)
		return SCE_PNG_ERROR_NOT_SUPPORTED;

	uint32_t bitDepth = ihdr[8];
	uint32_t colorType = ihdr[9];
	if (((bitDepth - 1U) & bitDepth) != 0 || (bitDepth - 1U) > 15U)
		return SCE_PNG_ERROR_INVALID_FORMAT;

	int32_t state;
	uint32_t bpp = bitDepth;
	if ((colorType & 1U) != 0) {
		if (bitDepth > 8U || colorType != 3U)
			return SCE_PNG_ERROR_INVALID_FORMAT;
		state = 1;
	} else {
		if (colorType > 6U || (colorType != 0 && bitDepth < 8U))
			return SCE_PNG_ERROR_INVALID_FORMAT;
		state = 3;
		bpp = bitDepth + ((((colorType & 4U) != 0) ? 1U : 0U) + (colorType & 2U)) * bitDepth;
	}

	*outFormat = (int32_t)(bpp + (interlace << 12) + ((colorType ^ 2U) << 8));
	*outColorType = (int32_t)colorType;
	*outBitsPerPixel = (int32_t)bpp;
	return state;
}

typedef struct PngIdatStream {
	SceDeflatePartialInputParam param;
	uint8_t *ringBuf;
	uint32_t ringSize;
	const uint8_t *srcPtr;
	uint32_t chunkRemain;
	uint32_t streamRemain;
	int32_t error;
	uint8_t buffer[2048];
} PngIdatStream;

static const void *pngDecAddInputData(SceDeflatePartialInputParam *param, unsigned int outputSize)
{
	(void)outputSize;
	PngIdatStream *st = (PngIdatStream *)param->cookie;
	uint8_t *dst = st->ringBuf;
	uint32_t dstSpace = st->ringSize;
	const uint8_t *src = st->srcPtr;
	uint32_t remain = st->chunkRemain;

	for (;;) {
		while (remain != 0) {
			uint32_t copyLen = (dstSpace >= remain) ? remain : dstSpace;
			sceClibMemcpy_safe(dst, src, copyLen);
			dst += copyLen;
			src += copyLen;
			remain -= copyLen;
			dstSpace -= copyLen;
			st->chunkRemain = remain;
			st->srcPtr = src;
			if (dstSpace == 0) {
				param->pBufEnd = (const void *)(((uintptr_t)(dst + 3)) & ~3U);
				return st->ringBuf;
			}
		}

		int32_t nextLen = (int32_t)ReadBe32(src + 4);
		if (nextLen < 0 || st->streamRemain < (uint32_t)(nextLen + 12)) {
			st->error = SCE_PNG_ERROR_INVALID_FORMAT;
			return NULL;
		}
		uint32_t tag = *(const uint32_t *)(src + 8);
		const uint8_t *chunkData = src + 12;
		src += 12;
		if (tag != 0x54414449U) {
			param->pBufEnd = (const void *)(((uintptr_t)(dst + 3)) & ~3U);
			return st->ringBuf;
		}
		if (sceGzipCrc32(0, chunkData - 4, (uint32_t)(nextLen + 4)) != ReadBe32(chunkData + nextLen)) {
			st->error = SCE_PNG_ERROR_INVALID_FORMAT;
			return NULL;
		}
		st->streamRemain -= (uint32_t)(nextLen + 12);
		remain = (uint32_t)nextLen;
	}
}

int32_t scePngGetOutputInfo(const void *src, uint32_t insize,
	int32_t *width, int32_t *height, int32_t *outputFormat, int32_t *streamFormat)
{
	if (src == NULL || width == NULL || height == NULL ||
		outputFormat == NULL || streamFormat == NULL) {
		return SCE_PNG_ERROR_INVALID_POINTER;
	}
	if (insize < 8U)
		return SCE_PNG_ERROR_INVALID_FORMAT;
	if ((int32_t)(insize - 8U) < 0)
		return SCE_PNG_ERROR_INVALID_SIZE;

	const uint8_t *p = (const uint8_t *)src;
	if (*(const uint32_t *)p != 0x474E5089U || *(const uint32_t *)(p + 4) != 0x0A1A0A0DU)
		return SCE_PNG_ERROR_INVALID_FORMAT;

	p += 8;
	uint32_t remain = insize - 8U;
	int32_t state = 0;
	int32_t colorType = 0;
	int32_t bpp = 0;
	uint64_t outBytes = 0;

	for (;;) {
		int32_t chunkLen = (int32_t)ReadBe32(p);
		if (chunkLen < 0 || remain < (uint32_t)(chunkLen + 12))
			return SCE_PNG_ERROR_INVALID_FORMAT;

		uint32_t tag = *(const uint32_t *)(p + 4);
		const uint8_t *data = p + 8;
		remain -= (uint32_t)(chunkLen + 12);

		if (tag == 0x52444849U) { /* IHDR */
			if (state != 0)
				return SCE_PNG_ERROR_INVALID_FORMAT;
			state = readIhdr(data, width, height, streamFormat, &colorType, &bpp);
			if (state < 0)
				return state;
			uint32_t rowBytes = ((uint32_t)(bpp * (*width)) + 7U) >> 3;
			uint32_t filterOverhead;
			if ((*streamFormat & 0x1000) != 0) {
				uint32_t mult = (bpp <= 7) ? 3U : 2U;
				filterOverhead = (((uint32_t)(*height) + 7U) & ~7U) * mult;
			} else {
				filterOverhead = (uint32_t)(*height);
			}
			if (bpp == 24 && colorType == 2) {
				*outputFormat = *streamFormat + 1032;
				outBytes = (uint64_t)(rowBytes + (uint32_t)(*width)) * (uint64_t)(uint32_t)(*height) + filterOverhead;
			} else {
				*outputFormat = *streamFormat;
				outBytes = (uint64_t)rowBytes * (uint64_t)(uint32_t)(*height) + filterOverhead;
			}
		} else if (tag == 0x45544C50U) { /* PLTE */
			uint32_t entries = (uint32_t)chunkLen / 3U;
			if (state != 1 || entries * 3U != (uint32_t)chunkLen || entries > (1U << bpp))
				return SCE_PNG_ERROR_INVALID_FORMAT;
			state = 3;
			outBytes += (4U << bpp);
		} else if (tag == 0x54414449U) { /* IDAT */
			if (state == 3 && (outBytes >> 31) == 0)
				return (int32_t)outBytes;
			return SCE_PNG_ERROR_INVALID_FORMAT;
		}

		if (sceGzipCrc32(0, p + 4, (uint32_t)(chunkLen + 4)) != ReadBe32(data + chunkLen))
			return SCE_PNG_ERROR_INVALID_FORMAT;
		p = data + chunkLen + 4;
	}
}

static uint8_t PaethPredictor(uint8_t a, uint8_t b, uint8_t c)
{
	int32_t pa = (int32_t)b - (int32_t)c;
	int32_t pb = (int32_t)a - (int32_t)c;
	int32_t pc = pa + pb;
	if (pa < 0) pa = -pa;
	if (pb < 0) pb = -pb;
	if (pc < 0) pc = -pc;
	if (pa <= pb && pa <= pc)
		return a;
	if (pb <= pc)
		return b;
	return c;
}

int32_t scePngDec(void *dst, uint32_t outsize, const void *src, uint32_t insize,
	int32_t *width, int32_t *height, int32_t *format)
{
	if (dst == NULL || src == NULL || width == NULL || height == NULL || format == NULL)
		return SCE_PNG_ERROR_INVALID_POINTER;
	if (insize < 8U)
		return SCE_PNG_ERROR_INVALID_FORMAT;
	if (((outsize >> 31) | ((insize - 8U) >> 31)) != 0)
		return SCE_PNG_ERROR_INVALID_SIZE;

	const uint8_t *p = (const uint8_t *)src;
	if (*(const uint32_t *)p != 0x474E5089U || *(const uint32_t *)(p + 4) != 0x0A1A0A0DU)
		return SCE_PNG_ERROR_INVALID_FORMAT;

	p += 8;
	uint32_t remain = insize - 8U;
	uint32_t dstRemain = outsize;
	uint8_t *outCur = (uint8_t *)dst;
	int32_t state = 0;
	int32_t colorType = 0;
	int32_t bpp = 0;
	const uint8_t *trnsRgb = NULL;
	uint32_t lastRowBytes = 0;

	for (;;) {
		uint32_t chunkLen = ReadBe32(p);
		if ((int32_t)chunkLen < 0 || remain < chunkLen + 12U)
			return SCE_PNG_ERROR_INVALID_FORMAT;

		uint32_t tag = *(const uint32_t *)(p + 4);
		const uint8_t *data = p + 8;
		remain -= (chunkLen + 12U);

		if (tag == 0x52444849U) { /* IHDR */
			if (state != 0)
				return SCE_PNG_ERROR_INVALID_FORMAT;
			state = readIhdr(data, width, height, format, &colorType, &bpp);
			if (state < 0)
				return state;
			if (sceGzipCrc32(0, p + 4, chunkLen + 4U) != ReadBe32(data + chunkLen))
				return SCE_PNG_ERROR_INVALID_FORMAT;
			p = data + chunkLen + 4U;
		} else if (tag == 0x45544C50U) { /* PLTE */
			if (state != 1)
				return SCE_PNG_ERROR_INVALID_FORMAT;
			uint32_t maxEntries = 1U << bpp;
			uint32_t entries = chunkLen / 3U;
			if (entries * 3U != chunkLen || entries > maxEntries)
				return SCE_PNG_ERROR_INVALID_FORMAT;
			if (dstRemain < maxEntries * 4U)
				return SCE_PNG_ERROR_INVALID_SIZE;
			dstRemain -= maxEntries * 4U;
			uint32_t *clut = (uint32_t *)dst;
			for (uint32_t i = 0; i < entries; i++) {
				clut[i] = ((uint32_t)data[3U * i]) |
					((uint32_t)data[3U * i + 1U] << 8) |
					((uint32_t)data[3U * i + 2U] << 16) | 0xFF000000U;
			}
			for (uint32_t i = entries; i < maxEntries; i++)
				clut[i] = 0;
			outCur = (uint8_t *)&clut[maxEntries];
			state = 3;
			if (sceGzipCrc32(0, p + 4, chunkLen + 4U) != ReadBe32(data + chunkLen))
				return SCE_PNG_ERROR_INVALID_FORMAT;
			p = data + chunkLen + 4U;
		} else if (tag == 0x534E5274U) { /* tRNS */
			if ((*format & 0x100) != 0) {
				if (chunkLen > (1U << bpp))
					return SCE_PNG_ERROR_INVALID_FORMAT;
				uint8_t *clutBytes = (uint8_t *)dst;
				for (uint32_t i = 0; i < chunkLen; i++)
					clutBytes[i * 4U + 3U] = data[i];
			} else {
				if ((*format & 0x400) != 0)
					return SCE_PNG_ERROR_INVALID_FORMAT;
				if (colorType == 2) {
					if (chunkLen != 6U)
						return SCE_PNG_ERROR_INVALID_FORMAT;
					trnsRgb = data;
				} else if (colorType == 0 && chunkLen != 2U) {
					return SCE_PNG_ERROR_INVALID_FORMAT;
				}
			}
			if (sceGzipCrc32(0, p + 4, chunkLen + 4U) != ReadBe32(data + chunkLen))
				return SCE_PNG_ERROR_INVALID_FORMAT;
			p = data + chunkLen + 4U;
		} else if (tag == 0x54414449U) { /* IDAT */
			if (state != 3 || !sceZlibIsValid(data))
				return SCE_PNG_ERROR_INVALID_FORMAT;

			const void *nextSrc = NULL;
			int32_t decLen;
			PngIdatStream stream;
			if (remain >= 8U && *(const uint32_t *)(data + chunkLen + 8U) == 0x54414449U) {
				if (sceGzipCrc32(0, p + 4, chunkLen + 4U) != ReadBe32(data + chunkLen))
					return SCE_PNG_ERROR_INVALID_FORMAT;
				stream.param.size = sizeof(stream.param);
				stream.param.pBufEnd = NULL;
				stream.param.cookie = &stream;
				stream.param.callback = pngDecAddInputData;
				stream.ringBuf = stream.buffer;
				stream.ringSize = sizeof(stream.buffer);
				stream.srcPtr = data + 2;
				stream.chunkRemain = chunkLen - 2U;
				stream.streamRemain = remain;
				stream.error = 0;

				const void *firstChunk = pngDecAddInputData(&stream.param, 0);
				decLen = sceDeflateDecompressPartial(outCur, dstRemain, firstChunk, &nextSrc, &stream.param);
				if (stream.error < 0)
					return stream.error;
				if (decLen < 0)
					return decLen;
				state = 8;
				remain = stream.streamRemain;
				p = stream.srcPtr + 4;
			} else {
				decLen = sceDeflateDecompress(outCur, dstRemain, data + 2, &nextSrc);
				if (decLen < 0)
					return decLen;
				if ((const uint8_t *)nextSrc > data + chunkLen)
					return SCE_PNG_ERROR_INVALID_FORMAT;
				state = 4;
			}

			uint8_t *rawRow = outCur;
			uint32_t adler = 1;
			uint32_t bppBytes = (uint32_t)(bpp + 7) >> 3;
			int32_t passStart = ((*format & 0x1000) != 0) ? 0 : 7;

			for (int32_t pass = passStart; pass < 8; pass++) {
				int32_t passH = (*height + (int32_t)s_adam7BiasY[pass]) >> s_adam7ShiftY[pass];
				int32_t passW = (*width + (int32_t)s_adam7BiasX[pass]) >> s_adam7ShiftX[pass];
				if (passH == 0 || passW == 0)
					continue;
				uint32_t rowBytes = ((uint32_t)(bpp * passW) + 7U) >> 3;
				lastRowBytes = rowBytes;
				decLen -= passH * (int32_t)(rowBytes + 1U);
				if (decLen < 0)
					return SCE_PNG_ERROR_INVALID_FORMAT;

				uint8_t *prevRow = NULL;
				for (int32_t y = 0; y < passH; y++) {
					adler = sceZlibAdler32(adler, rawRow, rowBytes + 1U);
					uint8_t filter = rawRow[0];
					uint8_t *srcRow = rawRow + 1;
					uint8_t *dstRow = outCur;

					if (filter == 0) {
						sceClibMemmove(dstRow, srcRow, rowBytes);
					} else if (filter == 1) {
						for (uint32_t x = 0; x < rowBytes; x++) {
							uint8_t left = (x >= bppBytes) ? dstRow[x - bppBytes] : 0;
							dstRow[x] = (uint8_t)(srcRow[x] + left);
						}
					} else if (filter == 2) {
						for (uint32_t x = 0; x < rowBytes; x++) {
							uint8_t up = (prevRow != NULL) ? prevRow[x] : 0;
							dstRow[x] = (uint8_t)(srcRow[x] + up);
						}
					} else if (filter == 3) {
						for (uint32_t x = 0; x < rowBytes; x++) {
							uint8_t left = (x >= bppBytes) ? dstRow[x - bppBytes] : 0;
							uint8_t up = (prevRow != NULL) ? prevRow[x] : 0;
							dstRow[x] = (uint8_t)(srcRow[x] + (((uint32_t)left + (uint32_t)up) >> 1));
						}
					} else if (filter == 4) {
						for (uint32_t x = 0; x < rowBytes; x++) {
							uint8_t left = (x >= bppBytes) ? dstRow[x - bppBytes] : 0;
							uint8_t up = (prevRow != NULL) ? prevRow[x] : 0;
							uint8_t upLeft = (prevRow != NULL && x >= bppBytes) ? prevRow[x - bppBytes] : 0;
							dstRow[x] = (uint8_t)(srcRow[x] + PaethPredictor(left, up, upLeft));
						}
					} else {
						return SCE_PNG_ERROR_INVALID_FORMAT;
					}

					prevRow = dstRow;
					outCur += rowBytes;
					rawRow += rowBytes + 1U;
				}
			}

			if (decLen != 0 || adler != ReadBe32(nextSrc))
				return SCE_PNG_ERROR_INVALID_FORMAT;

			if (state != 8) {
				if (sceGzipCrc32(0, p + 4, chunkLen + 4U) != ReadBe32(data + chunkLen))
					return SCE_PNG_ERROR_INVALID_FORMAT;
				p = data + chunkLen + 4U;
			}
		} else {
			if (sceGzipCrc32(0, p + 4, chunkLen + 4U) != ReadBe32(data + chunkLen))
				return SCE_PNG_ERROR_INVALID_FORMAT;
			if (tag == 0x444E4549U) { /* IEND */
				if (state <= 3)
					return SCE_PNG_ERROR_INVALID_FORMAT;
				if (bpp != 24 || colorType != 2)
					return (int32_t)(outCur - (uint8_t *)dst);
				uint64_t need = (uint64_t)((uint32_t)(*width) + lastRowBytes + 1U) * (uint64_t)(uint32_t)(*height);
				if ((need >> 32) != 0 || dstRemain < (uint32_t)need)
					return SCE_PNG_ERROR_INVALID_SIZE;
				uint32_t transKey = 0;
				if (trnsRgb != NULL && (trnsRgb[0] | trnsRgb[2] | trnsRgb[4]) == 0)
					transKey = (uint32_t)trnsRgb[5] | ((uint32_t)trnsRgb[3] << 8) |
						((uint32_t)trnsRgb[1] << 16) | 0xFF000000U;
				*format += 1032;
				return _scePngConvert24to32(dst, *height, *width, transKey);
			}
			p = data + chunkLen + 4U;
		}
	}
}

int32_t scePngConvertToRGBA(void *dst, const void *src, int32_t width, int32_t height, int32_t format)
{
	if (format == 1056)
		return SCE_PNG_ERROR_INVALID_FORMAT;
	if (dst == NULL || src == NULL)
		return SCE_PNG_ERROR_INVALID_POINTER;

	uint32_t bpp = (uint32_t)(format & 0xFF);
	const uint32_t *palette = NULL;
	const uint8_t *pixels = NULL;
	uint32_t grayPalette[256];

	if ((format & 0x100) != 0) {
		if (((bpp - 1U) & bpp) != 0 || (bpp - 1U) > 7U)
			return SCE_PNG_ERROR_INVALID_FORMAT;
		palette = (const uint32_t *)src;
		pixels = (const uint8_t *)src + (4U << bpp);
	} else if ((format & 0x200) != 0) {
		if ((format & 0x400) != 0)
			return SCE_PNG_ERROR_NOT_SUPPORTED;
		if (((bpp - 1U) & bpp) != 0 || (bpp - 1U) > 15U)
			return SCE_PNG_ERROR_INVALID_FORMAT;
		uint32_t bits = (bpp >= 8U) ? 8U : bpp;
		uint32_t count = 1U << bits;
		for (uint32_t i = 0; i < count; i++) {
			uint32_t v = i << (8U - bits);
			uint32_t g = v | (v >> bits);
			grayPalette[i] = 0xFF000000U | (g * 0x010101U);
		}
		palette = grayPalette;
		pixels = (const uint8_t *)src;
	} else if (format == 5152) {
		pixels = (const uint8_t *)src;
	} else {
		return SCE_PNG_ERROR_NOT_SUPPORTED;
	}

	uint32_t *out = (uint32_t *)dst;
	if ((format & 0x1000) != 0) {
		for (int32_t pass = 0; pass < 7; pass++) {
			int32_t passW = (width + (int32_t)s_adam7BiasX[pass]) >> s_adam7ShiftX[pass];
			int32_t passH = (height + (int32_t)s_adam7BiasY[pass]) >> s_adam7ShiftY[pass];
			if (passW <= 0 || passH <= 0)
				continue;
			uint32_t rowBytes = ((uint32_t)(bpp * (uint32_t)passW) + 7U) >> 3;
			for (int32_t y = 0; y < passH; y++) {
				int32_t dstY = (int32_t)s_adam7StartY[pass] + y * (int32_t)s_adam7StepY[pass];
				for (int32_t x = 0; x < passW; x++) {
					int32_t dstX = (int32_t)s_adam7StartX[pass] + x * (int32_t)s_adam7StepX[pass];
					uint32_t val;
					if (bpp == 32U) {
						val = ((const uint32_t *)pixels)[x];
					} else if (bpp >= 8U) {
						val = palette[pixels[(uint32_t)x * (bpp >> 3)]];
					} else {
						uint32_t shift = (uint32_t)(8U - bpp - (((uint32_t)x * bpp) & 7U));
						uint32_t idx = (pixels[((uint32_t)x * bpp) >> 3] >> shift) & ((1U << bpp) - 1U);
						val = palette[idx];
					}
					out[dstY * width + dstX] = val;
				}
				pixels += rowBytes;
			}
		}
		return 0;
	}

	if ((format & 0x300) == 0)
		return SCE_PNG_ERROR_NOT_SUPPORTED;

	for (int32_t y = 0; y < height; y++) {
		uint32_t *rowOut = &out[y * width];
		uint32_t rowBytes = ((uint32_t)(bpp * (uint32_t)width) + 7U) >> 3;
		if (bpp == 1U) {
			for (int32_t x = 0; x < width; x++)
				rowOut[x] = palette[(pixels[x >> 3] >> (7 - (x & 7))) & 1U];
		} else if (bpp == 2U) {
			for (int32_t x = 0; x < width; x++)
				rowOut[x] = palette[(pixels[x >> 2] >> (6 - 2 * (x & 3))) & 3U];
		} else if (bpp == 4U) {
			for (int32_t x = 0; x < width; x++)
				rowOut[x] = palette[(pixels[x >> 1] >> (4 - 4 * (x & 1))) & 0xFU];
		} else if (bpp == 8U || bpp == 16U) {
			uint32_t stride = bpp >> 3;
			for (int32_t x = 0; x < width; x++)
				rowOut[x] = palette[pixels[(uint32_t)x * stride]];
		} else {
			return SCE_PNG_ERROR_INVALID_FORMAT;
		}
		pixels += rowBytes;
	}
	return 0;
}
