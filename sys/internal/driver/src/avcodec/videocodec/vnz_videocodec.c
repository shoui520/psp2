/* Sony source: videocodec/vnz_videocodec.c (1.80's records). */
#include "videocodec_internal.h"
#include <sysmem/debug.h>
#include <processmgr/processmgr.h>
#include <sysmem/sysmem.h>
#include <string.h>
#include <threadmgr/fast_mutex_object.h>
#include <avcdec.h>
#include <intrmgr/syscall.h>
#include <threadmgr/threadmgr.h>
#include <kernel/error.h>
#include <vnz_wrapper/vnz_wrapper.h>

static const int videocodecDecoderErrors[] = {
	SCE_AVCDEC_ERROR_INVALID_PARAM,
	SCE_AVCDEC_ERROR_INVALID_PARAM,
	SCE_AVCDEC_ERROR_OUT_OF_MEMORY,
	SCE_AVCDEC_ERROR_INVALID_STATE,
	SCE_AVCDEC_ERROR_UNSUPPORT_IMAGE_SIZE,
	SCE_AVCDEC_ERROR_INVALID_STATE,
	SCE_AVCDEC_ERROR_INVALID_STATE,
	SCE_AVCDEC_ERROR_ALREADY_USED,
	SCE_AVCDEC_ERROR_INVALID_POINTER,
	SCE_AVCDEC_ERROR_INVALID_STATE,
	SCE_AVCDEC_ERROR_INITIALIZE,
	SCE_AVCDEC_ERROR_NOT_INITIALIZE,
	SCE_AVCDEC_ERROR_INVALID_STATE,
	SCE_AVCDEC_ERROR_ALREADY_USED,
	SCE_AVCDEC_ERROR_INVALID_STREAM,
	SCE_AVCDEC_ERROR_ES_BUFFER_FULL,
	SCE_AVCDEC_ERROR_ALREADY_USED,
};

static const int videocodecCodecEngineErrors[] = {
	SCE_AVCDEC_ERROR_INVALID_PARAM,
	SCE_AVCDEC_ERROR_INVALID_POINTER,
	SCE_AVCDEC_ERROR_INVALID_STATE,
	SCE_AVCDEC_ERROR_INITIALIZE,
	SCE_AVCDEC_ERROR_NOT_INITIALIZE,
	SCE_AVCDEC_ERROR_ALREADY_USED,
	SCE_AVCDEC_ERROR_INVALID_PARAM,
	SCE_AVCDEC_ERROR_INVALID_STATE,
	SCE_AVCDEC_ERROR_ES_BUFFER_FULL,
	SCE_AVCDEC_ERROR_ES_BUFFER_FULL,
	SCE_AVCDEC_ERROR_INVALID_STREAM,
	SCE_AVCDEC_ERROR_INVALID_PICTURE,
	SCE_AVCDEC_ERROR_OUT_OF_MEMORY,
	SCE_AVCDEC_ERROR_GREATER_THAN_1200_AT_LV30,
};

static const int videocodecM4vdecErrors[] = {
	VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM,
	VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM,
	VIDEOCODEC_M4VDEC_ERROR_OUT_OF_MEMORY,
	VIDEOCODEC_M4VDEC_ERROR_INVALID_STATE,
	VIDEOCODEC_M4VDEC_ERROR_UNSUPPORT_IMAGE_SIZE,
	VIDEOCODEC_M4VDEC_ERROR_INVALID_STATE,
	VIDEOCODEC_M4VDEC_ERROR_INVALID_STATE,
	VIDEOCODEC_M4VDEC_ERROR_ALREADY_USED,
	VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER,
	VIDEOCODEC_M4VDEC_ERROR_INVALID_STATE,
	VIDEOCODEC_M4VDEC_ERROR_INITIALIZE,
	VIDEOCODEC_M4VDEC_ERROR_NOT_INITIALIZE,
	VIDEOCODEC_M4VDEC_ERROR_INVALID_STATE,
	VIDEOCODEC_M4VDEC_ERROR_ALREADY_USED,
	VIDEOCODEC_M4VDEC_ERROR_INVALID_STREAM,
	VIDEOCODEC_M4VDEC_ERROR_ES_BUFFER_FULL,
	VIDEOCODEC_M4VDEC_ERROR_ALREADY_USED,
};

/* the VUI an encoder writes for AvcencInitParam.vuiIndex: NTSC, SMPTE 170M or BT.709 */
static const AvcencVui avcencVuis[] = {
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 1, 0, 2, 0, 1, 6, 6, 6 },
	{ 1, 0, 2, 0, 1, 1, 1, 1 },
	{ 1, 0, 2, 1, 1, 6, 6, 6 },
	{ 1, 0, 2, 1, 1, 1, 1, 1 },
};

/* codingOption[0] for each coding mode, 1..9 */
static const unsigned char avcencCodingOptions[] = {
	0, 0x10, 0, 0x10, 0x10, 0x10, 0x10, 0, 0x10,
};

/* the picture sizes sceAvcencQueryEncoderMemSizeBasic offers */
typedef struct AvcencBasicGeometry {
	unsigned char preset;
	unsigned char padding;
	unsigned short pitch;
	unsigned short alignedHorizontal;
	unsigned short alignedVertical;
	unsigned short horizontal;
	unsigned short vertical;
} AvcencBasicGeometry;

static const AvcencBasicGeometry avcencBasicGeometries[] = {
	{ 1, 0, 320, 320, 192, 320, 180 },
	{ 4, 0, 640, 640, 368, 640, 360 },
	{ 4, 0, 640, 640, 480, 640, 480 },
};

/* an H.264 level's limits: macroblocks per second, frame size in macroblocks, bit rate */
typedef struct AvcencLevelLimit {
	unsigned int maxMbps;
	unsigned int maxFs;
	unsigned int maxBr;
} AvcencLevelLimit;

static const AvcencLevelLimit avcencLevelLimits[9];

/* levels 20..41 by level - 20 (level_idc), NULL where no level is */
static const AvcencLevelLimit *const avcencLevelLimitTable[] = {
	&avcencLevelLimits[1], &avcencLevelLimits[2], &avcencLevelLimits[3],
	NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	&avcencLevelLimits[4], &avcencLevelLimits[5], &avcencLevelLimits[6],
	NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	&avcencLevelLimits[7], &avcencLevelLimits[8],
};

/* the GOP length for each basic frame rate */
static const unsigned int avcencBasicGopLengths[] = { 28, 58 };

/* levels 1b..1.3, 2, 2.1, 2.2, 3, 3.1, 3.2, 4, 4.1 */
static const AvcencLevelLimit avcencLevelLimits[] = {
	{ 11880, 396, 768000 },
	{ 11880, 396, 2000000 },
	{ 19800, 792, 4000000 },
	{ 20250, 1620, 4000000 },
	{ 40500, 1620, 10000000 },
	{ 108000, 3600, 14000000 },
	{ 216000, 5120, 20000000 },
	{ 245760, 8192, 20000000 },
	{ 245760, 8192, 50000000 },
};

/* a preset's level and frame rate (frameRate * 1000 / frameRateScale) */
typedef struct AvcencPresetRate {
	unsigned char levelLimit;
	unsigned char frameRate;
	unsigned short frameRateScale;
} AvcencPresetRate;

static const AvcencPresetRate avcencPresetRates[] = {
	{ 0, 30, 1001 },
	{ 2, 30, 1001 },
	{ 4, 15, 1001 },
	{ 4, 30, 1001 },
};

/* sceAvcencQueryEncoderMemSizeBasic's bit rates, by bitRateIndex */
static const unsigned int avcencBasicBitRates[] = {
	0, 320000, 416000, 640000, 1000000, 1500000,
};

/* presets 1..4: what a zero field of an AvcencInitParam takes */
static const AvcencInitParamInternal avcencPresets[] = {
	{
		.profile = 'B', .level = 13, .userOption = { 1, 2 },
		.pitch = 320, .alignedHorizontal = 320, .alignedVertical = 240,
		.horizontal = 320, .vertical = 240, .numOfStreams = 1,
		.rateControl = { 768000, 921600 }, .codingOption = { 0x10 }, .sliceOption = 1,
		.parameter = { { 768000, 768000 }, 28800, 58, 4004, 240000 },
	},
	{
		.profile = 'B', .level = 21, .userOption = { 1, 2 },
		.pitch = 480, .alignedHorizontal = 480, .alignedVertical = 272,
		.horizontal = 480, .vertical = 272, .numOfStreams = 1,
		.rateControl = { 4000000, 4800000 }, .codingOption = { 0x10 }, .sliceOption = 1,
		.parameter = { { 1000000, 1200000 }, 48960, 58, 4004, 240000 },
	},
	{
		.profile = 'B', .level = 30, .userOption = { 1, 2 },
		.pitch = 640, .alignedHorizontal = 640, .alignedVertical = 480,
		.horizontal = 640, .vertical = 480, .numOfStreams = 1,
		.rateControl = { 10000000, 12000000 }, .codingOption = { 0x10 }, .sliceOption = 1,
		.parameter = { { 2000000, 2400000 }, 115200, 28, 8008, 240000 },
	},
	{
		.profile = 'B', .level = 30, .userOption = { 1, 2 },
		.pitch = 640, .alignedHorizontal = 640, .alignedVertical = 480,
		.horizontal = 640, .vertical = 480, .numOfStreams = 1,
		.rateControl = { 10000000, 12000000 }, .codingOption = { 0x10 }, .sliceOption = 1,
		.parameter = { { 2000000, 2400000 }, 115200, 58, 4004, 240000 },
	},
};

/* sceAvcencQueryEncoderMemSizeBasic's numUnitsInTick for each frame rate (timeScale 240000) */
static const unsigned int avcencBasicNumUnitsInTick[] = { 8008, 4004 };

static const int videocodecAvcencErrors[] = {
	VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM,
	VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM,
	VIDEOCODEC_AVCENC_ERROR_OUT_OF_MEMORY,
	VIDEOCODEC_AVCENC_ERROR_INVALID_STATE,
	VIDEOCODEC_AVCENC_ERROR_UNSUPPORT_IMAGE_SIZE,
	VIDEOCODEC_AVCENC_ERROR_INVALID_STATE,
	VIDEOCODEC_AVCENC_ERROR_INVALID_STATE,
	VIDEOCODEC_AVCENC_ERROR_ALREADY_USED,
	VIDEOCODEC_AVCENC_ERROR_INVALID_POINTER,
	VIDEOCODEC_AVCENC_ERROR_INVALID_STATE,
	VIDEOCODEC_AVCENC_ERROR_INITIALIZE,
	VIDEOCODEC_AVCENC_ERROR_NOT_INITIALIZE,
	VIDEOCODEC_AVCENC_ERROR_INVALID_STATE,
	VIDEOCODEC_AVCENC_ERROR_ALREADY_USED,
	VIDEOCODEC_AVCENC_ERROR_UNSUPPORT_IMAGE_SIZE,
	VIDEOCODEC_AVCENC_ERROR_ES_BUFFER_FULL,
	VIDEOCODEC_AVCENC_ERROR_ALREADY_USED,
};

static inline __attribute__((always_inline)) int videocodecMapCodecEngineError(int result)
{
	unsigned int index = (unsigned int)result - 0x80620101;

	if (index <= 13)
		result = videocodecCodecEngineErrors[index];
	return result;
}

static inline __attribute__((always_inline)) int videocodecMapDecoderError(int result)
{
	unsigned int index = (unsigned int)result - 0x80620201;

	if (index <= 16)
		return videocodecDecoderErrors[index];
	if (result == (int)0x80620380)
		return SCE_AVCDEC_ERROR_INVALID_STREAM;
	if (result == (int)0x806203C0)
		return SCE_AVCDEC_ERROR_INVALID_STATE;
	return result;
}

static inline __attribute__((always_inline)) int videocodecMapM4vdecError(int result)
{
	unsigned int index = (unsigned int)result - 0x80620201;

	if (index <= 16)
		return videocodecM4vdecErrors[index];
	if (result == (int)0x80620480)
		return VIDEOCODEC_M4VDEC_ERROR_INVALID_STREAM;
	if (result == (int)0x806204C0)
		return VIDEOCODEC_M4VDEC_ERROR_INVALID_STATE;
	return result;
}

/* The frame buffer an MPEG-4 decoder needs for pInfo, in 256 KiB units. */
static inline __attribute__((always_inline)) int
m4vdecQueryDecoderMemSizeInline(const Dsp0VideocodecM4vDecoderInfo *pInfo, SceSize *pFrameMemSize)
{
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	Dsp0VideocodecM4vDecoderInfo info;
	SceSize frameMemSize;
	int ret;

	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		return ret;
	info.horizontal = pInfo->horizontal;
	info.vertical = pInfo->vertical;
	info.maxEsSize = pInfo->maxEsSize;
	ret = dsp0VideocodecQueryM4vDecoderMemSize(&info, &frameMemSize);
	ret = videocodecMapM4vdecError(ret);
	if (ret >= 0)
		*pFrameMemSize = (frameMemSize + 0x3FFFF) >> 18 << 18;
	sceKernelUnlockFastMutex(global->pAllMutex);
	return ret;
}

/*
 * Copies count pictures in from the user pointers in pUser, each into the next
 * VIDEOCODEC_PICTURE_COPY_STRIDE bytes of buf, and points pKernel at the copies.
 */
static inline __attribute__((always_inline)) int
videocodecCopyInPicturesInline(unsigned int count,
	void **pKernel, char *buf, void *const *pUser, SceSize size)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i != count; i++, buf += VIDEOCODEC_PICTURE_COPY_STRIDE) {
		if (pUser[i] == NULL) {
			pKernel[i] = NULL;
			continue;
		}
		pKernel[i] = buf;
		ret = sceKernelCopyFromUser(buf, pUser[i], size);
		if (ret < 0)
			break;
	}
	return ret;
}

/* Copies the count pictures pKernel points at back out to the user pointers in pUser. */
static inline __attribute__((always_inline)) int
videocodecCopyOutPicturesInline(unsigned int count,
	void *const *pUser, void *const *pKernel, SceSize size)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i != count; i++) {
		if (pKernel[i] == NULL)
			continue;
		ret = sceKernelCopyToUser(pUser[i], pKernel[i], size);
		if (ret < 0)
			break;
	}
	return ret;
}

/* The frame buffer an AVC decoder of codecType needs for pDecoderInfo, in 256 KiB units. */
static inline __attribute__((always_inline)) int
videocodecQueryDecoderMemSizeInline(unsigned int codecType,
		const SceAvcdecQueryDecoderInfo *pDecoderInfo, SceSize *pFrameMemSize)
{
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	uint32_t frameMemSize;
	unsigned int numOfRefFrames;
	int ret;

	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		return ret;
	numOfRefFrames = pDecoderInfo->numOfRefFrames;
	if (numOfRefFrames == 0)
		numOfRefFrames = 1;
	if (codecType == SCE_VIDEODEC_TYPE_HW_AVCDEC) {
		VeneziaDebugImageFormat format;

		format.width = pDecoderInfo->horizontal;
		format.height = pDecoderInfo->vertical;
		format.imageCount = numOfRefFrames;
		ret = SceCodecEngineWrapperForDriver_2B74706E(&format, &frameMemSize);
		ret = videocodecMapCodecEngineError(ret);
	} else {
		VideocodecDecoderQuery query;

		query.info.horizontal = pDecoderInfo->horizontal;
		query.info.vertical = pDecoderInfo->vertical;
		query.info.numOfRefFrames = numOfRefFrames;
		query.mode = 0;
		ret = dsp0VideocodecQueryAvcDecoderMemSize(&query, &frameMemSize);
		ret = videocodecMapDecoderError(ret);
	}
	if (ret >= 0)
		*pFrameMemSize = (frameMemSize + 0x3FFFF) >> 18 << 18;
	sceKernelUnlockFastMutex(global->pAllMutex);
	return ret;
}

int videocodecQueryDecoderMemSize(unsigned int codecType,
		const SceAvcdecQueryDecoderInfo *pDecoderInfo, SceSize *pFrameMemSize)
{
	return videocodecQueryDecoderMemSizeInline(codecType, pDecoderInfo, pFrameMemSize);
}

/*
 * Creates an AVC decoder of codecType for pDecoderInfo in a free slot, in pCtrl's frame buffer:
 * the process's unmap memory, memory locked for the decoder, or, when pCtrl has none, a block
 * of its own.
 */
int videocodecCreateDecoder(unsigned int codecType,
		SceAvcdecCtrl *pCtrl, const SceAvcdecQueryDecoderInfo *pDecoderInfo)
{
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	SceKernelAllocMemBlockKernelOpt opt;
	VideocodecAvcDecoder *avc;
	VideocodecDsp0Decoder *dsp0;
	uintptr_t physicalAddress;
	unsigned int i, numOfRefFrames, handle;
	void *base;
	int ret;

	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		return ret;
	if (codecType == SCE_VIDEODEC_TYPE_HW_AVCDEC) {
		if (global->avcdecInitCount == 0)
			goto notInitialized;
		for (i = 0; i < global->avcdecInfo.numOfStreams; i++) {
			avc = &global->avcDecoders[i];
			ret = lockFastMutex(&avc->mutex);
			if (ret < 0) {
				sceKernelUnlockFastMutex(global->pAllMutex);
				return ret;
			}
			if (avc->handle == 0)
				break;
			sceKernelUnlockFastMutex(&avc->mutex);
		}
		if (i >= global->avcdecInfo.numOfStreams)
			goto alreadyUsed;
		videocodecInitLockedRanges(&avc->lockedRanges, 0, avc->lockedRange, 1,
			VIDEOCODEC_DECODER_ERROR_BASE);
	} else {
		if (global->dsp0Library == 0)
			goto notInitialized;
		for (i = 0; i < global->dsp0DecoderInfo.numOfStreams; i++) {
			dsp0 = &global->dsp0Decoders[i];
			ret = lockFastMutex(&dsp0->mutex);
			if (ret < 0) {
				sceKernelUnlockFastMutex(global->pAllMutex);
				return ret;
			}
			if (dsp0->handle == 0)
				break;
			sceKernelUnlockFastMutex(&dsp0->mutex);
		}
		if (i >= global->dsp0DecoderInfo.numOfStreams)
			goto alreadyUsed;
		videocodecInitLockedRanges(&dsp0->lockedRanges, 0, dsp0->lockedRange, 1,
			VIDEOCODEC_DECODER_ERROR_BASE);
	}

	numOfRefFrames = pDecoderInfo->numOfRefFrames;
	if (numOfRefFrames == 0)
		numOfRefFrames = 1;
	if (codecType == SCE_VIDEODEC_TYPE_HW_AVCDEC) {
		VeneziaDebugImageGeometry libraryInfo;
		VeneziaDebugImageFormat decoderInfo;
		VeneziaVipCaptureParam param;

		if (global->avcdecInfo.horizontal < pDecoderInfo->horizontal ||
		    global->avcdecInfo.vertical < pDecoderInfo->vertical ||
		    numOfRefFrames > global->avcdecInfo.numOfRefFrames) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			goto avcUnlockRanges;
		}
		libraryInfo.width = global->avcdecInfo.horizontal;
		libraryInfo.height = global->avcdecInfo.vertical;
		libraryInfo.imageCount = global->avcdecInfo.numOfRefFrames;
		libraryInfo.processorCount = global->avcdecInfo.numOfStreams;
		decoderInfo.width = pDecoderInfo->horizontal;
		decoderInfo.height = pDecoderInfo->vertical;
		decoderInfo.imageCount = numOfRefFrames;
		if (pCtrl->frameBuf.pBuf != NULL) {
			if (((uintptr_t)pCtrl->frameBuf.pBuf & 0xFFFFF) != 0) {
				ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
				goto avcUnlockRanges;
			}
			ret = sceCodecEngineRegisterUnmapMemory(0, (SceUIntVAddr)pCtrl->frameBuf.pBuf,
				pCtrl->frameBuf.size);
			if (ret == SCE_CODECENGINE_ERROR_INVALID_POINTER) {
				if (videocodecLockRange(&avc->lockedRanges, &physicalAddress,
						(uintptr_t)pCtrl->frameBuf.pBuf, pCtrl->frameBuf.size, 32,
						VIDEOCODEC_CACHE_INVALIDATE, VIDEOCODEC_LOCK_CACHED) < 0) {
					ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
					goto avcUnlockRanges;
				}
				avc->frameBufFlags = 0;
				avc->frameBuf = pCtrl->frameBuf;
			} else {
				if (ret < 0) {
					ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
					goto avcUnlockRanges;
				}
				ret = sceCodecEngineUnmapUserVAtoKernelVA(0, pCtrl->frameBuf.pBuf, &base);
				if (ret < 0) {
					sceCodecEngineUnregisterUnmapMemory(0,
						(SceUIntVAddr)pCtrl->frameBuf.pBuf, pCtrl->frameBuf.size);
					goto avcUnlockRanges;
				}
				avc->frameBufFlags = VIDEOCODEC_FRAME_BUF_UNMAP;
				avc->frameBuf.pBuf = base;
				avc->frameBuf.size = pCtrl->frameBuf.size;
			}
		} else {
			SceSize frameMemSize;

			if (!sceKernelIsGameBudget()) {
				ret = VIDEOCODEC_AVCDEC_ERROR_NOT_GAME_PROCESS;
				goto avcUnlockRanges;
			}
			frameMemSize = 0;
			ret = videocodecQueryDecoderMemSize(SCE_VIDEODEC_TYPE_HW_AVCDEC, pDecoderInfo,
				&frameMemSize);
			if (ret < 0)
				goto avcUnlockRanges;
			memset(&opt, 0, sizeof(opt));
			opt.size = sizeof(opt);
			opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
			opt.alignment = 0x100000;
			ret = sceKernelAllocMemBlockForKernel("SceAvcdecHwFrameBuf",
				SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_CDRAM_RW, frameMemSize, &opt);
			if (ret < 0) {
				ret = SCE_AVCDEC_ERROR_OUT_OF_MEMORY;
				goto avcUnlockRanges;
			}
			avc->frameBufMemBlockId = ret;
			avc->frameBufFlags |= VIDEOCODEC_FRAME_BUF_MEMBLOCK;
			sceKernelGetMemBlockBase(ret, &avc->frameBuf.pBuf);
			avc->frameBuf.size = frameMemSize;
		}
		param.imageVirtualAddress = (uint32_t)avc->frameBuf.pBuf;
		param.imageSize = avc->frameBuf.size;
		param.storageVirtualAddress = (uint32_t)global->avcdecMemory.base;
		param.storageSize = global->avcdecMemory.size;
		param.imageFormat = &decoderInfo;
		param.geometry = &libraryInfo;
		ret = SceCodecEngineWrapperForDriver_9D3B1425(&handle, &param);
		ret = videocodecMapCodecEngineError(ret);
		if (ret >= 0) {
			avc->mode = 0;
			avc->info.horizontal = pDecoderInfo->horizontal;
			avc->info.vertical = pDecoderInfo->vertical;
			avc->info.numOfRefFrames = numOfRefFrames;
			pCtrl->handle = handle;
			avc->handle = handle;
			goto avcUnlock;
		}
		if (avc->frameBufFlags & VIDEOCODEC_FRAME_BUF_MEMBLOCK) {
			sceKernelFreeMemBlock(avc->frameBufMemBlockId);
			avc->frameBufFlags &= ~VIDEOCODEC_FRAME_BUF_MEMBLOCK;
		}
		if (avc->frameBufFlags & VIDEOCODEC_FRAME_BUF_UNMAP) {
			sceCodecEngineUnmapKernelVAtoUserVA(0, avc->frameBuf.pBuf, &base);
			sceCodecEngineUnregisterUnmapMemory(0, (SceUIntVAddr)base,
				avc->frameBuf.size);
			avc->frameBufFlags &= ~VIDEOCODEC_FRAME_BUF_UNMAP;
		}
		__builtin_memset(&avc->frameBuf, 0, sizeof(avc->frameBuf));
avcUnlockRanges:
		videocodecUnlockRanges(&avc->lockedRanges);
avcUnlock:
		sceKernelUnlockFastMutex(&avc->mutex);
	} else {
		Dsp0VideocodecDecoderParam param;
		VideocodecDecoderQuery query;

		if (global->dsp0DecoderInfo.horizontal < pDecoderInfo->horizontal ||
		    global->dsp0DecoderInfo.vertical < pDecoderInfo->vertical ||
		    numOfRefFrames > global->dsp0DecoderInfo.numOfRefFrames) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			goto dsp0UnlockRanges;
		}
		dsp0->frameBufFlags = 0;
		query.info.horizontal = pDecoderInfo->horizontal;
		query.info.vertical = pDecoderInfo->vertical;
		query.info.numOfRefFrames = numOfRefFrames;
		query.mode = global->dsp0Config;
		if (pCtrl->frameBuf.pBuf != NULL) {
			if (((uintptr_t)pCtrl->frameBuf.pBuf & 0xFF) != 0) {
				ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
				goto dsp0UnlockRanges;
			}
			ret = sceCodecEngineRegisterUnmapMemory(0, (SceUIntVAddr)pCtrl->frameBuf.pBuf,
				pCtrl->frameBuf.size);
			if (ret == SCE_CODECENGINE_ERROR_INVALID_POINTER) {
				if (videocodecLockRange(&dsp0->lockedRanges, &physicalAddress,
						(uintptr_t)pCtrl->frameBuf.pBuf, pCtrl->frameBuf.size, 32,
						VIDEOCODEC_CACHE_INVALIDATE, VIDEOCODEC_LOCK_CACHED) < 0) {
					ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
					goto dsp0UnlockRanges;
				}
				dsp0->frameBuf = pCtrl->frameBuf;
			} else {
				if (ret < 0) {
					ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
					goto dsp0UnlockRanges;
				}
				ret = sceCodecEngineUnmapUserVAtoKernelVA(0, pCtrl->frameBuf.pBuf, &base);
				if (ret < 0) {
					sceCodecEngineUnregisterUnmapMemory(0,
						(SceUIntVAddr)pCtrl->frameBuf.pBuf, pCtrl->frameBuf.size);
					goto dsp0UnlockRanges;
				}
				dsp0->frameBufFlags |= VIDEOCODEC_FRAME_BUF_UNMAP;
				dsp0->frameBuf.pBuf = base;
				dsp0->frameBuf.size = pCtrl->frameBuf.size;
			}
		} else {
			SceSize frameMemSize;

			ret = VIDEOCODEC_AVCDEC_ERROR_NOT_GAME_PROCESS;
			if (!sceKernelIsGameBudget())
				goto dsp0UnlockRanges;
			frameMemSize = 0;
			ret = videocodecQueryDecoderMemSize(codecType, pDecoderInfo, &frameMemSize);
			if (ret < 0)
				goto dsp0UnlockRanges;
			memset(&opt, 0, sizeof(opt));
			opt.size = sizeof(opt);
			opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
			opt.alignment = 0x100000;
			ret = sceKernelAllocMemBlockForKernel("SceAvcdecSwFrameBuf",
				SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_CDRAM_RW, frameMemSize, &opt);
			if (ret < 0) {
				ret = SCE_AVCDEC_ERROR_OUT_OF_MEMORY;
				goto dsp0UnlockRanges;
			}
			dsp0->frameBufMemBlockId = ret;
			dsp0->frameBufFlags |= VIDEOCODEC_FRAME_BUF_MEMBLOCK;
			sceKernelGetMemBlockBase(ret, &dsp0->frameBuf.pBuf);
			dsp0->frameBuf.size = frameMemSize;
		}
		param.pFrameBuf = dsp0->frameBuf.pBuf;
		param.frameBufSize = dsp0->frameBuf.size;
		param.pInfo = &query;
		ret = dsp0VideocodecCreateAvcDecoder(global->dsp0Library, &handle, &param);
		ret = videocodecMapDecoderError(ret);
		if (ret >= 0) {
			dsp0->mode = 0;
			dsp0->info.horizontal = pDecoderInfo->horizontal;
			dsp0->info.vertical = pDecoderInfo->vertical;
			dsp0->info.numOfRefFrames = numOfRefFrames;
			dsp0->handle = handle;
			pCtrl->handle = handle - 1;
			goto dsp0Unlock;
		}
		if (dsp0->frameBufFlags & VIDEOCODEC_FRAME_BUF_MEMBLOCK) {
			sceKernelFreeMemBlock(dsp0->frameBufMemBlockId);
			dsp0->frameBufFlags &= ~VIDEOCODEC_FRAME_BUF_MEMBLOCK;
		}
		if (dsp0->frameBufFlags & VIDEOCODEC_FRAME_BUF_UNMAP) {
			sceCodecEngineUnmapKernelVAtoUserVA(0, dsp0->frameBuf.pBuf, &base);
			sceCodecEngineUnregisterUnmapMemory(0, (SceUIntVAddr)base,
				dsp0->frameBuf.size);
			dsp0->frameBufFlags &= ~VIDEOCODEC_FRAME_BUF_UNMAP;
		}
		__builtin_memset(&dsp0->frameBuf, 0, sizeof(dsp0->frameBuf));
dsp0UnlockRanges:
		videocodecUnlockRanges(&dsp0->lockedRanges);
dsp0Unlock:
		sceKernelUnlockFastMutex(&dsp0->mutex);
	}
	sceKernelUnlockFastMutex(global->pAllMutex);
	return ret;

notInitialized:
	sceKernelUnlockFastMutex(global->pAllMutex);
	return SCE_AVCDEC_ERROR_NOT_INITIALIZE;
alreadyUsed:
	sceKernelUnlockFastMutex(global->pAllMutex);
	return SCE_AVCDEC_ERROR_ALREADY_USED;
}

int videocodecConvertValidationError(int result)
{
	unsigned int index = (unsigned int)result - 0x80620101;

	if (index <= 13)
		result = videocodecCodecEngineErrors[index];
	if (((unsigned int)result & 0xFFFFFF00) == 0x80620000)
		return result | 0x800;
	return result;
}

int videocodecConvertCodecEngineError(int result)
{
	unsigned int index = (unsigned int)result - 0x80620101;

	if (index <= 13)
		result = videocodecCodecEngineErrors[index];
	if (((unsigned int)result & 0xFFFFFF00) == 0x80620000)
		return result | 0x800;
	return result;
}

int videocodecConvertDecoderError(int result)
{
	unsigned int index = (unsigned int)result - 0x80620201;

	if (index <= 16) {
		result = videocodecDecoderErrors[index];
	} else {
		if (result == (int)0x80620380)
			result = SCE_AVCDEC_ERROR_INVALID_STREAM;
		else if (result == (int)0x806203C0)
			result = SCE_AVCDEC_ERROR_INVALID_STATE;
	}
	if (((unsigned int)result & 0xFFFFFF00) == 0x80620000)
		return result | 0x800;
	return result;
}

int SceAvcodecForDriver_C8366BB8(unsigned int codecType, unsigned int flag)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global;
	unsigned int interlaced, flags;
	int ret, err;

	if (codecType != 4097 || (flag & ~1U) != 0) {
		sceSyscallLeave(syscall);
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	}
	interlaced = flag != 0 ? 0x80 : 0;
	global = _GetVideocodecGlobal(0);
	ret = lockFastMutex(global->pAllMutex);
	if (ret >= 0) {
		flags = global->avcdecFlags;
		ret = SceCodecEngineWrapperForDriver_62996F94(0, 0);
		ret = videocodecMapCodecEngineError(ret);
		if (ret < 0) {
			sceKernelUnlockFastMutex(global->pAllMutex);
		} else {
			global->avcdecFlags = interlaced | (flags & ~0x80U);
			err = sceKernelUnlockFastMutex(global->pAllMutex);
			if (ret == 0)
				ret = err;
		}
	}
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_20C83C00(unsigned int codecType, unsigned int mode)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global;
	unsigned int bit, flags;
	int ret, err;

	if (codecType != 4097 || (mode & ~1U) != 0) {
		sceSyscallLeave(syscall);
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	}
	bit = mode != 0 ? 0x100 : 0;
	global = _GetVideocodecGlobal(0);
	ret = lockFastMutex(global->pAllMutex);
	if (ret >= 0) {
		flags = bit | (global->avcdecFlags & ~0x100U);
		ret = SceCodecEngineWrapperForDriver_62996F94((flags & 0x100) << 16, 0x1000000);
		ret = videocodecMapCodecEngineError(ret);
		if (ret < 0) {
			sceKernelUnlockFastMutex(global->pAllMutex);
		} else {
			global->avcdecFlags = flags;
			err = sceKernelUnlockFastMutex(global->pAllMutex);
			if (ret == 0)
				ret = err;
		}
	}
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_3CB7661A(unsigned int codecType, unsigned int mode)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global;
	unsigned int bits, flags;
	int ret, err;

	if (codecType != 4097 || (mode & ~6U) != 0) {
		sceSyscallLeave(syscall);
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	}
	bits = (mode & 2) != 0 ? 2 : 0;
	if ((mode & 4) != 0)
		bits = (mode & 2) != 0 ? 6 : 4;
	global = _GetVideocodecGlobal(0);
	ret = lockFastMutex(global->pAllMutex);
	if (ret >= 0) {
		flags = bits | (global->avcdecFlags & ~6U);
		ret = SceCodecEngineWrapperForDriver_62996F94(bits << 16, 0x60000);
		ret = videocodecMapCodecEngineError(ret);
		if (ret < 0) {
			sceKernelUnlockFastMutex(global->pAllMutex);
		} else {
			global->avcdecFlags = flags;
			err = sceKernelUnlockFastMutex(global->pAllMutex);
			if (ret == 0)
				ret = err;
		}
	}
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_73401748(unsigned int codecType, unsigned int mode)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global;
	unsigned int value, flags;
	int ret, err;

	if (codecType != 4097 || (mode & ~0x80U) != 0)
		goto invalidParam;
	global = _GetVideocodecGlobal(0);
	value = mode & 0x7F;
	if (value != 0)
		goto invalidParam;
	ret = lockFastMutex(global->pAllMutex);
	if (ret >= 0) {
		flags = global->avcdecFlags;
		ret = SceCodecEngineWrapperForDriver_62996F94(value, value);
		ret = videocodecMapCodecEngineError(ret);
		if (ret < 0) {
			sceKernelUnlockFastMutex(global->pAllMutex);
		} else {
			global->avcdecFlags = mode | (flags & ~0x80U);
			err = sceKernelUnlockFastMutex(global->pAllMutex);
			if (ret == 0)
				ret = err;
		}
	}
	sceSyscallLeave(syscall);
	return ret;

invalidParam:
	sceSyscallLeave(syscall);
	return SCE_AVCDEC_ERROR_INVALID_PARAM;
}

int SceAvcodecForDriver_866B49BF(unsigned int codecType, unsigned int mode)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global;
	unsigned int flags;
	int ret, err;

	if (codecType - 4097 > 1)
		goto invalidParam;
	global = _GetVideocodecGlobal(0);
	if (codecType == 4097) {
		if ((mode & 0x71) != 0)
			goto invalidParam;
		ret = lockFastMutex(global->pAllMutex);
		if (ret < 0)
			goto out;
		flags = mode | (global->avcdecFlags & ~0x8EU);
		err = SceCodecEngineWrapperForDriver_62996F94((flags << 16) & 0x60000, 0x60000);
		err = videocodecMapCodecEngineError(err);
		if (err < 0) {
			sceKernelUnlockFastMutex(global->pAllMutex);
			ret = err;
			goto out;
		}
		global->avcdecFlags = flags;
	} else {
		if (mode > 2)
			goto invalidParam;
		err = lockFastMutex(global->pAllMutex);
		if (err < 0) {
			ret = err;
			goto out;
		}
		global->dsp0Config |= mode;
	}
	ret = sceKernelUnlockFastMutex(global->pAllMutex);
	if (err != 0)
		ret = err;
out:
	sceSyscallLeave(syscall);
	return ret;

invalidParam:
	sceSyscallLeave(syscall);
	return SCE_AVCDEC_ERROR_INVALID_PARAM;
}

int SceAvcodecForDriver_A2085B6B(unsigned int codecType, unsigned int size)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global;
	int ret, err;

	if (codecType != 4097) {
		sceSyscallLeave(syscall);
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	}
	global = _GetVideocodecGlobal(0);
	ret = lockFastMutex(global->pAllMutex);
	if (ret >= 0) {
		if (global->avcdecInitCount > 0) {
			sceKernelUnlockFastMutex(global->pAllMutex);
			ret = SCE_AVCDEC_ERROR_ALREADY_USED;
		} else {
			err = SceCodecEngineWrapperForDriver_54F39D1D(size);
			err = videocodecMapCodecEngineError(err);
			ret = sceKernelUnlockFastMutex(global->pAllMutex);
			if (err != 0)
				ret = err;
		}
	}
	sceSyscallLeave(syscall);
	return ret;
}

int videocodecValidatePublicDecoderInfo(unsigned int codecType,
		const SceAvcdecQueryDecoderInfo *pDecoderInfo)
{
	unsigned int horizontal = pDecoderInfo->horizontal;
	unsigned int vertical, numOfRefFrames, imageSize;

	(void)codecType;
	if (horizontal > 1920)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	vertical = pDecoderInfo->vertical;
	if (vertical > 1088)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	if (((vertical | horizontal) & 15) != 0)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;

	numOfRefFrames = pDecoderInfo->numOfRefFrames;
	if (numOfRefFrames > 16)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	imageSize = ((vertical + 15) & ~15U) * ((horizontal + 15) & ~15U);
	if (imageSize > 0xE1000)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	return 3 * numOfRefFrames * imageSize <= 0xD2F001 ?
		0 : SCE_AVCDEC_ERROR_INVALID_PARAM;
}

