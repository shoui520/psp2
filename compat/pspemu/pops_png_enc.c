/*
 * pops_png_enc.c: ScePspemu lightweight RGBA-to-PNG encoder using zlib 1.2.5
 * deflate and libSceDeflt CRC32 (0x81030704..0x81030e3b).
 */
#include "host_common.h"

#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define PNG_ENC_ERR_ALREADY_INIT        ((int)0x80111201)
#define PNG_ENC_ERR_NOT_INIT            ((int)0x80111203)
#define PNG_ENC_ERR_ALREADY_OPEN        ((int)0x80111204)
#define PNG_ENC_ERR_NO_MEMORY           ((int)0x80111205)
#define PNG_ENC_ERR_UNSUPPORTED_FMT     ((int)0x80111206)
#define PNG_ENC_ERR_INVALID_RECT        ((int)0x80111207)
#define PNG_ENC_ERR_INVALID_ARG         ((int)0x80111208)
#define PNG_ENC_ERR_UNSUPPORTED_TYPE    ((int)0x80111209)
#define PNG_ENC_ERR_INVALID_HANDLE      ((int)0x8011120A)
#define PNG_ENC_ERR_DEFLATE_INIT        ((int)0x80111217)
#define PNG_ENC_ERR_DEFLATE_RUN         ((int)0x80111218)

#define PNG_ENC_IDAT_BUF_SIZE           0x8000U
#define PNG_ENC_IDAT_CHUNK_CAP          (PNG_ENC_IDAT_BUF_SIZE - 4U)

typedef struct PngEncAllocator {
	void *(*allocFn)(size_t size);
	void *(*memalignFn)(size_t align, size_t size);
	void (*freeFn)(void *ptr);
} PngEncAllocator;

typedef struct PngEncHandle {
	uint8_t *filteredRows;
	size_t filteredBytes;
	uint32_t reservedA;
	uint32_t reservedB;
	uint32_t reservedC;
	uint32_t reservedD;
} PngEncHandle;

_Static_assert(sizeof(PngEncHandle) == 0x18U, "PngEncHandle size");

typedef struct PngEncParam {
	int32_t imageType;
	int32_t reservedFlag;
	const uint8_t *srcPixels;
	uint8_t *dstPng;
	int32_t srcStride;
	int32_t srcFormat;
	int32_t cropX;
	int32_t cropY;
	int32_t width;
	int32_t height;
} PngEncParam;

_Static_assert(sizeof(PngEncParam) == 0x28U, "PngEncParam size");

typedef struct PngEncSession {
	PngEncHandle *handle;
	PngEncParam param;
} PngEncSession;

_Static_assert(sizeof(PngEncSession) == 0x2CU, "PngEncSession size");

typedef struct PngEncRgbPixel {
	uint32_t red;
	uint32_t green;
	uint32_t blue;
} PngEncRgbPixel;

typedef struct PngEncIhdrChunk {
	uint32_t chunkType;
	uint32_t widthBe;
	uint32_t heightBe;
	uint8_t bitDepth;
	uint8_t colorType;
	uint8_t compression;
	uint8_t filterMethod;
	uint8_t interlace;
} __attribute__((packed)) PngEncIhdrChunk;

_Static_assert(sizeof(PngEncIhdrChunk) == 17U, "PngEncIhdrChunk size");

unsigned int sceGzipCrc32(unsigned int crc, const unsigned char *src, unsigned int size);

static PngEncSession *g_pngEncActiveSession;
static PngEncAllocator g_pngEncAllocator;
static int g_pngEncInitialized;
static int g_pngEncReservedState;

static int __attribute__((noinline, noclone))
pngEncPrepareRows(const PngEncAllocator *alloc, PngEncSession *session);
static void __attribute__((noinline, noclone))
pngEncFreeRows(const PngEncAllocator *alloc, void *rows, int height);
static int __attribute__((noinline, noclone))
pngEncWriteStream(const PngEncAllocator *alloc, PngEncSession *session);

/* 0x81030704 */
static int __attribute__((noinline)) pngEncTerm(void)
{
	if (g_pngEncInitialized == 0)
		return PNG_ENC_ERR_NOT_INIT;

	if (g_pngEncActiveSession != NULL) {
		pngEncFreeRows(&g_pngEncAllocator,
		               g_pngEncActiveSession->handle->filteredRows,
		               g_pngEncActiveSession->param.height);
		g_pngEncAllocator.freeFn(g_pngEncActiveSession);
		g_pngEncActiveSession = NULL;
	}

	g_pngEncAllocator.allocFn = NULL;
	g_pngEncAllocator.memalignFn = NULL;
	g_pngEncAllocator.freeFn = NULL;
	g_pngEncInitialized = 0;
	return 0;
}

