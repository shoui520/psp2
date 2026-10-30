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

/* not a game process: no budget for the library's memory block */
#define VIDEOCODEC_ERROR_NOT_GAME_PROCESS	(-2141059008)	/* 0x80620840 */

int dsp0VideocodecQueryLibraryMemory(const Dsp0VideocodecInitParam *param,
				   SceSize *memorySize);
int dsp0VideocodecTermLibraryForProcess(unsigned int library, SceUID pid);
int videocodecDeleteDecoderForProcess(SceAvcdecCtrl *pCtrl, unsigned int flags, SceUID pid);
int m4vdecDeleteDecoderForProcess(SceAvcdecCtrl *pCtrl, SceUID pid);
int avcencDeleteEncoderForProcess(AvcencControl *pCtrl, SceUID pid);


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
		decoder->decodeFlags = 0;
		memset(decoder->decodeState, 0, sizeof(decoder->decodeState));
		decoder->decodeStatus[0] = 0;
		decoder->decodeStatus[1] = 0;
		decoder->decodeStatus[2] = 0;
		decoder->decodeStatus[3] = 0;
		decoder->pictureFlags = 0;
		decoder->pictureStatus[0] = 0;
		decoder->pictureStatus[1] = 0;
		decoder->pictureStatus[2] = 0;
		decoder->pictureStatus[3] = 0;
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
	global->dsp0MemoryFlags = 0;
	global->dsp0Config = 0;
	for (i = 0; i < 4; i++) {
		VideocodecDsp0Decoder *decoder = &global->dsp0Decoders[i];

		decoder->info.horizontal = 1280;
		decoder->info.vertical = 720;
		decoder->info.numOfRefFrames = 9;
		decoder->mode = 0;
		decoder->handle = 0;
		decoder->decodeFlags = 0;
		decoder->decodeStatus[0] = 0;
		decoder->decodeStatus[1] = 0;
		decoder->decodeStatus[2] = 0;
		decoder->decodeStatus[3] = 0;
		decoder->pictureFlags = 0;
		decoder->pictureStatus[0] = 0;
		decoder->pictureStatus[1] = 0;
		decoder->pictureStatus[2] = 0;
		decoder->pictureStatus[3] = 0;
		decoder->pictureStatus[4] = 0;
		decoder->pictureStatus[5] = 0;
		decoder->pictureStatus[6] = 0;
		decoder->pictureStatus[7] = 0;
		videocodecInitLockedRanges(&decoder->lockedRanges, 0, decoder->lockedRange, 1,
			VIDEOCODEC_DECODER_ERROR_BASE);
	}
	global->encoders[0].handle = 0;
	global->encoders[0].encodeFlags = 0;
	global->encoders[0].pictureFlags = 0;
	global->encoders[0].encodeStatus[0] = 0;
	global->encoders[0].encodeStatus[1] = 0;
	global->encoders[0].encodeStatus[2] = 0;
	global->encoders[0].encodeStatus[3] = 0;
	global->encoders[0].encodeStatus[4] = 0;
	global->encoders[0].encodeStatus[5] = 0;
	global->encoders[0].encodeStatus[6] = 0;
	global->encoders[0].encodeStatus[7] = 0;
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