int videocodecValidateDecoderInfo(unsigned int codecType,
		const SceAvcdecQueryDecoderInfo *pDecoderInfo)
{
	unsigned int horizontal = pDecoderInfo->horizontal;
	unsigned int vertical, numOfRefFrames, imageSize;

	(void)codecType;
	if (horizontal > 1920)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	vertical = pDecoderInfo->vertical;
	if (vertical > 1088)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	if (((vertical | horizontal) & 15) != 0)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	numOfRefFrames = pDecoderInfo->numOfRefFrames;
	if (numOfRefFrames > 16)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	imageSize = ((vertical + 15) & ~15U) * ((horizontal + 15) & ~15U);
	if (imageSize > 0x1FE000)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	return 3 * numOfRefFrames * imageSize <= 0x1800001 ?
		0 : SCE_AVCDEC_ERROR_INVALID_PARAM;
}

int SceAvcodecForDriver_C114C499(unsigned int codecType,
		const SceAvcdecQueryDecoderInfo *pQuery, SceAvcdecDecoderInfo *pDecoderInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	unsigned int horizontal, vertical, numOfRefFrames, imageSize;
	int ret;

	if (codecType != 4097)
		goto invalidParam;
	if (pDecoderInfo == NULL || pQuery == NULL) {
		ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
		goto out;
	}
	horizontal = pQuery->horizontal;
	if (horizontal > 1920)
		goto invalidParam;
	vertical = pQuery->vertical;
	if (vertical > 1088 || ((vertical | horizontal) & 15) != 0)
		goto invalidParam;
	numOfRefFrames = pQuery->numOfRefFrames;
	if (numOfRefFrames > 16)
		goto invalidParam;
	vertical = (vertical + 15) & ~15U;
	horizontal = (horizontal + 15) & ~15U;
	imageSize = vertical * horizontal;
	if (imageSize > 0xE1000)
		goto invalidParam;
	numOfRefFrames *= 3;
	imageSize = numOfRefFrames * imageSize;
	if (imageSize > 0xD2F001)
		goto invalidParam;
	ret = videocodecQueryDecoderMemSizeInline(codecType, pQuery, &pDecoderInfo->frameMemSize);
out:
	sceSyscallLeave(syscall);
	return ret;
invalidParam:
	ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
	goto out;
}

int SceAvcodecForDriver_01387A93(unsigned int codecType,
		const SceAvcdecQueryDecoderInfo *pQuery, SceAvcdecDecoderInfo *pDecoderInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	unsigned int horizontal, vertical, numOfRefFrames, imageSize;
	int ret;

	if (codecType != 4097)
		goto invalidParam;
	if (pDecoderInfo == NULL || pQuery == NULL) {
		ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
		goto out;
	}
	horizontal = pQuery->horizontal;
	if (horizontal > 1920)
		goto invalidParam;
	vertical = pQuery->vertical;
	if (vertical > 1088 || ((vertical | horizontal) & 15) != 0)
		goto invalidParam;
	numOfRefFrames = pQuery->numOfRefFrames;
	if (numOfRefFrames > 16)
		goto invalidParam;
	vertical = (vertical + 15) & ~15U;
	horizontal = (horizontal + 15) & ~15U;
	imageSize = vertical * horizontal;
	if (imageSize > 0x1FE000)
		goto invalidParam;
	numOfRefFrames *= 3;
	imageSize = numOfRefFrames * imageSize;
	if (imageSize > 0x1800001)
		goto invalidParam;
	ret = videocodecQueryDecoderMemSizeInline(codecType, pQuery, &pDecoderInfo->frameMemSize);
out:
	sceSyscallLeave(syscall);
	return ret;
invalidParam:
	ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
	goto out;
}

int SceAvcodecForDriver_D5A34236(unsigned int codecType,
		const SceAvcdecQueryDecoderInfo *pQuery, SceAvcdecDecoderInfo *pDecoderInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	if (codecType - 4097 > 1)
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
	else if (pDecoderInfo == NULL || pQuery == NULL)
		ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
	else
		ret = videocodecQueryDecoderMemSizeInline(codecType, pQuery,
			&pDecoderInfo->frameMemSize);
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_7E8CC4E5(unsigned int codecType,
		SceAvcdecCtrl *pCtrl, const SceAvcdecQueryDecoderInfo *pDecoderInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	unsigned int horizontal, vertical, numOfRefFrames, imageSize;
	int ret;

	if (codecType != 4097)
		goto invalidParam;
	if (pCtrl == NULL || pDecoderInfo == NULL) {
		ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
		goto out;
	}
	horizontal = pDecoderInfo->horizontal;
	if (horizontal > 1920)
		goto invalidParam;
	vertical = pDecoderInfo->vertical;
	if (vertical > 1088 || ((vertical | horizontal) & 15) != 0)
		goto invalidParam;
	numOfRefFrames = pDecoderInfo->numOfRefFrames;
	if (numOfRefFrames > 16)
		goto invalidParam;
	vertical = (vertical + 15) & ~15U;
	horizontal = (horizontal + 15) & ~15U;
	imageSize = vertical * horizontal;
	if (imageSize > 0xE1000)
		goto invalidParam;
	numOfRefFrames *= 3;
	imageSize = numOfRefFrames * imageSize;
	if (imageSize > 0xD2F001)
		goto invalidParam;
	ret = videocodecCreateDecoder(codecType, pCtrl, pDecoderInfo);
out:
	sceSyscallLeave(syscall);
	return ret;
invalidParam:
	ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
	goto out;
}

int SceAvcodecForDriver_301CCA25(unsigned int codecType,
		SceAvcdecCtrl *pCtrl, const SceAvcdecQueryDecoderInfo *pDecoderInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	unsigned int horizontal, vertical, numOfRefFrames, imageSize;
	int ret;

	if (codecType != 4097)
		goto invalidParam;
	if (pCtrl == NULL || pDecoderInfo == NULL) {
		ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
		goto out;
	}
	horizontal = pDecoderInfo->horizontal;
	if (horizontal > 1920)
		goto invalidParam;
	vertical = pDecoderInfo->vertical;
	if (vertical > 1088 || ((vertical | horizontal) & 15) != 0)
		goto invalidParam;
	numOfRefFrames = pDecoderInfo->numOfRefFrames;
	if (numOfRefFrames > 16)
		goto invalidParam;
	vertical = (vertical + 15) & ~15U;
	horizontal = (horizontal + 15) & ~15U;
	imageSize = vertical * horizontal;
	if (imageSize > 0x1FE000)
		goto invalidParam;
	numOfRefFrames *= 3;
	imageSize = numOfRefFrames * imageSize;
	if (imageSize > 0x1800001)
		goto invalidParam;
	ret = videocodecCreateDecoder(codecType, pCtrl, pDecoderInfo);
out:
	sceSyscallLeave(syscall);
	return ret;
invalidParam:
	ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
	goto out;
}

int SceAvcodecForDriver_663B3195(unsigned int codecType,
		SceAvcdecCtrl *pCtrl, const SceAvcdecQueryDecoderInfo *pDecoderInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	if (codecType - 4097 > 1) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
	} else {
		if (pCtrl == NULL || pDecoderInfo == NULL)
			ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
		else
			ret = videocodecCreateDecoder(codecType, pCtrl, pDecoderInfo);
	}
	sceSyscallLeave(syscall);
	return ret;
}

/*
 * Deletes pid's AVC decoder pCtrl->handle names (1 to 255 on the codec engine, from 256 on the
 * DSP0) and frees its frame buffer, unless the delete was stopped by a thread suspend.
 */
static inline __attribute__((always_inline)) int
videocodecDeleteDecoderInline(SceAvcdecCtrl *pCtrl, unsigned int flags, SceUID pid)
{
	VideocodecGlobal *global = _GetVideocodecGlobal(pid);
	unsigned int handle;
	void *base;
	int ret, err;

	if (pCtrl == NULL)
		return SCE_AVCDEC_ERROR_INVALID_POINTER;
	handle = pCtrl->handle;
	if (handle == 0)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		return ret;
	if (handle < 256) {
		VideocodecAvcDecoder *avc;

		if (handle > global->avcdecInfo.numOfStreams)
			goto invalidParam;
		avc = &global->avcDecoders[handle - 1];
		ret = lockFastMutex(&avc->mutex);
		if (ret < 0)
			goto unlock;
		if (handle != avc->handle) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&avc->mutex);
			goto unlock;
		}
		sceKernelUnlockFastMutex(global->pAllMutex);
		err = SceCodecEngineWrapperForDriver_B280BA55(handle, flags, pid);
		err = videocodecMapCodecEngineError(err);
		if (err == SCE_KERNEL_ERROR_THREAD_STOPPED || err == SCE_KERNEL_ERROR_THREAD_SUSPENDED) {
			ret = err;
		} else {
			avc->handle = 0;
			if (avc->frameBufFlags & VIDEOCODEC_FRAME_BUF_MEMBLOCK) {
				sceKernelFreeMemBlock(avc->frameBufMemBlockId);
				avc->frameBufFlags &= ~VIDEOCODEC_FRAME_BUF_MEMBLOCK;
			}
			if (avc->frameBufFlags & VIDEOCODEC_FRAME_BUF_UNMAP) {
				sceCodecEngineUnmapKernelVAtoUserVA(pid, avc->frameBuf.pBuf, &base);
				ret = sceCodecEngineUnregisterUnmapMemory(pid, (SceUIntVAddr)base,
					avc->frameBuf.size);
				avc->frameBufFlags &= ~VIDEOCODEC_FRAME_BUF_UNMAP;
			}
			__builtin_memset(&avc->frameBuf, 0, sizeof(avc->frameBuf));
			videocodecUnlockRangesProc(&avc->lockedRanges, pid);
		}
		sceKernelUnlockFastMutex(&avc->mutex);
	} else {
		VideocodecDsp0Decoder *dsp0;
		unsigned int index = handle - 256;

		if (index >= global->dsp0DecoderInfo.numOfStreams)
			goto invalidParam;
		dsp0 = &global->dsp0Decoders[index];
		ret = lockFastMutex(&dsp0->mutex);
		if (ret < 0)
			goto unlock;
		handle = dsp0->handle;
		if (handle == 0) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&dsp0->mutex);
			goto unlock;
		}
		sceKernelUnlockFastMutex(global->pAllMutex);
		err = dsp0VideocodecDeleteAvcDecoder(handle, pid);
		err = videocodecMapDecoderError(err);
		if (err == SCE_KERNEL_ERROR_THREAD_STOPPED || err == SCE_KERNEL_ERROR_THREAD_SUSPENDED) {
			ret = err;
		} else {
			dsp0->handle = 0;
			if (dsp0->frameBufFlags & VIDEOCODEC_FRAME_BUF_MEMBLOCK) {
				sceKernelFreeMemBlock(dsp0->frameBufMemBlockId);
				dsp0->frameBufFlags &= ~VIDEOCODEC_FRAME_BUF_MEMBLOCK;
			}
			if (dsp0->frameBufFlags & VIDEOCODEC_FRAME_BUF_UNMAP) {
				sceCodecEngineUnmapKernelVAtoUserVA(pid, dsp0->frameBuf.pBuf, &base);
				ret = sceCodecEngineUnregisterUnmapMemory(pid, (SceUIntVAddr)base,
					dsp0->frameBuf.size);
				dsp0->frameBufFlags &= ~VIDEOCODEC_FRAME_BUF_UNMAP;
			}
			__builtin_memset(&dsp0->frameBuf, 0, sizeof(dsp0->frameBuf));
			videocodecUnlockRangesProc(&dsp0->lockedRanges, pid);
		}
		sceKernelUnlockFastMutex(&dsp0->mutex);
	}
	if (ret != 0)
		return ret;
	return err < 0 ? err : 0;

invalidParam:
	ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
unlock:
	sceKernelUnlockFastMutex(global->pAllMutex);
	return ret;
}

int videocodecDeleteDecoderForProcess(SceAvcdecCtrl *pCtrl, unsigned int flags, SceUID pid)
{
	return videocodecDeleteDecoderInline(pCtrl, flags, pid);
}

int SceAvcodecForDriver_EA07A3B7(SceAvcdecCtrl *pCtrl)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	ret = videocodecDeleteDecoderInline(pCtrl, 1, 0);
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_43A9CD35(SceAvcdecCtrl *pCtrl)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	unsigned int handle, index, imageSize;
	int ret, limit;

	if (pCtrl == NULL) {
		ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
		goto out;
	}
	handle = pCtrl->handle;
	if (handle == 0) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		goto out;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto out;
	if (handle < 256) {
		VideocodecAvcDecoder *avc;

		if (handle > global->avcdecInfo.numOfStreams)
			goto invalidParam;
		avc = &global->avcDecoders[handle - 1];
		ret = lockFastMutex(&avc->mutex);
		if (ret < 0)
			goto unlock;
		if (avc->handle != handle) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&avc->mutex);
			goto unlock;
		}
		sceKernelUnlockFastMutex(global->pAllMutex);
		ret = videocodecMapCodecEngineError(SceCodecEngineWrapperForDriver_6A3638A8(handle));
		imageSize = avc->info.horizontal * avc->info.vertical;
		if (imageSize <= 0xE1000)
			limit = VIDEOCODEC_M4VDEC_MAX_AVAILABLE_SIZE;
		else
			limit = (3 * imageSize) >> 2;
		if (ret > limit)
			ret = limit;
		sceKernelUnlockFastMutex(&avc->mutex);
	} else {
		VideocodecDsp0Decoder *dsp0;

		index = handle - 256;
		if (index >= global->dsp0DecoderInfo.numOfStreams)
			goto invalidParam;
		dsp0 = &global->dsp0Decoders[index];
		ret = lockFastMutex(&dsp0->mutex);
		if (ret < 0)
			goto unlock;
		handle = dsp0->handle;
		if (handle == 0) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&dsp0->mutex);
			goto unlock;
		}
		sceKernelUnlockFastMutex(global->pAllMutex);
		ret = dsp0VideocodecAvcDecodeAvailableSize(handle);
		index = (unsigned int)ret - 0x80620201;
		if (index <= 16) {
			ret = videocodecDecoderErrors[index];
			if (ret > VIDEOCODEC_M4VDEC_MAX_AVAILABLE_SIZE)
				ret = VIDEOCODEC_M4VDEC_MAX_AVAILABLE_SIZE;
		} else if (ret == (int)0x80620380) {
			ret = SCE_AVCDEC_ERROR_INVALID_STREAM;
		} else if (ret == (int)0x806203C0) {
			ret = SCE_AVCDEC_ERROR_INVALID_STATE;
		} else if (ret > VIDEOCODEC_M4VDEC_MAX_AVAILABLE_SIZE) {
			ret = VIDEOCODEC_M4VDEC_MAX_AVAILABLE_SIZE;
		}
		sceKernelUnlockFastMutex(&dsp0->mutex);
	}
out:
	sceSyscallLeave(syscall);
	return ret;

invalidParam:
	ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
unlock:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto out;
}

int SceAvcodecForDriver_C518A109(SceAvcdecCtrl *pCtrl, int trickMode)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	unsigned int handle, index;
	int ret;

	if (pCtrl == NULL) {
		ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
		goto out;
	}
	handle = pCtrl->handle;
	if (handle == 0) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		goto out;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto out;
	if (handle < 256) {
		VideocodecAvcDecoder *avc;

		if (handle > global->avcdecInfo.numOfStreams)
			goto invalidParam;
		avc = &global->avcDecoders[handle - 1];
		ret = lockFastMutex(&avc->mutex);
		if (ret < 0)
			goto unlock;
		if (avc->handle != handle) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&avc->mutex);
			goto unlock;
		}
		sceKernelUnlockFastMutex(global->pAllMutex);
		if ((unsigned int)(trickMode - 1) <= 1)
			ret = videocodecMapCodecEngineError(
				SceCodecEngineWrapperForDriver_8DEF9631(handle, trickMode == 2));
		else
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		sceKernelUnlockFastMutex(&avc->mutex);
	} else {
		VideocodecDsp0Decoder *dsp0;

		index = handle - 256;
		if (index >= global->dsp0DecoderInfo.numOfStreams)
			goto invalidParam;
		dsp0 = &global->dsp0Decoders[index];
		ret = lockFastMutex(&dsp0->mutex);
		if (ret < 0)
			goto unlock;
		if (dsp0->handle == 0) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&dsp0->mutex);
			goto unlock;
		}
		sceKernelUnlockFastMutex(global->pAllMutex);
		sceKernelUnlockFastMutex(&dsp0->mutex);
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
	}
out:
	sceSyscallLeave(syscall);
	return ret;

invalidParam:
	ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
unlock:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto out;
}

/*
 * Submits the NAL unit access unit pAu to the codec-engine AVC decoder pCtrl names, with its
 * additional data and payload locked for the call. The DSP0's decoders do not take one.
 */
static inline __attribute__((always_inline)) int
videocodecDecodeNalAuInline(SceAvcdecCtrl *pCtrl, const VeneziaVipVideoInput *pAu,
		const SceUInt *timeout)
{
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	VeneziaVipVideoInput input;
	VideocodecLockedRanges ranges;
	VideocodecLockedRange lockedRange[3];
	uintptr_t physicalAddress;
	unsigned int handle, index;
	int ret;

	if (pAu == NULL || pCtrl == NULL)
		return SCE_AVCDEC_ERROR_INVALID_POINTER;
	handle = pCtrl->handle;
	if (handle == 0)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		return ret;
	if (handle < 256) {
		VideocodecAvcDecoder *avc;

		if (handle > global->avcdecInfo.numOfStreams)
			goto invalidParam;
		avc = &global->avcDecoders[handle - 1];
		ret = lockFastMutex(&avc->mutex);
		if (ret < 0)
			goto unlock;
		if (handle != avc->handle) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&avc->mutex);
			goto unlock;
		}
		sceKernelUnlockFastMutex(global->pAllMutex);
		videocodecInitLockedRanges(&ranges, 0, lockedRange, 3, VIDEOCODEC_DECODER_ERROR_BASE);
		if (pAu->flags & 1) {
			ret = videocodecLockRange(&ranges, &physicalAddress,
				(uintptr_t)pAu->additionalData[0].data, pAu->additionalData[0].size,
				16, 0, VIDEOCODEC_LOCK_CACHED | VIDEOCODEC_LOCK_SCATTERED);
			if (ret < 0)
				goto unlockRanges;
		}
		if (pAu->flags & 2) {
			ret = videocodecLockRange(&ranges, &physicalAddress,
				(uintptr_t)pAu->additionalData[1].data, pAu->additionalData[1].size,
				16, 0, VIDEOCODEC_LOCK_CACHED | VIDEOCODEC_LOCK_SCATTERED);
			if (ret < 0)
				goto unlockRanges;
		}
		ret = videocodecLockRange(&ranges, &physicalAddress,
			(uintptr_t)pAu->payload.data, pAu->payload.size,
			16, 0, VIDEOCODEC_LOCK_CACHED | VIDEOCODEC_LOCK_SCATTERED);
		if (ret >= 0) {
			input.presentationTimestamp = pAu->presentationTimestamp;
			input.decodingTimestamp = pAu->decodingTimestamp;
			input.additionalData[0] = pAu->additionalData[0];
			input.additionalData[1] = pAu->additionalData[1];
			input.payload = pAu->payload;
			input.nalLengthSize = pAu->nalLengthSize;
			input.flags = pAu->flags;
			ret = SceCodecEngineWrapperForDriver_972F0D0F(handle, &input, timeout);
			ret = videocodecMapCodecEngineError(ret);
		}
unlockRanges:
		videocodecUnlockRanges(&ranges);
		sceKernelUnlockFastMutex(&avc->mutex);
		return ret;
	} else {
		VideocodecDsp0Decoder *dsp0;

		index = handle - 256;
		if (index >= global->dsp0DecoderInfo.numOfStreams)
			goto invalidParam;
		dsp0 = &global->dsp0Decoders[index];
		ret = lockFastMutex(&dsp0->mutex);
		if (ret < 0)
			goto unlock;
		if (dsp0->handle == 0) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&dsp0->mutex);
			goto unlock;
		}
		sceKernelUnlockFastMutex(global->pAllMutex);
		videocodecInitLockedRanges(&ranges, 0, lockedRange, 3, VIDEOCODEC_DECODER_ERROR_BASE);
		videocodecUnlockRanges(&ranges);
		sceKernelUnlockFastMutex(&dsp0->mutex);
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	}

invalidParam:
	ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
unlock:
	sceKernelUnlockFastMutex(global->pAllMutex);
	return ret;
}

int SceAvcodecForDriver_C9EF7057(SceAvcdecCtrl *pCtrl, const VeneziaVipVideoInput *pAu,
		const SceUInt *timeout)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	if (timeout != NULL && *timeout < 2000)
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
	else
		ret = videocodecDecodeNalAuInline(pCtrl, pAu, timeout);
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_F714DB9A(SceAvcdecCtrl *pCtrl, const VeneziaVipVideoInput *pAu,
		const SceUInt *timeout)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	ret = videocodecDecodeNalAuInline(pCtrl, pAu, timeout);
	sceSyscallLeave(syscall);
	return ret;
}

/*
 * Decodes the NAL unit access unit pAu on the AVC decoder pCtrl names. The one picture
 * pArrayPicture may name is copied in from the caller, filled from the codec engine's
 * output, and copied back out.
 */
int SceAvcodecForDriver_0CC84413(const SceAvcdecCtrl *pCtrl, const VeneziaVipVideoInput *pAu,
	SceAvcdecArrayPicture *pArrayPicture)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global;
	VideocodecAvcDecoder *avc;
	SceAvcdecPicture **userPicture = NULL, **kernelPicture = NULL;
	SceAvcdecPicture *userPictures[1], *pictures[1];
	VeneziaDebugSnapshotOutput *pOutput;
	VeneziaDebugSnapshotList outputList;
	VeneziaVipVideoInput input;
	VideocodecLockedRanges ranges;
	VideocodecLockedRange lockedRange[3];
	uintptr_t physicalAddress;
	unsigned int handle, index, numOfOutput;
	char pictureBuf[sizeof(SceAvcdecPicture)];
	int ret, copied = 0, err;

	if (pArrayPicture != NULL && pArrayPicture->numOfElm == 1) {
		userPicture = pArrayPicture->pPicture;
		if (userPicture != NULL) {
			copied = sceKernelCopyFromUser(userPictures, userPicture,
				sizeof(userPictures));
			if (copied < 0 || (copied = videocodecCopyInPicturesInline(
					pArrayPicture->numOfElm, (void **)pictures, pictureBuf,
					(void *const *)userPictures, sizeof(SceAvcdecPicture))) < 0) {
				pArrayPicture->pPicture = userPicture;
				goto out;
			}
			pArrayPicture->pPicture = pictures;
			kernelPicture = pictures;
		}
	}
	global = _GetVideocodecGlobal(0);
	if (pCtrl == NULL) {
		ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
		goto copyOut;
	}
	if (pAu == NULL && pArrayPicture == NULL) {
		copied = SCE_AVCDEC_ERROR_INVALID_POINTER;
		goto out;
	}
	if (pArrayPicture != NULL) {
		if (pArrayPicture->numOfElm == 1) {
			if (pArrayPicture->pPicture[0] == NULL) {
				ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
				goto copyOut;
			}
		} else if (pArrayPicture->numOfElm > 1) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			goto copyOut;
		}
	}
	handle = pCtrl->handle;
	if (handle == 0) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		goto copyOut;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto copyOut;
	if (handle >= 256) {
		VideocodecDsp0Decoder *dsp0;

		index = handle - 256;
		if (index >= global->dsp0DecoderInfo.numOfStreams) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			goto unlockAll;
		}
		dsp0 = &global->dsp0Decoders[index];
		ret = lockFastMutex(&dsp0->mutex);
		if (ret < 0)
			goto unlockAll;
		if (dsp0->handle == 0) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&dsp0->mutex);
			goto unlockAll;
		}
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		sceKernelUnlockFastMutex(global->pAllMutex);
		videocodecInitLockedRanges(&ranges, 0, lockedRange, 3, VIDEOCODEC_DECODER_ERROR_BASE);
		videocodecUnlockRanges(&ranges);
		sceKernelUnlockFastMutex(&dsp0->mutex);
		goto copyOut;
	}
	if (handle > global->avcdecInfo.numOfStreams) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		goto unlockAll;
	}
	avc = &global->avcDecoders[handle - 1];
	ret = lockFastMutex(&avc->mutex);
	if (ret < 0)
		goto unlockAll;
	if (handle != avc->handle) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		sceKernelUnlockFastMutex(&avc->mutex);
		goto unlockAll;
	}
	sceKernelUnlockFastMutex(global->pAllMutex);
	videocodecInitLockedRanges(&ranges, 0, lockedRange, 3, VIDEOCODEC_DECODER_ERROR_BASE);
	if (pAu != NULL && pAu->payload.data != NULL) {
		if (pAu->flags & 1) {
			ret = videocodecLockRange(&ranges, &physicalAddress,
				(uintptr_t)pAu->additionalData[0].data, pAu->additionalData[0].size,
				16, 0, VIDEOCODEC_LOCK_CACHED | VIDEOCODEC_LOCK_SCATTERED);
			if (ret < 0)
				goto unlockRanges;
		}
		if (pAu->flags & 2) {
			ret = videocodecLockRange(&ranges, &physicalAddress,
				(uintptr_t)pAu->additionalData[1].data, pAu->additionalData[1].size,
				16, 0, VIDEOCODEC_LOCK_CACHED | VIDEOCODEC_LOCK_SCATTERED);
			if (ret < 0)
				goto unlockRanges;
		}
		ret = videocodecLockRange(&ranges, &physicalAddress,
			(uintptr_t)pAu->payload.data, pAu->payload.size,
			16, 0, VIDEOCODEC_LOCK_CACHED | VIDEOCODEC_LOCK_SCATTERED);
		if (ret < 0)
			goto unlockRanges;
	}
	if (pAu != NULL && pAu->payload.data != NULL) {
		input.presentationTimestamp = pAu->presentationTimestamp;
		input.decodingTimestamp = pAu->decodingTimestamp;
		input.additionalData[0] = pAu->additionalData[0];
		input.additionalData[1] = pAu->additionalData[1];
		input.payload = pAu->payload;
		input.nalLengthSize = pAu->nalLengthSize;
		input.flags = pAu->flags;
	} else {
		input.presentationTimestamp = ~0ULL;
		input.decodingTimestamp = ~0ULL;
		input.additionalData[0].data = NULL;
		input.additionalData[0].size = 0;
		input.additionalData[1].data = NULL;
		input.additionalData[1].size = 0;
		input.payload.data = NULL;
		input.payload.size = 0;
		input.nalLengthSize = 4;
		input.flags = 0;
	}
	outputList.count = 0;
	if (pArrayPicture != NULL) {
		outputList.count = pArrayPicture->numOfElm;
		pArrayPicture->numOfOutput = 0;
	}
	pOutput = &avc->output;
	outputList.snapshots = &pOutput;
	ret = SceCodecEngineWrapperForDriver_BB388700(handle, &input, &outputList, &numOfOutput);
	ret = videocodecMapCodecEngineError(ret);
	if ((ret < 0 && ret != SCE_AVCDEC_ERROR_ES_BUFFER_FULL) ||
	    outputList.count == 0 || numOfOutput == 0)
		goto unlockRanges;
	if (outputList.count < numOfOutput) {
		ret = SCE_AVCDEC_ERROR_INVALID_STATE;
		goto unlockRanges;
	}
	err = videocodecAvcdecPicture((SceAvcdecPictureForInterlaced *)pArrayPicture->pPicture[0],
		pOutput, NULL, (global->avcdecFlags & 0x80) != 0);
	pArrayPicture->numOfOutput = numOfOutput;
	if (ret >= 0 && err < 0)
		ret = err;
unlockRanges:
	videocodecUnlockRanges(&ranges);
	sceKernelUnlockFastMutex(&avc->mutex);
copyOut:
	if (pArrayPicture != NULL && pArrayPicture->numOfElm == 1 && userPicture != NULL) {
		copied = videocodecCopyOutPicturesInline(1, (void *const *)userPictures,
			(void *const *)kernelPicture, sizeof(SceAvcdecPicture));
		pArrayPicture->pPicture = userPicture;
	}
	if (ret < 0)
		copied = ret;
out:
	sceSyscallLeave(syscall);
	return copied;

unlockAll:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto copyOut;
}

/*
 * SceAvcodecForDriver_0CC84413 with up to two work pictures in pWorkPicture, copied in
 * and out like the decoded picture and handed to videocodecAvcdecPicture.
 */
int SceAvcodecForDriver_20892154(const SceAvcdecCtrl *pCtrl, const VeneziaVipVideoInput *pAu,
	SceAvcdecArrayPicture *pArrayPicture, SceAvcdecArrayPicture *pWorkPicture)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global;
	VideocodecAvcDecoder *avc;
	SceAvcdecPicture **userPicture = NULL, **kernelPicture = NULL;
	SceAvcdecPicture **userWork = NULL, **kernelWork = NULL;
	SceAvcdecPicture *userPictures[1], *pictures[1];
	SceAvcdecPicture *userWorks[2], *works[2];
	VeneziaDebugSnapshotOutput *pOutput;
	VeneziaDebugSnapshotList outputList;
	VeneziaVipVideoInput input;
	VideocodecLockedRanges ranges;
	VideocodecLockedRange lockedRange[3];
	uintptr_t physicalAddress;
	unsigned int handle, index, numOfOutput;
	char pictureBuf[sizeof(SceAvcdecPicture)];
	char workBuf[2 * sizeof(SceAvcdecPicture)];
	int ret, copied = 0, err;

	if (pArrayPicture != NULL && pArrayPicture->numOfElm == 1) {
		userPicture = pArrayPicture->pPicture;
		if (userPicture != NULL) {
			copied = sceKernelCopyFromUser(userPictures, userPicture,
				sizeof(userPictures));
			if (copied < 0 || (copied = videocodecCopyInPicturesInline(
					pArrayPicture->numOfElm, (void **)pictures, pictureBuf,
					(void *const *)userPictures, sizeof(SceAvcdecPicture))) < 0) {
				pArrayPicture->pPicture = userPicture;
				goto out;
			}
			pArrayPicture->pPicture = pictures;
			kernelPicture = pictures;
		}
	}
	if (pWorkPicture != NULL && pWorkPicture->numOfElm - 1 <= 1) {
		userWork = pWorkPicture->pPicture;
		if (userWork != NULL) {
			copied = sceKernelCopyFromUser(userWorks, userWork,
				pWorkPicture->numOfElm * sizeof(userWorks[0]));
			if (copied < 0 || (copied = videocodecCopyInPicturesInline(
					pWorkPicture->numOfElm, (void **)works, workBuf,
					(void *const *)userWorks, sizeof(SceAvcdecPicture))) < 0) {
				ret = copied;
				pWorkPicture->pPicture = userWork;
				goto copyOutPicture;
			}
			pWorkPicture->pPicture = works;
			kernelWork = works;
		}
	}
	global = _GetVideocodecGlobal(0);
	if (pCtrl == NULL || (pAu == NULL && pArrayPicture == NULL)) {
		ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
		goto copyOut;
	}
	if (pArrayPicture != NULL) {
		if (pArrayPicture->numOfElm == 1) {
			if (pArrayPicture->pPicture[0] == NULL) {
				ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
				goto copyOut;
			}
		} else if (pArrayPicture->numOfElm > 1) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			goto copyOut;
		}
	}
	handle = pCtrl->handle;
	if (handle == 0) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		goto copyOut;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto copyOut;
	if (handle >= 256) {
		VideocodecDsp0Decoder *dsp0;

		index = handle - 256;
		if (index >= global->dsp0DecoderInfo.numOfStreams) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			goto unlockAll;
		}
		dsp0 = &global->dsp0Decoders[index];
		ret = lockFastMutex(&dsp0->mutex);
		if (ret < 0)
			goto unlockAll;
		if (dsp0->handle == 0) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&dsp0->mutex);
			goto unlockAll;
		}
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		sceKernelUnlockFastMutex(global->pAllMutex);
		videocodecInitLockedRanges(&ranges, 0, lockedRange, 3, VIDEOCODEC_DECODER_ERROR_BASE);
		videocodecUnlockRanges(&ranges);
		sceKernelUnlockFastMutex(&dsp0->mutex);
		goto copyOut;
	}
	if (handle > global->avcdecInfo.numOfStreams) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		goto unlockAll;
	}
	avc = &global->avcDecoders[handle - 1];
	ret = lockFastMutex(&avc->mutex);
	if (ret < 0)
		goto unlockAll;
	if (handle != avc->handle) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		sceKernelUnlockFastMutex(&avc->mutex);
		goto unlockAll;
	}
	sceKernelUnlockFastMutex(global->pAllMutex);
	videocodecInitLockedRanges(&ranges, 0, lockedRange, 3, VIDEOCODEC_DECODER_ERROR_BASE);
	if (pAu != NULL && pAu->payload.data != NULL) {
		if (pAu->flags & 1) {
			ret = videocodecLockRange(&ranges, &physicalAddress,
				(uintptr_t)pAu->additionalData[0].data, pAu->additionalData[0].size,
				16, 0, VIDEOCODEC_LOCK_CACHED | VIDEOCODEC_LOCK_SCATTERED);
			if (ret < 0)
				goto unlockRanges;
		}
		if (pAu->flags & 2) {
			ret = videocodecLockRange(&ranges, &physicalAddress,
				(uintptr_t)pAu->additionalData[1].data, pAu->additionalData[1].size,
				16, 0, VIDEOCODEC_LOCK_CACHED | VIDEOCODEC_LOCK_SCATTERED);
			if (ret < 0)
				goto unlockRanges;
		}
		ret = videocodecLockRange(&ranges, &physicalAddress,
			(uintptr_t)pAu->payload.data, pAu->payload.size,
			16, 0, VIDEOCODEC_LOCK_CACHED | VIDEOCODEC_LOCK_SCATTERED);
		if (ret < 0)
			goto unlockRanges;
	}
	if (pAu != NULL && pAu->payload.data != NULL) {
		input.presentationTimestamp = pAu->presentationTimestamp;
		input.decodingTimestamp = pAu->decodingTimestamp;
		input.additionalData[0] = pAu->additionalData[0];
		input.additionalData[1] = pAu->additionalData[1];
		input.payload = pAu->payload;
		input.nalLengthSize = pAu->nalLengthSize;
		input.flags = pAu->flags;
	} else {
		input.presentationTimestamp = ~0ULL;
		input.decodingTimestamp = ~0ULL;
		input.additionalData[0].data = NULL;
		input.additionalData[0].size = 0;
		input.additionalData[1].data = NULL;
		input.additionalData[1].size = 0;
		input.payload.data = NULL;
		input.payload.size = 0;
		input.nalLengthSize = 4;
		input.flags = 0;
	}
	outputList.count = 0;
	if (pArrayPicture != NULL) {
		outputList.count = pArrayPicture->numOfElm;
		pArrayPicture->numOfOutput = 0;
	}
	pOutput = &avc->output;
	outputList.snapshots = &pOutput;
	ret = SceCodecEngineWrapperForDriver_BB388700(handle, &input, &outputList, &numOfOutput);
	ret = videocodecMapCodecEngineError(ret);
	if ((ret < 0 && ret != SCE_AVCDEC_ERROR_ES_BUFFER_FULL) ||
	    outputList.count == 0 || numOfOutput == 0)
		goto unlockRanges;
	if (outputList.count < numOfOutput) {
		ret = SCE_AVCDEC_ERROR_INVALID_STATE;
		goto unlockRanges;
	}
	err = videocodecAvcdecPicture((SceAvcdecPictureForInterlaced *)pArrayPicture->pPicture[0],
		pOutput, pWorkPicture, (global->avcdecFlags & 0x80) != 0);
	pArrayPicture->numOfOutput = numOfOutput;
	if (ret >= 0 && err < 0)
		ret = err;
unlockRanges:
	videocodecUnlockRanges(&ranges);
	sceKernelUnlockFastMutex(&avc->mutex);
copyOut:
	if (pWorkPicture != NULL && pWorkPicture->numOfElm - 1 <= 1 && userWork != NULL) {
		copied = videocodecCopyOutPicturesInline(pWorkPicture->numOfElm,
			(void *const *)userWorks, (void *const *)kernelWork,
			sizeof(SceAvcdecPicture));
		pWorkPicture->pPicture = userWork;
		if (ret >= 0 && copied < 0)
			ret = copied;
	}
copyOutPicture:
	if (pArrayPicture != NULL && pArrayPicture->numOfElm == 1 && userPicture != NULL) {
		copied = videocodecCopyOutPicturesInline(1, (void *const *)userPictures,
			(void *const *)kernelPicture, sizeof(SceAvcdecPicture));
		pArrayPicture->pPicture = userPicture;
	}
	if (ret < 0)
		copied = ret;
out:
	sceSyscallLeave(syscall);
	return copied;

unlockAll:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto copyOut;
}

/*
 * Decodes the access unit pAu on the AVC decoder pCtrl names, on the codec engine or the
 * DSP0. The one picture pArrayPicture may name is copied in from the caller, filled from
 * the decoder's output, and copied back out.
 */
