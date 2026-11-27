/*
 * remote_me_jpegdec.c: Media Engine JPEG decode, YCbCr-to-RGBA CSC, and
 * EDRAM work allocator command dispatchers for ScePspemu.
 */
#include "host_common.h"

#include <string.h>

typedef struct MeCscContext {
	uint8_t   colorSpaceMode;
	uint8_t   samplingMode;
	uint16_t  width;
	uint8_t  *dstRgb;
	uint32_t  dstPitch;
	uint32_t  height;
	int32_t   lineStrideBias;
	uint8_t  *yPlane;
	uint32_t  yPitch;
	uint8_t  *uPlane;
	uint32_t  uPitch;
	uint8_t  *vPlane;
	uint32_t  vPitch;
} MeCscContext;

typedef struct MeJpegDecodeHandle {
	uint8_t  headerData[7156];
	uint32_t streamStartGuest;
	uint32_t streamEndGuest;
} MeJpegDecodeHandle;

int      g_meWorkLockState;
uint8_t *g_meJpegYuvBuf;
SceUID   g_meJpegYuvMemId;
int      g_meJpegInited;

int remoteMeUnusedVideoDispatch(int cmd)
{
	switch (cmd) {
	case 32: case 33: case 34: case 35:
	case 36: case 37: case 38: case 39:
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	default:
		pspemuRecordTitleListFlags(16U);
		return 0;
	}
}

int remoteMeWorkDispatch(int cmd, uint32_t argA, uint32_t argB)
{
	switch (cmd) {
	case 384: {
		void *hostPtr = sceJpegWorkAlloc(argA);
		uint32_t guestAddr = 0U;
		sceJpegWorkHostToGuest(hostPtr, &guestAddr);
		return (int)guestAddr;
	}
	case 385: {
		void *hostPtr;
		if (sceJpegWorkGuestToHost(argA, &hostPtr) != 0)
			return -1;
		return sceJpegWorkFree(hostPtr);
	}
	case 386: {
		void *hostPtr = sceJpegWorkAllocZero(argA, argB);
		uint32_t guestAddr = 0U;
		sceJpegWorkHostToGuest(hostPtr, &guestAddr);
		return (int)guestAddr;
	}
	case 387: {
		int prev = g_meWorkLockState;
		g_meWorkLockState = 1;
		return prev;
	}
	case 388: {
		int prev = g_meWorkLockState;
		g_meWorkLockState = 0;
		return prev;
	}
	case 390:
	case 391:
		return 0;
	case 392:
		return sceJpegWorkGetFreeSize();
	default:
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	}
}

static int __attribute__((noinline, noclone)) remoteMeCscInitContext(
	MeCscContext *ctx, void *dstRgb, void *srcYuv, uint32_t width, uint32_t height,
	uint32_t sampling, uint8_t colorSpace, uint32_t dstPitch)
{
	uint32_t mode = sampling & 0xFU;
	if (mode != 5U && mode != 9U && mode != 10U)
		return -1;

	uint32_t shiftV = (sampling & 3U) - 1U;
	uint32_t shiftH = ((sampling >> 2) & 3U) - 1U;
	uint32_t chromaPitch = width >> shiftH;
	uint32_t chromaBlocks = ((((8U << shiftV) + height - 1U) >> shiftV) *
	                         (((8U << shiftH) + width - 1U) >> shiftH));
	uint8_t *yPtr = (uint8_t *)srcYuv;
	uint8_t *uPtr = yPtr + (chromaBlocks << (shiftH + shiftV));

	ctx->colorSpaceMode = colorSpace;
	ctx->samplingMode = (uint8_t)((mode == 5U) ? 5U : sampling);
	ctx->width = (uint16_t)width;
	ctx->dstRgb = (uint8_t *)dstRgb;
	ctx->dstPitch = dstPitch;
	ctx->height = height;
	ctx->lineStrideBias = (int32_t)(width % 1024U) - 1024;
	ctx->yPlane = yPtr;
	ctx->yPitch = width;
	ctx->uPlane = uPtr;
	ctx->uPitch = chromaPitch;
	ctx->vPlane = uPtr + chromaBlocks;
	ctx->vPitch = chromaPitch;
	return (int)((mode == 5U) ? 5U : sampling);
}