/* 0x81030764 */
static int __attribute__((noinline, noclone))
pngEncOpen(const PngEncParam *param, PngEncHandle *handle)
{
	PngEncSession *session;
	int ret;

	if (g_pngEncInitialized == 0)
		return PNG_ENC_ERR_NOT_INIT;
	if (handle == NULL || param == NULL || param->srcPixels == NULL)
		return PNG_ENC_ERR_INVALID_ARG;
	if (g_pngEncActiveSession != NULL)
		return PNG_ENC_ERR_ALREADY_OPEN;

	session = (PngEncSession *)g_pngEncAllocator.allocFn(sizeof(PngEncSession));
	if (session == NULL)
		return PNG_ENC_ERR_NO_MEMORY;

	memset(session, 0, sizeof(*session));
	session->param = *param;
	session->handle = handle;

	ret = pngEncPrepareRows(&g_pngEncAllocator, session);
	if (ret != 0) {
		g_pngEncAllocator.freeFn(session);
		memset(handle, 0, sizeof(*handle));
		return ret;
	}

	g_pngEncActiveSession = session;
	return 0;
}

/* 0x81030824 */
static int __attribute__((noinline, noclone)) pngEncExecute(PngEncHandle *handle)
{
	if (g_pngEncInitialized == 0)
		return PNG_ENC_ERR_NOT_INIT;
	if (handle == NULL)
		return PNG_ENC_ERR_INVALID_ARG;
	if (g_pngEncActiveSession != NULL && g_pngEncActiveSession->handle == handle)
		return pngEncWriteStream(&g_pngEncAllocator, g_pngEncActiveSession);
	return PNG_ENC_ERR_INVALID_HANDLE;
}

/* 0x81030874 */
static int __attribute__((noinline, noclone)) pngEncClose(PngEncHandle *handle)
{
	if (g_pngEncInitialized == 0)
		return PNG_ENC_ERR_NOT_INIT;
	if (g_pngEncActiveSession == NULL || g_pngEncActiveSession->handle != handle)
		return PNG_ENC_ERR_INVALID_HANDLE;

	pngEncFreeRows(&g_pngEncAllocator,
	               g_pngEncActiveSession->handle->filteredRows,
	               g_pngEncActiveSession->param.height);
	g_pngEncAllocator.freeFn(g_pngEncActiveSession);
	g_pngEncActiveSession = NULL;
	return 0;
}

/* 0x810308c8 */
int popsPngEncodeRgba(void *dstPng, const void *srcRgba, int width, int height,
                      int stride, int flags)
{
	PngEncHandle handle;
	PngEncParam param;
	int ret;

	param.imageType = 1;
	param.reservedFlag = 0;
	param.srcPixels = (const uint8_t *)srcRgba;
	param.dstPng = (uint8_t *)dstPng;
	param.srcStride = stride;
	param.srcFormat = flags;
	param.cropX = 0;
	param.cropY = 0;
	param.width = width;
	param.height = height;

	if (g_pngEncInitialized != 0)
		return PNG_ENC_ERR_ALREADY_INIT;

	g_pngEncInitialized = 1;
	g_pngEncReservedState = 0;
	memset(&handle, 0, sizeof(handle));
	g_pngEncActiveSession = NULL;
	g_pngEncAllocator.allocFn = malloc;
	g_pngEncAllocator.memalignFn = NULL;
	g_pngEncAllocator.freeFn = free;

	ret = pngEncOpen(&param, &handle);
	if (ret == 0)
		ret = pngEncExecute(&handle);
	pngEncClose(&handle);
	pngEncTerm();
	return ret;
}

/* 0x81030984 */
static const uint32_t *__attribute__((noinline, noclone))
pngEncGetPixelAddr(const uint32_t *base, int col, int cropX, int row, int cropY, int stride)
{
	return &base[(col + cropX) + stride * (row + cropY)];
}

/* 0x8103099c */
static uint32_t __attribute__((noinline, noclone))
pngEncUnpackRgba(const uint32_t *pixel, PngEncRgbPixel *outRgb)
{
	uint32_t word = *pixel;
	uint32_t green = (word >> 8) & 0xFFU;

	outRgb->red = word & 0xFFU;
	outRgb->green = green;
	outRgb->blue = (word >> 16) & 0xFFU;
	return green;
}