int SceAvcodecForDriver_48DECA56(const SceAvcdecCtrl *pCtrl, const SceAvcdecAu *pAu,
	SceAvcdecArrayPicture *pArrayPicture)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global;
	VideocodecAvcDecoder *avc = NULL;
	VideocodecDsp0Decoder *dsp0 = NULL;
	SceAvcdecPicture **userPicture = NULL, **kernelPicture = NULL;
	SceAvcdecPicture *userPictures[1], *pictures[1];
	VideocodecLockedRange lockedRange[1];
	VideocodecLockedRanges ranges;
	SceAvcdecAu au;
	uintptr_t physicalAddress;
	unsigned int handle, index, numOfOutput;
	char pictureBuf[sizeof(SceAvcdecPicture)];
	int ret, copied = 0, err;

	if (pArrayPicture != NULL && pArrayPicture->numOfElm == 1) {
		userPicture = pArrayPicture->pPicture;
		if (userPicture != NULL) {
			copied = sceKernelCopyFromUser(userPictures, userPicture,
				sizeof(userPictures));
			if (copied < 0 || (copied = videocodecCopyInPicturesInline(
					pArrayPicture->numOfElm, (void **)pictures, pictureBuf,
					(void *const *)userPictures, sizeof(SceAvcdecPicture))) < 0) {
				pArrayPicture->pPicture = userPicture;
				goto out;
			}
			pArrayPicture->pPicture = pictures;
			kernelPicture = pictures;
		}
	}
	global = _GetVideocodecGlobal(0);
	if (pCtrl == NULL) {
		ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
		goto copyOut;
	}
	if (pAu == NULL && pArrayPicture == NULL) {
		copied = SCE_AVCDEC_ERROR_INVALID_POINTER;
		goto out;
	}
	if (pArrayPicture != NULL) {
		if (pArrayPicture->numOfElm == 1) {
			if (pArrayPicture->pPicture[0] == NULL) {
				ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
				goto copyOut;
			}
		} else if (pArrayPicture->numOfElm > 1) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			goto copyOut;
		}
	}
	handle = pCtrl->handle;
	if (handle == 0) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		goto copyOut;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto copyOut;
	if (handle >= 256) {
		index = handle - 256;
		if (index >= global->dsp0DecoderInfo.numOfStreams) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			goto unlockAll;
		}
		dsp0 = &global->dsp0Decoders[index];
		ret = lockFastMutex(&dsp0->mutex);
		if (ret < 0)
			goto unlockAll;
		handle = dsp0->handle;
		if (handle == 0) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&dsp0->mutex);
			goto unlockAll;
		}
	} else {
		if (handle > global->avcdecInfo.numOfStreams) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			goto unlockAll;
		}
		avc = &global->avcDecoders[handle - 1];
		ret = lockFastMutex(&avc->mutex);
		if (ret < 0)
			goto unlockAll;
		if (handle != avc->handle) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&avc->mutex);
			goto unlockAll;
		}
	}
	sceKernelUnlockFastMutex(global->pAllMutex);
	videocodecInitLockedRanges(&ranges, 0, lockedRange, 1, VIDEOCODEC_DECODER_ERROR_BASE);
	if (pAu != NULL && pAu->es.pBuf != NULL) {
		ret = videocodecLockRange(&ranges, &physicalAddress, (uintptr_t)pAu->es.pBuf,
			pAu->es.size, 16, 0, VIDEOCODEC_LOCK_CACHED | VIDEOCODEC_LOCK_SCATTERED);
		if (ret < 0)
			goto unlockRanges;
	}
	if (pAu != NULL && pAu->es.pBuf != NULL) {
		__builtin_memcpy(&au, pAu, sizeof(au));
	} else {
		au.pts.upper = SCE_VIDEODEC_VOID_TIMESTAMP;
		au.pts.lower = SCE_VIDEODEC_VOID_TIMESTAMP;
		au.dts.upper = SCE_VIDEODEC_VOID_TIMESTAMP;
		au.dts.lower = SCE_VIDEODEC_VOID_TIMESTAMP;
		au.es.pBuf = NULL;
		au.es.size = 0;
	}
	if (dsp0 != NULL) {
		Dsp0VideocodecArrayPicture outputArray;
		Dsp0VideocodecPicture **pOutput;

		outputArray.numOfElm = 0;
		if (pArrayPicture != NULL) {
			outputArray.numOfElm = pArrayPicture->numOfElm;
			pArrayPicture->numOfOutput = 0;
		}
		pOutput = &dsp0->pOutput;
		outputArray.pPicture = &pOutput;
		err = dsp0VideocodecAvcDecode(handle, &au, &outputArray, &numOfOutput);
		ret = videocodecMapDecoderError(err);
		if ((ret < 0 && ret != SCE_AVCDEC_ERROR_ES_BUFFER_FULL) ||
		    outputArray.numOfElm == 0 || numOfOutput == 0)
			goto unlockRanges;
		if (outputArray.numOfElm < numOfOutput) {
			ret = SCE_AVCDEC_ERROR_INVALID_STATE;
			goto unlockRanges;
		}
		err = videocodecDsp0AvcdecPicture(
			(SceAvcdecPictureForInterlaced *)pArrayPicture->pPicture[0], pOutput, NULL);
	} else {
		VeneziaDebugSnapshotList outputList;
		VeneziaDebugSnapshotOutput *pOutput;

		outputList.count = 0;
		if (pArrayPicture != NULL) {
			outputList.count = pArrayPicture->numOfElm;
			pArrayPicture->numOfOutput = 0;
		}
		pOutput = &avc->output;
		outputList.snapshots = &pOutput;
		ret = SceCodecEngineWrapperForDriver_BB02EB22(handle, &au, &outputList,
			&numOfOutput);
		ret = videocodecMapCodecEngineError(ret);
		if ((ret < 0 && ret != SCE_AVCDEC_ERROR_ES_BUFFER_FULL) ||
		    outputList.count == 0 || numOfOutput == 0)
			goto unlockRanges;
		if (outputList.count < numOfOutput) {
			ret = SCE_AVCDEC_ERROR_INVALID_STATE;
			goto unlockRanges;
		}
		err = videocodecAvcdecPicture(
			(SceAvcdecPictureForInterlaced *)pArrayPicture->pPicture[0], pOutput, NULL,
			(global->avcdecFlags & 0x80) != 0);
	}
	pArrayPicture->numOfOutput = numOfOutput;
	if (ret >= 0 && err < 0)
		ret = err;
unlockRanges:
	videocodecUnlockRanges(&ranges);
	if (dsp0 != NULL)
		sceKernelUnlockFastMutex(&dsp0->mutex);
	else
		sceKernelUnlockFastMutex(&avc->mutex);
copyOut:
	if (pArrayPicture != NULL && pArrayPicture->numOfElm == 1 && userPicture != NULL) {
		copied = videocodecCopyOutPicturesInline(1, (void *const *)userPictures,
			(void *const *)kernelPicture, sizeof(SceAvcdecPicture));
		pArrayPicture->pPicture = userPicture;
	}
	if (ret < 0)
		copied = ret;
out:
	sceSyscallLeave(syscall);
	return copied;

unlockAll:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto copyOut;
}

/*
 * SceAvcodecForDriver_48DECA56 with up to two work pictures in pWorkPicture, copied in
 * and out like the decoded picture and handed to the picture conversion.
 */
int SceAvcodecForDriver_EAEB16D5(const SceAvcdecCtrl *pCtrl, const SceAvcdecAu *pAu,
	SceAvcdecArrayPicture *pArrayPicture, SceAvcdecArrayPicture *pWorkPicture)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global;
	VideocodecAvcDecoder *avc = NULL;
	VideocodecDsp0Decoder *dsp0 = NULL;
	SceAvcdecPicture **userPicture = NULL, **kernelPicture = NULL;
	SceAvcdecPicture **userWork = NULL, **kernelWork = NULL;
	SceAvcdecPicture *userPictures[1], *pictures[1];
	SceAvcdecPicture *userWorks[2], *works[2];
	VideocodecLockedRange lockedRange[1];
	VideocodecLockedRanges ranges;
	SceAvcdecAu au;
	uintptr_t physicalAddress;
	unsigned int handle, index, numOfOutput;
	char pictureBuf[sizeof(SceAvcdecPicture)];
	char workBuf[2 * sizeof(SceAvcdecPicture)];
	int ret, copied = 0, err;

	if (pArrayPicture != NULL && pArrayPicture->numOfElm == 1) {
		userPicture = pArrayPicture->pPicture;
		if (userPicture != NULL) {
			copied = sceKernelCopyFromUser(userPictures, userPicture,
				sizeof(userPictures));
			if (copied < 0 || (copied = videocodecCopyInPicturesInline(
					pArrayPicture->numOfElm, (void **)pictures, pictureBuf,
					(void *const *)userPictures, sizeof(SceAvcdecPicture))) < 0) {
				pArrayPicture->pPicture = userPicture;
				goto out;
			}
			pArrayPicture->pPicture = pictures;
			kernelPicture = pictures;
		}
	}
	if (pWorkPicture != NULL && pWorkPicture->numOfElm - 1 <= 1) {
		userWork = pWorkPicture->pPicture;
		if (userWork != NULL) {
			copied = sceKernelCopyFromUser(userWorks, userWork,
				pWorkPicture->numOfElm * sizeof(userWorks[0]));
			if (copied < 0 || (copied = videocodecCopyInPicturesInline(
					pWorkPicture->numOfElm, (void **)works, workBuf,
					(void *const *)userWorks, sizeof(SceAvcdecPicture))) < 0) {
				ret = copied;
				pWorkPicture->pPicture = userWork;
				goto copyOutPicture;
			}
			pWorkPicture->pPicture = works;
			kernelWork = works;
		}
	}
	global = _GetVideocodecGlobal(0);
	if (pCtrl == NULL || (pAu == NULL && pArrayPicture == NULL)) {
		ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
		goto copyOut;
	}
	if (pArrayPicture != NULL) {
		if (pArrayPicture->numOfElm == 1) {
			if (pArrayPicture->pPicture[0] == NULL) {
				ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
				goto copyOut;
			}
		} else if (pArrayPicture->numOfElm > 1) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			goto copyOut;
		}
	}
	handle = pCtrl->handle;
	if (handle == 0) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		goto copyOut;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto copyOut;
	if (handle >= 256) {
		index = handle - 256;
		if (index >= global->dsp0DecoderInfo.numOfStreams) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			goto unlockAll;
		}
		dsp0 = &global->dsp0Decoders[index];
		ret = lockFastMutex(&dsp0->mutex);
		if (ret < 0)
			goto unlockAll;
		handle = dsp0->handle;
		if (handle == 0) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&dsp0->mutex);
			goto unlockAll;
		}
	} else {
		if (handle > global->avcdecInfo.numOfStreams) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			goto unlockAll;
		}
		avc = &global->avcDecoders[handle - 1];
		ret = lockFastMutex(&avc->mutex);
		if (ret < 0)
			goto unlockAll;
		if (handle != avc->handle) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&avc->mutex);
			goto unlockAll;
		}
	}
	sceKernelUnlockFastMutex(global->pAllMutex);
	videocodecInitLockedRanges(&ranges, 0, lockedRange, 1, VIDEOCODEC_DECODER_ERROR_BASE);
	if (pAu != NULL && pAu->es.pBuf != NULL) {
		ret = videocodecLockRange(&ranges, &physicalAddress, (uintptr_t)pAu->es.pBuf,
			pAu->es.size, 16, 0, VIDEOCODEC_LOCK_CACHED | VIDEOCODEC_LOCK_SCATTERED);
		if (ret < 0)
			goto unlockRanges;
	}
	if (pAu != NULL && pAu->es.pBuf != NULL) {
		__builtin_memcpy(&au, pAu, sizeof(au));
	} else {
		au.pts.upper = SCE_VIDEODEC_VOID_TIMESTAMP;
		au.pts.lower = SCE_VIDEODEC_VOID_TIMESTAMP;
		au.dts.upper = SCE_VIDEODEC_VOID_TIMESTAMP;
		au.dts.lower = SCE_VIDEODEC_VOID_TIMESTAMP;
		au.es.pBuf = NULL;
		au.es.size = 0;
	}
	if (dsp0 != NULL) {
		Dsp0VideocodecArrayPicture outputArray;
		Dsp0VideocodecPicture **pOutput;

		outputArray.numOfElm = 0;
		if (pArrayPicture != NULL) {
			outputArray.numOfElm = pArrayPicture->numOfElm;
			pArrayPicture->numOfOutput = 0;
		}
		pOutput = &dsp0->pOutput;
		outputArray.pPicture = &pOutput;
		err = dsp0VideocodecAvcDecode(handle, &au, &outputArray, &numOfOutput);
		ret = videocodecMapDecoderError(err);
		if ((ret < 0 && ret != SCE_AVCDEC_ERROR_ES_BUFFER_FULL) ||
		    outputArray.numOfElm == 0 || numOfOutput == 0)
			goto unlockRanges;
		if (outputArray.numOfElm < numOfOutput) {
			ret = SCE_AVCDEC_ERROR_INVALID_STATE;
			goto unlockRanges;
		}
		err = videocodecDsp0AvcdecPicture(
			(SceAvcdecPictureForInterlaced *)pArrayPicture->pPicture[0], pOutput, pWorkPicture);
	} else {
		VeneziaDebugSnapshotList outputList;
		VeneziaDebugSnapshotOutput *pOutput;

		outputList.count = 0;
		if (pArrayPicture != NULL) {
			outputList.count = pArrayPicture->numOfElm;
			pArrayPicture->numOfOutput = 0;
		}
		pOutput = &avc->output;
		outputList.snapshots = &pOutput;
		ret = SceCodecEngineWrapperForDriver_BB02EB22(handle, &au, &outputList,
			&numOfOutput);
		ret = videocodecMapCodecEngineError(ret);
		if ((ret < 0 && ret != SCE_AVCDEC_ERROR_ES_BUFFER_FULL) ||
		    outputList.count == 0 || numOfOutput == 0)
			goto unlockRanges;
		if (outputList.count < numOfOutput) {
			ret = SCE_AVCDEC_ERROR_INVALID_STATE;
			goto unlockRanges;
		}
		err = videocodecAvcdecPicture(
			(SceAvcdecPictureForInterlaced *)pArrayPicture->pPicture[0], pOutput, pWorkPicture,
			(global->avcdecFlags & 0x80) != 0);
	}
	pArrayPicture->numOfOutput = numOfOutput;
	if (ret >= 0 && err < 0)
		ret = err;
unlockRanges:
	videocodecUnlockRanges(&ranges);
	if (dsp0 != NULL)
		sceKernelUnlockFastMutex(&dsp0->mutex);
	else
		sceKernelUnlockFastMutex(&avc->mutex);
copyOut:
	if (pWorkPicture != NULL && pWorkPicture->numOfElm - 1 <= 1 && userWork != NULL) {
		copied = videocodecCopyOutPicturesInline(pWorkPicture->numOfElm,
			(void *const *)userWorks, (void *const *)kernelWork,
			sizeof(SceAvcdecPicture));
		pWorkPicture->pPicture = userWork;
		if (ret >= 0 && copied < 0)
			ret = copied;
	}
copyOutPicture:
	if (pArrayPicture != NULL && pArrayPicture->numOfElm == 1 && userPicture != NULL) {
		copied = videocodecCopyOutPicturesInline(1, (void *const *)userPictures,
			(void *const *)kernelPicture, sizeof(SceAvcdecPicture));
		pArrayPicture->pPicture = userPicture;
	}
	if (ret < 0)
		copied = ret;
out:
	sceSyscallLeave(syscall);
	return copied;

unlockAll:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto copyOut;
}

/*
 * Submits the access unit pAu to the codec-engine AVC decoder pCtrl names, with its
 * stream locked for the call. The DSP0's decoders do not take one.
 */
static inline __attribute__((always_inline)) int
videocodecDecodeAuInline(SceAvcdecCtrl *pCtrl, const SceAvcdecAu *pAu, const SceUInt *timeout)
{
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	SceAvcdecAu au;
	VideocodecLockedRanges ranges;
	VideocodecLockedRange lockedRange[1];
	uintptr_t physicalAddress;
	unsigned int handle, index;
	int ret;

	if (pAu == NULL || pCtrl == NULL)
		return SCE_AVCDEC_ERROR_INVALID_POINTER;
	handle = pCtrl->handle;
	if (handle == 0)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		return ret;
	if (handle < 256) {
		VideocodecAvcDecoder *avc;

		if (handle > global->avcdecInfo.numOfStreams)
			goto invalidParam;
		avc = &global->avcDecoders[handle - 1];
		ret = lockFastMutex(&avc->mutex);
		if (ret < 0)
			goto unlock;
		if (handle != avc->handle) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&avc->mutex);
			goto unlock;
		}
		sceKernelUnlockFastMutex(global->pAllMutex);
		videocodecInitLockedRanges(&ranges, 0, lockedRange, 1, VIDEOCODEC_DECODER_ERROR_BASE);
		if (pAu->es.pBuf == NULL) {
			ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
		} else {
			ret = videocodecLockRange(&ranges, &physicalAddress, (uintptr_t)pAu->es.pBuf,
				pAu->es.size, 16, 0,
				VIDEOCODEC_LOCK_CACHED | VIDEOCODEC_LOCK_SCATTERED);
			if (ret >= 0) {
				au = *pAu;
				ret = SceCodecEngineWrapperForDriver_B420ABAC(handle, &au, timeout);
				ret = videocodecMapCodecEngineError(ret);
			}
		}
		videocodecUnlockRanges(&ranges);
		sceKernelUnlockFastMutex(&avc->mutex);
		return ret;
	} else {
		VideocodecDsp0Decoder *dsp0;

		index = handle - 256;
		if (index >= global->dsp0DecoderInfo.numOfStreams)
			goto invalidParam;
		dsp0 = &global->dsp0Decoders[index];
		ret = lockFastMutex(&dsp0->mutex);
		if (ret < 0)
			goto unlock;
		if (dsp0->handle == 0) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&dsp0->mutex);
			goto unlock;
		}
		sceKernelUnlockFastMutex(global->pAllMutex);
		videocodecInitLockedRanges(&ranges, 0, lockedRange, 1, VIDEOCODEC_DECODER_ERROR_BASE);
		videocodecUnlockRanges(&ranges);
		sceKernelUnlockFastMutex(&dsp0->mutex);
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	}

invalidParam:
	ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
unlock:
	sceKernelUnlockFastMutex(global->pAllMutex);
	return ret;
}

int SceAvcodecForDriver_78BA8E98(SceAvcdecCtrl *pCtrl, const SceAvcdecAu *pAu,
		const SceUInt *timeout)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	if (timeout != NULL && *timeout < 2000)
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
	else
		ret = videocodecDecodeAuInline(pCtrl, pAu, timeout);
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_D2ACCE22(SceAvcdecCtrl *pCtrl, const SceAvcdecAu *pAu,
		const SceUInt *timeout)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	ret = videocodecDecodeAuInline(pCtrl, pAu, timeout);
	sceSyscallLeave(syscall);
	return ret;
}

/*
 * Registers the callback cbid for the events in eventMask with the codec-engine AVC decoder
 * pCtrl names. The DSP0's decoders take none.
 */
static inline __attribute__((always_inline)) int
videocodecRegisterCallbackInline(SceAvcdecCtrl *pCtrl, SceUID cbid, unsigned int eventMask)
{
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	unsigned int handle, index;
	int ret;

	if (pCtrl == NULL)
		return SCE_AVCDEC_ERROR_INVALID_POINTER;
	handle = pCtrl->handle;
	if (handle == 0)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		return ret;
	if (handle < 256) {
		VideocodecAvcDecoder *avc;

		if (handle > global->avcdecInfo.numOfStreams)
			goto invalidParam;
		avc = &global->avcDecoders[handle - 1];
		ret = lockFastMutex(&avc->mutex);
		if (ret < 0)
			goto unlock;
		if (handle != avc->handle) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&avc->mutex);
			goto unlock;
		}
		sceKernelUnlockFastMutex(global->pAllMutex);
		ret = SceCodecEngineWrapperForDriver_16506603(handle, cbid, eventMask);
		ret = videocodecMapCodecEngineError(ret);
		sceKernelUnlockFastMutex(&avc->mutex);
		return ret;
	} else {
		VideocodecDsp0Decoder *dsp0;

		index = handle - 256;
		if (index >= global->dsp0DecoderInfo.numOfStreams)
			goto invalidParam;
		dsp0 = &global->dsp0Decoders[index];
		ret = lockFastMutex(&dsp0->mutex);
		if (ret < 0)
			goto unlock;
		if (dsp0->handle == 0) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&dsp0->mutex);
			goto unlock;
		}
		sceKernelUnlockFastMutex(global->pAllMutex);
		sceKernelUnlockFastMutex(&dsp0->mutex);
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	}

invalidParam:
	ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
unlock:
	sceKernelUnlockFastMutex(global->pAllMutex);
	return ret;
}

/*
 * Unregisters the callback cbid (0: every one) from the codec-engine AVC decoder pCtrl names.
 * The DSP0's decoders take none.
 */
static inline __attribute__((always_inline)) int
videocodecUnregisterCallbackInline(SceAvcdecCtrl *pCtrl, SceUID cbid)
{
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	unsigned int handle, index;
	int ret;

	if (pCtrl == NULL)
		return SCE_AVCDEC_ERROR_INVALID_POINTER;
	handle = pCtrl->handle;
	if (handle == 0)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		return ret;
	if (handle < 256) {
		VideocodecAvcDecoder *avc;

		if (handle > global->avcdecInfo.numOfStreams)
			goto invalidParam;
		avc = &global->avcDecoders[handle - 1];
		ret = lockFastMutex(&avc->mutex);
		if (ret < 0)
			goto unlock;
		if (handle != avc->handle) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&avc->mutex);
			goto unlock;
		}
		sceKernelUnlockFastMutex(global->pAllMutex);
		ret = SceCodecEngineWrapperForDriver_8FE370E6(handle, cbid);
		ret = videocodecMapCodecEngineError(ret);
		sceKernelUnlockFastMutex(&avc->mutex);
		return ret;
	} else {
		VideocodecDsp0Decoder *dsp0;

		index = handle - 256;
		if (index >= global->dsp0DecoderInfo.numOfStreams)
			goto invalidParam;
		dsp0 = &global->dsp0Decoders[index];
		ret = lockFastMutex(&dsp0->mutex);
		if (ret < 0)
			goto unlock;
		if (dsp0->handle == 0) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&dsp0->mutex);
			goto unlock;
		}
		sceKernelUnlockFastMutex(global->pAllMutex);
		sceKernelUnlockFastMutex(&dsp0->mutex);
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	}

invalidParam:
	ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
unlock:
	sceKernelUnlockFastMutex(global->pAllMutex);
	return ret;
}

int SceAvcodecForDriver_E7E14DE4(SceAvcdecCtrl *pCtrl, SceUID cbid, unsigned int eventMask)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	ret = videocodecRegisterCallbackInline(pCtrl, cbid, eventMask);
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_13C530E4(SceAvcdecCtrl *pCtrl)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	ret = videocodecUnregisterCallbackInline(pCtrl, 0);
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_E4F668C5(SceAvcdecCtrl *pCtrl, SceUID cbid)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	ret = videocodecUnregisterCallbackInline(pCtrl, cbid);
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_90F394E6(SceAvcdecCtrl *pCtrl, SceUID cbid, unsigned int eventMask)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	ret = videocodecRegisterCallbackInline(pCtrl, cbid, eventMask);
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_27211864(SceAvcdecCtrl *pCtrl)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	ret = videocodecUnregisterCallbackInline(pCtrl, 0);
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_D9FCDF3E(SceAvcdecCtrl *pCtrl, SceUID cbid)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	ret = videocodecUnregisterCallbackInline(pCtrl, cbid);
	sceSyscallLeave(syscall);
	return ret;
}

/*
 * Takes the next decoded picture of the codec-engine AVC decoder pCtrl names into the one
 * picture pArrayPicture names, waiting up to *timeout. The picture and the one or two work
 * pictures pWorkPicture may name are copied in from the caller and back out; checkTimeout
 * rejects a timeout below 2000 first.
 */
static inline __attribute__((always_inline)) int
videocodecGetPictureInline(SceAvcdecCtrl *pCtrl, SceAvcdecArrayPicture *pArrayPicture,
	SceAvcdecArrayPicture *pWorkPicture, const SceUInt *timeout, int checkTimeout)
{
	VideocodecGlobal *global;
	VideocodecAvcDecoder *avc;
	SceAvcdecPicture **userPicture = NULL, **kernelPicture = NULL;
	SceAvcdecPicture **userWork = NULL, **kernelWork = NULL;
	SceAvcdecPicture *userPictures[1], *pictures[1];
	SceAvcdecPicture *userWorks[2], *works[2];
	VeneziaDebugSnapshotOutput *pOutput;
	VeneziaDebugSnapshotList outputList;
	unsigned int handle, index, numOfOutput;
	char pictureBuf[sizeof(SceAvcdecPicture)];
	char workBuf[2 * sizeof(SceAvcdecPicture)];
	int ret, copied = 0, err;

	if (pArrayPicture != NULL && pArrayPicture->numOfElm == 1) {
		userPicture = pArrayPicture->pPicture;
		if (userPicture != NULL) {
			copied = sceKernelCopyFromUser(userPictures, userPicture,
				sizeof(userPictures));
			if (copied < 0 || (copied = videocodecCopyInPicturesInline(
					pArrayPicture->numOfElm, (void **)pictures, pictureBuf,
					(void *const *)userPictures, sizeof(SceAvcdecPicture))) < 0) {
				pArrayPicture->pPicture = userPicture;
				return copied;
			}
			pArrayPicture->pPicture = pictures;
			kernelPicture = pictures;
		}
	}
	if (pWorkPicture != NULL && pWorkPicture->numOfElm - 1 <= 1) {
		userWork = pWorkPicture->pPicture;
		if (userWork != NULL) {
			copied = sceKernelCopyFromUser(userWorks, userWork,
				pWorkPicture->numOfElm * sizeof(userWorks[0]));
			if (copied < 0 || (copied = videocodecCopyInPicturesInline(
					pWorkPicture->numOfElm, (void **)works, workBuf,
					(void *const *)userWorks, sizeof(SceAvcdecPicture))) < 0) {
				ret = copied;
				pWorkPicture->pPicture = userWork;
				goto copyOutPicture;
			}
			pWorkPicture->pPicture = works;
			kernelWork = works;
		}
	}
	if (checkTimeout && timeout != NULL && *timeout < 2000) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		goto copyOut;
	}
	global = _GetVideocodecGlobal(0);
	if (pCtrl == NULL || pArrayPicture == NULL ||
	    (pArrayPicture->numOfElm == 1 && pArrayPicture->pPicture[0] == NULL)) {
		ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
		goto copyOut;
	}
	handle = pCtrl->handle;
	if (handle == 0) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		goto copyOut;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto copyOut;
	if (handle >= 256) {
		VideocodecDsp0Decoder *dsp0;

		index = handle - 256;
		if (index >= global->dsp0DecoderInfo.numOfStreams) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			goto unlockAll;
		}
		dsp0 = &global->dsp0Decoders[index];
		ret = lockFastMutex(&dsp0->mutex);
		if (ret < 0)
			goto unlockAll;
		if (dsp0->handle == 0) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&dsp0->mutex);
			goto unlockAll;
		}
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		sceKernelUnlockFastMutex(global->pAllMutex);
		sceKernelUnlockFastMutex(&dsp0->mutex);
		goto copyOut;
	}
	if (handle > global->avcdecInfo.numOfStreams) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		goto unlockAll;
	}
	avc = &global->avcDecoders[handle - 1];
	ret = lockFastMutex(&avc->mutex);
	if (ret < 0)
		goto unlockAll;
	if (handle != avc->handle) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		sceKernelUnlockFastMutex(&avc->mutex);
		goto unlockAll;
	}
	sceKernelUnlockFastMutex(global->pAllMutex);
	pArrayPicture->numOfOutput = 0;
	outputList.count = pArrayPicture->numOfElm;
	pOutput = &avc->output;
	outputList.snapshots = &pOutput;
	ret = SceCodecEngineWrapperForDriver_30F6AFD4(handle, &outputList, &numOfOutput, timeout);
	ret = videocodecMapCodecEngineError(ret);
	if (((ret == SCE_AVCDEC_ERROR_ES_BUFFER_FULL || ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT) ||
	     ret >= 0) && outputList.count != 0 && numOfOutput != 0) {
		if (outputList.count < numOfOutput) {
			ret = SCE_AVCDEC_ERROR_INVALID_STATE;
		} else {
			err = videocodecAvcdecPicture(
				(SceAvcdecPictureForInterlaced *)pArrayPicture->pPicture[0], pOutput,
				pWorkPicture, (global->avcdecFlags & 0x80) != 0);
			pArrayPicture->numOfOutput = numOfOutput;
			if (ret >= 0 && err < 0)
				ret = err;
		}
	}
	sceKernelUnlockFastMutex(&avc->mutex);
copyOut:
	if (pWorkPicture != NULL && pWorkPicture->numOfElm - 1 <= 1 && userWork != NULL) {
		copied = videocodecCopyOutPicturesInline(pWorkPicture->numOfElm,
			(void *const *)userWorks, (void *const *)kernelWork,
			sizeof(SceAvcdecPicture));
		pWorkPicture->pPicture = userWork;
		if (ret >= 0 && copied < 0)
			ret = copied;
	}
copyOutPicture:
	if (pArrayPicture != NULL && pArrayPicture->numOfElm == 1 && userPicture != NULL) {
		copied = videocodecCopyOutPicturesInline(1, (void *const *)userPictures,
			(void *const *)kernelPicture, sizeof(SceAvcdecPicture));
		pArrayPicture->pPicture = userPicture;
	}
	if (ret < 0)
		copied = ret;
	return copied;

unlockAll:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto copyOut;
}

int SceAvcodecForDriver_F8BCBE20(SceAvcdecCtrl *pCtrl, SceAvcdecArrayPicture *pArrayPicture,
	const SceUInt *timeout)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	ret = videocodecGetPictureInline(pCtrl, pArrayPicture, NULL, timeout, 1);
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_D7CD7F82(SceAvcdecCtrl *pCtrl, SceAvcdecArrayPicture *pArrayPicture,
	const SceUInt *timeout)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	ret = videocodecGetPictureInline(pCtrl, pArrayPicture, NULL, timeout, 0);
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_6E250A5F(SceAvcdecCtrl *pCtrl, SceAvcdecArrayPicture *pArrayPicture,
	SceAvcdecArrayPicture *pWorkPicture, const SceUInt *timeout)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	ret = videocodecGetPictureInline(pCtrl, pArrayPicture, pWorkPicture, timeout, 0);
	sceSyscallLeave(syscall);
	return ret;
}

/*
 * Drains the last picture of the AVC decoder pCtrl names, on the codec engine or the DSP0,
 * into pArrayPicture's one picture.
 */
int SceAvcodecForDriver_DFC0AAC6(const SceAvcdecCtrl *pCtrl, SceAvcdecArrayPicture *pArrayPicture)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global;
	SceAvcdecPicture **userPicture = NULL, **kernelPicture = NULL;
	SceAvcdecPicture *userPictures[1], *pictures[1];
	unsigned int handle, index, numOfOutput;
	char pictureBuf[sizeof(SceAvcdecPicture)];
	int ret, copied = 0, err;

	if (pArrayPicture == NULL) {
		_GetVideocodecGlobal(0);
		copied = SCE_AVCDEC_ERROR_INVALID_POINTER;
		goto out;
	}
	if (pArrayPicture->numOfElm == 1) {
		userPicture = pArrayPicture->pPicture;
		if (userPicture != NULL) {
			copied = sceKernelCopyFromUser(userPictures, userPicture,
				sizeof(userPictures));
			if (copied < 0 || (copied = videocodecCopyInPicturesInline(
					pArrayPicture->numOfElm, (void **)pictures, pictureBuf,
					(void *const *)userPictures, sizeof(SceAvcdecPicture))) < 0) {
				pArrayPicture->pPicture = userPicture;
				goto out;
			}
			pArrayPicture->pPicture = pictures;
			kernelPicture = pictures;
		}
	}
	global = _GetVideocodecGlobal(0);
	if (pCtrl == NULL) {
		ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
		goto copyOut;
	}
	if (pArrayPicture->numOfElm == 1) {
		if (pArrayPicture->pPicture[0] == NULL) {
			ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
			goto copyOut;
		}
	} else if (pArrayPicture->numOfElm > 1) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		goto copyOut;
	}
	handle = pCtrl->handle;
	if (handle == 0) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		goto copyOut;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto copyOut;
	if (handle >= 256) {
		VideocodecDsp0Decoder *dsp0;
		Dsp0VideocodecArrayPicture outputArray;
		Dsp0VideocodecPicture **pOutput;

		index = handle - 256;
		if (index >= global->dsp0DecoderInfo.numOfStreams) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			goto unlockAll;
		}
		dsp0 = &global->dsp0Decoders[index];
		ret = lockFastMutex(&dsp0->mutex);
		if (ret < 0)
			goto unlockAll;
		handle = dsp0->handle;
		if (handle == 0) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&dsp0->mutex);
			goto unlockAll;
		}
		sceKernelUnlockFastMutex(global->pAllMutex);
		pArrayPicture->numOfOutput = 0;
		pOutput = &dsp0->pOutput;
		outputArray.numOfElm = 1;
		outputArray.pPicture = &pOutput;
		err = dsp0VideocodecAvcDecodeStop(handle, &outputArray, &numOfOutput);
		ret = videocodecMapDecoderError(err);
		if (ret >= 0 && numOfOutput != 0 && numOfOutput <= pArrayPicture->numOfElm) {
			if (numOfOutput > outputArray.numOfElm) {
				ret = SCE_AVCDEC_ERROR_INVALID_STATE;
			} else {
				err = videocodecDsp0AvcdecPicture(
					(SceAvcdecPictureForInterlaced *)pArrayPicture->pPicture[0],
					pOutput, NULL);
				if (err < 0)
					ret = err;
				pArrayPicture->numOfOutput = numOfOutput;
			}
		}
		sceKernelUnlockFastMutex(&dsp0->mutex);
	} else {
		VideocodecAvcDecoder *avc;
		VeneziaDebugSnapshotList outputList;
		VeneziaDebugSnapshotOutput *pOutput;

		if (handle > global->avcdecInfo.numOfStreams) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			goto unlockAll;
		}
		avc = &global->avcDecoders[handle - 1];
		ret = lockFastMutex(&avc->mutex);
		if (ret < 0)
			goto unlockAll;
		if (handle != avc->handle) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&avc->mutex);
			goto unlockAll;
		}
		sceKernelUnlockFastMutex(global->pAllMutex);
		pArrayPicture->numOfOutput = 0;
		outputList.count = pArrayPicture->numOfElm;
		pOutput = &avc->output;
		outputList.snapshots = &pOutput;
		ret = SceCodecEngineWrapperForDriver_B48B02A3(handle, &outputList, &numOfOutput);
		ret = videocodecMapCodecEngineError(ret);
		if (ret >= 0 && numOfOutput != 0 && numOfOutput <= pArrayPicture->numOfElm) {
			if (numOfOutput > outputList.count) {
				ret = SCE_AVCDEC_ERROR_INVALID_STATE;
			} else {
				err = videocodecAvcdecPicture(
					(SceAvcdecPictureForInterlaced *)pArrayPicture->pPicture[0],
					pOutput, NULL, (global->avcdecFlags & 0x80) != 0);
				if (err < 0)
					ret = err;
				pArrayPicture->numOfOutput = numOfOutput;
			}
		}
		sceKernelUnlockFastMutex(&avc->mutex);
	}
copyOut:
	if (pArrayPicture->numOfElm == 1 && userPicture != NULL) {
		copied = videocodecCopyOutPicturesInline(1, (void *const *)userPictures,
			(void *const *)kernelPicture, sizeof(SceAvcdecPicture));
		pArrayPicture->pPicture = userPicture;
	}
	if (ret < 0)
		copied = ret;
out:
	sceSyscallLeave(syscall);
	return copied;

unlockAll:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto copyOut;
}

/*
 * SceAvcodecForDriver_DFC0AAC6 with up to two work pictures in pWorkPicture, copied in and
 * out like the drained picture and handed to the picture conversion.
 */
