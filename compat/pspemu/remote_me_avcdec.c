/*
 * remote_me_avcdec.c: Media Engine H.264/AVC hardware video decoder,
 * asynchronous AU submission thread, CSC/zoom/upscale pipelines, and
 * title-specific AVC compatibility hooks for ScePspemu.
 */
#include "host_common.h"

#include <string.h>

#define AVCDEC_AU_RING_SLOTS          3
#define AVCDEC_AU_SLOT_BYTES          0x200000U
#define AVCDEC_PIXEL_RGBA8888_OPT     0x80000000U
#define AVCDEC_PIXEL_RGBA5650_OPT     0x80000001U
#define AVCDEC_PIXEL_RGBA5551_OPT     0x80000002U

typedef struct AvcdecCscParams {
	uint32_t cropX;
	uint32_t cropY;
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint32_t pixelFormat;
} AvcdecCscParams;

typedef struct AvcdecDecodeOutInfo {
	uint32_t reservedA[2];
	uint32_t frameWidth;
	uint32_t frameHeight;
	uint32_t cropLeft;
	uint32_t cropRight;
	uint32_t cropTop;
	uint32_t cropBottom;
	uint32_t hasFrame;
	uint32_t reservedB;
} AvcdecDecodeOutInfo;

typedef struct AvcdecPlaneOffsets {
	uint32_t yPlaneA;
	uint32_t cbPlaneA;
	uint32_t yPlaneB;
	uint32_t cbPlaneB;
	uint32_t yPlaneC;
	uint32_t cbPlaneC;
	uint32_t yPlaneD;
	uint32_t cbPlaneD;
	uint32_t isFirstFrame;
	uint32_t reservedA;
	uint32_t reservedB;
} AvcdecPlaneOffsets;

typedef struct AvcdecEdramConvReq {
	uint32_t width;
	uint32_t height;
	uint32_t reservedA;
	uint32_t srcAddr;
	uint32_t reservedB[7];
	uint32_t dstAddr;
	uint32_t reservedC[2];
} AvcdecEdramConvReq;

typedef struct AvcdecSeiPicTiming {
	uint8_t  presentFlag;
	uint8_t  padBytes[3];
	uint32_t reservedWord;
	uint32_t picStruct;
	uint32_t extraWords[28];
} AvcdecSeiPicTiming;

/* Initialized flags (0x81067164..0x8106716b) */
int g_avcAsyncDecodeMode = 1;
int g_avcProbeFirstFrameSei = 1;

/* BSS state */
uint8_t                 *g_avcClearedBuf;
void                    *g_avcPrevFrameDst;
int                      g_avcDisableInterlaceDetect;
uint8_t                 *g_avcWorkBuf;
int                      g_avcDynamicAsyncActive;
uint8_t                 *g_avcFrameBuf;
int                      g_avcFrameCount;
int                      g_avcMissingFrameStreak;
int                      g_avcIgnoreAuError;
int                      g_avcChecksumClearLateFrame;
int                      g_avcAuReadIdx;
int                      g_avcSynthPtsMode;
int                      g_avcResumeHoldFrames;
int                      g_avcUseLowLatencyMode;
int                      g_avcResumeSeen;
int                      g_avcUseGuestTimestamps;
int                      g_avcClearBufWhite;
int                      g_avcResumeHoldEnabled;
SceAvcdecCtrl            g_avcCtrl;
int                      g_avcFirstFramePending;
int                      g_avcHasDecoded;
int                      g_avcForceBlackOnMissing;
int                      g_avcDialogWasOpen;
SceUID                   g_avcAuReqSema;
uint8_t                  g_avcAuRingBuf[AVCDEC_AU_RING_SLOTS][AVCDEC_AU_SLOT_BYTES];
int                      g_avcInterlacedStream;
SceKernelLwMutexWork     g_avcDecoderLock;
int                      g_avcRepeatLastFrameMode;
int                      g_avcTimestampValid;
int                      g_avcUseExternalTimestamps;
int                      g_avcLastFrameAvailable;
int                      g_avcAuPending;
int                      g_avcDecoderActive;
int                      g_avcPatchReturnZero;
SceUID                   g_avcAuThreadId;
void                    *g_avcLastFrameDst;
SceUID                   g_avcFrameMemId;
SceUID                   g_avcCallbackId;
int                      g_avcChecksumClearEarlyFrame;
SceAvcdecAu              g_avcAuQueue[AVCDEC_AU_RING_SLOTS];
SceUInt                  g_avcCscDelayUs;
int                      g_avcAuWriteIdx;
int                      g_avcDirectOutputMode;
uint32_t                 g_avcPtsUpper;
uint8_t                 *g_avcYuvFrameBuf;
uint32_t                 g_avcPtsLower;
SceUInt                  g_avcInitDelayUs;
volatile SceInt8         g_avcResumeFlag;
SceUID                   g_avcWorkMemId;
int                      g_avcBlankOnMissing;
int                      g_avcUseAuThread;
int                      g_avcOutputCount;
int                      g_avcDisableDmacOnChecksum;
int                      g_avcPatchReturnZeroAlt;
SceUID                   g_avcPicoutSema;
void                    *g_avcDirectDstBuf;
SceUID                   g_avcClearedMemId;
SceUID                   g_avcAuDoneSema;
int                      g_avcChecksumMode;
uint32_t                 g_avcDtsUpper;
int                      g_avcIgnoreUnsupportSize;
uint32_t                 g_avcDtsLower;
int                      g_avcAuThreadExit;
SceAvcdecPictureInternal g_avcCurrentPic;
SceUID                   g_avcYuvMemId;

static void __attribute__((noinline, noclone)) avcdecBuildInternalPicture(
	SceAvcdecPictureInternal *dst, const SceAvcdecPicture *src, uint32_t pixelType, void *bufPtr)
{
	memset(dst, 0, sizeof(*dst));
	memcpy(&dst->pixelType, &src->frame, sizeof(SceAvcdecFrame));
	dst->info = src->info;

	uint32_t height = dst->verticalSize;
	if (dst->frameHeight < height)
		height = dst->frameHeight;
	uint32_t lumaBytes = dst->framePitch * height;

	dst->size = sizeof(*dst);
	dst->pixelType = pixelType;
	dst->pPicture[0] = bufPtr;
	dst->yPlane = bufPtr;
	dst->cbPlane = (uint8_t *)bufPtr + lumaBytes;
	dst->crPlane = (uint8_t *)bufPtr + lumaBytes + (lumaBytes >> 2);
}

static int __attribute__((noinline, noclone)) avcdecConvertYuvToRgbHw(
	void *dstRgb, void *srcYuv, const AvcdecCscParams *params)
{
	SceAvcdecPicture dstPic;
	SceAvcdecPicture srcPic;
	SceAvcdecPictureInternal dstInt;
	SceAvcdecPictureInternal srcInt;

	memset(&srcPic, 0, sizeof(srcPic));
	srcPic.size = sizeof(srcPic);
	srcPic.frame.pixelType = SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER;
	srcPic.frame.framePitch = g_avcCurrentPic.framePitch;
	srcPic.frame.frameWidth = params->width;
	srcPic.frame.frameHeight = params->height;
	srcPic.frame.horizontalSize = params->width;
	srcPic.frame.verticalSize = params->height;
	srcPic.frame.pPicture[0] = srcYuv;
	avcdecBuildInternalPicture(&srcInt, &srcPic, 0x40020U, srcYuv);

	if (params->cropX != 0U || params->cropY != 0U) {
		if ((srcInt.pixelType & SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER) != 0U) {
			uint32_t yOff = params->cropY * srcInt.framePitch + params->cropX;
			uint32_t uvOff = (params->cropY >> 1) * srcInt.framePitch + params->cropX;
			srcInt.yPlane = (uint8_t *)srcInt.yPlane + yOff;
			srcInt.cbPlane = (uint8_t *)srcInt.cbPlane + uvOff;
			srcInt.crPlane = NULL;
		} else if ((srcInt.pixelType & SCE_AVCDEC_PIXEL_YUV420_RASTER) != 0U) {
			uint32_t yOff = params->cropY * srcInt.framePitch + params->cropX;
			uint32_t uvOff = (((params->cropY >> 1) * srcInt.framePitch) >> 1) + (params->cropX >> 1);
			srcInt.yPlane = (uint8_t *)srcInt.yPlane + yOff;
			srcInt.cbPlane = (uint8_t *)srcInt.cbPlane + uvOff;
			srcInt.crPlane = (uint8_t *)srcInt.crPlane + uvOff;
		} else {
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
		}
	}

	uint32_t dstFmt;
	if (params->pixelFormat == 0U)
		dstFmt = AVCDEC_PIXEL_RGBA8888_OPT;
	else if (params->pixelFormat == 1U)
		dstFmt = AVCDEC_PIXEL_RGBA5650_OPT;
	else if (params->pixelFormat == 2U)
		dstFmt = AVCDEC_PIXEL_RGBA5551_OPT;
	else
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	dstPic.size = sizeof(dstPic);
	dstPic.frame.pixelType = dstFmt;
	dstPic.frame.framePitch = params->pitch;
	dstPic.frame.frameWidth = params->width;
	dstPic.frame.frameHeight = params->height;
	dstPic.frame.opt.rgba.alpha = 0;
	dstPic.frame.opt.rgba.cscCoefficient = SCE_AVCDEC_CSC_COEFFICIENT_ITU601;
	dstPic.frame.pPicture[0] = dstRgb;
	dstPic.frame.pPicture[1] = NULL;
	avcdecBuildInternalPicture(&dstInt, &dstPic, dstFmt, dstRgb);

	return sceAvcdecCscInternal(&dstInt, &srcInt);
}