int remoteMeJpegFinish(void)
{
	if (g_meJpegYuvMemId > 0) {
		sceKernelFreeMemBlock(g_meJpegYuvMemId);
		g_meJpegYuvMemId = 0;
	}
	int ret = sceJpegFinishMJpeg();
	g_meJpegInited = 0;
	return (ret < 0) ? ret : 0;
}

int remoteMeJpegCsc(void *dstBuf, uint32_t dstSize, void *srcYuv, int pixelFormat, int width, int height, int pitch)
{
	int swFormat;
	switch (pixelFormat) {
	case 0: swFormat = 0; break;
	case 1: swFormat = 1; break;
	case 2: swFormat = 2; break;
	case 3: swFormat = 3; break;
	default: pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	}

	int ret = sceJpegMjpegCsc(dstBuf, srcYuv, (uint32_t)(height + (width << 16)), pitch, swFormat, 514);
	if (ret >= 0) {
		remotefsFlushGuestAddr(dstBuf, dstSize);
		return 0;
	}
	return ret;
}

int remoteMeJpegInit(void)
{
	if (g_meJpegInited != 0)
		return 0;

	g_meJpegYuvMemId = sceKernelAllocMemBlock("ScePspemuJpegYuvFrameBuffer", PSPEMU_MEMBLOCK_CDRAM_RW, 0x100000U, NULL);
	if (g_meJpegYuvMemId < 0) {
		remoteMeJpegFinish();
		return g_meJpegYuvMemId;
	}

	int ret = sceKernelGetMemBlockBase(g_meJpegYuvMemId, (void **)&g_meJpegYuvBuf);
	if (ret < 0 || (ret = sceJpegInitMJpeg(0)) < 0) {
		remoteMeJpegFinish();
		return ret;
	}

	g_meJpegInited = 1;
	return 0;
}

int remoteMeJpegDispatch(int cmd, uint32_t argA, uint32_t argB, uint32_t argC, uint32_t argD, uint32_t argE, uint32_t argF)
{
	(void)argF;

	switch (cmd) {
	case 320: {
		void *ctx = meEdramGetBase(argA);
		const void *srcBuf = remotefsTranslateUserGuestAddr(argB, 1, argC);
		return sceJpegDecInit(ctx, srcBuf, argC, argD, (int)argE);
	}
	case 321:
		return sceJpegDecFinish();
	case 322: {
		MeJpegDecodeHandle *ctx = (MeJpegDecodeHandle *)meEdramGetBase(argA);
		remotefsTranslateUserGuestAddr(ctx->streamStartGuest, 1, ctx->streamEndGuest - ctx->streamStartGuest);
		return sceJpegDecDecode(ctx);
	}
	case 323: {
		void *ctx = meEdramGetBase(argA);
		void *outBuf = meEdramGetBase(argB);
		if (outBuf == NULL)
			outBuf = remotefsTranslateUserGuestAddr(argB, 2, 0U);
		return sceJpegDecClose(ctx, outBuf, (int)argC, (int)argD, (int)argE, (int)argF);
	}
	case 324: {
		const SceUInt8 *srcJpeg = (const SceUInt8 *)remotefsTranslateUserGuestAddr(argA, 1, argB);
		SceUInt8 *dstYuv = (SceUInt8 *)meEdramGetBase(argC);
		if (dstYuv == NULL)
			dstYuv = (SceUInt8 *)remotefsTranslateUserGuestAddr(argC, 2, argD);
		int ret = sceJpegDecGetOutputInfo(srcJpeg, argB, dstYuv, argD, (int)argE);
		if (ret >= 0) {
			int decodeMode = (argE == 1U) ? SCE_JPEG_MJPEG_ANY_SAMPLING : SCE_JPEG_MJPEG_ANY_SAMPLING_WITHOUT_DHT;
			if (sceJpegArmDecodeMJpegYCbCr(srcJpeg, argB, dstYuv, argD, decodeMode, NULL, 0U) < 0)
				ret = SCE_JPEG_ERROR_DECODE_ERROR;
			else
				remotefsFlushGuestAddr(dstYuv, argD);
		}
		return ret;
	}
	case 325:
		return sceJpegDecTerm(meEdramGetBase(argA));
	case 326:
		return sceJpegDecReset(meEdramGetBase(argA));
	case 327:
		return sceJpegDecDelete(meEdramGetBase(argA));
	default:
		pspemuRecordTitleListFlags(16U);
		return 0;
	}
}