int SceAvcodecForDriver_E355CAAE(const SceAvcdecCtrl *pCtrl, SceAvcdecArrayPicture *pArrayPicture,
	SceAvcdecArrayPicture *pWorkPicture)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global;
	SceAvcdecPicture **userPicture = NULL, **kernelPicture = NULL;
	SceAvcdecPicture **userWork = NULL, **kernelWork = NULL;
	SceAvcdecPicture *userPictures[1], *pictures[1];
	SceAvcdecPicture *userWorks[2], *works[2];
	unsigned int handle, index, numOfOutput;
	char pictureBuf[sizeof(SceAvcdecPicture)];
	char workBuf[2 * sizeof(SceAvcdecPicture)];
	int ret, copied = 0, err;

	if (pArrayPicture != NULL && pArrayPicture->numOfElm == 1) {
		userPicture = pArrayPicture->pPicture;
		if (userPicture != NULL) {
			copied = sceKernelCopyFromUser(userPictures, userPicture,
				sizeof(userPictures));
			if (copied < 0 || (copied = videocodecCopyInPicturesInline(
					pArrayPicture->numOfElm, (void **)pictures, pictureBuf,
					(void *const *)userPictures, sizeof(SceAvcdecPicture))) < 0) {
				pArrayPicture->pPicture = userPicture;
				goto out;
			}
			pArrayPicture->pPicture = pictures;
			kernelPicture = pictures;
		}
	}
	if (pWorkPicture != NULL && pWorkPicture->numOfElm - 1 <= 1) {
		userWork = pWorkPicture->pPicture;
		if (userWork != NULL) {
			copied = sceKernelCopyFromUser(userWorks, userWork,
				pWorkPicture->numOfElm * sizeof(userWorks[0]));
			if (copied < 0 || (copied = videocodecCopyInPicturesInline(
					pWorkPicture->numOfElm, (void **)works, workBuf,
					(void *const *)userWorks, sizeof(SceAvcdecPicture))) < 0) {
				ret = copied;
				pWorkPicture->pPicture = userWork;
				goto copyOutPicture;
			}
			pWorkPicture->pPicture = works;
			kernelWork = works;
		}
	}
	global = _GetVideocodecGlobal(0);
	if (pCtrl == NULL || pArrayPicture == NULL) {
		ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
		goto copyOut;
	}
	if (pArrayPicture->numOfElm == 1) {
		if (pArrayPicture->pPicture[0] == NULL) {
			ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
			goto copyOut;
		}
	} else if (pArrayPicture->numOfElm > 1) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		goto copyOut;
	}
	handle = pCtrl->handle;
	if (handle == 0) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		goto copyOut;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto copyOut;
	if (handle >= 256) {
		VideocodecDsp0Decoder *dsp0;
		Dsp0VideocodecArrayPicture outputArray;
		Dsp0VideocodecPicture **pOutput;

		index = handle - 256;
		if (index >= global->dsp0DecoderInfo.numOfStreams) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			goto unlockAll;
		}
		dsp0 = &global->dsp0Decoders[index];
		ret = lockFastMutex(&dsp0->mutex);
		if (ret < 0)
			goto unlockAll;
		handle = dsp0->handle;
		if (handle == 0) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&dsp0->mutex);
			goto unlockAll;
		}
		sceKernelUnlockFastMutex(global->pAllMutex);
		pArrayPicture->numOfOutput = 0;
		pOutput = &dsp0->pOutput;
		outputArray.numOfElm = 1;
		outputArray.pPicture = &pOutput;
		err = dsp0VideocodecAvcDecodeStop(handle, &outputArray, &numOfOutput);
		ret = videocodecMapDecoderError(err);
		if (ret >= 0 && numOfOutput != 0 && numOfOutput <= pArrayPicture->numOfElm) {
			if (numOfOutput > outputArray.numOfElm) {
				ret = SCE_AVCDEC_ERROR_INVALID_STATE;
			} else {
				err = videocodecDsp0AvcdecPicture(
					(SceAvcdecPictureForInterlaced *)pArrayPicture->pPicture[0],
					pOutput, pWorkPicture);
				if (err < 0)
					ret = err;
				pArrayPicture->numOfOutput = numOfOutput;
			}
		}
		sceKernelUnlockFastMutex(&dsp0->mutex);
	} else {
		VideocodecAvcDecoder *avc;
		VeneziaDebugSnapshotList outputList;
		VeneziaDebugSnapshotOutput *pOutput;

		if (handle > global->avcdecInfo.numOfStreams) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			goto unlockAll;
		}
		avc = &global->avcDecoders[handle - 1];
		ret = lockFastMutex(&avc->mutex);
		if (ret < 0)
			goto unlockAll;
		if (handle != avc->handle) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&avc->mutex);
			goto unlockAll;
		}
		sceKernelUnlockFastMutex(global->pAllMutex);
		pArrayPicture->numOfOutput = 0;
		outputList.count = pArrayPicture->numOfElm;
		pOutput = &avc->output;
		outputList.snapshots = &pOutput;
		ret = SceCodecEngineWrapperForDriver_B48B02A3(handle, &outputList, &numOfOutput);
		ret = videocodecMapCodecEngineError(ret);
		if (ret >= 0 && numOfOutput != 0 && numOfOutput <= pArrayPicture->numOfElm) {
			if (numOfOutput > outputList.count) {
				ret = SCE_AVCDEC_ERROR_INVALID_STATE;
			} else {
				err = videocodecAvcdecPicture(
					(SceAvcdecPictureForInterlaced *)pArrayPicture->pPicture[0],
					pOutput, pWorkPicture, (global->avcdecFlags & 0x80) != 0);
				if (err < 0)
					ret = err;
				pArrayPicture->numOfOutput = numOfOutput;
			}
		}
		sceKernelUnlockFastMutex(&avc->mutex);
	}
copyOut:
	if (pWorkPicture != NULL && pWorkPicture->numOfElm - 1 <= 1 && userWork != NULL) {
		copied = videocodecCopyOutPicturesInline(pWorkPicture->numOfElm,
			(void *const *)userWorks, (void *const *)kernelWork,
			sizeof(SceAvcdecPicture));
		pWorkPicture->pPicture = userWork;
		if (ret >= 0 && copied < 0)
			ret = copied;
	}
copyOutPicture:
	if (pArrayPicture != NULL && pArrayPicture->numOfElm == 1 && userPicture != NULL) {
		copied = videocodecCopyOutPicturesInline(1, (void *const *)userPictures,
			(void *const *)kernelPicture, sizeof(SceAvcdecPicture));
		pArrayPicture->pPicture = userPicture;
	}
	if (ret < 0)
		copied = ret;
out:
	sceSyscallLeave(syscall);
	return copied;

unlockAll:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto copyOut;
}

/* Drops what the AVC decoder pCtrl names still holds, on the codec engine or the DSP0. */
int SceAvcodecForDriver_D46B8463(const SceAvcdecCtrl *pCtrl)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	unsigned int handle, index;
	int ret;

	if (pCtrl == NULL) {
		ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
		goto out;
	}
	handle = pCtrl->handle;
	if (handle == 0) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		goto out;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto out;
	if (handle < 256) {
		VideocodecAvcDecoder *avc;

		if (handle > global->avcdecInfo.numOfStreams)
			goto invalidParam;
		avc = &global->avcDecoders[handle - 1];
		ret = lockFastMutex(&avc->mutex);
		if (ret < 0)
			goto unlock;
		if (handle != avc->handle) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&avc->mutex);
			goto unlock;
		}
		sceKernelUnlockFastMutex(global->pAllMutex);
		ret = SceCodecEngineWrapperForDriver_9EB3D63E(handle);
		ret = videocodecMapCodecEngineError(ret);
		sceKernelUnlockFastMutex(&avc->mutex);
	} else {
		VideocodecDsp0Decoder *dsp0;

		index = handle - 256;
		if (index >= global->dsp0DecoderInfo.numOfStreams)
			goto invalidParam;
		dsp0 = &global->dsp0Decoders[index];
		ret = lockFastMutex(&dsp0->mutex);
		if (ret < 0)
			goto unlock;
		handle = dsp0->handle;
		if (handle == 0) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&dsp0->mutex);
			goto unlock;
		}
		sceKernelUnlockFastMutex(global->pAllMutex);
		ret = dsp0VideocodecAvcDecodeFlush(handle);
		ret = videocodecMapDecoderError(ret);
		sceKernelUnlockFastMutex(&dsp0->mutex);
	}
out:
	sceSyscallLeave(syscall);
	return ret;

invalidParam:
	ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
unlock:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto out;
}

/*
 * Reads the SEI picture timing of field 0 or 1 of the last picture the codec-engine AVC
 * decoder pCtrl names returned. The DSP0's decoders report theirs with the picture.
 */
int SceAvcodecForDriver_BC244D21(SceAvcdecCtrl *pCtrl, unsigned int field,
	VideocodecSeiPictureTiming *pPictureTiming)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	VideocodecAvcDecoder *avc;
	const VeneziaDebugChannelState *timing;
	VideocodecSeiClockTimestamp *clock;
	unsigned int handle, index, i;
	int ret;

	if (pPictureTiming == NULL || pCtrl == NULL) {
		ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
		goto out;
	}
	handle = pCtrl->handle;
	if (handle == 0) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		goto out;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto out;
	if (handle >= 256) {
		VideocodecDsp0Decoder *dsp0;

		index = handle - 256;
		if (index >= global->dsp0DecoderInfo.numOfStreams)
			goto invalidParam;
		dsp0 = &global->dsp0Decoders[index];
		ret = lockFastMutex(&dsp0->mutex);
		if (ret < 0)
			goto unlock;
		if (dsp0->handle == 0) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&dsp0->mutex);
			goto unlock;
		}
		sceKernelUnlockFastMutex(global->pAllMutex);
		sceKernelUnlockFastMutex(&dsp0->mutex);
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		goto out;
	}
	if (handle > global->avcdecInfo.numOfStreams)
		goto invalidParam;
	avc = &global->avcDecoders[handle - 1];
	ret = lockFastMutex(&avc->mutex);
	if (ret < 0)
		goto unlock;
	if (handle != avc->handle) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		sceKernelUnlockFastMutex(&avc->mutex);
		goto unlock;
	}
	sceKernelUnlockFastMutex(global->pAllMutex);
	if (field > 1) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
	} else if (!(avc->output.valid & (field != 0 ? 0x20 : 0x4))) {
		ret = VIDEOCODEC_AVCDEC_ERROR_NO_SEI;
	} else {
		ret = 0;
		memset(pPictureTiming, 0, sizeof(*pPictureTiming));
		pPictureTiming->valid = 1;
		timing = &avc->output.data.channels[field];
		pPictureTiming->cpbRemovalDelay = timing->header[0];
		pPictureTiming->dpbOutputDelay = timing->header[1];
		pPictureTiming->picStruct = timing->status[0];
		for (i = 0; i < 3; i++) {
			pPictureTiming->clockTimestampFlag[i] = timing->status[1 + i];
			clock = &pPictureTiming->clockTimestamp[i];
			clock->clockMetadata[0] = timing->control[0][i];
			clock->clockMetadata[1] = timing->control[1][i];
			clock->clockMetadata[2] = timing->control[2][i];
			clock->clockMetadata[3] = timing->control[3][i];
			clock->clockMetadata[4] = timing->control[4][i];
			clock->clockMetadata[5] = timing->control[5][i];
			clock->clockMetadata[6] = timing->control[6][i];
			clock->clockMetadata[7] = timing->control[8][i];
			clock->clockMetadata[8] = timing->control[10][i];
			clock->clockMetadata[9] = timing->control[12][i];
			clock->clockMetadata[10] = timing->control[7][i];
			clock->clockMetadata[11] = timing->control[9][i];
			clock->clockMetadata[12] = timing->control[11][i];
			clock->timeOffset = timing->parameters[i];
		}
	}
	sceKernelUnlockFastMutex(&avc->mutex);
out:
	sceSyscallLeave(syscall);
	return ret;

invalidParam:
	ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
unlock:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto out;
}

/*
 * Copies the SEI user data payloads of field 0 or 1 of the last picture the codec-engine
 * AVC decoder pCtrl names returned to the user buffer pBuf, each 8-byte aligned payload
 * packed after the one before.
 */
int SceAvcodecForDriver_427914D5(SceAvcdecCtrl *pCtrl, unsigned int field,
	VideocodecSeiUserData *pUserData, SceAvcdecBuf *pBuf)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	VideocodecAvcDecoder *avc;
	VideocodecLockedRanges ranges;
	VideocodecLockedRange lockedRange[1];
	uintptr_t physicalAddress;
	const VideocodecSeiUserDataBlock *block;
	uintptr_t start, data, dest;
	SceSize size, length, copied, offset;
	unsigned int handle, index, count, i, truncated;
	int ret;

	if (pUserData == NULL || pCtrl == NULL || pBuf == NULL) {
		ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
		goto out;
	}
	if (pUserData->size != sizeof(*pUserData)) {
		ret = SCE_AVCDEC_ERROR_INVALID_ARGUMENT_SIZE;
		goto out;
	}
	handle = pCtrl->handle;
	if (handle == 0) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		goto out;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto out;
	if (handle >= 256) {
		VideocodecDsp0Decoder *dsp0;

		index = handle - 256;
		if (index >= global->dsp0DecoderInfo.numOfStreams)
			goto invalidParam;
		dsp0 = &global->dsp0Decoders[index];
		ret = lockFastMutex(&dsp0->mutex);
		if (ret < 0)
			goto unlock;
		if (dsp0->handle == 0) {
			ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
			sceKernelUnlockFastMutex(&dsp0->mutex);
			goto unlock;
		}
		sceKernelUnlockFastMutex(global->pAllMutex);
		sceKernelUnlockFastMutex(&dsp0->mutex);
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		goto out;
	}
	if (handle > global->avcdecInfo.numOfStreams)
		goto invalidParam;
	avc = &global->avcDecoders[handle - 1];
	ret = lockFastMutex(&avc->mutex);
	if (ret < 0)
		goto unlock;
	if (handle != avc->handle) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		sceKernelUnlockFastMutex(&avc->mutex);
		goto unlock;
	}
	sceKernelUnlockFastMutex(global->pAllMutex);
	if (field > 1) {
		ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
		goto unlockDecoder;
	}
	if (field != 0) {
		if (!(avc->output.valid & 0x2000))
			goto noSei;
		index = 1;
	} else {
		if (!(avc->output.valid & 0x1000))
			goto noSei;
		index = 0;
	}
	memset(&pUserData->numOfUserData, 0,
	       sizeof(*pUserData) - sizeof(pUserData->size));
	size = avc->seiUserDataSize;
	block = avc->pSeiUserData[index];
	videocodecInitLockedRanges(&ranges, 0, lockedRange, 1, VIDEOCODEC_DECODER_ERROR_BASE);
	ret = videocodecLockRange(&ranges, &physicalAddress, (uintptr_t)pBuf->pBuf,
		pBuf->size, 32, 0, VIDEOCODEC_LOCK_CACHED | VIDEOCODEC_LOCK_SCATTERED);
	if (ret < 0)
		goto unlockRanges;
	start = (uintptr_t)(block + 1);
	data = (uintptr_t)block + block->dataOffset;
	count = VIDEOCODEC_SEI_USER_DATA_COUNT(block->info);
	dest = (uintptr_t)pBuf->pBuf;
	if (data < start || start > data + size || data > start + size ||
	    data + size > start + size) {
		ret = SCE_AVCDEC_ERROR_INVALID_STATE;
		goto unlockRanges;
	}
	copied = 0;
	offset = 0;
	truncated = 0;
	if (count != 0) {
		for (i = 0;;) {
			length = block->userData[i] & 0x3FFFFFFF;
			if (copied + length > pBuf->size) {
				length = pBuf->size - copied;
				truncated = 1;
			} else {
				truncated = 0;
			}
			if (length + offset > size) {
				length = size - offset;
				truncated = 1;
			}
			ret = sceKernelCopyToUser((void *)(dest + copied),
				(const void *)(data + offset), length);
			if (ret < 0)
				break;
			pUserData->userData[i].payloadType = block->userData[i] >> 30;
			pUserData->userData[i].payloadSize = length;
			copied += length;
			offset += (length + 7) & ~7;
			i++;
			if (i == count || truncated)
				break;
		}
		pUserData->numOfUserData = i;
		pUserData->userDataSize = copied;
		if (truncated) {
			ret = VIDEOCODEC_AVCDEC_ERROR_SEI_TRUNCATED;
			goto unlockRanges;
		}
	} else {
		pUserData->numOfUserData = 0;
		pUserData->userDataSize = 0;
	}
	if (block->info & VIDEOCODEC_SEI_USER_DATA_INCOMPLETE)
		ret = VIDEOCODEC_AVCDEC_ERROR_SEI_INCOMPLETE;
	if (block->info & VIDEOCODEC_SEI_USER_DATA_TRUNCATED)
		ret = VIDEOCODEC_AVCDEC_ERROR_SEI_TRUNCATED;
unlockRanges:
	videocodecUnlockRanges(&ranges);
unlockDecoder:
	sceKernelUnlockFastMutex(&avc->mutex);
out:
	sceSyscallLeave(syscall);
	return ret;

noSei:
	ret = VIDEOCODEC_AVCDEC_ERROR_NO_SEI;
	goto unlockDecoder;

invalidParam:
	ret = SCE_AVCDEC_ERROR_INVALID_PARAM;
unlock:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto out;
}

/* Converts an AVC decoder's picture into pDst's pixel format. */
int SceAvcodecForDriver_16B501C9(SceAvcdecPicture *pDst, const SceAvcdecPicture *pSrc)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	int ret;

	if (pDst == NULL || pSrc == NULL) {
		ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
	} else {
		ret = lockFastMutex(global->pAllMutex);
		if (ret >= 0) {
			ret = videocodecAvcdecCsc(pDst, pSrc);
			sceKernelUnlockFastMutex(global->pAllMutex);
		}
	}
	sceSyscallLeave(syscall);
	return ret;
}

/* As SceAvcodecForDriver_16B501C9, for pictures whose frames may have separate planes. */
int SceAvcodecForDriver_B8A9632A(VideocodecPicture *pDst, const VideocodecPicture *pSrc)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	int ret;

	if (pDst == NULL || pSrc == NULL) {
		ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
	} else {
		ret = lockFastMutex(global->pAllMutex);
		if (ret >= 0) {
			ret = videocodecAvcdecCscInternal(pDst, pSrc);
			sceKernelUnlockFastMutex(global->pAllMutex);
		}
	}
	sceSyscallLeave(syscall);
	return ret;
}

/* videocodecCopyInPicturesInline, out of line for the MPEG-4 decoder */
static __attribute__((noinline, noclone)) int videocodecCopyInPictures(unsigned int count,
	void **pKernel, char *buf, void *const *pUser, SceSize size)
{
	return videocodecCopyInPicturesInline(count, pKernel, buf, pUser, size);
}

/* videocodecCopyOutPicturesInline, out of line for the MPEG-4 decoder */
static __attribute__((noinline, noclone)) int videocodecCopyOutPictures(unsigned int count,
	void *const *pUser, void *const *pKernel, SceSize size)
{
	return videocodecCopyOutPicturesInline(count, pUser, pKernel, size);
}

/*
 * Creates an MPEG-4 decoder for pInfo in a free DSP0 slot, in pCtrl's frame buffer: the
 * process's unmap memory, memory locked for the decoder, or, when pCtrl has none, a block of
 * its own.
 */
int m4vdecCreateDecoder(SceAvcdecCtrl *pCtrl, const Dsp0VideocodecM4vDecoderInfo *pInfo)
{
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	SceKernelAllocMemBlockKernelOpt opt;
	Dsp0VideocodecDecoderParam param;
	Dsp0VideocodecM4vDecoderInfo info;
	VideocodecDsp0Decoder *decoder;
	uintptr_t physicalAddress;
	SceSize frameMemSize;
	unsigned int i, handle;
	void *base;
	int ret;

	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		return ret;
	if (global->dsp0Library == 0) {
		sceKernelUnlockFastMutex(global->pAllMutex);
		return VIDEOCODEC_M4VDEC_ERROR_NOT_INITIALIZE;
	}
	for (i = 0; i < global->dsp0DecoderInfo.numOfStreams; i++) {
		decoder = &global->dsp0Decoders[i];
		ret = lockFastMutex(&decoder->mutex);
		if (ret < 0) {
			sceKernelUnlockFastMutex(global->pAllMutex);
			return ret;
		}
		if (decoder->handle == 0)
			break;
		sceKernelUnlockFastMutex(&decoder->mutex);
	}
	if (i >= global->dsp0DecoderInfo.numOfStreams) {
		sceKernelUnlockFastMutex(global->pAllMutex);
		return VIDEOCODEC_M4VDEC_ERROR_ALREADY_USED;
	}
	videocodecInitLockedRanges(&decoder->lockedRanges, 0, decoder->lockedRange, 1,
		VIDEOCODEC_M4VDEC_ERROR_BASE);

	if (global->dsp0DecoderInfo.horizontal < pInfo->horizontal ||
	    global->dsp0DecoderInfo.vertical < pInfo->vertical) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		goto unlockRanges;
	}
	info.horizontal = pInfo->horizontal;
	info.vertical = pInfo->vertical;
	info.maxEsSize = pInfo->maxEsSize;
	decoder->frameBufFlags = 0;
	if (pCtrl->frameBuf.pBuf != NULL) {
		if (((uintptr_t)pCtrl->frameBuf.pBuf & 0xFF) != 0) {
			ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER;
			goto unlockRanges;
		}
		ret = sceCodecEngineRegisterUnmapMemory(0, (SceUIntVAddr)pCtrl->frameBuf.pBuf,
			pCtrl->frameBuf.size);
		if (ret == SCE_CODECENGINE_ERROR_INVALID_POINTER) {
			if (videocodecLockRange(&decoder->lockedRanges, &physicalAddress,
					(uintptr_t)pCtrl->frameBuf.pBuf, pCtrl->frameBuf.size, 32,
					VIDEOCODEC_CACHE_INVALIDATE, VIDEOCODEC_LOCK_CACHED) < 0) {
				ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER;
				goto unlockRanges;
			}
			decoder->frameBuf = pCtrl->frameBuf;
		} else {
			if (ret < 0) {
				ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER;
				goto unlockRanges;
			}
			ret = sceCodecEngineUnmapUserVAtoKernelVA(0, pCtrl->frameBuf.pBuf, &base);
			if (ret < 0) {
				sceCodecEngineUnregisterUnmapMemory(0, (SceUIntVAddr)pCtrl->frameBuf.pBuf,
					pCtrl->frameBuf.size);
				goto unlockRanges;
			}
			decoder->frameBufFlags |= VIDEOCODEC_FRAME_BUF_UNMAP;
			decoder->frameBuf.pBuf = base;
			decoder->frameBuf.size = pCtrl->frameBuf.size;
		}
	} else {
		ret = VIDEOCODEC_M4VDEC_ERROR_NOT_GAME_PROCESS;
		if (!sceKernelIsGameBudget())
			goto unlockRanges;
		frameMemSize = 0;
		ret = m4vdecQueryDecoderMemSizeInline(pInfo, &frameMemSize);
		if (ret < 0)
			goto unlockRanges;
		memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
		opt.alignment = 0x100000;
		ret = sceKernelAllocMemBlockForKernel("SceM4vdecSwFrameBuf",
			SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_CDRAM_RW, frameMemSize, &opt);
		if (ret < 0) {
			ret = VIDEOCODEC_M4VDEC_ERROR_OUT_OF_MEMORY;
			goto unlockRanges;
		}
		decoder->frameBufMemBlockId = ret;
		decoder->frameBufFlags |= VIDEOCODEC_FRAME_BUF_MEMBLOCK;
		sceKernelGetMemBlockBase(ret, &decoder->frameBuf.pBuf);
		decoder->frameBuf.size = frameMemSize;
	}
	param.pFrameBuf = decoder->frameBuf.pBuf;
	param.frameBufSize = decoder->frameBuf.size;
	param.pInfo = &info;
	ret = dsp0VideocodecCreateM4vDecoder(global->dsp0Library, &handle, &param);
	ret = videocodecMapM4vdecError(ret);
	if (ret >= 0) {
		decoder->mode = 0;
		decoder->info.horizontal = pInfo->horizontal;
		decoder->info.vertical = pInfo->vertical;
		decoder->info.numOfRefFrames = pInfo->maxEsSize;
		decoder->handle = handle;
		pCtrl->handle = handle - 1;
		goto unlock;
	}
	if (decoder->frameBufFlags & VIDEOCODEC_FRAME_BUF_MEMBLOCK) {
		sceKernelFreeMemBlock(decoder->frameBufMemBlockId);
		decoder->frameBufFlags &= ~VIDEOCODEC_FRAME_BUF_MEMBLOCK;
	}
	if (decoder->frameBufFlags & VIDEOCODEC_FRAME_BUF_UNMAP) {
		sceCodecEngineUnmapKernelVAtoUserVA(0, decoder->frameBuf.pBuf, &base);
		sceCodecEngineUnregisterUnmapMemory(0, (SceUIntVAddr)base, decoder->frameBuf.size);
		decoder->frameBufFlags &= ~VIDEOCODEC_FRAME_BUF_UNMAP;
	}
	__builtin_memset(&decoder->frameBuf, 0, sizeof(decoder->frameBuf));
unlockRanges:
	videocodecUnlockRanges(&decoder->lockedRanges);
unlock:
	sceKernelUnlockFastMutex(&decoder->mutex);
	sceKernelUnlockFastMutex(global->pAllMutex);
	return ret;
}

int SceAvcodecForDriver_C5C10C8B(unsigned int codecType,
		const Dsp0VideocodecM4vDecoderInfo *pInfo, SceSize *pFrameMemSize)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	if (codecType != SCE_VIDEODEC_TYPE_HW_AVCDEC + 1) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
	} else if (pFrameMemSize == NULL || pInfo == NULL) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER;
	} else if (pInfo->horizontal > 1280 || pInfo->vertical > 720 ||
		   pInfo->maxEsSize > 0x17C000) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
	} else {
		ret = m4vdecQueryDecoderMemSizeInline(pInfo, pFrameMemSize);
	}
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_C99242F5(unsigned int codecType,
		const Dsp0VideocodecM4vDecoderInfo *pInfo, SceSize *pFrameMemSize)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	if (codecType != SCE_VIDEODEC_TYPE_HW_AVCDEC + 1)
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
	else if (pFrameMemSize == NULL || pInfo == NULL)
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER;
	else
		ret = m4vdecQueryDecoderMemSizeInline(pInfo, pFrameMemSize);
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_47D8E150(unsigned int codecType, SceAvcdecCtrl *pCtrl,
		const Dsp0VideocodecM4vDecoderInfo *pInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	if (codecType != SCE_VIDEODEC_TYPE_HW_AVCDEC + 1) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
	} else if (pInfo == NULL || pCtrl == NULL) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER;
	} else if (pInfo->horizontal > 1280 || pInfo->vertical > 720 ||
		   pInfo->maxEsSize > 0x17C000) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
	} else {
		ret = m4vdecCreateDecoder(pCtrl, pInfo);
	}
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_C363B31A(unsigned int codecType, SceAvcdecCtrl *pCtrl,
		const Dsp0VideocodecM4vDecoderInfo *pInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	if (codecType != SCE_VIDEODEC_TYPE_HW_AVCDEC + 1)
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
	else if (pInfo == NULL || pCtrl == NULL)
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER;
	else
		ret = m4vdecCreateDecoder(pCtrl, pInfo);
	sceSyscallLeave(syscall);
	return ret;
}

/*
 * Deletes pid's MPEG-4 decoder pCtrl->handle names (from 512, on the DSP0) and frees its frame
 * buffer, unless the delete was stopped by a thread suspend.
 */
static inline __attribute__((always_inline)) int
m4vdecDeleteDecoderInline(SceAvcdecCtrl *pCtrl, SceUID pid)
{
	VideocodecGlobal *global = _GetVideocodecGlobal(pid);
	VideocodecDsp0Decoder *decoder;
	unsigned int handle, index;
	void *base;
	int ret, err;

	if (pCtrl == NULL)
		return VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER;
	handle = pCtrl->handle;
	if (handle == 0)
		return VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		return ret;
	if (handle < 512 || (index = handle - 512) >= global->dsp0DecoderInfo.numOfStreams) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		goto unlock;
	}
	decoder = &global->dsp0Decoders[index];
	ret = lockFastMutex(&decoder->mutex);
	if (ret < 0)
		goto unlock;
	handle = decoder->handle;
	if (handle == 0) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		sceKernelUnlockFastMutex(&decoder->mutex);
		goto unlock;
	}
	sceKernelUnlockFastMutex(global->pAllMutex);
	err = dsp0VideocodecDeleteM4vDecoder(handle, pid);
	err = videocodecMapM4vdecError(err);
	if (err == SCE_KERNEL_ERROR_THREAD_STOPPED || err == SCE_KERNEL_ERROR_THREAD_SUSPENDED) {
		sceKernelUnlockFastMutex(&decoder->mutex);
		return err;
	}
	decoder->handle = 0;
	if (decoder->frameBufFlags & VIDEOCODEC_FRAME_BUF_MEMBLOCK) {
		sceKernelFreeMemBlock(decoder->frameBufMemBlockId);
		decoder->frameBufFlags &= ~VIDEOCODEC_FRAME_BUF_MEMBLOCK;
	}
	if (decoder->frameBufFlags & VIDEOCODEC_FRAME_BUF_UNMAP) {
		sceCodecEngineUnmapKernelVAtoUserVA(pid, decoder->frameBuf.pBuf, &base);
		ret = sceCodecEngineUnregisterUnmapMemory(pid, (SceUIntVAddr)base,
			decoder->frameBuf.size);
		decoder->frameBufFlags &= ~VIDEOCODEC_FRAME_BUF_UNMAP;
	}
	__builtin_memset(&decoder->frameBuf, 0, sizeof(decoder->frameBuf));
	videocodecUnlockRangesProc(&decoder->lockedRanges, pid);
	sceKernelUnlockFastMutex(&decoder->mutex);
	if (ret != 0)
		return ret;
	return err < 0 ? err : 0;

unlock:
	sceKernelUnlockFastMutex(global->pAllMutex);
	return ret;
}

int m4vdecDeleteDecoderForProcess(SceAvcdecCtrl *pCtrl, SceUID pid)
{
	return m4vdecDeleteDecoderInline(pCtrl, pid);
}

int SceAvcodecForDriver_77529FD2(SceAvcdecCtrl *pCtrl)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	ret = m4vdecDeleteDecoderInline(pCtrl, 0);
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_91E94B76(SceAvcdecCtrl *pCtrl)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	VideocodecDsp0Decoder *decoder;
	unsigned int handle, index;
	int ret;

	if (pCtrl == NULL) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER;
		goto out;
	}
	handle = pCtrl->handle;
	if (handle == 0) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		goto out;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto out;
	if (handle < 512 || (index = handle - 512) >= global->dsp0DecoderInfo.numOfStreams) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		goto unlock;
	}
	decoder = &global->dsp0Decoders[index];
	ret = lockFastMutex(&decoder->mutex);
	if (ret < 0)
		goto unlock;
	handle = decoder->handle;
	if (handle == 0) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		sceKernelUnlockFastMutex(&decoder->mutex);
		goto unlock;
	}
	sceKernelUnlockFastMutex(global->pAllMutex);
	ret = videocodecMapM4vdecError(dsp0VideocodecM4vDecodeAvailableSize(handle));
	if (ret > VIDEOCODEC_M4VDEC_MAX_AVAILABLE_SIZE)
		ret = VIDEOCODEC_M4VDEC_MAX_AVAILABLE_SIZE;
	sceKernelUnlockFastMutex(&decoder->mutex);
out:
	sceSyscallLeave(syscall);
	return ret;

unlock:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto out;
}

/*
 * Decodes pAu on the MPEG-4 decoder pCtrl names. The one picture pArrayPicture may name is
 * copied in from the caller, filled from the DSP0's output, and copied back out.
 */
int SceAvcodecForDriver_2C7A9841(const SceAvcdecCtrl *pCtrl, const M4vdecAu *pAu,
	M4vdecArrayPicture *pArrayPicture)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global;
	VideocodecDsp0Decoder *decoder;
	M4vdecPicture **userPicture = NULL, **kernelPicture = NULL;
	M4vdecPicture *userPictures[1], *pictures[1];
	VideocodecLockedRange lockedRange[1];
	VideocodecLockedRanges ranges;
	Dsp0VideocodecArrayPicture outputArray;
	Dsp0VideocodecPicture **pOutput;
	SceAvcdecAu au;
	uintptr_t physicalAddress;
	unsigned int handle, index, numOfOutput;
	char pictureBuf[sizeof(M4vdecPicture)];
	int ret, copied = 0, err;

	if (pArrayPicture != NULL && pArrayPicture->numOfElm == 1) {
		userPicture = pArrayPicture->pPicture;
		if (userPicture != NULL) {
			copied = sceKernelCopyFromUser(userPictures, userPicture,
				sizeof(userPictures));
			if (copied < 0 || (copied = videocodecCopyInPictures(pArrayPicture->numOfElm,
					(void **)pictures, pictureBuf, (void *const *)userPictures,
					sizeof(M4vdecPicture))) < 0) {
				pArrayPicture->pPicture = userPicture;
				goto out;
			}
			pArrayPicture->pPicture = pictures;
			kernelPicture = pictures;
		}
	}
	global = _GetVideocodecGlobal(0);
	if (pCtrl == NULL) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER;
		goto copyOut;
	}
	if (pAu == NULL && pArrayPicture == NULL) {
		copied = VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER;
		goto out;
	}
	if (pArrayPicture != NULL) {
		if (pArrayPicture->numOfElm == 1) {
			if (pArrayPicture->pPicture[0] == NULL) {
				ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER;
				goto copyOut;
			}
		} else if (pArrayPicture->numOfElm > 1) {
			ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
			goto copyOut;
		}
	}
	handle = pCtrl->handle;
	if (handle == 0) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		goto copyOut;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto copyOut;
	if (handle < 512 || (index = handle - 512) >= global->dsp0DecoderInfo.numOfStreams) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		goto unlockAll;
	}
	decoder = &global->dsp0Decoders[index];
	ret = lockFastMutex(&decoder->mutex);
	if (ret < 0)
		goto unlockAll;
	handle = decoder->handle;
	if (handle == 0) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		sceKernelUnlockFastMutex(&decoder->mutex);
		goto unlockAll;
	}
	sceKernelUnlockFastMutex(global->pAllMutex);
	videocodecInitLockedRanges(&ranges, 0, lockedRange, 1, VIDEOCODEC_M4VDEC_ERROR_BASE);
	if (pAu != NULL && pAu->es.pBuf != NULL) {
		ret = videocodecLockRange(&ranges, &physicalAddress, (uintptr_t)pAu->es.pBuf,
			pAu->es.size, 16, 0, VIDEOCODEC_LOCK_CACHED | VIDEOCODEC_LOCK_SCATTERED);
		if (ret < 0)
			goto unlockRanges;
	}
	if (pAu != NULL && pAu->es.pBuf != NULL) {
		__builtin_memcpy(&au, pAu, sizeof(au));
	} else {
		au.pts.upper = SCE_VIDEODEC_VOID_TIMESTAMP;
		au.pts.lower = SCE_VIDEODEC_VOID_TIMESTAMP;
		au.dts.upper = SCE_VIDEODEC_VOID_TIMESTAMP;
		au.dts.lower = SCE_VIDEODEC_VOID_TIMESTAMP;
		au.es.pBuf = NULL;
		au.es.size = 0;
	}
	outputArray.numOfElm = 0;
	if (pArrayPicture != NULL) {
		outputArray.numOfElm = pArrayPicture->numOfElm;
		pArrayPicture->numOfOutput = 0;
	}
	pOutput = &decoder->pOutput;
	outputArray.pPicture = &pOutput;
	err = dsp0VideocodecM4vDecode(handle, &au, &outputArray, &numOfOutput);
	ret = videocodecMapM4vdecError(err);
	if ((ret < 0 && ret != VIDEOCODEC_M4VDEC_ERROR_ES_BUFFER_FULL) ||
	    outputArray.numOfElm == 0 || numOfOutput == 0)
		goto unlockRanges;
	if (outputArray.numOfElm < numOfOutput) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_STATE;
		goto unlockRanges;
	}
	err = videocodecDsp0M4vdecPicture(pArrayPicture->pPicture[0], pOutput, NULL);
	pArrayPicture->numOfOutput = numOfOutput;
	if (ret >= 0 && err < 0)
		ret = err;
unlockRanges:
	videocodecUnlockRanges(&ranges);
	sceKernelUnlockFastMutex(&decoder->mutex);
copyOut:
	if (pArrayPicture != NULL && pArrayPicture->numOfElm == 1 && userPicture != NULL) {
		copied = videocodecCopyOutPictures(1, (void *const *)userPictures,
			(void *const *)kernelPicture, sizeof(M4vdecPicture));
		pArrayPicture->pPicture = userPicture;
	}
	if (ret < 0)
		copied = ret;
out:
	sceSyscallLeave(syscall);
	return copied;

unlockAll:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto copyOut;
}

/*
 * SceAvcodecForDriver_2C7A9841 with work pictures: pWorkPicture's one or two pictures are
 * copied in as well, handed to the output conversion, and copied back out first.
 */
