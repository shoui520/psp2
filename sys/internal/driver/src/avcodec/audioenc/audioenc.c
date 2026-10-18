/* SceAudioenc control and information handling (Sony audioenc/audioenc.c). */
#include <psp2/audioenc.h>
#include <sysmem/sysmem.h>
#include <sysmem/debug.h>
#include <intrmgr/syscall.h>
#include <stdint.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/fast_mutex_object.h>
#include <string.h>
#include "audioenc_internal.h"
#include <processmgr/processmgr.h>

AudioencResident audioencResident;

static const SceUInt32 audioencChannelContextSize[2] = { 0, 0 };
static const SceUInt32 audioencEncoderContextSize[2] = { 0, 0 };
static const char *const audioencMutexNames[2] = {
	"SceAudioencCelp", "SceAudioencSilk"
};

SCE_DBGINFO_DEFINE(static, copyOutInfoSource, 631, "_copyOutSceAudioenc");
SCE_DBGINFO_DEFINE(static, copyOutInfoDestination, 632, "_copyOutSceAudioenc");
SCE_DBGINFO_DEFINE(static, copyOutCtrlInfo, 776, "_copyOutSceAudioencCtrl");
SCE_DBGINFO_DEFINE(static, processCreateStorage, 232, "_sceAudioencProcessCreateEntry");
SCE_DBGINFO_DEFINE(static, processExitStorage, 310, "_sceAudioencProcessExitEntry");
SCE_DBGINFO_DEFINE(static, residentOwnerGet, 345, "_audioencGetResidentEncoderOwner");
SCE_DBGINFO_DEFINE(static, residentOwnerSet, 351, "_audioencSetResidentEncoderOwner");
SCE_DBGINFO_DEFINE(static, residentOwnerClear, 358, "_audioencClearResidentEncoderOwner");
SCE_DBGINFO_DEFINE(static, copyInInfoSource, 598, "_copyInSceAudioenc");
SCE_DBGINFO_DEFINE(static, workManagerStorage, 371, "_getWorkManager");
SCE_DBGINFO_DEFINE(static, initLibraryBudget, 415, "sceAudioencInitLibraryForUser");
SCE_DBGINFO_DEFINE(static, termLibraryBudget, 493, "sceAudioencTermLibraryForUser");
SCE_DBGINFO_DEFINE(static, createEncoderBudget, 1211, "sceAudioencCreateEncoderForUser");
SCE_DBGINFO_DEFINE(static, deleteEncoderBudget, 1390, "sceAudioencDeleteEncoderForUser");
SCE_DBGINFO_DEFINE(static, copyInCtrlInfo, 691, "_copyInSceAudioencCtrl");
SCE_DBGINFO_DEFINE(static, copyInCtrlInfoDestination, 692, "_copyInSceAudioencCtrl");

int _lockProcessSuspendCore(void);
int _unlockProcessSuspendCore(void);

int _copyOutSceAudioencCtrl(SceAudioencCtrl *userCtrl, SceAudioencInfo *userInfo,
			  SceAudioencOptInfo *userOptInfo, SceAudioencCtrl *ctrl)
{
	SceAudioencInfo *info;
	SceAudioencOptInfo *optInfo;
	int ret;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
	int hasUserInfo = userInfo != NULL;

	SceDebugForDriver_35A35322(hasUserInfo, &copyOutCtrlInfo, caller);
#endif
	optInfo = ctrl->pOptInfo;
	if (optInfo && userOptInfo) {
#if FW_DEBUG
		SceDebugForDriver_35A35322(1, &copyOutInfoSource, caller);
		SceDebugForDriver_35A35322(1, &copyOutInfoDestination, caller);
#endif
		ret = SceSysmemForDriver_6D88EF8A(userOptInfo, optInfo, optInfo->size);
		if (ret < 0)
			return ret;
	}
	info = ctrl->pInfo;
#if FW_DEBUG
	SceDebugForDriver_35A35322(info != NULL, &copyOutInfoSource, caller);
	SceDebugForDriver_35A35322(hasUserInfo, &copyOutInfoDestination, caller);
#endif
	ret = SceSysmemForDriver_6D88EF8A(userInfo, info, info->size);
	if (ret < 0)
		return ret;
	ctrl->pInfo = userInfo;
	ctrl->pOptInfo = userOptInfo;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyOutInfoSource, caller);
	SceDebugForDriver_35A35322(userCtrl != NULL, &copyOutInfoDestination, caller);
#endif
	return SceSysmemForDriver_6D88EF8A(userCtrl, ctrl, ctrl->size);
}

int initializeAudioenc(void)
{
	SceAudioencInitParam param;
	int ret;

	memset(&audioencResident, 0, sizeof(audioencResident));
	audioencResident.manager.codecType = SCE_AUDIOENC_TYPE_CELP;
	audioencResident.manager.pEncoders = audioencResident.encoders;
	audioencResident.manager.pWork = audioencResident.work;
	audioencResident.processStorage = -1;
	audioencResident.owners[0] = -1;
	audioencResident.manager.maxEncoders = 1;
	ret = sceKernelInitializeFastMutex((SceKernelFastMutex *)&audioencResident.mutex,
					  "SceAudioencResident", 0x8000, NULL);
	if (ret < 0)
		return ret;
	audioencResident.manager.pMutex = &audioencResident.mutex;
	param.celp.size = sizeof(param.celp);
	param.celp.totalStreams = 1;
	ret = _audioencInternalInitLibrary(&audioencResident.manager, &param, 1);
	if (ret < 0)
		return ret;
	ret = SceProcessmgrForDriver_3801D7D6("SceAudioenc", sizeof(AudioencProcess));
	if (ret >= 0)
		audioencResident.processStorage = ret;
	return ret;
}

int _sceAudioencProcessCreateEntry(SceUID pid)
{
	AudioencProcess *process = NULL;
	unsigned int codec, encoder, channel;
	int ret;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	ret = SceProcessmgrForDriver_AF80F39C(pid,
			audioencResident.processStorage, (void **)&process, 1);
	if (ret < 0)
		return ret;
#if FW_DEBUG
	SceDebugForDriver_35A35322(process != NULL, &processCreateStorage, caller);
#endif
	memset(process, 0, sizeof(*process));
	process->internal.managers[1].maxEncoders = 0;
	process->internal.managers[1].pEncoders = NULL;
	process->internal.managers[1].pWork = NULL;
	process->external.managers[1].maxEncoders = 0;
	process->external.managers[1].totalEncoders = 0;
	process->external.managers[1].pEncoders = NULL;
	process->external.managers[1].pWork = NULL;
	process->internal.managers[0].pEncoders = process->internal.encoders;
	process->external.managers[0].maxEncoders = 1;
	process->external.managers[0].totalEncoders = 1;
	process->internal.managers[0].pWork = process->internal.work;
	process->external.managers[0].pEncoders = process->external.encoders;
	process->external.managers[0].pWork = process->external.work;
	process->internal.managers[0].codecType = SCE_AUDIOENC_TYPE_CELP;
	process->external.managers[0].codecType = SCE_AUDIOENC_TYPE_CELP;
	process->internal.managers[0].maxEncoders = 1;
	process->internal.managers[1].codecType = AUDIOENC_TYPE_SILK;
	process->external.managers[1].codecType = AUDIOENC_TYPE_SILK;
	for (codec = 0; codec < 2; codec++) {
		AudioencWorkManager *external = &process->external.managers[codec];

		for (encoder = 0; encoder < external->totalEncoders; encoder++)
			external->pEncoders[encoder].contextSize = audioencEncoderContextSize[codec];
		if (external->pChannels) {
			for (channel = 0; channel < external->totalChannels; channel++) {
				external->pChannels[channel].contextSize = audioencChannelContextSize[codec];
				external->pChannels[channel].encoderIndex = -1;
			}
		}
		ret = sceKernelInitializeFastMutex((SceKernelFastMutex *)&process->mutexes[codec],
						  audioencMutexNames[codec], 0x8000, NULL);
		if (ret < 0)
			break;
		process->internal.managers[codec].pMutex = &process->mutexes[codec];
		external->pMutex = &process->mutexes[codec];
	}
	return ret;
}

int _sceAudioencProcessExitEntry(SceUID pid)
{
	AudioencProcess *process = NULL;
	unsigned int codec;
	int ret, status;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	ret = SceProcessmgrForDriver_AF80F39C(pid,
			audioencResident.processStorage, (void **)&process, 0);
	status = ret < 0 ? ret : 0;
#if FW_DEBUG
	SceDebugForDriver_35A35322(process != NULL, &processExitStorage, caller);
#endif
	for (codec = 0; codec < 2; codec++) {
		ret = _audioencInternalExitLibrary(pid, &process->internal.managers[codec], 0, 0);
		if (ret < 0)
			status = ret;
		ret = _audioencInternalExitLibrary(pid, &process->external.managers[codec], 1, 0);
		if (ret < 0)
			status = ret;
		ret = sceKernelFinalizeFastMutex((SceKernelFastMutex *)&process->mutexes[codec]);
		if (ret < 0)
			status = ret;
	}
	ret = _audioencInternalExitLibrary(pid, &audioencResident.manager, 0, 1);
	return ret < 0 ? ret : status;
}

SceUID _audioencGetResidentEncoderOwner(unsigned int encoder)
{
#if FW_DEBUG
	SceDebugForDriver_35A35322(encoder == 0, &residentOwnerGet, __builtin_return_address(0));
#endif
	return audioencResident.owners[encoder];
}

int _audioencSetResidentEncoderOwner(unsigned int encoder)
{
#if FW_DEBUG
	SceDebugForDriver_35A35322(encoder == 0, &residentOwnerSet, __builtin_return_address(0));
#endif
	audioencResident.owners[encoder] = sceKernelGetProcessId();
	return 0;
}

int _audioencClearResidentEncoderOwner(unsigned int encoder)
{
#if FW_DEBUG
	SceDebugForDriver_35A35322(encoder == 0, &residentOwnerClear, __builtin_return_address(0));
#endif
	audioencResident.owners[encoder] = -1;
	return 0;
}

int sceAudioencInitLibraryForUser(SceUInt32 codecType, SceAudioencInitParam *userParam)
{
	SceAudioencInitParam param;
	AudioencProcess *process;
	AudioencWorkManager *manager;
	SceUInt32 syscall = sceSyscallEnter();
	unsigned int index;
	int ret;
#if FW_DEBUG
	void *caller = __builtin_return_address(0);
#endif

	ret = sceKernelIsGameBudget();
	if (!ret) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0, &initLibraryBudget,
			"Warning: sceAudioencInitLibrary must not be called by non-GAME process\n");
