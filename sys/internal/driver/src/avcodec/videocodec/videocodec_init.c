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

VideocodecResident videocodecResident = {
	.processLocalStorageId = -1,
};

struct ThreadmgrFastMutex videocodecTileRegMutex;

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

#if FW_DEBUG
SCE_DBGINFO_DEFINE(static, videocodecGlobalStorage, 124, "_GetVideocodecGlobal");
#endif

void videocodecNotifyProcessChange(void)
{
	if (videocodecResident.notifyProcessChange != 0)
		videocodecResident.notifyProcessChange();
}

/* The preceding decoder helpers await their complete CodecEngine owner. */
int SceAvcodecForDriver_C84AFFC4(void (*callback)(void))
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
	void *storage = 0;

	if (pid != 0) {
		if (SceProcessmgrForDriver_AF80F39C(pid,
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

/* Process create/exit await the complete process-local decoder owner. */
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

/* Intervening decoder operations await the complete process owner. */
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
	int ret = (int)0x80621802;

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
