/* Sony source: videocodec/videocodec_init.c. */
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
#include <sysmem/memblock.h>
#include <sysmem/cpu.h>
#include <lowio/iftu.h>

/* not a game process: no budget for the library's memory block */
#define VIDEOCODEC_ERROR_NOT_GAME_PROCESS	(-2141059008)	/* 0x80620840 */

int dsp0VideocodecQueryLibraryMemory(const Dsp0VideocodecInitParam *param,
				   SceSize *memorySize);
int dsp0VideocodecTermLibraryForProcess(unsigned int library, SceUID pid);


char videocodecSwDecoderMutexName[16] = "SceVcodecSWDec0";
char videocodecHwDecoderMutexName[16] = "SceVcodecHWDec0";
char videocodecSwEncoderMutexName[16] = "SceVcodecSWEnc0";

VideocodecResident videocodecResident = {
	.processLocalStorageId = -1,
};

struct ThreadmgrFastMutex videocodecTileRegMutex;

#if FW_DEBUG
SCE_DBGINFO_DEFINE(static, videocodecGlobalStorage, 124, "_GetVideocodecGlobal");
SCE_DBGINFO_DEFINE(static, videocodecExitStorage, 333, "_sceVideocodecProcessExitEntry");
SCE_DBGINFO_DEFINE(static, videocodecCreateStorage, 313, "_sceVideocodecProcessCreateEntry");
#endif

static inline __attribute__((always_inline)) void *
videocodecGetGlobal(SceUID pid)
{
	void *storage = 0;

	if (pid != 0) {
		if (sceKernelGetRemoteKPLS(pid,
				videocodecResident.processLocalStorageId,
				&storage, 0) < 0)
			storage = 0;
	} else {
		storage = SceProcessmgrForDriver_EE694840(
			videocodecResident.processLocalStorageId);
	}
#if FW_DEBUG
	sceKernelAssert(storage != 0, &videocodecGlobalStorage,
		__builtin_return_address(0));
#endif
	return storage;
}

void videocodecNotifyProcessChange(SceUID pid, int active)
{
	if (videocodecResident.notifyProcessChange != 0)
		videocodecResident.notifyProcessChange(pid, active);
}

static void videocodecFinalizeAvcDecoderMutexes(VideocodecGlobal *global,
		const VideocodecSlotCounts *counts)
{
	unsigned int i = counts->numOfAvcDecoders;

	while (i != 0) {
		i--;
		sceKernelFinalizeFastMutex((SceKernelFastMutex *)&global->avcDecoders[i].mutex);
	}
}

static inline __attribute__((always_inline)) int
videocodecInitAvcDecoderMutexesInline(VideocodecGlobal *global, const VideocodecSlotCounts *counts)
{
	VideocodecSlotCounts initialized = {0, 0, 0};
	unsigned int i;
	int ret = 0;

	for (i = 0; i < counts->numOfAvcDecoders; ) {
		videocodecHwDecoderMutexName[14] = '0' + i % 10;
		ret = sceKernelInitializeFastMutex((SceKernelFastMutex *)&global->avcDecoders[i].mutex,
			videocodecHwDecoderMutexName, 0x8002, 0);
		if (ret < 0) {
			videocodecFinalizeAvcDecoderMutexes(global, &initialized);
			break;
		}
		initialized.numOfAvcDecoders = ++i;
	}
	return ret;
}

__attribute__((noinline)) int videocodecInitAvcDecoderMutexes(VideocodecGlobal *global,
		const VideocodecSlotCounts *counts)
{
	return videocodecInitAvcDecoderMutexesInline(global, counts);
}

__attribute__((noinline)) int videocodecFinalizeDsp0Mutexes(VideocodecGlobal *global,
		const unsigned int *numOfDsp0Decoders, unsigned int numOfEncoders)
{
	unsigned int i;

	for (i = numOfEncoders; i != 0; i--)
		sceKernelFinalizeFastMutex((SceKernelFastMutex *)&global->encoders[i - 1].mutex);
	for (i = *numOfDsp0Decoders; i != 0; i--)
		sceKernelFinalizeFastMutex((SceKernelFastMutex *)&global->dsp0Decoders[i - 1].mutex);
	return 0;
}

static inline int videocodecIsOwnerAvailable(SceUID pid)
{
	if ((videocodecResident.ownerProcessIds[0] == -1 ||
	     videocodecResident.ownerProcessIds[0] == pid ||
	     videocodecResident.ownerProcessIds[1] == -1 ||
	     videocodecResident.ownerProcessIds[1] == pid ||
	     videocodecResident.ownerProcessIds[2] == -1 ||
	     videocodecResident.ownerProcessIds[2] == pid ||
	     videocodecResident.ownerProcessIds[3] == -1 ||
	     videocodecResident.ownerProcessIds[3] == pid) &&
	    (videocodecResident.activeProcessId == -1 ||
	     videocodecResident.activeProcessId == pid))
		return 1;
	return 0;
}

/*
 * The memory a library of codecType needs for pInitInfo (and, for the DSP0, an encoder for
 * pEncoderInfo).
 */
static inline __attribute__((always_inline)) int
videocodecQueryMemSizeInline(unsigned int codecType,
		const SceVideodecQueryInitInfo *pInitInfo,
		const SceVideodecQueryInitInfo *pEncoderInfo, SceVideodecMemInfo *pMemInfo)
{
	VideocodecGlobal *global = videocodecGetGlobal(0);
	SceUID pid = sceKernelGetProcessId();
	unsigned int horizontal, vertical, numOfRefFrames, numOfStreams;
	unsigned int maxRefFrames, maxHorizontal, maxVertical;
	unsigned int numOfEncoders;
	int isDsp0, ret, unlockRet;
	SceSize memSize;

	if (codecType == SCE_VIDEODEC_TYPE_HW_AVCDEC) {
		if (pInitInfo->hwAvc.size != sizeof(SceVideodecQueryInitInfoHwAvcdec))
			return SCE_VIDEODEC_ERROR_INVALID_ARGUMENT_SIZE;
		horizontal = pInitInfo->hwAvc.horizontal;
		vertical = pInitInfo->hwAvc.vertical;
		numOfRefFrames = pInitInfo->hwAvc.numOfRefFrames;
		numOfStreams = pInitInfo->hwAvc.numOfStreams;
		if (numOfRefFrames == 0)
			numOfRefFrames = 1;
		maxRefFrames = 0xC00000 / ((3 * vertical * horizontal) >> 1);
		if (maxRefFrames >= 16)
			maxRefFrames = 16;
		isDsp0 = 0;
		maxVertical = 1088;
		maxHorizontal = 1920;
	} else {
		if (pInitInfo->hwAvc.size != sizeof(SceVideodecQueryInitInfoHwAvcdec))
			return SCE_VIDEODEC_ERROR_INVALID_ARGUMENT_SIZE;
		horizontal = pInitInfo->hwAvc.horizontal;
		vertical = pInitInfo->hwAvc.vertical;
		numOfRefFrames = pInitInfo->hwAvc.numOfRefFrames;
		numOfStreams = pInitInfo->hwAvc.numOfStreams;
		if (numOfRefFrames == 0)
			numOfRefFrames = 1;
		maxRefFrames = 0x697800 / ((3 * vertical * horizontal) >> 1);
		isDsp0 = 1;
		if (maxRefFrames >= 16)
			maxRefFrames = 16;
		maxVertical = 720;
		maxHorizontal = 1280;
	}
	if (numOfStreams == 0)
		numOfStreams = 1;
	if (horizontal > maxHorizontal || horizontal < 64)
		return SCE_VIDEODEC_ERROR_INVALID_PARAM;
	if (vertical > maxVertical || vertical < 64)
		return SCE_VIDEODEC_ERROR_INVALID_PARAM;
	if (numOfRefFrames > maxRefFrames || numOfStreams > 4)
		return SCE_VIDEODEC_ERROR_INVALID_PARAM;
	if (pEncoderInfo != NULL) {
		unsigned int encoderHorizontal, encoderVertical;
		unsigned int encoderRefFrames, encoderStreams;

		if (pEncoderInfo->hwAvc.size != sizeof(SceVideodecQueryInitInfoHwAvcdec))
			return SCE_VIDEODEC_ERROR_INVALID_ARGUMENT_SIZE;
		encoderHorizontal = pEncoderInfo->hwAvc.horizontal;
		encoderRefFrames = pEncoderInfo->hwAvc.numOfRefFrames;
		encoderStreams = pEncoderInfo->hwAvc.numOfStreams;
		encoderVertical = pEncoderInfo->hwAvc.vertical;
		if (encoderRefFrames == 0)
			encoderRefFrames = 1;
		if (encoderStreams == 0)
			encoderStreams = 1;
		if (encoderHorizontal - 64 > 896 || encoderVertical - 64 > 512)
			return SCE_VIDEODEC_ERROR_INVALID_PARAM;
		if (vertical < encoderVertical || horizontal < encoderHorizontal)
			return SCE_VIDEODEC_ERROR_INVALID_PARAM;
		if (encoderStreams > 1 || encoderRefFrames > 1)
			return SCE_VIDEODEC_ERROR_INVALID_PARAM;
		numOfEncoders = 1;
	} else {
		numOfEncoders = 0;
	}

	ret = lockFastMutex((SceKernelFastMutex *)&videocodecResident.allMutex);
	if (ret < 0)
		return ret;
	if (!videocodecIsOwnerAvailable(pid)) {
		ret = SCE_VIDEODEC_ERROR_ALREADY_USED;
	} else if (!isDsp0) {
		VeneziaDebugImageGeometry geometry;

		if (global->avcdecInitCount > 0) {
			ret = SCE_VIDEODEC_ERROR_ALREADY_USED;
		} else {
			geometry.width = horizontal;
			geometry.height = vertical;
			geometry.imageCount = numOfRefFrames;
			geometry.processorCount = numOfStreams;
			ret = videocodecConvertValidationError(
				SceCodecEngineWrapperForDriver_8CD35820(&geometry, &memSize));
			if (ret >= 0) {
				ret = 0;
				pMemInfo->memSize = memSize;
			}
		}
	} else {
		Dsp0VideocodecInitParam param;

		if (global->dsp0InitCount > 0) {
			ret = SCE_VIDEODEC_ERROR_ALREADY_USED;
		} else {
			param.horizontal = horizontal;
			param.vertical = vertical;
			param.numOfRefFrames = numOfRefFrames;
			param.numOfStreams = numOfStreams;
			param.numOfEncoders = numOfEncoders;
			ret = videocodecConvertDecoderError(
				dsp0VideocodecQueryLibraryMemory(&param, &memSize));
			if (ret >= 0) {
				ret = 0;
				pMemInfo->memSize = memSize;
			}
		}
	}
	unlockRet = sceKernelUnlockFastMutex((SceKernelFastMutex *)&videocodecResident.allMutex);
	return unlockRet < 0 ? unlockRet : ret;
}

__attribute__((noinline))
static int videocodecQueryMemSize(unsigned int codecType,
		const SceVideodecQueryInitInfo *pInitInfo,
		const SceVideodecQueryInitInfo *pEncoderInfo, SceVideodecMemInfo *pMemInfo)
{
	return videocodecQueryMemSizeInline(codecType, pInitInfo, pEncoderInfo, pMemInfo);
}

static int videocodecReleaseLibrary(int isDsp0, unsigned int flags, SceUID pid,
		const VideocodecSlotCounts *counts)
{
	VideocodecGlobal *global = videocodecGetGlobal(pid);
	VideocodecMemory *memory;
	void *userAddress;
	unsigned int i;

	if (isDsp0) {
		if (flags & 2) {
			for (i = counts->numOfEncoders; i != 0; i--)
				sceKernelFinalizeFastMutex(
					(SceKernelFastMutex *)&global->encoders[i - 1].mutex);
			for (i = counts->numOfDsp0Decoders; i != 0; i--)
				sceKernelFinalizeFastMutex(
					(SceKernelFastMutex *)&global->dsp0Decoders[i - 1].mutex);
		}
		if (flags & 1) {
			memory = &global->dsp0Memory;
			if (memory->kind == VIDEOCODEC_MEMORY_BLOCK) {
				sceKernelFreeMemBlock(memory->memBlockId);
			} else if (memory->kind == VIDEOCODEC_MEMORY_UNMAP &&
				   sceCodecEngineUnmapKernelVAtoUserVA(pid, memory->base,
					&userAddress) >= 0) {
				sceCodecEngineUnregisterUnmapMemory(pid,
					(SceUIntVAddr)userAddress, memory->size);
			}
			memory->base = NULL;
			memory->size = 0;
			memory->kind = 0;
			memory->memBlockId = 0;
		}
	} else {
		if (flags & 2) {
			for (i = counts->numOfAvcDecoders; i != 0; i--)
				sceKernelFinalizeFastMutex(
					(SceKernelFastMutex *)&global->avcDecoders[i - 1].mutex);
		}
		if (flags & 1) {
			memory = &global->avcdecMemory;
			if (memory->kind == VIDEOCODEC_MEMORY_BLOCK) {
				sceKernelFreeMemBlock(memory->memBlockId);
			} else if (memory->kind == VIDEOCODEC_MEMORY_UNMAP &&
				   sceCodecEngineUnmapKernelVAtoUserVA(pid, memory->base,
					&userAddress) >= 0) {
				sceCodecEngineUnregisterUnmapMemory(pid,
					(SceUIntVAddr)userAddress, memory->size);
			}
			memory->base = NULL;
			memory->size = 0;
			memory->kind = 0;
			memory->memBlockId = 0;
		}
	}
	return 0;
}

/* The hardware memory of the AVC decoder library: the caller's unmap memory, or a new block. */
__attribute__((noinline)) int videocodecAllocAvcdecMemory(VideocodecGlobal *global, const SceVideodecCtrl *pCtrl,
		const SceVideodecMemInfo *pMemInfo)
{
	VideocodecMemory *memory = &global->avcdecMemory;
	SceKernelAllocMemBlockKernelOpt opt;
	SceSize size = pMemInfo->memSize;
	void *base;
	SceUID uid;
	int ret;

	memory->size = size;
	memory->kind = VIDEOCODEC_MEMORY_BLOCK;
	if (pCtrl->memBufUid == 0 && pCtrl->vaContext == 0) {
		size = (size + 0x3FFFF) >> 18 << 18;
		if (!sceKernelIsGameBudget())
			return VIDEOCODEC_ERROR_NOT_GAME_PROCESS;
		memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT |
			   SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
		opt.alignment = 0x40000;
		uid = sceKernelAllocMemBlockForKernel("SceVideocodecHwMemBuf_Phycont",
			SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_PHYCONT_NC_RW, size, &opt);
		if (uid < 0)
			return uid;
		ret = sceKernelGetMemBlockBase(uid, &base);
		if (ret < 0)
			return ret;
		memory->base = base;
		memory->memBlockId = uid;
		memory->size = size;
		memory->kind = VIDEOCODEC_MEMORY_BLOCK;
		return ret;
	}
	if (pCtrl->memBuf.pBuf != NULL || pCtrl->memBuf.size != 0 ||
	    size > pCtrl->contextSize)
		return SCE_VIDEODEC_ERROR_INVALID_PARAM;
	if ((pCtrl->vaContext & 0x3FFFF) != 0)
		return SCE_VIDEODEC_ERROR_INVALID_POINTER;
	ret = sceCodecEngineRegisterUnmapMemory(0, pCtrl->vaContext, pCtrl->contextSize);
	if (ret < 0)
		return ret;
	ret = sceCodecEngineUnmapUserVAtoKernelVA(0, (const void *)pCtrl->vaContext, &base);
	if (ret < 0) {
		sceCodecEngineUnregisterUnmapMemory(0, pCtrl->vaContext, pCtrl->contextSize);
		return ret;
	}
	memory->base = base;
	memory->size = pCtrl->contextSize;
	memory->kind = VIDEOCODEC_MEMORY_UNMAP;
	memory->memBlockId = 0;
	return ret;
}

static inline int videocodecIsLockAborted(int ret)
{
	return ret == SCE_KERNEL_ERROR_THREAD_STOPPED || ret == SCE_KERNEL_ERROR_THREAD_SUSPENDED;
}

static inline int videocodecLockAvcDecoders(VideocodecGlobal *global,
		const VideocodecSlotCounts *counts, VideocodecSlotCounts *locked)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i < counts->numOfAvcDecoders; i++) {
		ret = lockFastMutex((SceKernelFastMutex *)&global->avcDecoders[i].mutex);
		if (ret < 0) {
			while (i != 0) {
				i--;
				sceKernelUnlockFastMutex((SceKernelFastMutex *)&global->avcDecoders[i].mutex);
			}
			return ret;
		}
		locked->numOfAvcDecoders = i + 1;
	}
	return ret;
}

static inline void videocodecUnlockAvcDecoders(VideocodecGlobal *global,
		const VideocodecSlotCounts *counts)
{
	unsigned int i;

	for (i = counts->numOfAvcDecoders; i != 0; i--)
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&global->avcDecoders[i - 1].mutex);
}

static inline int videocodecLockDsp0(VideocodecGlobal *global,
		const VideocodecSlotCounts *counts, VideocodecSlotCounts *locked)
{
	unsigned int i, numOfDecoders = 0;
	int ret = 0;

	for (i = 0; i < counts->numOfDsp0Decoders; i++) {
		ret = lockFastMutex((SceKernelFastMutex *)&global->dsp0Decoders[i].mutex);
		if (ret < 0) {
			while (i != 0) {
				i--;
				sceKernelUnlockFastMutex((SceKernelFastMutex *)&global->dsp0Decoders[i].mutex);
			}
			return ret;
		}
		locked->numOfDsp0Decoders = numOfDecoders = i + 1;
	}
	for (i = 0; i < counts->numOfEncoders; i++) {
		ret = lockFastMutex((SceKernelFastMutex *)&global->encoders[i].mutex);
		if (ret < 0) {
			while (i != 0) {
				i--;
				sceKernelUnlockFastMutex((SceKernelFastMutex *)&global->encoders[i].mutex);
			}
			while (numOfDecoders != 0) {
				numOfDecoders--;
				sceKernelUnlockFastMutex((SceKernelFastMutex *)&global->dsp0Decoders[numOfDecoders].mutex);
			}
			return ret;
		}
		locked->numOfEncoders = i + 1;
	}
	return ret;
}

static inline void videocodecUnlockDsp0(VideocodecGlobal *global,
		const VideocodecSlotCounts *counts)
{
	unsigned int i;

	for (i = counts->numOfEncoders; i != 0; i--)
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&global->encoders[i - 1].mutex);
	for (i = counts->numOfDsp0Decoders; i != 0; i--)
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&global->dsp0Decoders[i - 1].mutex);
}