static int __attribute__((noinline, noclone)) avcdecLockDecoder(void)
{
	int ret = sceKernelLockLwMutex(&g_avcDecoderLock, 1, NULL);
	if (ret < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	return ret;
}

int pspemuLockLwMutexOrFatal(SceKernelLwMutexWork *work)
{
	int ret = sceKernelLockLwMutex(work, 1, NULL);
	if (ret < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	return ret;
}

static void __attribute__((noinline, noclone)) avcdecResetState(void)
{
	g_avcFirstFramePending = 1;
	g_avcFrameCount = 0;
	g_avcTimestampValid = 0;
	g_avcInterlacedStream = 0;
	g_avcOutputCount = 0;
	g_avcChecksumClearLateFrame = 0;
	if (g_avcDynamicAsyncActive != 0) {
		g_avcUseAuThread = 0;
		g_avcProbeFirstFrameSei = 1;
		g_avcForceBlackOnMissing = 0;
	}
	g_avcDynamicAsyncActive = 0;
	if (g_avcDisableDmacOnChecksum != 0)
		remoteDmacplusSetEnabled(1);
	g_avcChecksumClearEarlyFrame = 0;
	g_avcDisableDmacOnChecksum = 0;
	g_avcUseGuestTimestamps = 0;
	g_avcHasDecoded = 0;
}

static int __attribute__((noinline, noclone)) avcdecDrainPicoutSema(void)
{
	for (int i = 0; i < 4; i++) {
		SceUInt timeout = 0;
		sceKernelWaitSema(g_avcPicoutSema, 1, &timeout);
	}
	return 0;
}

static int __attribute__((noinline, noclone)) avcdecSubmitAu(const SceAvcdecAu *au)
{
	int status = 0;

	if (g_avcUseAuThread == 0)
		return sceAvcdecDecodeAuInternal(&g_avcCtrl, au, &status);

	if (sceKernelWaitSema(g_avcAuDoneSema, 1, NULL) < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	if (au == NULL || au->es.size > 0x600000U) {
		if (sceKernelSignalSema(g_avcAuDoneSema, 1) < 0)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
		return -1;
	}

	int slot = g_avcAuWriteIdx;
	g_avcAuPending = 1;
	g_avcAuQueue[slot] = *au;
	uint8_t *slotBuf = g_avcAuRingBuf[slot];
	memcpy(slotBuf, au->es.pBuf, au->es.size);
	g_avcAuQueue[slot].es.pBuf = slotBuf;
	g_avcAuWriteIdx = (slot + 1) % AVCDEC_AU_RING_SLOTS;

	if (sceKernelSignalSema(g_avcAuReqSema, 1) < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	return 0;
}

static int avcdecAuInputThread(SceSize args, void *argp)
{
	(void)args;
	(void)argp;

	for (;;) {
		if (sceKernelWaitSema(g_avcAuReqSema, 1, NULL) < 0)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
		if (g_avcAuThreadExit != 0)
			break;
		if (g_avcAuPending != 0) {
			int status = 0;
			sceAvcdecDecodeAuInternal(&g_avcCtrl, &g_avcAuQueue[g_avcAuReadIdx], &status);
		}
		g_avcAuReadIdx = (g_avcAuReadIdx + 1) % AVCDEC_AU_RING_SLOTS;
		if (sceKernelSignalSema(g_avcAuDoneSema, 1) < 0)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	}
	return 0;
}

static int avcdecPicoutCallback(int notifyId, int notifyCount, int notifyArg, void *common)
{
	(void)notifyId;
	(void)notifyCount;
	(void)notifyArg;
	(void)common;
	sceKernelSignalSema(g_avcPicoutSema, 1);
	return 0;
}

static int __attribute__((noinline, noclone)) avcdecUnlockDecoder(void)
{
	int ret = sceKernelUnlockLwMutex(&g_avcDecoderLock, 1);
	if (ret < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	return ret;
}

int pspemuUnlockLwMutexOrFatal(SceKernelLwMutexWork *work)
{
	int ret = sceKernelUnlockLwMutex(work, 1);
	if (ret < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	return ret;
}

static int __attribute__((noinline, noclone)) avcdecCleanup(int flags)
{
	int ret = 0;
	g_avcAuThreadExit = 1;

	if (g_avcAuReqSema > 0 && g_avcAuThreadId > 0) {
		sceKernelSignalSema(g_avcAuReqSema, 1);
		SceUInt timeout = 2000000U;
		sceKernelWaitThreadEnd(g_avcAuThreadId, NULL, &timeout);
		sceKernelDeleteThread(g_avcAuThreadId);
		g_avcAuThreadId = 0;
	}
	if (g_avcAuReqSema != 0) {
		sceKernelDeleteSema(g_avcAuReqSema);
		g_avcAuReqSema = 0;
	}
	if (g_avcAuDoneSema != 0) {
		sceKernelDeleteSema(g_avcAuDoneSema);
		g_avcAuDoneSema = 0;
	}
	if (g_avcCallbackId > 0) {
		int cbRet = sceKernelDeleteCallback(g_avcCallbackId);
		if (cbRet < 0)
			ret = cbRet;
		g_avcCallbackId = 0;
	}
	if (g_avcPicoutSema > 0) {
		int semaRet = sceKernelDeleteSema(g_avcPicoutSema);
		if (semaRet < 0)
			ret = semaRet;
		g_avcPicoutSema = 0;
	}
	int mtxRet = sceKernelDeleteLwMutex(&g_avcDecoderLock);
	if (mtxRet != 0)
		ret = mtxRet;

	if ((flags & 2) != 0) {
		sceAvcdecUnregisterCallbackInternal(&g_avcCtrl);
		int delRet = sceAvcdecDeleteDecoder(&g_avcCtrl);
		if (delRet < 0)
			ret = delRet;
	}
	if ((flags & 1) != 0) {
		int termRet = sceVideodecTermLibrary(SCE_VIDEODEC_TYPE_HW_AVCDEC);
		if (termRet < 0)
			ret = termRet;
	}
	if (g_avcFrameMemId > 0) {
		int freeRet = sceKernelFreeMemBlock(g_avcFrameMemId);
		if (freeRet < 0)
			ret = freeRet;
		g_avcFrameMemId = 0;
	}
	if (g_avcYuvMemId > 0) {
		int freeRet = sceKernelFreeMemBlock(g_avcYuvMemId);
		if (freeRet < 0)
			ret = freeRet;
		g_avcYuvMemId = 0;
	}
	if (g_avcWorkMemId > 0) {
		int freeRet = sceKernelFreeMemBlock(g_avcWorkMemId);
		if (freeRet < 0)
			ret = freeRet;
		g_avcWorkMemId = 0;
	}
	if (g_avcClearedMemId > 0) {
		int freeRet = sceKernelFreeMemBlock(g_avcClearedMemId);
		if (freeRet < 0)
			ret = freeRet;
		g_avcClearedMemId = 0;
	}
	avcdecResetState();
	return ret;
}

static int __attribute__((noinline, noclone)) avcdecOutputCscFrame(
	void *dstRgb, uint32_t dstSize, void *srcYuv, AvcdecCscParams *params)
{
	uint8_t *baseYuv;

	if (g_avcCscDelayUs != 0U && params->cropY == 0U && params->cropX == 0U)
		sceKernelDelayThread(g_avcCscDelayUs);

	if (remoteDmacplusIsEnabled() != 0) {
		baseYuv = (uint8_t *)srcYuv;
	} else {
		uint32_t tileX = params->cropX >> 4;
		if ((params->cropX & 0x10U) == 0U) {
			baseYuv = (uint8_t *)srcYuv - (16U * params->cropY + ((16U * params->height * tileX) >> 2));
		} else {
			baseYuv = (uint8_t *)srcYuv -
				(16U * params->cropY +
				 ((3U * g_avcCurrentPic.framePitch * g_avcCurrentPic.verticalSize) >> 2) +
				 ((16U * params->height * (tileX - 1U)) >> 2));
		}
	}

	if (params->width > 63U && (((uintptr_t)dstRgb) & 0xFU) == 0U) {
		if (params->pixelFormat != 3U)
			return avcdecConvertYuvToRgbHw(dstRgb, baseYuv, params);

		params->pixelFormat = 0U;
		int ret = avcdecConvertYuvToRgbHw(g_avcFrameBuf, baseYuv, params);
		if (ret >= 0) {
			uint32_t pixelCount = params->height * params->pitch;
			if ((pixelCount & 0xFU) != 0U)
				pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

			const uint32_t *srcPx = (const uint32_t *)g_avcFrameBuf;
			uint16_t *dstPx = (uint16_t *)dstRgb;
			for (uint32_t i = 0; i < pixelCount; i++) {
				uint32_t px = srcPx[i];
				dstPx[i] = (uint16_t)(((px >> 4) & 0x000FU) |
				                      ((px >> 8) & 0x00F0U) |
				                      ((px >> 12) & 0x0F00U) |
				                      ((px >> 16) & 0xF000U));
			}
			remotefsFlushGuestAddr(dstRgb, dstSize);
		}
		return ret;
	}

	int swFormat;
	uint32_t bpp;
	switch (params->pixelFormat) {
	case 0U: swFormat = 16; bpp = 4U; break;
	case 1U: swFormat = 17; bpp = 2U; break;
	case 2U: swFormat = 18; bpp = 2U; break;
	case 3U: swFormat = 19; bpp = 2U; break;
	default: pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	}

	uint32_t srcPitch = g_avcCurrentPic.framePitch;
	uint32_t srcVert = g_avcCurrentPic.verticalSize;
	uint32_t width = params->width;
	uint32_t height = params->height;
	uint8_t *yDst = g_avcFrameBuf;
	const uint8_t *ySrc = baseYuv + params->cropY * srcPitch + params->cropX;

	for (uint32_t row = 0; row < height; row++) {
		memcpy(yDst, ySrc, width);
		ySrc += srcPitch;
		yDst += width;
	}

	uint32_t halfH = height >> 1;
	uint32_t halfW = width >> 1;
	if (halfH != 0U) {
		uint32_t lumaSize = width * height;
		uint8_t *uDst = g_avcFrameBuf + lumaSize;
		uint8_t *vDst = uDst + (lumaSize >> 2);
		const uint8_t *uvRow = baseYuv + srcVert * srcPitch + (params->cropY >> 1) * srcPitch + params->cropX;
		for (uint32_t row = 0; row < halfH; row++) {
			const uint8_t *uvSrc = uvRow;
			for (uint32_t col = 0; col < halfW; col++) {
				*uDst++ = uvSrc[0];
				*vDst++ = uvSrc[1];
				uvSrc += 2;
			}
			uvRow += srcPitch;
		}
	}

	int ret = sceJpegMjpegCsc(dstRgb, g_avcFrameBuf, (width << 16) | (height & 0xFFFFU), (int)params->pitch, swFormat, 514);
	if (ret >= 0) {
		uint8_t *flushPtr = (uint8_t *)dstRgb;
		for (uint32_t row = 0; row < params->height; row++) {
			remotefsFlushGuestAddr(flushPtr, params->width * bpp);
			flushPtr += params->pitch * bpp;
		}
	}
	return ret;
}

static int __attribute__((noinline, noclone)) avcdecCopyDecodedFrame(
	SceAvcdecPictureInternal *outPic, const SceAvcdecPicture *inPic, void *srcYuv, void *dstEdram)
{
	SceAvcdecPicture dstPic;
	SceAvcdecPictureInternal srcInt;
	SceAvcdecPictureInternal dstInt;

	memset(&dstPic, 0, sizeof(dstPic));
	dstPic.size = sizeof(dstPic);
	dstPic.frame.pixelType = inPic->frame.pixelType;
	dstPic.frame.framePitch = inPic->frame.horizontalSize;
	dstPic.frame.frameWidth = inPic->frame.horizontalSize;
	dstPic.frame.frameHeight = inPic->frame.verticalSize;
	dstPic.frame.pPicture[0] = dstEdram;

	avcdecBuildInternalPicture(&srcInt, inPic, inPic->frame.pixelType | 0x40000U, srcYuv);
	avcdecBuildInternalPicture(&dstInt, &dstPic, inPic->frame.pixelType, dstEdram);

	int ret = sceAvcdecCscInternal(&dstInt, &srcInt);
	if (ret < 0) {
		if (g_avcIgnoreUnsupportSize == 0)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	} else {
		memcpy(outPic, &dstInt, sizeof(SceAvcdecPicture));
	}
	return ret;
}

static int __attribute__((noinline, noclone)) avcdecDecodeStopInternal(void)
{
	SceAvcdecArrayPicture arrayPic;
	SceAvcdecPicture *picPtr;
	SceAvcdecPicture pic;
	SceAvcdecPicture savedPic;
	SceAvcdecPictureInternal picInt;
	int haveSaved = 0;
	int ret;

	for (;;) {
		memset(&pic, 0, sizeof(pic));
		pic.size = sizeof(pic);
		if (g_avcDirectOutputMode != 0) {
			void *dst = (g_avcDirectDstBuf != NULL) ? g_avcDirectDstBuf : g_avcFrameBuf;
			pic.frame.framePitch = 512U;
			pic.frame.frameWidth = 480U;
			pic.frame.frameHeight = 272U;
			pic.frame.pPicture[0] = dst;
		} else {
			pic.frame.pixelType = SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER;
			pic.frame.framePitch = 480U;
			pic.frame.frameWidth = 480U;
			pic.frame.frameHeight = 272U;
			pic.frame.pPicture[0] = g_avcYuvFrameBuf;
		}
		picPtr = &pic;
		arrayPic.numOfOutput = 0U;
		arrayPic.numOfElm = 1U;
		arrayPic.pPicture = &picPtr;

		ret = sceAvcdecDecodeStop(&g_avcCtrl, &arrayPic);
		if (g_avcAsyncDecodeMode == 0)
			break;
		if (g_avcInterlacedStream == 0) {
			arrayPic.numOfOutput = 0U;
			break;
		}
		if (arrayPic.numOfOutput == 0U) {
			if (haveSaved != 0) {
				pic = savedPic;
				arrayPic.numOfOutput = 1U;
			}
			break;
		}
		haveSaved = 1;
		savedPic = pic;
	}

	if (arrayPic.numOfOutput != 0U) {
		if (g_avcDirectOutputMode != 0) {
			avcdecBuildInternalPicture(&picInt, &pic, 0U, pic.frame.pPicture[0]);
		} else {
			ret = avcdecCopyDecodedFrame(&picInt, &pic, g_avcYuvFrameBuf, meEdramGetBase(0U));
		}
		g_avcLastFrameAvailable = 1;
		g_avcCurrentPic = picInt;
	} else {
		g_avcLastFrameAvailable = 0;
	}

	if (g_avcIgnoreUnsupportSize != 0 && ret == SCE_AVCDEC_ERROR_UNSUPPORT_IMAGE_SIZE) {
		ret = 0;
		g_avcCurrentPic.framePitch = pic.frame.horizontalSize;
		g_avcCurrentPic.horizontalSize = pic.frame.horizontalSize;
		g_avcCurrentPic.verticalSize = pic.frame.verticalSize;
		g_avcLastFrameAvailable = 1;
	}

	avcdecDrainPicoutSema();
	sceAvcdecDecodeFlush(&g_avcCtrl);
	g_avcAuPending = 0;
	avcdecResetState();
	return ret;
}

static int __attribute__((noinline, noclone)) avcdecRepeatLastFrame(
	uint32_t *outNumFrames, SceAvcdecPictureInternal *outPic)
{
	if (g_avcFrameCount <= 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	*outPic = g_avcCurrentPic;
	*outNumFrames = 1U;
	return 0;
}

static int __attribute__((noinline, noclone)) avcdecUpscaleToVitaScreen(void *dstBuf, void *srcBuf)
{
	SceAvcdecPicture srcPic;
	SceAvcdecPicture dstPic;
	SceAvcdecPictureInternal srcInt;
	SceAvcdecPictureInternal dstInt;

	memset(&srcPic, 0, sizeof(srcPic));
	srcPic.size = sizeof(srcPic);
	srcPic.frame.framePitch = 512U;
	srcPic.frame.frameWidth = 480U;
	srcPic.frame.frameHeight = 272U;
	srcPic.frame.pPicture[0] = srcBuf;
	avcdecBuildInternalPicture(&srcInt, &srcPic, 0U, srcBuf);
	srcInt.horizontalSize = srcInt.frameWidth;
	srcInt.verticalSize = srcInt.frameHeight;

	memset(&dstPic, 0, sizeof(dstPic));
	dstPic.size = sizeof(dstPic);
	dstPic.frame.pixelType = 0x80000U;
	dstPic.frame.framePitch = 1024U;
	dstPic.frame.frameWidth = 960U;
	dstPic.frame.frameHeight = 544U;
	dstPic.frame.pPicture[0] = dstBuf;
	avcdecBuildInternalPicture(&dstInt, &dstPic, 0x80000U, dstBuf);

	return sceAvcdecCscInternal(&dstInt, &srcInt);
}

static int __attribute__((noinline, noclone)) avcdecZoomCropFrame(
	void *dstBuf, void *srcBuf, int zoomScale, int cropW, int cropH, int cropX, int cropY)
{
	SceAvcdecPicture srcPic;
	SceAvcdecPicture dstPic;
	SceAvcdecPictureInternal srcInt;
	SceAvcdecPictureInternal dstInt;

	int scaledW = (zoomScale * cropW) / 10;
	int scaledH = (zoomScale * cropH) / 10;

	memset(&srcPic, 0, sizeof(srcPic));
	srcPic.size = sizeof(srcPic);
	srcPic.frame.pixelType = 0x40010U;
	srcPic.frame.framePitch = 480U;
	srcPic.frame.frameWidth = 480U;
	srcPic.frame.frameHeight = 272U;
	srcPic.frame.horizontalSize = 480U;
	srcPic.frame.verticalSize = 272U;
	srcPic.frame.pPicture[0] = srcBuf;
	avcdecBuildInternalPicture(&srcInt, &srcPic, 0x40010U, srcBuf);

	srcInt.frameWidth = (uint32_t)cropW;
	srcInt.horizontalSize = (uint32_t)cropW;
	srcInt.frameHeight = (uint32_t)cropH;
	srcInt.verticalSize = (uint32_t)cropH;
	int uvOff = (cropX / 2) + ((480 * (cropY / 2)) >> 1);
	srcInt.yPlane = (uint8_t *)srcInt.yPlane + cropX + 480 * cropY;
	srcInt.cbPlane = (uint8_t *)srcInt.cbPlane + uvOff;
	srcInt.crPlane = (uint8_t *)srcInt.crPlane + uvOff;

	if (((scaledW | scaledH) & 0xF) != 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	memset(&dstPic, 0, sizeof(dstPic));
	dstPic.size = sizeof(dstPic);
	dstPic.frame.pixelType = 0x80020U;
	dstPic.frame.framePitch = (uint32_t)scaledW;
	dstPic.frame.frameWidth = (uint32_t)scaledW;
	dstPic.frame.frameHeight = (uint32_t)scaledH;
	dstPic.frame.horizontalSize = (uint32_t)scaledW;
	dstPic.frame.verticalSize = (uint32_t)scaledH;
	dstPic.frame.pPicture[0] = g_avcWorkBuf;
	avcdecBuildInternalPicture(&dstInt, &dstPic, 0x80020U, g_avcWorkBuf);

	int ret = sceAvcdecCscInternal(&dstInt, &srcInt);
	if (ret < 0)
		return ret;

	memset(&srcPic, 0, sizeof(srcPic));
	srcPic.size = sizeof(srcPic);
	srcPic.frame.pixelType = 0x40020U;
	srcPic.frame.framePitch = (uint32_t)scaledW;
	srcPic.frame.frameWidth = (uint32_t)scaledW;
	srcPic.frame.frameHeight = (uint32_t)scaledH;
	srcPic.frame.horizontalSize = (uint32_t)scaledW;
	srcPic.frame.verticalSize = (uint32_t)scaledH;
	srcPic.frame.pPicture[0] = g_avcWorkBuf;
	avcdecBuildInternalPicture(&srcInt, &srcPic, 0x40020U, g_avcWorkBuf);

	srcInt.frameWidth = 480U;
	srcInt.horizontalSize = 480U;
	srcInt.frameHeight = 272U;
	srcInt.verticalSize = 272U;
	int yCrop = ((int)(272U * (unsigned int)(zoomScale - 10) / 20U) + 3 - (cropY * zoomScale) / 10) & ~7;
	uint32_t xCrop = ((uint32_t)(480U * (unsigned int)(zoomScale - 10) / 20U) + 7U - (uint32_t)((cropX * zoomScale) / 10)) & ~15U;
	srcInt.yPlane = (uint8_t *)srcInt.yPlane + xCrop + (int)(yCrop * scaledW);
	srcInt.cbPlane = (uint8_t *)srcInt.cbPlane + xCrop + (int)((yCrop >> 1) * scaledW);

	memset(&dstPic, 0, sizeof(dstPic));
	dstPic.size = sizeof(dstPic);
	dstPic.frame.pixelType = SCE_AVCDEC_PIXEL_YUV420_RASTER;
	dstPic.frame.framePitch = 480U;
	dstPic.frame.frameWidth = 480U;
	dstPic.frame.frameHeight = 272U;
	dstPic.frame.horizontalSize = 480U;
	dstPic.frame.verticalSize = 272U;
	dstPic.frame.pPicture[0] = dstBuf;
	avcdecBuildInternalPicture(&dstInt, &dstPic, SCE_AVCDEC_PIXEL_YUV420_RASTER, dstBuf);

	sceAvcdecCscInternal(&dstInt, &srcInt);
	return 0;
}

static void __attribute__((noinline, noclone)) avcdecBuildCurrentPicture(SceAvcdecPicture *dst)
{
	memset(dst, 0, sizeof(*dst));
	dst->size = sizeof(*dst);
	memcpy(&dst->frame, &g_avcCurrentPic.pixelType, sizeof(SceAvcdecFrame));
	dst->info = g_avcCurrentPic.info;
}

static int __attribute__((noinline, noclone)) avcdecConvertEdramYuv(const AvcdecEdramConvReq *req)
{
	SceAvcdecPicture pic;
	SceAvcdecPictureInternal srcInt;
	SceAvcdecPictureInternal dstInt;

	uint32_t yuvBytes = (3U * req->width * req->height) >> 1;
	uint8_t *srcPtr = (uint8_t *)meEdramGetBase(req->srcAddr);
	if (srcPtr == NULL)
		srcPtr = (uint8_t *)remotefsTranslateUserGuestAddr(req->srcAddr, 1, yuvBytes);
	uint8_t *dstPtr = (uint8_t *)meEdramGetBase(req->dstAddr);
	if (dstPtr == NULL)
		dstPtr = (uint8_t *)remotefsTranslateUserGuestAddr(req->dstAddr, 2, yuvBytes);

	if ((g_avcCurrentPic.horizontalSize & 0x10U) != 0U) {
		int lumaPixels = (int)(req->width * req->height);
		int chromaPixels = lumaPixels / 4;
		memcpy(dstPtr, srcPtr, (size_t)lumaPixels);
		const uint8_t *uvSrc = srcPtr + lumaPixels;
		uint8_t *uDst = dstPtr + lumaPixels;
		uint8_t *vDst = uDst + chromaPixels;
		for (int i = 0; i < chromaPixels; i++) {
			uDst[i] = uvSrc[2 * i];
			vDst[i] = uvSrc[2 * i + 1];
		}
		remotefsFlushGuestAddr(dstPtr, yuvBytes);
		return 0;
	}

	avcdecBuildCurrentPicture(&pic);
	avcdecBuildInternalPicture(&srcInt, &pic, pic.frame.pixelType | 0x40000U, srcPtr);
	avcdecBuildInternalPicture(&dstInt, &pic, SCE_AVCDEC_PIXEL_YUV420_RASTER, dstPtr);
	if (sceAvcdecCscInternal(&dstInt, &srcInt) < 0) {
		if (g_avcIgnoreUnsupportSize == 0)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
		return -4;
	}
	return 0;
}

int __attribute__((noinline, noclone)) avcdecClearEdramFrame(void *dstEdram, uint32_t width, uint32_t height)
{
	SceAvcdecPicture srcPic;
	SceAvcdecPicture dstPic;
	SceAvcdecPictureInternal srcInt;
	SceAvcdecPictureInternal dstInt;

	if (width > 640U || height > 480U)
		return -1;

	memset(&srcPic, 0, sizeof(srcPic));
	srcPic.size = sizeof(srcPic);
	srcPic.frame.pixelType = SCE_AVCDEC_PIXEL_BGRA8888;
	srcPic.frame.framePitch = width;
	srcPic.frame.frameWidth = width;
	srcPic.frame.frameHeight = height;
	srcPic.frame.pPicture[0] = g_avcClearedBuf;
	avcdecBuildInternalPicture(&srcInt, &srcPic, SCE_AVCDEC_PIXEL_BGRA8888, g_avcClearedBuf);
	srcInt.horizontalSize = srcInt.frameWidth;
	srcInt.verticalSize = srcInt.frameHeight;

	memset(&dstPic, 0, sizeof(dstPic));
	dstPic.size = sizeof(dstPic);
	dstPic.frame.pixelType = SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER;
	dstPic.frame.framePitch = width;
	dstPic.frame.frameWidth = width;
	dstPic.frame.frameHeight = height;
	dstPic.frame.pPicture[0] = dstEdram;
	avcdecBuildInternalPicture(&dstInt, &dstPic, SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER, dstEdram);

	return sceAvcdecCscInternal(&dstInt, &srcInt);
}

static int __attribute__((noinline, noclone)) avcdecFillDecodeOutputInfo(
	AvcdecDecodeOutInfo *outInfo, AvcdecPlaneOffsets *outPlanes, int isStop)
{
	uint32_t lumaBytes = g_avcCurrentPic.framePitch * g_avcCurrentPic.verticalSize;
	uint32_t quarter = lumaBytes / 4U;
	uint32_t eighth = lumaBytes / 8U;

	outInfo->frameWidth = g_avcCurrentPic.horizontalSize;
	outInfo->frameHeight = g_avcCurrentPic.verticalSize;
	outInfo->cropLeft = g_avcCurrentPic.frameCropLeftOffset;
	outInfo->cropRight = g_avcCurrentPic.frameCropRightOffset;
	outInfo->cropTop = g_avcCurrentPic.frameCropTopOffset;
	outInfo->cropBottom = g_avcCurrentPic.frameCropBottomOffset;
	outInfo->hasFrame = (uint32_t)g_avcLastFrameAvailable;
	outInfo->reservedB = 0U;

	outPlanes->yPlaneA = 0U;
	outPlanes->cbPlaneA = 2U * quarter + 2U * eighth;
	outPlanes->yPlaneB = quarter;
	outPlanes->cbPlaneB = 3U * quarter + 2U * eighth;
	outPlanes->yPlaneC = 2U * quarter;
	outPlanes->cbPlaneC = 4U * quarter + 2U * eighth;
	outPlanes->yPlaneD = 2U * quarter + eighth;
	outPlanes->cbPlaneD = 4U * quarter + 3U * eighth;
	outPlanes->reservedA = 0U;
	outPlanes->reservedB = 0U;

	if (g_avcResumeHoldEnabled != 0) {
		if (sceKernelAtomicGetAndSet8(&g_avcResumeFlag, 0) != 0) {
			g_avcResumeHoldFrames = 20;
			g_avcResumeSeen = 1;
		}
		if (SceCommonDialog_FA26EB30() == SCE_COMMON_DIALOG_STATUS_RUNNING) {
			if (g_avcDialogWasOpen == 0 && g_avcResumeSeen != 0) {
				g_avcDialogWasOpen = 1;
				g_avcResumeHoldFrames = 20;
			}
		} else {
			g_avcDialogWasOpen = 0;
		}

		if (g_avcFrameCount <= 1) {
			g_avcResumeHoldFrames = 0;
		} else if (g_avcResumeHoldFrames > 0) {
			g_avcResumeHoldFrames--;
			outInfo->hasFrame = 0U;
		}
	}

	if (outInfo->hasFrame != 0U && g_avcFirstFramePending != 0 && isStop == 0) {
		g_avcFirstFramePending = 0;
		outPlanes->isFirstFrame = 1U;
	} else {
		outPlanes->isFirstFrame = 0U;
	}

	if (g_avcBlankOnMissing != 0) {
		if (outInfo->hasFrame != 0U) {
			if (g_avcMissingFrameStreak <= 3)
				g_avcMissingFrameStreak = 0;
		} else {
			g_avcMissingFrameStreak++;
			if (g_avcMissingFrameStreak == 2) {
				avcdecClearEdramFrame(meEdramGetBase(0U), outInfo->frameWidth, outInfo->frameHeight);
				outInfo->hasFrame = 1U;
			} else if (g_avcMissingFrameStreak > 2) {
				g_avcMissingFrameStreak = 0;
				outInfo->hasFrame = 3U;
			}
		}
	}

	if (g_avcChecksumClearLateFrame != 0 && (unsigned int)(g_avcFrameCount - 704) <= 1U)
		avcdecClearEdramFrame(meEdramGetBase(0U), outInfo->frameWidth, outInfo->frameHeight);

	if (g_avcChecksumClearEarlyFrame != 0 && (unsigned int)(g_avcFrameCount - 119) <= 1U)
		avcdecClearEdramFrame(meEdramGetBase(0U), outInfo->frameWidth, outInfo->frameHeight);

	return 0;
}

static int __attribute__((noinline, noclone)) avcdecDecodeAuFrame(const uint8_t *esBuf, int esSize)
{
	SceAvcdecArrayPicture arrayPic;
	SceAvcdecAu au;
	SceAvcdecPicture *picPtr;
	SceAvcdecPicture pic;
	SceAvcdecPicture clearPic;
	SceAvcdecPictureInternal picInt;
	SceAvcdecPictureInternal clearPicInt;
	AvcdecSeiPicTiming seiTiming;
	int ret;

	g_avcHasDecoded = 1;
	memset(&pic, 0, sizeof(pic));
	pic.size = sizeof(pic);
	if (g_avcDirectOutputMode != 0) {
		void *dst = (g_avcDirectDstBuf != NULL) ? g_avcDirectDstBuf : g_avcFrameBuf;
		pic.frame.framePitch = 512U;
		pic.frame.frameWidth = 480U;
		pic.frame.frameHeight = 272U;
		pic.frame.pPicture[0] = dst;
	} else {
		pic.frame.pixelType = SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER;
		pic.frame.framePitch = 480U;
		pic.frame.frameWidth = 480U;
		pic.frame.frameHeight = 272U;
		pic.frame.pPicture[0] = g_avcYuvFrameBuf;
	}

	picPtr = &pic;
	arrayPic.pPicture = &picPtr;
	arrayPic.numOfOutput = 0U;
	arrayPic.numOfElm = 1U;

	if (g_avcSynthPtsMode != 0) {
		uint32_t ptsLo = 3003U;
		uint32_t dtsLo = (uint32_t)g_avcFrameCount;
		if (g_avcFrameCount != 0) {
			if ((g_avcFrameCount & 1) != 0) {
				ptsLo = 3003U * (uint32_t)(g_avcFrameCount + 2);
				dtsLo = ptsLo - 6006U;
			} else {
				ptsLo = 3003U * (uint32_t)g_avcFrameCount;
				dtsLo = ptsLo;
			}
		}
		au.pts.upper = 0U;
		au.pts.lower = ptsLo;
		au.dts.upper = 0U;
		au.dts.lower = dtsLo;
	} else if ((g_avcUseExternalTimestamps != 0 || g_avcUseGuestTimestamps != 0) && (g_avcTimestampValid & 1) != 0) {
		au.pts.upper = g_avcPtsUpper;
		au.pts.lower = g_avcPtsLower;
		au.dts.upper = g_avcDtsUpper;
		au.dts.lower = g_avcDtsLower;
	} else if (g_avcFrameCount != 0) {
		memset(&au, 0xFF, 16U);
	} else {
		au.pts.upper = 0U;
		au.pts.lower = 90000U;
		au.dts.upper = 0U;
		au.dts.lower = 90000U;
	}

	au.es.pBuf = (void *)esBuf;
	au.es.size = (SceUInt32)esSize;

	if (g_avcChecksumMode != 0 && g_avcFrameCount == 0) {
		int sum = 0;
		for (int i = 0; i < esSize; i++)
			sum += esBuf[i];
		if (g_avcChecksumMode == 1) {
			if (sum == 26679)
				g_avcChecksumClearLateFrame = 1;
		} else if (g_avcChecksumMode == 2) {
			if (sum == 1820332 || sum == 1197379 || sum == 3678357 || sum == 4612978) {
				g_avcDynamicAsyncActive = 1;
				g_avcUseAuThread = 1;
				g_avcForceBlackOnMissing = 1;
				g_avcProbeFirstFrameSei = 0;
			}
		} else if (g_avcChecksumMode == 3) {
			if (sum == 611237) {
				g_avcDisableDmacOnChecksum = 1;
				remoteDmacplusSetEnabled(0);
			}
		} else if (g_avcChecksumMode == 4 && sum == 14073) {
			g_avcChecksumClearEarlyFrame = 1;
		}
	}

	if (g_avcAsyncDecodeMode == 0) {
		ret = sceAvcdecDecode(&g_avcCtrl, &au, &arrayPic);
		avcdecDrainPicoutSema();
	} else {
		ret = avcdecSubmitAu(&au);
		if (ret < 0 && g_avcIgnoreAuError == 0)
			return ret;

		if (g_avcProbeFirstFrameSei != 0 && g_avcFrameCount == 0) {
			if (sceAvcdecDecodeStop(&g_avcCtrl, &arrayPic) >= 0) {
				if (arrayPic.numOfOutput != 0U) {
					if (sceAvcdecGetSeiPictureTimingInternal(&g_avcCtrl, 0, &seiTiming) >= 0 &&
					    seiTiming.presentFlag == 1U) {
						g_avcInterlacedStream = (seiTiming.picStruct == 2U && g_avcDisableInterlaceDetect == 0) ? 1 : 0;
					}
					g_avcOutputCount++;
				}
			} else {
				arrayPic.numOfOutput = 0U;
			}
			ret = avcdecSubmitAu(&au);
		} else if (g_avcFrameCount != 0 && ret == 0) {
			SceUInt64 startUs = sceKernelGetProcessTimeWide();
			int gotPic = 0;
			for (;;) {
				if (sceKernelGetProcessTimeWide() - startUs > 5999U) {
					ret = 0;
					break;
				}
				SceUInt timeout = 100U;
				if (sceKernelWaitSemaCB(g_avcPicoutSema, 1, &timeout) == 0) {
					gotPic = 1;
					break;
				}
			}
			if (gotPic) {
				SceUInt opt = 0U;
				ret = sceAvcdecDecodeGetPictureInternal(&g_avcCtrl, &arrayPic, &opt);
			}
		}
	}

	int forceBlank = 0;
	if (arrayPic.numOfOutput != 0U) {
		if (g_avcDirectOutputMode != 0)
			avcdecBuildInternalPicture(&picInt, &pic, 0U, pic.frame.pPicture[0]);
		else
			ret = avcdecCopyDecodedFrame(&picInt, &pic, g_avcYuvFrameBuf, meEdramGetBase(0U));

		g_avcLastFrameAvailable = 1;
		g_avcCurrentPic = picInt;
		if (g_avcFrameCount == 0 && g_avcInterlacedStream != 0)
			g_avcLastFrameAvailable = 0;
	} else if (g_avcFrameCount <= 0) {
		g_avcLastFrameAvailable = 0;
	} else if ((g_avcAsyncDecodeMode != 0 && g_avcOutputCount != 0) || g_avcFirstFramePending == 0) {
		ret = avcdecRepeatLastFrame(&arrayPic.numOfOutput, &picInt);
		if (arrayPic.numOfOutput != 0U) {
			g_avcLastFrameAvailable = 1;
			g_avcCurrentPic = picInt;
		} else {
			g_avcLastFrameAvailable = 0;
			if (g_avcForceBlackOnMissing != 0)
				forceBlank = 1;
		}
	} else {
		g_avcLastFrameAvailable = 0;
		if (g_avcForceBlackOnMissing != 0)
			forceBlank = 1;
	}

	if (forceBlank) {
		void *edram = meEdramGetBase(0U);
		memset(&clearPic, 0, sizeof(clearPic));
		clearPic.size = sizeof(clearPic);
		clearPic.frame.pixelType = SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER;
		clearPic.frame.framePitch = 480U;
		clearPic.frame.frameWidth = 480U;
		clearPic.frame.frameHeight = 272U;
		clearPic.frame.pPicture[0] = edram;
		avcdecBuildInternalPicture(&clearPicInt, &clearPic, SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER, edram);
		avcdecClearEdramFrame(edram, 480U, 272U);
		g_avcLastFrameAvailable = 1;
		g_avcCurrentPic = clearPicInt;
		arrayPic.numOfOutput = 1U;
		g_avcOutputCount++;
	}

	if (g_avcIgnoreUnsupportSize != 0 && ret == SCE_AVCDEC_ERROR_UNSUPPORT_IMAGE_SIZE) {
		ret = 0;
		g_avcCurrentPic.framePitch = pic.frame.horizontalSize;
		g_avcCurrentPic.horizontalSize = pic.frame.horizontalSize;
		g_avcCurrentPic.verticalSize = pic.frame.verticalSize;
		g_avcLastFrameAvailable = 1;
	}

	g_avcFrameCount++;
	g_avcTimestampValid = 0;
	return ret;
}

int remoteMeAvcdecZoomFrame(void *dstBuf, void *srcBuf, int zoomScale)
{
	if ((unsigned int)(zoomScale - 10) > 18U)
		return -1;
	if (zoomScale <= 19)
		return avcdecZoomCropFrame(dstBuf, srcBuf, zoomScale, 480, 320, 0, 0);
	return avcdecZoomCropFrame(dstBuf, srcBuf, zoomScale, 320, 160, 64, 56);
}

int remoteMeAvcdecNotifyResume(void)
{
	sceKernelAtomicGetAndSet8(&g_avcResumeFlag, 1);
	return 0;
}

int remoteMeAvcdecCsc(void *dstRgb, uint32_t dstSize, void *srcYuv, uint32_t *params)
{
	if (g_avcDirectOutputMode != 0) {
		g_avcDirectDstBuf = dstRgb;
		return 0;
	}

	int ret = avcdecOutputCscFrame(dstRgb, dstSize, srcYuv, (AvcdecCscParams *)params);
	if (g_avcRepeatLastFrameMode != 0) {
		void *prev;
		if (g_avcFrameCount != 0) {
			prev = g_avcLastFrameDst;
		} else {
			if (g_avcLastFrameDst != NULL)
				avcdecOutputCscFrame(g_avcLastFrameDst, dstSize, srcYuv, (AvcdecCscParams *)params);
			if (g_avcPrevFrameDst != NULL)
				avcdecOutputCscFrame(g_avcPrevFrameDst, dstSize, srcYuv, (AvcdecCscParams *)params);
			prev = NULL;
		}
		g_avcPrevFrameDst = prev;
		g_avcLastFrameDst = dstRgb;
	}
	return ret;
}

int remoteMeAvcdecSetTimestamp(uint32_t ptsHi, uint32_t ptsLo, uint32_t dtsHi, uint32_t dtsLo)
{
	g_avcPtsUpper = ptsHi;
	g_avcTimestampValid = 1;
	g_avcPtsLower = ptsLo;
	g_avcDtsUpper = dtsHi;
	g_avcDtsLower = dtsLo;
	return 0;
}

int remoteMeAvcdecCopyFrameToHost(void *dstHost, void *srcEdram, uint32_t frameBytes)
{
	SceAvcdecPicture pic;
	SceAvcdecPictureInternal picInt;
	void *edram = meEdramGetBase(0U);

	if (edram != srcEdram || frameBytes != ((3U * g_avcCurrentPic.horizontalSize * g_avcCurrentPic.verticalSize) >> 1))
		return -1;

	avcdecBuildCurrentPicture(&pic);
	int ret = avcdecCopyDecodedFrame(&picInt, &pic, edram, dstHost);
	return (ret < 0) ? ret : 0;
}

int remoteMeAvcdecDispatch(int cmd, uint32_t argA, uint32_t argB, uint32_t argC, uint32_t argD, uint32_t argE)
{
	switch (cmd) {
	case 0: {
		uint32_t *outMemInfo = (uint32_t *)remotefsTranslateUserGuestAddr(argA, 2, 40U);
		outMemInfo[0] = 0x1F6400U;
		outMemInfo[1] = 0x15C00U;
		remotefsFlushGuestAddr(outMemInfo, 40U);
		return 15404;
	}
	case 1: {
		uint32_t *outHandle = (uint32_t *)remotefsTranslateUserGuestAddr(argA, 3, 4U);
		if (g_avcInitDelayUs != 0U)
			sceKernelDelayThread(g_avcInitDelayUs);
		if (g_avcPatchReturnZero != 0)
			pspemuApplyAvcPatchReturnZero();
		if (g_avcPatchReturnZeroAlt != 0)
			pspemuApplyAvcPatchReturnZeroAlt();
		*outHandle = 0x1A7B00U;
		remotefsFlushGuestAddr(outHandle, 4U);
		if (pspemuCompatHasFlag(0x800U) != 0)
			g_compatMsfsSeekFlag = 1;
		return 1;
	}
	case 2: {
		const uint8_t *esBuf = (const uint8_t *)meEdramGetBase(argB);
		if (esBuf == NULL)
			esBuf = (const uint8_t *)remotefsTranslateUserGuestAddr(argB, 1, argC);
		AvcdecDecodeOutInfo *outInfo = (AvcdecDecodeOutInfo *)remotefsTranslateUserGuestAddr(argD, 3, sizeof(*outInfo));
		AvcdecPlaneOffsets *outPlanes = (AvcdecPlaneOffsets *)remotefsTranslateUserGuestAddr(argE, 3, sizeof(*outPlanes));
		avcdecLockDecoder();
		int decRet = avcdecDecodeAuFrame(esBuf, (int)argC);
		avcdecUnlockDecoder();
		avcdecFillDecodeOutputInfo(outInfo, outPlanes, 0);
		remotefsFlushGuestAddr(outInfo, sizeof(*outInfo));
		remotefsFlushGuestAddr(outPlanes, sizeof(*outPlanes));
		return (decRet != 0) ? -8 : 1;
	}
	case 3: {
		AvcdecDecodeOutInfo *outInfo = (AvcdecDecodeOutInfo *)remotefsTranslateUserGuestAddr(argB, 3, sizeof(*outInfo));
		AvcdecPlaneOffsets *outPlanes = (AvcdecPlaneOffsets *)remotefsTranslateUserGuestAddr(argC, 3, sizeof(*outPlanes));
		avcdecLockDecoder();
		int stopRet = avcdecDecodeStopInternal();
		avcdecUnlockDecoder();
		avcdecFillDecodeOutputInfo(outInfo, outPlanes, 1);
		remotefsFlushGuestAddr(outInfo, sizeof(*outInfo));
		remotefsFlushGuestAddr(outPlanes, sizeof(*outPlanes));
		if (pspemuCompatHasFlag(0x800U) != 0)
			g_compatMsfsSeekFlag = 0;
		return (stopRet != 0) ? -8 : 1;
	}
	case 4:
		avcdecDrainPicoutSema();
		sceAvcdecDecodeFlush(&g_avcCtrl);
		g_avcAuPending = 0;
		avcdecResetState();
		return 1;
	case 5: {
		uint32_t *outMemInfo = (uint32_t *)remotefsTranslateUserGuestAddr(argB, 3, 40U);
		uint32_t mbH = (argC + 15U) >> 4;
		uint32_t slots = 2U * (argE + 1U);
		int ret;
		if (slots <= 8U) {
			if ((mbH & 1U) != 0U)
				mbH++;
			uint32_t lumaStride = (((16U * argD * mbH) >> 1) + 1023U) & ~1023U;
			uint32_t chromaStride = (((16U * argD * mbH) >> 2) + 1023U) & ~1023U;
			uint32_t totalBytes = slots * (lumaStride + chromaStride) + 0x4000U;
			if (totalBytes <= 0x100000U && slots != 0U) {
				outMemInfo[0] = totalBytes;
				outMemInfo[1] = 0U;
				ret = 1;
			} else {
				ret = -5;
			}
		} else {
			ret = -5;
		}
		remotefsFlushGuestAddr(outMemInfo, 40U);
		return ret;
	}
	case 6: {
		uint32_t *outVal = (uint32_t *)remotefsTranslateUserGuestAddr(argA, 3, 4U);
		*outVal = 120U;
		remotefsFlushGuestAddr(outVal, 4U);
		return 0;
	}
	case 11:
		g_avcUseGuestTimestamps = 1;
		return 0;
	case 16: {
		AvcdecEdramConvReq *convReq = (AvcdecEdramConvReq *)remotefsTranslateUserGuestAddr(argA, 1, sizeof(*convReq));
		int ret = avcdecConvertEdramYuv(convReq);
		remotefsFlushGuestAddr(convReq, sizeof(*convReq));
		return ret;
	}
	default:
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	}
}

int remoteMeAvcdecSetActive(int active)
{
	SceAvcdecDecoderInfo decoderInfo;
	SceAvcdecQueryDecoderInfo query;
	int enable = (active != 0) ? 1 : 0;

	if (g_avcDecoderActive == enable)
		return 0;

	avcdecLockDecoder();
	int ret;
	if (enable != 0) {
		query.horizontal = 480U;
		query.vertical = 272U;
		query.numOfRefFrames = 3U;
		ret = 0;
		if (g_avcUseLowLatencyMode != 0)
			ret = sceAvcdecSetDecodeModeInternal(SCE_VIDEODEC_TYPE_HW_AVCDEC, 6);
		if (ret >= 0) {
			ret = sceAvcdecQueryDecoderMemSize(SCE_VIDEODEC_TYPE_HW_AVCDEC, &query, &decoderInfo);
			if (ret >= 0) {
				g_avcCtrl.handle = 0;
				g_avcCtrl.frameBuf.pBuf = NULL;
				g_avcCtrl.frameBuf.size = 0U;
				ret = sceAvcdecCreateDecoder(SCE_VIDEODEC_TYPE_HW_AVCDEC, &g_avcCtrl, &query);
				if (ret >= 0) {
					ret = sceAvcdecRegisterCallbackInternal(&g_avcCtrl, g_avcCallbackId, 2);
					if (ret < 0) {
						sceAvcdecUnregisterCallbackInternal(&g_avcCtrl);
						sceAvcdecDeleteDecoder(&g_avcCtrl);
					}
				}
			}
		}
	} else {
		sceAvcdecUnregisterCallbackInternal(&g_avcCtrl);
		ret = sceAvcdecDeleteDecoder(&g_avcCtrl);
	}
	avcdecUnlockDecoder();
	g_avcDecoderActive = enable;
	return ret;
}

int remoteMeAvcdecSetPatchFlag(int patchId)
{
	if (patchId == 10) {
		g_avcPatchReturnZero = 1;
		return 0;
	}
	if (patchId != 11)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	g_avcPatchReturnZeroAlt = 1;
	return 0;
}

int remoteMeAvcdecInit(void)
{
	SceVideodecQueryInitInfoHwAvcdec initInfo;
	int avcFlags = pspemuCompatGetParam(24);

	if (avcFlags != -1) {
		if ((avcFlags & 1) != 0)
			g_avcAsyncDecodeMode = 0;
		if ((avcFlags & 2) != 0) {
			g_avcUseAuThread = 1;
			g_avcProbeFirstFrameSei = 0;
		}
		if ((avcFlags & 4) != 0)
			g_avcBlankOnMissing = 1;
		if ((avcFlags & 0x10) != 0)
			g_avcRepeatLastFrameMode = 1;
		if ((avcFlags & 0x20) != 0)
			g_avcSynthPtsMode = 1;
		if ((avcFlags & 0x40) != 0)
			g_avcResumeHoldEnabled = 1;
		if ((avcFlags & 0x80) != 0)
			g_avcDisableInterlaceDetect = 1;
		if ((avcFlags & 0x100) != 0)
			g_avcUseExternalTimestamps = 1;
		if ((avcFlags & 0x400) != 0)
			g_avcDirectOutputMode = 1;
		if ((avcFlags & 8) != 0)
			g_avcIgnoreAuError = 1;
		if ((avcFlags & 0x200) != 0)
			g_avcIgnoreUnsupportSize = 1;
		if ((avcFlags & 0x800) != 0)
			g_avcUseLowLatencyMode = 1;
	}

	int cscDelay = pspemuCompatGetParam(0);
	if (cscDelay != -1)
		g_avcCscDelayUs = (SceUInt)cscDelay;

	int initDelay = pspemuCompatGetParam(20);
	if (initDelay != -1)
		g_avcInitDelayUs = (SceUInt)initDelay;

	int checksumMode = pspemuCompatGetParam(22);
	if (checksumMode != -1) {
		g_avcChecksumMode = checksumMode;
		if (checksumMode == 4)
			g_avcClearBufWhite = 1;
	}

	initInfo.size = sizeof(initInfo);
	initInfo.horizontal = 480U;
	initInfo.vertical = 272U;
	initInfo.numOfRefFrames = 3U;
	initInfo.numOfStreams = 1U;

	avcdecResetState();
	g_avcAuThreadExit = 0;
	g_avcPtsUpper = 0xFFFFFFFFU;
	g_avcPtsLower = 0xFFFFFFFFU;
	g_avcDtsUpper = 0xFFFFFFFFU;
	g_avcDtsLower = 0xFFFFFFFFU;

	if (g_avcUseLowLatencyMode != 0) {
		int cfgRet = sceVideodecSetConfigInternal(SCE_VIDEODEC_TYPE_HW_AVCDEC, 2);
		if (cfgRet < 0)
			return cfgRet;
	}

	int ret = sceVideodecInitLibrary(SCE_VIDEODEC_TYPE_HW_AVCDEC, (const SceVideodecQueryInitInfo *)&initInfo);
	if (ret < 0) {
		avcdecCleanup(0);
		return ret;
	}

	g_avcCallbackId = sceKernelCreateCallback("ScePspemuAvcDecoder", 0, avcdecPicoutCallback, NULL);
	if (g_avcCallbackId < 0) {
		avcdecCleanup(1);
		return ret;
	}

	g_avcFrameMemId = sceKernelAllocMemBlock("ScePspemuAvcFrameBuf", PSPEMU_MEMBLOCK_CDRAM_RW, 0x100000U, NULL);
	if (g_avcFrameMemId < 0)
		return g_avcFrameMemId;
	ret = sceKernelGetMemBlockBase(g_avcFrameMemId, (void **)&g_avcFrameBuf);
	if (ret < 0) {
		avcdecCleanup(1);
		return ret;
	}

	g_avcYuvMemId = sceKernelAllocMemBlock("ScePspemuAvcYuvFrameBuf", PSPEMU_MEMBLOCK_CDRAM_RW, 0x100000U, NULL);
	if (g_avcYuvMemId < 0)
		return g_avcYuvMemId;
	ret = sceKernelGetMemBlockBase(g_avcYuvMemId, (void **)&g_avcYuvFrameBuf);
	if (ret < 0) {
		avcdecCleanup(1);
		return ret;
	}

	g_avcWorkMemId = sceKernelAllocMemBlock("ScePspemuAvcWorkBuf", PSPEMU_MEMBLOCK_CDRAM_RW, 0x100000U, NULL);
	if (g_avcWorkMemId < 0)
		return g_avcWorkMemId;
	ret = sceKernelGetMemBlockBase(g_avcWorkMemId, (void **)&g_avcWorkBuf);
	if (ret < 0) {
		avcdecCleanup(1);
		return ret;
	}

	g_avcClearedMemId = sceKernelAllocMemBlock("ScePspemuAvcClearedBuf", PSPEMU_MEMBLOCK_CDRAM_RW, 0x200000U, NULL);
	if (g_avcClearedMemId < 0)
		return g_avcClearedMemId;

	int mtxRet = sceKernelCreateLwMutex(&g_avcDecoderLock, "ScePspemuAvcDecoderLock", 0, 0, NULL);
	if (mtxRet != 0) {
		avcdecCleanup(1);
		return mtxRet;
	}

	ret = sceKernelGetMemBlockBase(g_avcClearedMemId, (void **)&g_avcClearedBuf);
	if (ret < 0) {
		avcdecCleanup(1);
		return ret;
	}

	memset(g_avcClearedBuf, (g_avcClearBufWhite != 0) ? 0xFF : 0x00, 0x200000U);

	g_avcPicoutSema = sceKernelCreateSema("ScePspemuAvcPicout", 0, 0, 4, NULL);
	if (g_avcPicoutSema <= 0) {
		avcdecCleanup(1);
		return g_avcPicoutSema;
	}

	g_avcAuReadIdx = 0;
	g_avcAuPending = 0;
	g_avcAuWriteIdx = 0;
	g_avcAuReqSema = sceKernelCreateSema("ScePspemuAuInputRequest", 0, 0, AVCDEC_AU_RING_SLOTS, NULL);
	if (g_avcAuReqSema <= 0) {
		avcdecCleanup(1);
		return g_avcAuReqSema;
	}

	g_avcAuDoneSema = sceKernelCreateSema("ScePspemuAuInputDone", 0, AVCDEC_AU_RING_SLOTS, AVCDEC_AU_RING_SLOTS, NULL);
	if (g_avcAuDoneSema <= 0) {
		avcdecCleanup(1);
		return g_avcAuDoneSema;
	}

	g_avcAuThreadId = sceKernelCreateThread(
		"ScePspemuRemoteAvcAuInput",
		avcdecAuInputThread,
		96,
		0x1000U,
		0,
		0x40000,
		NULL);
	if (g_avcAuThreadId < 0) {
		avcdecCleanup(1);
		return g_avcAuThreadId;
	}

	ret = sceKernelStartThread(g_avcAuThreadId, 0, NULL);
	if (ret != 0) {
		avcdecCleanup(1);
		return ret;
	}
	return 0;
}

int remoteMeAvcdecUpscale(void *dstBuf, void *srcBuf)
{
	return avcdecUpscaleToVitaScreen(dstBuf, srcBuf);
}

int remoteMeAvcdecHasDecoded(void)
{
	return g_avcHasDecoded;
}