int SceAvcodecForDriver_823E187E(const SceAvcdecCtrl *pCtrl, const M4vdecAu *pAu,
	M4vdecArrayPicture *pArrayPicture, M4vdecArrayPicture *pWorkPicture)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global;
	VideocodecDsp0Decoder *decoder;
	M4vdecPicture **userPicture = NULL, **kernelPicture = NULL;
	M4vdecPicture **userWork = NULL, **kernelWork = NULL;
	M4vdecPicture *userPictures[1], *pictures[1];
	M4vdecPicture *userWorks[2], *works[2];
	VideocodecLockedRange lockedRange[1];
	VideocodecLockedRanges ranges;
	Dsp0VideocodecArrayPicture outputArray;
	Dsp0VideocodecPicture **pOutput;
	SceAvcdecAu au;
	uintptr_t physicalAddress;
	unsigned int handle, index, numOfOutput;
	char pictureBuf[sizeof(M4vdecPicture)];
	char workBuf[2 * sizeof(M4vdecPicture)];
	int ret, copied = 0, err;

	if (pArrayPicture != NULL && pArrayPicture->numOfElm == 1) {
		userPicture = pArrayPicture->pPicture;
		if (userPicture != NULL) {
			copied = sceKernelCopyFromUser(userPictures, userPicture,
				sizeof(userPictures));
			if (copied < 0 || (copied = videocodecCopyInPictures(pArrayPicture->numOfElm,
					(void **)pictures, pictureBuf, (void *const *)userPictures,
					sizeof(M4vdecPicture))) < 0) {
				pArrayPicture->pPicture = userPicture;
				goto out;
			}
			pArrayPicture->pPicture = pictures;
			kernelPicture = pictures;
		}
	}
	if (pWorkPicture != NULL && pWorkPicture->numOfElm - 1 <= 1) {
		userWork = pWorkPicture->pPicture;
		if (userWork != NULL) {
			copied = sceKernelCopyFromUser(userWorks, userWork,
				pWorkPicture->numOfElm * sizeof(userWorks[0]));
			if (copied < 0 || (copied = videocodecCopyInPictures(pWorkPicture->numOfElm,
					(void **)works, workBuf, (void *const *)userWorks,
					sizeof(M4vdecPicture))) < 0) {
				ret = copied;
				pWorkPicture->pPicture = userWork;
				goto copyOut;
			}
			pWorkPicture->pPicture = works;
			kernelWork = works;
		}
	}
	global = _GetVideocodecGlobal(0);
	if (pCtrl == NULL || (pAu == NULL && pArrayPicture == NULL)) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER;
		goto copyOutWork;
	}
	if (pArrayPicture != NULL) {
		if (pArrayPicture->numOfElm == 1) {
			if (pArrayPicture->pPicture[0] == NULL) {
				ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER;
				goto copyOutWork;
			}
		} else if (pArrayPicture->numOfElm > 1) {
			ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
			goto copyOutWork;
		}
	}
	handle = pCtrl->handle;
	if (handle == 0) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		goto copyOutWork;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto copyOutWork;
	if (handle < 512 || (index = handle - 512) >= global->dsp0DecoderInfo.numOfStreams) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		goto unlockAll;
	}
	decoder = &global->dsp0Decoders[index];
	ret = lockFastMutex(&decoder->mutex);
	if (ret < 0)
		goto unlockAll;
	handle = decoder->handle;
	if (handle == 0) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		sceKernelUnlockFastMutex(&decoder->mutex);
		goto unlockAll;
	}
	sceKernelUnlockFastMutex(global->pAllMutex);
	videocodecInitLockedRanges(&ranges, 0, lockedRange, 1, VIDEOCODEC_M4VDEC_ERROR_BASE);
	if (pAu != NULL && pAu->es.pBuf != NULL) {
		ret = videocodecLockRange(&ranges, &physicalAddress, (uintptr_t)pAu->es.pBuf,
			pAu->es.size, 16, 0, VIDEOCODEC_LOCK_CACHED | VIDEOCODEC_LOCK_SCATTERED);
		if (ret < 0)
			goto unlockRanges;
	}
	if (pAu != NULL && pAu->es.pBuf != NULL) {
		__builtin_memcpy(&au, pAu, sizeof(au));
	} else {
		au.pts.upper = SCE_VIDEODEC_VOID_TIMESTAMP;
		au.pts.lower = SCE_VIDEODEC_VOID_TIMESTAMP;
		au.dts.upper = SCE_VIDEODEC_VOID_TIMESTAMP;
		au.dts.lower = SCE_VIDEODEC_VOID_TIMESTAMP;
		au.es.pBuf = NULL;
		au.es.size = 0;
	}
	outputArray.numOfElm = 0;
	if (pArrayPicture != NULL) {
		outputArray.numOfElm = pArrayPicture->numOfElm;
		pArrayPicture->numOfOutput = 0;
	}
	pOutput = &decoder->pOutput;
	outputArray.pPicture = &pOutput;
	err = dsp0VideocodecM4vDecode(handle, &au, &outputArray, &numOfOutput);
	ret = videocodecMapM4vdecError(err);
	if ((ret < 0 && ret != VIDEOCODEC_M4VDEC_ERROR_ES_BUFFER_FULL) ||
	    outputArray.numOfElm == 0 || numOfOutput == 0)
		goto unlockRanges;
	if (outputArray.numOfElm < numOfOutput) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_STATE;
		goto unlockRanges;
	}
	err = videocodecDsp0M4vdecPicture(pArrayPicture->pPicture[0], pOutput, pWorkPicture);
	pArrayPicture->numOfOutput = numOfOutput;
	if (ret >= 0 && err < 0)
		ret = err;
unlockRanges:
	videocodecUnlockRanges(&ranges);
	sceKernelUnlockFastMutex(&decoder->mutex);
copyOutWork:
	if (pWorkPicture != NULL && pWorkPicture->numOfElm - 1 <= 1 && userWork != NULL) {
		copied = videocodecCopyOutPictures(pWorkPicture->numOfElm,
			(void *const *)userWorks, (void *const *)kernelWork, sizeof(M4vdecPicture));
		pWorkPicture->pPicture = userWork;
		if (copied < 0 && ret >= 0)
			ret = copied;
	}
copyOut:
	if (pArrayPicture != NULL && pArrayPicture->numOfElm == 1 && userPicture != NULL) {
		copied = videocodecCopyOutPictures(1, (void *const *)userPictures,
			(void *const *)kernelPicture, sizeof(M4vdecPicture));
		pArrayPicture->pPicture = userPicture;
	}
	if (ret < 0)
		copied = ret;
out:
	sceSyscallLeave(syscall);
	return copied;

unlockAll:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto copyOutWork;
}

/* Drains the last picture of the MPEG-4 decoder pCtrl names into pArrayPicture's one picture. */
int SceAvcodecForDriver_F67F93DC(const SceAvcdecCtrl *pCtrl, M4vdecArrayPicture *pArrayPicture)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global;
	VideocodecDsp0Decoder *decoder;
	M4vdecPicture **userPicture = NULL, **kernelPicture = NULL;
	M4vdecPicture *userPictures[1], *pictures[1];
	Dsp0VideocodecArrayPicture outputArray;
	Dsp0VideocodecPicture **pOutput;
	unsigned int handle, index, numOfOutput;
	char pictureBuf[sizeof(M4vdecPicture)];
	int ret, copied = 0, err;

	if (pArrayPicture == NULL) {
		_GetVideocodecGlobal(0);
		copied = VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER;
		goto out;
	}
	if (pArrayPicture->numOfElm == 1) {
		userPicture = pArrayPicture->pPicture;
		if (userPicture != NULL) {
			copied = sceKernelCopyFromUser(userPictures, userPicture,
				sizeof(userPictures));
			if (copied < 0 || (copied = videocodecCopyInPictures(pArrayPicture->numOfElm,
					(void **)pictures, pictureBuf, (void *const *)userPictures,
					sizeof(M4vdecPicture))) < 0) {
				pArrayPicture->pPicture = userPicture;
				goto out;
			}
			pArrayPicture->pPicture = pictures;
			kernelPicture = pictures;
		}
	}
	global = _GetVideocodecGlobal(0);
	if (pCtrl == NULL) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER;
		goto copyOut;
	}
	if (pArrayPicture->numOfElm == 1) {
		if (pArrayPicture->pPicture[0] == NULL) {
			ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER;
			goto copyOut;
		}
	} else if (pArrayPicture->numOfElm > 1) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		goto copyOut;
	}
	handle = pCtrl->handle;
	if (handle == 0) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		goto copyOut;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto copyOut;
	if (handle < 512 || (index = handle - 512) >= global->dsp0DecoderInfo.numOfStreams) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		goto unlockAll;
	}
	decoder = &global->dsp0Decoders[index];
	ret = lockFastMutex(&decoder->mutex);
	if (ret < 0)
		goto unlockAll;
	handle = decoder->handle;
	if (handle == 0) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		sceKernelUnlockFastMutex(&decoder->mutex);
		goto unlockAll;
	}
	sceKernelUnlockFastMutex(global->pAllMutex);
	pArrayPicture->numOfOutput = 0;
	pOutput = &decoder->pOutput;
	outputArray.numOfElm = 1;
	outputArray.pPicture = &pOutput;
	err = dsp0VideocodecM4vDecodeStop(handle, &outputArray, &numOfOutput);
	ret = videocodecMapM4vdecError(err);
	if (ret < 0 || numOfOutput == 0 || numOfOutput > pArrayPicture->numOfElm)
		goto unlock;
	if (numOfOutput > outputArray.numOfElm) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_STATE;
		goto unlock;
	}
	err = videocodecDsp0M4vdecPicture(pArrayPicture->pPicture[0], pOutput, NULL);
	if (err < 0)
		ret = err;
	pArrayPicture->numOfOutput = numOfOutput;
unlock:
	sceKernelUnlockFastMutex(&decoder->mutex);
copyOut:
	if (pArrayPicture->numOfElm == 1 && userPicture != NULL) {
		copied = videocodecCopyOutPictures(1, (void *const *)userPictures,
			(void *const *)kernelPicture, sizeof(M4vdecPicture));
		pArrayPicture->pPicture = userPicture;
	}
	if (ret < 0)
		copied = ret;
out:
	sceSyscallLeave(syscall);
	return copied;

unlockAll:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto copyOut;
}

/* SceAvcodecForDriver_F67F93DC with work pictures, copied as SceAvcodecForDriver_823E187E's. */
int SceAvcodecForDriver_3F146942(const SceAvcdecCtrl *pCtrl, M4vdecArrayPicture *pArrayPicture,
	M4vdecArrayPicture *pWorkPicture)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global;
	VideocodecDsp0Decoder *decoder;
	M4vdecPicture **userPicture = NULL, **kernelPicture = NULL;
	M4vdecPicture **userWork = NULL, **kernelWork = NULL;
	M4vdecPicture *userPictures[1], *pictures[1];
	M4vdecPicture *userWorks[2], *works[2];
	Dsp0VideocodecArrayPicture outputArray;
	Dsp0VideocodecPicture **pOutput;
	unsigned int handle, index, numOfOutput;
	char pictureBuf[sizeof(M4vdecPicture)];
	char workBuf[2 * sizeof(M4vdecPicture)];
	int ret, copied = 0, err;

	if (pArrayPicture != NULL && pArrayPicture->numOfElm == 1) {
		userPicture = pArrayPicture->pPicture;
		if (userPicture != NULL) {
			copied = sceKernelCopyFromUser(userPictures, userPicture,
				sizeof(userPictures));
			if (copied < 0 || (copied = videocodecCopyInPictures(pArrayPicture->numOfElm,
					(void **)pictures, pictureBuf, (void *const *)userPictures,
					sizeof(M4vdecPicture))) < 0) {
				pArrayPicture->pPicture = userPicture;
				goto out;
			}
			pArrayPicture->pPicture = pictures;
			kernelPicture = pictures;
		}
	}
	if (pWorkPicture != NULL && pWorkPicture->numOfElm - 1 <= 1) {
		userWork = pWorkPicture->pPicture;
		if (userWork != NULL) {
			copied = sceKernelCopyFromUser(userWorks, userWork,
				pWorkPicture->numOfElm * sizeof(userWorks[0]));
			if (copied < 0 || (copied = videocodecCopyInPictures(pWorkPicture->numOfElm,
					(void **)works, workBuf, (void *const *)userWorks,
					sizeof(M4vdecPicture))) < 0) {
				ret = copied;
				pWorkPicture->pPicture = userWork;
				goto copyOut;
			}
			pWorkPicture->pPicture = works;
			kernelWork = works;
		}
	}
	global = _GetVideocodecGlobal(0);
	if (pCtrl == NULL || pArrayPicture == NULL) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER;
		goto copyOutWork;
	}
	if (pArrayPicture->numOfElm == 1) {
		if (pArrayPicture->pPicture[0] == NULL) {
			ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER;
			goto copyOutWork;
		}
	} else if (pArrayPicture->numOfElm > 1) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		goto copyOutWork;
	}
	handle = pCtrl->handle;
	if (handle == 0) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		goto copyOutWork;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto copyOutWork;
	if (handle < 512 || (index = handle - 512) >= global->dsp0DecoderInfo.numOfStreams) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		goto unlockAll;
	}
	decoder = &global->dsp0Decoders[index];
	ret = lockFastMutex(&decoder->mutex);
	if (ret < 0)
		goto unlockAll;
	handle = decoder->handle;
	if (handle == 0) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		sceKernelUnlockFastMutex(&decoder->mutex);
		goto unlockAll;
	}
	sceKernelUnlockFastMutex(global->pAllMutex);
	pArrayPicture->numOfOutput = 0;
	pOutput = &decoder->pOutput;
	outputArray.numOfElm = 1;
	outputArray.pPicture = &pOutput;
	err = dsp0VideocodecM4vDecodeStop(handle, &outputArray, &numOfOutput);
	ret = videocodecMapM4vdecError(err);
	if (ret < 0 || numOfOutput == 0 || numOfOutput > pArrayPicture->numOfElm)
		goto unlock;
	if (numOfOutput > outputArray.numOfElm) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_STATE;
		goto unlock;
	}
	err = videocodecDsp0M4vdecPicture(pArrayPicture->pPicture[0], pOutput, pWorkPicture);
	if (err < 0)
		ret = err;
	pArrayPicture->numOfOutput = numOfOutput;
unlock:
	sceKernelUnlockFastMutex(&decoder->mutex);
copyOutWork:
	if (pWorkPicture != NULL && pWorkPicture->numOfElm - 1 <= 1 && userWork != NULL) {
		copied = videocodecCopyOutPictures(pWorkPicture->numOfElm,
			(void *const *)userWorks, (void *const *)kernelWork, sizeof(M4vdecPicture));
		pWorkPicture->pPicture = userWork;
		if (copied < 0 && ret >= 0)
			ret = copied;
	}
copyOut:
	if (pArrayPicture != NULL && pArrayPicture->numOfElm == 1 && userPicture != NULL) {
		copied = videocodecCopyOutPictures(1, (void *const *)userPictures,
			(void *const *)kernelPicture, sizeof(M4vdecPicture));
		pArrayPicture->pPicture = userPicture;
	}
	if (ret < 0)
		copied = ret;
out:
	sceSyscallLeave(syscall);
	return copied;

unlockAll:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto copyOutWork;
}

int SceAvcodecForDriver_FD805BF1(const SceAvcdecCtrl *pCtrl)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	VideocodecDsp0Decoder *decoder;
	unsigned int handle, index;
	int ret;

	if (pCtrl == NULL) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER;
		goto out;
	}
	handle = pCtrl->handle;
	if (handle == 0) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		goto out;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto out;
	if (handle < 512 || (index = handle - 512) >= global->dsp0DecoderInfo.numOfStreams) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		goto unlock;
	}
	decoder = &global->dsp0Decoders[index];
	ret = lockFastMutex(&decoder->mutex);
	if (ret < 0)
		goto unlock;
	handle = decoder->handle;
	if (handle == 0) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
		sceKernelUnlockFastMutex(&decoder->mutex);
		goto unlock;
	}
	sceKernelUnlockFastMutex(global->pAllMutex);
	ret = videocodecMapM4vdecError(dsp0VideocodecM4vDecodeFlush(handle));
	sceKernelUnlockFastMutex(&decoder->mutex);
out:
	ret = videocodecConvertM4vdecError(ret);
	sceSyscallLeave(syscall);
	return ret;

unlock:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto out;
}

int SceAvcodecForDriver_A6CAE217(M4vdecPicture *pPictureOut, const M4vdecPicture *pPictureIn)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	int ret;

	if (pPictureOut == NULL || pPictureIn == NULL) {
		ret = VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER;
	} else {
		ret = lockFastMutex(global->pAllMutex);
		if (ret >= 0) {
			ret = videocodecConvertM4vdecError(videocodecM4vdecCsc(pPictureOut, pPictureIn));
			sceKernelUnlockFastMutex(global->pAllMutex);
		}
	}
	sceSyscallLeave(syscall);
	return ret;
}

/* Checks an AVC encoder's init parameters, the preset filling their zero fields. */
static __attribute__((noinline)) int avcencCheckInitParam(const AvcencInitParam *pParam)
{
	AvcencInitParam param = *pParam;
	const AvcencInitParamInternal *preset;
	const AvcencPresetRate *rate;
	const AvcencLevelLimit *limit;
	VideocodecGlobal *global;
	unsigned int mode, presetIndex, config, mbs;
	int ret;

	if (param.preset > 4)
		return VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
	if (param.mode == 0)
		param.mode = 2;
	mode = param.mode & AVCENC_MODE_MASK;
	if (param.preset == 0)
		param.preset = 4;
	presetIndex = param.preset - 1;
	preset = &avcencPresets[presetIndex];
	if (param.pitch == 0)
		param.pitch = preset->pitch;
	if (param.alignedHorizontal == 0)
		param.alignedHorizontal = preset->alignedHorizontal;
	if (param.alignedVertical == 0)
		param.alignedVertical = preset->alignedVertical;
	if (param.horizontal == 0)
		param.horizontal = preset->horizontal;
	if (param.vertical == 0)
		param.vertical = preset->vertical;
	if (param.numOfStreams == 0)
		param.numOfStreams = preset->numOfStreams;
	if (param.bitRate[0] == 0)
		param.bitRate[0] = preset->parameter.bitRate[0];
	if (param.bitRate[1] == 0)
		param.bitRate[1] = preset->parameter.bitRate[1];
	if (param.numUnitsInTick == 0)
		param.numUnitsInTick = preset->parameter.numUnitsInTick;
	if (param.timeScale == 0)
		param.timeScale = preset->parameter.timeScale;
	if (param.sliceOption == 0)
		param.sliceOption = preset->sliceOption;
	if (param.sliceParam[0] == 0)
		param.sliceParam[0] = preset->sliceParam[0];
	if (param.sliceParam[1] == 0)
		param.sliceParam[1] = preset->sliceParam[1];

	if (param.pitch > 736 || param.alignedHorizontal > 720 || param.alignedVertical > 480 ||
	    param.pitch < param.alignedHorizontal ||
	    param.alignedHorizontal < param.horizontal ||
	    param.alignedVertical < param.vertical ||
	    param.horizontal < 64 || param.horizontal > 720 ||
	    param.vertical < 64 || param.vertical > 480 ||
	    param.numOfStreams > 1 ||
	    mode < 1 || mode > 9 || (mode >= 4 && mode <= 7) ||
	    (param.mode & AVCENC_MODE_RESERVED) != 0 ||
	    ((param.mode & AVCENC_MODE_VUI) && param.vuiIndex > 2) ||
	    (param.sliceOption != 255 &&
	     (param.sliceParam[0] != 0 || param.sliceParam[1] != 0)))
		return VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;

	global = _GetVideocodecGlobal(0);
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		return ret;
	config = global->availablePreset;
	ret = sceKernelUnlockFastMutex(global->pAllMutex);
	if (ret < 0)
		return ret;
	if (mode == 3 && config != 3)
		return VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;

	rate = &avcencPresetRates[presetIndex];
	limit = &avcencLevelLimits[rate->levelLimit];
	mbs = ((param.horizontal + 15) >> 4) * ((param.vertical + 15) >> 4);
	if (mbs * 1000 * rate->frameRate / rate->frameRateScale > limit->maxMbps ||
	    mbs > limit->maxFs || limit->maxBr < param.bitRate[0] ||
	    limit->maxBr < param.bitRate[1])
		return VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
	return ret;
}

/* Fills pOut from the preset pParam names, with pParam's own fields over it. */
static __attribute__((noinline)) int avcencInitParamToInternal(AvcencInitParamInternal *pOut,
	const AvcencInitParam *pParam)
{
	AvcencInitParam param = *pParam;
	const AvcencInitParamInternal *preset;
	unsigned int mode;

	if (param.preset > 4)
		return VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
	if (param.mode == 0)
		param.mode = 2;
	mode = param.mode & AVCENC_MODE_MASK;
	if (param.preset == 0)
		param.preset = 4;
	preset = &avcencPresets[param.preset - 1];
	if (param.pitch == 0)
		param.pitch = preset->pitch;
	if (param.alignedHorizontal == 0)
		param.alignedHorizontal = preset->alignedHorizontal;
	if (param.alignedVertical == 0)
		param.alignedVertical = preset->alignedVertical;
	if (param.horizontal == 0)
		param.horizontal = preset->horizontal;
	if (param.vertical == 0)
		param.vertical = preset->vertical;
	if (param.numOfStreams == 0)
		param.numOfStreams = preset->numOfStreams;
	if (param.bitRate[0] == 0)
		param.bitRate[0] = preset->parameter.bitRate[0];
	if (param.bitRate[1] == 0)
		param.bitRate[1] = preset->parameter.bitRate[1];
	if (param.numUnitsInTick == 0)
		param.numUnitsInTick = preset->parameter.numUnitsInTick;
	if (param.timeScale == 0)
		param.timeScale = preset->parameter.timeScale;
	if (param.sliceOption == 0)
		param.sliceOption = preset->sliceOption;
	if (param.sliceParam[0] == 0)
		param.sliceParam[0] = preset->sliceParam[0];
	if (param.sliceParam[1] == 0)
		param.sliceParam[1] = preset->sliceParam[1];

	memset(pOut, 0, sizeof(*pOut));
	memcpy(pOut, preset, sizeof(*pOut));
	pOut->pitch = param.pitch;
	pOut->alignedHorizontal = param.alignedHorizontal;
	pOut->alignedVertical = param.alignedVertical;
	pOut->codingOption[0] = avcencCodingOptions[mode - 1];
	pOut->horizontal = param.horizontal;
	pOut->vertical = param.vertical;
	pOut->numOfStreams = param.numOfStreams;
	if (mode == 3) {
		pOut->codingOption[1] = 1;
		pOut->codingOption[2] = 1;
		pOut->parameter.gopLength = param.timeScale / param.numUnitsInTick;
	} else if (mode == 8 || mode == 9) {
		pOut->userOption[1] = 0;
	}
	pOut->sliceOption = param.sliceOption;
	pOut->sliceParam[0] = param.sliceParam[0];
	pOut->parameter.bitRate[0] = param.bitRate[0];
	pOut->sliceParam[1] = param.sliceParam[1];
	pOut->parameter.bitRate[1] = param.bitRate[1];
	pOut->parameter.numUnitsInTick = param.numUnitsInTick;
	pOut->parameter.timeScale = param.timeScale;
	if ((param.mode & AVCENC_MODE_VUI) && param.vuiIndex <= 4)
		pOut->vui = avcencVuis[param.vuiIndex];
	return 0;
}

static inline __attribute__((always_inline)) int videocodecMapAvcencErrorInline(int result)
{
	unsigned int index = (unsigned int)result - 0x80620201;

	if (index <= 16)
		return videocodecAvcencErrors[index];
	if (result == (int)0x80620541)
		return VIDEOCODEC_AVCENC_ERROR_DSP0_PARAM;
	if (result == (int)0x80620542)
		return VIDEOCODEC_AVCENC_ERROR_INVALID_GOP_LENGTH;
	if (result == (int)0x80620543)
		return VIDEOCODEC_AVCENC_ERROR_INVALID_FRAME_RATE;
	if (result == (int)0x80620580)
		return VIDEOCODEC_AVCENC_ERROR_INVALID_STREAM;
	if (result == (int)0x806205C0)
		return VIDEOCODEC_AVCENC_ERROR_INVALID_STATE;
	return result;
}

/* The memory an AVC encoder for pParam needs, checked against the level's limits. */
static inline __attribute__((always_inline)) int
avcencQueryEncoderMemSizeInline(const AvcencInitParamInternal *pParam,
	AvcencEncoderMemSize *pMemSize)
{
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	Dsp0VideocodecEncoderInitParam param;
	const AvcencLevelLimit *limit;
	unsigned int horizontal, vertical, numOfStreams, level;
	unsigned int alignedHorizontal, alignedVertical, numUnitsInTick, timeScale, mbs;
	SceSize streamSize, bufferSize, workSize;
	int ret;

	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		return ret;
	horizontal = pParam->horizontal;
	if (horizontal > global->dsp0EncoderInfo.horizontal)
		goto invalid;
	vertical = pParam->vertical;
	if (vertical > global->dsp0EncoderInfo.vertical)
		goto invalid;
	numOfStreams = pParam->numOfStreams;
	if (numOfStreams > global->dsp0EncoderInfo.numOfRefFrames)
		goto invalid;
	level = pParam->level;
	alignedHorizontal = pParam->alignedHorizontal;
	alignedVertical = pParam->alignedVertical;
	numUnitsInTick = pParam->parameter.numUnitsInTick;
	timeScale = pParam->parameter.timeScale;
	if (level <= 13) {
		limit = &avcencLevelLimits[0];
	} else {
		if (level > 41)
			goto invalid;
		if ((unsigned char)(level - 20) > 21)
			goto invalid;
		limit = avcencLevelLimitTable[(unsigned char)(level - 20)];
		if (limit == NULL)
			goto invalid;
	}
	if (numUnitsInTick > 120000 || timeScale < 2 * numUnitsInTick ||
	    timeScale > 60 * numUnitsInTick) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_FRAME_RATE;
		goto unlock;
	}
	mbs = ((alignedHorizontal + 15) >> 4) * ((alignedVertical + 15) >> 4);
	if (limit->maxMbps < (unsigned int)((unsigned long long)mbs * timeScale / numUnitsInTick / 2) ||
	    mbs > limit->maxFs ||
	    pParam->parameter.bitRate[1] > limit->maxBr ||
	    limit->maxBr < pParam->parameter.bitRate[0] ||
	    pParam->parameter.bitRate[1] < 32000 || pParam->parameter.bitRate[0] < 32000 ||
	    pParam->parameter.cpbSize > limit->maxFs * 768 / 4)
		goto invalid;
	if (pParam->parameter.gopLength > 300 ||
	    2 * numUnitsInTick * pParam->parameter.gopLength > 10 * timeScale) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_GOP_LENGTH;
		goto unlock;
	}
	if (level - 10 > 31)
		goto invalid;

	memset(&param, 0, sizeof(param));
	param.profile = pParam->profile;
	param.level = level;
	param.userOption[0] = pParam->userOption[0];
	param.userOption[1] = pParam->userOption[1];
	param.pitch = pParam->pitch;
	param.alignedHorizontal = alignedHorizontal;
	param.alignedVertical = alignedVertical;
	param.horizontal = horizontal;
	param.vertical = vertical;
	param.numOfStreams = numOfStreams != 0 ? numOfStreams : 1;
	param.rateControl[0] = numUnitsInTick;
	param.rateControl[1] = timeScale;
	param.rateControl[2] = pParam->rateControl[0];
	param.rateControl[3] = pParam->rateControl[1];
	param.codingOption[0] = pParam->codingOption[0];
	param.codingOption[1] = pParam->codingOption[1];
	param.codingOption[2] = pParam->codingOption[2];
	param.sliceOption = pParam->sliceOption;
	param.sliceParam[0] = pParam->sliceParam[0];
	param.sliceParam[1] = pParam->sliceParam[1];
	param.vuiPresent = pParam->vui.videoSignalTypePresentFlag;
	param.vui[0] = pParam->vui.videoFormat;
	param.vui[1] = pParam->vui.videoFullRangeFlag;
	param.vui[2] = pParam->vui.colourDescriptionPresentFlag;
	param.vui[3] = pParam->vui.colourPrimaries;
	param.vui[4] = pParam->vui.transferCharacteristics;
	param.vui[5] = pParam->vui.matrixCoefficients;
	ret = dsp0VideocodecQueryEncoderMemSize(&param, &streamSize, &bufferSize, &workSize);
	ret = videocodecMapAvcencErrorInline(ret);
	if (ret >= 0) {
		pMemSize->streamMemorySize = (streamSize + 0x3FFFF) >> 18 << 18;
		pMemSize->bufferMemorySize = (bufferSize + 0x3FFFF) >> 18 << 18;
		pMemSize->workMemorySize = workSize;
	}
	goto unlock;
invalid:
	ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
unlock:
	sceKernelUnlockFastMutex(global->pAllMutex);
	return ret;
}

static __attribute__((noinline)) int avcencQueryEncoderMemSize(const AvcencInitParamInternal *pParam,
	AvcencEncoderMemSize *pMemSize)
{
	return avcencQueryEncoderMemSizeInline(pParam, pMemSize);
}

/*
 * Create an AVC encoder in a free slot of the process: its stream memory from the caller or a
 * block of its own, its frame memory the caller's, and the stream's SPS and PPS copied to the
 * caller's header memory.
 */
static __attribute__((noinline)) int avcencCreateEncoder(AvcencControl *pCtrl,
	const AvcencInitParamInternal *pParam)
{
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	VideocodecEncoder *encoder;
	VideocodecLockedRanges ranges;
	VideocodecLockedRange range[1];
	Dsp0VideocodecEncoderInitParam param;
	Dsp0VideocodecEncoderParam createParam;
	AvcencEncoderMemSize memSize;
	VideocodecMemBlock memBlock;
	uintptr_t physicalAddress;
	uintptr_t bufferAddress;
	void *base;
	unsigned int handle, i;
	int ret;

	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		return ret;
	if (global->dsp0Library == 0) {
		sceKernelUnlockFastMutex(global->pAllMutex);
		return VIDEOCODEC_AVCENC_ERROR_NOT_INITIALIZE;
	}
	encoder = &global->encoders[0];
	for (i = 0; i < global->dsp0EncoderInfo.numOfStreams; i++, encoder++) {
		ret = lockFastMutex(&encoder->mutex);
		if (ret < 0) {
			sceKernelUnlockFastMutex(global->pAllMutex);
			return ret;
		}
		if (encoder->handle == 0)
			break;
		sceKernelUnlockFastMutex(&encoder->mutex);
	}
	if (i >= global->dsp0EncoderInfo.numOfStreams) {
		sceKernelUnlockFastMutex(global->pAllMutex);
		return VIDEOCODEC_AVCENC_ERROR_ALREADY_USED;
	}
	videocodecInitLockedRanges(&encoder->lockedRanges, 0, encoder->lockedRange, 1,
		VIDEOCODEC_ENCODER_ERROR_BASE);
	videocodecInitLockedRanges(&ranges, 0, range, 1, VIDEOCODEC_ENCODER_ERROR_BASE);
	encoder->encodeFlags = 0;

	memset(&param, 0, sizeof(param));
	param.profile = pParam->profile;
	param.level = pParam->level;
	param.userOption[0] = pParam->userOption[0];
	param.userOption[1] = pParam->userOption[1];
	param.pitch = pParam->pitch;
	param.alignedHorizontal = pParam->alignedHorizontal;
	param.alignedVertical = pParam->alignedVertical;
	param.horizontal = pParam->horizontal;
	param.vertical = pParam->vertical;
	param.numOfStreams = pParam->numOfStreams != 0 ? pParam->numOfStreams : 1;
	param.rateControl[0] = pParam->parameter.numUnitsInTick;
	param.rateControl[1] = pParam->parameter.timeScale;
	param.rateControl[2] = pParam->rateControl[0];
	param.rateControl[3] = pParam->rateControl[1];
	param.codingOption[0] = pParam->codingOption[0];
	param.codingOption[1] = pParam->codingOption[1];
	param.codingOption[2] = pParam->codingOption[2];
	param.sliceOption = pParam->sliceOption;
	param.sliceParam[0] = pParam->sliceParam[0];
	param.sliceParam[1] = pParam->sliceParam[1];
	param.vuiPresent = pParam->vui.videoSignalTypePresentFlag;
	param.vui[0] = pParam->vui.videoFormat;
	param.vui[1] = pParam->vui.videoFullRangeFlag;
	param.vui[2] = pParam->vui.colourDescriptionPresentFlag;
	param.vui[3] = pParam->vui.colourPrimaries;
	param.vui[4] = pParam->vui.transferCharacteristics;
	param.vui[5] = pParam->vui.matrixCoefficients;
	memSize.streamMemorySize = 0;
	memSize.bufferMemorySize = 0;
	memSize.workMemorySize = 0;
	ret = avcencQueryEncoderMemSize(pParam, &memSize);
	if (ret < 0)
		goto release;

	/* the stream memory: the caller's, or a block of its own */
	encoder->streamMemory.kind = VIDEOCODEC_MEMORY_BLOCK;
	if (pCtrl->workMemory != NULL) {
		if (memSize.streamMemorySize > pCtrl->workMemorySize) {
			ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
			goto release;
		}
		if (((uintptr_t)pCtrl->workMemory & 0xFF) != 0) {
			ret = VIDEOCODEC_AVCENC_ERROR_INVALID_POINTER;
			goto release;
		}
		ret = sceCodecEngineRegisterUnmapMemory(0, (SceUIntVAddr)pCtrl->workMemory,
			pCtrl->workMemorySize);
		if (ret < 0)
			goto release;
		ret = sceCodecEngineUnmapUserVAtoKernelVA(0, pCtrl->workMemory, &base);
		if (ret < 0) {
			sceCodecEngineUnregisterUnmapMemory(0, (SceUIntVAddr)pCtrl->workMemory,
				pCtrl->workMemorySize);
			goto release;
		}
		encoder->streamMemory.base = base;
		encoder->streamMemory.memBlockId = 0;
		encoder->streamMemory.size = pCtrl->workMemorySize;
		encoder->streamMemory.kind = VIDEOCODEC_MEMORY_UNMAP;
	} else {
		if (videocodecAllocMemBlock("SceAvcencSwMemBuf_Phycont",
				SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_PHYCONT_NC_RW, 0x40000,
				(memSize.streamMemorySize + 0x3FFFF) >> 18 << 18, &memBlock) < 0) {
			ret = VIDEOCODEC_AVCENC_ERROR_OUT_OF_MEMORY;
			goto release;
		}
		encoder->streamMemory.size = memBlock.size;
		encoder->streamMemory.kind = VIDEOCODEC_MEMORY_BLOCK;
		encoder->streamMemory.base = memBlock.base;
		encoder->streamMemory.memBlockId = memBlock.memBlockId;
	}
	memset(encoder->streamMemory.base, 0, encoder->streamMemory.size);
	encoder->encodeFlags |= VIDEOCODEC_ENCODER_STREAM_MEMORY;

	/* the frame memory: the caller's, locked or mapped for the hardware */
	if (pCtrl->frameMemory == NULL || ((uintptr_t)pCtrl->frameMemory & 0xFF) != 0) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_POINTER;
		goto release;
	}
	if (pCtrl->frameMemorySize < memSize.bufferMemorySize) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
		goto release;
	}
	ret = sceCodecEngineRegisterUnmapMemory(0, (SceUIntVAddr)pCtrl->frameMemory,
		pCtrl->frameMemorySize);
	if (ret == SCE_CODECENGINE_ERROR_INVALID_POINTER) {
		if (videocodecLockRange(&encoder->lockedRanges, &physicalAddress,
				(uintptr_t)pCtrl->frameMemory, pCtrl->frameMemorySize, 32,
				VIDEOCODEC_CACHE_INVALIDATE, VIDEOCODEC_LOCK_CACHED) < 0) {
			ret = VIDEOCODEC_AVCENC_ERROR_INVALID_POINTER;
			goto release;
		}
		encoder->frameBuf.pBuf = pCtrl->frameMemory;
		encoder->frameBuf.size = pCtrl->frameMemorySize;
		bufferAddress = (uintptr_t)pCtrl->frameMemory;
	} else {
		if (ret < 0) {
			ret = VIDEOCODEC_AVCENC_ERROR_INVALID_POINTER;
			goto release;
		}
		ret = sceCodecEngineUnmapUserVAtoKernelVA(0, pCtrl->frameMemory, &base);
		if (ret < 0) {
			sceCodecEngineUnregisterUnmapMemory(0, (SceUIntVAddr)pCtrl->frameMemory,
				pCtrl->frameMemorySize);
			goto release;
		}
		encoder->frameBuf.pBuf = base;
		encoder->frameBuf.size = pCtrl->frameMemorySize;
		encoder->encodeFlags |= VIDEOCODEC_ENCODER_FRAME_BUF_UNMAP;
		bufferAddress = (uintptr_t)base;
	}

	createParam.streamAddress = (uintptr_t)encoder->streamMemory.base;
	createParam.streamSize = encoder->streamMemory.size;
	createParam.bufferAddress = bufferAddress;
	createParam.bufferSize = pCtrl->frameMemorySize;
	createParam.pInitParam = &param;
	createParam.parameter.bitRate[0] = pParam->parameter.bitRate[0];
	createParam.parameter.bitRate[1] = pParam->parameter.bitRate[1];
	createParam.parameter.cpbSize = pParam->parameter.cpbSize * 8;
	createParam.parameter.gopLength = pParam->parameter.gopLength;
	createParam.parameter.numUnitsInTick = encoder->numUnitsInTick;
	createParam.parameter.timeScale = encoder->timeScale;
	ret = dsp0VideocodecCreateEncoder(global->dsp0Library, &handle, &createParam);
	ret = videocodecMapAvcencErrorInline(ret);
	if (ret < 0)
		goto release;
	encoder->handle = handle;
	pCtrl->handle = handle - 1;
	encoder->level = param.level;
	encoder->frameRateGop = param.codingOption[2];
	encoder->pitch = param.pitch;
	encoder->bitRate[0] = pParam->parameter.bitRate[0];
	encoder->bitRate[1] = pParam->parameter.bitRate[1];
	encoder->cpbSize = pParam->parameter.cpbSize * 8;
	encoder->gopLength = pParam->parameter.gopLength;
	encoder->alignedHorizontal = param.alignedHorizontal;
	encoder->alignedVertical = param.alignedVertical;
	encoder->numUnitsInTick = param.rateControl[0];
	encoder->timeStampNumUnitsInTick = param.rateControl[0];
	encoder->timeScale = param.rateControl[1];
	encoder->timeStampTimeScale = param.rateControl[1];
	encoder->lastPts.upper = 0;
	encoder->lastPts.lower = 0;
	encoder->encodeFlags |= VIDEOCODEC_ENCODER_NEW_PARAMETERS;

	/* the SPS and PPS, one after the other in the caller's header memory */
	if (pCtrl->headerMemory != NULL) {
		void *header = pCtrl->headerMemory;

		if (createParam.headers[0].size + createParam.headers[1].size > 0x400) {
			ret = VIDEOCODEC_AVCENC_ERROR_INVALID_STATE;
			goto release;
		}
		if (pCtrl->headerMemorySize < 0x400) {
			ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
			goto release;
		}
		if (videocodecLockRange(&ranges, &physicalAddress, (uintptr_t)header,
				pCtrl->headerMemorySize, 32, 0,
				VIDEOCODEC_LOCK_CACHED | VIDEOCODEC_LOCK_SCATTERED) < 0) {
			ret = VIDEOCODEC_AVCENC_ERROR_INVALID_POINTER;
			goto release;
		}
		ret = SceCodecEngineWrapperForDriver_FAF590A6(header,
			(const void *)createParam.headers[0].address, createParam.headers[0].size);
		if (ret < 0)
			goto release;
		pCtrl->headers[0].data = header;
		pCtrl->headers[0].size = createParam.headers[0].size;
		pCtrl->headers[0].type = createParam.headers[0].type;
		pCtrl->headers[0].referencePriority = createParam.headers[0].referencePriority;
		header = (char *)header + createParam.headers[0].size;
		ret = SceCodecEngineWrapperForDriver_FAF590A6(header,
			(const void *)createParam.headers[1].address, createParam.headers[1].size);
		if (ret < 0)
			goto release;
		pCtrl->headers[1].data = header;
		pCtrl->headers[1].type = createParam.headers[1].type;
		pCtrl->headers[1].referencePriority = createParam.headers[1].referencePriority;
		pCtrl->headers[1].size = createParam.headers[1].size;
	}
	videocodecUnlockRanges(&ranges);
	goto unlock;