int videocodecTermLibraryForProcess(unsigned int codecType, unsigned int flags, SceUID pid)
{
	VideocodecGlobal *global = videocodecGetGlobal(pid);
	VideocodecSlotCounts counts = {0, 0, 0};
	VideocodecSlotCounts locked;
	SceAvcdecCtrl ctrl;
	AvcencControl encoder;
	unsigned int i, handle;
	int ret, owner;

	if (codecType - SCE_VIDEODEC_TYPE_HW_AVCDEC > 1)
		return SCE_VIDEODEC_ERROR_INVALID_PARAM;
	ret = lockFastMutex((SceKernelFastMutex *)&videocodecResident.allMutex);
	if (ret < 0)
		return ret;
	if (codecType == SCE_VIDEODEC_TYPE_HW_AVCDEC) {
		if (global->avcdecInitCount > 0) {
			counts.numOfAvcDecoders = global->avcdecInfo.numOfStreams;
			locked.numOfAvcDecoders = 0;
			locked.numOfDsp0Decoders = 0;
			locked.numOfEncoders = 0;
			ret = videocodecLockAvcDecoders(global, &counts, &locked);
			if (videocodecIsLockAborted(ret)) {
				videocodecUnlockAvcDecoders(global, &counts);
				goto out;
			}
			for (i = 0; i < global->avcdecInfo.numOfStreams; i++) {
				handle = global->avcDecoders[i].handle;
				if (handle == 0)
					continue;
				ctrl.handle = handle;
				ret = videocodecDeleteDecoderForProcess(&ctrl, flags, pid);
				if (videocodecIsLockAborted(ret)) {
					videocodecUnlockAvcDecoders(global, &counts);
					goto out;
				}
			}
			ret = SceCodecEngineWrapperForDriver_86861877(1, flags, pid);
			if (videocodecIsLockAborted(ret)) {
				videocodecUnlockAvcDecoders(global, &counts);
				goto out;
			}
			global->avcdecInfo.horizontal = 1920;
			global->avcdecInfo.vertical = 1088;
			global->avcdecInfo.numOfRefFrames = 4;
			global->avcdecInfo.numOfStreams = 0;
			global->avcdecFlags = 0;
			global->avcdecInitCount--;
			videocodecUnlockAvcDecoders(global, &counts);
			ret = 0;
			videocodecReleaseLibrary(0, 3, pid, &counts);
		} else {
			ret = SCE_VIDEODEC_ERROR_NOT_INITIALIZE;
		}
	} else {
		if (global->dsp0InitCount > 0) {
			counts.numOfDsp0Decoders = global->dsp0DecoderInfo.numOfStreams;
			counts.numOfEncoders = global->dsp0EncoderInfo.numOfStreams;
			locked.numOfAvcDecoders = 0;
			locked.numOfDsp0Decoders = 0;
			locked.numOfEncoders = 0;
			ret = videocodecLockDsp0(global, &counts, &locked);
			if (videocodecIsLockAborted(ret)) {
				videocodecUnlockDsp0(global, &counts);
				goto out;
			}
			for (i = 0; i < global->dsp0DecoderInfo.numOfStreams; i++) {
				handle = global->dsp0Decoders[i].handle;
				if (handle & 0x100) {
					ctrl.handle = handle - 1;
					ret = videocodecDeleteDecoderForProcess(&ctrl, flags, pid);
				} else if (handle & 0x200) {
					ctrl.handle = handle - 1;
					ret = m4vdecDeleteDecoderForProcess(&ctrl, pid);
				} else {
					continue;
				}
				if (videocodecIsLockAborted(ret)) {
					videocodecUnlockDsp0(global, &counts);
					goto out;
				}
			}
			for (i = 0; i < global->dsp0EncoderInfo.numOfStreams; i++) {
				handle = global->encoders[i].handle;
				if ((handle & 0x1000) == 0)
					continue;
				encoder.handle = handle - 1;
				ret = avcencDeleteEncoderForProcess(&encoder, pid);
				if (videocodecIsLockAborted(ret)) {
					videocodecUnlockDsp0(global, &counts);
					goto out;
				}
			}
			ret = videocodecConvertDecoderError(
				dsp0VideocodecTermLibraryForProcess(global->dsp0Library, pid));
			if (ret < 0 && videocodecIsLockAborted(ret)) {
				videocodecUnlockDsp0(global, &counts);
				goto out;
			}
			global->dsp0Library = 0;
			global->dsp0Flags = 0;
			global->dsp0DecoderInfo.horizontal = 1280;
			global->dsp0DecoderInfo.vertical = 720;
			global->dsp0DecoderInfo.numOfRefFrames = 9;
			global->dsp0DecoderInfo.numOfStreams = 0;
			global->dsp0EncoderInfo.horizontal = 960;
			global->dsp0EncoderInfo.vertical = 576;
			global->dsp0EncoderInfo.numOfRefFrames = 1;
			global->dsp0EncoderInfo.numOfStreams = 0;
			global->dsp0Config = 0;
			global->dsp0InitCount--;
			videocodecUnlockDsp0(global, &counts);
			ret = 0;
			videocodecReleaseLibrary(1, 3, pid, &counts);
		} else {
			ret = SCE_VIDEODEC_ERROR_NOT_INITIALIZE;
		}
	}
	if (global->avcdecInitCount == 0 && global->dsp0InitCount == 0) {
		if (videocodecResident.ownerProcessIds[0] == pid)
			owner = 0;
		else if (videocodecResident.ownerProcessIds[1] == pid)
			owner = 1;
		else if (videocodecResident.ownerProcessIds[2] == pid)
			owner = 2;
		else if (videocodecResident.ownerProcessIds[3] == pid)
			owner = 3;
		else
			goto out;
		videocodecResident.ownerProcessIds[owner] = -1;
		if (videocodecResident.activeProcessId == pid) {
			videocodecResident.activeProcessId = -1;
			videocodecNotifyProcessChange(pid, 0);
		}
	}
out:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&videocodecResident.allMutex);
	return ret;
}


int SceAvcodecForDriver_C84AFFC4(void (*callback)(SceUID pid, int active))
{
	lockFastMutex((SceKernelFastMutex *)&videocodecResident.allMutex);
	if (videocodecResident.notifyProcessChange != 0) {
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&videocodecResident.allMutex);
		return (int)0x80620808;
	}
	videocodecResident.notifyProcessChange = callback;
	return sceKernelUnlockFastMutex((SceKernelFastMutex *)&videocodecResident.allMutex);
}

void *_GetVideocodecGlobal(SceUID pid)
{
	return videocodecGetGlobal(pid);
}

int initializeVideocodec(void)
{
	int ret;

	ret = sceKernelRegisterKPLS("SceVideodec", 7584);
	if (ret < 0)
		goto out;
	videocodecResident.processLocalStorageId = ret;
	ret = sceKernelInitializeFastMutex(
		(SceKernelFastMutex *)&videocodecResident.allMutex,
		"SceVideocodecAll", 0x8002, 0);
	if (ret < 0)
		goto out;
	ret = sceKernelInitializeFastMutex(
		(SceKernelFastMutex *)&videocodecTileRegMutex,
		"SceVideocodecTileReg", 2, 0);
	if (ret < 0) {
		sceKernelFinalizeFastMutex(
			(SceKernelFastMutex *)&videocodecResident.allMutex);
		goto out;
	}
	videocodecResident.activeProcessId = -1;
	videocodecResident.ownerProcessIds[0] = -1;
	videocodecResident.ownerProcessIds[1] = -1;
	videocodecResident.ownerProcessIds[2] = -1;
	videocodecResident.ownerProcessIds[3] = -1;
	videocodecResident.notifyProcessChange = 0;
out:
	return ret;
}

int _sceVideocodecProcessCreateEntry(SceUID pid, SceUInt32 type)
{
	VideocodecGlobal *global;
	void *storage = NULL;
	unsigned int i;
	int ret;

	ret = sceKernelGetRemoteKPLS(pid, videocodecResident.processLocalStorageId, &storage, 1);
	if (ret < 0)
		return ret;
#if FW_DEBUG
	sceKernelAssert(storage != NULL, &videocodecCreateStorage, __builtin_return_address(0));
#endif
	global = storage;
	if (global == NULL)
		return SCE_VIDEODEC_ERROR_OUT_OF_MEMORY;
	memset(global, 0, sizeof(*global));
	global->pAllMutex = &videocodecResident.allMutex;
	global->avcdecInfo.horizontal = 1920;
	global->avcdecInfo.vertical = 1088;
	global->avcdecInfo.numOfRefFrames = 4;
	global->avcdecInfo.numOfStreams = 0;
	global->avcdecFlags = 0;
	for (i = 0; i < 4; i++) {
		VideocodecAvcDecoder *decoder = &global->avcDecoders[i];

		decoder->info.horizontal = 1920;
		decoder->info.vertical = 1088;
		decoder->info.numOfRefFrames = 4;
		decoder->mode = 0;
		decoder->handle = 0;
		decoder->frameBufFlags = 0;
		memset(decoder->decodeState, 0, sizeof(decoder->decodeState));
		__builtin_memset(&decoder->frameBuf, 0, sizeof(decoder->frameBuf));
		decoder->frameBufMemBlockId = 0;
		videocodecInitLockedRanges(&decoder->lockedRanges, 0, decoder->lockedRange, 1,
			VIDEOCODEC_DECODER_ERROR_BASE);
	}
	global->dsp0Flags = 0;
	global->dsp0DecoderInfo.horizontal = 1280;
	global->dsp0DecoderInfo.vertical = 720;
	global->dsp0DecoderInfo.numOfRefFrames = 9;
	global->dsp0DecoderInfo.numOfStreams = 0;
	global->dsp0EncoderInfo.horizontal = 960;
	global->dsp0EncoderInfo.vertical = 576;
	global->dsp0EncoderInfo.numOfRefFrames = 1;
	global->dsp0EncoderInfo.numOfStreams = 0;
	global->dsp0Library = 0;
	global->availablePreset = 0;
	global->dsp0Config = 0;
	for (i = 0; i < 4; i++) {
		VideocodecDsp0Decoder *decoder = &global->dsp0Decoders[i];

		decoder->info.horizontal = 1280;
		decoder->info.vertical = 720;
		decoder->info.numOfRefFrames = 9;
		decoder->mode = 0;
		decoder->handle = 0;
		decoder->frameBufFlags = 0;
		__builtin_memset(&decoder->pOutput, 0, sizeof(decoder->pOutput));
		decoder->frameBufMemBlockId = 0;
		__builtin_memset(&decoder->frameBuf, 0, sizeof(decoder->frameBuf));
		videocodecInitLockedRanges(&decoder->lockedRanges, 0, decoder->lockedRange, 1,
			VIDEOCODEC_DECODER_ERROR_BASE);
	}
	global->encoders[0].handle = 0;
	global->encoders[0].encodeFlags = 0;
	global->encoders[0].frameBufMemBlockId = 0;
	__builtin_memset(&global->encoders[0].frameBuf, 0, sizeof(global->encoders[0].frameBuf));
	videocodecInitLockedRanges(&global->encoders[0].lockedRanges, 0,
		global->encoders[0].lockedRange, 1, VIDEOCODEC_ENCODER_ERROR_BASE);
	return 0;
}

int _sceVideocodecProcessExitEntry(SceUID pid, SceUInt32 type)
{
	void *storage = NULL;
	int ret;

	ret = sceKernelGetRemoteKPLS(pid, videocodecResident.processLocalStorageId, &storage, 0);
	if (ret < 0)
		return ret;
#if FW_DEBUG
	sceKernelAssert(storage != NULL, &videocodecExitStorage, __builtin_return_address(0));
#endif
	videocodecTermLibraryForProcess(SCE_VIDEODEC_TYPE_HW_AVCDEC, 0, pid);
	videocodecTermLibraryForProcess(SCE_VIDEODEC_TYPE_HW_AVCDEC + 1, 0, pid);
	return 0;
}

int videocodecProcessSuspend(SceUID pid)
{
	lockFastMutex((SceKernelFastMutex *)&videocodecResident.allMutex);
	if (videocodecResident.activeProcessId == pid)
		videocodecResident.activeProcessId = -1;
	return sceKernelUnlockFastMutex((SceKernelFastMutex *)&videocodecResident.allMutex);
}

int videocodecProcessResume(SceUID pid)
{
	SceUID owner;
	int ret, unlockRet;

	ret = lockFastMutex((SceKernelFastMutex *)&videocodecResident.allMutex);
	if ((owner = videocodecResident.ownerProcessIds[0]) == pid ||
	    (owner = videocodecResident.ownerProcessIds[1]) == pid ||
	    (owner = videocodecResident.ownerProcessIds[2]) == pid ||
	    (owner = videocodecResident.ownerProcessIds[3]) == pid) {
		if (videocodecResident.activeProcessId == -1 ||
		    videocodecResident.activeProcessId == owner)
			videocodecResident.activeProcessId = owner;
		else
			ret = (int)0x80620808;
	}
	unlockRet = sceKernelUnlockFastMutex(
		(SceKernelFastMutex *)&videocodecResident.allMutex);
	return unlockRet < 0 ? unlockRet : ret;
}

int videocodecValidatePublicDecoderInfo(unsigned int codecType,
		const SceAvcdecQueryDecoderInfo *pDecoderInfo);

