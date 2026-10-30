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

/* The frame buffer an AVC decoder of codecType needs for pDecoderInfo, in 256 KiB units. */
int videocodecQueryDecoderMemSize(unsigned int codecType,
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

int _sceVideoencTermLibrary(unsigned int codecType)
{
	return SceAvcodecForDriver_58F1E353(codecType);
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