release:
	if (encoder->encodeFlags & VIDEOCODEC_ENCODER_FRAME_BUF_MEMBLOCK) {
		sceKernelFreeMemBlock(encoder->frameBufMemBlockId);
		encoder->encodeFlags &= ~VIDEOCODEC_ENCODER_FRAME_BUF_MEMBLOCK;
	}
	if (encoder->encodeFlags & VIDEOCODEC_ENCODER_FRAME_BUF_UNMAP) {
		sceCodecEngineUnmapKernelVAtoUserVA(0, encoder->frameBuf.pBuf, &base);
		sceCodecEngineUnregisterUnmapMemory(0, (SceUIntVAddr)base, encoder->frameBuf.size);
		encoder->encodeFlags &= ~VIDEOCODEC_ENCODER_FRAME_BUF_UNMAP;
	}
	__builtin_memset(&encoder->frameBuf, 0, sizeof(encoder->frameBuf));
	if (encoder->encodeFlags & VIDEOCODEC_ENCODER_STREAM_MEMORY) {
		if (encoder->streamMemory.kind != VIDEOCODEC_MEMORY_BLOCK) {
			if (sceCodecEngineUnmapKernelVAtoUserVA(0, encoder->streamMemory.base,
					&base) >= 0)
				sceCodecEngineUnregisterUnmapMemory(0, (SceUIntVAddr)base,
					encoder->streamMemory.size);
		} else {
			memBlock.kind = 0;
			memBlock.memBlockId = encoder->streamMemory.memBlockId;
			memBlock.base = encoder->streamMemory.base;
			memBlock.size = encoder->streamMemory.size;
			videocodecFreeMemBlock(&memBlock);
		}
		encoder->streamMemory.base = NULL;
		encoder->streamMemory.size = 0;
		encoder->streamMemory.kind = 0;
		encoder->streamMemory.memBlockId = 0;
		encoder->encodeFlags &= ~VIDEOCODEC_ENCODER_STREAM_MEMORY;
	}
	videocodecUnlockRanges(&ranges);
	videocodecUnlockRanges(&encoder->lockedRanges);
unlock:
	sceKernelUnlockFastMutex(&encoder->mutex);
	sceKernelUnlockFastMutex(global->pAllMutex);
	return ret;
}

static __attribute__((noinline)) int videocodecMapAvcencError(int result)
{
	return videocodecMapAvcencErrorInline(result);
}

int SceAvcodecForDriver_7DC13C29(unsigned int codecType, unsigned int preset)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	int ret, unlocked;

	if (codecType != VIDEOCODEC_CODEC_TYPE_AVCENC) {
		sceSyscallLeave(syscall);
		return VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret >= 0) {
		global->availablePreset = preset;
		unlocked = sceKernelUnlockFastMutex(global->pAllMutex);
		if (ret == 0)
			ret = unlocked;
	}
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_7385FB51(unsigned int codecType, const AvcencInitParamBasic *pParam,
	AvcencEncoderMemSize *pMemSize)
{
	SceUInt32 syscall = sceSyscallEnter();
	const AvcencBasicGeometry *geometry;
	AvcencInitParamInternal internal;
	AvcencInitParam param;
	unsigned int flags, frameRate, bitRateIndex, gopLength, bitRate[2];
	int ret;

	if (codecType != VIDEOCODEC_CODEC_TYPE_AVCENC)
		goto invalid;
	if (pParam == NULL || pMemSize == NULL) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_POINTER;
		goto out;
	}
	if (pParam->size != sizeof(*pParam)) {
		ret = VIDEOCODEC_ENCODER_ERROR_INVALID_ARGUMENT_SIZE;
		goto out;
	}
	flags = pParam->flags;
	frameRate = pParam->frameRate - 1;
	bitRateIndex = pParam->bitRateIndex;
	if (frameRate > 1 || (flags & AVCENC_BASIC_GEOMETRY) - 1 > 2 || bitRateIndex - 1 > 4)
		goto invalid;
	geometry = &avcencBasicGeometries[(flags & AVCENC_BASIC_GEOMETRY) - 1];
	if (flags & AVCENC_BASIC_BIT_RATE) {
		bitRate[0] = pParam->bitRate[0];
		bitRate[1] = pParam->bitRate[1];
	} else {
		bitRate[0] = avcencBasicBitRates[bitRateIndex];
		bitRate[1] = bitRate[0] * 12 / 10;
	}
	if (flags & AVCENC_BASIC_GOP_LENGTH)
		gopLength = pParam->gopLength;
	else
		gopLength = avcencBasicGopLengths[frameRate];
	param = (AvcencInitParam){
		.pitch = geometry->pitch,
		.alignedHorizontal = geometry->alignedHorizontal,
		.alignedVertical = geometry->alignedVertical,
		.horizontal = geometry->horizontal,
		.vertical = geometry->vertical,
		.bitRate = { bitRate[0], bitRate[1] },
		.mode = 2,
		.preset = geometry->preset,
		.numUnitsInTick = avcencBasicNumUnitsInTick[frameRate],
		.timeScale = 240000,
	};
	ret = avcencCheckInitParam(&param);
	if (ret >= 0) {
		avcencInitParamToInternal(&internal, &param);
		if (gopLength != 0)
			internal.parameter.gopLength = gopLength;
		internal.parameter.cpbSize = 3 * param.alignedHorizontal * param.alignedVertical / 8;
		ret = avcencQueryEncoderMemSize(&internal, pMemSize);
	}
	goto out;
invalid:
	ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
out:
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_5C3434FB(unsigned int codecType, const AvcencInitParam *pParam,
	AvcencEncoderMemSize *pMemSize)
{
	SceUInt32 syscall = sceSyscallEnter();
	AvcencInitParamInternal internal;
	int ret;

	if (codecType != VIDEOCODEC_CODEC_TYPE_AVCENC) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
	} else if (pParam == NULL || pMemSize == NULL) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_POINTER;
	} else {
		ret = avcencCheckInitParam(pParam);
		if (ret >= 0) {
			avcencInitParamToInternal(&internal, pParam);
			ret = avcencQueryEncoderMemSize(&internal, pMemSize);
		}
	}
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_BF3835E2(unsigned int codecType, const AvcencInitParamInternal *pParam,
	AvcencEncoderMemSize *pMemSize)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	if (codecType != VIDEOCODEC_CODEC_TYPE_AVCENC)
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
	else if (pParam == NULL || pMemSize == NULL)
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_POINTER;
	else
		ret = avcencQueryEncoderMemSizeInline(pParam, pMemSize);
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_C5E720E2(unsigned int codecType, AvcencControl *pCtrl,
	const AvcencInitParamBasic *pParam)
{
	SceUInt32 syscall = sceSyscallEnter();
	const AvcencBasicGeometry *geometry;
	AvcencInitParamInternal internal;
	AvcencInitParam param;
	unsigned int flags, frameRate, bitRateIndex, gopLength, bitRate[2];
	int ret;

	if (codecType != VIDEOCODEC_CODEC_TYPE_AVCENC)
		goto invalid;
	if (pCtrl == NULL || pParam == NULL)
		return VIDEOCODEC_AVCENC_ERROR_INVALID_POINTER;
	if (pParam->size != sizeof(*pParam)) {
		ret = VIDEOCODEC_ENCODER_ERROR_INVALID_ARGUMENT_SIZE;
		goto out;
	}
	flags = pParam->flags;
	frameRate = pParam->frameRate - 1;
	bitRateIndex = pParam->bitRateIndex;
	if (frameRate > 1 || (flags & AVCENC_BASIC_GEOMETRY) - 1 > 2 || bitRateIndex - 1 > 4)
		goto invalid;
	geometry = &avcencBasicGeometries[(flags & AVCENC_BASIC_GEOMETRY) - 1];
	if (flags & AVCENC_BASIC_BIT_RATE) {
		bitRate[0] = pParam->bitRate[0];
		bitRate[1] = pParam->bitRate[1];
	} else {
		bitRate[0] = avcencBasicBitRates[bitRateIndex];
		bitRate[1] = bitRate[0] * 12 / 10;
	}
	if (flags & AVCENC_BASIC_GOP_LENGTH)
		gopLength = pParam->gopLength;
	else
		gopLength = avcencBasicGopLengths[frameRate];
	param = (AvcencInitParam){
		.pitch = geometry->pitch,
		.alignedHorizontal = geometry->alignedHorizontal,
		.alignedVertical = geometry->alignedVertical,
		.horizontal = geometry->horizontal,
		.vertical = geometry->vertical,
		.bitRate = { bitRate[0], bitRate[1] },
		.mode = 2,
		.preset = geometry->preset,
		.numUnitsInTick = avcencBasicNumUnitsInTick[frameRate],
		.timeScale = 240000,
	};
	ret = avcencCheckInitParam(&param);
	if (ret >= 0) {
		avcencInitParamToInternal(&internal, &param);
		if (gopLength != 0)
			internal.parameter.gopLength = gopLength;
		internal.parameter.cpbSize = 3 * param.alignedHorizontal * param.alignedVertical / 8;
		ret = avcencCreateEncoder(pCtrl, &internal);
	}
	goto out;
invalid:
	ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
out:
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_B2A51EB0(unsigned int codecType, AvcencControl *pCtrl,
	const AvcencInitParam *pParam)
{
	SceUInt32 syscall = sceSyscallEnter();
	AvcencInitParamInternal internal;
	int ret;

	if (codecType != VIDEOCODEC_CODEC_TYPE_AVCENC) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
	} else {
		if (pParam == NULL || pCtrl == NULL)
			return VIDEOCODEC_AVCENC_ERROR_INVALID_POINTER;
		ret = avcencCheckInitParam(pParam);
		if (ret >= 0) {
			avcencInitParamToInternal(&internal, pParam);
			ret = avcencCreateEncoder(pCtrl, &internal);
		}
	}
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_71C37694(unsigned int codecType, AvcencControl *pCtrl,
	const AvcencInitParamInternal *pParam)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	if (codecType != VIDEOCODEC_CODEC_TYPE_AVCENC) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
	} else {
		if (pParam == NULL || pCtrl == NULL)
			return VIDEOCODEC_AVCENC_ERROR_INVALID_POINTER;
		ret = avcencCreateEncoder(pCtrl, pParam);
	}
	sceSyscallLeave(syscall);
	return ret;
}

/* Delete the AVC encoder pCtrl names in process pid, releasing its frame and stream memory. */
static inline __attribute__((always_inline)) int
avcencDeleteEncoderInline(AvcencControl *pCtrl, SceUID pid)
{
	VideocodecGlobal *global = _GetVideocodecGlobal(pid);
	VideocodecEncoder *encoder;
	VideocodecMemBlock memBlock;
	unsigned int handle, index;
	void *base;
	int ret, err;

	if (pCtrl == NULL)
		return VIDEOCODEC_AVCENC_ERROR_INVALID_POINTER;
	handle = pCtrl->handle;
	if (handle == 0)
		return VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		return ret;
	if (handle < 4096 || (index = handle - 4096) >= global->dsp0EncoderInfo.numOfStreams) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
		goto unlock;
	}
	encoder = &global->encoders[index];
	ret = lockFastMutex(&encoder->mutex);
	if (ret < 0)
		goto unlock;
	handle = encoder->handle;
	if (handle == 0) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
		sceKernelUnlockFastMutex(&encoder->mutex);
		goto unlock;
	}
	sceKernelUnlockFastMutex(global->pAllMutex);
	err = dsp0VideocodecDeleteEncoder(handle, pid);
	err = videocodecMapAvcencErrorInline(err);
	if (err == SCE_KERNEL_ERROR_THREAD_STOPPED || err == SCE_KERNEL_ERROR_THREAD_SUSPENDED) {
		sceKernelUnlockFastMutex(&encoder->mutex);
		return err;
	}
	encoder->handle = 0;
	if (encoder->encodeFlags & VIDEOCODEC_ENCODER_FRAME_BUF_MEMBLOCK) {
		sceKernelFreeMemBlock(encoder->frameBufMemBlockId);
		encoder->encodeFlags &= ~VIDEOCODEC_ENCODER_FRAME_BUF_MEMBLOCK;
	}
	if (encoder->encodeFlags & VIDEOCODEC_ENCODER_FRAME_BUF_UNMAP) {
		sceCodecEngineUnmapKernelVAtoUserVA(pid, encoder->frameBuf.pBuf, &base);
		ret = sceCodecEngineUnregisterUnmapMemory(pid, (SceUIntVAddr)base,
			encoder->frameBuf.size);
		encoder->encodeFlags &= ~VIDEOCODEC_ENCODER_FRAME_BUF_UNMAP;
	}
	__builtin_memset(&encoder->frameBuf, 0, sizeof(encoder->frameBuf));
	if (encoder->encodeFlags & VIDEOCODEC_ENCODER_STREAM_MEMORY) {
		if (encoder->streamMemory.kind != VIDEOCODEC_MEMORY_BLOCK) {
			if (sceCodecEngineUnmapKernelVAtoUserVA(pid, encoder->streamMemory.base,
					&base) >= 0)
				sceCodecEngineUnregisterUnmapMemory(pid, (SceUIntVAddr)base,
					encoder->streamMemory.size);
		} else {
			memBlock.kind = 0;
			memBlock.memBlockId = encoder->streamMemory.memBlockId;
			memBlock.base = encoder->streamMemory.base;
			memBlock.size = encoder->streamMemory.size;
			videocodecFreeMemBlock(&memBlock);
		}
		encoder->streamMemory.base = NULL;
		encoder->streamMemory.size = 0;
		encoder->streamMemory.kind = 0;
		encoder->streamMemory.memBlockId = 0;
		encoder->encodeFlags &= ~VIDEOCODEC_ENCODER_STREAM_MEMORY;
	}
	__builtin_memset(&encoder->streamMemory, 0, 8);
	encoder->encodeFlags = 0;
	videocodecUnlockRangesProc(&encoder->lockedRanges, pid);
	sceKernelUnlockFastMutex(&encoder->mutex);
	if (ret != 0)
		return ret;
	return err < 0 ? err : 0;

unlock:
	sceKernelUnlockFastMutex(global->pAllMutex);
	return ret;
}

int avcencDeleteEncoderForProcess(AvcencControl *pCtrl, SceUID pid)
{
	return avcencDeleteEncoderInline(pCtrl, pid);
}

int SceAvcodecForDriver_2895CB6D(AvcencControl *pCtrl)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	ret = avcencDeleteEncoderInline(pCtrl, 0);
	sceSyscallLeave(syscall);
	return ret;
}

/*
 * Encodes pFrame on the AVC encoder pCtrl names. The one AU pArrayAu names is copied in from
 * the caller, filled with the encoded stream (copied to its es buffer) and its time stamps,
 * and copied back out.
 */
int SceAvcodecForDriver_A0E63278(const AvcencControl *pCtrl, const AvcencCscFrame *pFrame,
	AvcencArrayAu *pArrayAu)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global;
	VideocodecEncoder *encoder;
	AvcencAu **userAu = NULL, **kernelAu = NULL;
	AvcencAu *userAus[1], *aus[1], *au;
	VideocodecLockedRange lockedRange[1];
	VideocodecLockedRanges ranges;
	Dsp0VideocodecEncoderPicture input;
	Dsp0VideocodecEncodeFrame frame;
	uintptr_t physicalAddress;
	unsigned int handle, index, numOfOutput, prevNumUnitsInTick, prevTimeScale, timeScale;
	unsigned int mode, i;
	SceUInt64 decodeTime, presentationTime;
	char auBuf[sizeof(AvcencAu)];
	int ret, copied = 0;

	if (pArrayAu == NULL) {
		_GetVideocodecGlobal(0);
		copied = VIDEOCODEC_AVCENC_ERROR_INVALID_POINTER;
		goto out;
	}
	if (pArrayAu->numOfElm == 1) {
		userAu = pArrayAu->pAu;
		if (userAu != NULL) {
			copied = sceKernelCopyFromUser(userAus, userAu, sizeof(userAus));
			if (copied < 0 || (copied = videocodecCopyInPictures(pArrayAu->numOfElm,
					(void **)aus, auBuf, (void *const *)userAus,
					sizeof(AvcencAu))) < 0) {
				pArrayAu->pAu = userAu;
				goto out;
			}
			pArrayAu->pAu = aus;
			kernelAu = aus;
		}
	}
	global = _GetVideocodecGlobal(0);
	if (pCtrl == NULL || pFrame == NULL || pArrayAu->pAu[0] == NULL) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_POINTER;
		goto copyOut;
	}
	if (pArrayAu->numOfElm != 1 || (handle = pCtrl->handle) == 0) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
		goto copyOut;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto copyOut;
	if (handle < 4096 || (index = handle - 4096) >= global->dsp0EncoderInfo.numOfStreams) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
		goto unlockAll;
	}
	encoder = &global->encoders[index];
	ret = lockFastMutex(&encoder->mutex);
	if (ret < 0)
		goto unlockAll;
	handle = encoder->handle;
	if (handle == 0) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
		sceKernelUnlockFastMutex(&encoder->mutex);
		goto unlockAll;
	}
	sceKernelUnlockFastMutex(global->pAllMutex);
	videocodecInitLockedRanges(&ranges, 0, lockedRange, 1, VIDEOCODEC_ENCODER_ERROR_BASE);
	au = pArrayAu->pAu[0];
	if (au->size != sizeof(*au)) {
		ret = VIDEOCODEC_ENCODER_ERROR_INVALID_ARGUMENT_SIZE;
		goto unlockRanges;
	}
	if ((pFrame->pixelType != 32 && pFrame->pixelType != 16) ||
	    pFrame->frameWidth != encoder->alignedHorizontal ||
	    pFrame->frameHeight != encoder->alignedVertical) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
		goto unlockRanges;
	}
	if (encoder->encodeFlags & VIDEOCODEC_ENCODER_NEW_PARAMETERS) {
		encoder->encodeFlags &= ~VIDEOCODEC_ENCODER_NEW_PARAMETERS;
		encoder->numOfEncoded = 0;
		encoder->decodeTime = 0;
		encoder->presentationTime = 0;
		encoder->pendingNumUnitsInTick[0] = encoder->numUnitsInTick;
		encoder->pendingNumUnitsInTick[1] = encoder->numUnitsInTick;
		encoder->pendingTimeScale[0] = encoder->timeScale;
		encoder->pendingTimeScale[1] = encoder->timeScale;
	}
	ret = dsp0VideocodecGetInputPicture(handle, &input);
	ret = videocodecMapAvcencErrorInline(ret);
	if (ret < 0)
		goto unlockRanges;
	if (input.size < 3 * pFrame->frameHeight * pFrame->framePitch / 2) {
		ret = VIDEOCODEC_AVCENC_ERROR_UNSUPPORT_IMAGE_SIZE;
		goto unlockRanges;
	}
	frame.pPicture = (const void *)input.address;
	frame.pitch = pFrame->framePitch;
	frame.horizontal = pFrame->frameWidth;
	frame.vertical = pFrame->frameHeight;
	mode = pFrame->encodeMode;
	if (mode == 32 || mode == 1 || mode == 64) {
		ret = videocodecAvcencInputCsc((VideocodecEncoderFrame *)&frame, pFrame,
			encoder->encodeFlags &
			(VIDEOCODEC_ENCODER_FRAME_BUF_MEMBLOCK | VIDEOCODEC_ENCODER_FRAME_BUF_UNMAP));
		if (ret < 0)
			goto unlockRanges;
		mode = pFrame->encodeMode;
	}
	frame.mode = mode;
	frame.parameter.bitRate[0] = encoder->bitRate[0];
	frame.parameter.bitRate[1] = encoder->bitRate[1];
	frame.parameter.cpbSize = encoder->cpbSize;
	frame.parameter.gopLength = encoder->gopLength;
	frame.parameter.numUnitsInTick = encoder->numUnitsInTick;
	frame.parameter.timeScale = encoder->timeScale;
	ret = dsp0VideocodecEncode(handle, &frame, &encoder->au, &numOfOutput);
	ret = videocodecMapAvcencError(ret);
	if (ret < 0 && ret != VIDEOCODEC_AVCENC_ERROR_ES_BUFFER_FULL)
		goto unlockRanges;

	/* the time stamps run two frames behind the parameters */
	prevNumUnitsInTick = encoder->pendingNumUnitsInTick[0];
	prevTimeScale = encoder->pendingTimeScale[0];
	encoder->pendingNumUnitsInTick[0] = encoder->pendingNumUnitsInTick[1];
	encoder->pendingNumUnitsInTick[1] = encoder->numUnitsInTick;
	encoder->pendingTimeScale[0] = encoder->pendingTimeScale[1];
	encoder->pendingTimeScale[1] = encoder->timeScale;
	if (numOfOutput != 0) {
		int err;

		err = videocodecLockRange(&ranges, &physicalAddress, (uintptr_t)au->pEs,
			encoder->au.streamSize, 16, 0,
			VIDEOCODEC_LOCK_CACHED | VIDEOCODEC_LOCK_SCATTERED);
		if (err < 0 || (err = SceCodecEngineWrapperForDriver_FAF590A6(au->pEs,
				(const void *)encoder->au.streamAddress,
				encoder->au.streamSize)) < 0) {
			ret = err;
			goto unlockRanges;
		}
		au->esSize = encoder->au.streamSize;
		timeScale = encoder->timeStampTimeScale;
		if (encoder->numOfEncoded != 0) {
			SceInt64 delta = (SceInt64)((SceUInt64)(2 * prevNumUnitsInTick) * timeScale) /
				prevTimeScale;

			decodeTime = encoder->decodeTime + delta;
			presentationTime = encoder->presentationTime + delta;
			encoder->decodeTime = decodeTime;
			encoder->presentationTime = presentationTime;
		} else {
			decodeTime = encoder->decodeTime;
			presentationTime = encoder->presentationTime;
		}
		au->dts.integer = decodeTime / timeScale;
		au->dts.fraction = decodeTime % timeScale * prevTimeScale / timeScale;
		au->dts.timeScale = prevTimeScale;
		au->pts.integer = presentationTime / timeScale;
		au->pts.fraction = presentationTime % timeScale * prevTimeScale / timeScale;
		au->pts.timeScale = prevTimeScale;
		encoder->lastPts = encoder->au.pts;
		au->numOfNal = encoder->au.numOfNal;
		au->coprocessorFlags[0] = encoder->au.coprocessorFlags[0];
		au->reserved = 0;
		au->coprocessorFlags[1] = encoder->au.coprocessorFlags[1];
		encoder->numOfEncoded++;
		for (i = 0; i < encoder->au.numOfNal; i++)
			encoder->au.nal[i].address += (uintptr_t)au->pEs - encoder->au.streamAddress;
	}
	pArrayAu->numOfOutput = numOfOutput;
unlockRanges:
	videocodecUnlockRanges(&ranges);
	sceKernelUnlockFastMutex(&encoder->mutex);
copyOut:
	if (pArrayAu->numOfElm == 1 && userAu != NULL) {
		copied = videocodecCopyOutPictures(1, (void *const *)userAus,
			(void *const *)kernelAu, sizeof(AvcencAu));
		pArrayAu->pAu = userAu;
	}
	if (ret < 0)
		copied = ret;
out:
	sceSyscallLeave(syscall);
	return copied;

unlockAll:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto copyOut;
}

/*
 * Stops the AVC encoder pCtrl names: the last AU is delivered into the one AU pArrayAu names
 * as A0E63278 does, and the time stamps restart at the next encode.
 */
int SceAvcodecForDriver_DE515BFF(const AvcencControl *pCtrl, AvcencArrayAu *pArrayAu)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global;
	VideocodecEncoder *encoder;
	AvcencAu **userAu = NULL, **kernelAu = NULL;
	AvcencAu *userAus[1], *aus[1], *au;
	VideocodecLockedRange lockedRange[1];
	VideocodecLockedRanges ranges;
	uintptr_t physicalAddress;
	unsigned int handle, index, numOfOutput, prevNumUnitsInTick, prevTimeScale, timeScale;
	unsigned int i;
	SceUInt64 decodeTime, presentationTime;
	SceInt64 delta;
	char auBuf[sizeof(AvcencAu)];
	int ret, copied = 0;

	if (pArrayAu == NULL) {
		_GetVideocodecGlobal(0);
		copied = VIDEOCODEC_AVCENC_ERROR_INVALID_POINTER;
		goto out;
	}
	if (pArrayAu->numOfElm == 1) {
		userAu = pArrayAu->pAu;
		if (userAu != NULL) {
			copied = sceKernelCopyFromUser(userAus, userAu, sizeof(userAus));
			if (copied < 0 || (copied = videocodecCopyInPictures(pArrayAu->numOfElm,
					(void **)aus, auBuf, (void *const *)userAus,
					sizeof(AvcencAu))) < 0) {
				pArrayAu->pAu = userAu;
				goto out;
			}
			pArrayAu->pAu = aus;
			kernelAu = aus;
		}
	}
	global = _GetVideocodecGlobal(0);
	if (pCtrl == NULL || pArrayAu->pAu[0] == NULL) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_POINTER;
		goto copyOut;
	}
	if (pArrayAu->numOfElm != 1 || (handle = pCtrl->handle) == 0) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
		goto copyOut;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto copyOut;
	if (handle < 4096 || (index = handle - 4096) >= global->dsp0EncoderInfo.numOfStreams) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
		goto unlockAll;
	}
	encoder = &global->encoders[index];
	ret = lockFastMutex(&encoder->mutex);
	if (ret < 0)
		goto unlockAll;
	handle = encoder->handle;
	if (handle == 0) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
		sceKernelUnlockFastMutex(&encoder->mutex);
		goto unlockAll;
	}
	sceKernelUnlockFastMutex(global->pAllMutex);
	videocodecInitLockedRanges(&ranges, 0, lockedRange, 1,
		VIDEOCODEC_ENCODER_ERROR_BASE);
	au = pArrayAu->pAu[0];
	if (au->size != sizeof(*au)) {
		ret = VIDEOCODEC_ENCODER_ERROR_INVALID_ARGUMENT_SIZE;
		goto unlockRanges;
	}
	ret = dsp0VideocodecEncodeStop(handle, &encoder->au, &numOfOutput);
	ret = videocodecMapAvcencErrorInline(ret);
	if (ret < 0)
		goto unlockRanges;
	prevNumUnitsInTick = encoder->pendingNumUnitsInTick[0];
	encoder->pendingNumUnitsInTick[0] = encoder->pendingNumUnitsInTick[1];
	encoder->pendingNumUnitsInTick[1] = encoder->numUnitsInTick;
	prevTimeScale = encoder->pendingTimeScale[0];
	encoder->pendingTimeScale[0] = encoder->pendingTimeScale[1];
	encoder->pendingTimeScale[1] = encoder->timeScale;
	if (numOfOutput != 0) {
		ret = videocodecLockRange(&ranges, &physicalAddress, (uintptr_t)au->pEs,
			encoder->au.streamSize, 16, 0,
			VIDEOCODEC_LOCK_CACHED | VIDEOCODEC_LOCK_SCATTERED);
		if (ret < 0)
			goto unlockRanges;
		ret = SceCodecEngineWrapperForDriver_FAF590A6(au->pEs,
			(const void *)encoder->au.streamAddress, encoder->au.streamSize);
		if (ret < 0)
			goto unlockRanges;
		timeScale = encoder->timeStampTimeScale;
		au->esSize = encoder->au.streamSize;
		delta = (SceInt64)((SceUInt64)(2 * prevNumUnitsInTick) * timeScale) / prevTimeScale;
		decodeTime = encoder->decodeTime + delta;
		presentationTime = encoder->presentationTime + delta;
		encoder->decodeTime = decodeTime;
		encoder->presentationTime = presentationTime;
		au->dts.integer = decodeTime / timeScale;
		au->dts.fraction = decodeTime % timeScale * prevTimeScale / timeScale;
		au->dts.timeScale = prevTimeScale;
		au->pts.integer = presentationTime / timeScale;
		au->pts.fraction = presentationTime % timeScale * prevTimeScale / timeScale;
		au->pts.timeScale = prevTimeScale;
		ret = sceKernelVAtoPA((const void *)encoder->au.streamAddress, &physicalAddress);
		encoder->lastPts.upper = encoder->au.pts.upper + 2;
		encoder->lastPts.lower = encoder->au.pts.lower + 2;
		au->numOfNal = encoder->au.numOfNal;
		au->coprocessorFlags[0] = encoder->au.coprocessorFlags[0];
		au->reserved = 0;
		au->coprocessorFlags[1] = encoder->au.coprocessorFlags[1];
		encoder->numOfEncoded++;
		for (i = 0; i < encoder->au.numOfNal; i++)
			encoder->au.nal[i].address += (uintptr_t)au->pEs - encoder->au.streamAddress;
	}
	pArrayAu->numOfOutput = numOfOutput;
unlockRanges:
	encoder->encodeFlags |= VIDEOCODEC_ENCODER_NEW_PARAMETERS;
	videocodecUnlockRanges(&ranges);
	sceKernelUnlockFastMutex(&encoder->mutex);
copyOut:
	if (pArrayAu->numOfElm == 1 && userAu != NULL) {
		copied = videocodecCopyOutPictures(1, (void *const *)userAus,
			(void *const *)kernelAu, sizeof(AvcencAu));
		pArrayAu->pAu = userAu;
	}
	if (ret < 0)
		copied = ret;
out:
	sceSyscallLeave(syscall);
	return copied;

unlockAll:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto copyOut;
}

/* Flushes the AVC encoder pCtrl names; the time stamps restart at the next encode. */
int SceAvcodecForDriver_229AB2CF(const AvcencControl *pCtrl)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	VideocodecEncoder *encoder;
	unsigned int handle, index, numOfOutput;
	int ret;

	if (pCtrl == NULL) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_POINTER;
		goto out;
	}
	handle = pCtrl->handle;
	if (handle == 0) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
		goto out;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto out;
	if (handle < 4096 || (index = handle - 4096) >= global->dsp0EncoderInfo.numOfStreams) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
		goto unlockAll;
	}
	encoder = &global->encoders[index];
	ret = lockFastMutex(&encoder->mutex);
	if (ret < 0)
		goto unlockAll;
	handle = encoder->handle;
	if (handle == 0) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
		sceKernelUnlockFastMutex(&encoder->mutex);
		goto unlockAll;
	}
	sceKernelUnlockFastMutex(global->pAllMutex);
	ret = dsp0VideocodecEncodeStop(handle, &encoder->au, &numOfOutput);
	ret = videocodecMapAvcencErrorInline(ret);
	if (ret >= 0) {
		encoder->pendingNumUnitsInTick[0] = encoder->pendingNumUnitsInTick[1];
		encoder->pendingNumUnitsInTick[1] = encoder->numUnitsInTick;
		encoder->pendingTimeScale[0] = encoder->pendingTimeScale[1];
		encoder->pendingTimeScale[1] = encoder->timeScale;
		if (numOfOutput != 0) {
			encoder->lastPts.upper = encoder->au.pts.upper + 2;
			encoder->lastPts.lower = encoder->au.pts.lower + 2;
		}
	}
	encoder->encodeFlags |= VIDEOCODEC_ENCODER_NEW_PARAMETERS;
	sceKernelUnlockFastMutex(&encoder->mutex);
out:
	ret = videocodecConvertAvcencError(ret);
	sceSyscallLeave(syscall);
	return ret;

unlockAll:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto out;
}

/*
 * Changes the bit rates, CPB size, GOP length and frame rate of the AVC encoder pCtrl names;
 * a zero field keeps the current value. Checked against the encoder's level as at create.
 */
int SceAvcodecForDriver_E0232BFE(const AvcencControl *pCtrl, const AvcencEncoderParameter *pParam)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	VideocodecEncoder *encoder;
	const AvcencLevelLimit *limit;
	unsigned int handle, index, level, mbs;
	AvcencEncoderParameter param;
	int ret;

	if (pParam == NULL || pCtrl == NULL) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_POINTER;
		goto out;
	}
	handle = pCtrl->handle;
	if (handle == 0) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
		goto out;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto out;
	if (handle < 4096 || (index = handle - 4096) >= global->dsp0EncoderInfo.numOfStreams) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
		goto unlockAll;
	}
	encoder = &global->encoders[index];
	ret = lockFastMutex(&encoder->mutex);
	if (ret < 0)
		goto unlockAll;
	if (encoder->handle == 0) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
		sceKernelUnlockFastMutex(&encoder->mutex);
		goto unlockAll;
	}
	sceKernelUnlockFastMutex(global->pAllMutex);
	param = *pParam;
	if (param.bitRate[0] == 0)
		param.bitRate[0] = encoder->bitRate[0];
	if (param.bitRate[1] == 0)
		param.bitRate[1] = encoder->bitRate[1];
	if (param.cpbSize == 0)
		param.cpbSize = encoder->cpbSize >> 3;
	if (param.gopLength == 0)
		param.gopLength = encoder->gopLength;
	if (param.numUnitsInTick == 0)
		param.numUnitsInTick = encoder->numUnitsInTick;
	if (param.timeScale == 0)
		param.timeScale = encoder->timeScale;
	if (encoder->frameRateGop == 1)
		param.gopLength = param.timeScale / param.numUnitsInTick;
	level = encoder->level;
	if (level <= 13) {
		limit = &avcencLevelLimits[0];
	} else {
		if (level > 41)
			goto invalid;
		if ((unsigned char)(level - 20) > 21)
			goto invalid;
		limit = avcencLevelLimitTable[(unsigned char)(level - 20)];
		if (limit == NULL)
			goto invalid;
	}
	if (param.numUnitsInTick > 120000 || param.timeScale < 2 * param.numUnitsInTick ||
	    param.timeScale > 60 * param.numUnitsInTick) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_FRAME_RATE;
		goto unlock;
	}
	mbs = ((encoder->alignedHorizontal + 15) >> 4) * ((encoder->alignedVertical + 15) >> 4);
	if (limit->maxMbps <
		    (unsigned int)((unsigned long long)param.timeScale * mbs / param.numUnitsInTick / 2) ||
	    mbs > limit->maxFs ||
	    limit->maxBr < param.bitRate[1] || limit->maxBr < param.bitRate[0] ||
	    param.bitRate[1] < 32000 || param.bitRate[0] < 32000 ||
	    param.cpbSize > limit->maxFs * 768 / 4)
		goto invalid;
	if (param.gopLength > 300 ||
	    param.numUnitsInTick * 2 * param.gopLength > 10 * param.timeScale) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_GOP_LENGTH;
		goto unlock;
	}
	ret = 0;
	encoder->bitRate[0] = param.bitRate[0];
	encoder->bitRate[1] = param.bitRate[1];
	encoder->numUnitsInTick = param.numUnitsInTick;
	encoder->timeScale = param.timeScale;
	encoder->cpbSize = param.cpbSize * 8;
	encoder->gopLength = param.gopLength;
	goto unlock;
invalid:
	ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
unlock:
	sceKernelUnlockFastMutex(&encoder->mutex);
out:
	sceSyscallLeave(syscall);
	return ret;

unlockAll:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto out;
}

/* Copies NAL unit index of the AVC encoder's last AU to pNalUnit. */
int SceAvcodecForDriver_1A584DB7(const AvcencControl *pCtrl, unsigned int index,
	AvcencNalUnit *pNalUnit)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	VideocodecEncoder *encoder;
	const Dsp0VideocodecNalUnit *nal;
	unsigned int handle, slot;
	int ret;

	if (pNalUnit == NULL || pCtrl == NULL) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_POINTER;
		goto out;
	}
	handle = pCtrl->handle;
	if (handle == 0) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
		goto out;
	}
	ret = lockFastMutex(global->pAllMutex);
	if (ret < 0)
		goto out;
	if (handle < 4096 || (slot = handle - 4096) >= global->dsp0EncoderInfo.numOfStreams) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
		goto unlockAll;
	}
	encoder = &global->encoders[slot];
	ret = lockFastMutex(&encoder->mutex);
	if (ret < 0)
		goto unlockAll;
	if (encoder->handle == 0) {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
		sceKernelUnlockFastMutex(&encoder->mutex);
		goto unlockAll;
	}
	sceKernelUnlockFastMutex(global->pAllMutex);
	if (index < encoder->au.numOfNal && encoder->numOfEncoded != 0) {
		nal = &encoder->au.nal[index];
		pNalUnit->data = (void *)nal->address;
		pNalUnit->size = nal->size;
		pNalUnit->type = nal->type;
		pNalUnit->referencePriority = nal->referencePriority;
	} else {
		ret = VIDEOCODEC_AVCENC_ERROR_INVALID_PARAM;
	}
	sceKernelUnlockFastMutex(&encoder->mutex);
out:
	sceSyscallLeave(syscall);
	return ret;