#endif
		goto leave;
	}
	if (codecType == AUDIOENC_TYPE_SILK) {
		ret = SCE_AUDIOENC_ERROR_UNSUPPORTED;
		goto leave;
	}
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
	if (codecType - SCE_AUDIOENC_TYPE_CELP > 1) {
		ret = SCE_AUDIOENC_ERROR_INVALID_TYPE;
		goto unlock;
	}
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInInfoSource, caller);
#endif
	if (!userParam || ((uintptr_t)userParam & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(&param, userParam, sizeof(param));
	if (ret < 0)
		goto unlock;
	if (param.size != sizeof(param)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	process = SceProcessmgrForDriver_EE694840(audioencResident.processStorage);
#if FW_DEBUG
	SceDebugForDriver_35A35322(process != NULL, &workManagerStorage, caller);
#endif
	if (codecType == process->internal.managers[0].codecType)
		index = 0;
	else if (codecType == process->internal.managers[1].codecType)
		index = 1;
	else {
		ret = SCE_AUDIOENC_ERROR_INVALID_TYPE;
		goto unlock;
	}
	manager = &process->internal.managers[index];
	ret = lockFastMutex((SceKernelFastMutex *)manager->pMutex);
	if (ret < 0)
		goto unlock;
	ret = _audioencInternalInitLibrary(manager, &param, 1);
	sceKernelUnlockFastMutex((SceKernelFastMutex *)manager->pMutex);
	if (ret >= 0) {
#if FW_DEBUG
		SceDebugForDriver_35A35322(1, &copyOutInfoSource, caller);
		SceDebugForDriver_35A35322(1, &copyOutInfoDestination, caller);
#endif
		ret = SceSysmemForDriver_6D88EF8A(userParam, &param, param.size);
	}
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int sceAudioencTermLibraryForUser(SceUInt32 codecType)
{
	AudioencProcess *process;
	AudioencWorkManager *manager;
	SceUInt32 syscall = sceSyscallEnter();
	unsigned int index;
	int ret;
#if FW_DEBUG
	void *caller = __builtin_return_address(0);
#endif

	ret = sceKernelIsGameBudget();
	if (!ret) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0, &termLibraryBudget,
			"Warning: sceAudioencTermLibrary must not be called by non-GAME process\n");
#endif
		goto leave;
	}
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
	process = SceProcessmgrForDriver_EE694840(audioencResident.processStorage);
#if FW_DEBUG
	SceDebugForDriver_35A35322(process != NULL, &workManagerStorage, caller);
#endif
	if (codecType == process->internal.managers[0].codecType)
		index = 0;
	else if (codecType == process->internal.managers[1].codecType)
		index = 1;
	else {
		ret = SCE_AUDIOENC_ERROR_INVALID_TYPE;
		goto unlock;
	}
	manager = &process->internal.managers[index];
	ret = lockFastMutex((SceKernelFastMutex *)manager->pMutex);
	if (ret >= 0) {
		ret = _audioencInternalTermLibrary(manager, 1);
		sceKernelUnlockFastMutex((SceKernelFastMutex *)manager->pMutex);
	}
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int sceAudioencGetContextSizeForUser(const SceAudioencCtrl *userCtrl,
				   SceUInt32 codecType)
{
	SceAudioencCtrl ctrl;
	AudioencInfo info;
	SceAudioencOptInfo optInfo;
	SceAudioencInfo *kernelInfo;
	SceAudioencOptInfo *kernelOptInfo;
	const SceAudioencInfo *userInfo;
	const SceAudioencOptInfo *userOptInfo;
	SceUInt32 copyType, infoSize, index, bitRate;
	SceUInt32 syscall = sceSyscallEnter();
	int ret, invalidSize;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	ctrl.pInfo = &info.publicInfo;
	ctrl.pOptInfo = &optInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInCtrlInfo, caller);
	SceDebugForDriver_35A35322(1, &copyInCtrlInfoDestination, caller);
#endif
	copyType = codecType ? codecType : (SceUInt32)-1;
	kernelInfo = ctrl.pInfo;
	kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInInfoSource, caller);
#endif
	if (!userCtrl || ((uintptr_t)userCtrl & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(&ctrl, userCtrl, sizeof(ctrl));
	if (ret < 0)
		goto unlock;
	if (ctrl.size - 40 > 4 || (ctrl.size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	userInfo = ctrl.pInfo;
	userOptInfo = ctrl.pOptInfo;
	ctrl.pInfo = kernelInfo;
	ctrl.pOptInfo = kernelOptInfo;
	if (copyType == SCE_AUDIOENC_TYPE_CELP)
		infoSize = sizeof(SceAudioencInfoCelp);
	else if (copyType == AUDIOENC_TYPE_SILK)
		infoSize = sizeof(AudioencInfoSilk);
	else {
		ret = SCE_AUDIOENC_ERROR_INVALID_TYPE;
		goto unlock;
	}
#if FW_DEBUG
	SceDebugForDriver_35A35322(kernelInfo != NULL, &copyInInfoSource, caller);
#endif
	if (!userInfo || ((uintptr_t)userInfo & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(kernelInfo, userInfo, infoSize);
	if (ret < 0)
		goto unlock;
	invalidSize = infoSize < kernelInfo->size;
	if (infoSize > kernelInfo->size)
		invalidSize |= 1;
	if (invalidSize || (kernelInfo->size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	if (userOptInfo) {
		if (copyType != SCE_AUDIOENC_TYPE_CELP) {
			ret = copyType == AUDIOENC_TYPE_SILK ?
				SCE_AUDIOENC_ERROR_UNSUPPORTED : SCE_AUDIOENC_ERROR_INVALID_TYPE;
			goto unlock;
		}
		kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
		SceDebugForDriver_35A35322(kernelOptInfo != NULL, &copyInInfoSource, caller);
#endif
		if (((uintptr_t)userOptInfo & 3) ||
		    SceSysmemForDriver_BC996A7A(kernelOptInfo, userOptInfo, sizeof(*kernelOptInfo)) < 0 ||
		    kernelOptInfo->size != sizeof(*kernelOptInfo))
			ctrl.pOptInfo = NULL;
	} else {
		ctrl.pOptInfo = NULL;
	}
	if (ctrl.wordLength != 16) {
		ret = SCE_AUDIOENC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	if (codecType == SCE_AUDIOENC_TYPE_CELP) {
		if (ctrl.pInfo->celp.excitationMode != 0 || ctrl.pInfo->celp.samplingRate != 8000)
			goto invalidCelp;
		bitRate = ctrl.pInfo->celp.bitRate;
		if (bitRate != 3850 && bitRate != 4650 && bitRate != 5700 &&
		    bitRate != 6600 && bitRate != 7300 && bitRate != 8700 &&
		    bitRate != 9900 && bitRate != 10700 && bitRate != 11800 && bitRate != 12200)
			goto invalidCelp;
		index = 0;
	} else if (codecType == AUDIOENC_TYPE_SILK) {
		if (info.silk.samplingRate != 12000 && info.silk.samplingRate != 16000 &&
		    info.silk.samplingRate != 8000) {
			ret = AUDIOENC_SILK_ERROR_INVALID_SAMPLING_RATE;
			goto unlock;
		}
		index = 1;
	} else {
		ret = SCE_AUDIOENC_ERROR_INVALID_TYPE;
		goto unlock;
	}
	ret = audioencChannelContextSize[index] + audioencEncoderContextSize[index];
	goto unlock;
invalidCelp:
	ret = SCE_AUDIOENC_CELP_ERROR_INVALID_CONFIG;
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int sceAudioencCreateEncoderForUser(SceAudioencCtrl *userCtrl,
				   SceUInt32 codecType)
{
	SceAudioencCtrl ctrl;
	SceAudioencInitParam param;
	AudioencProcess *process;
	AudioencWorkManager *manager;
	int resident;
	AudioencInfo info;
	SceAudioencOptInfo optInfo;
	SceAudioencInfo *kernelInfo;
	SceAudioencOptInfo *kernelOptInfo;
	const SceAudioencInfo *userInfo;
	const SceAudioencOptInfo *userOptInfo;
	SceUInt32 copyType, infoSize, index, bitRate;
	SceUInt32 syscall = sceSyscallEnter();
	int ret, invalidSize;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	resident = !sceKernelIsGameBudget();
#if FW_DEBUG
	if (resident)
		SceDebugForDriver_D9703808(1, 0, &createEncoderBudget,
			"Warning: sceAudioencCreateEncoder must not be called by non-GAME process\n");
#endif
	if (codecType == AUDIOENC_TYPE_SILK) {
		ret = SCE_AUDIOENC_ERROR_UNSUPPORTED;
		goto leave;
	}
	ctrl.pInfo = &info.publicInfo;
	ctrl.pOptInfo = &optInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInCtrlInfo, caller);
	SceDebugForDriver_35A35322(1, &copyInCtrlInfoDestination, caller);
#endif
	copyType = codecType ? codecType : (SceUInt32)-1;
	kernelInfo = ctrl.pInfo;
	kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInInfoSource, caller);
#endif
	if (!userCtrl || ((uintptr_t)userCtrl & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(&ctrl, userCtrl, sizeof(ctrl));
	if (ret < 0)
		goto unlock;
	if (ctrl.size - 40 > 4 || (ctrl.size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	userInfo = ctrl.pInfo;
	userOptInfo = ctrl.pOptInfo;
	ctrl.pInfo = kernelInfo;
	ctrl.pOptInfo = kernelOptInfo;
	if (copyType == SCE_AUDIOENC_TYPE_CELP)
		infoSize = sizeof(SceAudioencInfoCelp);
	else if (copyType == AUDIOENC_TYPE_SILK)
		infoSize = sizeof(AudioencInfoSilk);
	else {
		ret = SCE_AUDIOENC_ERROR_INVALID_TYPE;
		goto unlock;
	}
#if FW_DEBUG
	SceDebugForDriver_35A35322(kernelInfo != NULL, &copyInInfoSource, caller);
#endif
	if (!userInfo || ((uintptr_t)userInfo & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(kernelInfo, userInfo, infoSize);
	if (ret < 0)
		goto unlock;
	invalidSize = infoSize < kernelInfo->size;
	if (infoSize > kernelInfo->size)
		invalidSize |= 1;
	if (invalidSize || (kernelInfo->size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	if (userOptInfo) {
		if (copyType != SCE_AUDIOENC_TYPE_CELP) {
			ret = copyType == AUDIOENC_TYPE_SILK ?
				SCE_AUDIOENC_ERROR_UNSUPPORTED : SCE_AUDIOENC_ERROR_INVALID_TYPE;
			goto unlock;
		}
		kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
		SceDebugForDriver_35A35322(kernelOptInfo != NULL, &copyInInfoSource, caller);
#endif
		if (((uintptr_t)userOptInfo & 3) ||
		    SceSysmemForDriver_BC996A7A(kernelOptInfo, userOptInfo, sizeof(*kernelOptInfo)) < 0 ||
		    kernelOptInfo->size != sizeof(*kernelOptInfo))
			ctrl.pOptInfo = NULL;
	} else {
		ctrl.pOptInfo = NULL;
	}
	if (ctrl.wordLength != 16) {
		ret = SCE_AUDIOENC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	if (codecType == SCE_AUDIOENC_TYPE_CELP) {
		if (ctrl.pInfo->celp.excitationMode != 0 || ctrl.pInfo->celp.samplingRate != 8000)
			goto invalidCelp;
		bitRate = ctrl.pInfo->celp.bitRate;
		if (bitRate != 3850 && bitRate != 4650 && bitRate != 5700 &&
		    bitRate != 6600 && bitRate != 7300 && bitRate != 8700 &&
		    bitRate != 9900 && bitRate != 10700 && bitRate != 11800 && bitRate != 12200)
			goto invalidCelp;
		index = 0;
	} else if (codecType == AUDIOENC_TYPE_SILK) {
		if (info.silk.samplingRate != 12000 && info.silk.samplingRate != 16000 &&
		    info.silk.samplingRate != 8000) {
			ret = AUDIOENC_SILK_ERROR_INVALID_SAMPLING_RATE;
			goto unlock;
		}
		index = 1;
	} else {
		ret = SCE_AUDIOENC_ERROR_INVALID_TYPE;
		goto unlock;
	}
	if (resident && audioencResident.manager.codecType != codecType) {
		param.celp.size = sizeof(param.celp);
		param.celp.totalStreams = 1;
		ret = _audioencInternalTermLibrary(&audioencResident.manager, 0);
		if (ret < 0)
			goto unlock;
		audioencResident.manager.codecType = codecType;
		audioencResident.manager.maxEncoders = 1;
		ret = _audioencInternalInitLibrary(&audioencResident.manager, &param, 0);
		if (ret < 0)
			goto unlock;
	}
	ctrl.maxEsSize = 27;
	ctrl.maxPcmSize = 640;
	process = SceProcessmgrForDriver_EE694840(audioencResident.processStorage);
#if FW_DEBUG
	SceDebugForDriver_35A35322(process != NULL, &workManagerStorage, caller);
#endif
	if (resident) {
		manager = &audioencResident.manager;
	} else {
		if (codecType == process->internal.managers[0].codecType)
			index = 0;
		else if (codecType == process->internal.managers[1].codecType)
			index = 1;
		else {
			ret = SCE_AUDIOENC_ERROR_INVALID_TYPE;
			goto unlock;
		}
		manager = &process->internal.managers[index];
	}
	ret = lockFastMutex((SceKernelFastMutex *)manager->pMutex);
	if (ret >= 0) {
		ret = _audioencInternalCreateEncoder(manager, &ctrl, 0, 0, resident);
		sceKernelUnlockFastMutex((SceKernelFastMutex *)manager->pMutex);
		if (ret >= 0)
			ret = _copyOutSceAudioencCtrl(userCtrl, (SceAudioencInfo *)userInfo,
						    (SceAudioencOptInfo *)userOptInfo, &ctrl);
	}
	goto unlock;
invalidCelp:
	ret = SCE_AUDIOENC_CELP_ERROR_INVALID_CONFIG;
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int sceAudioencCreateEncoderResidentForUser(SceAudioencCtrl *userCtrl,
					  SceUInt32 codecType)
{
	SceAudioencCtrl ctrl;
	AudioencInfo info;
	SceAudioencOptInfo optInfo;
	SceAudioencInitParam param;
	SceAudioencInfo *kernelInfo, *userInfo;
	SceAudioencOptInfo *kernelOptInfo, *userOptInfo;
	AudioencProcess *process;
	SceUInt32 copyType, infoSize, bitRate, pcmSize;
	SceUInt32 syscall = sceSyscallEnter();
	int ret, invalidSize;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	ctrl.pInfo = &info.publicInfo;
	ctrl.pOptInfo = &optInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInCtrlInfo, caller);
	SceDebugForDriver_35A35322(1, &copyInCtrlInfoDestination, caller);
#endif
	kernelInfo = ctrl.pInfo;
	copyType = codecType ? codecType : (SceUInt32)-1;
	kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInInfoSource, caller);
#endif
	if (!userCtrl || ((uintptr_t)userCtrl & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(&ctrl, userCtrl, sizeof(ctrl));
	if (ret < 0)
		goto unlock;
	if (ctrl.size - 40 > 4 || (ctrl.size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	userInfo = ctrl.pInfo;
	ctrl.pInfo = kernelInfo;
	userOptInfo = ctrl.pOptInfo;
	ctrl.pOptInfo = kernelOptInfo;
	if (copyType == SCE_AUDIOENC_TYPE_CELP)
		infoSize = sizeof(SceAudioencInfoCelp);
	else if (copyType == AUDIOENC_TYPE_SILK)
		infoSize = sizeof(AudioencInfoSilk);
	else {
		ret = SCE_AUDIOENC_ERROR_INVALID_TYPE;
		goto unlock;
	}
#if FW_DEBUG
	SceDebugForDriver_35A35322(kernelInfo != NULL, &copyInInfoSource, caller);
#endif
	if (!userInfo || ((uintptr_t)userInfo & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(kernelInfo, userInfo, infoSize);
	if (ret < 0)
		goto unlock;
	invalidSize = infoSize < kernelInfo->size;
	if (infoSize > kernelInfo->size)
		invalidSize |= 1;
	if (invalidSize || (kernelInfo->size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	if (userOptInfo) {
		if (copyType != SCE_AUDIOENC_TYPE_CELP) {
			ret = SCE_AUDIOENC_ERROR_UNSUPPORTED;
			goto unlock;
		}
		kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
		SceDebugForDriver_35A35322(kernelOptInfo != NULL, &copyInInfoSource, caller);
#endif
		if (((uintptr_t)userOptInfo & 3) ||
		    SceSysmemForDriver_BC996A7A(kernelOptInfo, userOptInfo, sizeof(*kernelOptInfo)) < 0 ||
		    kernelOptInfo->size != sizeof(*kernelOptInfo))
			ctrl.pOptInfo = NULL;
	} else {
		ctrl.pOptInfo = NULL;
	}
	if (ctrl.wordLength != 16) {
		ret = SCE_AUDIOENC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	if (codecType == SCE_AUDIOENC_TYPE_CELP) {
		if (ctrl.pInfo->celp.excitationMode != 0 || ctrl.pInfo->celp.samplingRate != 8000)
			goto invalidCelp;
		bitRate = ctrl.pInfo->celp.bitRate;
		if (bitRate != 3850 && bitRate != 4650 && bitRate != 5700 &&
		    bitRate != 6600 && bitRate != 7300 && bitRate != 8700 &&
		    bitRate != 9900 && bitRate != 10700 && bitRate != 11800 && bitRate != 12200)
			goto invalidCelp;
	} else if (codecType == AUDIOENC_TYPE_SILK) {
		if (((AudioencInfo *)ctrl.pInfo)->silk.samplingRate != 12000 &&
		    ((AudioencInfo *)ctrl.pInfo)->silk.samplingRate != 16000 &&
		    ((AudioencInfo *)ctrl.pInfo)->silk.samplingRate != 8000) {
			ret = AUDIOENC_SILK_ERROR_INVALID_SAMPLING_RATE;
			goto unlock;
		}
	} else {
		ret = SCE_AUDIOENC_ERROR_INVALID_TYPE;
		goto unlock;
	}
	if (audioencResident.manager.codecType != codecType) {
		param.celp.size = sizeof(param.celp);
		param.celp.totalStreams = 1;
		ret = _audioencInternalTermLibrary(&audioencResident.manager, 0);
		if (ret < 0)
			goto unlock;
		audioencResident.manager.maxEncoders = 1;
		audioencResident.manager.codecType = codecType;
		ret = _audioencInternalInitLibrary(&audioencResident.manager, &param, 0);
		if (ret < 0)
			goto unlock;
	}
	ctrl.maxEsSize = codecType == AUDIOENC_TYPE_SILK ? 150 : 27;
	if (codecType == AUDIOENC_TYPE_SILK) {
		pcmSize = ((AudioencInfo *)ctrl.pInfo)->silk.samplingRate / 1000;
		if (5 * pcmSize)
			ctrl.maxPcmSize = 40 * pcmSize;
	} else {
		ctrl.maxPcmSize = 640;
	}
	process = SceProcessmgrForDriver_EE694840(audioencResident.processStorage);
#if FW_DEBUG
	SceDebugForDriver_35A35322(process != NULL, &workManagerStorage, caller);
#else
	(void)process;
#endif
	ret = lockFastMutex((SceKernelFastMutex *)audioencResident.manager.pMutex);
	if (ret >= 0) {
		ret = _audioencInternalCreateEncoder(&audioencResident.manager, &ctrl, 0, 0, 1);
		sceKernelUnlockFastMutex((SceKernelFastMutex *)audioencResident.manager.pMutex);
		if (ret >= 0)
			ret = _copyOutSceAudioencCtrl(userCtrl, userInfo, userOptInfo, &ctrl);
	}
	goto unlock;
invalidCelp:
	ret = SCE_AUDIOENC_CELP_ERROR_INVALID_CONFIG;
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int sceAudioencCreateEncoderExternalForUser(SceAudioencCtrl *userCtrl,
					  SceUInt32 codecType, SceUIntVAddr context,
					  SceUInt32 contextSize)
{
	SceAudioencCtrl ctrl;
	AudioencInfo info;
	SceAudioencOptInfo optInfo;
	SceAudioencInfo *kernelInfo, *userInfo;
	SceAudioencOptInfo *kernelOptInfo, *userOptInfo;
	AudioencProcess *process;
	AudioencWorkManager *manager;
	SceUInt32 copyType, infoSize, bitRate, index, pcmSize;
	SceUInt32 syscall = sceSyscallEnter();
	int ret, invalidSize;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	if (!context) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto leave;
	}
	ctrl.pInfo = &info.publicInfo;
	ctrl.pOptInfo = &optInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInCtrlInfo, caller);
	SceDebugForDriver_35A35322(1, &copyInCtrlInfoDestination, caller);
#endif
	copyType = codecType ? codecType : (SceUInt32)-1;
	kernelInfo = ctrl.pInfo;
	kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInInfoSource, caller);
#endif
	if (!userCtrl || ((uintptr_t)userCtrl & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(&ctrl, userCtrl, sizeof(ctrl));
	if (ret < 0)
		goto unlock;
	if (ctrl.size - 40 > 4 || (ctrl.size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	userInfo = ctrl.pInfo;
	userOptInfo = ctrl.pOptInfo;
	ctrl.pInfo = kernelInfo;
	ctrl.pOptInfo = kernelOptInfo;
	if (copyType == SCE_AUDIOENC_TYPE_CELP)
		infoSize = sizeof(SceAudioencInfoCelp);
	else if (copyType == AUDIOENC_TYPE_SILK)
		infoSize = sizeof(AudioencInfoSilk);
	else {
		ret = SCE_AUDIOENC_ERROR_INVALID_TYPE;
		goto unlock;
	}
#if FW_DEBUG
	SceDebugForDriver_35A35322(kernelInfo != NULL, &copyInInfoSource, caller);
#endif
	if (!userInfo || ((uintptr_t)userInfo & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(kernelInfo, userInfo, infoSize);
	if (ret < 0)
		goto unlock;
	invalidSize = infoSize < kernelInfo->size;
	if (infoSize > kernelInfo->size)
		invalidSize |= 1;
	if (invalidSize || (kernelInfo->size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	if (userOptInfo) {
		if (copyType != SCE_AUDIOENC_TYPE_CELP) {
			ret = copyType == AUDIOENC_TYPE_SILK ?
				SCE_AUDIOENC_ERROR_UNSUPPORTED : SCE_AUDIOENC_ERROR_INVALID_TYPE;
			goto unlock;
		}
		kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
		SceDebugForDriver_35A35322(kernelOptInfo != NULL, &copyInInfoSource, caller);
#endif
		if (((uintptr_t)userOptInfo & 3) ||
		    SceSysmemForDriver_BC996A7A(kernelOptInfo, userOptInfo, sizeof(*kernelOptInfo)) < 0 ||
		    kernelOptInfo->size != sizeof(*kernelOptInfo))
			ctrl.pOptInfo = NULL;
	} else {
		ctrl.pOptInfo = NULL;
	}
	if (ctrl.wordLength != 16) {
		ret = SCE_AUDIOENC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	if (codecType == SCE_AUDIOENC_TYPE_CELP) {
		if (ctrl.pInfo->celp.excitationMode != 0 || ctrl.pInfo->celp.samplingRate != 8000)
			goto invalidCelp;
		bitRate = ctrl.pInfo->celp.bitRate;
		if (bitRate != 3850 && bitRate != 4650 && bitRate != 5700 &&
		    bitRate != 6600 && bitRate != 7300 && bitRate != 8700 &&
		    bitRate != 9900 && bitRate != 10700 && bitRate != 11800 && bitRate != 12200)
			goto invalidCelp;
		index = 0;
	} else if (codecType == AUDIOENC_TYPE_SILK) {
		if (((AudioencInfo *)ctrl.pInfo)->silk.samplingRate != 12000 &&
		    ((AudioencInfo *)ctrl.pInfo)->silk.samplingRate != 16000 &&
		    ((AudioencInfo *)ctrl.pInfo)->silk.samplingRate != 8000) {
			ret = AUDIOENC_SILK_ERROR_INVALID_SAMPLING_RATE;
			goto unlock;
		}
		index = 1;
	} else {
		ret = SCE_AUDIOENC_ERROR_INVALID_TYPE;
		goto unlock;
	}
	ret = audioencChannelContextSize[index] + audioencEncoderContextSize[index];
	if (ret < 0)
		goto unlock;
	if (!ret) {
		ret = SCE_AUDIOENC_ERROR_UNSUPPORTED;
		goto unlock;
	}
	if (contextSize != (SceUInt32)ret) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	ctrl.maxEsSize = codecType == AUDIOENC_TYPE_SILK ? 150 : 27;
	if (codecType == AUDIOENC_TYPE_SILK) {
		pcmSize = ((AudioencInfo *)ctrl.pInfo)->silk.samplingRate / 1000;
		if (5 * pcmSize)
			ctrl.maxPcmSize = 40 * pcmSize;
	} else {
		ctrl.maxPcmSize = 640;
	}
	process = SceProcessmgrForDriver_EE694840(audioencResident.processStorage);
#if FW_DEBUG
	SceDebugForDriver_35A35322(process != NULL, &workManagerStorage, caller);
#endif
	if (codecType == process->external.managers[0].codecType)
		index = 0;
	else if (codecType == process->external.managers[1].codecType)
		index = 1;
	else {
		ret = SCE_AUDIOENC_ERROR_INVALID_TYPE;
		goto unlock;
	}
	manager = &process->external.managers[index];
	ret = lockFastMutex((SceKernelFastMutex *)manager->pMutex);
	if (ret >= 0) {
		ret = _audioencInternalCreateEncoder(manager, &ctrl, context, contextSize, 0);
		sceKernelUnlockFastMutex((SceKernelFastMutex *)manager->pMutex);
		if (ret >= 0)
			ret = _copyOutSceAudioencCtrl(userCtrl, userInfo, userOptInfo, &ctrl);
	}
	goto unlock;
invalidCelp:
	ret = SCE_AUDIOENC_CELP_ERROR_INVALID_CONFIG;
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int sceAudioencDeleteEncoderForUser(SceAudioencCtrl *userCtrl)
{
	SceAudioencCtrl ctrl;
	AudioencInfo info;
	SceAudioencOptInfo optInfo;
	SceAudioencInfo *kernelInfo, *userInfo;
	SceAudioencOptInfo *kernelOptInfo, *userOptInfo;
	AudioencProcess *process;
	AudioencWorkManager *manager;
	SceUInt32 codecType, infoSize, index;
	SceUInt32 syscall = sceSyscallEnter();
	int ret, invalidSize, external, resident;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	resident = !sceKernelIsGameBudget();
#if FW_DEBUG
	if (resident)
		SceDebugForDriver_D9703808(1, 0, &deleteEncoderBudget,
			"Warning: sceAudioencDeleteEncoder must not be called by non-GAME process\n");
#endif
	ctrl.pInfo = &info.publicInfo;
	ctrl.pOptInfo = &optInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInCtrlInfo, caller);
	SceDebugForDriver_35A35322(1, &copyInCtrlInfoDestination, caller);
#endif
	kernelInfo = ctrl.pInfo;
	kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInInfoSource, caller);
#endif
	if (!userCtrl || ((uintptr_t)userCtrl & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(&ctrl, userCtrl, sizeof(ctrl));
	if (ret < 0)
		goto unlock;
	if (ctrl.size - 40 > 4 || (ctrl.size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	userInfo = ctrl.pInfo;
	userOptInfo = ctrl.pOptInfo;
	ctrl.pInfo = kernelInfo;
	ctrl.pOptInfo = kernelOptInfo;
	codecType = audioencGetHandleCodecType(ctrl.handle);
	if (codecType == SCE_AUDIOENC_TYPE_CELP)
		infoSize = sizeof(SceAudioencInfoCelp);
	else if (codecType == AUDIOENC_TYPE_SILK)
		infoSize = sizeof(AudioencInfoSilk);
	else
		goto invalidHandle;
	kernelInfo = ctrl.pInfo;
#if FW_DEBUG
	SceDebugForDriver_35A35322(kernelInfo != NULL, &copyInInfoSource, caller);
#endif
	if (!userInfo || ((uintptr_t)userInfo & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(kernelInfo, userInfo, infoSize);
	if (ret < 0)
		goto unlock;
	invalidSize = infoSize < kernelInfo->size;
	if (infoSize > kernelInfo->size)
		invalidSize |= 1;
	if (invalidSize || (kernelInfo->size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	if (userOptInfo) {
		if (codecType != SCE_AUDIOENC_TYPE_CELP) {
			ret = codecType == AUDIOENC_TYPE_SILK ?
				SCE_AUDIOENC_ERROR_UNSUPPORTED : SCE_AUDIOENC_ERROR_INVALID_HANDLE;
			goto unlock;
		}
		kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
		SceDebugForDriver_35A35322(kernelOptInfo != NULL, &copyInInfoSource, caller);
#endif
		if (((uintptr_t)userOptInfo & 3) ||
		    SceSysmemForDriver_BC996A7A(kernelOptInfo, userOptInfo, sizeof(*kernelOptInfo)) < 0 ||
		    kernelOptInfo->size != sizeof(*kernelOptInfo))
			ctrl.pOptInfo = NULL;
	} else {
		ctrl.pOptInfo = NULL;
	}
	if (ctrl.wordLength != 16) {
		ret = SCE_AUDIOENC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	if (audioencIsExternalHandle(ctrl.handle))
		goto invalidHandle;
	if (resident) {
		if (!audioencIsResidentHandle(ctrl.handle))
			goto invalidHandle;
		codecType = audioencGetHandleCodecType(ctrl.handle);
		audioencIsExternalHandle(ctrl.handle);
		process = SceProcessmgrForDriver_EE694840(audioencResident.processStorage);
#if FW_DEBUG
		SceDebugForDriver_35A35322(process != NULL, &workManagerStorage, caller);
#else
		(void)process;
#endif
		if (codecType - SCE_AUDIOENC_TYPE_CELP > 1)
			goto invalidHandle;
		manager = &audioencResident.manager;
	} else {
		if (audioencIsResidentHandle(ctrl.handle))
			goto invalidHandle;
		codecType = audioencGetHandleCodecType(ctrl.handle);
		external = audioencIsExternalHandle(ctrl.handle);
		process = SceProcessmgrForDriver_EE694840(audioencResident.processStorage);
#if FW_DEBUG
		SceDebugForDriver_35A35322(process != NULL, &workManagerStorage, caller);
#endif
		if (!external) {
			if (codecType == process->internal.managers[0].codecType)
				index = 0;
			else if (codecType == process->internal.managers[1].codecType)
				index = 1;
			else
				goto invalidHandle;
			manager = &process->internal.managers[index];
		} else {
			if (codecType == process->external.managers[0].codecType)
				index = 0;
			else if (codecType == process->external.managers[1].codecType)
				index = 1;
			else
				goto invalidHandle;
			manager = &process->external.managers[index];
		}
	}
	index = audioencGetHandleIndex(ctrl.handle);
	if (!manager->totalEncoders) {
		ret = SCE_AUDIOENC_ERROR_NOT_INITIALIZED;
		goto unlock;
	}
	if (index >= manager->totalEncoders)
		goto invalidHandle;
	if (!manager->pEncoders[index].active) {
		ret = SCE_AUDIOENC_ERROR_NOT_HANDLE_IN_USE;
		goto unlock;
	}
	ret = lockFastMutex((SceKernelFastMutex *)manager->pMutex);
	if (ret >= 0) {
		ret = _audioencInternalDeleteEncoder(manager, &ctrl, 0);
		sceKernelUnlockFastMutex((SceKernelFastMutex *)manager->pMutex);
		if (ret >= 0)
			ret = _copyOutSceAudioencCtrl(userCtrl, userInfo, userOptInfo, &ctrl);
	}
	goto unlock;
invalidHandle:
	ret = SCE_AUDIOENC_ERROR_INVALID_HANDLE;
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int sceAudioencDeleteEncoderResidentForUser(SceAudioencCtrl *userCtrl)
{
	SceAudioencCtrl ctrl;
	AudioencInfo info;
	SceAudioencOptInfo optInfo;
	SceAudioencInfo *kernelInfo, *userInfo;
	SceAudioencOptInfo *kernelOptInfo, *userOptInfo;
	AudioencProcess *process;
	SceUInt32 codecType, infoSize, index;
	SceUInt32 syscall = sceSyscallEnter();
	int ret, invalidSize;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	ctrl.pInfo = &info.publicInfo;
	ctrl.pOptInfo = &optInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInCtrlInfo, caller);
	SceDebugForDriver_35A35322(1, &copyInCtrlInfoDestination, caller);
#endif
	kernelInfo = ctrl.pInfo;
	kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInInfoSource, caller);
#endif
	if (!userCtrl || ((uintptr_t)userCtrl & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(&ctrl, userCtrl, sizeof(ctrl));
	if (ret < 0)
		goto unlock;
	if (ctrl.size - 40 > 4 || (ctrl.size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	userInfo = ctrl.pInfo;
	userOptInfo = ctrl.pOptInfo;
	ctrl.pInfo = kernelInfo;
	ctrl.pOptInfo = kernelOptInfo;
	codecType = audioencGetHandleCodecType(ctrl.handle);
	if (codecType == SCE_AUDIOENC_TYPE_CELP)
		infoSize = sizeof(SceAudioencInfoCelp);
	else if (codecType == AUDIOENC_TYPE_SILK)
		infoSize = sizeof(AudioencInfoSilk);
	else
		goto invalidHandle;
	kernelInfo = ctrl.pInfo;
#if FW_DEBUG
	SceDebugForDriver_35A35322(kernelInfo != NULL, &copyInInfoSource, caller);
#endif
	if (!userInfo || ((uintptr_t)userInfo & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(kernelInfo, userInfo, infoSize);
	if (ret < 0)
		goto unlock;
	invalidSize = infoSize < kernelInfo->size;
	if (infoSize > kernelInfo->size)
		invalidSize |= 1;
	if (invalidSize || (kernelInfo->size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	if (userOptInfo) {
		if (codecType != SCE_AUDIOENC_TYPE_CELP) {
			ret = codecType == AUDIOENC_TYPE_SILK ?
				SCE_AUDIOENC_ERROR_UNSUPPORTED : SCE_AUDIOENC_ERROR_INVALID_HANDLE;
			goto unlock;
		}
		kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
		SceDebugForDriver_35A35322(kernelOptInfo != NULL, &copyInInfoSource, caller);
#endif
		if (((uintptr_t)userOptInfo & 3) ||
		    SceSysmemForDriver_BC996A7A(kernelOptInfo, userOptInfo, sizeof(*kernelOptInfo)) < 0 ||
		    kernelOptInfo->size != sizeof(*kernelOptInfo))
			ctrl.pOptInfo = NULL;
	} else {
		ctrl.pOptInfo = NULL;
	}
	if (ctrl.wordLength != 16) {
		ret = SCE_AUDIOENC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	if (audioencIsExternalHandle(ctrl.handle) || !audioencIsResidentHandle(ctrl.handle))
		goto invalidHandle;
	codecType = audioencGetHandleCodecType(ctrl.handle);
	audioencIsExternalHandle(ctrl.handle);
	process = SceProcessmgrForDriver_EE694840(audioencResident.processStorage);
#if FW_DEBUG
	SceDebugForDriver_35A35322(process != NULL, &workManagerStorage, caller);
#else
	(void)process;
#endif
	if (codecType - SCE_AUDIOENC_TYPE_CELP > 1)
		goto invalidHandle;
	index = audioencGetHandleIndex(ctrl.handle);
	if (!audioencResident.manager.totalEncoders) {
		ret = SCE_AUDIOENC_ERROR_NOT_INITIALIZED;
		goto unlock;
	}
	if (index >= audioencResident.manager.totalEncoders)
		goto invalidHandle;
	if (!audioencResident.manager.pEncoders[index].active) {
		ret = SCE_AUDIOENC_ERROR_NOT_HANDLE_IN_USE;
		goto unlock;
	}
	ret = lockFastMutex((SceKernelFastMutex *)audioencResident.manager.pMutex);
	if (ret >= 0) {
		ret = _audioencInternalDeleteEncoder(&audioencResident.manager, &ctrl, 0);
		sceKernelUnlockFastMutex((SceKernelFastMutex *)audioencResident.manager.pMutex);
		if (ret >= 0)
			ret = _copyOutSceAudioencCtrl(userCtrl, userInfo, userOptInfo, &ctrl);
	}
	goto unlock;
invalidHandle:
	ret = SCE_AUDIOENC_ERROR_INVALID_HANDLE;
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int sceAudioencDeleteEncoderExternalForUser(SceAudioencCtrl *userCtrl,
					  SceUIntVAddr context)
{
	SceAudioencCtrl ctrl;
	AudioencInfo info;
	SceAudioencOptInfo optInfo;
	SceAudioencInfo *kernelInfo, *userInfo;
	SceAudioencOptInfo *kernelOptInfo, *userOptInfo;
	AudioencProcess *process;
	AudioencWorkManager *manager;
	SceUInt32 codecType, infoSize, index;
	SceUInt32 syscall = sceSyscallEnter();
	int ret, invalidSize, external;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	if (!context) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto leave;
	}
	ctrl.pInfo = &info.publicInfo;
	ctrl.pOptInfo = &optInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInCtrlInfo, caller);
	SceDebugForDriver_35A35322(1, &copyInCtrlInfoDestination, caller);
#endif
	kernelInfo = ctrl.pInfo;
	kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInInfoSource, caller);
#endif
	if (!userCtrl || ((uintptr_t)userCtrl & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(&ctrl, userCtrl, sizeof(ctrl));
	if (ret < 0)
		goto unlock;
	if (ctrl.size - 40 > 4 || (ctrl.size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	userInfo = ctrl.pInfo;
	userOptInfo = ctrl.pOptInfo;
	ctrl.pInfo = kernelInfo;
	ctrl.pOptInfo = kernelOptInfo;
	codecType = audioencGetHandleCodecType(ctrl.handle);
	if (codecType == SCE_AUDIOENC_TYPE_CELP)
		infoSize = sizeof(SceAudioencInfoCelp);
	else if (codecType == AUDIOENC_TYPE_SILK)
		infoSize = sizeof(AudioencInfoSilk);
	else
		goto invalidHandle;
	kernelInfo = ctrl.pInfo;
#if FW_DEBUG
	SceDebugForDriver_35A35322(kernelInfo != NULL, &copyInInfoSource, caller);
#endif
	if (!userInfo || ((uintptr_t)userInfo & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(kernelInfo, userInfo, infoSize);
	if (ret < 0)
		goto unlock;
	invalidSize = infoSize < kernelInfo->size;
	if (infoSize > kernelInfo->size)
		invalidSize |= 1;
	if (invalidSize || (kernelInfo->size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	if (userOptInfo) {
		if (codecType != SCE_AUDIOENC_TYPE_CELP) {
			ret = codecType == AUDIOENC_TYPE_SILK ?
				SCE_AUDIOENC_ERROR_UNSUPPORTED : SCE_AUDIOENC_ERROR_INVALID_HANDLE;
			goto unlock;
		}
		kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
		SceDebugForDriver_35A35322(kernelOptInfo != NULL, &copyInInfoSource, caller);
#endif
		if (((uintptr_t)userOptInfo & 3) ||
		    SceSysmemForDriver_BC996A7A(kernelOptInfo, userOptInfo, sizeof(*kernelOptInfo)) < 0 ||
		    kernelOptInfo->size != sizeof(*kernelOptInfo))
			ctrl.pOptInfo = NULL;
	} else {
		ctrl.pOptInfo = NULL;
	}
	if (ctrl.wordLength != 16) {
		ret = SCE_AUDIOENC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	if (!audioencIsExternalHandle(ctrl.handle) || audioencIsResidentHandle(ctrl.handle))
		goto invalidHandle;
	codecType = audioencGetHandleCodecType(ctrl.handle);
	external = audioencIsExternalHandle(ctrl.handle);
	process = SceProcessmgrForDriver_EE694840(audioencResident.processStorage);
#if FW_DEBUG
	SceDebugForDriver_35A35322(process != NULL, &workManagerStorage, caller);
#endif
	if (external) {
		if (codecType == process->external.managers[0].codecType)
			index = 0;
		else if (codecType == process->external.managers[1].codecType)
			index = 1;
		else
			goto invalidHandle;
		manager = &process->external.managers[index];
	} else {
		if (codecType == process->internal.managers[0].codecType)
			index = 0;
		else if (codecType == process->internal.managers[1].codecType)
			index = 1;
		else
			goto invalidHandle;
		manager = &process->internal.managers[index];
	}
	index = audioencGetHandleIndex(ctrl.handle);
	if (!manager->totalEncoders) {
		ret = SCE_AUDIOENC_ERROR_NOT_INITIALIZED;
		goto unlock;
	}
	if (index >= manager->totalEncoders)
		goto invalidHandle;
	if (!manager->pEncoders[index].active) {
		ret = SCE_AUDIOENC_ERROR_NOT_HANDLE_IN_USE;
		goto unlock;
	}
	ret = lockFastMutex((SceKernelFastMutex *)manager->pMutex);
	if (ret >= 0) {
		ret = _audioencInternalDeleteEncoder(manager, &ctrl, (SceUIntVAddr *)context);
		sceKernelUnlockFastMutex((SceKernelFastMutex *)manager->pMutex);
		if (ret >= 0)
			ret = _copyOutSceAudioencCtrl(userCtrl, userInfo, userOptInfo, &ctrl);
	}
	goto unlock;
invalidHandle:
	ret = SCE_AUDIOENC_ERROR_INVALID_HANDLE;
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int sceAudioencEncodeForUser(SceAudioencCtrl *userCtrl)
{
	SceAudioencCtrl ctrl;
	AudioencInfo info;
	SceAudioencOptInfo optInfo;
	SceAudioencInfo *kernelInfo, *userInfo;
	SceAudioencOptInfo *kernelOptInfo, *userOptInfo;
	AudioencProcess *process;
	AudioencWorkManager *manager;
	AudioencInfoSilk *silk;
	SceUInt32 codecType, infoSize, index;
	SceUInt32 syscall = sceSyscallEnter();
	int ret, invalidSize, external, resident;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	ctrl.pInfo = &info.publicInfo;
	ctrl.pOptInfo = &optInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInCtrlInfo, caller);
	SceDebugForDriver_35A35322(1, &copyInCtrlInfoDestination, caller);
#endif
	kernelInfo = ctrl.pInfo;
	kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInInfoSource, caller);
#endif
	if (!userCtrl || ((uintptr_t)userCtrl & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(&ctrl, userCtrl, sizeof(ctrl));
	if (ret < 0)
		goto unlock;
	if (ctrl.size - 40 > 4 || (ctrl.size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	userInfo = ctrl.pInfo;
	userOptInfo = ctrl.pOptInfo;
	ctrl.pInfo = kernelInfo;
	ctrl.pOptInfo = kernelOptInfo;
	codecType = audioencGetHandleCodecType(ctrl.handle);
	if (codecType == SCE_AUDIOENC_TYPE_CELP)
		infoSize = sizeof(SceAudioencInfoCelp);
	else if (codecType == AUDIOENC_TYPE_SILK)
		infoSize = sizeof(AudioencInfoSilk);
	else
		goto invalidHandle;
	kernelInfo = ctrl.pInfo;
#if FW_DEBUG
	SceDebugForDriver_35A35322(kernelInfo != NULL, &copyInInfoSource, caller);
#endif
	if (!userInfo || ((uintptr_t)userInfo & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(kernelInfo, userInfo, infoSize);
	if (ret < 0)
		goto unlock;
	invalidSize = infoSize < kernelInfo->size;
	if (infoSize > kernelInfo->size)
		invalidSize |= 1;
	if (invalidSize || (kernelInfo->size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	if (userOptInfo) {
		if (codecType != SCE_AUDIOENC_TYPE_CELP) {
			ret = codecType == AUDIOENC_TYPE_SILK ?
				SCE_AUDIOENC_ERROR_UNSUPPORTED : SCE_AUDIOENC_ERROR_INVALID_HANDLE;
			goto unlock;
		}
		kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
		SceDebugForDriver_35A35322(kernelOptInfo != NULL, &copyInInfoSource, caller);
#endif
		if (((uintptr_t)userOptInfo & 3) ||
		    SceSysmemForDriver_BC996A7A(kernelOptInfo, userOptInfo, sizeof(*kernelOptInfo)) < 0 ||
		    kernelOptInfo->size != sizeof(*kernelOptInfo))
			ctrl.pOptInfo = NULL;
	} else {
		ctrl.pOptInfo = NULL;
	}
	if (ctrl.wordLength != 16) {
		ret = SCE_AUDIOENC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	codecType = audioencGetHandleCodecType(ctrl.handle);
	external = audioencIsExternalHandle(ctrl.handle);
	resident = audioencIsResidentHandle(ctrl.handle);
	process = SceProcessmgrForDriver_EE694840(audioencResident.processStorage);
#if FW_DEBUG
	SceDebugForDriver_35A35322(process != NULL, &workManagerStorage, caller);
#endif
	if (resident) {
		if (codecType - SCE_AUDIOENC_TYPE_CELP > 1)
			goto invalidHandle;
		manager = &audioencResident.manager;
	} else if (external) {
		if (codecType == process->external.managers[0].codecType)
			index = 0;
		else if (codecType == process->external.managers[1].codecType)
			index = 1;
		else
			goto invalidHandle;
		manager = &process->external.managers[index];
	} else {
		if (codecType == process->internal.managers[0].codecType)
			index = 0;
		else if (codecType == process->internal.managers[1].codecType)
			index = 1;
		else
			goto invalidHandle;
		manager = &process->internal.managers[index];
	}
	index = audioencGetHandleIndex(ctrl.handle);
	if (!manager->totalEncoders) {
		ret = SCE_AUDIOENC_ERROR_NOT_INITIALIZED;
		goto unlock;
	}
	if (index >= manager->totalEncoders)
		goto invalidHandle;
	if (!manager->pEncoders[index].active) {
		ret = SCE_AUDIOENC_ERROR_NOT_HANDLE_IN_USE;
		goto unlock;
	}
	codecType = audioencGetHandleCodecType(ctrl.handle);
	index = audioencGetHandleIndex(ctrl.handle);
	if (codecType != SCE_AUDIOENC_TYPE_CELP) {
		if (codecType != AUDIOENC_TYPE_SILK) {
			ret = SCE_AUDIOENC_ERROR_INVALID_TYPE;
			goto releaseRanges;
		}
		silk = &((AudioencInfo *)ctrl.pInfo)->silk;
		if ((silk->samplingRate != 12000 && silk->samplingRate != 16000 &&
		     silk->samplingRate != 8000) ||
		    (silk->maxInternalSamplingRate != 12000 &&
		     silk->maxInternalSamplingRate != 16000 &&
		     silk->maxInternalSamplingRate != 8000)) {
			ret = AUDIOENC_SILK_ERROR_INVALID_SAMPLING_RATE;
			goto releaseRanges;
		}
		if (silk->bitRate - 6000 > 34000) {
			ret = AUDIOENC_SILK_ERROR_INVALID_BIT_RATE;
			goto releaseRanges;
		}
		if (silk->packetLossPercentage > 100 || silk->complexity > 2 ||
		    silk->useInBandFec > 1) {
			ret = AUDIOENC_SILK_ERROR_INVALID_CONFIG;
			goto releaseRanges;
		}
	}
	if (!ctrl.pInputPcm || ((uintptr_t)ctrl.pInputPcm & 1)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto releaseRanges;
	}
	ctrl.inputPcmSize = manager->pWork[index].maxPcmSize;
	ret = sceKernelLockRangeWithMode(0x10, ctrl.pInputPcm, ctrl.inputPcmSize);
	if (ret < 0)
		goto releaseRanges;
	manager->pWork[index].inputPcmSize = ctrl.inputPcmSize;
	ret = SceSysmemForDriver_9C78064C(0x10, ctrl.pInputPcm, ctrl.inputPcmSize);
	if (ret < 0)
		goto releaseRanges;
	if (!ctrl.pOutputEs) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto releaseRanges;
	}
	ctrl.outputEsSize = manager->pWork[index].maxEsSize;
	ret = sceKernelLockRangeWithMode(0x30, ctrl.pOutputEs, ctrl.outputEsSize);
	if (ret < 0)
		goto releaseRanges;
	manager->pWork[index].outputEsSize = ctrl.outputEsSize;
	ret = SceSysmemForDriver_9C78064C(0x30, ctrl.pOutputEs, ctrl.outputEsSize);
	if (ret >= 0)
		ret = _audioencInternalEncode(manager, &ctrl, 1);
releaseRanges:
	index = audioencGetHandleIndex(ctrl.handle);
	if (manager->pWork[index].inputPcmSize) {
		SceSysmemForDriver_22CBE925(0x10, ctrl.pInputPcm,
					     manager->pWork[index].inputPcmSize);
		manager->pWork[index].inputPcmSize = 0;
	}
	if (manager->pWork[index].outputEsSize) {
		SceSysmemForDriver_22CBE925(0x30, ctrl.pOutputEs,
					     manager->pWork[index].outputEsSize);
		manager->pWork[index].outputEsSize = 0;
	}
	if (ret >= 0)
		ret = _copyOutSceAudioencCtrl(userCtrl, userInfo, userOptInfo, &ctrl);
	goto unlock;
invalidHandle:
	ret = SCE_AUDIOENC_ERROR_INVALID_HANDLE;
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int sceAudioencEncodeNFramesForUser(SceAudioencCtrl *userCtrl, SceUInt32 nFrames)
{
	SceAudioencCtrl ctrl;
	AudioencInfo info;
	SceAudioencOptInfo optInfo;
	SceAudioencInfo *kernelInfo, *userInfo;
	SceAudioencOptInfo *kernelOptInfo, *userOptInfo;
	AudioencProcess *process;
	AudioencWorkManager *manager;
	AudioencInfoSilk *silk;
	SceUInt32 codecType, infoSize, index, maxFrames;
	SceUInt32 syscall = sceSyscallEnter();
	int ret, invalidSize, external, resident;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	if (!nFrames) {
		ret = AUDIOENC_ERROR_INVALID_NFRAMES;
		goto leave;
	}
	ctrl.pInfo = &info.publicInfo;
	ctrl.pOptInfo = &optInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInCtrlInfo, caller);
	SceDebugForDriver_35A35322(1, &copyInCtrlInfoDestination, caller);
#endif
	kernelInfo = ctrl.pInfo;
	kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInInfoSource, caller);
#endif
	if (!userCtrl || ((uintptr_t)userCtrl & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(&ctrl, userCtrl, sizeof(ctrl));
	if (ret < 0)
		goto unlock;
	if (ctrl.size - 40 > 4 || (ctrl.size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	userInfo = ctrl.pInfo;
	userOptInfo = ctrl.pOptInfo;
	ctrl.pInfo = kernelInfo;
	ctrl.pOptInfo = kernelOptInfo;
	codecType = audioencGetHandleCodecType(ctrl.handle);
	if (codecType == SCE_AUDIOENC_TYPE_CELP)
		infoSize = sizeof(SceAudioencInfoCelp);
	else if (codecType == AUDIOENC_TYPE_SILK)
		infoSize = sizeof(AudioencInfoSilk);
	else
		goto invalidHandle;
	kernelInfo = ctrl.pInfo;
#if FW_DEBUG
	SceDebugForDriver_35A35322(kernelInfo != NULL, &copyInInfoSource, caller);
#endif
	if (!userInfo || ((uintptr_t)userInfo & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(kernelInfo, userInfo, infoSize);
	if (ret < 0)
		goto unlock;
	invalidSize = infoSize < kernelInfo->size;
	if (infoSize > kernelInfo->size)
		invalidSize |= 1;
	if (invalidSize || (kernelInfo->size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	if (userOptInfo) {
		if (codecType != SCE_AUDIOENC_TYPE_CELP) {
			ret = codecType == AUDIOENC_TYPE_SILK ?
				SCE_AUDIOENC_ERROR_UNSUPPORTED : SCE_AUDIOENC_ERROR_INVALID_HANDLE;
			goto unlock;
		}
		kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
		SceDebugForDriver_35A35322(kernelOptInfo != NULL, &copyInInfoSource, caller);
#endif
		if (((uintptr_t)userOptInfo & 3) ||
		    SceSysmemForDriver_BC996A7A(kernelOptInfo, userOptInfo, sizeof(*kernelOptInfo)) < 0 ||
		    kernelOptInfo->size != sizeof(*kernelOptInfo))
			ctrl.pOptInfo = NULL;
	} else {
		ctrl.pOptInfo = NULL;
	}
	if (ctrl.wordLength != 16) {
		ret = SCE_AUDIOENC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	codecType = audioencGetHandleCodecType(ctrl.handle);
	external = audioencIsExternalHandle(ctrl.handle);
	resident = audioencIsResidentHandle(ctrl.handle);
	process = SceProcessmgrForDriver_EE694840(audioencResident.processStorage);
#if FW_DEBUG
	SceDebugForDriver_35A35322(process != NULL, &workManagerStorage, caller);
#endif
	if (resident) {
		if (codecType - SCE_AUDIOENC_TYPE_CELP > 1)
			goto invalidHandle;
		manager = &audioencResident.manager;
	} else if (external) {
		if (codecType == process->external.managers[0].codecType)
			index = 0;
		else if (codecType == process->external.managers[1].codecType)
			index = 1;
		else
			goto invalidHandle;
		manager = &process->external.managers[index];
	} else {
		if (codecType == process->internal.managers[0].codecType)
			index = 0;
		else if (codecType == process->internal.managers[1].codecType)
			index = 1;
		else
			goto invalidHandle;
		manager = &process->internal.managers[index];
	}
	index = audioencGetHandleIndex(ctrl.handle);
	if (!manager->totalEncoders) {
		ret = SCE_AUDIOENC_ERROR_NOT_INITIALIZED;
		goto unlock;
	}
	if (index >= manager->totalEncoders)
		goto invalidHandle;
	if (!manager->pEncoders[index].active) {
		ret = SCE_AUDIOENC_ERROR_NOT_HANDLE_IN_USE;
		goto unlock;
	}
	codecType = audioencGetHandleCodecType(ctrl.handle);
	index = audioencGetHandleIndex(ctrl.handle);
	if (codecType == SCE_AUDIOENC_TYPE_CELP) {
		maxFrames = 1;
	} else {
		if (codecType != AUDIOENC_TYPE_SILK) {
			ret = SCE_AUDIOENC_ERROR_INVALID_TYPE;
			goto releaseRanges;
		}
		silk = &((AudioencInfo *)ctrl.pInfo)->silk;
		if ((silk->samplingRate != 12000 && silk->samplingRate != 16000 &&
		     silk->samplingRate != 8000) ||
		    (silk->maxInternalSamplingRate != 12000 &&
		     silk->maxInternalSamplingRate != 16000 &&
		     silk->maxInternalSamplingRate != 8000)) {
			ret = AUDIOENC_SILK_ERROR_INVALID_SAMPLING_RATE;
			goto releaseRanges;
		}
		if (silk->bitRate - 6000 > 34000) {
			ret = AUDIOENC_SILK_ERROR_INVALID_BIT_RATE;
			goto releaseRanges;
		}
		if (silk->packetLossPercentage > 100 || silk->complexity > 2 ||
		    silk->useInBandFec > 1) {
			ret = AUDIOENC_SILK_ERROR_INVALID_CONFIG;
			goto releaseRanges;
		}
		maxFrames = 5;
	}
	if (nFrames > maxFrames) {
		ret = AUDIOENC_ERROR_INVALID_NFRAMES;
		goto releaseRanges;
	}
	if (!ctrl.pInputPcm || ((uintptr_t)ctrl.pInputPcm & 1)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto releaseRanges;
	}
	ctrl.inputPcmSize = manager->pWork[index].maxPcmSize * nFrames;
	ret = sceKernelLockRangeWithMode(0x10, ctrl.pInputPcm, ctrl.inputPcmSize);
	if (ret < 0)
		goto releaseRanges;
	manager->pWork[index].inputPcmSize = ctrl.inputPcmSize;
	ret = SceSysmemForDriver_9C78064C(0x10, ctrl.pInputPcm, ctrl.inputPcmSize);
	if (ret < 0)
		goto releaseRanges;
	if (!ctrl.pOutputEs) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto releaseRanges;
	}
	ctrl.outputEsSize = manager->pWork[index].maxEsSize * nFrames;
	ret = sceKernelLockRangeWithMode(0x30, ctrl.pOutputEs, ctrl.outputEsSize);
	if (ret < 0)
		goto releaseRanges;
	manager->pWork[index].outputEsSize = ctrl.outputEsSize;
	ret = SceSysmemForDriver_9C78064C(0x30, ctrl.pOutputEs, ctrl.outputEsSize);
	if (ret >= 0)
		ret = _audioencInternalEncode(manager, &ctrl, nFrames);
releaseRanges:
	index = audioencGetHandleIndex(ctrl.handle);
	if (manager->pWork[index].inputPcmSize) {
		SceSysmemForDriver_22CBE925(0x10, ctrl.pInputPcm,
					     manager->pWork[index].inputPcmSize);
		manager->pWork[index].inputPcmSize = 0;
	}
	if (manager->pWork[index].outputEsSize) {
		SceSysmemForDriver_22CBE925(0x30, ctrl.pOutputEs,
					     manager->pWork[index].outputEsSize);
		manager->pWork[index].outputEsSize = 0;
	}
	if (ret >= 0)
		ret = _copyOutSceAudioencCtrl(userCtrl, userInfo, userOptInfo, &ctrl);
	goto unlock;
invalidHandle:
	ret = SCE_AUDIOENC_ERROR_INVALID_HANDLE;
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int sceAudioencClearContextForUser(SceAudioencCtrl *userCtrl)
{
	SceAudioencCtrl ctrl;
	AudioencInfo info;
	SceAudioencOptInfo optInfo;
	SceAudioencInfo *kernelInfo, *userInfo;
	SceAudioencOptInfo *kernelOptInfo, *userOptInfo;
	AudioencProcess *process;
	AudioencWorkManager *manager;
	SceUInt32 codecType, infoSize, index, bitRate;
	SceUInt32 syscall = sceSyscallEnter();
	int ret, invalidSize, external, resident;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	ctrl.pInfo = &info.publicInfo;
	ctrl.pOptInfo = &optInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInCtrlInfo, caller);
	SceDebugForDriver_35A35322(1, &copyInCtrlInfoDestination, caller);
#endif
	kernelInfo = ctrl.pInfo;
	kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInInfoSource, caller);
#endif
	if (!userCtrl || ((uintptr_t)userCtrl & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(&ctrl, userCtrl, sizeof(ctrl));
	if (ret < 0)
		goto unlock;
	if (ctrl.size - 40 > 4 || (ctrl.size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	userInfo = ctrl.pInfo;
	userOptInfo = ctrl.pOptInfo;
	ctrl.pInfo = kernelInfo;
	ctrl.pOptInfo = kernelOptInfo;
	codecType = audioencGetHandleCodecType(ctrl.handle);
	if (codecType == SCE_AUDIOENC_TYPE_CELP)
		infoSize = sizeof(SceAudioencInfoCelp);
	else if (codecType == AUDIOENC_TYPE_SILK)
		infoSize = sizeof(AudioencInfoSilk);
	else
		goto invalidHandle;
	kernelInfo = ctrl.pInfo;
#if FW_DEBUG
	SceDebugForDriver_35A35322(kernelInfo != NULL, &copyInInfoSource, caller);
#endif
	if (!userInfo || ((uintptr_t)userInfo & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(kernelInfo, userInfo, infoSize);
	if (ret < 0)
		goto unlock;
	invalidSize = infoSize < kernelInfo->size;
	if (infoSize > kernelInfo->size)
		invalidSize |= 1;
	if (invalidSize || (kernelInfo->size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	if (userOptInfo) {
		if (codecType != SCE_AUDIOENC_TYPE_CELP) {
			ret = codecType == AUDIOENC_TYPE_SILK ?
				SCE_AUDIOENC_ERROR_UNSUPPORTED : SCE_AUDIOENC_ERROR_INVALID_HANDLE;
			goto unlock;
		}
		kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
		SceDebugForDriver_35A35322(kernelOptInfo != NULL, &copyInInfoSource, caller);
#endif
		if (((uintptr_t)userOptInfo & 3) ||
		    SceSysmemForDriver_BC996A7A(kernelOptInfo, userOptInfo, sizeof(*kernelOptInfo)) < 0 ||
		    kernelOptInfo->size != sizeof(*kernelOptInfo))
			ctrl.pOptInfo = NULL;
	} else {
		ctrl.pOptInfo = NULL;
	}
	if (ctrl.wordLength != 16) {
		ret = SCE_AUDIOENC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	codecType = audioencGetHandleCodecType(ctrl.handle);
	if (codecType == SCE_AUDIOENC_TYPE_CELP) {
		if (ctrl.pInfo->celp.excitationMode != 0 || ctrl.pInfo->celp.samplingRate != 8000)
			goto invalidConfig;
		bitRate = ctrl.pInfo->celp.bitRate;
		if (bitRate != 3850 && bitRate != 4650 && bitRate != 5700 &&
		    bitRate != 6600 && bitRate != 7300 && bitRate != 8700 &&
		    bitRate != 9900 && bitRate != 10700 && bitRate != 11800 && bitRate != 12200)
			goto invalidConfig;
	} else if (codecType == AUDIOENC_TYPE_SILK) {
		if (((AudioencInfo *)ctrl.pInfo)->silk.samplingRate != 12000 &&
		    ((AudioencInfo *)ctrl.pInfo)->silk.samplingRate != 16000 &&
		    ((AudioencInfo *)ctrl.pInfo)->silk.samplingRate != 8000) {
			ret = AUDIOENC_SILK_ERROR_INVALID_SAMPLING_RATE;
			goto unlock;
		}
	} else {
		ret = SCE_AUDIOENC_ERROR_INVALID_TYPE;
		goto unlock;
	}
	codecType = audioencGetHandleCodecType(ctrl.handle);
	external = audioencIsExternalHandle(ctrl.handle);
	resident = audioencIsResidentHandle(ctrl.handle);
	process = SceProcessmgrForDriver_EE694840(audioencResident.processStorage);
#if FW_DEBUG
	SceDebugForDriver_35A35322(process != NULL, &workManagerStorage, caller);
#endif
	if (resident) {
		if (codecType - SCE_AUDIOENC_TYPE_CELP > 1)
			goto invalidHandle;
		manager = &audioencResident.manager;
	} else if (external) {
		if (codecType == process->external.managers[0].codecType)
			index = 0;
		else if (codecType == process->external.managers[1].codecType)
			index = 1;
		else
			goto invalidHandle;
		manager = &process->external.managers[index];
	} else {
		if (codecType == process->internal.managers[0].codecType)
			index = 0;
		else if (codecType == process->internal.managers[1].codecType)
			index = 1;
		else
			goto invalidHandle;
		manager = &process->internal.managers[index];
	}
	index = audioencGetHandleIndex(ctrl.handle);
	if (!manager->totalEncoders) {
		ret = SCE_AUDIOENC_ERROR_NOT_INITIALIZED;
		goto unlock;
	}
	if (index >= manager->totalEncoders)
		goto invalidHandle;
	if (!manager->pEncoders[index].active) {
		ret = SCE_AUDIOENC_ERROR_NOT_HANDLE_IN_USE;
		goto unlock;
	}
	ret = _audioencInternalClearContext(manager, &ctrl);
	if (ret >= 0)
		ret = _copyOutSceAudioencCtrl(userCtrl, userInfo, userOptInfo, &ctrl);
	goto unlock;
invalidConfig:
	ret = SCE_AUDIOENC_CELP_ERROR_INVALID_CONFIG;
	goto unlock;
invalidHandle:
	ret = SCE_AUDIOENC_ERROR_INVALID_HANDLE;
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int sceAudioencGetOptInfoForUser(SceAudioencCtrl *userCtrl)
{
	SceAudioencCtrl ctrl;
	AudioencInfo info;
	SceAudioencOptInfo optInfo;
	SceAudioencInfo *kernelInfo, *userInfo;
	SceAudioencOptInfo *kernelOptInfo, *userOptInfo;
	AudioencProcess *process;
	AudioencWorkManager *manager;
	SceUInt32 codecType, infoSize, index;
	SceUInt32 syscall = sceSyscallEnter();
	int ret, invalidSize, external, resident;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	ctrl.pInfo = &info.publicInfo;
	ctrl.pOptInfo = &optInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInCtrlInfo, caller);
	SceDebugForDriver_35A35322(1, &copyInCtrlInfoDestination, caller);
#endif
	kernelInfo = ctrl.pInfo;
	kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInInfoSource, caller);
#endif
	if (!userCtrl || ((uintptr_t)userCtrl & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(&ctrl, userCtrl, sizeof(ctrl));
	if (ret < 0)
		goto unlock;
	if (ctrl.size - 40 > 4 || (ctrl.size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	userInfo = ctrl.pInfo;
	userOptInfo = ctrl.pOptInfo;
	ctrl.pInfo = kernelInfo;
	ctrl.pOptInfo = kernelOptInfo;
	codecType = audioencGetHandleCodecType(ctrl.handle);
	if (codecType == SCE_AUDIOENC_TYPE_CELP)
		infoSize = sizeof(SceAudioencInfoCelp);
	else if (codecType == AUDIOENC_TYPE_SILK)
		infoSize = sizeof(AudioencInfoSilk);
	else
		goto invalidHandle;
	kernelInfo = ctrl.pInfo;
#if FW_DEBUG
	SceDebugForDriver_35A35322(kernelInfo != NULL, &copyInInfoSource, caller);
#endif
	if (!userInfo || ((uintptr_t)userInfo & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(kernelInfo, userInfo, infoSize);
	if (ret < 0)
		goto unlock;
	invalidSize = infoSize < kernelInfo->size;
	if (infoSize > kernelInfo->size)
		invalidSize |= 1;
	if (invalidSize || (kernelInfo->size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	if (userOptInfo) {
		if (codecType != SCE_AUDIOENC_TYPE_CELP) {
			ret = codecType == AUDIOENC_TYPE_SILK ?
				SCE_AUDIOENC_ERROR_UNSUPPORTED : SCE_AUDIOENC_ERROR_INVALID_HANDLE;
			goto unlock;
		}
		kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
		SceDebugForDriver_35A35322(kernelOptInfo != NULL, &copyInInfoSource, caller);
#endif
		if (((uintptr_t)userOptInfo & 3) ||
		    SceSysmemForDriver_BC996A7A(kernelOptInfo, userOptInfo, sizeof(*kernelOptInfo)) < 0 ||
		    kernelOptInfo->size != sizeof(*kernelOptInfo))
			ctrl.pOptInfo = NULL;
	} else {
		ctrl.pOptInfo = NULL;
	}
	if (ctrl.wordLength != 16) {
		ret = SCE_AUDIOENC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	if (!ctrl.pOptInfo) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	codecType = audioencGetHandleCodecType(ctrl.handle);
	external = audioencIsExternalHandle(ctrl.handle);
	resident = audioencIsResidentHandle(ctrl.handle);
	process = SceProcessmgrForDriver_EE694840(audioencResident.processStorage);
#if FW_DEBUG
	SceDebugForDriver_35A35322(process != NULL, &workManagerStorage, caller);
#endif
	if (resident) {
		if (codecType - SCE_AUDIOENC_TYPE_CELP > 1)
			goto invalidHandle;
		manager = &audioencResident.manager;
	} else if (external) {
		if (codecType == process->external.managers[0].codecType)
			index = 0;
		else if (codecType == process->external.managers[1].codecType)
			index = 1;
		else
			goto invalidHandle;
		manager = &process->external.managers[index];
	} else {
		if (codecType == process->internal.managers[0].codecType)
			index = 0;
		else if (codecType == process->internal.managers[1].codecType)
			index = 1;
		else
			goto invalidHandle;
		manager = &process->internal.managers[index];
	}
	index = audioencGetHandleIndex(ctrl.handle);
	if (!manager->totalEncoders) {
		ret = SCE_AUDIOENC_ERROR_NOT_INITIALIZED;
		goto unlock;
	}
	if (index >= manager->totalEncoders)
		goto invalidHandle;
	if (!manager->pEncoders[index].active) {
		ret = SCE_AUDIOENC_ERROR_NOT_HANDLE_IN_USE;
		goto unlock;
	}
	ret = _audioencInternalGetOptInfo(manager, &ctrl);
	if (ret >= 0)
		ret = _copyOutSceAudioencCtrl(userCtrl, userInfo, userOptInfo, &ctrl);
	goto unlock;
invalidHandle:
	ret = SCE_AUDIOENC_ERROR_INVALID_HANDLE;
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int sceAudioencGetInternalErrorForUser(SceAudioencCtrl *userCtrl,
				      int *userInternalError)
{
	SceAudioencCtrl ctrl;
	AudioencInfo info;
	SceAudioencOptInfo optInfo;
	SceAudioencInfo *kernelInfo, *userInfo;
	SceAudioencOptInfo *kernelOptInfo, *userOptInfo;
	AudioencProcess *process;
	AudioencWorkManager *manager;
	SceUInt32 codecType, infoSize, index;
	SceUInt32 syscall = sceSyscallEnter();
	int ret, invalidSize, external, resident;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	ctrl.pInfo = &info.publicInfo;
	ctrl.pOptInfo = &optInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInCtrlInfo, caller);
	SceDebugForDriver_35A35322(1, &copyInCtrlInfoDestination, caller);
#endif
	kernelInfo = ctrl.pInfo;
	kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, &copyInInfoSource, caller);
#endif
	if (!userCtrl || ((uintptr_t)userCtrl & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(&ctrl, userCtrl, sizeof(ctrl));
	if (ret < 0)
		goto unlock;
	if (ctrl.size - 40 > 4 || (ctrl.size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	userInfo = ctrl.pInfo;
	userOptInfo = ctrl.pOptInfo;
	ctrl.pInfo = kernelInfo;
	ctrl.pOptInfo = kernelOptInfo;
	codecType = audioencGetHandleCodecType(ctrl.handle);
	if (codecType == SCE_AUDIOENC_TYPE_CELP)
		infoSize = sizeof(SceAudioencInfoCelp);
	else if (codecType == AUDIOENC_TYPE_SILK)
		infoSize = sizeof(AudioencInfoSilk);
	else
		goto invalidHandle;
	kernelInfo = ctrl.pInfo;
#if FW_DEBUG
	SceDebugForDriver_35A35322(kernelInfo != NULL, &copyInInfoSource, caller);
#endif
	if (!userInfo || ((uintptr_t)userInfo & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = SceSysmemForDriver_BC996A7A(kernelInfo, userInfo, infoSize);
	if (ret < 0)
		goto unlock;
	invalidSize = infoSize < kernelInfo->size;
	if (infoSize > kernelInfo->size)
		invalidSize |= 1;
	if (invalidSize || (kernelInfo->size & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	if (userOptInfo) {
		if (codecType != SCE_AUDIOENC_TYPE_CELP) {
			ret = codecType == AUDIOENC_TYPE_SILK ?
				SCE_AUDIOENC_ERROR_UNSUPPORTED : SCE_AUDIOENC_ERROR_INVALID_HANDLE;
			goto unlock;
		}
		kernelOptInfo = ctrl.pOptInfo;
#if FW_DEBUG
		SceDebugForDriver_35A35322(kernelOptInfo != NULL, &copyInInfoSource, caller);
#endif
		if (((uintptr_t)userOptInfo & 3) ||
		    SceSysmemForDriver_BC996A7A(kernelOptInfo, userOptInfo, sizeof(*kernelOptInfo)) < 0 ||
		    kernelOptInfo->size != sizeof(*kernelOptInfo))
			ctrl.pOptInfo = NULL;
	} else {
		ctrl.pOptInfo = NULL;
	}
	if (ctrl.wordLength != 16) {
		ret = SCE_AUDIOENC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	if (!userInternalError || ((uintptr_t)userInternalError & 3)) {
		ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
		goto unlock;
	}
	codecType = audioencGetHandleCodecType(ctrl.handle);
	external = audioencIsExternalHandle(ctrl.handle);
	resident = audioencIsResidentHandle(ctrl.handle);
	process = SceProcessmgrForDriver_EE694840(audioencResident.processStorage);
#if FW_DEBUG
	SceDebugForDriver_35A35322(process != NULL, &workManagerStorage, caller);
#endif
	if (resident) {
		if (codecType - SCE_AUDIOENC_TYPE_CELP > 1)
			goto invalidHandle;
		manager = &audioencResident.manager;
	} else if (external) {
		if (codecType == process->external.managers[0].codecType)
			index = 0;
		else if (codecType == process->external.managers[1].codecType)
			index = 1;
		else
			goto invalidHandle;
		manager = &process->external.managers[index];
	} else {
		if (codecType == process->internal.managers[0].codecType)
			index = 0;
		else if (codecType == process->internal.managers[1].codecType)
			index = 1;
		else
			goto invalidHandle;
		manager = &process->internal.managers[index];
	}
	index = audioencGetHandleIndex(ctrl.handle);
	if (!manager->totalEncoders) {
		ret = SCE_AUDIOENC_ERROR_NOT_INITIALIZED;
		goto unlock;
	}
	if (index >= manager->totalEncoders)
		goto invalidHandle;
	if (!manager->pEncoders[index].active) {
		ret = SCE_AUDIOENC_ERROR_NOT_HANDLE_IN_USE;
		goto unlock;
	}
	ret = _audioencInternalGetInternalError(manager, &ctrl, userInternalError);
	if (ret >= 0)
		ret = _copyOutSceAudioencCtrl(userCtrl, userInfo, userOptInfo, &ctrl);
	goto unlock;
invalidHandle:
	ret = SCE_AUDIOENC_ERROR_INVALID_HANDLE;
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