/* 0x810309b0 */
static uint8_t *__attribute__((noinline, noclone))
pngEncStoreRgb(uint8_t *dstRows, int col, int row, int width, int height,
               const PngEncRgbPixel *rgb)
{
	size_t pos = (size_t)row + (size_t)(3 * width * row) + (size_t)(3 * col) + 1U;
	uint8_t *out = &dstRows[pos];

	(void)height;
	out[0] = (uint8_t)rgb->red;
	out[1] = (uint8_t)rgb->green;
	out[2] = (uint8_t)rgb->blue;
	return out;
}

/* 0x810309d8 */
static uint8_t *__attribute__((noinline, noclone))
pngEncAllocRows(const PngEncAllocator *alloc, size_t *outSize, int width, int height)
{
	size_t totalBytes = (size_t)height + (size_t)(3 * width * height);
	uint8_t *rows;

	*outSize = 0U;
	rows = (uint8_t *)alloc->allocFn(totalBytes);
	if (rows != NULL) {
		memset(rows, 0, totalBytes);
		*outSize = totalBytes;
	}
	return rows;
}

/* 0x81030a00 */
static int __attribute__((noinline, noclone))
pngEncPrepareRows(const PngEncAllocator *alloc, PngEncSession *session)
{
	const PngEncParam *param = &session->param;
	size_t allocSize = 0U;
	uint8_t *rows;
	int row;
	int col;

	if (param->cropX < 0 || param->width <= 0 || param->cropY < 0 || param->height <= 0 ||
	    4 * param->srcStride * (param->cropY + param->height) <= 0) {
		return PNG_ENC_ERR_INVALID_RECT;
	}

	if (param->imageType != 1)
		return PNG_ENC_ERR_UNSUPPORTED_TYPE;
	if (param->srcFormat != 0)
		return PNG_ENC_ERR_UNSUPPORTED_FMT;

	rows = pngEncAllocRows(alloc, &allocSize, param->width, param->height);
	if (rows == NULL)
		return PNG_ENC_ERR_NO_MEMORY;

	for (row = 0; row < param->height; row++) {
		for (col = 0; col < param->width; col++) {
			PngEncRgbPixel rgb;
			const uint32_t *pix = pngEncGetPixelAddr(
				(const uint32_t *)(const void *)param->srcPixels,
				col, param->cropX, row, param->cropY, param->srcStride);
			pngEncUnpackRgba(pix, &rgb);
			pngEncStoreRgb(rows, col, row, param->width, param->height, &rgb);
		}
	}

	session->handle->filteredRows = rows;
	session->handle->filteredBytes = allocSize;
	return 0;
}

/* 0x81030b28 */
static void __attribute__((noinline, noclone))
pngEncFreeRows(const PngEncAllocator *alloc, void *rows, int height)
{
	(void)height;
	if (rows != NULL)
		alloc->freeFn(rows);
}

/* 0x81030b34 */
static voidpf pngEncZlibAlloc(voidpf opaque, uInt items, uInt size)
{
	const PngEncAllocator *alloc = (const PngEncAllocator *)opaque;
	return alloc->allocFn((size_t)items * (size_t)size);
}

/* 0x81030b40 */
static void pngEncZlibFree(voidpf opaque, voidpf address)
{
	const PngEncAllocator *alloc = (const PngEncAllocator *)opaque;
	alloc->freeFn(address);
}