unlockAll:
	sceKernelUnlockFastMutex(global->pAllMutex);
	goto out;
}

int SceAvcodecForDriver_4F11F915(AvcencCscFrame *pDst, const AvcencCscFrame *pSrc)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global = _GetVideocodecGlobal(0);
	int ret;

	if (pSrc == NULL || pDst == NULL) {
		ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
	} else {
		ret = lockFastMutex(global->pAllMutex);
		if (ret >= 0) {
			ret = videocodecAvcencCsc(pDst, pSrc);
			sceKernelUnlockFastMutex(global->pAllMutex);
		}
	}
	ret = videocodecConvertAvcencError(ret);
	sceSyscallLeave(syscall);
	return ret;
}

int _sceAvcdecSetDecodeModeInternal(unsigned int codecType, unsigned int mode)
{
	return SceAvcodecForDriver_866B49BF(codecType, mode);
}

int _sceVideodecSetConfigInternal(unsigned int codecType, unsigned int flags)
{
	return SceAvcodecForDriver_43985AA8(codecType, flags);
}

int _sceAvcdecDecodeSetUserDataSei1FieldMemSizeNongameapp(unsigned int codecType,
						     unsigned int size)
{
	return SceAvcodecForDriver_A2085B6B(codecType, size);
}

int _sceVideodecSetConfig(unsigned int codecType, unsigned int flags)
{
	return SceAvcodecForDriver_12DAE04A(codecType, flags);
}

int _sceAvcdecQueryDecoderMemSizeInternal(unsigned int codecType,
		const SceAvcdecQueryDecoderInfo *pQuery,
		SceAvcdecDecoderInfo *pDecoderInfo)
{
	SceAvcdecQueryDecoderInfo *query;
	SceAvcdecDecoderInfo *decoderInfo;
	int ret, copied;

	if (pQuery != NULL) {
		query = __builtin_alloca(sizeof(*query));
		ret = sceKernelCopyFromUser(query, pQuery, sizeof(*query));
		if (ret < 0)
			return ret;
		pQuery = query;
	}
	if (pDecoderInfo == NULL)
		return SceAvcodecForDriver_D5A34236(codecType, pQuery, pDecoderInfo);
	decoderInfo = __builtin_alloca(sizeof(*decoderInfo));
	*decoderInfo = (SceAvcdecDecoderInfo){0};
	ret = SceAvcodecForDriver_D5A34236(codecType, pQuery, decoderInfo);
	copied = sceKernelCopyToUser(pDecoderInfo, decoderInfo,
				    sizeof(*decoderInfo));
	return copied < 0 ? copied : ret;
}