int remoteMeCscDispatch(int cmd, uint32_t argA, uint32_t argB, uint32_t argC, uint32_t argD,
                        uint32_t argE, uint32_t argF, uint32_t argG, uint32_t argH)
{
	MeCscContext localCtx;

	switch (cmd) {
	case 336: {
		uint32_t yuvFmt = (argE & 3U) | (((argE >> 2) & 3U) << 8);
		uint32_t height = argC & 0xFFFFU;
		uint32_t width = argC >> 16;
		uint32_t yuvBytes;

		switch (yuvFmt) {
		case 257U: yuvBytes = 3U * width * height; break;
		case 513U: yuvBytes = 2U * width * height; break;
		case 514U: yuvBytes = (3U * width * height) >> 1; break;
		default: pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
		}

		void *dstRgb = meEdramGetBase(argA);
		if (dstRgb == NULL)
			dstRgb = remotefsTranslateUserGuestAddr(argA, 2, 4U * height * argD);
		void *srcYuv = meEdramGetBase(argB);
		if (srcYuv == NULL)
			srcYuv = remotefsTranslateUserGuestAddr(argB, 1, yuvBytes);

		int ret = remoteMeCscInitContext(&localCtx, dstRgb, srcYuv, width, height, argE, 0U, argD);
		if (ret < 0)
			return ret;
		ret = sceJpegMjpegCsc(dstRgb, srcYuv, argC, (int)argD, 0, (int)yuvFmt);
		if (ret < 0)
			return ret;
		remotefsFlushGuestAddr(dstRgb, 4U * height * argD);
		return 0;
	}
	case 337: {
		MeCscContext *ctx = (MeCscContext *)meEdramGetBase(argA);
		uint32_t width = ctx->width;
		uint32_t shiftH = ((ctx->samplingMode >> 2) & 3U) - 1U;
		uint32_t shiftV = (ctx->samplingMode & 3U) - 1U;
		uint32_t stripYBytes = 16U * width;
		uint32_t stripUvBytes = (stripYBytes >> shiftV) >> shiftH;
		uint32_t totalYBytes = ctx->height * width;
		uint32_t uvRowOff = (argB >> shiftH) * (width >> shiftV);
		uint8_t *srcBase = ctx->yPlane;

		memcpy(g_meJpegYuvBuf, srcBase + width * argB, stripYBytes);
		memcpy(g_meJpegYuvBuf + stripYBytes, srcBase + totalYBytes + uvRowOff, stripUvBytes);
		memcpy(g_meJpegYuvBuf + stripYBytes + stripUvBytes,
		       srcBase + totalYBytes + ((totalYBytes >> shiftV) >> shiftH) + uvRowOff,
		       stripUvBytes);

		uint32_t yuvFmt = (argC & 3U) | (((argC >> 2) & 3U) << 8);
		if (yuvFmt != 514U && yuvFmt != 513U && yuvFmt != 257U)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

		uint8_t *dstStrip = ctx->dstRgb;
		int ret = sceJpegMjpegCsc(dstStrip, g_meJpegYuvBuf, (width << 16) + 16U, (int)ctx->dstPitch, 0, (int)yuvFmt);
		if (ret >= 0) {
			uint32_t stripOutBytes = ctx->dstPitch << 6;
			ctx->dstRgb = dstStrip + stripOutBytes;
			remotefsFlushGuestAddr(dstStrip, stripOutBytes);
			return (int)(ctx->height - 16U - argB);
		}
		return ret;
	}
	case 338: {
		MeCscContext *ctx = (MeCscContext *)meEdramGetBase(argA);
		void *dstRgb = meEdramGetBase(argB);
		if (dstRgb == NULL)
			dstRgb = remotefsTranslateUserGuestAddr(argB, 2, 4U * argH * argE);

		uint32_t yuvBytes;
		switch (argF & 0xFU) {
		case 5U:  yuvBytes = 3U * argE * argD; break;
		case 9U:  yuvBytes = 2U * argE * argD; break;
		case 10U: yuvBytes = (3U * argE * argD) / 2U; break;
		default:  return -1;
		}

		void *srcYuv = meEdramGetBase(argC);
		if (srcYuv == NULL)
			srcYuv = remotefsTranslateUserGuestAddr(argC, 1, yuvBytes);
		return remoteMeCscInitContext(ctx, dstRgb, srcYuv, argD, argE, argF, (uint8_t)argG, argH);
	}
	default:
		pspemuRecordTitleListFlags(16U);
		return 0;
	}
}