/* 0x81030b4c */
static int __attribute__((noinline, noclone))
pngEncWriteStream(const PngEncAllocator *alloc, PngEncSession *session)
{
	static const uint8_t pngSig[8] = { 0x89U, 'P', 'N', 'G', 0x0DU, 0x0AU, 0x1AU, 0x0AU };
	static const uint8_t iendTag[4] = { 'I', 'E', 'N', 'D' };
	PngEncIhdrChunk ihdr;
	z_stream zstr;
	uint8_t *startOut;
	uint8_t *idatBuf;
	uint32_t ihdrLenBe = __builtin_bswap32(13U);
	uint32_t ihdrCrcBe;
	uint32_t rowStride;
	uint32_t rowsLeft;
	const uint8_t *srcCursor;
	uint32_t chunkBytes;

	if (session->param.imageType != 1)
		return PNG_ENC_ERR_UNSUPPORTED_TYPE;

	startOut = session->param.dstPng;

	ihdr.chunkType = 0x52444849U; /* "IHDR" */
	ihdr.widthBe = __builtin_bswap32((uint32_t)session->param.width);
	ihdr.heightBe = __builtin_bswap32((uint32_t)session->param.height);
	ihdr.bitDepth = 8U;
	ihdr.colorType = 2U;
	ihdr.compression = 0U;
	ihdr.filterMethod = 0U;
	ihdr.interlace = 0U;
	ihdrCrcBe = __builtin_bswap32(sceGzipCrc32(0U, (const unsigned char *)&ihdr, sizeof(ihdr)));

	memcpy(session->param.dstPng, pngSig, sizeof(pngSig));
	session->param.dstPng += sizeof(pngSig);
	memcpy(session->param.dstPng, &ihdrLenBe, sizeof(ihdrLenBe));
	session->param.dstPng += sizeof(ihdrLenBe);
	memcpy(session->param.dstPng, &ihdr, sizeof(ihdr));
	session->param.dstPng += sizeof(ihdr);
	memcpy(session->param.dstPng, &ihdrCrcBe, sizeof(ihdrCrcBe));
	session->param.dstPng += sizeof(ihdrCrcBe);

	if (alloc->memalignFn != NULL)
		idatBuf = (uint8_t *)alloc->memalignFn(64U, PNG_ENC_IDAT_BUF_SIZE);
	else
		idatBuf = (uint8_t *)alloc->allocFn(PNG_ENC_IDAT_BUF_SIZE);
	if (idatBuf == NULL)
		return PNG_ENC_ERR_NO_MEMORY;

	memcpy(idatBuf, "IDAT", 4U);
	zstr.zalloc = pngEncZlibAlloc;
	zstr.zfree = pngEncZlibFree;
	zstr.opaque = (voidpf)alloc;

	if (deflateInit2_(&zstr, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15, 1,
	                  Z_DEFAULT_STRATEGY, "1.2.5", (int)sizeof(z_stream)) != Z_OK) {
		alloc->freeFn(idatBuf);
		return PNG_ENC_ERR_DEFLATE_INIT;
	}

	rowStride = (uint32_t)(3 * session->param.width + 1);
	rowsLeft = (uint32_t)session->param.height;
	srcCursor = session->handle->filteredRows;
	chunkBytes = 0U;
	zstr.next_in = NULL;
	zstr.avail_in = 0U;
	zstr.next_out = &idatBuf[4];
	zstr.avail_out = PNG_ENC_IDAT_CHUNK_CAP;

	for (;;) {
		int flushMode = Z_NO_FLUSH;
		int zret;

		if (zstr.avail_in == 0U) {
			uint32_t rowsPerChunk = PNG_ENC_IDAT_CHUNK_CAP / rowStride;
			uint32_t takeRows;

			srcCursor += chunkBytes;
			zstr.next_in = (Bytef *)srcCursor;
			if (rowsPerChunk < rowsLeft) {
				takeRows = rowsPerChunk;
				rowsLeft -= rowsPerChunk;
			} else {
				takeRows = rowsLeft;
				rowsLeft = 0U;
				flushMode = Z_FINISH;
			}
			chunkBytes = takeRows * rowStride;
			zstr.avail_in = chunkBytes;
		}

		zret = deflate(&zstr, flushMode);
		if (zstr.avail_out == 0U || zret == Z_STREAM_END) {
			uint32_t dataLen = PNG_ENC_IDAT_CHUNK_CAP - zstr.avail_out;
			uint32_t totalLen = PNG_ENC_IDAT_BUF_SIZE - zstr.avail_out;
			uint32_t lenBe = __builtin_bswap32(dataLen);
			uint32_t crcBe = __builtin_bswap32(sceGzipCrc32(0U, idatBuf, totalLen));

			memcpy(session->param.dstPng, &lenBe, sizeof(lenBe));
			session->param.dstPng += sizeof(lenBe);
			memcpy(session->param.dstPng, idatBuf, totalLen);
			session->param.dstPng += totalLen;
			memcpy(session->param.dstPng, &crcBe, sizeof(crcBe));
			session->param.dstPng += sizeof(crcBe);

			if (zret == Z_STREAM_END) {
				uint32_t zeroLen = 0U;
				uint32_t iendCrcBe = __builtin_bswap32(sceGzipCrc32(0U, iendTag, sizeof(iendTag)));

				deflateEnd(&zstr);
				alloc->freeFn(idatBuf);

				memcpy(session->param.dstPng, &zeroLen, sizeof(zeroLen));
				session->param.dstPng += sizeof(zeroLen);
				memcpy(session->param.dstPng, iendTag, sizeof(iendTag));
				session->param.dstPng += sizeof(iendTag);
				memcpy(session->param.dstPng, &iendCrcBe, sizeof(iendCrcBe));
				session->param.dstPng += sizeof(iendCrcBe);
				return (int)(session->param.dstPng - startOut);
			}

			zstr.next_out = &idatBuf[4];
			zstr.avail_out = PNG_ENC_IDAT_CHUNK_CAP;
		} else if (zret != Z_OK) {
			deflateEnd(&zstr);
			alloc->freeFn(idatBuf);
			return PNG_ENC_ERR_DEFLATE_RUN;
		}
	}
}