int SceAvcodecForDriver_536C8296(unsigned int codecType,
		const SceVideodecQueryInitInfo *pInitInfo, SceVideodecMemInfo *pMemInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceAvcdecQueryDecoderInfo decoderInfo;
	int ret;

	if (codecType != SCE_VIDEODEC_TYPE_HW_AVCDEC) {
		ret = SCE_VIDEODEC_ERROR_INVALID_PARAM;
	} else if (pInitInfo == NULL || pMemInfo == NULL) {
		ret = SCE_VIDEODEC_ERROR_INVALID_POINTER;
	} else {
		decoderInfo.horizontal = pInitInfo->hwAvc.horizontal;
		decoderInfo.vertical = pInitInfo->hwAvc.vertical;
		decoderInfo.numOfRefFrames = pInitInfo->hwAvc.numOfRefFrames;
		ret = videocodecConvertCodecEngineError(
			videocodecValidatePublicDecoderInfo(codecType, &decoderInfo));
		if (ret >= 0)
			ret = videocodecQueryMemSizeInline(codecType, pInitInfo, NULL, pMemInfo);
	}
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_FACCD284(unsigned int codecType,
		const SceVideodecQueryInitInfo *pInitInfo, SceVideodecMemInfo *pMemInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceAvcdecQueryDecoderInfo decoderInfo;
	int ret;

	if (codecType != SCE_VIDEODEC_TYPE_HW_AVCDEC) {
		ret = SCE_VIDEODEC_ERROR_INVALID_PARAM;
	} else if (pInitInfo == NULL || pMemInfo == NULL) {
		ret = SCE_VIDEODEC_ERROR_INVALID_POINTER;
	} else {
		decoderInfo.horizontal = pInitInfo->hwAvc.horizontal;
		decoderInfo.vertical = pInitInfo->hwAvc.vertical;
		decoderInfo.numOfRefFrames = pInitInfo->hwAvc.numOfRefFrames;
		ret = videocodecConvertCodecEngineError(
			videocodecValidateDecoderInfo(codecType, &decoderInfo));
		if (ret >= 0)
			ret = videocodecQueryMemSizeInline(codecType, pInitInfo, NULL, pMemInfo);
	}
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_22D7B653(unsigned int codecType,
		const SceVideodecQueryInitInfo *pInitInfo, SceVideodecMemInfo *pMemInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	if (codecType - SCE_VIDEODEC_TYPE_HW_AVCDEC > 1)
		ret = SCE_VIDEODEC_ERROR_INVALID_PARAM;
	else if (pInitInfo == NULL || pMemInfo == NULL)
		ret = SCE_VIDEODEC_ERROR_INVALID_POINTER;
	else
		ret = videocodecQueryMemSizeInline(codecType, pInitInfo, NULL, pMemInfo);
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_86B1ACC5(unsigned int codecType,
		const SceVideodecQueryInitInfo *pInitInfo,
		const SceVideodecQueryInitInfo *pEncoderInfo, SceVideodecMemInfo *pMemInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	if (codecType != SCE_VIDEODEC_TYPE_HW_AVCDEC + 1) {
		ret = VIDEOCODEC_ENCODER_ERROR_INVALID_PARAM;
	} else if (pEncoderInfo == NULL || pInitInfo == NULL || pMemInfo == NULL) {
		ret = VIDEOCODEC_ENCODER_ERROR_INVALID_POINTER;
	} else {
		ret = videocodecQueryMemSize(codecType, pInitInfo, pEncoderInfo, pMemInfo);
		if (((unsigned int)ret & 0xFFFFFF00) == 0x80620800)
			ret |= 0x1000;
	}
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_32DEE3C5(unsigned int codecType,
		const SceVideodecQueryInitInfo *pInitInfo,
		const SceVideodecQueryInitInfo *pEncoderInfo, SceVideodecMemInfo *pMemInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret;

	if (codecType != SCE_VIDEODEC_TYPE_HW_AVCDEC + 1) {
		ret = VIDEOCODEC_ENCODER_ERROR_INVALID_PARAM;
	} else if (pInitInfo != NULL || pEncoderInfo == NULL || pMemInfo == NULL) {
		ret = VIDEOCODEC_ENCODER_ERROR_INVALID_POINTER;
	} else {
		SceVideodecQueryInitInfo initInfo = {
			.hwAvc = {
				.size = sizeof(initInfo.hwAvc),
				.horizontal = pEncoderInfo->hwAvc.horizontal,
				.vertical = pEncoderInfo->hwAvc.vertical,
				.numOfRefFrames = 1,
				.numOfStreams = 1,
			},
		};

		ret = videocodecQueryMemSize(codecType, &initInfo, pEncoderInfo, pMemInfo);
		if (((unsigned int)ret & 0xFFFFFF00) == 0x80620800)
			ret |= 0x1000;
	}
	sceSyscallLeave(syscall);
	return ret;
}

int videocodecFreeMemBlock(VideocodecMemBlock *pMemBlock)
{
	sceKernelFreeMemBlock(pMemBlock->memBlockId);
	pMemBlock->memBlockId = 0;
	return 0;
}

int videocodecAllocMemBlock(const char *name, SceKernelMemBlockType type, SceSize alignment,
		SceSize size, VideocodecMemBlock *pMemBlock)
{
	SceKernelAllocMemBlockKernelOpt opt;
	void *base;
	SceUID uid;
	int ret;

	if (!sceKernelIsGameBudget())
		return VIDEOCODEC_ERROR_NOT_GAME_PROCESS;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.alignment = alignment;
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT |
		   SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
	ret = sceKernelAllocMemBlockForKernel(name, type, size, &opt);
	uid = ret;
	if (ret < 0)
		return ret;
	ret = sceKernelGetMemBlockBase(uid, &base);
	if (ret < 0)
		return ret;
	pMemBlock->size = size;
	pMemBlock->kind = VIDEOCODEC_MEMORY_BLOCK;
	pMemBlock->memBlockId = uid;
	pMemBlock->base = base;
	return ret;
}

/*
 * Initializes a library of codecType in pCtrl's memory (or a new block), for pInitInfo and, on
 * the DSP0, pEncoderInfo. The query runs out of line or, where the caller inlines it, inline.
 */
static inline __attribute__((always_inline)) int
videocodecInitLibraryWithUnmapMemInline(unsigned int codecType, const SceVideodecCtrl *pCtrl,
		const SceVideodecQueryInitInfo *pInitInfo,
		const SceVideodecQueryInitInfo *pEncoderInfo, int inlineQuery, int inlineMutexes)
{
	VideocodecGlobal *global = videocodecGetGlobal(0);
	SceUID pid = sceKernelGetProcessId();
	VideocodecSlotCounts counts = {0, 0, 0};
	VideocodecSlotCounts initialized;
	SceVideodecMemInfo memInfo;
	Dsp0VideocodecInitParam param;
	Dsp0VideocodecLibraryMemory libraryMemory;
	VideocodecMemBlock memBlock;
	unsigned int horizontal, vertical, numOfRefFrames, numOfStreams;
	unsigned int maxRefFrames, maxHorizontal, maxVertical;
	unsigned int encoderHorizontal, encoderVertical, hasEncoder;
	unsigned int library, flags, i;
	int isDsp0, owner, ret;
	void *base;

	if (inlineQuery)
		ret = videocodecQueryMemSizeInline(codecType, pInitInfo, pEncoderInfo, &memInfo);
	else
		ret = videocodecQueryMemSize(codecType, pInitInfo, pEncoderInfo, &memInfo);
	if (ret < 0)
		return ret;
	if (codecType == SCE_VIDEODEC_TYPE_HW_AVCDEC) {
		if (pInitInfo->hwAvc.size != sizeof(SceVideodecQueryInitInfoHwAvcdec))
			return SCE_VIDEODEC_ERROR_INVALID_ARGUMENT_SIZE;
		horizontal = pInitInfo->hwAvc.horizontal;
		vertical = pInitInfo->hwAvc.vertical;
		isDsp0 = 0;
		numOfStreams = pInitInfo->hwAvc.numOfStreams;
		numOfRefFrames = pInitInfo->hwAvc.numOfRefFrames;
		if (numOfStreams == 0)
			numOfStreams = 1;
		if (numOfRefFrames == 0)
			numOfRefFrames = 1;
		maxRefFrames = 0xC00000 / ((3 * vertical * horizontal) >> 1);
		counts.numOfAvcDecoders = numOfStreams;
		if (maxRefFrames >= 16)
			maxRefFrames = 16;
		maxVertical = 1088;
		maxHorizontal = 1920;
	} else {
		if (pInitInfo->hwAvc.size != sizeof(SceVideodecQueryInitInfoHwAvcdec))
			return SCE_VIDEODEC_ERROR_INVALID_ARGUMENT_SIZE;
		horizontal = pInitInfo->hwAvc.horizontal;
		vertical = pInitInfo->hwAvc.vertical;
		isDsp0 = 1;
		numOfStreams = pInitInfo->hwAvc.numOfStreams;
		numOfRefFrames = pInitInfo->hwAvc.numOfRefFrames;
		if (numOfRefFrames == 0)
			numOfRefFrames = 1;
		if (numOfStreams == 0)
			numOfStreams = 1;
		maxRefFrames = 0x697800 / ((3 * vertical * horizontal) >> 1);
		maxVertical = 720;
		if (maxRefFrames >= 16)
			maxRefFrames = 16;
		maxHorizontal = 1280;
		counts.numOfDsp0Decoders = numOfStreams;
	}
	if (horizontal > maxHorizontal || horizontal < 64)
		return SCE_VIDEODEC_ERROR_INVALID_PARAM;
	if (vertical > maxVertical || vertical < 64)
		return SCE_VIDEODEC_ERROR_INVALID_PARAM;
	if (numOfRefFrames > maxRefFrames || numOfStreams > 4)
		return SCE_VIDEODEC_ERROR_INVALID_PARAM;
	if (pEncoderInfo == NULL) {
		hasEncoder = 0;
		encoderVertical = 576;
		encoderHorizontal = 960;
	} else {
		unsigned int encoderRefFrames, encoderStreams;

		if (pEncoderInfo->hwAvc.size != sizeof(SceVideodecQueryInitInfoHwAvcdec))
			return SCE_VIDEODEC_ERROR_INVALID_ARGUMENT_SIZE;
		encoderHorizontal = pEncoderInfo->hwAvc.horizontal;
		encoderRefFrames = pEncoderInfo->hwAvc.numOfRefFrames;
		encoderStreams = pEncoderInfo->hwAvc.numOfStreams;
		encoderVertical = pEncoderInfo->hwAvc.vertical;
		if (encoderRefFrames == 0)
			encoderRefFrames = 1;
		if (encoderStreams == 0)
			encoderStreams = 1;
		if (encoderHorizontal - 64 > 896 || encoderVertical - 64 > 512)
			return SCE_VIDEODEC_ERROR_INVALID_PARAM;
		if (vertical < encoderVertical || horizontal < encoderHorizontal)
			return SCE_VIDEODEC_ERROR_INVALID_PARAM;
		if (encoderStreams > 1 || encoderRefFrames > 1)
			return SCE_VIDEODEC_ERROR_INVALID_PARAM;
		counts.numOfEncoders = 1;
		hasEncoder = 1;
	}

	ret = lockFastMutex((SceKernelFastMutex *)&videocodecResident.allMutex);
	if (ret < 0)
		return ret;
	if (videocodecResident.ownerProcessIds[0] == pid)
		owner = 0;
	else if (videocodecResident.ownerProcessIds[1] == pid)
		owner = 1;
	else if (videocodecResident.ownerProcessIds[2] == pid)
		owner = 2;
	else if (videocodecResident.ownerProcessIds[3] == pid)
		owner = 3;
	else if (videocodecResident.ownerProcessIds[0] == -1)
		owner = 0;
	else if (videocodecResident.ownerProcessIds[1] == -1)
		owner = 1;
	else if (videocodecResident.ownerProcessIds[2] == -1)
		owner = 2;
	else if (videocodecResident.ownerProcessIds[3] == -1)
		owner = 3;
	else
		goto alreadyUsed;
	if (videocodecResident.activeProcessId != -1 && pid != videocodecResident.activeProcessId)
		goto alreadyUsed;

	if (!isDsp0) {
		if (global->avcdecInitCount > 0)
			goto alreadyUsed;
		flags = 0;
		ret = videocodecAllocAvcdecMemory(global, pCtrl, &memInfo);
		if (ret < 0)
			goto release;
		flags = 1;
		if (inlineMutexes)
			ret = videocodecInitAvcDecoderMutexesInline(global, &counts);
		else
			ret = videocodecInitAvcDecoderMutexes(global, &counts);
		if (ret < 0)
			goto release;
		global->avcdecInfo.horizontal = horizontal;
		global->avcdecInfo.vertical = vertical;
		global->avcdecInfo.numOfRefFrames = numOfRefFrames;
		global->avcdecInfo.numOfStreams = numOfStreams;
		global->avcdecInitCount++;
	} else {
		VideocodecMemory *memory = &global->dsp0Memory;

		if (global->dsp0InitCount > 0)
			goto alreadyUsed;
		param.numOfEncoders = pEncoderInfo != NULL ? hasEncoder : 0;
		param.horizontal = horizontal;
		param.vertical = vertical;
		param.numOfRefFrames = numOfRefFrames;
		param.numOfStreams = numOfStreams;
		memory->size = memInfo.memSize;
		memory->kind = VIDEOCODEC_MEMORY_BLOCK;
		flags = 0;
		if (pCtrl->memBufUid != 0 || pCtrl->vaContext != 0) {
			if (pCtrl->memBuf.pBuf != NULL || pCtrl->memBuf.size != 0 ||
			    memInfo.memSize > pCtrl->contextSize) {
				ret = SCE_VIDEODEC_ERROR_INVALID_PARAM;
				goto release;
			}
			if ((pCtrl->vaContext & 0xFF) != 0) {
				ret = SCE_VIDEODEC_ERROR_INVALID_POINTER;
				goto release;
			}
			ret = sceCodecEngineRegisterUnmapMemory(0, pCtrl->vaContext,
				pCtrl->contextSize);
			if (ret < 0)
				goto release;
			ret = sceCodecEngineUnmapUserVAtoKernelVA(0, (const void *)pCtrl->vaContext,
				&base);
			if (ret < 0) {
				sceCodecEngineUnregisterUnmapMemory(0, pCtrl->vaContext,
					pCtrl->contextSize);
				goto release;
			}
			memory->memBlockId = 0;
			memory->kind = VIDEOCODEC_MEMORY_UNMAP;
			memory->base = base;
			memory->size = pCtrl->contextSize;
		} else {
			ret = videocodecAllocMemBlock("SceVideocodecSwMemBuf_Phycont",
				SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_PHYCONT_NC_RW, 0x40000,
				(memInfo.memSize + 0x3FFFF) >> 18 << 18, &memBlock);
			if (ret < 0)
				goto release;
			memory->size = memBlock.size;
			memory->kind = VIDEOCODEC_MEMORY_BLOCK;
			memory->base = memBlock.base;
			memory->memBlockId = memBlock.memBlockId;
		}
		if (memory->base == NULL)
			goto out;
		libraryMemory.base = memory->base;
		libraryMemory.size = memory->size;
		libraryMemory.param = &param;
		initialized.numOfAvcDecoders = 0;
		initialized.numOfDsp0Decoders = 0;
		initialized.numOfEncoders = 0;
		flags = 1;
		for (i = 0; i < counts.numOfDsp0Decoders; i++) {
			videocodecSwDecoderMutexName[14] = '0' + i % 10;
			ret = sceKernelInitializeFastMutex(
				(SceKernelFastMutex *)&global->dsp0Decoders[i].mutex,
				videocodecSwDecoderMutexName, 0x8002, 0);
			if (ret < 0) {
				videocodecFinalizeDsp0Mutexes(global, &initialized.numOfDsp0Decoders,
						initialized.numOfEncoders);
				goto release;
			}
			initialized.numOfDsp0Decoders++;
		}
		for (i = 0; i < counts.numOfEncoders; i++) {
			videocodecSwEncoderMutexName[14] = '0' + i % 10;
			ret = sceKernelInitializeFastMutex(
				(SceKernelFastMutex *)&global->encoders[i].mutex,
				videocodecSwEncoderMutexName, 0x8002, 0);
			if (ret < 0) {
				videocodecFinalizeDsp0Mutexes(global, &initialized.numOfDsp0Decoders,
						initialized.numOfEncoders);
				goto release;
			}
			initialized.numOfEncoders++;
		}
		flags = 3;
		ret = videocodecConvertDecoderError(
			dsp0VideocodecInitLibrary(&library, &libraryMemory));
		if (ret < 0)
			goto release;
		global->dsp0InitCount++;
		global->dsp0Library = library;
		global->dsp0DecoderInfo.horizontal = horizontal;
		global->dsp0DecoderInfo.vertical = vertical;
		global->dsp0DecoderInfo.numOfRefFrames = numOfRefFrames;
		global->dsp0DecoderInfo.numOfStreams = numOfStreams;
		global->dsp0EncoderInfo.horizontal = encoderHorizontal;
		global->dsp0EncoderInfo.vertical = encoderVertical;
		global->dsp0EncoderInfo.numOfRefFrames = 1;
		global->dsp0EncoderInfo.numOfStreams = hasEncoder;
	}
	videocodecResident.ownerProcessIds[owner] = pid;
	if (videocodecResident.activeProcessId == -1) {
		videocodecResident.activeProcessId = pid;
		videocodecNotifyProcessChange(pid, 1);
	}
	ret = 0;
	goto out;

alreadyUsed:
	ret = SCE_VIDEODEC_ERROR_ALREADY_USED;
	flags = 0;
release:
	videocodecReleaseLibrary(isDsp0, flags, pid, &counts);
out:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&videocodecResident.allMutex);
	return ret;
}

__attribute__((noinline))
int videocodecInitLibraryWithUnmapMem(unsigned int codecType, const SceVideodecCtrl *pCtrl,
		const SceVideodecQueryInitInfo *pInitInfo,
		const SceVideodecQueryInitInfo *pEncoderInfo)
{
	return videocodecInitLibraryWithUnmapMemInline(codecType, pCtrl, pInitInfo, pEncoderInfo,
		0, 0);
}

int SceAvcodecForDriver_F9E271DE(unsigned int codecType, VideocodecInstanceInfo *pInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global = videocodecGetGlobal(0);
	VideocodecSlotCounts counts = {0, 0, 0};
	VideocodecSlotCounts locked;
	unsigned int numOfInstances;
	int ret, unlockRet;

	if (codecType != SCE_VIDEODEC_TYPE_HW_AVCDEC) {
		ret = SCE_VIDEODEC_ERROR_INVALID_PARAM;
		goto leave;
	}
	if (pInfo == NULL) {
		ret = SCE_VIDEODEC_ERROR_INVALID_POINTER;
		goto leave;
	}
	if (pInfo->size != sizeof(*pInfo)) {
		ret = SCE_VIDEODEC_ERROR_INVALID_ARGUMENT_SIZE;
		goto leave;
	}
	ret = lockFastMutex((SceKernelFastMutex *)&videocodecResident.allMutex);
	if (ret < 0)
		goto leave;
	counts.numOfAvcDecoders = global->avcdecInfo.numOfStreams;
	locked.numOfAvcDecoders = 0;
	locked.numOfDsp0Decoders = 0;
	locked.numOfEncoders = 0;
	ret = videocodecLockAvcDecoders(global, &counts, &locked);
	if (ret < 0)
		goto unlockAll;
	pInfo->horizontal = 0;
	pInfo->vertical = 0;
	pInfo->numOfRefFrames = 0;
	pInfo->numOfStreams = 0;
	pInfo->memoryType = 0;
	pInfo->numOfInstances = 0;
	if (global->avcdecInitCount == 0) {
		ret = SCE_VIDEODEC_ERROR_NOT_INITIALIZE;
	} else {
		pInfo->memoryType = VIDEOCODEC_INSTANCE_MEMORY_BLOCK;
		if (global->avcdecMemory.kind == VIDEOCODEC_MEMORY_UNMAP)
			pInfo->memoryType = VIDEOCODEC_INSTANCE_MEMORY_UNMAP;
		numOfInstances = global->avcDecoders[0].handle != 0;
		if (global->avcDecoders[1].handle != 0)
			numOfInstances++;
		if (global->avcDecoders[2].handle != 0)
			numOfInstances++;
		if (global->avcDecoders[3].handle != 0)
			numOfInstances++;
		pInfo->horizontal = global->avcdecInfo.horizontal;
		pInfo->vertical = global->avcdecInfo.vertical;
		pInfo->numOfRefFrames = global->avcdecInfo.numOfRefFrames;
		pInfo->numOfStreams = global->avcdecInfo.numOfStreams;
		pInfo->numOfInstances = numOfInstances;
	}
	videocodecUnlockAvcDecoders(global, &counts);
unlockAll:
	unlockRet = sceKernelUnlockFastMutex((SceKernelFastMutex *)&videocodecResident.allMutex);
	if (unlockRet < 0)
		return unlockRet;
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_95E8AFCA(unsigned int codecType,
		const SceVideodecQueryInitInfo *pInitInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceAvcdecQueryDecoderInfo decoderInfo;
	SceVideodecCtrl ctrl;
	int ret;

	if (codecType != SCE_VIDEODEC_TYPE_HW_AVCDEC) {
		ret = SCE_VIDEODEC_ERROR_INVALID_PARAM;
	} else if (pInitInfo == NULL) {
		ret = SCE_VIDEODEC_ERROR_INVALID_POINTER;
	} else {
		decoderInfo.horizontal = pInitInfo->hwAvc.horizontal;
		decoderInfo.vertical = pInitInfo->hwAvc.vertical;
		decoderInfo.numOfRefFrames = pInitInfo->hwAvc.numOfRefFrames;
		ret = videocodecConvertCodecEngineError(
			videocodecValidatePublicDecoderInfo(codecType, &decoderInfo));
		if (ret >= 0) {
			ctrl.memBuf.pBuf = NULL;
			ctrl.memBuf.size = 0;
			ctrl.memBufUid = 0;
			ctrl.vaContext = 0;
			ctrl.contextSize = 0;
			ret = videocodecInitLibraryWithUnmapMemInline(codecType, &ctrl, pInitInfo,
				NULL, 1, 0);
		}
	}
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_4DD3826D(unsigned int codecType,
		const SceVideodecQueryInitInfo *pInitInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceAvcdecQueryDecoderInfo decoderInfo;
	SceVideodecCtrl ctrl;
	int ret;

	if (codecType != SCE_VIDEODEC_TYPE_HW_AVCDEC) {
		ret = SCE_VIDEODEC_ERROR_INVALID_PARAM;
	} else if (pInitInfo == NULL) {
		ret = SCE_VIDEODEC_ERROR_INVALID_POINTER;
	} else {
		decoderInfo.horizontal = pInitInfo->hwAvc.horizontal;
		decoderInfo.vertical = pInitInfo->hwAvc.vertical;
		decoderInfo.numOfRefFrames = pInitInfo->hwAvc.numOfRefFrames;
		ret = videocodecConvertCodecEngineError(
			videocodecValidateDecoderInfo(codecType, &decoderInfo));
		if (ret >= 0) {
			ctrl.memBuf.pBuf = NULL;
			ctrl.memBuf.size = 0;
			ctrl.memBufUid = 0;
			ctrl.vaContext = 0;
			ctrl.contextSize = 0;
			ret = videocodecInitLibraryWithUnmapMemInline(codecType, &ctrl, pInitInfo,
				NULL, 1, 0);
		}
	}
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_4AE6B437(unsigned int codecType,
		const SceVideodecQueryInitInfo *pInitInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceVideodecCtrl ctrl;
	int ret;

	if (codecType - SCE_VIDEODEC_TYPE_HW_AVCDEC > 1) {
		ret = SCE_VIDEODEC_ERROR_INVALID_PARAM;
	} else if (pInitInfo == NULL) {
		ret = SCE_VIDEODEC_ERROR_INVALID_POINTER;
	} else {
		ctrl.memBuf.pBuf = NULL;
		ctrl.memBuf.size = 0;
		ctrl.memBufUid = 0;
		ctrl.vaContext = 0;
		ctrl.contextSize = 0;
		ret = videocodecInitLibraryWithUnmapMemInline(codecType, &ctrl, pInitInfo, NULL,
			1, 1);
	}
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_C93DA0C7(unsigned int codecType, SceVideodecCtrl *pCtrl,
		const SceVideodecQueryInitInfo *pInitInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceAvcdecQueryDecoderInfo decoderInfo;
	SceVideodecCtrl ctrl;
	int ret;

	if (codecType != SCE_VIDEODEC_TYPE_HW_AVCDEC) {
		ret = SCE_VIDEODEC_ERROR_INVALID_PARAM;
	} else if (pInitInfo == NULL || pCtrl == NULL) {
		ret = SCE_VIDEODEC_ERROR_INVALID_POINTER;
	} else {
		decoderInfo.horizontal = pInitInfo->hwAvc.horizontal;
		decoderInfo.vertical = pInitInfo->hwAvc.vertical;
		decoderInfo.numOfRefFrames = pInitInfo->hwAvc.numOfRefFrames;
		ret = videocodecConvertCodecEngineError(
			videocodecValidatePublicDecoderInfo(codecType, &decoderInfo));
		if (ret >= 0) {
			ctrl.memBuf.pBuf = NULL;
			ctrl.memBuf.size = 0;
			ctrl.memBufUid = 0;
			ctrl.vaContext = pCtrl->vaContext;
			ctrl.contextSize = pCtrl->contextSize;
			ret = videocodecInitLibraryWithUnmapMemInline(codecType, &ctrl, pInitInfo,
				NULL, 0, 0);
		}
	}
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_6DC39EDC(unsigned int codecType, SceVideodecCtrl *pCtrl,
		const SceVideodecQueryInitInfo *pInitInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceAvcdecQueryDecoderInfo decoderInfo;
	SceVideodecCtrl ctrl;
	int ret;

	if (codecType != SCE_VIDEODEC_TYPE_HW_AVCDEC) {
		ret = SCE_VIDEODEC_ERROR_INVALID_PARAM;
	} else if (pInitInfo == NULL || pCtrl == NULL) {
		ret = SCE_VIDEODEC_ERROR_INVALID_POINTER;
	} else {
		decoderInfo.horizontal = pInitInfo->hwAvc.horizontal;
		decoderInfo.vertical = pInitInfo->hwAvc.vertical;
		decoderInfo.numOfRefFrames = pInitInfo->hwAvc.numOfRefFrames;
		ret = videocodecConvertCodecEngineError(
			videocodecValidateDecoderInfo(codecType, &decoderInfo));
		if (ret >= 0) {
			ctrl.memBuf.pBuf = NULL;
			ctrl.memBuf.size = 0;
			ctrl.memBufUid = 0;
			ctrl.vaContext = pCtrl->vaContext;
			ctrl.contextSize = pCtrl->contextSize;
			ret = videocodecInitLibraryWithUnmapMemInline(codecType, &ctrl, pInitInfo,
				NULL, 0, 0);
		}
	}
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_BC55D10F(unsigned int codecType, SceVideodecCtrl *pCtrl,
		const SceVideodecQueryInitInfo *pInitInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceVideodecCtrl ctrl;
	int ret;

	if (codecType - SCE_VIDEODEC_TYPE_HW_AVCDEC > 1) {
		ret = SCE_VIDEODEC_ERROR_INVALID_PARAM;
	} else if (pInitInfo == NULL || pCtrl == NULL) {
		ret = SCE_VIDEODEC_ERROR_INVALID_POINTER;
	} else {
		ctrl.memBuf.pBuf = NULL;
		ctrl.memBuf.size = 0;
		ctrl.memBufUid = 0;
		ctrl.vaContext = pCtrl->vaContext;
		ctrl.contextSize = pCtrl->contextSize;
		ret = videocodecInitLibraryWithUnmapMemInline(codecType, &ctrl, pInitInfo, NULL,
			1, 1);
	}
	sceSyscallLeave(syscall);
	return ret;
}

/* The video encoder's errors are the decoder's with 0x1000 set. */
static inline int videocodecToEncoderError(int ret)
{
	if (((unsigned int)ret & 0xFFFFFF00) == 0x80620800)
		ret |= 0x1000;
	return ret;
}

int SceAvcodecForDriver_E782E735(unsigned int codecType,
		const SceVideodecQueryInitInfo *pInitInfo,
		const SceVideodecQueryInitInfo *pEncoderInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceVideodecQueryInitInfo initInfo;
	SceVideodecCtrl ctrl;
	int ret;

	if (codecType != 0x1002) {
		ret = VIDEOCODEC_ENCODER_ERROR_INVALID_PARAM;
	} else if (pInitInfo != NULL || pEncoderInfo == NULL) {
		ret = VIDEOCODEC_ENCODER_ERROR_INVALID_POINTER;
	} else {
		initInfo = (SceVideodecQueryInitInfo){
			.hwAvc = {
				.size = sizeof(initInfo.hwAvc),
				.horizontal = pEncoderInfo->hwAvc.horizontal,
				.vertical = pEncoderInfo->hwAvc.vertical,
				.numOfRefFrames = 1,
				.numOfStreams = 1,
			},
		};
		ctrl.memBuf.pBuf = NULL;
		ctrl.memBuf.size = 0;
		ctrl.memBufUid = 0;
		ctrl.vaContext = 0;
		ctrl.contextSize = 0;
		ret = videocodecToEncoderError(videocodecInitLibraryWithUnmapMem(codecType, &ctrl,
			&initInfo, pEncoderInfo));
	}
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_04F734CC(unsigned int codecType,
		const SceVideodecQueryInitInfo *pInitInfo,
		const SceVideodecQueryInitInfo *pEncoderInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceVideodecCtrl ctrl;
	int ret;

	if (codecType != 0x1002) {
		ret = VIDEOCODEC_ENCODER_ERROR_INVALID_PARAM;
	} else {
		/* Sony returns here without leaving the syscall */
		if (pEncoderInfo == NULL || pInitInfo == NULL)
			return SCE_VIDEODEC_ERROR_INVALID_POINTER;
		ctrl.memBuf.pBuf = NULL;
		ctrl.memBuf.size = 0;
		ctrl.memBufUid = 0;
		ctrl.vaContext = 0;
		ctrl.contextSize = 0;
		ret = videocodecToEncoderError(videocodecInitLibraryWithUnmapMem(codecType, &ctrl,
			pInitInfo, pEncoderInfo));
	}
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_CD7FE8D0(unsigned int codecType, const SceVideodecCtrl *pCtrl,
		const SceVideodecQueryInitInfo *pInitInfo,
		const SceVideodecQueryInitInfo *pEncoderInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceVideodecQueryInitInfo initInfo;
	SceVideodecCtrl ctrl;
	int ret;

	if (codecType != 0x1002) {
		ret = VIDEOCODEC_ENCODER_ERROR_INVALID_PARAM;
	} else if (pCtrl == NULL || pInitInfo != NULL || pEncoderInfo == NULL) {
		ret = VIDEOCODEC_ENCODER_ERROR_INVALID_POINTER;
	} else {
		initInfo = (SceVideodecQueryInitInfo){
			.hwAvc = {
				.size = sizeof(initInfo.hwAvc),
				.horizontal = pEncoderInfo->hwAvc.horizontal,
				.vertical = pEncoderInfo->hwAvc.vertical,
				.numOfRefFrames = 1,
				.numOfStreams = 1,
			},
		};
		ctrl.memBuf.pBuf = NULL;
		ctrl.memBuf.size = 0;
		ctrl.memBufUid = 0;
		ctrl.vaContext = pCtrl->vaContext;
		ctrl.contextSize = pCtrl->contextSize;
		ret = videocodecToEncoderError(videocodecInitLibraryWithUnmapMem(codecType, &ctrl,
			&initInfo, pEncoderInfo));
	}
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_BCB769EB(unsigned int codecType, const SceVideodecCtrl *pCtrl,
		const SceVideodecQueryInitInfo *pInitInfo,
		const SceVideodecQueryInitInfo *pEncoderInfo)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceVideodecCtrl ctrl;
	int ret;

	if (codecType != 0x1002) {
		ret = VIDEOCODEC_ENCODER_ERROR_INVALID_PARAM;
	} else if (pInitInfo == NULL || pCtrl == NULL || pEncoderInfo == NULL) {
		ret = VIDEOCODEC_ENCODER_ERROR_INVALID_POINTER;
	} else {
		ctrl.memBuf.pBuf = NULL;
		ctrl.memBuf.size = 0;
		ctrl.memBufUid = 0;
		ctrl.vaContext = pCtrl->vaContext;
		ctrl.contextSize = pCtrl->contextSize;
		ret = videocodecToEncoderError(videocodecInitLibraryWithUnmapMem(codecType, &ctrl,
			pInitInfo, pEncoderInfo));
	}
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_2641888C(unsigned int codecType)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret = sceKernelGetProcessId();

	if (ret >= 0)
		ret = videocodecTermLibraryForProcess(codecType, 1, ret);
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_58F1E353(unsigned int codecType)
{
	SceUInt32 syscall = sceSyscallEnter();
	int ret = VIDEOCODEC_ENCODER_ERROR_INVALID_PARAM;

	if (codecType == 0x1002) {
		ret = sceKernelGetProcessId();
		if (ret >= 0)
			ret = videocodecTermLibraryForProcess(codecType, 1, ret);
		if (((unsigned int)ret & 0xFFFFFF00) == 0x80620800)
			ret |= 0x1000;
	}
	sceSyscallLeave(syscall);
	return ret;
}

/* _sceVideodecSetConfig and its Internal twin: the AVC decoder's codec-engine configuration */
static inline __attribute__((always_inline)) int
videocodecSetConfigInline(unsigned int codecType, unsigned int flags)
{
	SceUInt32 syscall = sceSyscallEnter();
	VideocodecGlobal *global = videocodecGetGlobal(0);
	SceUID pid;
	int ret;

	if (codecType != SCE_VIDEODEC_TYPE_HW_AVCDEC) {
		sceSyscallLeave(syscall);
		return SCE_VIDEODEC_ERROR_INVALID_PARAM;
	}
	pid = sceKernelGetProcessId();
	if (pid >= 0) {
		ret = lockFastMutex((SceKernelFastMutex *)&videocodecResident.allMutex);
		if (ret < 0)
			goto leave;
		if (global->avcdecInfo.numOfStreams == 0) {
			ret = videocodecConvertValidationError(
				SceCodecEngineWrapperForDriver_86861877(flags, 1, pid));
			sceKernelUnlockFastMutex((SceKernelFastMutex *)&videocodecResident.allMutex);
			goto leave;
		}
		pid = SCE_VIDEODEC_ERROR_ALREADY_USED;
	}
	/* Sony unlocks here also when it never locked (a negative process ID) */
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&videocodecResident.allMutex);
	ret = pid;
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int SceAvcodecForDriver_12DAE04A(unsigned int codecType, unsigned int flags)
{
	return videocodecSetConfigInline(codecType, flags);
}

int SceAvcodecForDriver_43985AA8(unsigned int codecType, unsigned int flags)
{
	return videocodecSetConfigInline(codecType, flags);
}

/* The MPEG-4 decoder's errors: the video codec's with 0x900 set. */
int videocodecConvertM4vdecError(int result)
{
	if (((unsigned int)result & 0xFFFFFF00) == 0x80620000)
		return result | 0x900;
	return result;
}

/* The AVC encoder's errors: the video codec's with 0x1000 set. */
int videocodecConvertAvcencError(int result)
{
	if (((unsigned int)result & 0xFFFFFF00) == 0x80620000)
		return result | 0x1000;
	return result;
}

static inline __attribute__((always_inline)) int
videocodecInitLockedRangesInline(VideocodecLockedRanges *ranges, SceUID pid,
		VideocodecLockedRange *entries, unsigned int maxRanges, int errorBase)
{
	ranges->pid = pid;
	ranges->maxRanges = maxRanges;
	ranges->ranges = entries;
	ranges->errorBase = errorBase;
	ranges->reserved[1] = 0;
	ranges->reserved[0] = 0;
	ranges->numOfRanges = 0;
	memset(entries, 0, maxRanges * sizeof(*entries));
	return 0;
}

__attribute__((noinline)) int videocodecInitLockedRanges(VideocodecLockedRanges *ranges, SceUID pid,
		VideocodecLockedRange *entries, unsigned int maxRanges, int errorBase)
{
	return videocodecInitLockedRangesInline(ranges, pid, entries, maxRanges, errorBase);
}

static inline __attribute__((always_inline)) void
videocodecUnlockRangesInline(VideocodecLockedRanges *ranges, SceUID pid)
{
	VideocodecLockedRange *range;
	unsigned char n = ranges->numOfRanges;

	if (n == 0)
		return;
	if (pid != 0) {
		do {
			range = &ranges->ranges[n - 1];
			sceKernelUnlockRangeProc(pid, (void *)range->address, range->size);
			n = ranges->numOfRanges - 1;
			ranges->numOfRanges = n;
		} while (n != 0);
	} else {
		do {
			range = &ranges->ranges[n - 1];
			sceKernelUnlockRangeWithMode(range->mode, (void *)range->address,
				range->size);
			n = ranges->numOfRanges - 1;
			ranges->numOfRanges = n;
		} while (n != 0);
	}
}

/* Unlocks every range ranges holds, last first, in its own process. */
int videocodecUnlockRanges(VideocodecLockedRanges *ranges)
{
	videocodecUnlockRangesInline(ranges, ranges->pid);
	return 0;
}

/* Unlocks every range ranges holds, last first, in process pid (0: the kernel's). */
int videocodecUnlockRangesProc(VideocodecLockedRanges *ranges, SceUID pid)
{
	videocodecUnlockRangesInline(ranges, pid);
	return 0;
}

/*
 * Locks (or, for a mode without kernel access, checks) the 256-byte aligned range around
 * [address, address + size) and returns its physical address in *pPhysicalAddress. The range
 * must lie in one physical range unless flags has VIDEOCODEC_LOCK_SCATTERED; outside the
 * uncached physical window the caller's flags allow it after cacheOp. On failure every range
 * ranges holds is unlocked.
 */
int videocodecLockRange(VideocodecLockedRanges *ranges, uintptr_t *pPhysicalAddress,
		uintptr_t address, SceSize size, int mode, int cacheOp, unsigned int flags)
{
	unsigned int offset = address & 0xFF;
	uintptr_t start = address & ~0xFFU;
	SceSize lockSize = (size + 0xFF + offset) & ~0xFFU;
	VideocodecLockedRange *range;
	SceKernelVARange vaRange;
	SceKernelPAVector vector;
	SceKernelPARange paRange;
	uintptr_t physicalAddress;
	int ret;

	if (ranges->numOfRanges >= ranges->maxRanges) {
		ret = (ranges->errorBase & ~0xFF) | 4;
		if (ranges->errorBase < 0)
			goto fail;
		ret = sceKernelIsAccessibleRange(mode, (const void *)start, lockSize);
		if (ret < 0)
			goto fail;
	} else if ((mode & (SCE_KERNEL_MEMORY_REF_PERM_KERN_R |
			    SCE_KERNEL_MEMORY_REF_PERM_KERN_W)) == 0) {
		ret = sceKernelIsAccessibleRange(mode, (const void *)start, lockSize);
		if (ret < 0)
			goto fail;
	} else {
		ret = sceKernelLockRangeWithMode(mode, (void *)start, lockSize);
		if (ret < 0)
			goto fail;
		range = &ranges->ranges[ranges->numOfRanges];
		range->mode = mode;
		range->address = start;
		range->size = lockSize;
		ranges->numOfRanges++;
		ret = sceKernelIsAccessibleRange(mode, (const void *)start, lockSize);
		if (ret < 0)
			goto fail;
	}

	vaRange.addr = start;
	vaRange.size = lockSize;
	vector.size = sizeof(vector);
	vector.capacity = 1;
	vector.nDataInVector = 0;
	vector.total = 0;
	vector.pRanges = &paRange;
	ret = sceKernelVARangeToPAVector(&vaRange, &vector);
	if (flags & VIDEOCODEC_LOCK_SCATTERED) {
		if (ret < 0)
			goto fault;
	} else if (ret < 0 || vector.total > 1) {
		goto fault;
	}
	physicalAddress = paRange.addr;
	if (physicalAddress == 0)
		goto fault;
	if (!(physicalAddress >= 0x20000000 && lockSize + physicalAddress <= 0x28000000)) {
		if (flags == 0)
			goto fault;
		switch (cacheOp) {
		case VIDEOCODEC_CACHE_INVALIDATE:
			sceKernelDcacheInvalidateRange((const void *)start, lockSize);
			break;
		case VIDEOCODEC_CACHE_CLEAN_INVALIDATE:
			sceKernelDcacheCleanInvalidateRange((const void *)start, lockSize);
			break;
		case VIDEOCODEC_CACHE_CLEAN:
			sceKernelDcacheCleanRange((const void *)start, lockSize);
			break;
		}
	}
	*pPhysicalAddress = physicalAddress + offset;
	return 0;

fault:
	ret = (ranges->errorBase & ~0xFF) | 7;
fail:
	videocodecUnlockRangesInline(ranges, ranges->pid);
	return ret;
}

/* the modes the encoder's input conversion locks with when its frame is the user's */
static const int videocodecCscUserLockModes[2] = {
	SCE_KERNEL_MEMORY_REF_PERM_KERN_R, SCE_KERNEL_MEMORY_REF_PERM_USER_W
};

/* Nine-bit fixed-point RGB to YCbCr coefficients: ITU-R BT.601, BT.709, then both full range. */
static const SceIftuCscParams videocodecCscToYuv[4] = { {
	.post_add_0 = 64,
	.post_add_1_2 = 514,
	.post_clamp_max_0 = 943,
	.post_clamp_min_0 = 64,
	.post_clamp_max_1_2 = 963,
	.post_clamp_min_1_2 = 64,
	.ctm = {
		{ 131, 258, 50 },
		{ 0xfb4, 0xf6c, 224 },
		{ 224, 0xf44, 0xfdc },
	},
}, {
	.post_add_0 = 64,
	.post_add_1_2 = 514,
	.post_clamp_max_0 = 943,
	.post_clamp_min_0 = 64,
	.post_clamp_max_1_2 = 963,
	.post_clamp_min_1_2 = 64,
	.ctm = {
		{ 93, 314, 31 },
		{ 0xfcd, 0xf53, 224 },
		{ 224, 0xf34, 0xfec },
	},
}, {
	.post_add_0 = 0,
	.post_add_1_2 = 514,
	.post_clamp_max_0 = 1023,
	.post_clamp_min_0 = 0,
	.post_clamp_max_1_2 = 1023,
	.post_clamp_min_1_2 = 0,
	.ctm = {
		{ 153, 300, 58 },
		{ 0xfaa, 0xf57, 256 },
		{ 256, 0xf2a, 0xfd7 },
	},
}, {
	.post_add_0 = 0,
	.post_add_1_2 = 514,
	.post_clamp_max_0 = 1023,
	.post_clamp_min_0 = 0,
	.post_clamp_max_1_2 = 1023,
	.post_clamp_min_1_2 = 0,
	.ctm = {
		{ 108, 366, 36 },
		{ 0xfc6, 0xf3b, 256 },
		{ 256, 0xf18, 0xfe9 },
	},
} };

/* SCE_AVCDEC_CT_TYPE_BIT_IDR_I_P_B_PIC_TYPE_FIELD values (before the shift), by the VIP's picture type */
static const unsigned char videocodecPictureTypeBits[8] = { 0, 1, 0, 3, 0, 2 };

/* the modes the encoder's input conversion locks with otherwise */
static const int videocodecCscKernelLockModes[2] = {
	SCE_KERNEL_MEMORY_REF_PERM_KERN_R, SCE_KERNEL_MEMORY_REF_PERM_KERN_W
};

/* the modes a colour conversion locks its source and its destination with */
static const int videocodecCscLockModes[2] = {
	SCE_KERNEL_MEMORY_REF_PERM_KERN_R, SCE_KERNEL_MEMORY_REF_PERM_KERN_W
};

/* Nine-bit fixed-point YCbCr to RGB coefficients, in videocodecCscToYuv's order. */
static const SceIftuCscParams videocodecCscToRgb[4] = { {
	.post_add_0 = 64,
	.post_add_1_2 = 514,
	.post_clamp_max_0 = 1023,
	.post_clamp_min_0 = 0,
	.post_clamp_max_1_2 = 0,
	.post_clamp_min_1_2 = 0,
	.ctm = {
		{ 596, 0, 817 },
		{ 596, 0xf38, 0xe60 },
		{ 596, 1032, 0 },
	},
}, {
	.post_add_0 = 64,
	.post_add_1_2 = 514,
	.post_clamp_max_0 = 1023,
	.post_clamp_min_0 = 0,
	.post_clamp_max_1_2 = 0,
	.post_clamp_min_1_2 = 0,
	.ctm = {
		{ 596, 0, 917 },
		{ 596, 0xf93, 0xef0 },
		{ 596, 1081, 0 },
	},
}, {
	.post_add_0 = 0,
	.post_add_1_2 = 514,
	.post_clamp_max_0 = 1023,
	.post_clamp_min_0 = 0,
	.post_clamp_max_1_2 = 0,
	.post_clamp_min_1_2 = 0,
	.ctm = {
		{ 512, 0, 717 },
		{ 512, 0xf50, 0xe93 },
		{ 512, 907, 0 },
	},
}, {
	.post_add_0 = 0,
	.post_add_1_2 = 514,
	.post_clamp_max_0 = 1023,
	.post_clamp_min_0 = 0,
	.post_clamp_max_1_2 = 0,
	.post_clamp_min_1_2 = 0,
	.ctm = {
		{ 512, 0, 806 },
		{ 512, 0xfa1, 0xf11 },
		{ 512, 950, 0 },
	},
} };

/* pixel formats a colour conversion takes */
#define VIDEOCODEC_CSC_IS_RGB(format) \
	((format) == SCE_AVCDEC_PIXEL_BGRA8888 || (format) <= VIDEOCODEC_PIXEL_BGRA5551)
#define VIDEOCODEC_CSC_IS_YUV(format) \
	((format) - SCE_AVCDEC_PIXEL_YUV420_RASTER <= 1 || \
	 (format) - SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER <= 1)

/*
 * Converts the picture pSrc holds into pDst's pixel format and size on the IFTU, locking
 * both for the conversion with the modes pModes gives (source, destination). An
 * interlaced source is converted one field at a time into the two halves of pDst. The
 * encoder's conversions (sourcePlanes 0) take no source with separate planes.
 */
static inline __attribute__((always_inline)) int
videocodecCscInline(VideocodecFrame *pDst, VideocodecFrame *pSrc, unsigned int cscCoefficient,
		const int *pModes, int sourcePlanes)
{
	VideocodecLockedRanges ranges;
	VideocodecLockedRange lockedRange[8];
	SceIftuConvParams params;
	SceIftuFrameBuf dstBuf;
	SceIftuPlaneState srcPlane;
	unsigned int dstType = pDst->frame.pixelType;
	unsigned int dstFormat = dstType & 0xFF;
	unsigned int dstPitch = pDst->frame.framePitch;
	unsigned int dstWidth = pDst->frame.frameWidth;
	unsigned int dstHeight = pDst->frame.frameHeight;
	uintptr_t dstPicture = (uintptr_t)pDst->frame.pPicture[0];
	unsigned int srcType = pSrc->frame.pixelType;
	unsigned int srcFormat = srcType & 0xFF;
	unsigned int srcPitch = pSrc->frame.framePitch;
	unsigned int srcWidth = pSrc->frame.frameWidth;
	unsigned int srcHeight = pSrc->frame.frameHeight;
	unsigned int srcAlignedWidth = (srcWidth + 15) & ~15U;
	unsigned int srcAlignedHeight = (srcHeight + 7) & ~7U;
	uintptr_t srcPicture = (uintptr_t)pSrc->frame.pPicture[0];
	uintptr_t srcFields = (uintptr_t)pSrc->frame.pPicture[1];
	uintptr_t srcY, srcCb = 0, srcCr = 0, dstY, dstCb = 0, dstCr = 0;
	unsigned int srcIftuFormat, dstIftuFormat, alpha;
	unsigned int srcPadding, srcChromaPadding, dstPadding, dstChromaPadding;
	SceSize srcLumaSize = 0, srcChromaSize = 0, dstLumaSize, dstChromaSize, dstSize;
	uintptr_t lockAddress;
	SceSize lockSize;
	int fields, fieldMode, halfHeight;
	int ret;

	videocodecInitLockedRangesInline(&ranges, 0, lockedRange, 8, VIDEOCODEC_DECODER_ERROR_BASE);

	alpha = 255;
	if (VIDEOCODEC_CSC_IS_RGB(dstFormat)) {
		if (dstType & SCE_AVCDEC_OPTION_ENABLE) {
			alpha = pDst->frame.opt.rgba.alpha;
			if (pDst->frame.opt.rgba.cscCoefficient != 0)
				cscCoefficient = pDst->frame.opt.rgba.cscCoefficient;
		}
		if (cscCoefficient == SCE_AVCDEC_CSC_COEFFICIENT_DEFAULT)
			cscCoefficient = (srcHeight > 576 || srcWidth > 720) ?
				SCE_AVCDEC_CSC_COEFFICIENT_ITU709 : SCE_AVCDEC_CSC_COEFFICIENT_ITU601;
	}
	if ((srcType & 0xF7) == 0) {
		if (srcType & SCE_AVCDEC_OPTION_ENABLE) {
			alpha = pSrc->frame.opt.rgba.alpha;
			if (pSrc->frame.opt.rgba.cscCoefficient != 0)
				cscCoefficient = pSrc->frame.opt.rgba.cscCoefficient;
		}
		if (cscCoefficient == SCE_AVCDEC_CSC_COEFFICIENT_DEFAULT)
			cscCoefficient = (dstHeight > 576 || dstWidth > 720) ?
				SCE_AVCDEC_CSC_COEFFICIENT_ITU709 : SCE_AVCDEC_CSC_COEFFICIENT_ITU601;
		cscCoefficient--;
	} else if (cscCoefficient != SCE_AVCDEC_CSC_COEFFICIENT_DEFAULT) {
		cscCoefficient--;
	}

	if (sourcePlanes && (srcType & VIDEOCODEC_FRAME_SEPARATE_PLANES)) {
		if (srcFormat != SCE_AVCDEC_PIXEL_YUV420_RASTER &&
		    srcFormat != SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER)
			return SCE_AVCDEC_ERROR_INVALID_COLOR_FORMAT;
		srcPicture = (uintptr_t)pSrc->pPlane[0];
		if (srcFormat == SCE_AVCDEC_PIXEL_YUV420_RASTER) {
			if (pSrc->pPlane[1] == NULL || ((uintptr_t)pSrc->pPlane[1] & 0xF) != 0 ||
			    pSrc->pPlane[2] == NULL || ((uintptr_t)pSrc->pPlane[2] & 0xF) != 0)
				return SCE_AVCDEC_ERROR_INVALID_POINTER;
		} else if (pSrc->pPlane[1] == NULL || ((uintptr_t)pSrc->pPlane[1] & 0xF) != 0) {
			return SCE_AVCDEC_ERROR_INVALID_POINTER;
		}
	}
	if (dstType & VIDEOCODEC_FRAME_SEPARATE_PLANES) {
		if (dstFormat != SCE_AVCDEC_PIXEL_YUV420_RASTER &&
		    dstFormat != SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER)
			return SCE_AVCDEC_ERROR_INVALID_COLOR_FORMAT;
		dstPicture = (uintptr_t)pDst->pPlane[0];
		if (dstFormat == SCE_AVCDEC_PIXEL_YUV420_RASTER) {
			if (pDst->pPlane[1] == NULL || ((uintptr_t)pDst->pPlane[1] & 0xF) != 0 ||
			    pDst->pPlane[2] == NULL || ((uintptr_t)pDst->pPlane[2] & 0xF) != 0)
				return SCE_AVCDEC_ERROR_INVALID_POINTER;
		} else if (pDst->pPlane[1] == NULL || ((uintptr_t)pDst->pPlane[1] & 0xF) != 0) {
			return SCE_AVCDEC_ERROR_INVALID_POINTER;
		}
	}
	if (srcPicture == 0 || (srcPicture & 0xF) != 0 || dstPicture == 0 || (dstPicture & 0xF) != 0)
		return SCE_AVCDEC_ERROR_INVALID_POINTER;

	if (!(dstType & VIDEOCODEC_FRAME_KEEP_SIZE) &&
	    srcAlignedHeight <= dstHeight && srcAlignedWidth <= dstWidth) {
		dstHeight = srcAlignedHeight;
		dstWidth = srcAlignedWidth;
	}
	if (srcAlignedWidth - 64 > 1968 || srcAlignedHeight - 64 > 1024 ||
	    srcWidth > (srcPitch & 0xFFFF) ||
	    dstHeight - 64 > 1024 || dstWidth - 64 > 1968 ||
	    dstWidth > dstPitch || dstPitch - 64 > 1968 ||
	    (dstWidth > srcAlignedWidth && dstWidth > 4 * srcAlignedWidth) ||
	    (dstWidth < srcAlignedWidth && srcAlignedWidth > 4 * dstWidth) ||
	    (dstHeight > srcAlignedHeight && dstHeight > 4 * srcAlignedHeight) ||
	    (dstHeight < srcAlignedHeight && srcAlignedHeight > 4 * dstHeight))
		return SCE_AVCDEC_ERROR_UNSUPPORT_IMAGE_SIZE;
	if ((srcFormat != VIDEOCODEC_PIXEL_PHYSICAL && srcFormat != VIDEOCODEC_PIXEL_PHYSICAL_PACKED &&
	     !VIDEOCODEC_CSC_IS_RGB(srcFormat) && !VIDEOCODEC_CSC_IS_YUV(srcFormat)) ||
	    (!VIDEOCODEC_CSC_IS_RGB(dstFormat) && !VIDEOCODEC_CSC_IS_YUV(dstFormat)) ||
	    cscCoefficient > 3)
		return SCE_AVCDEC_ERROR_INVALID_COLOR_FORMAT;

	srcPadding = (srcPitch & 0xFFFF) - srcWidth;
	halfHeight = 0;
	if (srcFormat == VIDEOCODEC_PIXEL_PHYSICAL) {
		srcY = srcPicture;
		srcIftuFormat = VIDEOCODEC_IFTU_PHYSICAL;
		srcChromaPadding = srcPadding;
		if (srcFields != 0) {
			if (dstFormat == VIDEOCODEC_PIXEL_YUV420_TILED ||
			    dstFormat == VIDEOCODEC_PIXEL_YUV420_PACKED_TILED)
				return SCE_AVCDEC_ERROR_INVALID_COLOR_FORMAT;
			fields = 1;
			fieldMode = srcFields == 2 ? 2 : 3;
			dstHeight >>= 1;
			halfHeight = 1;
		} else {
			fields = 0;
			fieldMode = 0;
		}
	} else if (srcFormat == VIDEOCODEC_PIXEL_PHYSICAL_PACKED) {
		if (srcWidth > srcPitch >> 16)
			return SCE_AVCDEC_ERROR_UNSUPPORT_IMAGE_SIZE;
		if (srcFields == 0 || (srcFields & 0xF) != 0)
			return SCE_AVCDEC_ERROR_INVALID_POINTER;
		fields = 0;
		fieldMode = 0;
		srcY = srcPicture;
		srcCb = srcFields;
		srcChromaPadding = (srcPitch >> 16) - srcWidth;
		srcIftuFormat = VIDEOCODEC_IFTU_TILED | SCE_IFTU_PIXELFORMAT_YUV420;
	} else if ((srcType & 0xF7) == 0) {
		if ((srcPitch & 3) != 0 || ((srcHeight | srcWidth) & 7) != 0 || srcFields != 0)
			return SCE_AVCDEC_ERROR_UNSUPPORT_IMAGE_SIZE;
		ret = videocodecLockRange(&ranges, &srcY, srcPicture,
			4 * (srcPitch & 0xFFFF) * srcHeight, pModes[0], 0, VIDEOCODEC_LOCK_CACHED);
		if (ret < 0)
			goto unlock;
		srcIftuFormat = srcFormat != SCE_AVCDEC_PIXEL_RGBA8888 ?
			SCE_IFTU_PIXELFORMAT_RGBX8888 : SCE_IFTU_PIXELFORMAT_BGRX8888;
		srcPadding *= 4;
		fields = 0;
		fieldMode = 0;
		srcChromaPadding = 0;
	} else if (srcFormat - 1 <= 1) {
		if ((srcPitch & 7) != 0 || ((srcHeight | srcWidth) & 7) != 0 || srcFields != 0)
			return SCE_AVCDEC_ERROR_UNSUPPORT_IMAGE_SIZE;
		ret = videocodecLockRange(&ranges, &srcY, srcPicture,
			2 * (srcPitch & 0xFFFF) * srcHeight, pModes[0], 0, VIDEOCODEC_LOCK_CACHED);
		if (ret < 0)
			goto unlock;
		srcIftuFormat = srcFormat == VIDEOCODEC_PIXEL_BGR565 ?
			SCE_IFTU_PIXELFORMAT_BGR565 : SCE_IFTU_PIXELFORMAT_BGRA5551;
		srcPadding *= 2;
		fields = 0;
		fieldMode = 0;
		srcChromaPadding = 0;
	} else {
		if (srcFields != 0) {
			srcHeight >>= 1;
			srcAlignedHeight >>= 1;
			fields = 2;
		} else {
			fields = 0;
		}
		srcLumaSize = (srcPitch & 0xFFFF) * srcHeight;
		srcChromaSize = srcLumaSize >> 2;
		if (sourcePlanes && (srcType & VIDEOCODEC_FRAME_SEPARATE_PLANES)) {
			lockAddress = (uintptr_t)pSrc->pPlane[0];
			lockSize = srcLumaSize;
		} else {
			lockAddress = srcPicture;
			lockSize = srcLumaSize + 2 * srcChromaSize;
		}
		ret = videocodecLockRange(&ranges, &srcY, lockAddress, lockSize, pModes[0], 0,
			VIDEOCODEC_LOCK_CACHED);
		if (ret < 0)
			goto unlock;
		if (srcFormat == SCE_AVCDEC_PIXEL_YUV420_RASTER) {
			if ((srcPitch & 0x1F) != 0 || (srcWidth & 0xF) != 0 || (srcHeight & 7) != 0)
				goto badSize;
			srcChromaPadding = srcPadding >> 1;
			if (sourcePlanes && (pSrc->frame.pixelType & VIDEOCODEC_FRAME_SEPARATE_PLANES)) {
				ret = videocodecLockRange(&ranges, &srcCb, (uintptr_t)pSrc->pPlane[1],
					srcChromaSize, pModes[0], 0, VIDEOCODEC_LOCK_CACHED);
				if (ret < 0)
					goto unlock;
				ret = videocodecLockRange(&ranges, &srcCr, (uintptr_t)pSrc->pPlane[2],
					srcChromaSize, pModes[0], 0, VIDEOCODEC_LOCK_CACHED);
				if (ret < 0)
					goto unlock;
			} else {
				srcCb = srcY + srcLumaSize;
				srcCr = srcY + srcLumaSize + srcChromaSize;
			}
			srcIftuFormat = SCE_IFTU_PIXELFORMAT_YUV420;
		} else if (srcFormat == VIDEOCODEC_PIXEL_YUV420_TILED) {
			if ((srcPitch & 0xF) != 0 || ((srcHeight | srcWidth) & 0xF) != 0)
				goto badSize;
			if ((srcPicture & 0xFF) != 0)
				goto badPointer;
			srcIftuFormat = VIDEOCODEC_IFTU_TILED | SCE_IFTU_PIXELFORMAT_YUV420;
			srcChromaPadding = srcPadding;
			srcCb = srcY + srcLumaSize;
		} else if (srcFormat == SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER) {
			if (((srcPitch | srcWidth) & 0xF) != 0 || (srcHeight & 7) != 0)
				goto badSize;
			if (sourcePlanes && (pSrc->frame.pixelType & VIDEOCODEC_FRAME_SEPARATE_PLANES)) {
				ret = videocodecLockRange(&ranges, &srcCb, (uintptr_t)pSrc->pPlane[1],
					2 * srcChromaSize, pModes[0], 0, VIDEOCODEC_LOCK_CACHED);
				if (ret < 0)
					goto unlock;
			} else {
				srcCb = srcY + srcLumaSize;
			}
			srcChromaPadding = srcPadding;
			srcIftuFormat = SCE_IFTU_PIXELFORMAT_NV12;
		} else if (srcFormat == VIDEOCODEC_PIXEL_YUV420_PACKED_TILED) {
			if ((srcPitch & 0xF) != 0 || (srcWidth & 0x1F) != 0 || (srcHeight & 7) != 0)
				goto badSize;
			if ((srcPicture & 0xFF) != 0)
				goto badPointer;
			srcIftuFormat = VIDEOCODEC_IFTU_TILED | SCE_IFTU_PIXELFORMAT_NV12;
			srcChromaPadding = srcPadding;
			srcCb = srcY + srcLumaSize;
		} else {
			ret = SCE_AVCDEC_ERROR_INVALID_COLOR_FORMAT;
			goto unlock;
		}
		fieldMode = 0;
		if (fields != 0) {
			if (pDst->frame.pixelType & VIDEOCODEC_FRAME_WHOLE_FIELDS) {
				fields = 0;
			} else {
				fields = 3;
				dstHeight >>= 1;
				halfHeight = 1;
			}
		}
	}

	if ((dstType & 0xF7) == 0) {
		if ((dstPitch & 3) != 0 || ((dstHeight | dstWidth) & 7) != 0)
			goto badSize;
		dstIftuFormat = dstFormat != SCE_AVCDEC_PIXEL_RGBA8888 ?
			SCE_IFTU_PIXELFORMAT_RGBX8888 : SCE_IFTU_PIXELFORMAT_BGRX8888;
		dstLumaSize = 0;
		dstChromaSize = 0;
		dstPadding = 4 * (dstPitch - dstWidth);
		dstChromaPadding = 0;
		dstSize = 4 * dstPitch * dstHeight;
	} else if (dstFormat - 1 <= 1) {
		if ((dstPitch & 7) != 0 || ((dstHeight | dstWidth) & 7) != 0)
			goto badSize;
		dstIftuFormat = dstFormat == VIDEOCODEC_PIXEL_BGR565 ?
			SCE_IFTU_PIXELFORMAT_BGR565 : SCE_IFTU_PIXELFORMAT_BGRA5551;
		dstLumaSize = 0;
		dstChromaSize = 0;
		dstPadding = 2 * (dstPitch - dstWidth);
		dstChromaPadding = 0;
		dstSize = 2 * dstPitch * dstHeight;
	} else {
		dstLumaSize = dstPitch * dstHeight;
		dstChromaSize = dstLumaSize >> 2;
		if (dstFormat == SCE_AVCDEC_PIXEL_YUV420_RASTER) {
			if ((dstPitch & 0x1F) != 0 || (dstWidth & 0xF) != 0 || (dstHeight & 7) != 0)
				goto badSize;
			dstPadding = dstPitch - dstWidth;
			dstChromaPadding = (dstPitch - dstWidth) >> 1;
			dstIftuFormat = SCE_IFTU_PIXELFORMAT_YUV420;
		} else if (dstFormat == VIDEOCODEC_PIXEL_YUV420_TILED) {
			if ((dstPitch & 0xF) != 0 || ((dstHeight | dstWidth) & 0xF) != 0 ||
			    dstHeight != srcAlignedHeight || dstWidth != srcAlignedWidth)
				goto badSize;
			if ((dstPicture & 0xFF) != 0)
				goto badPointer;
			dstIftuFormat = VIDEOCODEC_IFTU_TILED | SCE_IFTU_PIXELFORMAT_YUV420;
			dstPadding = dstPitch - dstWidth;
			dstChromaPadding = dstPitch - dstWidth;
		} else if (dstFormat == SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER) {
			if (((dstWidth | dstPitch) & 0xF) != 0 || (dstHeight & 7) != 0)
				goto badSize;
			dstPadding = dstPitch - dstWidth;
			dstChromaPadding = dstPitch - dstWidth;
			dstIftuFormat = SCE_IFTU_PIXELFORMAT_NV12;
		} else if (dstFormat == VIDEOCODEC_PIXEL_YUV420_PACKED_TILED) {
			if ((dstPitch & 0xF) != 0 || (dstWidth & 0x1F) != 0 || (dstHeight & 7) != 0 ||
			    dstHeight != srcAlignedHeight || dstWidth != srcAlignedWidth)
				goto badSize;
			if ((dstPicture & 0xFF) != 0)
				goto badPointer;
			dstPadding = dstPitch - dstWidth;
			dstIftuFormat = VIDEOCODEC_IFTU_TILED | SCE_IFTU_PIXELFORMAT_NV12;
			dstChromaPadding = dstPitch - dstWidth;
		} else {
			ret = SCE_AVCDEC_ERROR_INVALID_COLOR_FORMAT;
			goto unlock;
		}
		dstSize = (3 * dstLumaSize) >> 1;
	}

	if (halfHeight)
		dstSize *= 2;
	if (pDst->frame.pixelType & VIDEOCODEC_FRAME_SEPARATE_PLANES) {
		ret = videocodecLockRange(&ranges, &dstY, dstPicture, dstLumaSize, pModes[1],
			VIDEOCODEC_CACHE_INVALIDATE, VIDEOCODEC_LOCK_CACHED);
		if (ret < 0)
			goto unlock;
		if ((pDst->frame.pixelType & 0xFF) == SCE_AVCDEC_PIXEL_YUV420_RASTER) {
			ret = videocodecLockRange(&ranges, &dstCb, (uintptr_t)pDst->pPlane[1],
				dstChromaSize, pModes[1], VIDEOCODEC_CACHE_INVALIDATE,
				VIDEOCODEC_LOCK_CACHED);
			if (ret < 0)
				goto unlock;
			ret = videocodecLockRange(&ranges, &dstCr, (uintptr_t)pDst->pPlane[2],
				dstChromaSize, pModes[1], VIDEOCODEC_CACHE_INVALIDATE,
				VIDEOCODEC_LOCK_CACHED);
			if (ret < 0)
				goto unlock;
		} else {
			ret = videocodecLockRange(&ranges, &dstCb, (uintptr_t)pDst->pPlane[1],
				2 * dstChromaSize, pModes[1], VIDEOCODEC_CACHE_INVALIDATE,
				VIDEOCODEC_LOCK_CACHED);
			if (ret < 0)
				goto unlock;
			dstCr = 0;
		}
	} else {
		ret = videocodecLockRange(&ranges, &dstY, dstPicture, dstSize, pModes[1],
			VIDEOCODEC_CACHE_INVALIDATE, VIDEOCODEC_LOCK_CACHED);
		if (ret < 0)
			goto unlock;
		dstCb = dstY + dstLumaSize;
		dstCr = dstY + dstLumaSize + dstChromaSize;
	}
	if (fieldMode != 0)
		dstHeight *= 2;

	srcPlane.fb.pixelformat = srcIftuFormat;
	srcPlane.fb.width = srcAlignedWidth;
	srcPlane.fb.height = srcAlignedHeight;
	srcPlane.fb.leftover_stride = srcPadding;
	srcPlane.fb.leftover_align = srcChromaPadding;
	srcPlane.fb.paddr0 = srcY;
	srcPlane.fb.paddr1 = srcCb;
	srcPlane.fb.paddr2 = srcCr;
	srcPlane.dst_x = 0;
	srcPlane.dst_y = 0;
	srcPlane.sampleX = 0;
	srcPlane.sampleY = 0;
	srcPlane.vtop_padding = 0;
	srcPlane.vbot_padding = 0;
	srcPlane.hleft_padding = 0;
	srcPlane.hright_padding = 0;
	dstBuf.pixelformat = dstIftuFormat;
	dstBuf.width = dstWidth;
	dstBuf.height = dstHeight;
	dstBuf.leftover_stride = dstPadding;
	dstBuf.leftover_align = dstChromaPadding;
	dstBuf.paddr0 = dstY;
	dstBuf.paddr1 = dstCb;
	dstBuf.paddr2 = dstCr;
	params.size = sizeof(params);
	params.filterMode = 1;
	params.csc_params1 = (SceIftuCscParams *)&videocodecCscToYuv[cscCoefficient];
	params.csc_params2 = (SceIftuCscParams *)&videocodecCscToRgb[cscCoefficient];
	params.csc_control = 1;
	params.backgroundColor[0] = 0;
	params.backgroundColor[1] = 0;
	params.backgroundColor[2] = 0;
	params.alpha = alpha;
	params.fieldMode = fieldMode;
	srcPlane.scaleX = (srcAlignedWidth << 16) / dstWidth;
	srcPlane.scaleY = (srcAlignedHeight << 16) / dstHeight;
	ret = sceIftuConvert(&dstBuf, &srcPlane, &params);
	if (ret < 0 || fields == 0)
		goto unlock;

	/* the second field, into the second half of the destination */
	if ((sourcePlanes && (pSrc->frame.pixelType & VIDEOCODEC_FRAME_SEPARATE_PLANES)) ||
	    (pDst->frame.pixelType & VIDEOCODEC_FRAME_SEPARATE_PLANES))
		goto badSize;
	if (fieldMode != 0) {
		fieldMode = fieldMode == 2 ? 3 : 2;
	} else {
		ret = videocodecLockRange(&ranges, &srcY, srcFields, srcLumaSize + 2 * srcChromaSize,
			pModes[0], 0, VIDEOCODEC_LOCK_CACHED);
		if (ret < 0)
			goto unlock;
		srcCb = srcY + srcLumaSize;
		srcCr = srcY + srcLumaSize + srcChromaSize;
	}
	ret = videocodecLockRange(&ranges, &dstY, dstPicture + (dstSize >> 1), dstSize >> 1,
		pModes[1], VIDEOCODEC_CACHE_INVALIDATE, VIDEOCODEC_LOCK_CACHED);
	if (ret < 0)
		goto unlock;
	srcPlane.fb.paddr0 = srcY;
	srcPlane.fb.paddr1 = srcCb;
	srcPlane.fb.paddr2 = srcCr;
	pDst->frame.pPicture[1] = (void *)(dstPicture + (dstSize >> 1));
	params.fieldMode = fieldMode;
	dstBuf.paddr0 = dstY;
	dstCb = dstY + dstLumaSize;
	dstBuf.paddr1 = dstCb;
	dstCr = dstY + dstLumaSize + dstChromaSize;
	dstBuf.paddr2 = dstCr;
	ret = sceIftuConvert(&dstBuf, &srcPlane, &params);
	goto unlock;

badSize:
	ret = SCE_AVCDEC_ERROR_UNSUPPORT_IMAGE_SIZE;
	goto unlock;
badPointer:
	ret = SCE_AVCDEC_ERROR_INVALID_POINTER;
unlock:
	videocodecUnlockRangesInline(&ranges, ranges.pid);
	return ret;
}

static __attribute__((noinline, noclone)) int videocodecCsc(VideocodecFrame *pDst, VideocodecFrame *pSrc,
		unsigned int cscCoefficient, const int *pModes)
{
	return videocodecCscInline(pDst, pSrc, cscCoefficient, pModes, 1);
}

/* Converts an MPEG-4 decoder's picture into pPictureOut's pixel format. */
int videocodecM4vdecCsc(M4vdecPicture *pPictureOut, const M4vdecPicture *pPictureIn)
{
	SceAvcdecFrame frame;
	VideocodecFrame in;
	unsigned int inType, outType;

	if (pPictureIn->size != sizeof(*pPictureIn) || pPictureOut->size != sizeof(*pPictureOut))
		return VIDEOCODEC_M4VDEC_ERROR_INVALID_ARGUMENT_SIZE;
	if ((pPictureIn->frame.framePitch & 0xF) != 0 ||
	    ((pPictureIn->frame.frameWidth | pPictureIn->frame.frameHeight) & 0xF) != 0 ||
	    (pPictureOut->frame.framePitch & 0xF) != 0 ||
	    ((pPictureOut->frame.frameWidth | pPictureOut->frame.frameHeight) & 0xF) != 0)
		return SCE_AVCDEC_ERROR_UNSUPPORT_IMAGE_SIZE;
	outType = pPictureOut->frame.pixelType;
	pPictureOut->info = pPictureIn->info;
	inType = pPictureIn->frame.pixelType;
	if ((inType & 0xF7) != 0 && (inType & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_RASTER &&
	    (inType & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER)
		return VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
	if (!VIDEOCODEC_CSC_IS_RGB(outType & 0xFF) &&
	    (outType & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_RASTER &&
	    (outType & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER)
		return VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;

	memset(&frame, 0, sizeof(frame));
	frame.pixelType = inType;
	frame.framePitch = pPictureIn->frame.framePitch;
	frame.frameWidth = pPictureIn->frame.frameWidth;
	frame.frameHeight = pPictureIn->frame.frameHeight;
	frame.horizontalSize = pPictureOut->frame.horizontalSize = pPictureIn->frame.horizontalSize;
	frame.verticalSize = pPictureOut->frame.verticalSize = pPictureIn->frame.verticalSize;
	frame.frameCropLeftOffset = pPictureOut->frame.frameCropLeftOffset =
		pPictureIn->frame.frameCropLeftOffset;
	frame.frameCropRightOffset = pPictureOut->frame.frameCropRightOffset =
		pPictureIn->frame.frameCropRightOffset;
	frame.frameCropTopOffset = pPictureOut->frame.frameCropTopOffset =
		pPictureIn->frame.frameCropTopOffset;
	frame.frameCropBottomOffset = pPictureOut->frame.frameCropBottomOffset =
		pPictureIn->frame.frameCropBottomOffset;
	frame.opt = pPictureIn->frame.opt;
	frame.pPicture[0] = pPictureIn->frame.pPicture[0];
	frame.pPicture[1] = pPictureIn->frame.pPicture[1];
	if (frame.horizontalSize <= frame.frameWidth && frame.verticalSize <= frame.frameHeight) {
		frame.frameWidth = frame.horizontalSize;
		frame.frameHeight = frame.verticalSize;
	}
	in.frame = frame;
	in.frame.pixelType = inType & ~VIDEOCODEC_FRAME_SEPARATE_PLANES;
	pPictureOut->frame.pixelType = outType & ~VIDEOCODEC_FRAME_SEPARATE_PLANES;
	return videocodecConvertM4vdecError(videocodecCsc((VideocodecFrame *)&pPictureOut->frame,
		&in, SCE_AVCDEC_CSC_COEFFICIENT_DEFAULT, videocodecCscLockModes));
}

/*
 * Converts an AVC decoder's picture into pPictureOut's pixel format, with the coefficients
 * its stream's matrix coefficients and range ask for.
 */
int videocodecAvcdecCsc(SceAvcdecPicture *pPictureOut, const SceAvcdecPicture *pPictureIn)
{
	SceAvcdecFrame frame;
	VideocodecFrame in;
	unsigned int inType, outType, cscCoefficient, fullRange;

	if (pPictureIn->size != sizeof(*pPictureIn) || pPictureOut->size != sizeof(*pPictureOut))
		return SCE_AVCDEC_ERROR_INVALID_ARGUMENT_SIZE;
	if ((pPictureIn->frame.framePitch & 0xF) != 0 ||
	    ((pPictureIn->frame.frameWidth | pPictureIn->frame.frameHeight) & 0xF) != 0 ||
	    (pPictureOut->frame.framePitch & 0xF) != 0 ||
	    ((pPictureOut->frame.frameWidth | pPictureOut->frame.frameHeight) & 0xF) != 0)
		return SCE_AVCDEC_ERROR_UNSUPPORT_IMAGE_SIZE;
	pPictureOut->info = pPictureIn->info;
	cscCoefficient = SCE_AVCDEC_CSC_COEFFICIENT_DEFAULT;
	if (pPictureOut->info.matrixCoefficients != 255) {
		if ((unsigned int)(pPictureOut->info.matrixCoefficients - 4) <= 2) {
			fullRange = SCE_AVCDEC_CSC_COEFFICIENT_ITU601_FULL;
			cscCoefficient = SCE_AVCDEC_CSC_COEFFICIENT_ITU601;
		} else {
			fullRange = SCE_AVCDEC_CSC_COEFFICIENT_ITU709_FULL;
			cscCoefficient = SCE_AVCDEC_CSC_COEFFICIENT_ITU709;
		}
		if (pPictureOut->info.videoFullRangeFlag == 1)
			cscCoefficient = fullRange;
	}
	inType = pPictureIn->frame.pixelType;
	outType = pPictureOut->frame.pixelType;
	if ((inType & 0xF7) != 0 && (inType & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_RASTER &&
	    (inType & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	if (!VIDEOCODEC_CSC_IS_RGB(outType & 0xFF) &&
	    (outType & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_RASTER &&
	    (outType & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;

	memset(&frame, 0, sizeof(frame));
	frame.pixelType = inType;
	frame.framePitch = pPictureIn->frame.framePitch;
	frame.frameWidth = pPictureIn->frame.frameWidth;
	frame.frameHeight = pPictureIn->frame.frameHeight;
	frame.horizontalSize = pPictureOut->frame.horizontalSize = pPictureIn->frame.horizontalSize;
	frame.verticalSize = pPictureOut->frame.verticalSize = pPictureIn->frame.verticalSize;
	frame.frameCropLeftOffset = pPictureOut->frame.frameCropLeftOffset =
		pPictureIn->frame.frameCropLeftOffset;
	frame.frameCropRightOffset = pPictureOut->frame.frameCropRightOffset =
		pPictureIn->frame.frameCropRightOffset;
	frame.frameCropTopOffset = pPictureOut->frame.frameCropTopOffset =
		pPictureIn->frame.frameCropTopOffset;
	frame.frameCropBottomOffset = pPictureOut->frame.frameCropBottomOffset =
		pPictureIn->frame.frameCropBottomOffset;
	frame.opt = pPictureIn->frame.opt;
	frame.pPicture[0] = pPictureIn->frame.pPicture[0];
	frame.pPicture[1] = pPictureIn->frame.pPicture[1];
	if (frame.horizontalSize <= frame.frameWidth && frame.verticalSize <= frame.frameHeight) {
		frame.frameWidth = frame.horizontalSize;
		frame.frameHeight = frame.verticalSize;
	}
	in.frame = frame;
	in.frame.pixelType = inType & ~VIDEOCODEC_FRAME_SEPARATE_PLANES;
	pPictureOut->frame.pixelType = outType & ~VIDEOCODEC_FRAME_SEPARATE_PLANES;
	return videocodecCsc((VideocodecFrame *)&pPictureOut->frame, &in, cscCoefficient,
		videocodecCscLockModes);
}

/*
 * Converts the encoder's input frame into its YUV420 tiled frame; flags lock the destination
 * for user writes.
 */
int videocodecAvcencInputCsc(VideocodecEncoderFrame *pDst, const AvcencCscFrame *pSrc,
		unsigned int flags)
{
	SceAvcdecFrame in, out;
	unsigned int format;

	if (pSrc->size != sizeof(*pSrc))
		return VIDEOCODEC_ENCODER_ERROR_INVALID_ARGUMENT_SIZE;
	if ((pSrc->framePitch & 0xF) != 0 || ((pSrc->frameHeight | pSrc->frameWidth) & 0xF) != 0)
		return SCE_AVCDEC_ERROR_UNSUPPORT_IMAGE_SIZE;
	format = pSrc->pixelType & 0xFF;
	if (format != SCE_AVCDEC_PIXEL_YUV420_RASTER && format != SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER)
		return VIDEOCODEC_ENCODER_ERROR_INVALID_PARAM;

	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));
	in.pixelType = format;
	in.framePitch = pSrc->framePitch;
	in.frameWidth = pSrc->frameWidth;
	in.frameHeight = pSrc->frameHeight;
	in.pPicture[0] = pSrc->pPicture[0];
	out.pixelType = VIDEOCODEC_PIXEL_YUV420_TILED;
	out.framePitch = pDst->framePitch;
	out.frameWidth = pDst->frameWidth;
	out.frameHeight = pDst->frameHeight;
	out.pPicture[0] = pDst->pPicture;
	return videocodecConvertAvcencError(videocodecCscInline((VideocodecFrame *)&out,
		(VideocodecFrame *)&in, SCE_AVCDEC_CSC_COEFFICIENT_DEFAULT,
		flags != 0 ? videocodecCscUserLockModes : videocodecCscKernelLockModes, 0));
}

/*
 * Converts one encoder frame into another's pixel format at the source's size; of the CSC
 * coefficients only ITU-R BT.601 and BT.709 are taken.
 */
int videocodecAvcencCsc(AvcencCscFrame *pDst, const AvcencCscFrame *pSrc)
{
	SceAvcdecFrame out, frame, in;
	unsigned int outType, inType, cscCoefficient;

	if (pSrc->size != sizeof(*pSrc) || pDst->size != sizeof(*pDst))
		return VIDEOCODEC_ENCODER_ERROR_INVALID_ARGUMENT_SIZE;
	if ((pSrc->framePitch & 0xF) != 0 || ((pSrc->frameWidth | pSrc->frameHeight) & 0xF) != 0 ||
	    (pDst->framePitch & 0xF) != 0 || ((pDst->frameHeight | pDst->frameWidth) & 0xF) != 0)
		return SCE_AVCDEC_ERROR_UNSUPPORT_IMAGE_SIZE;
	outType = pDst->pixelType;
	inType = pSrc->pixelType;
	cscCoefficient = SCE_AVCDEC_CSC_COEFFICIENT_DEFAULT;
	if ((outType & 0xF7) == 0 && (outType & SCE_AVCDEC_OPTION_ENABLE))
		cscCoefficient = pDst->opt.rgba.cscCoefficient;
	if ((inType & 0xF7) == 0 && (inType & SCE_AVCDEC_OPTION_ENABLE) &&
	    pSrc->opt.rgba.cscCoefficient != 0)
		cscCoefficient = pSrc->opt.rgba.cscCoefficient;
	if (cscCoefficient != SCE_AVCDEC_CSC_COEFFICIENT_DEFAULT && cscCoefficient - 1 > 1)
		return SCE_AVCDEC_ERROR_INVALID_COLOR_FORMAT;

	memset(&frame, 0, sizeof(frame));
	memset(&out, 0, sizeof(out));
	out.pixelType = outType | VIDEOCODEC_FRAME_KEEP_SIZE;
	out.framePitch = pDst->framePitch;
	out.frameWidth = pDst->frameWidth;
	out.frameHeight = pDst->frameHeight;
	out.horizontalSize = pDst->frameWidth;
	out.verticalSize = pDst->frameHeight;
	out.opt.rgba.alpha = pDst->opt.rgba.alpha;
	out.opt.rgba.cscCoefficient = pDst->opt.rgba.cscCoefficient;
	out.pPicture[0] = pDst->pPicture[0];
	out.pPicture[1] = pDst->pPicture[1];
	frame.pixelType = inType;
	frame.framePitch = pSrc->framePitch;
	frame.frameWidth = pSrc->frameWidth;
	frame.frameHeight = pSrc->frameHeight;
	frame.horizontalSize = pSrc->frameWidth;
	frame.verticalSize = pSrc->frameHeight;
	frame.opt.rgba.alpha = pSrc->opt.rgba.alpha;
	frame.opt.rgba.cscCoefficient = pSrc->opt.rgba.cscCoefficient;
	frame.pPicture[0] = pSrc->pPicture[0];
	frame.pPicture[1] = pSrc->pPicture[1];
	if (!((inType & 0xEF) == 0 || (inType & 0xFF) == SCE_AVCDEC_PIXEL_BGRA8888 ||
	      (inType & 0xFF) == SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER))
		return VIDEOCODEC_ENCODER_ERROR_INVALID_PARAM;
	if (!VIDEOCODEC_CSC_IS_RGB(outType & 0xFF) &&
	    (outType & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_RASTER &&
	    (outType & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER)
		return VIDEOCODEC_ENCODER_ERROR_INVALID_PARAM;

	memset(&in, 0, sizeof(in));
	in = frame;
	out.pixelType = (outType & ~(VIDEOCODEC_FRAME_SEPARATE_PLANES | VIDEOCODEC_FRAME_KEEP_SIZE)) |
		VIDEOCODEC_FRAME_KEEP_SIZE;
	out.horizontalSize = pSrc->frameWidth;
	out.verticalSize = pSrc->frameHeight;
	return videocodecConvertAvcencError(videocodecCscInline((VideocodecFrame *)&out,
		(VideocodecFrame *)&in, SCE_AVCDEC_CSC_COEFFICIENT_DEFAULT, videocodecCscLockModes, 0));
}

/*
 * Converts a decoder's frame into pFrameOut. The IFTU scales by at most four, so a frame
 * further from pFrameOut's size, and an interlaced physical frame, is converted through the
 * work pictures pArrayPicture lends first.
 */
int videocodecDecoderCsc(SceAvcdecFrame *pFrameOut, const SceAvcdecFrame *pFrameIn,
		unsigned int cscCoefficient, const SceAvcdecArrayPicture *pArrayPicture)
{
	VideocodecFrame src, work, in;
	SceAvcdecFrame frame;
	SceAvcdecPicture *pWork;
	const SceAvcdecFrame *pFrame;
	unsigned int alignedWidth, alignedHeight, keepSize, index, count, type;
	int ret;

	alignedWidth = (pFrameIn->frameWidth + 15) & ~15U;
	alignedHeight = (pFrameIn->frameHeight + 7) & ~7U;
	keepSize = pFrameOut->pixelType & VIDEOCODEC_FRAME_KEEP_SIZE;
	pFrame = pFrameIn;
	index = 0;
	count = 1;

	/* an interlaced physical frame is first woven into the first work picture */
	if (pFrameIn->pixelType == VIDEOCODEC_PIXEL_PHYSICAL &&
	    (uintptr_t)pFrameIn->pPicture[1] - 2 <= 1 && pArrayPicture != NULL &&
	    pArrayPicture->numOfElm != 0 && (pWork = pArrayPicture->pPicture[0]) != NULL) {
		if (pWork->size != sizeof(*pWork))
			return SCE_AVCDEC_ERROR_INVALID_ARGUMENT_SIZE;
		if ((pWork->frame.pixelType & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER &&
		    (pWork->frame.pixelType & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_RASTER)
			return SCE_AVCDEC_ERROR_INVALID_PARAM;
		pWork->frame.horizontalSize = pFrameIn->horizontalSize;
		pWork->frame.verticalSize = pFrameIn->verticalSize;
		pWork->frame.frameCropLeftOffset = pFrameIn->frameCropLeftOffset;
		pWork->frame.frameCropRightOffset = pFrameIn->frameCropRightOffset;
		pWork->frame.frameCropTopOffset = pFrameIn->frameCropTopOffset;
		pWork->frame.frameCropBottomOffset = pFrameIn->frameCropBottomOffset;
		work.frame = pWork->frame;
		if (pFrameIn->frameWidth <= work.frame.frameWidth &&
		    pFrameIn->frameHeight <= work.frame.frameHeight) {
			work.frame.frameWidth = pFrameIn->frameWidth;
			work.frame.frameHeight = pFrameIn->frameHeight;
		}
		src.frame = *pFrameIn;
		work.frame.pixelType &= ~VIDEOCODEC_FRAME_SEPARATE_PLANES;
		src.frame.pixelType &= ~VIDEOCODEC_FRAME_SEPARATE_PLANES;
		ret = videocodecCscInline(&work, &src, cscCoefficient, videocodecCscLockModes, 1);
		if (ret < 0)
			return ret;
		pFrame = &work.frame;
		index = 1;
		count = 2;
	}

	if (!(keepSize && alignedWidth < pFrameOut->frameWidth &&
	      pFrameOut->frameWidth > 4 * alignedWidth) &&
	    !(alignedWidth > pFrameOut->frameWidth && alignedWidth > 4 * pFrameOut->frameWidth) &&
	    !(keepSize && alignedHeight < pFrameOut->frameHeight &&
	      pFrameOut->frameHeight > 4 * alignedHeight) &&
	    !(alignedHeight > pFrameOut->frameHeight && alignedHeight > 4 * pFrameOut->frameHeight)) {
		src.frame = *pFrame;
		src.frame.pixelType &= ~VIDEOCODEC_FRAME_SEPARATE_PLANES;
		pFrameOut->pixelType &= ~VIDEOCODEC_FRAME_SEPARATE_PLANES;
		return videocodecCsc((VideocodecFrame *)pFrameOut, &src, cscCoefficient,
			videocodecCscLockModes);
	}

	/* scaled in two steps, through a work picture */
	if (pArrayPicture == NULL || pArrayPicture->numOfElm < count ||
	    pArrayPicture->pPicture[index] == NULL)
		return SCE_AVCDEC_ERROR_UNSUPPORT_IMAGE_SIZE;
	pWork = pArrayPicture->pPicture[index];
	if (pArrayPicture->numOfElm == 2 && pArrayPicture->pPicture[1] != NULL)
		pWork = pArrayPicture->pPicture[1];
	if (pWork->size != sizeof(*pWork))
		return SCE_AVCDEC_ERROR_INVALID_ARGUMENT_SIZE;
	if ((pWork->frame.pixelType & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER &&
	    (pWork->frame.pixelType & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_RASTER)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;

	if (pFrame != pFrameIn) {
		work.frame = pWork->frame;
		if (pFrame->frameWidth <= work.frame.frameWidth &&
		    pFrame->frameHeight <= work.frame.frameHeight) {
			work.frame.frameWidth = pFrame->frameWidth;
			work.frame.frameHeight = pFrame->frameHeight;
		}
		type = pFrame->pixelType;
		if ((type & 0xEF) != 0 && (type & 0xFF) != SCE_AVCDEC_PIXEL_BGRA8888 &&
		    (type & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER)
			return SCE_AVCDEC_ERROR_INVALID_PARAM;
		if (!VIDEOCODEC_CSC_IS_RGB(work.frame.pixelType & 0xFF) &&
		    (work.frame.pixelType & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_RASTER &&
		    (work.frame.pixelType & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER)
			return SCE_AVCDEC_ERROR_INVALID_PARAM;
		memset(&frame, 0, sizeof(frame));
		frame.pixelType = type;
		frame.framePitch = pFrame->framePitch;
		frame.frameWidth = pFrame->frameWidth;
		frame.frameHeight = pFrame->frameHeight;
		frame.horizontalSize = work.frame.horizontalSize = pFrame->horizontalSize;
		frame.verticalSize = work.frame.verticalSize = pFrame->verticalSize;
		frame.frameCropLeftOffset = work.frame.frameCropLeftOffset =
			pFrame->frameCropLeftOffset;
		frame.frameCropRightOffset = work.frame.frameCropRightOffset =
			pFrame->frameCropRightOffset;
		frame.frameCropTopOffset = work.frame.frameCropTopOffset =
			pFrame->frameCropTopOffset;
		frame.frameCropBottomOffset = work.frame.frameCropBottomOffset =
			pFrame->frameCropBottomOffset;
		frame.opt = pFrame->opt;
		frame.pPicture[0] = pFrame->pPicture[0];
		frame.pPicture[1] = pFrame->pPicture[1];
		if (frame.horizontalSize <= frame.frameWidth &&
		    frame.verticalSize <= frame.frameHeight) {
			frame.frameWidth = frame.horizontalSize;
			frame.frameHeight = frame.verticalSize;
		}
		in.frame = frame;
		work.frame.pixelType &= ~VIDEOCODEC_FRAME_SEPARATE_PLANES;
		ret = videocodecCscInline(&work, &in, cscCoefficient, videocodecCscLockModes, 0);
	} else {
		pWork->frame.horizontalSize = pFrameIn->horizontalSize;
		pWork->frame.verticalSize = pFrameIn->verticalSize;
		pWork->frame.frameCropLeftOffset = pFrameIn->frameCropLeftOffset;
		pWork->frame.frameCropRightOffset = pFrameIn->frameCropRightOffset;
		pWork->frame.frameCropTopOffset = pFrameIn->frameCropTopOffset;
		pWork->frame.frameCropBottomOffset = pFrameIn->frameCropBottomOffset;
		work.frame = pWork->frame;
		if (pFrameIn->frameWidth <= work.frame.frameWidth &&
		    pFrameIn->frameHeight <= work.frame.frameHeight) {
			work.frame.frameWidth = pFrameIn->frameWidth;
			work.frame.frameHeight = pFrameIn->frameHeight;
		}
		src.frame = *pFrameIn;
		src.frame.pixelType &= ~VIDEOCODEC_FRAME_SEPARATE_PLANES;
		work.frame.pixelType &= ~VIDEOCODEC_FRAME_SEPARATE_PLANES;
		ret = videocodecCsc(&work, &src, cscCoefficient, videocodecCscLockModes);
	}
	if (ret < 0)
		return ret;

	/* the work picture into pFrameOut */
	type = work.frame.pixelType;
	if ((type & 0xF7) != 0 && (type & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_RASTER &&
	    (type & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	if (!VIDEOCODEC_CSC_IS_RGB(pFrameOut->pixelType & 0xFF) &&
	    (pFrameOut->pixelType & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_RASTER &&
	    (pFrameOut->pixelType & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	memset(&frame, 0, sizeof(frame));
	frame.pixelType = type;
	frame.framePitch = work.frame.framePitch;
	frame.frameWidth = work.frame.frameWidth;
	frame.frameHeight = work.frame.frameHeight;
	frame.horizontalSize = pFrameOut->horizontalSize = work.frame.horizontalSize;
	frame.verticalSize = pFrameOut->verticalSize = work.frame.verticalSize;
	frame.frameCropLeftOffset = pFrameOut->frameCropLeftOffset =
		work.frame.frameCropLeftOffset;
	frame.frameCropRightOffset = pFrameOut->frameCropRightOffset =
		work.frame.frameCropRightOffset;
	frame.frameCropTopOffset = pFrameOut->frameCropTopOffset =
		work.frame.frameCropTopOffset;
	frame.frameCropBottomOffset = pFrameOut->frameCropBottomOffset =
		work.frame.frameCropBottomOffset;
	frame.opt = work.frame.opt;
	frame.pPicture[0] = work.frame.pPicture[0];
	frame.pPicture[1] = work.frame.pPicture[1];
	if (frame.horizontalSize <= frame.frameWidth && frame.verticalSize <= frame.frameHeight) {
		frame.frameWidth = frame.horizontalSize;
		frame.frameHeight = frame.verticalSize;
	}
	in.frame = frame;
	in.frame.pixelType = type & ~VIDEOCODEC_FRAME_SEPARATE_PLANES;
	pFrameOut->pixelType &= ~VIDEOCODEC_FRAME_SEPARATE_PLANES;
	return videocodecCsc((VideocodecFrame *)pFrameOut, &in, cscCoefficient,
		videocodecCscLockModes);
}

/*
 * Fills an AVC decoder's picture from the picture the VIP decoded: its timing, aspect,
 * colour and picture timing information, its crop, and its pixels converted with the
 * coefficients its stream asks for. An interlaced picture needs interlaced set.
 */
int videocodecAvcdecPicture(SceAvcdecPictureForInterlaced *pPicture, const VeneziaDebugSnapshotOutput *pOutput,
		const SceAvcdecArrayPicture *pArrayPicture, int interlaced)
{
	SceAvcdecFrame frame;
	unsigned int format, typeBits, fieldMode, cscCoefficient, fullRange;
	unsigned int aspectRatioIdc, picStruct, ctTypeField;
	int structure, width, height, ret;

	if (pPicture->size != sizeof(*pPicture))
		return SCE_AVCDEC_ERROR_INVALID_ARGUMENT_SIZE;
	if ((pPicture->frame.framePitch & 0xF) != 0 ||
	    ((pPicture->frame.frameWidth | pPicture->frame.frameHeight) & 0xF) != 0)
		return SCE_AVCDEC_ERROR_UNSUPPORT_IMAGE_SIZE;
	format = pPicture->frame.pixelType & 0xFF;
	if (!VIDEOCODEC_CSC_IS_RGB(format) && format != SCE_AVCDEC_PIXEL_YUV420_RASTER &&
	    format != SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;

	memset(&pPicture->info, -1, sizeof(pPicture->info));
	pPicture->info.ctType = 0;
	typeBits = videocodecPictureTypeBits[(signed char)pOutput->context.executionMode % 6] << 2;
	pPicture->info.ctType = typeBits;
	fieldMode = 0;
	if (pOutput->valid & 1) {
		const VeneziaDebugRegisterInfo *vui = &pOutput->data.registers;

		if (vui->control[14]) {
			pPicture->info.numUnitsInTick = vui->programCounter;
			pPicture->info.timeScale = vui->stackPointer;
			pPicture->info.fixedFrameRateFlag = vui->control[15];
		}
		if (vui->control[0]) {
			aspectRatioIdc = vui->control[1];
			pPicture->info.aspectRatioIdc = aspectRatioIdc;
			if (aspectRatioIdc == 255) {
				pPicture->info.sarWidth = vui->executionStatus.high;
				pPicture->info.sarHeight = vui->executionStatus.low;
			}
		}
		if (vui->control[7]) {
			pPicture->info.colourPrimaries = vui->control[8];
			pPicture->info.transferCharacteristics = vui->control[9];
			pPicture->info.matrixCoefficients = vui->control[10];
		}
		if (vui->control[4])
			pPicture->info.videoFullRangeFlag = vui->control[6];
		if (vui->executionFlags[1] && (pOutput->valid & 4)) {
			pPicture->info.ctType = typeBits | SCE_AVCDEC_CT_TYPE_BIT_ENABLE_PIC_STRUCT;
			picStruct = pOutput->data.channels[0].status[0];
			pPicture->info.picStruct[0] = picStruct;
			if (pOutput->data.channels[0].status[1]) {
				pPicture->info.ctType = typeBits | SCE_AVCDEC_CT_TYPE_BIT_ENABLE_PIC_STRUCT |
					SCE_AVCDEC_CT_TYPE_BIT_ENABLE_CT_TYPE;
				ctTypeField = pOutput->data.channels[0].control[0][0] & SCE_AVCDEC_CT_TYPE_BIT_CT_TYPE_FIELD;
				pPicture->info.ctType = typeBits | SCE_AVCDEC_CT_TYPE_BIT_ENABLE_PIC_STRUCT |
					SCE_AVCDEC_CT_TYPE_BIT_ENABLE_CT_TYPE | ctTypeField;
				if (picStruct == 1)
					fieldMode = 2;
				else if (picStruct == 2)
					fieldMode = 3;
				else if (picStruct == 3 && ctTypeField == 1)
					fieldMode = 2;
				else if (picStruct == 4 && ctTypeField == 1)
					fieldMode = 3;
			}
			if (pOutput->valid & 0x20)
				pPicture->info.picStruct[1] = pOutput->data.channels[1].status[0];
		}
	}

	structure = (signed char)pOutput->context.instructionWidth;
	if (structure == 2) {
		pPicture->info.ctType |= SCE_AVCDEC_CT_TYPE_BIT_FIELD_PAIR;
		if (pOutput->context.flagA == 1)
			pPicture->info.ctType |= SCE_AVCDEC_CT_TYPE_BIT_FIELD_PAIR |
				SCE_AVCDEC_CT_TYPE_BIT_TOP_FIELD_FIRST;
		else
			structure = 3;
		fieldMode = structure;
	}
	pPicture->info.outputPts.upper = pOutput->data.trace.marked;
	pPicture->info.outputPts.lower = pOutput->data.trace.value;
	pPicture->info.flag = ~pOutput->event;
	if (!interlaced && fieldMode != 0)
		return SCE_AVCDEC_ERROR_INTERLACED;

	memset(&frame, 0, sizeof(frame));
	width = (int16_t)pOutput->context.instruction.high;
	height = (int16_t)pOutput->context.instruction.low;
	frame.pPicture[0] = (void *)pOutput->physicalAddress;
	pPicture->frame.horizontalSize = width;
	pPicture->frame.verticalSize = height;
	pPicture->frame.frameCropLeftOffset = (int16_t)pOutput->context.operand.high;
	pPicture->frame.frameCropRightOffset = (int16_t)pOutput->context.operand.low;
	pPicture->frame.frameCropTopOffset = (int16_t)pOutput->context.address.high;
	pPicture->frame.frameCropBottomOffset = (int16_t)pOutput->context.address.low;
	frame.pixelType = VIDEOCODEC_PIXEL_PHYSICAL;
	frame.framePitch = width;
	frame.frameWidth = width;
	frame.frameHeight = height;
	frame.horizontalSize = width;
	frame.verticalSize = height;
	frame.frameCropLeftOffset = pPicture->frame.frameCropLeftOffset;
	frame.frameCropRightOffset = pPicture->frame.frameCropRightOffset;
	frame.frameCropTopOffset = pPicture->frame.frameCropTopOffset;
	frame.frameCropBottomOffset = pPicture->frame.frameCropBottomOffset;
	frame.pPicture[1] = (void *)fieldMode;

	lockFastMutex(&videocodecTileRegMutex);
	SceCodecEngineWrapperForDriver_18197103(pOutput->virtualAddress);
	cscCoefficient = SCE_AVCDEC_CSC_COEFFICIENT_DEFAULT;
	if (pPicture->info.matrixCoefficients != 255) {
		if ((unsigned int)(pPicture->info.matrixCoefficients - 4) <= 2) {
			fullRange = SCE_AVCDEC_CSC_COEFFICIENT_ITU601_FULL;
			cscCoefficient = SCE_AVCDEC_CSC_COEFFICIENT_ITU601;
		} else {
			fullRange = SCE_AVCDEC_CSC_COEFFICIENT_ITU709_FULL;
			cscCoefficient = SCE_AVCDEC_CSC_COEFFICIENT_ITU709;
		}
		if (pPicture->info.videoFullRangeFlag == 1)
			cscCoefficient = fullRange;
	}
	if (pOutput->event & 1)
		ret = 0;
	else
		ret = videocodecDecoderCsc(&pPicture->frame, &frame, cscCoefficient, pArrayPicture);
	sceKernelUnlockFastMutex(&videocodecTileRegMutex);
	return ret;
}

/*
 * Fills an AVC decoder's picture from the picture the DSP0 decoder reported: its timing,
 * aspect, colour and picture timing information, its crop, and its pixels converted with
 * the coefficients its stream asks for.
 */
int videocodecDsp0AvcdecPicture(SceAvcdecPictureForInterlaced *pPicture, Dsp0VideocodecPicture *const *ppOutput,
		const SceAvcdecArrayPicture *pArrayPicture)
{
	const Dsp0VideocodecPicture *pOutput = *ppOutput;
	const Dsp0VideocodecVui *vui;
	const Dsp0VideocodecSei *sei;
	SceAvcdecFrame frame;
	unsigned int format, aspectRatioIdc, picStruct, cscCoefficient, fullRange;
	unsigned int cropLeft, cropRight, cropTop, cropBottom;

	if (pOutput == NULL)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	if (pPicture->size != sizeof(*pPicture))
		return SCE_AVCDEC_ERROR_INVALID_ARGUMENT_SIZE;
	if ((pPicture->frame.framePitch & 0xF) != 0 ||
	    ((pPicture->frame.frameWidth | pPicture->frame.frameHeight) & 0xF) != 0)
		return SCE_AVCDEC_ERROR_UNSUPPORT_IMAGE_SIZE;
	format = pPicture->frame.pixelType & 0xFF;
	if (!VIDEOCODEC_CSC_IS_RGB(format) && format != SCE_AVCDEC_PIXEL_YUV420_RASTER &&
	    format != SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;

	memset(&pPicture->info, -1, sizeof(pPicture->info));
	pPicture->info.ctType = 0;
	vui = pOutput->avc.pVui;
	if (vui != NULL) {
		sei = pOutput->avc.pSei;
		if (vui->timingInfoPresentFlag) {
			pPicture->info.numUnitsInTick = vui->numUnitsInTick;
			pPicture->info.timeScale = vui->timeScale;
			pPicture->info.fixedFrameRateFlag = vui->fixedFrameRateFlag;
		}
		if (vui->aspectRatioInfoPresentFlag) {
			aspectRatioIdc = vui->aspectRatioIdc;
			pPicture->info.aspectRatioIdc = aspectRatioIdc;
			if (aspectRatioIdc == 255) {
				pPicture->info.sarWidth = vui->sarWidth;
				pPicture->info.sarHeight = vui->sarHeight;
			}
		}
		if (vui->colourDescriptionPresentFlag) {
			pPicture->info.colourPrimaries = vui->colourPrimaries;
			pPicture->info.transferCharacteristics = vui->transferCharacteristics;
			pPicture->info.matrixCoefficients = vui->matrixCoefficients;
		}
		if (vui->videoSignalTypePresentFlag)
			pPicture->info.videoFullRangeFlag = vui->videoFullRangeFlag;
		if (sei != NULL && vui->picStructPresentFlag &&
		    (sei->flags & DSP0_VIDEOCODEC_SEI_PICTURE_TIMING)) {
			pPicture->info.ctType = SCE_AVCDEC_CT_TYPE_BIT_ENABLE_PIC_STRUCT;
			picStruct = sei->pictureTiming.picStruct;
			pPicture->info.picStruct[1] = 0xFF;
			pPicture->info.picStruct[0] = picStruct;
			if (sei->pictureTiming.clockTimestampFlag[0]) {
				pPicture->info.ctType = SCE_AVCDEC_CT_TYPE_BIT_ENABLE_PIC_STRUCT |
					SCE_AVCDEC_CT_TYPE_BIT_ENABLE_CT_TYPE;
				pPicture->info.ctType = SCE_AVCDEC_CT_TYPE_BIT_ENABLE_PIC_STRUCT |
					SCE_AVCDEC_CT_TYPE_BIT_ENABLE_CT_TYPE |
					(sei->pictureTiming.clockTimestamp[0].clockMetadata[0] & SCE_AVCDEC_CT_TYPE_BIT_CT_TYPE_FIELD);
			}
		}
	}
	pPicture->info.outputPts.upper = pOutput->avc.pts >> 32;
	pPicture->info.outputPts.lower = pOutput->avc.pts;

	memset(&frame, 0, sizeof(frame));
	pPicture->frame.horizontalSize = pOutput->frameWidth;
	pPicture->frame.verticalSize = pOutput->frameHeight;
	if (pOutput->avc.frameCroppingFlag) {
		cropLeft = pOutput->avc.frameCropLeftOffset;
		cropRight = pOutput->avc.frameCropRightOffset;
		cropTop = pOutput->avc.frameCropTopOffset;
		cropBottom = pOutput->avc.frameCropBottomOffset;
	} else {
		cropLeft = 0;
		cropRight = 0;
		cropTop = 0;
		cropBottom = 0;
	}
	frame.frameCropLeftOffset = cropLeft;
	frame.frameCropRightOffset = cropRight;
	frame.frameCropTopOffset = cropTop;
	frame.frameCropBottomOffset = cropBottom;
	pPicture->frame.frameCropLeftOffset = cropLeft;
	pPicture->frame.frameCropRightOffset = cropRight;
	pPicture->frame.frameCropTopOffset = cropTop;
	pPicture->frame.frameCropBottomOffset = cropBottom;
	frame.pixelType = VIDEOCODEC_PIXEL_PHYSICAL_PACKED;
	frame.framePitch = pOutput->lumaPitch | pOutput->chromaPitch << 16;
	frame.frameWidth = pOutput->frameWidth;
	frame.frameHeight = pOutput->frameHeight;
	frame.horizontalSize = pOutput->frameWidth;
	frame.verticalSize = pOutput->frameHeight;
	frame.pPicture[0] = (void *)pOutput->lumaAddress;
	frame.pPicture[1] = (void *)pOutput->chromaAddress;

	cscCoefficient = SCE_AVCDEC_CSC_COEFFICIENT_DEFAULT;
	if (pPicture->info.matrixCoefficients != 255) {
		if ((unsigned int)(pPicture->info.matrixCoefficients - 4) <= 2) {
			fullRange = SCE_AVCDEC_CSC_COEFFICIENT_ITU601_FULL;
			cscCoefficient = SCE_AVCDEC_CSC_COEFFICIENT_ITU601;
		} else {
			fullRange = SCE_AVCDEC_CSC_COEFFICIENT_ITU709_FULL;
			cscCoefficient = SCE_AVCDEC_CSC_COEFFICIENT_ITU709;
		}
		if (pPicture->info.videoFullRangeFlag == 1)
			cscCoefficient = fullRange;
	}
	return videocodecDecoderCsc(&pPicture->frame, &frame, cscCoefficient, pArrayPicture);
}

/*
 * Fills an MPEG-4 decoder's picture from the picture the DSP0 decoder reported: its VOL
 * information, its size, and its pixels converted with the default coefficients, through
 * the work pictures pArrayPicture lends (one or two).
 */
int videocodecDsp0M4vdecPicture(M4vdecPicture *pPicture, Dsp0VideocodecPicture *const *ppOutput,
		const M4vdecArrayPicture *pArrayPicture)
{
	const Dsp0VideocodecPicture *pOutput = *ppOutput;
	SceAvcdecArrayPicture array;
	SceAvcdecPicture *pWork[2];
	SceAvcdecPicture work[2];
	SceAvcdecFrame frame;
	unsigned int format, i;
	int ret;

	if (pOutput == NULL)
		return VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;
	if (pPicture->size != sizeof(*pPicture))
		return VIDEOCODEC_M4VDEC_ERROR_INVALID_ARGUMENT_SIZE;
	if ((pPicture->frame.framePitch & 0xF) != 0 ||
	    ((pPicture->frame.frameWidth | pPicture->frame.frameHeight) & 0xF) != 0)
		return SCE_AVCDEC_ERROR_UNSUPPORT_IMAGE_SIZE;
	format = pPicture->frame.pixelType & 0xFF;
	if (!VIDEOCODEC_CSC_IS_RGB(format) && format != SCE_AVCDEC_PIXEL_YUV420_RASTER &&
	    format != SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER)
		return VIDEOCODEC_M4VDEC_ERROR_INVALID_PARAM;

	memset(&pPicture->info, -1, sizeof(pPicture->info));
	pPicture->info.vopTimeIncrementResolution = pOutput->m4v.vopTimeIncrementResolution;
	pPicture->info.fixedVopRate = pOutput->m4v.fixedVopRate;
	pPicture->info.aspectRatioInfo = pOutput->m4v.aspectRatioInfo;
	pPicture->info.parWidth = pOutput->m4v.parWidth;
	pPicture->info.parHeight = pOutput->m4v.parHeight;
	pPicture->info.colourPrimaries = pOutput->m4v.colourPrimaries;
	pPicture->info.transferCharacteristics = pOutput->m4v.transferCharacteristics;
	pPicture->info.videoRange = pOutput->m4v.videoRange;
	pPicture->info.infoFlags = pOutput->m4v.infoFlags;
	pPicture->info.outputPts.upper = pOutput->m4v.pts >> 32;
	pPicture->info.outputPts.lower = pOutput->m4v.pts;

	memset(&frame, 0, sizeof(frame));
	pPicture->frame.horizontalSize = pOutput->frameWidth;
	frame.pPicture[0] = (void *)pOutput->lumaAddress;
	frame.pPicture[1] = (void *)pOutput->chromaAddress;
	frame.horizontalSize = pOutput->frameWidth;
	pPicture->frame.verticalSize = pOutput->frameHeight;
	frame.verticalSize = pOutput->frameHeight;
	frame.frameWidth = pOutput->frameWidth;
	frame.frameHeight = pOutput->frameHeight;
	frame.pixelType = VIDEOCODEC_PIXEL_PHYSICAL_PACKED;
	frame.framePitch = pOutput->lumaPitch | pOutput->chromaPitch << 16;
	pPicture->frame.frameCropLeftOffset = 0;
	pPicture->frame.frameCropRightOffset = 0;
	pPicture->frame.frameCropTopOffset = 0;
	pPicture->frame.frameCropBottomOffset = 0;

	if (pArrayPicture != NULL) {
		if (pArrayPicture->numOfElm - 1 > 1)
			return VIDEOCODEC_M4VDEC_ERROR_INVALID_POINTER;
		for (i = 0; i < pArrayPicture->numOfElm; i++) {
			if (pArrayPicture->pPicture[i]->size != sizeof(M4vdecPicture))
				return VIDEOCODEC_M4VDEC_ERROR_INVALID_ARGUMENT_SIZE;
			work[i].size = sizeof(work[i]);
			work[i].frame = pArrayPicture->pPicture[i]->frame;
			pWork[i] = &work[i];
		}
		array.numOfElm = pArrayPicture->numOfElm;
		array.pPicture = pWork;
	}
	ret = videocodecDecoderCsc(&pPicture->frame, &frame,
		SCE_AVCDEC_CSC_COEFFICIENT_DEFAULT, pArrayPicture != NULL ? &array : NULL);
	if ((ret & 0xFFFFFF00) == VIDEOCODEC_DECODER_ERROR_BASE)
		ret |= 0x900;
	return ret;
}

/*
 * Converts an AVC decoder's picture into pPictureOut's pixel format, as videocodecAvcdecCsc,
 * for pictures whose frames may have separate planes.
 */
int videocodecAvcdecCscInternal(VideocodecPicture *pPictureOut, const VideocodecPicture *pPictureIn)
{
	VideocodecFrame in;
	unsigned int inType, outType, cscCoefficient, fullRange;

	if (pPictureIn->size != sizeof(*pPictureIn) || pPictureOut->size != sizeof(*pPictureOut))
		return SCE_AVCDEC_ERROR_INVALID_ARGUMENT_SIZE;
	if ((pPictureIn->frame.frame.framePitch & 0xF) != 0 ||
	    ((pPictureIn->frame.frame.frameWidth | pPictureIn->frame.frame.frameHeight) & 0xF) != 0 ||
	    (pPictureOut->frame.frame.framePitch & 0xF) != 0 ||
	    ((pPictureOut->frame.frame.frameWidth | pPictureOut->frame.frame.frameHeight) & 0xF) != 0)
		return SCE_AVCDEC_ERROR_UNSUPPORT_IMAGE_SIZE;
	pPictureOut->info = pPictureIn->info;
	cscCoefficient = SCE_AVCDEC_CSC_COEFFICIENT_DEFAULT;
	if (pPictureOut->info.matrixCoefficients != 255) {
		if ((unsigned int)(pPictureOut->info.matrixCoefficients - 4) <= 2) {
			fullRange = SCE_AVCDEC_CSC_COEFFICIENT_ITU601_FULL;
			cscCoefficient = SCE_AVCDEC_CSC_COEFFICIENT_ITU601;
		} else {
			fullRange = SCE_AVCDEC_CSC_COEFFICIENT_ITU709_FULL;
			cscCoefficient = SCE_AVCDEC_CSC_COEFFICIENT_ITU709;
		}
		if (pPictureOut->info.videoFullRangeFlag == 1)
			cscCoefficient = fullRange;
	}
	inType = pPictureIn->frame.frame.pixelType;
	outType = pPictureOut->frame.frame.pixelType;
	if ((inType & 0xEF) != 0 && (inType & 0xFF) != SCE_AVCDEC_PIXEL_BGRA8888 &&
	    (inType & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;
	if (!VIDEOCODEC_CSC_IS_RGB(outType & 0xFF) &&
	    (outType & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_RASTER &&
	    (outType & 0xFF) != SCE_AVCDEC_PIXEL_YUV420_PACKED_RASTER)
		return SCE_AVCDEC_ERROR_INVALID_PARAM;

	memset(&in, 0, sizeof(in));
	in.frame.pixelType = inType;
	in.frame.framePitch = pPictureIn->frame.frame.framePitch;
	in.frame.frameWidth = pPictureIn->frame.frame.frameWidth;
	in.frame.frameHeight = pPictureIn->frame.frame.frameHeight;
	in.frame.horizontalSize = pPictureOut->frame.frame.horizontalSize =
		pPictureIn->frame.frame.horizontalSize;
	in.frame.verticalSize = pPictureOut->frame.frame.verticalSize =
		pPictureIn->frame.frame.verticalSize;
	in.frame.frameCropLeftOffset = pPictureOut->frame.frame.frameCropLeftOffset =
		pPictureIn->frame.frame.frameCropLeftOffset;
	in.frame.frameCropRightOffset = pPictureOut->frame.frame.frameCropRightOffset =
		pPictureIn->frame.frame.frameCropRightOffset;
	in.frame.frameCropTopOffset = pPictureOut->frame.frame.frameCropTopOffset =
		pPictureIn->frame.frame.frameCropTopOffset;
	in.frame.frameCropBottomOffset = pPictureOut->frame.frame.frameCropBottomOffset =
		pPictureIn->frame.frame.frameCropBottomOffset;
	in.frame.opt = pPictureIn->frame.frame.opt;
	in.frame.pPicture[0] = pPictureIn->frame.frame.pPicture[0];
	in.frame.pPicture[1] = pPictureIn->frame.frame.pPicture[1];
	in.pPlane[0] = pPictureIn->frame.pPlane[0];
	in.pPlane[1] = pPictureIn->frame.pPlane[1];
	in.pPlane[2] = pPictureIn->frame.pPlane[2];
	in.reserved[0] = pPictureIn->frame.reserved[0];
	in.reserved[1] = pPictureIn->frame.reserved[1];
	in.reserved[2] = pPictureIn->frame.reserved[2];
	if (in.frame.horizontalSize <= in.frame.frameWidth &&
	    in.frame.verticalSize <= in.frame.frameHeight) {
		in.frame.frameWidth = in.frame.horizontalSize;
		in.frame.frameHeight = in.frame.verticalSize;
	}
	return videocodecCscInline(&pPictureOut->frame, &in, cscCoefficient, videocodecCscLockModes, 1);
}