int _sceAvcdecCreateDecoderInternal(unsigned int codecType,
		SceAvcdecCtrl *pCtrl, const SceAvcdecQueryDecoderInfo *pDecoderInfo)
{
	SceAvcdecCtrl *ctrl = NULL;
	SceAvcdecQueryDecoderInfo *decoderInfo;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		*ctrl = (SceAvcdecCtrl){0};
		ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (ret < 0)
			return ret;
	}
	if (pDecoderInfo != NULL) {
		decoderInfo = __builtin_alloca(sizeof(*decoderInfo));
		ret = sceKernelCopyFromUser(decoderInfo, pDecoderInfo,
					   sizeof(*decoderInfo));
		if (ret < 0)
			return ret;
		pDecoderInfo = decoderInfo;
	}
	ret = SceAvcodecForDriver_663B3195(codecType, ctrl, pDecoderInfo);
	if (pCtrl != NULL) {
		copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceAvcdecDecodeAuInternal(SceAvcdecCtrl *pCtrl,
		const SceAvcdecAu *pAu, const SceUInt *timeout)
{
	SceAvcdecCtrl *ctrl = NULL;
	SceAvcdecAu *au = NULL;
	SceUInt *wait = NULL;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		*ctrl = (SceAvcdecCtrl){0};
		copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	if (pAu != NULL) {
		au = __builtin_alloca(sizeof(*au));
		copied = sceKernelCopyFromUser(au, pAu, sizeof(*au));
		if (copied < 0)
			return copied;
	}
	if (timeout != NULL) {
		wait = __builtin_alloca(sizeof(*wait));
		copied = sceKernelCopyFromUser(wait, timeout, sizeof(*wait));
		if (copied < 0)
			return copied;
	}
	ret = SceAvcodecForDriver_D2ACCE22(ctrl, au, wait);
	if (pCtrl != NULL) {
		copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceAvcdecDecodeGetPictureInternal(SceAvcdecCtrl *pCtrl,
		SceAvcdecArrayPicture *pArrayPicture, const SceUInt *timeout)
{
	SceAvcdecCtrl *ctrl;
	SceAvcdecArrayPicture *arrayPicture;
	SceUInt *wait = NULL;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		*ctrl = (SceAvcdecCtrl){0};
		copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
		if (pArrayPicture != NULL)
			goto copyArrayPicture;
	} else {
		ctrl = NULL;
		if (pArrayPicture != NULL)
			goto copyArrayPicture;
	}
	arrayPicture = pArrayPicture;
	goto copyTimeout;

copyArrayPicture:
	arrayPicture = __builtin_alloca(sizeof(*arrayPicture));
	*arrayPicture = (SceAvcdecArrayPicture){0};
	copied = sceKernelCopyFromUser(arrayPicture, pArrayPicture,
				      sizeof(*arrayPicture));
	if (copied < 0)
		return copied;
copyTimeout:
	if (timeout != NULL) {
		wait = __builtin_alloca(sizeof(*wait));
		copied = sceKernelCopyFromUser(wait, timeout, sizeof(*wait));
		if (copied < 0)
			return copied;
	}
	ret = SceAvcodecForDriver_D7CD7F82(ctrl, arrayPicture, wait);
	if (pCtrl != NULL) {
		copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	if (pArrayPicture != NULL) {
		copied = sceKernelCopyToUser(pArrayPicture, arrayPicture,
					    sizeof(*arrayPicture));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceAvcdecDecodeGetPictureWithWorkPictureInternal(SceAvcdecCtrl *pCtrl,
		SceAvcdecArrayPicture *pArrayPicture,
		SceAvcdecArrayPicture *pWorkPicture, const SceUInt *timeout)
{
	SceAvcdecCtrl *ctrl = NULL;
	SceAvcdecArrayPicture *arrayPicture = NULL;
	SceAvcdecArrayPicture *workPicture = NULL;
	const SceUInt *wait = timeout;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		*ctrl = (SceAvcdecCtrl){0};
		copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	if (pArrayPicture != NULL) {
		arrayPicture = __builtin_alloca(sizeof(*arrayPicture));
		*arrayPicture = (SceAvcdecArrayPicture){0};
		copied = sceKernelCopyFromUser(arrayPicture, pArrayPicture,
					      sizeof(*arrayPicture));
		if (copied < 0)
			return copied;
	}
	if (pWorkPicture != NULL) {
		workPicture = __builtin_alloca(sizeof(*workPicture));
		*workPicture = (SceAvcdecArrayPicture){0};
		copied = sceKernelCopyFromUser(workPicture, pWorkPicture,
					      sizeof(*workPicture));
		if (copied < 0)
			return copied;
	}
	if (timeout != NULL) {
		SceUInt *timeoutCopy = __builtin_alloca(sizeof(*timeoutCopy));

		copied = sceKernelCopyFromUser(timeoutCopy, timeout,
					      sizeof(*timeoutCopy));
		wait = timeoutCopy;
		if (copied < 0)
			return copied;
	}
	ret = SceAvcodecForDriver_6E250A5F(ctrl, arrayPicture, workPicture, wait);
	if (pCtrl != NULL) {
		copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	if (pArrayPicture != NULL) {
		copied = sceKernelCopyToUser(pArrayPicture, arrayPicture,
					    sizeof(*arrayPicture));
		if (copied < 0)
			return copied;
	}
	if (pWorkPicture != NULL) {
		copied = sceKernelCopyToUser(pWorkPicture, workPicture,
					    sizeof(*workPicture));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceAvcdecDecodeAuNalAuInternal(SceAvcdecCtrl *pCtrl,
		const VeneziaVipVideoInput *pAu, const SceUInt *timeout)
{
	SceAvcdecCtrl *ctrl = NULL;
	VeneziaVipVideoInput *au = NULL;
	SceUInt *wait = NULL;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		*ctrl = (SceAvcdecCtrl){0};
		copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	if (pAu != NULL) {
		au = __builtin_alloca(sizeof(*au));
		copied = sceKernelCopyFromUser(au, pAu, sizeof(*au));
		if (copied < 0)
			return copied;
	}
	if (timeout != NULL) {
		wait = __builtin_alloca(sizeof(*wait));
		copied = sceKernelCopyFromUser(wait, timeout, sizeof(*wait));
		if (copied < 0)
			return copied;
	}
	ret = SceAvcodecForDriver_F714DB9A(ctrl, au, wait);
	if (pCtrl != NULL) {
		copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceAvcdecRegisterCallbackInternal(SceAvcdecCtrl *pCtrl, SceUID cbid,
				     unsigned int eventMask)
{
	SceAvcdecCtrl *ctrl;
	int ret, copied;

	if (pCtrl == NULL)
		return SceAvcodecForDriver_90F394E6(pCtrl, cbid, eventMask);
	ctrl = __builtin_alloca(sizeof(*ctrl));
	*ctrl = (SceAvcdecCtrl){0};
	copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
	if (copied < 0)
		return copied;
	ret = SceAvcodecForDriver_90F394E6(ctrl, cbid, eventMask);
	copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
	return copied < 0 ? copied : ret;
}

int _sceAvcdecUnregisterCallbackInternal(SceAvcdecCtrl *pCtrl)
{
	SceAvcdecCtrl *ctrl;
	int ret, copied;

	if (pCtrl == NULL)
		return SceAvcodecForDriver_27211864(pCtrl);
	ctrl = __builtin_alloca(sizeof(*ctrl));
	*ctrl = (SceAvcdecCtrl){0};
	copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
	if (copied < 0)
		return copied;
	ret = SceAvcodecForDriver_27211864(ctrl);
	copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
	return copied < 0 ? copied : ret;
}

int _sceAvcdecUnregisterCallbackWithCbidInternal(SceAvcdecCtrl *pCtrl,
					      SceUID cbid)
{
	SceAvcdecCtrl *ctrl;
	int ret, copied;

	if (pCtrl == NULL)
		return SceAvcodecForDriver_D9FCDF3E(pCtrl, cbid);
	ctrl = __builtin_alloca(sizeof(*ctrl));
	*ctrl = (SceAvcdecCtrl){0};
	copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
	if (copied < 0)
		return copied;
	ret = SceAvcodecForDriver_D9FCDF3E(ctrl, cbid);
	copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
	return copied < 0 ? copied : ret;
}

int _sceAvcdecGetSeiPictureTimingInternal(SceAvcdecCtrl *pCtrl,
		unsigned int field, VideocodecSeiPictureTiming *pPictureTiming)
{
	SceAvcdecCtrl *ctrl = NULL;
	VideocodecSeiPictureTiming *pictureTiming = NULL;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		*ctrl = (SceAvcdecCtrl){0};
		copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	if (pPictureTiming != NULL) {
		pictureTiming = __builtin_alloca(sizeof(*pictureTiming));
		memset(pictureTiming, 0, sizeof(*pictureTiming));
	}
	ret = SceAvcodecForDriver_BC244D21(ctrl, field, pictureTiming);
	if (pCtrl != NULL) {
		copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	if (pPictureTiming != NULL) {
		copied = sceKernelCopyToUser(pPictureTiming, pictureTiming,
					    sizeof(*pictureTiming));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceVideodecInitLibraryInternal(unsigned int codecType,
		const SceVideodecQueryInitInfo *pInitInfo)
{
	SceVideodecQueryInitInfo *initInfo;
	int ret;

	if (pInitInfo != NULL) {
		initInfo = __builtin_alloca(sizeof(*initInfo));
		ret = sceKernelCopyFromUser(initInfo, pInitInfo, sizeof(*initInfo));
		if (ret < 0)
			return ret;
		pInitInfo = initInfo;
	}
	return SceAvcodecForDriver_4AE6B437(codecType, pInitInfo);
}

int _sceVideodecQueryMemSizeInternal(unsigned int codecType,
		const SceVideodecQueryInitInfo *pInitInfo,
		SceVideodecMemInfo *pMemInfo)
{
	SceVideodecQueryInitInfo *initInfo;
	SceVideodecMemInfo *memInfo;
	int ret, copied;

	if (pInitInfo != NULL) {
		initInfo = __builtin_alloca(sizeof(*initInfo));
		ret = sceKernelCopyFromUser(initInfo, pInitInfo, sizeof(*initInfo));
		if (ret < 0)
			return ret;
		pInitInfo = initInfo;
	}
	if (pMemInfo == NULL)
		return SceAvcodecForDriver_22D7B653(codecType, pInitInfo, pMemInfo);
	memInfo = __builtin_alloca(sizeof(*memInfo));
	*memInfo = (SceVideodecMemInfo){0};
	ret = SceAvcodecForDriver_22D7B653(codecType, pInitInfo, memInfo);
	copied = sceKernelCopyToUser(pMemInfo, memInfo, sizeof(*memInfo));
	return copied < 0 ? copied : ret;
}

int _sceAvcdecDecodeSetTrickModeNongameapp(SceAvcdecCtrl *pCtrl, int trickMode)
{
	SceAvcdecCtrl *ctrl;
	int ret, copied;

	if (pCtrl == NULL)
		return SceAvcodecForDriver_C518A109(pCtrl, trickMode);
	ctrl = __builtin_alloca(sizeof(*ctrl));
	*ctrl = (SceAvcdecCtrl){0};
	copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
	if (copied < 0)
		return copied;
	ret = SceAvcodecForDriver_C518A109(ctrl, trickMode);
	copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
	return copied < 0 ? copied : ret;
}

int _sceAvcdecQueryDecoderMemSizeNongameapp(unsigned int codecType,
		const SceAvcdecQueryDecoderInfo *pQuery,
		SceAvcdecDecoderInfo *pDecoderInfo)
{
	SceAvcdecQueryDecoderInfo *query;
	SceAvcdecDecoderInfo *decoderInfo;
	int ret, copied;

	if (pQuery != NULL) {
		query = __builtin_alloca(sizeof(*query));
		ret = sceKernelCopyFromUser(query, pQuery, sizeof(*query));
		if (ret < 0)
			return ret;
		pQuery = query;
	}
	if (pDecoderInfo == NULL)
		return SceAvcodecForDriver_01387A93(codecType, pQuery, pDecoderInfo);
	decoderInfo = __builtin_alloca(sizeof(*decoderInfo));
	*decoderInfo = (SceAvcdecDecoderInfo){0};
	ret = SceAvcodecForDriver_01387A93(codecType, pQuery, decoderInfo);
	copied = sceKernelCopyToUser(pDecoderInfo, decoderInfo,
				    sizeof(*decoderInfo));
	return copied < 0 ? copied : ret;
}

int _sceAvcdecCreateDecoderNongameapp(unsigned int codecType,
		SceAvcdecCtrl *pCtrl, const SceAvcdecQueryDecoderInfo *pDecoderInfo)
{
	SceAvcdecCtrl *ctrl = NULL;
	SceAvcdecQueryDecoderInfo *decoderInfo;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		*ctrl = (SceAvcdecCtrl){0};
		ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (ret < 0)
			return ret;
	}
	if (pDecoderInfo != NULL) {
		decoderInfo = __builtin_alloca(sizeof(*decoderInfo));
		ret = sceKernelCopyFromUser(decoderInfo, pDecoderInfo,
					   sizeof(*decoderInfo));
		if (ret < 0)
			return ret;
		pDecoderInfo = decoderInfo;
	}
	ret = SceAvcodecForDriver_301CCA25(codecType, ctrl, pDecoderInfo);
	if (pCtrl != NULL) {
		copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceVideodecQueryInstanceNongameapp(unsigned int codecType,
		VideocodecInstanceInfo *pInfo)
{
	VideocodecInstanceInfo info;
	int ret, copied;

	if (pInfo == NULL)
		return SceAvcodecForDriver_F9E271DE(codecType, NULL);
	__builtin_memset(&info, 0, sizeof(info));
	ret = sceKernelCopyFromUser(&info, pInfo, sizeof(info));
	if (ret < 0)
		return ret;
	ret = SceAvcodecForDriver_F9E271DE(codecType, &info);
	copied = sceKernelCopyToUser(pInfo, &info, sizeof(info));
	return copied < 0 ? copied : ret;
}

int _sceVideodecQueryMemSizeNongameapp(unsigned int codecType,
		const SceVideodecQueryInitInfo *pInitInfo,
		SceVideodecMemInfo *pMemInfo)
{
	SceVideodecQueryInitInfo *initInfo;
	SceVideodecMemInfo *memInfo;
	int ret, copied;

	if (pInitInfo != NULL) {
		initInfo = __builtin_alloca(sizeof(*initInfo));
		ret = sceKernelCopyFromUser(initInfo, pInitInfo, sizeof(*initInfo));
		if (ret < 0)
			return ret;
		pInitInfo = initInfo;
	}
	if (pMemInfo == NULL)
		return SceAvcodecForDriver_FACCD284(codecType, pInitInfo, pMemInfo);
	memInfo = __builtin_alloca(sizeof(*memInfo));
	*memInfo = (SceVideodecMemInfo){0};
	ret = SceAvcodecForDriver_FACCD284(codecType, pInitInfo, memInfo);
	copied = sceKernelCopyToUser(pMemInfo, memInfo, sizeof(*memInfo));
	return copied < 0 ? copied : ret;
}

int _sceVideodecInitLibraryWithUnmapMemNongameapp(unsigned int codecType,
		SceVideodecCtrl *pCtrl, const SceVideodecQueryInitInfo *pInitInfo)
{
	SceVideodecCtrl *ctrl = NULL;
	const SceVideodecQueryInitInfo *initInfo = pInitInfo;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		__builtin_memset(ctrl, 0, sizeof(*ctrl));
		copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	if (pInitInfo != NULL) {
		SceVideodecQueryInitInfo *initCopy = __builtin_alloca(sizeof(*initCopy));

		copied = sceKernelCopyFromUser(initCopy, pInitInfo, sizeof(*initCopy));
		initInfo = initCopy;
		if (copied < 0)
			return copied;
	}
	ret = SceAvcodecForDriver_6DC39EDC(codecType, ctrl, initInfo);
	if (pCtrl != NULL) {
		copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceVideodecInitLibraryNongameapp(unsigned int codecType,
		const SceVideodecQueryInitInfo *pInitInfo)
{
	SceVideodecQueryInitInfo *initInfo;
	int ret;

	if (pInitInfo != NULL) {
		initInfo = __builtin_alloca(sizeof(*initInfo));
		ret = sceKernelCopyFromUser(initInfo, pInitInfo, sizeof(*initInfo));
		if (ret < 0)
			return ret;
		pInitInfo = initInfo;
	}
	return SceAvcodecForDriver_4DD3826D(codecType, pInitInfo);
}

int _sceAvcdecGetSeiUserDataNongameapp(SceAvcdecCtrl *pCtrl, unsigned int field,
		VideocodecSeiUserData *pUserData, SceAvcdecBuf *pBuf)
{
	SceAvcdecCtrl *ctrl = NULL;
	VideocodecSeiUserData *userData = NULL;
	SceAvcdecBuf *buf = NULL;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		*ctrl = (SceAvcdecCtrl){0};
		copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	if (pUserData != NULL) {
		userData = __builtin_alloca(sizeof(*userData));
		memset(userData, 0, sizeof(*userData));
		copied = sceKernelCopyFromUser(userData, pUserData,
					      sizeof(*userData));
		if (copied < 0)
			return copied;
	}
	if (pBuf != NULL) {
		buf = __builtin_alloca(sizeof(*buf));
		*buf = (SceAvcdecBuf){0};
		copied = sceKernelCopyFromUser(buf, pBuf, sizeof(*buf));
		if (copied < 0)
			return copied;
	}
	ret = SceAvcodecForDriver_427914D5(ctrl, field, userData, buf);
	if (pCtrl != NULL) {
		copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	if (pUserData != NULL) {
		copied = sceKernelCopyToUser(pUserData, userData, sizeof(*userData));
		if (copied < 0)
			return copied;
	}
	if (pBuf != NULL) {
		copied = sceKernelCopyToUser(pBuf, buf, sizeof(*buf));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceVideodecQueryMemSize(unsigned int codecType,
		const SceVideodecQueryInitInfo *pInitInfo,
		SceVideodecMemInfo *pMemInfo)
{
	SceVideodecQueryInitInfo *initInfo;
	SceVideodecMemInfo *memInfo;
	int ret, copied;

	if (pInitInfo != NULL) {
		initInfo = __builtin_alloca(sizeof(*initInfo));
		ret = sceKernelCopyFromUser(initInfo, pInitInfo, sizeof(*initInfo));
		if (ret < 0)
			return ret;
		pInitInfo = initInfo;
	}
	if (pMemInfo == NULL)
		return SceAvcodecForDriver_536C8296(codecType, pInitInfo, pMemInfo);
	memInfo = __builtin_alloca(sizeof(*memInfo));
	*memInfo = (SceVideodecMemInfo){0};
	ret = SceAvcodecForDriver_536C8296(codecType, pInitInfo, memInfo);
	copied = sceKernelCopyToUser(pMemInfo, memInfo, sizeof(*memInfo));
	return copied < 0 ? copied : ret;
}

int _sceVideodecInitLibraryWithUnmapMem(unsigned int codecType,
		SceVideodecCtrl *pCtrl, const SceVideodecQueryInitInfo *pInitInfo)
{
	SceVideodecCtrl *ctrl = NULL;
	SceVideodecQueryInitInfo *initInfo;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		__builtin_memset(ctrl, 0, sizeof(*ctrl));
		copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	if (pInitInfo != NULL) {
		initInfo = __builtin_alloca(sizeof(*initInfo));
		copied = sceKernelCopyFromUser(initInfo, pInitInfo,
					      sizeof(*initInfo));
		if (copied < 0)
			return copied;
		pInitInfo = initInfo;
	}
	ret = SceAvcodecForDriver_C93DA0C7(codecType, ctrl, pInitInfo);
	if (pCtrl != NULL) {
		copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceVideodecInitLibraryWithUnmapMemInternal(unsigned int codecType,
		SceVideodecCtrl *pCtrl, const SceVideodecQueryInitInfo *pInitInfo)
{
	SceVideodecCtrl *ctrl = NULL;
	SceVideodecQueryInitInfo *initInfo;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		__builtin_memset(ctrl, 0, sizeof(*ctrl));
		copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	if (pInitInfo != NULL) {
		initInfo = __builtin_alloca(sizeof(*initInfo));
		copied = sceKernelCopyFromUser(initInfo, pInitInfo,
					      sizeof(*initInfo));
		if (copied < 0)
			return copied;
		pInitInfo = initInfo;
	}
	ret = SceAvcodecForDriver_BC55D10F(codecType, ctrl, pInitInfo);
	if (pCtrl != NULL) {
		copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceVideodecInitLibrary(unsigned int codecType,
		const SceVideodecQueryInitInfo *pInitInfo)
{
	SceVideodecQueryInitInfo *initInfo;
	int ret;

	if (pInitInfo != NULL) {
		initInfo = __builtin_alloca(sizeof(*initInfo));
		ret = sceKernelCopyFromUser(initInfo, pInitInfo, sizeof(*initInfo));
		if (ret < 0)
			return ret;
		pInitInfo = initInfo;
	}
	return SceAvcodecForDriver_95E8AFCA(codecType, pInitInfo);
}

int _sceVideodecTermLibrary(unsigned int codecType)
{
	return SceAvcodecForDriver_2641888C(codecType);
}

int _sceAvcdecQueryDecoderMemSize(unsigned int codecType,
		const SceAvcdecQueryDecoderInfo *pQuery,
		SceAvcdecDecoderInfo *pDecoderInfo)
{
	SceAvcdecQueryDecoderInfo *query;
	SceAvcdecDecoderInfo *decoderInfo;
	int ret, copied;

	if (pQuery != NULL) {
		query = __builtin_alloca(sizeof(*query));
		ret = sceKernelCopyFromUser(query, pQuery, sizeof(*query));
		if (ret < 0)
			return ret;
		pQuery = query;
	}
	if (pDecoderInfo == NULL)
		return SceAvcodecForDriver_C114C499(codecType, pQuery, pDecoderInfo);
	decoderInfo = __builtin_alloca(sizeof(*decoderInfo));
	*decoderInfo = (SceAvcdecDecoderInfo){0};
	ret = SceAvcodecForDriver_C114C499(codecType, pQuery, decoderInfo);
	copied = sceKernelCopyToUser(pDecoderInfo, decoderInfo,
				    sizeof(*decoderInfo));
	return copied < 0 ? copied : ret;
}

int _sceAvcdecSetInterlacedStreamMode(unsigned int codecType, unsigned int flag)
{
	return SceAvcodecForDriver_C8366BB8(codecType, flag);
}

int _sceAvcdecCreateDecoder(unsigned int codecType, SceAvcdecCtrl *pCtrl,
			   const SceAvcdecQueryDecoderInfo *pDecoderInfo)
{
	SceAvcdecCtrl *ctrl = NULL;
	SceAvcdecQueryDecoderInfo *decoderInfo;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		*ctrl = (SceAvcdecCtrl){ 0 };
		ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (ret < 0)
			return ret;
	}
	if (pDecoderInfo != NULL) {
		decoderInfo = __builtin_alloca(sizeof(*decoderInfo));
		ret = sceKernelCopyFromUser(decoderInfo, pDecoderInfo,
					   sizeof(*decoderInfo));
		if (ret < 0)
			return ret;
		pDecoderInfo = decoderInfo;
	}
	ret = SceAvcodecForDriver_7E8CC4E5(codecType, ctrl, pDecoderInfo);
	if (pCtrl != NULL) {
		copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceAvcdecDeleteDecoder(SceAvcdecCtrl *pCtrl)
{
	SceAvcdecCtrl *ctrl;
	int ret, copied;

	if (pCtrl == NULL)
		return SceAvcodecForDriver_EA07A3B7(NULL);
	ctrl = __builtin_alloca(sizeof(*ctrl));
	*ctrl = (SceAvcdecCtrl){ 0 };
	ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
	if (ret < 0)
		return ret;
	ret = SceAvcodecForDriver_EA07A3B7(ctrl);
	copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
	return copied < 0 ? copied : ret;
}

int _sceAvcdecDecodeAvailableSize(SceAvcdecCtrl *pCtrl)
{
	SceAvcdecCtrl *ctrl;
	int ret, copied;

	if (pCtrl == NULL)
		return SceAvcodecForDriver_43A9CD35(NULL);
	ctrl = __builtin_alloca(sizeof(*ctrl));
	*ctrl = (SceAvcdecCtrl){ 0 };
	ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
	if (ret < 0)
		return ret;
	ret = SceAvcodecForDriver_43A9CD35(ctrl);
	copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
	return copied < 0 ? copied : ret;
}

int _sceAvcdecDecode(const SceAvcdecCtrl *pCtrl, const SceAvcdecAu *pAu,
		    SceAvcdecArrayPicture *pArrayPicture)
{
	SceAvcdecCtrl *ctrl = NULL;
	SceAvcdecAu *au = NULL;
	SceAvcdecArrayPicture *arrayPicture;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (ret < 0)
			return ret;
	}
	if (pAu != NULL) {
		au = __builtin_alloca(sizeof(*au));
		ret = sceKernelCopyFromUser(au, pAu, sizeof(*au));
		if (ret < 0)
			return ret;
	}
	if (pArrayPicture == NULL)
		return SceAvcodecForDriver_48DECA56(ctrl, au, NULL);
	arrayPicture = __builtin_alloca(sizeof(*arrayPicture));
	*arrayPicture = (SceAvcdecArrayPicture){ 0 };
	ret = sceKernelCopyFromUser(arrayPicture, pArrayPicture,
				   sizeof(*arrayPicture));
	if (ret < 0)
		return ret;
	ret = SceAvcodecForDriver_48DECA56(ctrl, au, arrayPicture);
	copied = sceKernelCopyToUser(pArrayPicture, arrayPicture,
				     sizeof(*arrayPicture));
	return copied < 0 ? copied : ret;
}

int _sceAvcdecDecodeStop(const SceAvcdecCtrl *pCtrl,
			SceAvcdecArrayPicture *pArrayPicture)
{
	SceAvcdecArrayPicture *arrayPicture;
	int ret, copied;

	if (pCtrl != NULL) {
		SceAvcdecCtrl *ctrl = __builtin_alloca(sizeof(*ctrl));

		ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (ret < 0)
			return ret;
		pCtrl = ctrl;
		if (pArrayPicture != NULL)
			goto copyPicture;
		goto submit;
	}
	if (pArrayPicture != NULL)
		goto copyPicture;
submit:
	return SceAvcodecForDriver_DFC0AAC6(pCtrl, pArrayPicture);

copyPicture:
	arrayPicture = __builtin_alloca(sizeof(*arrayPicture));
	*arrayPicture = (SceAvcdecArrayPicture){ 0 };
	ret = sceKernelCopyFromUser(arrayPicture, pArrayPicture,
				   sizeof(*arrayPicture));
	if (ret < 0)
		return ret;
	ret = SceAvcodecForDriver_DFC0AAC6(pCtrl, arrayPicture);
	copied = sceKernelCopyToUser(pArrayPicture, arrayPicture,
				     sizeof(*arrayPicture));
	return copied < 0 ? copied : ret;
}

int _sceAvcdecDecodeFlush(const SceAvcdecCtrl *pCtrl)
{
	SceAvcdecCtrl *ctrl = NULL;
	int ret;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (ret < 0)
			return ret;
	}
	return SceAvcodecForDriver_D46B8463(ctrl);
}

int _sceAvcdecCsc(SceAvcdecPicture *pDst, const SceAvcdecPicture *pSrc)
{
	SceAvcdecPicture *dst = NULL;
	SceAvcdecPicture *src;
	int ret, copied;

	if (pDst != NULL) {
		dst = __builtin_alloca(sizeof(*dst));
		memset(dst, 0, sizeof(*dst));
		ret = sceKernelCopyFromUser(dst, pDst, sizeof(*dst));
		if (ret < 0)
			return ret;
	}
	if (pSrc != NULL) {
		src = __builtin_alloca(sizeof(*src));
		ret = sceKernelCopyFromUser(src, pSrc, sizeof(*src));
		if (ret < 0)
			return ret;
		pSrc = src;
	}
	ret = SceAvcodecForDriver_16B501C9(dst, pSrc);
	if (pDst != NULL) {
		copied = sceKernelCopyToUser(pDst, dst, sizeof(*dst));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceAvcdecCscInternal(VideocodecPicture *pDst, const VideocodecPicture *pSrc)
{
	VideocodecPicture *dst = NULL;
	VideocodecPicture *src;
	int ret, copied;

	if (pDst != NULL) {
		dst = __builtin_alloca(sizeof(*dst));
		memset(dst, 0, sizeof(*dst));
		ret = sceKernelCopyFromUser(dst, pDst, sizeof(*dst));
		if (ret < 0)
			return ret;
	}
	if (pSrc != NULL) {
		src = __builtin_alloca(sizeof(*src));
		ret = sceKernelCopyFromUser(src, pSrc, sizeof(*src));
		if (ret < 0)
			return ret;
		pSrc = src;
	}
	ret = SceAvcodecForDriver_B8A9632A(dst, pSrc);
	if (pDst != NULL) {
		copied = sceKernelCopyToUser(pDst, dst, sizeof(*dst));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceAvcdecSetDecodeMode(unsigned int codecType, unsigned int mode)
{
	return SceAvcodecForDriver_73401748(codecType, mode);
}

int _sceAvcdecDecodeWithWorkPicture(const SceAvcdecCtrl *pCtrl,
		const SceAvcdecAu *pAu, SceAvcdecArrayPicture *pArrayPicture,
		SceAvcdecArrayPicture *pWorkPicture)
{
	SceAvcdecCtrl *ctrl = NULL;
	SceAvcdecAu *au = NULL;
	SceAvcdecArrayPicture *arrayPicture = NULL;
	SceAvcdecArrayPicture *workPicture = NULL;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	if (pAu != NULL) {
		au = __builtin_alloca(sizeof(*au));
		copied = sceKernelCopyFromUser(au, pAu, sizeof(*au));
		if (copied < 0)
			return copied;
	}
	if (pArrayPicture != NULL) {
		arrayPicture = __builtin_alloca(sizeof(*arrayPicture));
		*arrayPicture = (SceAvcdecArrayPicture){0};
		copied = sceKernelCopyFromUser(arrayPicture, pArrayPicture,
					      sizeof(*arrayPicture));
		if (copied < 0)
			return copied;
	}
	if (pWorkPicture != NULL) {
		workPicture = __builtin_alloca(sizeof(*workPicture));
		*workPicture = (SceAvcdecArrayPicture){0};
		copied = sceKernelCopyFromUser(workPicture, pWorkPicture,
					      sizeof(*workPicture));
		if (copied < 0)
			return copied;
	}
	ret = SceAvcodecForDriver_EAEB16D5(ctrl, au, arrayPicture, workPicture);
	if (pArrayPicture != NULL) {
		copied = sceKernelCopyToUser(pArrayPicture, arrayPicture,
					    sizeof(*arrayPicture));
		if (copied < 0)
			return copied;
	}
	if (pWorkPicture != NULL) {
		copied = sceKernelCopyToUser(pWorkPicture, workPicture,
					    sizeof(*workPicture));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceAvcdecDecodeStopWithWorkPicture(const SceAvcdecCtrl *pCtrl,
		SceAvcdecArrayPicture *pArrayPicture,
		SceAvcdecArrayPicture *pWorkPicture)
{
	SceAvcdecCtrl *ctrl = NULL;
	SceAvcdecArrayPicture *arrayPicture = NULL;
	SceAvcdecArrayPicture *workPicture = NULL;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	if (pArrayPicture != NULL) {
		arrayPicture = __builtin_alloca(sizeof(*arrayPicture));
		*arrayPicture = (SceAvcdecArrayPicture){0};
		copied = sceKernelCopyFromUser(arrayPicture, pArrayPicture,
					      sizeof(*arrayPicture));
		if (copied < 0)
			return copied;
	}
	if (pWorkPicture != NULL) {
		workPicture = __builtin_alloca(sizeof(*workPicture));
		*workPicture = (SceAvcdecArrayPicture){0};
		copied = sceKernelCopyFromUser(workPicture, pWorkPicture,
					      sizeof(*workPicture));
		if (copied < 0)
			return copied;
	}
	ret = SceAvcodecForDriver_E355CAAE(ctrl, arrayPicture, workPicture);
	if (pArrayPicture != NULL) {
		copied = sceKernelCopyToUser(pArrayPicture, arrayPicture,
					    sizeof(*arrayPicture));
		if (copied < 0)
			return copied;
	}
	if (pWorkPicture != NULL) {
		copied = sceKernelCopyToUser(pWorkPicture, workPicture,
					    sizeof(*workPicture));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceAvcdecDecodeNalAu(const SceAvcdecCtrl *pCtrl, const VeneziaVipVideoInput *pAu,
		    SceAvcdecArrayPicture *pArrayPicture)
{
	SceAvcdecArrayPicture *arrayPicture;
	int ret, copied;

	if (pCtrl != NULL) {
		SceAvcdecCtrl *ctrl = __builtin_alloca(sizeof(*ctrl));

		ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (ret < 0)
			return ret;
		pCtrl = ctrl;
	}
	if (pAu != NULL) {
		VeneziaVipVideoInput *au = __builtin_alloca(sizeof(*au));

		ret = sceKernelCopyFromUser(au, pAu, sizeof(*au));
		if (ret < 0)
			return ret;
		pAu = au;
		if (pArrayPicture != NULL)
			goto copyPicture;
		goto submit;
	}
	if (pArrayPicture != NULL)
		goto copyPicture;
submit:
	return SceAvcodecForDriver_0CC84413(pCtrl, pAu, pArrayPicture);

copyPicture:
	arrayPicture = __builtin_alloca(sizeof(*arrayPicture));
	*arrayPicture = (SceAvcdecArrayPicture){ 0 };
	ret = sceKernelCopyFromUser(arrayPicture, pArrayPicture,
				   sizeof(*arrayPicture));
	if (ret < 0)
		return ret;
	ret = SceAvcodecForDriver_0CC84413(pCtrl, pAu, arrayPicture);
	copied = sceKernelCopyToUser(pArrayPicture, arrayPicture,
				     sizeof(*arrayPicture));
	return copied < 0 ? copied : ret;
}


int _sceAvcdecDecodeNalAuWithWorkPicture(const SceAvcdecCtrl *pCtrl,
		const VeneziaVipVideoInput *pAu,
		SceAvcdecArrayPicture *pArrayPicture,
		SceAvcdecArrayPicture *pWorkPicture)
{
	SceAvcdecCtrl *ctrl = NULL;
	VeneziaVipVideoInput *au;
	SceAvcdecArrayPicture *arrayPicture = NULL;
	SceAvcdecArrayPicture *workPicture = NULL;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (ret < 0)
			return ret;
	}
	if (pAu != NULL) {
		au = __builtin_alloca(sizeof(*au));
		ret = sceKernelCopyFromUser(au, pAu, sizeof(*au));
		if (ret < 0)
			return ret;
		pAu = au;
	}
	if (pArrayPicture != NULL) {
		arrayPicture = __builtin_alloca(sizeof(*arrayPicture));
		*arrayPicture = (SceAvcdecArrayPicture){ 0 };
		ret = sceKernelCopyFromUser(arrayPicture, pArrayPicture,
					   sizeof(*arrayPicture));
		if (ret < 0)
			return ret;
	}
	if (pWorkPicture != NULL) {
		workPicture = __builtin_alloca(sizeof(*workPicture));
		*workPicture = (SceAvcdecArrayPicture){ 0 };
		ret = sceKernelCopyFromUser(workPicture, pWorkPicture,
					   sizeof(*workPicture));
		if (ret < 0)
			return ret;
	}
	ret = SceAvcodecForDriver_20892154(ctrl, pAu, arrayPicture, workPicture);
	if (pArrayPicture != NULL) {
		copied = sceKernelCopyToUser(pArrayPicture, arrayPicture,
					     sizeof(*arrayPicture));
		if (copied < 0)
			return copied;
	}
	if (pWorkPicture != NULL) {
		copied = sceKernelCopyToUser(pWorkPicture, workPicture,
					     sizeof(*workPicture));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceM4vdecQueryDecoderMemSizeInternal(unsigned int codecType,
		const Dsp0VideocodecM4vDecoderInfo *pInfo, SceSize *pFrameMemSize)
{
	Dsp0VideocodecM4vDecoderInfo *info;
	SceSize *frameMemSize;
	int ret, copied;

	if (pInfo != NULL) {
		info = __builtin_alloca(sizeof(*info));
		ret = sceKernelCopyFromUser(info, pInfo, sizeof(*info));
		if (ret < 0)
			return ret;
		pInfo = info;
	}
	if (pFrameMemSize == NULL)
		return SceAvcodecForDriver_C99242F5(codecType, pInfo, pFrameMemSize);
	frameMemSize = __builtin_alloca(sizeof(*frameMemSize));
	*frameMemSize = 0;
	ret = SceAvcodecForDriver_C99242F5(codecType, pInfo, frameMemSize);
	copied = sceKernelCopyToUser(pFrameMemSize, frameMemSize,
				    sizeof(*frameMemSize));
	return copied < 0 ? copied : ret;
}

int _sceM4vdecCreateDecoderInternal(unsigned int codecType,
		SceAvcdecCtrl *pCtrl, const Dsp0VideocodecM4vDecoderInfo *pInfo)
{
	SceAvcdecCtrl *ctrl = NULL;
	Dsp0VideocodecM4vDecoderInfo *info;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		*ctrl = (SceAvcdecCtrl){0};
		ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (ret < 0)
			return ret;
	}
	if (pInfo != NULL) {
		info = __builtin_alloca(sizeof(*info));
		ret = sceKernelCopyFromUser(info, pInfo, sizeof(*info));
		if (ret < 0)
			return ret;
		pInfo = info;
	}
	ret = SceAvcodecForDriver_C363B31A(codecType, ctrl, pInfo);
	if (pCtrl != NULL) {
		copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceM4vdecQueryDecoderMemSize(unsigned int codecType,
		const Dsp0VideocodecM4vDecoderInfo *pInfo, SceSize *pFrameMemSize)
{
	Dsp0VideocodecM4vDecoderInfo *info;
	SceSize *frameMemSize;
	int ret, copied;

	if (pInfo != NULL) {
		info = __builtin_alloca(sizeof(*info));
		ret = sceKernelCopyFromUser(info, pInfo, sizeof(*info));
		if (ret < 0)
			return ret;
		pInfo = info;
	}
	if (pFrameMemSize == NULL)
		return SceAvcodecForDriver_C5C10C8B(codecType, pInfo, pFrameMemSize);
	frameMemSize = __builtin_alloca(sizeof(*frameMemSize));
	*frameMemSize = 0;
	ret = SceAvcodecForDriver_C5C10C8B(codecType, pInfo, frameMemSize);
	copied = sceKernelCopyToUser(pFrameMemSize, frameMemSize,
				    sizeof(*frameMemSize));
	return copied < 0 ? copied : ret;
}

int _sceM4vdecCreateDecoder(unsigned int codecType,
		SceAvcdecCtrl *pCtrl, const Dsp0VideocodecM4vDecoderInfo *pInfo)
{
	SceAvcdecCtrl *ctrl = NULL;
	Dsp0VideocodecM4vDecoderInfo *info;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		*ctrl = (SceAvcdecCtrl){0};
		ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (ret < 0)
			return ret;
	}
	if (pInfo != NULL) {
		info = __builtin_alloca(sizeof(*info));
		ret = sceKernelCopyFromUser(info, pInfo, sizeof(*info));
		if (ret < 0)
			return ret;
		pInfo = info;
	}
	ret = SceAvcodecForDriver_47D8E150(codecType, ctrl, pInfo);
	if (pCtrl != NULL) {
		copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceM4vdecDeleteDecoder(SceAvcdecCtrl *pCtrl)
{
	SceAvcdecCtrl *ctrl;
	int ret, copied;

	if (pCtrl == NULL)
		return SceAvcodecForDriver_77529FD2(NULL);
	ctrl = __builtin_alloca(sizeof(*ctrl));
	*ctrl = (SceAvcdecCtrl){ 0 };
	ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
	if (ret < 0)
		return ret;
	ret = SceAvcodecForDriver_77529FD2(ctrl);
	copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
	return copied < 0 ? copied : ret;
}



int _sceM4vdecDecodeAvailableSize(SceAvcdecCtrl *pCtrl)
{
	SceAvcdecCtrl *ctrl;
	int ret, copied;

	if (pCtrl == NULL)
		return SceAvcodecForDriver_91E94B76(NULL);
	ctrl = __builtin_alloca(sizeof(*ctrl));
	*ctrl = (SceAvcdecCtrl){ 0 };
	ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
	if (ret < 0)
		return ret;
	ret = SceAvcodecForDriver_91E94B76(ctrl);
	copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
	return copied < 0 ? copied : ret;
}


int _sceM4vdecDecode(const SceAvcdecCtrl *pCtrl, const M4vdecAu *pAu,
		    M4vdecArrayPicture *pArrayPicture)
{
	SceAvcdecCtrl *ctrl = NULL;
	M4vdecAu *au = NULL;
	M4vdecArrayPicture *arrayPicture;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (ret < 0)
			return ret;
		if (pAu != NULL)
			goto copyAu;
	} else if (pAu != NULL) {
		goto copyAu;
	}
	if (pArrayPicture != NULL)
		goto copyPicture;
submit:
	return SceAvcodecForDriver_2C7A9841(ctrl, au, pArrayPicture);

copyAu:
	au = __builtin_alloca(sizeof(*au));
	ret = sceKernelCopyFromUser(au, pAu, sizeof(*au));
	if (ret < 0)
		return ret;
	if (pArrayPicture == NULL)
		goto submit;

copyPicture:
	arrayPicture = __builtin_alloca(sizeof(*arrayPicture));
	*arrayPicture = (M4vdecArrayPicture){ 0 };
	ret = sceKernelCopyFromUser(arrayPicture, pArrayPicture,
				   sizeof(*arrayPicture));
	if (ret < 0)
		return ret;
	ret = SceAvcodecForDriver_2C7A9841(ctrl, au, arrayPicture);
	copied = sceKernelCopyToUser(pArrayPicture, arrayPicture,
				     sizeof(*arrayPicture));
	return copied < 0 ? copied : ret;
}

int _sceM4vdecDecodeStop(const SceAvcdecCtrl *pCtrl,
		       M4vdecArrayPicture *pArrayPicture)
{
	M4vdecArrayPicture *arrayPicture;
	int ret, copied;

	if (pCtrl != NULL) {
		SceAvcdecCtrl *ctrl = __builtin_alloca(sizeof(*ctrl));

		ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (ret < 0)
			return ret;
		pCtrl = ctrl;
		if (pArrayPicture != NULL)
			goto copyPicture;
		goto submit;
	}
	if (pArrayPicture != NULL)
		goto copyPicture;
submit:
	return SceAvcodecForDriver_F67F93DC(pCtrl, pArrayPicture);

copyPicture:
	arrayPicture = __builtin_alloca(sizeof(*arrayPicture));
	*arrayPicture = (M4vdecArrayPicture){ 0 };
	ret = sceKernelCopyFromUser(arrayPicture, pArrayPicture,
				   sizeof(*arrayPicture));
	if (ret < 0)
		return ret;
	ret = SceAvcodecForDriver_F67F93DC(pCtrl, arrayPicture);
	copied = sceKernelCopyToUser(pArrayPicture, arrayPicture,
				     sizeof(*arrayPicture));
	return copied < 0 ? copied : ret;
}

int _sceM4vdecDecodeFlush(const SceAvcdecCtrl *pCtrl)
{
	SceAvcdecCtrl *ctrl = NULL;
	int ret;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (ret < 0)
			return ret;
	}
	return SceAvcodecForDriver_FD805BF1(ctrl);
}

int _sceM4vdecCsc(M4vdecPicture *pDst, const M4vdecPicture *pSrc)
{
	M4vdecPicture *dst = NULL;
	M4vdecPicture *src;
	int ret, copied;

	if (pDst != NULL) {
		dst = __builtin_alloca(sizeof(*dst));
		memset(dst, 0, sizeof(*dst));
		ret = sceKernelCopyFromUser(dst, pDst, sizeof(*dst));
		if (ret < 0)
			return ret;
	}
	if (pSrc != NULL) {
		src = __builtin_alloca(sizeof(*src));
		ret = sceKernelCopyFromUser(src, pSrc, sizeof(*src));
		if (ret < 0)
			return ret;
		pSrc = src;
	}
	ret = SceAvcodecForDriver_A6CAE217(dst, pSrc);
	if (pDst != NULL) {
		copied = sceKernelCopyToUser(pDst, dst, sizeof(*dst));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceM4vdecDecodeWithWorkPicture(const SceAvcdecCtrl *pCtrl,
		const M4vdecAu *pAu, M4vdecArrayPicture *pArrayPicture,
		M4vdecArrayPicture *pWorkPicture)
{
	SceAvcdecCtrl *ctrl = NULL;
	M4vdecAu *au = NULL;
	M4vdecArrayPicture *arrayPicture = NULL;
	M4vdecArrayPicture *workPicture = NULL;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	if (pAu != NULL) {
		au = __builtin_alloca(sizeof(*au));
		copied = sceKernelCopyFromUser(au, pAu, sizeof(*au));
		if (copied < 0)
			return copied;
	}
	if (pArrayPicture != NULL) {
		arrayPicture = __builtin_alloca(sizeof(*arrayPicture));
		*arrayPicture = (M4vdecArrayPicture){0};
		copied = sceKernelCopyFromUser(arrayPicture, pArrayPicture,
					      sizeof(*arrayPicture));
		if (copied < 0)
			return copied;
	}
	if (pWorkPicture != NULL) {
		workPicture = __builtin_alloca(sizeof(*workPicture));
		*workPicture = (M4vdecArrayPicture){0};
		copied = sceKernelCopyFromUser(workPicture, pWorkPicture,
					      sizeof(*workPicture));
		if (copied < 0)
			return copied;
	}
	ret = SceAvcodecForDriver_823E187E(ctrl, au, arrayPicture, workPicture);
	if (pArrayPicture != NULL) {
		copied = sceKernelCopyToUser(pArrayPicture, arrayPicture,
					    sizeof(*arrayPicture));
		if (copied < 0)
			return copied;
	}
	if (pWorkPicture != NULL) {
		copied = sceKernelCopyToUser(pWorkPicture, workPicture,
					    sizeof(*workPicture));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceM4vdecDecodeStopWithWorkPicture(const SceAvcdecCtrl *pCtrl,
		M4vdecArrayPicture *pArrayPicture,
		M4vdecArrayPicture *pWorkPicture)
{
	SceAvcdecCtrl *ctrl = NULL;
	M4vdecArrayPicture *arrayPicture = NULL;
	M4vdecArrayPicture *workPicture = NULL;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	if (pArrayPicture != NULL) {
		arrayPicture = __builtin_alloca(sizeof(*arrayPicture));
		*arrayPicture = (M4vdecArrayPicture){0};
		copied = sceKernelCopyFromUser(arrayPicture, pArrayPicture,
					      sizeof(*arrayPicture));
		if (copied < 0)
			return copied;
	}
	if (pWorkPicture != NULL) {
		workPicture = __builtin_alloca(sizeof(*workPicture));
		*workPicture = (M4vdecArrayPicture){0};
		copied = sceKernelCopyFromUser(workPicture, pWorkPicture,
					      sizeof(*workPicture));
		if (copied < 0)
			return copied;
	}
	ret = SceAvcodecForDriver_3F146942(ctrl, arrayPicture, workPicture);
	if (pArrayPicture != NULL) {
		copied = sceKernelCopyToUser(pArrayPicture, arrayPicture,
					    sizeof(*arrayPicture));
		if (copied < 0)
			return copied;
	}
	if (pWorkPicture != NULL) {
		copied = sceKernelCopyToUser(pWorkPicture, workPicture,
					    sizeof(*workPicture));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceAvcencSetAvailablePreset(unsigned int codecType, unsigned int preset)
{
	return SceAvcodecForDriver_7DC13C29(codecType, preset);
}

int _sceAvcencQueryEncoderMemSize(unsigned int codecType, const AvcencInitParam *pParam,
		AvcencEncoderMemSize *pMemSize)
{
	AvcencInitParam *param;
	AvcencEncoderMemSize *memSize;
	int ret, copied;

	if (pParam != NULL) {
		param = __builtin_alloca(sizeof(*param));
		ret = sceKernelCopyFromUser(param, pParam, sizeof(*param));
		if (ret < 0)
			return ret;
		pParam = param;
	}
	if (pMemSize == NULL)
		return SceAvcodecForDriver_5C3434FB(codecType, pParam, pMemSize);
	memSize = __builtin_alloca(sizeof(*memSize));
	*memSize = (AvcencEncoderMemSize){0};
	ret = SceAvcodecForDriver_5C3434FB(codecType, pParam, memSize);
	copied = sceKernelCopyToUser(pMemSize, memSize, sizeof(*memSize));
	return copied < 0 ? copied : ret;
}

int _sceAvcencCreateEncoder(unsigned int codecType, AvcencControl *pCtrl,
		const AvcencInitParam *pParam)
{
	AvcencControl *ctrl = NULL;
	AvcencInitParam *param;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		*ctrl = (AvcencControl){0};
		ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (ret < 0)
			return ret;
	}
	if (pParam != NULL) {
		param = __builtin_alloca(sizeof(*param));
		ret = sceKernelCopyFromUser(param, pParam, sizeof(*param));
		if (ret < 0)
			return ret;
		pParam = param;
	}
	ret = SceAvcodecForDriver_B2A51EB0(codecType, ctrl, pParam);
	if (pCtrl != NULL) {
		copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceAvcencSetEncoderParameter(const AvcencControl *pCtrl,
				 const AvcencEncoderParameter *pParameter)
{
	AvcencControl *ctrl = NULL;
	AvcencEncoderParameter *parameter = NULL;
	int ret;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (ret < 0)
			return ret;
	}
	if (pParameter != NULL) {
		parameter = __builtin_alloca(sizeof(*parameter));
		ret = sceKernelCopyFromUser(parameter, pParameter,
					   sizeof(*parameter));
		if (ret < 0)
			return ret;
	}
	return SceAvcodecForDriver_E0232BFE(ctrl, parameter);
}

int _sceAvcencQueryEncoderMemSizeInternal(unsigned int codecType, const AvcencInitParamInternal *pParam,
		AvcencEncoderMemSize *pMemSize)
{
	AvcencInitParamInternal *param;
	AvcencEncoderMemSize *memSize;
	int ret, copied;

	if (pParam != NULL) {
		param = __builtin_alloca(sizeof(*param));
		ret = sceKernelCopyFromUser(param, pParam, sizeof(*param));
		if (ret < 0)
			return ret;
		pParam = param;
	}
	if (pMemSize == NULL)
		return SceAvcodecForDriver_BF3835E2(codecType, pParam, pMemSize);
	memSize = __builtin_alloca(sizeof(*memSize));
	*memSize = (AvcencEncoderMemSize){0};
	ret = SceAvcodecForDriver_BF3835E2(codecType, pParam, memSize);
	copied = sceKernelCopyToUser(pMemSize, memSize, sizeof(*memSize));
	return copied < 0 ? copied : ret;
}

int _sceAvcencCreateEncoderInternal(unsigned int codecType, AvcencControl *pCtrl,
		const AvcencInitParamInternal *pParam)
{
	AvcencControl *ctrl = NULL;
	AvcencInitParamInternal *param;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		*ctrl = (AvcencControl){0};
		ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (ret < 0)
			return ret;
	}
	if (pParam != NULL) {
		param = __builtin_alloca(sizeof(*param));
		ret = sceKernelCopyFromUser(param, pParam, sizeof(*param));
		if (ret < 0)
			return ret;
		pParam = param;
	}
	ret = SceAvcodecForDriver_71C37694(codecType, ctrl, pParam);
	if (pCtrl != NULL) {
		copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceVideoencInitLibrary(unsigned int codecType,
		const SceVideodecQueryInitInfo *pInitInfo,
		const SceVideodecQueryInitInfo *pEncoderInfo)
{
	SceVideodecQueryInitInfo *initInfo, *encoderInfo;
	int ret;

	if (pInitInfo != NULL) {
		initInfo = __builtin_alloca(sizeof(*initInfo));
		ret = sceKernelCopyFromUser(initInfo, pInitInfo, sizeof(*initInfo));
		if (ret < 0)
			return ret;
		pInitInfo = initInfo;
	}
	if (pEncoderInfo != NULL) {
		encoderInfo = __builtin_alloca(sizeof(*encoderInfo));
		ret = sceKernelCopyFromUser(encoderInfo, pEncoderInfo,
					   sizeof(*encoderInfo));
		if (ret < 0)
			return ret;
		pEncoderInfo = encoderInfo;
	}
	return SceAvcodecForDriver_E782E735(codecType, pInitInfo, pEncoderInfo);
}

int _sceVideoencQueryMemSize(unsigned int codecType,
		const SceVideodecQueryInitInfo *pInitInfo,
		const SceVideodecQueryInitInfo *pEncoderInfo,
		SceVideodecMemInfo *pMemInfo)
{
	SceVideodecQueryInitInfo *initInfo, *encoderInfo;
	SceVideodecMemInfo *memInfo;
	int ret, copied;

	if (pInitInfo != NULL) {
		initInfo = __builtin_alloca(sizeof(*initInfo));
		ret = sceKernelCopyFromUser(initInfo, pInitInfo, sizeof(*initInfo));
		if (ret < 0)
			return ret;
		pInitInfo = initInfo;
	}
	if (pEncoderInfo != NULL) {
		encoderInfo = __builtin_alloca(sizeof(*encoderInfo));
		ret = sceKernelCopyFromUser(encoderInfo, pEncoderInfo,
					   sizeof(*encoderInfo));
		if (ret < 0)
			return ret;
		pEncoderInfo = encoderInfo;
	}
	if (pMemInfo == NULL)
		return SceAvcodecForDriver_32DEE3C5(codecType, pInitInfo, pEncoderInfo, pMemInfo);
	memInfo = __builtin_alloca(sizeof(*memInfo));
	*memInfo = (SceVideodecMemInfo){0};
	ret = SceAvcodecForDriver_32DEE3C5(codecType, pInitInfo, pEncoderInfo, memInfo);
	copied = sceKernelCopyToUser(pMemInfo, memInfo, sizeof(*memInfo));
	return copied < 0 ? copied : ret;
}

int _sceVideoencInitLibraryWithUnmapMem(unsigned int codecType,
		SceVideodecCtrl *pCtrl, const SceVideodecQueryInitInfo *pInitInfo,
		const SceVideodecQueryInitInfo *pEncoderInfo)
{
	SceVideodecCtrl ctrlBuf, *ctrl = NULL;
	SceVideodecQueryInitInfo *initInfo, *encoderInfo;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = &ctrlBuf;
		__builtin_memset(ctrl, 0, sizeof(*ctrl));
		copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	if (pInitInfo != NULL) {
		initInfo = __builtin_alloca(sizeof(*initInfo));
		copied = sceKernelCopyFromUser(initInfo, pInitInfo,
					      sizeof(*initInfo));
		if (copied < 0)
			return copied;
		pInitInfo = initInfo;
	}
	if (pEncoderInfo != NULL) {
		encoderInfo = __builtin_alloca(sizeof(*encoderInfo));
		copied = sceKernelCopyFromUser(encoderInfo, pEncoderInfo,
					      sizeof(*encoderInfo));
		if (copied < 0)
			return copied;
		pEncoderInfo = encoderInfo;
	}
	ret = SceAvcodecForDriver_CD7FE8D0(codecType, ctrl, pInitInfo, pEncoderInfo);
	if (pCtrl != NULL) {
		copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceVideoencInitLibraryInternal(unsigned int codecType,
		const SceVideodecQueryInitInfo *pInitInfo,
		const SceVideodecQueryInitInfo *pEncoderInfo)
{
	SceVideodecQueryInitInfo *initInfo, *encoderInfo;
	int ret;

	if (pInitInfo != NULL) {
		initInfo = __builtin_alloca(sizeof(*initInfo));
		ret = sceKernelCopyFromUser(initInfo, pInitInfo, sizeof(*initInfo));
		if (ret < 0)
			return ret;
		pInitInfo = initInfo;
	}
	if (pEncoderInfo != NULL) {
		encoderInfo = __builtin_alloca(sizeof(*encoderInfo));
		ret = sceKernelCopyFromUser(encoderInfo, pEncoderInfo,
					   sizeof(*encoderInfo));
		if (ret < 0)
			return ret;
		pEncoderInfo = encoderInfo;
	}
	return SceAvcodecForDriver_04F734CC(codecType, pInitInfo, pEncoderInfo);
}

int _sceVideoencQueryMemSizeInternal(unsigned int codecType,
		const SceVideodecQueryInitInfo *pInitInfo,
		const SceVideodecQueryInitInfo *pEncoderInfo,
		SceVideodecMemInfo *pMemInfo)
{
	SceVideodecQueryInitInfo *initInfo, *encoderInfo;
	SceVideodecMemInfo *memInfo;
	int ret, copied;

	if (pInitInfo != NULL) {
		initInfo = __builtin_alloca(sizeof(*initInfo));
		ret = sceKernelCopyFromUser(initInfo, pInitInfo, sizeof(*initInfo));
		if (ret < 0)
			return ret;
		pInitInfo = initInfo;
	}
	if (pEncoderInfo != NULL) {
		encoderInfo = __builtin_alloca(sizeof(*encoderInfo));
		ret = sceKernelCopyFromUser(encoderInfo, pEncoderInfo,
					   sizeof(*encoderInfo));
		if (ret < 0)
			return ret;
		pEncoderInfo = encoderInfo;
	}
	if (pMemInfo == NULL)
		return SceAvcodecForDriver_86B1ACC5(codecType, pInitInfo, pEncoderInfo, pMemInfo);
	memInfo = __builtin_alloca(sizeof(*memInfo));
	*memInfo = (SceVideodecMemInfo){0};
	ret = SceAvcodecForDriver_86B1ACC5(codecType, pInitInfo, pEncoderInfo, memInfo);
	copied = sceKernelCopyToUser(pMemInfo, memInfo, sizeof(*memInfo));
	return copied < 0 ? copied : ret;
}

int _sceVideoencInitLibraryWithUnmapMemInternal(unsigned int codecType,
		SceVideodecCtrl *pCtrl, const SceVideodecQueryInitInfo *pInitInfo,
		const SceVideodecQueryInitInfo *pEncoderInfo)
{
	SceVideodecCtrl ctrlBuf, *ctrl = NULL;
	SceVideodecQueryInitInfo *initInfo, *encoderInfo;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = &ctrlBuf;
		__builtin_memset(ctrl, 0, sizeof(*ctrl));
		copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	if (pInitInfo != NULL) {
		initInfo = __builtin_alloca(sizeof(*initInfo));
		copied = sceKernelCopyFromUser(initInfo, pInitInfo,
					      sizeof(*initInfo));
		if (copied < 0)
			return copied;
		pInitInfo = initInfo;
	}
	if (pEncoderInfo != NULL) {
		encoderInfo = __builtin_alloca(sizeof(*encoderInfo));
		copied = sceKernelCopyFromUser(encoderInfo, pEncoderInfo,
					      sizeof(*encoderInfo));
		if (copied < 0)
			return copied;
		pEncoderInfo = encoderInfo;
	}
	ret = SceAvcodecForDriver_BCB769EB(codecType, ctrl, pInitInfo, pEncoderInfo);
	if (pCtrl != NULL) {
		copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceVideoencTermLibrary(unsigned int codecType)
{
	return SceAvcodecForDriver_58F1E353(codecType);
}

int _sceAvcencQueryEncoderMemSizeBasic(unsigned int codecType, const AvcencInitParamBasic *pParam,
		AvcencEncoderMemSize *pMemSize)
{
	AvcencInitParamBasic *param;
	AvcencEncoderMemSize *memSize;
	int ret, copied;

	if (pParam != NULL) {
		param = __builtin_alloca(sizeof(*param));
		ret = sceKernelCopyFromUser(param, pParam, sizeof(*param));
		if (ret < 0)
			return ret;
		pParam = param;
	}
	if (pMemSize == NULL)
		return SceAvcodecForDriver_7385FB51(codecType, pParam, pMemSize);
	memSize = __builtin_alloca(sizeof(*memSize));
	*memSize = (AvcencEncoderMemSize){0};
	ret = SceAvcodecForDriver_7385FB51(codecType, pParam, memSize);
	copied = sceKernelCopyToUser(pMemSize, memSize, sizeof(*memSize));
	return copied < 0 ? copied : ret;
}

int _sceAvcencCreateEncoderBasic(unsigned int codecType, AvcencControl *pCtrl,
		const AvcencInitParamBasic *pParam)
{
	AvcencControl *ctrl = NULL;
	AvcencInitParamBasic *param;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		*ctrl = (AvcencControl){0};
		ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (ret < 0)
			return ret;
	}
	if (pParam != NULL) {
		param = __builtin_alloca(sizeof(*param));
		ret = sceKernelCopyFromUser(param, pParam, sizeof(*param));
		if (ret < 0)
			return ret;
		pParam = param;
	}
	ret = SceAvcodecForDriver_C5E720E2(codecType, ctrl, pParam);
	if (pCtrl != NULL) {
		copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceAvcencDeleteEncoder(AvcencControl *pCtrl)
{
	AvcencControl *control;
	int ret, copied;

	if (pCtrl == NULL)
		return SceAvcodecForDriver_2895CB6D(NULL);
	control = __builtin_alloca(sizeof(*control));
	*control = (AvcencControl){0};
	copied = sceKernelCopyFromUser(control, pCtrl, sizeof(*control));
	if (copied < 0)
		return copied;
	ret = SceAvcodecForDriver_2895CB6D(control);
	copied = sceKernelCopyToUser(pCtrl, control, sizeof(*control));
	return copied < 0 ? copied : ret;
}

int _sceAvcencEncode(const AvcencControl *pCtrl, const AvcencCscFrame *pFrame,
		    AvcencArrayAu *pArrayAu)
{
	AvcencControl *ctrl = NULL;
	AvcencCscFrame *frame = NULL;
	AvcencArrayAu *arrayAu;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
		if (pFrame != NULL)
			goto copyFrame;
	} else if (pFrame != NULL) {
		goto copyFrame;
	}
	if (pArrayAu != NULL)
		goto copyAu;
submit:
	return SceAvcodecForDriver_A0E63278(ctrl, frame, pArrayAu);

copyFrame:
	frame = __builtin_alloca(sizeof(*frame));
	copied = sceKernelCopyFromUser(frame, pFrame, sizeof(*frame));
	if (copied < 0)
		return copied;
	if (pArrayAu == NULL)
		goto submit;
copyAu:
	arrayAu = __builtin_alloca(sizeof(*arrayAu));
	*arrayAu = (AvcencArrayAu){0};
	copied = sceKernelCopyFromUser(arrayAu, pArrayAu, sizeof(*arrayAu));
	if (copied < 0)
		return copied;
	ret = SceAvcodecForDriver_A0E63278(ctrl, frame, arrayAu);
	copied = sceKernelCopyToUser(pArrayAu, arrayAu, sizeof(*arrayAu));
	return copied < 0 ? copied : ret;
}

int _sceAvcencEncodeStop(const AvcencControl *pCtrl, AvcencArrayAu *pArrayAu)
{
	AvcencControl *ctrl;
	AvcencArrayAu *arrayAu;
	int ret, copied;

	if (pCtrl == NULL) {
		ctrl = NULL;
		if (pArrayAu == NULL)
			goto submit;
	} else {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
		if (pArrayAu == NULL)
			goto submit;
	}

	arrayAu = __builtin_alloca(sizeof(*arrayAu));
	*arrayAu = (AvcencArrayAu){0};
	copied = sceKernelCopyFromUser(arrayAu, pArrayAu, sizeof(*arrayAu));
	if (copied < 0)
		return copied;
	ret = SceAvcodecForDriver_DE515BFF(ctrl, arrayAu);
	copied = sceKernelCopyToUser(pArrayAu, arrayAu, sizeof(*arrayAu));
	return copied < 0 ? copied : ret;

submit:
	return SceAvcodecForDriver_DE515BFF(ctrl, pArrayAu);
}

int _sceAvcencEncodeFlush(const AvcencControl *pCtrl)
{
	AvcencControl *control = NULL;
	int ret;

	if (pCtrl != NULL) {
		control = __builtin_alloca(sizeof(*control));
		ret = sceKernelCopyFromUser(control, pCtrl, sizeof(*control));
		if (ret < 0)
			return ret;
	}
	return SceAvcodecForDriver_229AB2CF(control);
}

int _sceAvcencGetNalUnit(const AvcencControl *pCtrl, unsigned int index,
		       AvcencNalUnit *pNalUnit)
{
	AvcencControl *control = NULL;
	AvcencNalUnit *nalUnit;
	int ret, copied;

	if (pCtrl != NULL) {
		control = __builtin_alloca(sizeof(*control));
		copied = sceKernelCopyFromUser(control, pCtrl, sizeof(*control));
		if (copied < 0)
			return copied;
		if (pNalUnit != NULL)
			goto copyNalUnit;
	} else {
		control = NULL;
		if (pNalUnit != NULL)
			goto copyNalUnit;
	}
	return SceAvcodecForDriver_1A584DB7(control, index, pNalUnit);
copyNalUnit:
	nalUnit = __builtin_alloca(sizeof(*nalUnit));
	*nalUnit = (AvcencNalUnit){0};
	copied = sceKernelCopyFromUser(nalUnit, pNalUnit, sizeof(*nalUnit));
	if (copied < 0)
		return copied;
	ret = SceAvcodecForDriver_1A584DB7(control, index, nalUnit);
	copied = sceKernelCopyToUser(pNalUnit, nalUnit, sizeof(*nalUnit));
	return copied < 0 ? copied : ret;
}

int _sceAvcencCsc(AvcencCscFrame *pDst, const AvcencCscFrame *pSrc)
{
	AvcencCscFrame *destination = NULL;
	AvcencCscFrame *source = NULL;
	int ret, copied;

	if (pDst != NULL) {
		destination = __builtin_alloca(sizeof(*destination));
		*destination = (AvcencCscFrame){0};
		copied = sceKernelCopyFromUser(destination, pDst,
					      sizeof(*destination));
		if (copied < 0)
			return copied;
	}
	if (pSrc != NULL) {
		source = __builtin_alloca(sizeof(*source));
		copied = sceKernelCopyFromUser(source, pSrc, sizeof(*source));
		if (copied < 0)
			return copied;
	}
	ret = SceAvcodecForDriver_4F11F915(destination, source);
	if (pDst != NULL) {
		copied = sceKernelCopyToUser(pDst, destination,
					    sizeof(*destination));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceAvcdecDecodeAuNongameapp(SceAvcdecCtrl *pCtrl,
		const SceAvcdecAu *pAu, const SceUInt *timeout)
{
	SceAvcdecCtrl *ctrl = NULL;
	SceAvcdecAu *au = NULL;
	SceUInt *wait = NULL;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		*ctrl = (SceAvcdecCtrl){0};
		copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	if (pAu != NULL) {
		au = __builtin_alloca(sizeof(*au));
		copied = sceKernelCopyFromUser(au, pAu, sizeof(*au));
		if (copied < 0)
			return copied;
	}
	if (timeout != NULL) {
		wait = __builtin_alloca(sizeof(*wait));
		copied = sceKernelCopyFromUser(wait, timeout, sizeof(*wait));
		if (copied < 0)
			return copied;
	}
	ret = SceAvcodecForDriver_78BA8E98(ctrl, au, wait);
	if (pCtrl != NULL) {
		copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceAvcdecDecodeGetPictureNongameapp(SceAvcdecCtrl *pCtrl,
		SceAvcdecArrayPicture *pArrayPicture, const SceUInt *timeout)
{
	SceAvcdecCtrl *ctrl;
	SceAvcdecArrayPicture *arrayPicture;
	SceUInt *wait = NULL;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		*ctrl = (SceAvcdecCtrl){0};
		copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
		if (pArrayPicture != NULL)
			goto copyArrayPicture;
	} else {
		ctrl = NULL;
		if (pArrayPicture != NULL)
			goto copyArrayPicture;
	}
	arrayPicture = pArrayPicture;
	goto copyTimeout;

copyArrayPicture:
	arrayPicture = __builtin_alloca(sizeof(*arrayPicture));
	*arrayPicture = (SceAvcdecArrayPicture){0};
	copied = sceKernelCopyFromUser(arrayPicture, pArrayPicture,
				      sizeof(*arrayPicture));
	if (copied < 0)
		return copied;
copyTimeout:
	if (timeout != NULL) {
		wait = __builtin_alloca(sizeof(*wait));
		copied = sceKernelCopyFromUser(wait, timeout, sizeof(*wait));
		if (copied < 0)
			return copied;
	}
	ret = SceAvcodecForDriver_F8BCBE20(ctrl, arrayPicture, wait);
	if (pCtrl != NULL) {
		copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	if (pArrayPicture != NULL) {
		copied = sceKernelCopyToUser(pArrayPicture, arrayPicture,
					    sizeof(*arrayPicture));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceAvcdecDecodeAuNalAuNongameapp(SceAvcdecCtrl *pCtrl,
		const VeneziaVipVideoInput *pAu, const SceUInt *timeout)
{
	SceAvcdecCtrl *ctrl = NULL;
	VeneziaVipVideoInput *au = NULL;
	SceUInt *wait = NULL;
	int ret, copied;

	if (pCtrl != NULL) {
		ctrl = __builtin_alloca(sizeof(*ctrl));
		*ctrl = (SceAvcdecCtrl){0};
		copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	if (pAu != NULL) {
		au = __builtin_alloca(sizeof(*au));
		copied = sceKernelCopyFromUser(au, pAu, sizeof(*au));
		if (copied < 0)
			return copied;
	}
	if (timeout != NULL) {
		wait = __builtin_alloca(sizeof(*wait));
		copied = sceKernelCopyFromUser(wait, timeout, sizeof(*wait));
		if (copied < 0)
			return copied;
	}
	ret = SceAvcodecForDriver_C9EF7057(ctrl, au, wait);
	if (pCtrl != NULL) {
		copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
		if (copied < 0)
			return copied;
	}
	return ret;
}

int _sceAvcdecRegisterCallbackNongameapp(SceAvcdecCtrl *pCtrl, SceUID cbid,
				      unsigned int eventMask)
{
	SceAvcdecCtrl *ctrl;
	int ret, copied;

	if (pCtrl == NULL)
		return SceAvcodecForDriver_E7E14DE4(pCtrl, cbid, eventMask);
	ctrl = __builtin_alloca(sizeof(*ctrl));
	*ctrl = (SceAvcdecCtrl){0};
	copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
	if (copied < 0)
		return copied;
	ret = SceAvcodecForDriver_E7E14DE4(ctrl, cbid, eventMask);
	copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
	return copied < 0 ? copied : ret;
}

int _sceAvcdecUnregisterCallbackNongameapp(SceAvcdecCtrl *pCtrl)
{
	SceAvcdecCtrl *ctrl;
	int ret, copied;

	if (pCtrl == NULL)
		return SceAvcodecForDriver_13C530E4(pCtrl);
	ctrl = __builtin_alloca(sizeof(*ctrl));
	*ctrl = (SceAvcdecCtrl){0};
	copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
	if (copied < 0)
		return copied;
	ret = SceAvcodecForDriver_13C530E4(ctrl);
	copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
	return copied < 0 ? copied : ret;
}

int _sceAvcdecUnregisterCallbackWithCbidNongameapp(SceAvcdecCtrl *pCtrl,
					       SceUID cbid)
{
	SceAvcdecCtrl *ctrl;
	int ret, copied;

	if (pCtrl == NULL)
		return SceAvcodecForDriver_E4F668C5(pCtrl, cbid);
	ctrl = __builtin_alloca(sizeof(*ctrl));
	*ctrl = (SceAvcdecCtrl){0};
	copied = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
	if (copied < 0)
		return copied;
	ret = SceAvcodecForDriver_E4F668C5(ctrl, cbid);
	copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
	return copied < 0 ? copied : ret;
}

int _sceAvcdecSetRecoveryPointSEIMode(unsigned int codecType, unsigned int mode)
{
	return SceAvcodecForDriver_20C83C00(codecType, mode);
}

int _sceAvcdecSetLowDelayModeNongameapp(unsigned int codecType, unsigned int mode)
{
	return SceAvcodecForDriver_3CB7661A(codecType, mode);
}
