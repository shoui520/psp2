/* SceAudiodec control and information handling (Sony audiodec/audiodec.c). */
#include "audiodec_internal.h"
#include <sce/debug.h>
#include <sce/sysmem.h>
#include <sce/processmgr.h>
#include <sce/cpu.h>
#include <sce/syscall.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include <psp2kern/kernel/threadmgr.h>
#include <string.h>
#include <stdint.h>

static AudiodecResident audiodecResident;

int ksceKernelIsGameBudget(void);
int _lockProcessSuspendCore(void);
int _unlockProcessSuspendCore(void);

static const char *const audiodecMutexNames[6] = {
	"SceAudiodecAtrac9", "SceAudiodecMp3", "SceAudiodecAac",
	"SceAudiodecAac6ch", "SceAudiodecCelp", "SceAudiodecSilk"
};

/* Context storage per channel and per decoder, in codec implementation order. */
static const SceInt32 audiodecChannelContextSize[6] = {
	1024, 0, 0, 0, 0, 0
};
static const SceInt32 audiodecDecoderContextSize[6] = {
	1024, 0, 0x18000, 0x28000, 0, 0
};

static const SceUInt32 audiodecMaxStreams[6] = { 6, 6, 6, 7, 1, 1 };

#if FW_DEBUG
SCE_DBGINFO_DEFINE(static, copyOutCtrlInfo, 948, "_copyOutSceAudiodecCtrl");
SCE_DBGINFO_DEFINE(static, copyOutInfoSource, 811, "_copyOutSceAudiodec");
SCE_DBGINFO_DEFINE(static, copyOutInfoDestination, 812, "_copyOutSceAudiodec");
SCE_DBGINFO_DEFINE(static, copyInCtrlInfo, 877, "_copyInSceAudiodecCtrl");
SCE_DBGINFO_DEFINE(static, copyInInfoSource, 778, "_copyInSceAudiodec");
SCE_DBGINFO_DEFINE(static, processCreateStorage, 295, "_sceAudiodecProcessCreateEntry");
SCE_DBGINFO_DEFINE(static, processExitStorage, 428, "_sceAudiodecProcessExitEntry");
SCE_DBGINFO_DEFINE(static, residentOwnerGet, 463, "_audiodecGetResidentDecoderOwner");
SCE_DBGINFO_DEFINE(static, residentOwnerSet, 469, "_audiodecSetResidentDecoderOwner");
SCE_DBGINFO_DEFINE(static, residentOwnerClear, 476, "_audiodecClearResidentDecoderOwner");
SCE_DBGINFO_DEFINE(static, workManagerStorage, 489, "_getWorkManager");
SCE_DBGINFO_DEFINE(static, workManagersDestination, 531, "_getWorkManagers");
SCE_DBGINFO_DEFINE(static, workManagersStorage, 535, "_getWorkManagers");
SCE_DBGINFO_DEFINE(static, initLibraryBudget, 595, "sceAudiodecInitLibraryForUser");
SCE_DBGINFO_DEFINE(static, termLibraryBudget, 673, "sceAudiodecTermLibraryForUser");
SCE_DBGINFO_DEFINE(static, createDecoderBudget, 1601, "sceAudiodecCreateDecoderForUser");
SCE_DBGINFO_DEFINE(static, deleteDecoderBudget, 1782, "sceAudiodecDeleteDecoderForUser");
#endif

int audiodecGetPcmSize(int *pcmSize, const AudiodecInfo *const *info,
		     SceUInt32 codecType)
{
	int size, channels;
	SceUInt32 version;

	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
		channels = atrac9GetChannelCount((*info)->publicInfo.at9.configData);
		size = 2 * channels *
			atrac9GetSamplesPerFrame((*info)->publicInfo.at9.configData);
		break;
	case SCE_AUDIODEC_TYPE_MP3:
		version = (*info)->publicInfo.mp3.version;
		if (version == SCE_AUDIODEC_MP3_MPEG_VERSION_1)
			size = 2304 * (*info)->publicInfo.mp3.ch;
		else {
			if (version & ~2U)
				return SCE_AUDIODEC_MP3_ERROR_INVALID_MPEG_VERSION;
			size = 1152 * (*info)->publicInfo.mp3.ch;
		}
		break;
	case SCE_AUDIODEC_TYPE_AAC:
		size = (*info)->publicInfo.aac.ch;
		if ((*info)->publicInfo.aac.isSbr)
			size <<= 12;
		else
			size <<= 11;
		break;
	case SCE_AUDIODEC_TYPE_CELP:
		size = 640;
		break;
	case AUDIODEC_TYPE_SILK:
		size = 40 * ((*info)->silk.samplingRate / 1000U);
		break;
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		size = 2;
		if (!(*info)->aacMultichannel.downmix)
			size = (*info)->aacMultichannel.aac.ch;
		if ((*info)->aacMultichannel.aac.isSbr)
			size <<= 12;
		else
			size <<= 11;
		break;
	default:
		return SCE_AUDIODEC_ERROR_INVALID_TYPE;
	}
	if (size > 0)
		*pcmSize = size;
	return size;
}

int audiodecLockDecodeBuffers(SceAudiodecCtrl *ctrl,
			     AudiodecDecoderWork *const *decoderWork,
			     SceUInt32 nFrames, SceUInt32 startSample,
			     SceUInt32 nSamples)
{
	SceUInt32 codecType = audiodecGetHandleCodecType(ctrl->handle);
	SceUInt32 index = audiodecGetHandleIndex(ctrl->handle);
	SceUInt32 samplesPerFrame, maxFrames, maxSkipFrames, samplingRate;
	int ret;

	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
		samplesPerFrame = (*decoderWork)[index].samplesPerFrame;
		maxFrames = 8;
		maxSkipFrames = 5;
		break;
	case SCE_AUDIODEC_TYPE_CELP:
		if (ctrl->pInfo->celp.lostCount > 1)
			return SCE_AUDIODEC_CELP_ERROR_INVALID_CONFIG;
		/* fall through */
	case SCE_AUDIODEC_TYPE_MP3:
	case SCE_AUDIODEC_TYPE_AAC:
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		samplesPerFrame = 0;
		maxFrames = 1;
		maxSkipFrames = 0;
		break;
	case AUDIODEC_TYPE_SILK:
		samplingRate = ((const AudiodecInfo *)ctrl->pInfo)->silk.samplingRate;
		if (samplingRate != 12000 && samplingRate != 8000 &&
		    samplingRate != 16000 && samplingRate != 24000)
			return (SceInt32)0x807F4001;
		samplesPerFrame = 0;
		maxFrames = 5;
		maxSkipFrames = 0;
		break;
	default:
		return SCE_AUDIODEC_ERROR_INVALID_TYPE;
	}
	if (nFrames && nFrames > maxFrames)
		return SCE_AUDIODEC_ERROR_INVALID_NFRAMES;
	if (startSample | nSamples) {
		if (!samplesPerFrame)
			return SCE_AUDIODEC_ERROR_UNSUPPORTED;
		if (startSample > samplesPerFrame * maxSkipFrames)
			return (SceInt32)0x807F0082;
		if (nSamples > samplesPerFrame * maxFrames)
			return (SceInt32)0x807F0083;
		nFrames = (nSamples - 1 + startSample + samplesPerFrame) /
			samplesPerFrame;
	}
	if (!ctrl->pEs)
		return SCE_AUDIODEC_ERROR_INVALID_PTR;
	ctrl->inputEsSize = audiodecCalculateEsSize(codecType,
		&(*decoderWork)[index], nFrames);
	ret = ksceKernelLockRangeWithMode(0x10, ctrl->pEs, ctrl->inputEsSize);
	if (ret < 0)
		return ret;
	(*decoderWork)[index].inputEsSize = ctrl->inputEsSize;
	ret = ksceKernelIsAccessibleRange(0x10, ctrl->pEs, ctrl->inputEsSize);
	if (ret < 0)
		return ret;
	if (!ctrl->pPcm || ((uintptr_t)ctrl->pPcm & 1))
		return SCE_AUDIODEC_ERROR_INVALID_PTR;
	ctrl->outputPcmSize = audiodecCalculatePcmSize(codecType,
		&(*decoderWork)[index], nFrames, startSample, nSamples);
	ret = ksceKernelLockRangeWithMode(0x30, ctrl->pPcm, ctrl->outputPcmSize);
	if (ret < 0)
		return ret;
	(*decoderWork)[index].outputPcmSize = ctrl->outputPcmSize;
	return ksceKernelIsAccessibleRange(0x30, ctrl->pPcm, ctrl->outputPcmSize);
}

int audiodecValidateDecoder(const SceAudiodecCtrl *ctrl, SceUInt32 codecType,
			   int checkContext, SceUInt32 contextSize)
{
	SceUInt32 samplingRate, channels, tableIndex;
	int ret = 0;

	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
		ret = atrac9ValidateConfig(ctrl);
		if (ret < 0)
			return ret;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
	case SCE_AUDIODEC_TYPE_AAC:
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		break;
	case SCE_AUDIODEC_TYPE_CELP:
		if (ctrl->pInfo->celp.excitationMode != 0 ||
		    ctrl->pInfo->celp.samplingRate != 8000)
			return SCE_AUDIODEC_CELP_ERROR_INVALID_CONFIG;
		switch (ctrl->pInfo->celp.bitRate) {
		case 3850:
		case 4650:
		case 5700:
		case 6600:
		case 7300:
		case 8700:
		case 9900:
		case 10700:
		case 11800:
		case 12200:
			break;
		default:
			return SCE_AUDIODEC_CELP_ERROR_INVALID_CONFIG;
		}
		break;
	case AUDIODEC_TYPE_SILK:
		samplingRate = ((const AudiodecInfo *)ctrl->pInfo)->silk.samplingRate;
		if (samplingRate != 12000) {
			if (samplingRate <= 12000) {
				if (samplingRate != 8000)
					return (SceInt32)0x807F4001;
			} else if (samplingRate != 16000 && samplingRate != 24000) {
				return (SceInt32)0x807F4001;
			}
		}
		break;
	default:
		return SCE_AUDIODEC_ERROR_INVALID_TYPE;
	}
	if (!checkContext)
		return ret;
	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
		channels = atrac9GetChannelCount(ctrl->pInfo->at9.configData);
		if (channels - 1 > 1)
			return SCE_AUDIODEC_AT9_ERROR_INVALID_CONFIG;
		tableIndex = 0;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
		channels = ctrl->pInfo->mp3.ch;
		if (channels - 1 > 1)
			return SCE_AUDIODEC_MP3_ERROR_INVALID_CH;
		tableIndex = 1;
		break;
	case SCE_AUDIODEC_TYPE_AAC:
		channels = ctrl->pInfo->aac.ch;
		if (channels - 1 > 1)
			return SCE_AUDIODEC_AAC_ERROR_INVALID_CH;
		tableIndex = 2;
		break;
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		channels = ctrl->pInfo->aac.ch;
		if (channels - 1 > 5)
			return SCE_AUDIODEC_AAC_ERROR_INVALID_CH;
		tableIndex = 3;
		break;
	case SCE_AUDIODEC_TYPE_CELP:
		channels = 1;
		tableIndex = 4;
		break;
	case AUDIODEC_TYPE_SILK:
		channels = 1;
		tableIndex = 5;
		break;
	default:
		return SCE_AUDIODEC_ERROR_INVALID_TYPE;
	}
	ret = audiodecDecoderContextSize[tableIndex] +
		audiodecChannelContextSize[tableIndex] * channels;
	if (ret >= 0) {
		if (!ret)
			return SCE_AUDIODEC_ERROR_UNSUPPORTED;
		if (contextSize != (SceUInt32)ret)
			ret = SCE_AUDIODEC_ERROR_INVALID_SIZE;
	}
	return ret;
}

int _copyOutSceAudiodecCtrl(SceAudiodecCtrl *userCtrl,
			  SceAudiodecInfo *userInfo, SceAudiodecCtrl *ctrl)
{
	SceAudiodecInfo *info;
	int ret;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
	int hasUserInfo = userInfo != NULL;

	ksceKernelAssert(hasUserInfo, &copyOutCtrlInfo, caller);
#endif
	info = ctrl->pInfo;
#if FW_DEBUG
	ksceKernelAssert(info != NULL, &copyOutInfoSource, caller);
	ksceKernelAssert(hasUserInfo, &copyOutInfoDestination, caller);
#endif
	ret = ksceKernelCopyToUser(userInfo, info, info->size);
	if (ret < 0)
		return ret;
	ctrl->pInfo = userInfo;
#if FW_DEBUG
	ksceKernelAssert(1, &copyOutInfoSource, caller);
	ksceKernelAssert(userCtrl != NULL, &copyOutInfoDestination, caller);
#endif
	return ksceKernelCopyToUser(userCtrl, ctrl, ctrl->size);
}

int _copyInSceAudiodecCtrl(SceAudiodecCtrl *ctrl,
			 const SceAudiodecCtrl *userCtrl,
			 const SceAudiodecInfo **userInfoResult,
			 SceUInt32 codecType)
{
	SceAudiodecInfo *info;
	const SceAudiodecInfo *userInfo;
	SceUInt32 maxSize, minSize;
	int ret, invalidSize;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);

	ksceKernelAssert(userInfoResult != NULL, &copyInCtrlInfo, caller);
#endif
	info = ctrl->pInfo;
#if FW_DEBUG
	ksceKernelAssert(1, &copyInInfoSource, caller);
#endif
	if (!userCtrl || ((uintptr_t)userCtrl & 3))
		return SCE_AUDIODEC_ERROR_INVALID_PTR;
	ret = ksceKernelCopyFromUser(ctrl, userCtrl, sizeof(*ctrl));
	if (ret < 0)
		return ret;
	if (ctrl->size != sizeof(*ctrl))
		return SCE_AUDIODEC_ERROR_INVALID_SIZE;
	userInfo = ctrl->pInfo;
	ctrl->pInfo = info;
	if (codecType)
		ret = SCE_AUDIODEC_ERROR_INVALID_TYPE;
	else {
		codecType = audiodecGetHandleCodecType(ctrl->handle);
		ret = SCE_AUDIODEC_ERROR_INVALID_HANDLE;
	}
	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
		maxSize = sizeof(SceAudiodecInfoAt9);
		minSize = maxSize;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
		maxSize = sizeof(SceAudiodecInfoMp3);
		minSize = maxSize;
		break;
	case SCE_AUDIODEC_TYPE_AAC:
	case SCE_AUDIODEC_TYPE_CELP:
		maxSize = sizeof(SceAudiodecInfoAac);
		minSize = 16;
		break;
	case AUDIODEC_TYPE_SILK:
		maxSize = sizeof(AudiodecInfoSilk);
		minSize = maxSize;
		break;
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		maxSize = sizeof(AudiodecInfoAacMultichannel);
		minSize = maxSize;
		break;
	default:
		return ret;
	}
	info = ctrl->pInfo;
#if FW_DEBUG
	ksceKernelAssert(info != NULL, &copyInInfoSource, caller);
#endif
	if (!userInfo || ((uintptr_t)userInfo & 3))
		return SCE_AUDIODEC_ERROR_INVALID_PTR;
	ret = ksceKernelCopyFromUser(info, userInfo, maxSize);
	if (ret < 0)
		return ret;
	invalidSize = maxSize < info->size;
	if (minSize > info->size)
		invalidSize |= 1;
	if (invalidSize || (info->size & 3))
		return SCE_AUDIODEC_ERROR_INVALID_SIZE;
	*userInfoResult = userInfo;
	return ret;
}

int initializeAudiodec(void)
{
	SceAudiodecInitParam param;
	int ret;

	memset(&audiodecResident, 0, sizeof(audiodecResident));
	audiodecResident.processStorage = -1;
	memset(audiodecResident.owners, -1, sizeof(audiodecResident.owners));
	audiodecResident.decoders[0].decodeLock = 0;
	audiodecResident.manager.codecType = SCE_AUDIODEC_TYPE_CELP;
	audiodecResident.decoders[0].lockGuard = 0;
	audiodecResident.manager.pDecoders = audiodecResident.decoders;
	audiodecResident.decoders[1].decodeLock = 0;
	audiodecResident.manager.pWork = audiodecResident.work;
	audiodecResident.decoders[1].lockGuard = 0;
	audiodecResident.manager.maxDecoders = 7;
	audiodecResident.decoders[2].decodeLock = 0;
	audiodecResident.decoders[2].lockGuard = 0;
	audiodecResident.decoders[3].decodeLock = 0;
	audiodecResident.decoders[3].lockGuard = 0;
	audiodecResident.decoders[4].decodeLock = 0;
	audiodecResident.decoders[4].lockGuard = 0;
	audiodecResident.decoders[5].decodeLock = 0;
	audiodecResident.decoders[5].lockGuard = 0;
	audiodecResident.decoders[6].decodeLock = 0;
	audiodecResident.decoders[6].lockGuard = 0;
	ret = ksceKernelInitializeFastMutex((SceKernelFastMutex *)&audiodecResident.mutex,
					  "SceAudiodecResident", 0x8000, NULL);
	if (ret < 0)
		return ret;
	audiodecResident.manager.pMutex = &audiodecResident.mutex;
	param.celp.size = sizeof(param.celp);
	param.celp.totalStreams = 7;
	ret = _audiodecInternalInitLibrary(&audiodecResident.manager, &param, 1);
	if (ret < 0)
		return ret;
	ret = ksceKernelCreateProcessLocalStorage("SceAudiodec", 7952);
	if (ret >= 0)
		audiodecResident.processStorage = ret;
	return ret;
}

int _sceAudiodecProcessCreateEntry(SceUID pid)
{
	AudiodecProcess *process = NULL;
	unsigned int codec, decoder, channel;
	int ret;
#if FW_DEBUG
	void *caller = __builtin_return_address(0);
#endif

	ret = ksceKernelGetProcessLocalStorageAddrForPid(pid,
			audiodecResident.processStorage, (void **)&process, 1);
	if (ret < 0)
		return ret;
#if FW_DEBUG
	ksceKernelAssert(process != NULL, &processCreateStorage, caller);
#endif
	memset(process, 0, sizeof(*process));
	process->internal.managers[0].pWork = &process->internal.work[0];
	process->internal.managers[3].pDecoders = &process->internal.decoders[32];
	process->internal.managers[0].pDecoders = &process->internal.decoders[0];
	process->internal.managers[0].pChannels = process->internal.channels;
	process->internal.managers[1].pDecoders = &process->internal.decoders[16];
	process->internal.managers[2].codecType = SCE_AUDIODEC_TYPE_AAC;
	process->internal.managers[1].pWork = &process->internal.work[16];
	process->internal.managers[2].pDecoders = &process->internal.decoders[24];
	process->internal.managers[2].pWork = &process->internal.work[24];
	process->internal.managers[3].pWork = &process->internal.work[32];
	process->internal.managers[3].codecType = AUDIODEC_TYPE_AAC_MULTICHANNEL;
	process->internal.managers[4].pDecoders = &process->internal.decoders[33];
	process->internal.managers[4].pWork = &process->internal.work[33];
	process->internal.managers[0].codecType = SCE_AUDIODEC_TYPE_AT9;
	process->internal.managers[0].maxDecoders = 16;
	process->internal.managers[0].maxChannels = 16;
	process->internal.managers[1].codecType = SCE_AUDIODEC_TYPE_MP3;
	process->internal.managers[3].maxDecoders = 1;
	process->internal.managers[4].codecType = SCE_AUDIODEC_TYPE_CELP;
	process->internal.managers[5].codecType = AUDIODEC_TYPE_SILK;
	process->internal.managers[5].maxDecoders = 0;
	process->internal.managers[5].pDecoders = NULL;
	process->internal.managers[5].pWork = NULL;
	process->internal.managers[1].maxDecoders = 8;
	process->internal.managers[2].maxDecoders = 8;
	process->internal.managers[4].maxDecoders = 8;
	process->external.managers[0].codecType = SCE_AUDIODEC_TYPE_AT9;
	process->external.managers[0].maxDecoders = 16;
	process->external.managers[0].totalDecoders = 16;
	process->external.managers[0].maxChannels = 16;
	process->external.managers[0].totalChannels = 16;
	process->external.managers[0].pDecoders = &process->external.decoders[0];
	process->external.managers[0].pChannels = process->external.channels;
	process->external.managers[0].pWork = &process->external.work[0];
	process->external.managers[1].codecType = SCE_AUDIODEC_TYPE_MP3;
	process->external.managers[1].maxDecoders = 8;
	process->external.managers[1].totalDecoders = 8;
	process->external.managers[2].maxDecoders = 8;
	process->external.managers[2].totalDecoders = 8;
	process->external.managers[1].pWork = &process->external.work[16];
	process->external.managers[2].codecType = SCE_AUDIODEC_TYPE_AAC;
	process->external.managers[1].pDecoders = &process->external.decoders[16];
	process->external.managers[2].pDecoders = &process->external.decoders[24];
	process->external.managers[3].codecType = AUDIODEC_TYPE_AAC_MULTICHANNEL;
	process->external.managers[3].maxDecoders = 1;
	process->external.managers[3].totalDecoders = 1;
	process->external.managers[2].pWork = &process->external.work[24];
	process->external.managers[3].pWork = &process->external.work[32];
	process->external.managers[4].codecType = SCE_AUDIODEC_TYPE_CELP;
	process->external.managers[3].pDecoders = &process->external.decoders[32];
	process->external.managers[4].maxDecoders = 8;
	process->external.managers[4].totalDecoders = 8;
	process->external.managers[4].pDecoders = &process->external.decoders[33];
	process->external.managers[5].codecType = AUDIODEC_TYPE_SILK;
	process->external.managers[5].maxDecoders = 0;
	process->external.managers[5].totalDecoders = 0;
	process->external.managers[4].pWork = &process->external.work[33];
	process->external.managers[5].pDecoders = NULL;
	process->external.managers[5].pWork = NULL;
	for (codec = 0; codec < 6; codec++) {
		AudiodecWorkManager *internal = &process->internal.managers[codec];
		AudiodecWorkManager *external = &process->external.managers[codec];

		for (decoder = 0; decoder < internal->maxDecoders; decoder++) {
			internal->pDecoders[decoder].decodeLock = 0;
			internal->pDecoders[decoder].lockGuard = 0;
		}
		for (decoder = 0; decoder < external->maxDecoders; decoder++) {
			external->pDecoders[decoder].decodeLock = 0;
			external->pDecoders[decoder].contextSize = audiodecDecoderContextSize[codec];
			external->pDecoders[decoder].lockGuard = 0;
		}
		if (external->pChannels) {
			for (channel = 0; channel < external->maxChannels; channel++) {
				external->pChannels[channel].contextSize = audiodecChannelContextSize[codec];
				external->pChannels[channel].decoderIndex = -1;
			}
		}
		ret = ksceKernelInitializeFastMutex((SceKernelFastMutex *)&process->mutexes[codec],
						  audiodecMutexNames[codec], 0x8000, NULL);
		if (ret < 0)
			break;
		internal->pMutex = &process->mutexes[codec];
		external->pMutex = &process->mutexes[codec];
	}
	return ret;
}

int _sceAudiodecProcessExitEntry(SceUID pid)
{
	AudiodecProcess *process = NULL;
	int codec = 0;
	int ret, result;
#if FW_DEBUG
	void *caller = __builtin_return_address(0);
#endif

	ret = ksceKernelGetProcessLocalStorageAddrForPid(pid,
			audiodecResident.processStorage, (void **)&process, 0);
	result = ret & (ret >> 31);
#if FW_DEBUG
	ksceKernelAssert(process != NULL, &processExitStorage, caller);
#endif
	do {
		ret = _audiodecInternalExitLibrary(pid,
				&process->internal.managers[codec], 0, 0);
		result &= ~(ret >> 31);
		if (ret < 0)
			result = ret;
		ret = _audiodecInternalExitLibrary(pid,
				&process->external.managers[codec], 1, 0);
		result &= ~(ret >> 31);
		if (ret < 0)
			result = ret;
		ret = ksceKernelFinalizeFastMutex((SceKernelFastMutex *)&process->mutexes[codec]);
		codec++;
		result &= ~(ret >> 31);
		if (ret < 0)
			result = ret;
	} while (codec != 6);
	ret = _audiodecInternalExitLibrary(pid, &audiodecResident.manager, 0, 1);
	return ret < 0 ? ret : result;
}

SceUID _audiodecGetResidentDecoderOwner(unsigned int decoder)
{
#if FW_DEBUG
	ksceKernelAssert(decoder < 7, &residentOwnerGet, __builtin_return_address(0));
#endif
	return audiodecResident.owners[decoder];
}

int _audiodecSetResidentDecoderOwner(unsigned int decoder)
{
#if FW_DEBUG
	ksceKernelAssert(decoder < 7, &residentOwnerSet, __builtin_return_address(0));
#endif
	audiodecResident.owners[decoder] = ksceKernelGetProcessId();
	return 0;
}

int _audiodecClearResidentDecoderOwner(unsigned int decoder)
{
#if FW_DEBUG
	ksceKernelAssert(decoder < 7, &residentOwnerClear, __builtin_return_address(0));
#endif
	audiodecResident.owners[decoder] = -1;
	return 0;
}

int sceAudiodecInitLibraryForUser(SceUInt32 codecType, SceAudiodecInitParam *userParam)
{
	SceAudiodecInitParam param;
	AudiodecProcess *process;
	AudiodecWorkManager *manager;
	SceUInt32 syscall = sceSyscallEnter();
	unsigned int index;
	int ret;
#if FW_DEBUG
	void *caller = __builtin_return_address(0);
#endif

	ret = ksceKernelIsGameBudget();
	if (!ret) {
#if FW_DEBUG
		ksceKernelPrintfLevelWithInfo(1, 0, &initLibraryBudget,
			"Warning: sceAudiodecInitLibrary must not be called by non-GAME process\n");
#endif
		goto leave;
	}
	if (codecType == AUDIODEC_TYPE_SILK) {
		ret = SCE_AUDIODEC_ERROR_UNSUPPORTED;
		goto leave;
	}
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
	if (codecType - SCE_AUDIODEC_TYPE_AT9 > 5) {
		ret = SCE_AUDIODEC_ERROR_INVALID_TYPE;
		goto unlock;
	}
#if FW_DEBUG
	ksceKernelAssert(1, &copyInInfoSource, caller);
#endif
	if (!userParam || ((uintptr_t)userParam & 3)) {
		ret = SCE_AUDIODEC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = ksceKernelCopyFromUser(&param, userParam, sizeof(param));
	if (ret < 0)
		goto unlock;
	if (param.size != sizeof(param)) {
		ret = SCE_AUDIODEC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	process = ksceKernelGetProcessLocalStorageAddr(audiodecResident.processStorage);
#if FW_DEBUG
	ksceKernelAssert(process != NULL, &workManagerStorage, caller);
#endif
	if (codecType == process->internal.managers[0].codecType)
		index = 0;
	else if (codecType == process->internal.managers[1].codecType)
		index = 1;
	else if (codecType == process->internal.managers[2].codecType)
		index = 2;
	else if (codecType == process->internal.managers[3].codecType)
		index = 3;
	else if (codecType == process->internal.managers[4].codecType)
		index = 4;
	else if (codecType == process->internal.managers[5].codecType)
		index = 5;
	else {
		ret = SCE_AUDIODEC_ERROR_INVALID_TYPE;
		goto unlock;
	}
	manager = &process->internal.managers[index];
	ret = ksceKernelLockFastMutex((SceKernelFastMutex *)manager->pMutex);
	if (ret < 0)
		goto unlock;
	ret = _audiodecInternalInitLibrary(manager, &param, 1);
	ksceKernelUnlockFastMutex((SceKernelFastMutex *)manager->pMutex);
	if (ret >= 0) {
#if FW_DEBUG
		ksceKernelAssert(1, &copyOutInfoSource, caller);
		ksceKernelAssert(1, &copyOutInfoDestination, caller);
#endif
		ret = ksceKernelCopyToUser(userParam, &param, param.size);
	}
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int sceAudiodecTermLibraryForUser(SceUInt32 codecType)
{
	AudiodecProcess *process;
	AudiodecWorkManager *manager;
	SceUInt32 syscall = sceSyscallEnter();
	unsigned int index;
	int ret;
#if FW_DEBUG
	void *caller = __builtin_return_address(0);
#endif

	ret = ksceKernelIsGameBudget();
	if (!ret) {
#if FW_DEBUG
		ksceKernelPrintfLevelWithInfo(1, 0, &termLibraryBudget,
			"Warning: sceAudiodecTermLibrary must not be called by non-GAME process\n");
#endif
		goto leave;
	}
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
	process = ksceKernelGetProcessLocalStorageAddr(audiodecResident.processStorage);
#if FW_DEBUG
	ksceKernelAssert(process != NULL, &workManagerStorage, caller);
#endif
	if (codecType == process->internal.managers[0].codecType)
		index = 0;
	else if (codecType == process->internal.managers[1].codecType)
		index = 1;
	else if (codecType == process->internal.managers[2].codecType)
		index = 2;
	else if (codecType == process->internal.managers[3].codecType)
		index = 3;
	else if (codecType == process->internal.managers[4].codecType)
		index = 4;
	else if (codecType == process->internal.managers[5].codecType)
		index = 5;
	else {
		ret = SCE_AUDIODEC_ERROR_INVALID_TYPE;
		goto unlock;
	}
	manager = &process->internal.managers[index];
	ret = ksceKernelLockFastMutex((SceKernelFastMutex *)manager->pMutex);
	if (ret >= 0) {
		ret = _audiodecInternalTermLibrary(manager, 1);
		ksceKernelUnlockFastMutex((SceKernelFastMutex *)manager->pMutex);
	}
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int sceAudiodecGetContextSizeForUser(const SceAudiodecCtrl *userCtrl,
				   SceUInt32 codecType)
{
	SceAudiodecCtrl ctrl;
	AudiodecInfo info;
	SceAudiodecInfo *kernelInfo;
	const SceAudiodecInfo *userInfo;
	SceUInt32 copyType, maxSize, minSize, samplingRate, channels, tableIndex;
	SceUInt32 syscall = sceSyscallEnter();
	int ret, invalidSize;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	ctrl.pInfo = &info.publicInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
#if FW_DEBUG
	ksceKernelAssert(1, &copyInCtrlInfo, caller);
#endif
	copyType = codecType ? codecType : (SceUInt32)-1;
	kernelInfo = ctrl.pInfo;
#if FW_DEBUG
	ksceKernelAssert(1, &copyInInfoSource, caller);
#endif
	if (!userCtrl || ((uintptr_t)userCtrl & 3)) {
		ret = SCE_AUDIODEC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = ksceKernelCopyFromUser(&ctrl, userCtrl, sizeof(ctrl));
	if (ret < 0)
		goto unlock;
	if (ctrl.size != sizeof(ctrl)) {
		ret = SCE_AUDIODEC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	userInfo = ctrl.pInfo;
	ctrl.pInfo = kernelInfo;
	switch (copyType) {
	case SCE_AUDIODEC_TYPE_AT9:
		maxSize = sizeof(SceAudiodecInfoAt9);
		minSize = maxSize;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
		maxSize = sizeof(SceAudiodecInfoMp3);
		minSize = maxSize;
		break;
	case SCE_AUDIODEC_TYPE_AAC:
	case SCE_AUDIODEC_TYPE_CELP:
		maxSize = sizeof(SceAudiodecInfoAac);
		minSize = 16;
		break;
	case AUDIODEC_TYPE_SILK:
		maxSize = sizeof(AudiodecInfoSilk);
		minSize = maxSize;
		break;
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		maxSize = sizeof(AudiodecInfoAacMultichannel);
		minSize = maxSize;
		break;
	default:
		ret = SCE_AUDIODEC_ERROR_INVALID_TYPE;
		goto unlock;
	}
#if FW_DEBUG
	ksceKernelAssert(kernelInfo != NULL, &copyInInfoSource, caller);
#endif
	if (!userInfo || ((uintptr_t)userInfo & 3)) {
		ret = SCE_AUDIODEC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = ksceKernelCopyFromUser(kernelInfo, userInfo, maxSize);
	if (ret < 0)
		goto unlock;
	invalidSize = maxSize < kernelInfo->size;
	if (minSize > kernelInfo->size)
		invalidSize |= 1;
	if (invalidSize || (kernelInfo->size & 3)) {
		ret = SCE_AUDIODEC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	if (ctrl.wordLength != SCE_AUDIODEC_WORD_LENGTH_16BITS) {
		ret = SCE_AUDIODEC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
		ret = atrac9ValidateConfig(&ctrl);
		if (ret < 0)
			goto unlock;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
	case SCE_AUDIODEC_TYPE_AAC:
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		break;
	case SCE_AUDIODEC_TYPE_CELP:
		if (ctrl.pInfo->celp.excitationMode != 0 ||
		    ctrl.pInfo->celp.samplingRate != 8000) {
			ret = SCE_AUDIODEC_CELP_ERROR_INVALID_CONFIG;
			goto unlock;
		}
		if (ctrl.pInfo->celp.bitRate != 3850 &&
		    ctrl.pInfo->celp.bitRate != 4650 &&
		    ctrl.pInfo->celp.bitRate != 5700 &&
		    ctrl.pInfo->celp.bitRate != 6600 &&
		    ctrl.pInfo->celp.bitRate != 7300 &&
		    ctrl.pInfo->celp.bitRate != 8700 &&
		    ctrl.pInfo->celp.bitRate != 9900 &&
		    ctrl.pInfo->celp.bitRate != 10700 &&
		    ctrl.pInfo->celp.bitRate != 11800 &&
		    ctrl.pInfo->celp.bitRate != 12200) {
			ret = SCE_AUDIODEC_CELP_ERROR_INVALID_CONFIG;
			goto unlock;
		}
		break;
	case AUDIODEC_TYPE_SILK:
		samplingRate = ((const AudiodecInfo *)ctrl.pInfo)->silk.samplingRate;
		if (samplingRate != 12000) {
			if (samplingRate <= 12000) {
				if (samplingRate != 8000) {
					ret = (SceInt32)0x807F4001;
					goto unlock;
				}
			} else if (samplingRate != 16000 && samplingRate != 24000) {
				ret = (SceInt32)0x807F4001;
				goto unlock;
			}
		}
		break;
	default:
		ret = SCE_AUDIODEC_ERROR_INVALID_TYPE;
		goto unlock;
	}
	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
		channels = atrac9GetChannelCount(ctrl.pInfo->at9.configData);
		if (channels - 1 > 1) {
			ret = SCE_AUDIODEC_AT9_ERROR_INVALID_CONFIG;
			goto unlock;
		}
		tableIndex = 0;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
		channels = ctrl.pInfo->mp3.ch;
		if (channels - 1 > 1) {
			ret = SCE_AUDIODEC_MP3_ERROR_INVALID_CH;
			goto unlock;
		}
		tableIndex = 1;
		break;
	case SCE_AUDIODEC_TYPE_AAC:
		channels = ctrl.pInfo->aac.ch;
		if (channels - 1 > 1) {
			ret = SCE_AUDIODEC_AAC_ERROR_INVALID_CH;
			goto unlock;
		}
		tableIndex = 2;
		break;
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		channels = ctrl.pInfo->aac.ch;
		if (channels - 1 > 5) {
			ret = SCE_AUDIODEC_AAC_ERROR_INVALID_CH;
			goto unlock;
		}
		tableIndex = 3;
		break;
	case SCE_AUDIODEC_TYPE_CELP:
		channels = 1;
		tableIndex = 4;
		break;
	case AUDIODEC_TYPE_SILK:
		channels = 1;
		tableIndex = 5;
		break;
	default:
		ret = SCE_AUDIODEC_ERROR_INVALID_TYPE;
		goto unlock;
	}
	ret = audiodecDecoderContextSize[tableIndex] +
		audiodecChannelContextSize[tableIndex] * channels;
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int sceAudiodecCreateDecoderForUser(SceAudiodecCtrl *userCtrl, SceUInt32 codecType)
{
	SceAudiodecCtrl ctrl;
	AudiodecInfo info;
	const SceAudiodecInfo *userInfo = NULL;
	SceAudiodecInitParam param;
	AudiodecProcess *process;
	AudiodecWorkManager *manager;
	SceUInt32 syscall = sceSyscallEnter();
	unsigned int index, channels;
	int resident, ret;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	resident = !ksceKernelIsGameBudget();
#if FW_DEBUG
	if (resident)
		ksceKernelPrintfLevelWithInfo(1, 0, &createDecoderBudget,
			"Warning: sceAudiodecCreateDecoder must not be called by non-GAME process\n");
#endif
	if (codecType == AUDIODEC_TYPE_SILK) {
		ret = SCE_AUDIODEC_ERROR_UNSUPPORTED;
		goto leave;
	}
	ctrl.pInfo = &info.publicInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
	ret = _copyInSceAudiodecCtrl(&ctrl, userCtrl, &userInfo,
				    codecType ? codecType : (SceUInt32)-1);
	if (ret < 0)
		goto unlock;
	if (ctrl.wordLength != SCE_AUDIODEC_WORD_LENGTH_16BITS) {
		ret = SCE_AUDIODEC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	ret = audiodecValidateDecoder(&ctrl, codecType, 0, 0);
	if (ret < 0)
		goto unlock;
	if (resident && codecType != audiodecResident.manager.codecType) {
		if (codecType != SCE_AUDIODEC_TYPE_CELP) {
			ret = SCE_AUDIODEC_ERROR_UNSUPPORTED;
			goto unlock;
		}
		param.celp.size = sizeof(param.celp);
		param.celp.totalStreams = 7;
		ret = _audiodecInternalTermLibrary(&audiodecResident.manager, 0);
		if (ret < 0)
			goto unlock;
		audiodecResident.manager.codecType = codecType;
		audiodecResident.manager.maxDecoders = 7;
		ret = _audiodecInternalInitLibrary(&audiodecResident.manager, &param, 0);
		if (ret < 0)
			goto unlock;
	}
	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
		channels = atrac9GetChannelCount(ctrl.pInfo->at9.configData);
		if (channels - 1 > 1) {
			ret = SCE_AUDIODEC_AT9_ERROR_INVALID_CONFIG;
			goto unlock;
		}
		ctrl.pInfo->at9.ch = channels;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
		if (ctrl.pInfo->mp3.ch - 1 > 1) {
			ret = SCE_AUDIODEC_MP3_ERROR_INVALID_CH;
			goto unlock;
		}
		break;
	case SCE_AUDIODEC_TYPE_AAC:
		if (ctrl.pInfo->aac.ch - 1 > 1) {
			ret = SCE_AUDIODEC_AAC_ERROR_INVALID_CH;
			goto unlock;
		}
		break;
	case SCE_AUDIODEC_TYPE_CELP:
		break;
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		if (ctrl.pInfo->aac.ch - 1 > 5) {
			ret = SCE_AUDIODEC_AAC_ERROR_INVALID_CH;
			goto unlock;
		}
		break;
	default:
		ret = SCE_AUDIODEC_ERROR_INVALID_TYPE;
		goto unlock;
	}
	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
		ret = atrac9GetMaxEsSize(ctrl.pInfo->at9.configData);
		if (ret > 1024) {
			ret = SCE_AUDIODEC_AT9_ERROR_INVALID_CONFIG;
			goto unlock;
		}
		if (ret < 0)
			goto unlock;
		if (ret)
			ctrl.maxEsSize = ret;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
		ctrl.maxEsSize = 1441;
		break;
	case SCE_AUDIODEC_TYPE_AAC:
		ctrl.maxEsSize = ctrl.pInfo->aac.isAdts ? 1792 : 1536;
		break;
	case SCE_AUDIODEC_TYPE_CELP:
		ctrl.maxEsSize = 27;
		break;
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		ctrl.maxEsSize = ctrl.pInfo->aac.isAdts ? 4864 : 4608;
		break;
	default:
		ret = SCE_AUDIODEC_ERROR_INVALID_TYPE;
		goto unlock;
	}
	ret = audiodecGetPcmSize((int *)&ctrl.maxPcmSize,
			       (const AudiodecInfo *const *)&ctrl.pInfo, codecType);
	if (ret < 0)
		goto unlock;
	process = ksceKernelGetProcessLocalStorageAddr(audiodecResident.processStorage);
#if FW_DEBUG
	ksceKernelAssert(process != NULL, &workManagerStorage, caller);
#endif
	if (resident) {
		if (codecType - SCE_AUDIODEC_TYPE_CELP > 1) {
			ret = SCE_AUDIODEC_ERROR_INVALID_TYPE;
			goto unlock;
		}
		manager = &audiodecResident.manager;
	} else {
		if (codecType == process->internal.managers[0].codecType)
			index = 0;
		else if (codecType == process->internal.managers[1].codecType)
			index = 1;
		else if (codecType == process->internal.managers[2].codecType)
			index = 2;
		else if (codecType == process->internal.managers[3].codecType)
			index = 3;
		else if (codecType == process->internal.managers[4].codecType)
			index = 4;
		else if (codecType == process->internal.managers[5].codecType)
			index = 5;
		else {
			ret = SCE_AUDIODEC_ERROR_INVALID_TYPE;
			goto unlock;
		}
		manager = &process->internal.managers[index];
	}
	ret = ksceKernelLockFastMutex((SceKernelFastMutex *)manager->pMutex);
	if (ret >= 0) {
		ret = _audiodecInternalCreateDecoder(manager, &ctrl, 0, 0, resident);
		ksceKernelUnlockFastMutex((SceKernelFastMutex *)manager->pMutex);
		if (ret >= 0)
			ret = _copyOutSceAudiodecCtrl(userCtrl, (SceAudiodecInfo *)userInfo, &ctrl);
	}
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int sceAudiodecCreateDecoderResidentForUser(SceAudiodecCtrl *userCtrl, SceUInt32 codecType)
{
	SceAudiodecCtrl ctrl;
	AudiodecInfo info;
	const SceAudiodecInfo *userInfo;
	SceAudiodecInfo *kernelInfo;
	SceUInt32 copyType, maxSize, minSize, samplingRate;
	SceAudiodecInitParam param;
	AudiodecProcess *process;
	AudiodecWorkManager *manager;
	SceUInt32 syscall = sceSyscallEnter();
	unsigned int channels, streams;
	int ret, invalidSize;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	ctrl.pInfo = &info.publicInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
#if FW_DEBUG
	ksceKernelAssert(1, &copyInCtrlInfo, caller);
#endif
	copyType = codecType ? codecType : (SceUInt32)-1;
	kernelInfo = ctrl.pInfo;
#if FW_DEBUG
	ksceKernelAssert(1, &copyInInfoSource, caller);
#endif
	if (!userCtrl || ((uintptr_t)userCtrl & 3)) {
		ret = SCE_AUDIODEC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = ksceKernelCopyFromUser(&ctrl, userCtrl, sizeof(ctrl));
	if (ret < 0)
		goto unlock;
	if (ctrl.size != sizeof(ctrl)) {
		ret = SCE_AUDIODEC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	userInfo = ctrl.pInfo;
	ctrl.pInfo = kernelInfo;
	switch (copyType) {
	case SCE_AUDIODEC_TYPE_AT9:
		maxSize = sizeof(SceAudiodecInfoAt9);
		minSize = maxSize;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
		maxSize = sizeof(SceAudiodecInfoMp3);
		minSize = maxSize;
		break;
	case SCE_AUDIODEC_TYPE_AAC:
	case SCE_AUDIODEC_TYPE_CELP:
		maxSize = sizeof(SceAudiodecInfoAac);
		minSize = 16;
		break;
	case AUDIODEC_TYPE_SILK:
		maxSize = sizeof(AudiodecInfoSilk);
		minSize = maxSize;
		break;
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		maxSize = sizeof(AudiodecInfoAacMultichannel);
		minSize = maxSize;
		break;
	default:
		ret = SCE_AUDIODEC_ERROR_INVALID_TYPE;
		goto unlock;
	}
#if FW_DEBUG
	ksceKernelAssert(kernelInfo != NULL, &copyInInfoSource, caller);
#endif
	if (!userInfo || ((uintptr_t)userInfo & 3)) {
		ret = SCE_AUDIODEC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = ksceKernelCopyFromUser(kernelInfo, userInfo, maxSize);
	if (ret < 0)
		goto unlock;
	invalidSize = maxSize < kernelInfo->size;
	if (minSize > kernelInfo->size)
		invalidSize |= 1;
	if (invalidSize || (kernelInfo->size & 3)) {
		ret = SCE_AUDIODEC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	if (ctrl.wordLength != SCE_AUDIODEC_WORD_LENGTH_16BITS) {
		ret = SCE_AUDIODEC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
		ret = atrac9ValidateConfig(&ctrl);
		if (ret < 0)
			goto unlock;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
	case SCE_AUDIODEC_TYPE_AAC:
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		break;
	case SCE_AUDIODEC_TYPE_CELP:
		if (ctrl.pInfo->celp.excitationMode != 0 ||
		    ctrl.pInfo->celp.samplingRate != 8000) {
			ret = SCE_AUDIODEC_CELP_ERROR_INVALID_CONFIG;
			goto unlock;
		}
		if (ctrl.pInfo->celp.bitRate != 3850 &&
		    ctrl.pInfo->celp.bitRate != 4650 &&
		    ctrl.pInfo->celp.bitRate != 5700 &&
		    ctrl.pInfo->celp.bitRate != 6600 &&
		    ctrl.pInfo->celp.bitRate != 7300 &&
		    ctrl.pInfo->celp.bitRate != 8700 &&
		    ctrl.pInfo->celp.bitRate != 9900 &&
		    ctrl.pInfo->celp.bitRate != 10700 &&
		    ctrl.pInfo->celp.bitRate != 11800 &&
		    ctrl.pInfo->celp.bitRate != 12200) {
			ret = SCE_AUDIODEC_CELP_ERROR_INVALID_CONFIG;
			goto unlock;
		}
		break;
	case AUDIODEC_TYPE_SILK:
		samplingRate = ((const AudiodecInfo *)ctrl.pInfo)->silk.samplingRate;
		if (samplingRate != 12000) {
			if (samplingRate <= 12000) {
				if (samplingRate != 8000) {
					ret = (SceInt32)0x807F4001;
					goto unlock;
				}
			} else if (samplingRate != 16000 && samplingRate != 24000) {
				ret = (SceInt32)0x807F4001;
				goto unlock;
			}
		}
		break;
	default:
		ret = SCE_AUDIODEC_ERROR_INVALID_TYPE;
		goto unlock;
	}
	if (codecType != audiodecResident.manager.codecType) {
		if (codecType == SCE_AUDIODEC_TYPE_CELP)
			streams = 7;
		else if (codecType == AUDIODEC_TYPE_SILK)
			streams = 1;
		else {
			ret = SCE_AUDIODEC_ERROR_UNSUPPORTED;
			goto unlock;
		}
		param.celp.size = sizeof(param.celp);
		param.celp.totalStreams = streams;
		ret = _audiodecInternalTermLibrary(&audiodecResident.manager, 0);
		if (ret < 0)
			goto unlock;
		audiodecResident.manager.codecType = codecType;
		audiodecResident.manager.maxDecoders = streams;
		ret = _audiodecInternalInitLibrary(&audiodecResident.manager, &param, 0);
		if (ret < 0)
			goto unlock;
	}
	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
		channels = atrac9GetChannelCount(ctrl.pInfo->at9.configData);
		if (channels - 1 > 1) {
			ret = SCE_AUDIODEC_AT9_ERROR_INVALID_CONFIG;
			goto unlock;
		}
		ctrl.pInfo->at9.ch = channels;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
		if (ctrl.pInfo->mp3.ch - 1 > 1) {
			ret = SCE_AUDIODEC_MP3_ERROR_INVALID_CH;
			goto unlock;
		}
		break;
	case SCE_AUDIODEC_TYPE_AAC:
		if (ctrl.pInfo->aac.ch - 1 > 1) {
			ret = SCE_AUDIODEC_AAC_ERROR_INVALID_CH;
			goto unlock;
		}
		break;
	case SCE_AUDIODEC_TYPE_CELP:
	case AUDIODEC_TYPE_SILK:
		break;
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		if (ctrl.pInfo->aac.ch - 1 > 5) {
			ret = SCE_AUDIODEC_AAC_ERROR_INVALID_CH;
			goto unlock;
		}
		break;
	default:
		ret = SCE_AUDIODEC_ERROR_INVALID_TYPE;
		goto unlock;
	}
	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
		ret = atrac9GetMaxEsSize(ctrl.pInfo->at9.configData);
		if (ret > 1024) {
			ret = SCE_AUDIODEC_AT9_ERROR_INVALID_CONFIG;
			goto unlock;
		}
		if (ret < 0)
			goto unlock;
		if (ret)
			ctrl.maxEsSize = ret;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
		ctrl.maxEsSize = 1441;
		break;
	case SCE_AUDIODEC_TYPE_AAC:
		ctrl.maxEsSize = ctrl.pInfo->aac.isAdts ? 1792 : 1536;
		break;
	case SCE_AUDIODEC_TYPE_CELP:
		ctrl.maxEsSize = 27;
		break;
	case AUDIODEC_TYPE_SILK:
		ctrl.maxEsSize = 150;
		break;
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		ctrl.maxEsSize = ctrl.pInfo->aac.isAdts ? 4864 : 4608;
		break;
	default:
		ret = SCE_AUDIODEC_ERROR_INVALID_TYPE;
		goto unlock;
	}
	ret = audiodecGetPcmSize((int *)&ctrl.maxPcmSize,
			       (const AudiodecInfo *const *)&ctrl.pInfo, codecType);
	if (ret < 0)
		goto unlock;
	process = ksceKernelGetProcessLocalStorageAddr(audiodecResident.processStorage);
#if FW_DEBUG
	ksceKernelAssert(process != NULL, &workManagerStorage, caller);
#endif
	if (codecType - SCE_AUDIODEC_TYPE_CELP > 1) {
		ret = SCE_AUDIODEC_ERROR_INVALID_TYPE;
		goto unlock;
	}
	manager = &audiodecResident.manager;
	ret = ksceKernelLockFastMutex((SceKernelFastMutex *)manager->pMutex);
	if (ret >= 0) {
		ret = _audiodecInternalCreateDecoder(manager, &ctrl, 0, 0, 1);
		ksceKernelUnlockFastMutex((SceKernelFastMutex *)manager->pMutex);
		if (ret >= 0)
			ret = _copyOutSceAudiodecCtrl(userCtrl, (SceAudiodecInfo *)userInfo, &ctrl);
	}
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int sceAudiodecCreateDecoderExternalForUser(SceAudiodecCtrl *userCtrl,
				       SceUInt32 codecType, SceUIntVAddr vaContext,
				       SceUInt32 contextSize)
{
	SceAudiodecCtrl ctrl;
	AudiodecInfo info;
	const SceAudiodecInfo *userInfo;
	SceAudiodecInfo *kernelInfo;
	SceUInt32 copyType, maxSize, minSize;
	AudiodecProcess *process;
	AudiodecWorkManager *manager;
	SceUInt32 syscall = sceSyscallEnter();
	unsigned int index, channels;
	int ret, invalidSize;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	if (codecType == AUDIODEC_TYPE_SILK) {
		ret = SCE_AUDIODEC_ERROR_UNSUPPORTED;
		goto leave;
	}
	if (!vaContext) {
		ret = SCE_AUDIODEC_ERROR_INVALID_PTR;
		goto leave;
	}
	ctrl.pInfo = &info.publicInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
#if FW_DEBUG
	ksceKernelAssert(1, &copyInCtrlInfo, caller);
#endif
	copyType = codecType ? codecType : (SceUInt32)-1;
	kernelInfo = ctrl.pInfo;
#if FW_DEBUG
	ksceKernelAssert(1, &copyInInfoSource, caller);
#endif
	if (!userCtrl || ((uintptr_t)userCtrl & 3)) {
		ret = SCE_AUDIODEC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = ksceKernelCopyFromUser(&ctrl, userCtrl, sizeof(ctrl));
	if (ret < 0)
		goto unlock;
	if (ctrl.size != sizeof(ctrl)) {
		ret = SCE_AUDIODEC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	userInfo = ctrl.pInfo;
	ctrl.pInfo = kernelInfo;
	switch (copyType) {
	case SCE_AUDIODEC_TYPE_AT9:
		maxSize = sizeof(SceAudiodecInfoAt9);
		minSize = maxSize;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
		maxSize = sizeof(SceAudiodecInfoMp3);
		minSize = maxSize;
		break;
	case SCE_AUDIODEC_TYPE_AAC:
	case SCE_AUDIODEC_TYPE_CELP:
		maxSize = sizeof(SceAudiodecInfoAac);
		minSize = 16;
		break;
	case AUDIODEC_TYPE_SILK:
		maxSize = sizeof(AudiodecInfoSilk);
		minSize = maxSize;
		break;
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		maxSize = sizeof(AudiodecInfoAacMultichannel);
		minSize = maxSize;
		break;
	default:
		ret = SCE_AUDIODEC_ERROR_INVALID_TYPE;
		goto unlock;
	}
#if FW_DEBUG
	ksceKernelAssert(kernelInfo != NULL, &copyInInfoSource, caller);
#endif
	if (!userInfo || ((uintptr_t)userInfo & 3)) {
		ret = SCE_AUDIODEC_ERROR_INVALID_PTR;
		goto unlock;
	}
	ret = ksceKernelCopyFromUser(kernelInfo, userInfo, maxSize);
	if (ret < 0)
		goto unlock;
	invalidSize = maxSize < kernelInfo->size;
	if (minSize > kernelInfo->size)
		invalidSize |= 1;
	if (invalidSize || (kernelInfo->size & 3)) {
		ret = SCE_AUDIODEC_ERROR_INVALID_SIZE;
		goto unlock;
	}
	if (ctrl.wordLength != SCE_AUDIODEC_WORD_LENGTH_16BITS) {
		ret = SCE_AUDIODEC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	ret = audiodecValidateDecoder(&ctrl, codecType, vaContext, contextSize);
	if (ret < 0)
		goto unlock;
	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
		channels = atrac9GetChannelCount(ctrl.pInfo->at9.configData);
		if (channels - 1 > 1) {
			ret = SCE_AUDIODEC_AT9_ERROR_INVALID_CONFIG;
			goto unlock;
		}
		ctrl.pInfo->at9.ch = channels;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
		if (ctrl.pInfo->mp3.ch - 1 > 1) {
			ret = SCE_AUDIODEC_MP3_ERROR_INVALID_CH;
			goto unlock;
		}
		break;
	case SCE_AUDIODEC_TYPE_AAC:
		if (ctrl.pInfo->aac.ch - 1 > 1) {
			ret = SCE_AUDIODEC_AAC_ERROR_INVALID_CH;
			goto unlock;
		}
		break;
	case SCE_AUDIODEC_TYPE_CELP:
		break;
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		if (ctrl.pInfo->aac.ch - 1 > 5) {
			ret = SCE_AUDIODEC_AAC_ERROR_INVALID_CH;
			goto unlock;
		}
		break;
	default:
		ret = SCE_AUDIODEC_ERROR_INVALID_TYPE;
		goto unlock;
	}
	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
		ret = atrac9GetMaxEsSize(ctrl.pInfo->at9.configData);
		if (ret > 1024) {
			ret = SCE_AUDIODEC_AT9_ERROR_INVALID_CONFIG;
			goto unlock;
		}
		if (ret < 0)
			goto unlock;
		if (ret)
			ctrl.maxEsSize = ret;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
		ctrl.maxEsSize = 1441;
		break;
	case SCE_AUDIODEC_TYPE_AAC:
		ctrl.maxEsSize = ctrl.pInfo->aac.isAdts ? 1792 : 1536;
		break;
	case SCE_AUDIODEC_TYPE_CELP:
		ctrl.maxEsSize = 27;
		break;
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		ctrl.maxEsSize = ctrl.pInfo->aac.isAdts ? 4864 : 4608;
		break;
	default:
		ret = SCE_AUDIODEC_ERROR_INVALID_TYPE;
		goto unlock;
	}
	ret = audiodecGetPcmSize((int *)&ctrl.maxPcmSize,
			       (const AudiodecInfo *const *)&ctrl.pInfo, codecType);
	if (ret < 0)
		goto unlock;
	process = ksceKernelGetProcessLocalStorageAddr(audiodecResident.processStorage);
#if FW_DEBUG
	ksceKernelAssert(process != NULL, &workManagerStorage, caller);
#endif
	if (codecType == process->external.managers[0].codecType)
		index = 0;
	else if (codecType == process->external.managers[1].codecType)
		index = 1;
	else if (codecType == process->external.managers[2].codecType)
		index = 2;
	else if (codecType == process->external.managers[3].codecType)
		index = 3;
	else if (codecType == process->external.managers[4].codecType)
		index = 4;
	else if (codecType == process->external.managers[5].codecType)
		index = 5;
	else {
		ret = SCE_AUDIODEC_ERROR_INVALID_TYPE;
		goto unlock;
	}
	manager = &process->external.managers[index];
	ret = ksceKernelLockFastMutex((SceKernelFastMutex *)manager->pMutex);
	if (ret >= 0) {
		ret = _audiodecInternalCreateDecoder(manager, &ctrl, vaContext, contextSize, 0);
		ksceKernelUnlockFastMutex((SceKernelFastMutex *)manager->pMutex);
		if (ret >= 0)
			ret = _copyOutSceAudiodecCtrl(userCtrl, (SceAudiodecInfo *)userInfo, &ctrl);
	}
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}


int sceAudiodecDeleteDecoderForUser(SceAudiodecCtrl *userCtrl)
{
	SceAudiodecCtrl ctrl;
	AudiodecInfo info;
	const SceAudiodecInfo *userInfo = NULL;
	AudiodecProcess *process;
	AudiodecWorkManager *manager;
	SceUInt32 syscall = sceSyscallEnter();
	SceUInt32 codecType, decoderIndex, index, sdkVersion;
	SceKernelIntrStatus intr;
	int resident, external, ret, lockResult;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	resident = !ksceKernelIsGameBudget();
#if FW_DEBUG
	if (resident)
		ksceKernelPrintfLevelWithInfo(1, 0, &deleteDecoderBudget,
			"Warning: sceAudiodecDeleteDecoder must not be called by non-GAME process\n");
#endif
	ctrl.pInfo = &info.publicInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
	ret = _copyInSceAudiodecCtrl(&ctrl, userCtrl, &userInfo, 0);
	if (ret < 0)
		goto unlock;
	if (ctrl.wordLength != SCE_AUDIODEC_WORD_LENGTH_16BITS) {
		ret = SCE_AUDIODEC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	if (audiodecIsExternalHandle(ctrl.handle))
		goto invalid;
	if (resident) {
		if (!audiodecIsResidentHandle(ctrl.handle))
			goto invalid;
		codecType = audiodecGetHandleCodecType(ctrl.handle);
		decoderIndex = audiodecGetHandleIndex(ctrl.handle);
		audiodecIsExternalHandle(ctrl.handle);
		process = ksceKernelGetProcessLocalStorageAddr(audiodecResident.processStorage);
#if FW_DEBUG
		ksceKernelAssert(process != NULL, &workManagerStorage, caller);
#endif
		if (codecType - SCE_AUDIODEC_TYPE_CELP > 1)
			goto invalid;
		manager = &audiodecResident.manager;
	} else {
		if (audiodecIsResidentHandle(ctrl.handle))
			goto invalid;
		codecType = audiodecGetHandleCodecType(ctrl.handle);
		decoderIndex = audiodecGetHandleIndex(ctrl.handle);
		external = audiodecIsExternalHandle(ctrl.handle);
		process = ksceKernelGetProcessLocalStorageAddr(audiodecResident.processStorage);
#if FW_DEBUG
		ksceKernelAssert(process != NULL, &workManagerStorage, caller);
#endif
		if (external) {
			if (codecType == process->external.managers[0].codecType)
				index = 0;
			else if (codecType == process->external.managers[1].codecType)
				index = 1;
			else if (codecType == process->external.managers[2].codecType)
				index = 2;
			else if (codecType == process->external.managers[3].codecType)
				index = 3;
			else if (codecType == process->external.managers[4].codecType)
				index = 4;
			else if (codecType == process->external.managers[5].codecType)
				index = 5;
			else
				goto invalid;
			manager = &process->external.managers[index];
		} else {
			if (codecType == process->internal.managers[0].codecType)
				index = 0;
			else if (codecType == process->internal.managers[1].codecType)
				index = 1;
			else if (codecType == process->internal.managers[2].codecType)
				index = 2;
			else if (codecType == process->internal.managers[3].codecType)
				index = 3;
			else if (codecType == process->internal.managers[4].codecType)
				index = 4;
			else if (codecType == process->internal.managers[5].codecType)
				index = 5;
			else
				goto invalid;
			manager = &process->internal.managers[index];
		}
	}
	index = audiodecGetHandleIndex(ctrl.handle);
	if (!manager->totalDecoders) {
		ret = SCE_AUDIODEC_ERROR_NOT_INITIALIZED;
		goto unlock;
	}
	if (index >= manager->totalDecoders)
		goto invalid;
	if (!manager->pDecoders[index].active) {
		ret = SCE_AUDIODEC_ERROR_NOT_HANDLE_IN_USE;
		goto unlock;
	}
	ret = ksceKernelLockFastMutex((SceKernelFastMutex *)manager->pMutex);
	if (ret < 0)
		goto unlock;
	for (;;) {
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&manager->pDecoders[decoderIndex].lockGuard);
		lockResult = ksceKernelSpinlockLowTrylock(&manager->pDecoders[decoderIndex].decodeLock);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&manager->pDecoders[decoderIndex].lockGuard, intr);
		if (lockResult >= 0)
			break;
		sdkVersion = 0;
		if (SceProcessmgrForDriver_D141C076(0, &sdkVersion) < 0 || sdkVersion > 0x30FFFFF) {
			ret = (SceInt32)0xC07F0013;
			goto unlockMutex;
		}
		ksceKernelDelayThread(100);
	}
	ret = _audiodecInternalDeleteDecoder(manager, &ctrl, NULL);
	ksceKernelSpinlockLowUnlock(&manager->pDecoders[decoderIndex].decodeLock);
unlockMutex:
	ksceKernelUnlockFastMutex((SceKernelFastMutex *)manager->pMutex);
	if (ret >= 0)
		ret = _copyOutSceAudiodecCtrl(userCtrl, (SceAudiodecInfo *)userInfo, &ctrl);
	goto unlock;
invalid:
	ret = SCE_AUDIODEC_ERROR_INVALID_HANDLE;
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int sceAudiodecDeleteDecoderResidentForUser(SceAudiodecCtrl *userCtrl)
{
	SceAudiodecCtrl ctrl;
	AudiodecInfo info;
	const SceAudiodecInfo *userInfo = NULL;
	AudiodecProcess *process;
	AudiodecWorkManager *manager;
	SceUInt32 syscall = sceSyscallEnter();
	SceUInt32 codecType, decoderIndex, index, sdkVersion;
	SceKernelIntrStatus intr;
	int ret, lockResult;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	ctrl.pInfo = &info.publicInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
	ret = _copyInSceAudiodecCtrl(&ctrl, userCtrl, &userInfo, 0);
	if (ret < 0)
		goto unlock;
	if (ctrl.wordLength != SCE_AUDIODEC_WORD_LENGTH_16BITS) {
		ret = SCE_AUDIODEC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	if (audiodecIsExternalHandle(ctrl.handle))
		goto invalid;
	if (!audiodecIsResidentHandle(ctrl.handle))
		goto invalid;
	codecType = audiodecGetHandleCodecType(ctrl.handle);
	decoderIndex = audiodecGetHandleIndex(ctrl.handle);
	audiodecIsExternalHandle(ctrl.handle);
	process = ksceKernelGetProcessLocalStorageAddr(audiodecResident.processStorage);
#if FW_DEBUG
	ksceKernelAssert(process != NULL, &workManagerStorage, caller);
#endif
	if (codecType - SCE_AUDIODEC_TYPE_CELP > 1)
		goto invalid;
	manager = &audiodecResident.manager;
	index = audiodecGetHandleIndex(ctrl.handle);
	if (!manager->totalDecoders) {
		ret = SCE_AUDIODEC_ERROR_NOT_INITIALIZED;
		goto unlock;
	}
	if (index >= manager->totalDecoders)
		goto invalid;
	if (!manager->pDecoders[index].active) {
		ret = SCE_AUDIODEC_ERROR_NOT_HANDLE_IN_USE;
		goto unlock;
	}
	ret = ksceKernelLockFastMutex((SceKernelFastMutex *)manager->pMutex);
	if (ret < 0)
		goto unlock;
	for (;;) {
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&manager->pDecoders[decoderIndex].lockGuard);
		lockResult = ksceKernelSpinlockLowTrylock(&manager->pDecoders[decoderIndex].decodeLock);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&manager->pDecoders[decoderIndex].lockGuard, intr);
		if (lockResult >= 0)
			break;
		sdkVersion = 0;
		if (SceProcessmgrForDriver_D141C076(0, &sdkVersion) < 0 || sdkVersion > 0x30FFFFF) {
			ret = (SceInt32)0xC07F0013;
			goto unlockMutex;
		}
		ksceKernelDelayThread(100);
	}
	ret = _audiodecInternalDeleteDecoder(manager, &ctrl, NULL);
	ksceKernelSpinlockLowUnlock(&manager->pDecoders[decoderIndex].decodeLock);
unlockMutex:
	ksceKernelUnlockFastMutex((SceKernelFastMutex *)manager->pMutex);
	if (ret >= 0)
		ret = _copyOutSceAudiodecCtrl(userCtrl, (SceAudiodecInfo *)userInfo, &ctrl);
	goto unlock;
invalid:
	ret = SCE_AUDIODEC_ERROR_INVALID_HANDLE;
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int sceAudiodecDeleteDecoderExternalForUser(SceAudiodecCtrl *userCtrl,
				       SceUIntVAddr *vaContext)
{
	SceAudiodecCtrl ctrl;
	AudiodecInfo info;
	const SceAudiodecInfo *userInfo = NULL;
	AudiodecProcess *process;
	AudiodecWorkManager *manager;
	SceUInt32 syscall = sceSyscallEnter();
	SceUInt32 codecType, decoderIndex, index, sdkVersion;
	SceKernelIntrStatus intr;
	int external, ret, lockResult;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	if (!vaContext) {
		ret = SCE_AUDIODEC_ERROR_INVALID_PTR;
		goto leave;
	}
	ctrl.pInfo = &info.publicInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
	ret = _copyInSceAudiodecCtrl(&ctrl, userCtrl, &userInfo, 0);
	if (ret < 0)
		goto unlock;
	if (ctrl.wordLength != SCE_AUDIODEC_WORD_LENGTH_16BITS) {
		ret = SCE_AUDIODEC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	if (!audiodecIsExternalHandle(ctrl.handle))
		goto invalid;
	if (audiodecIsResidentHandle(ctrl.handle))
		goto invalid;
	codecType = audiodecGetHandleCodecType(ctrl.handle);
	decoderIndex = audiodecGetHandleIndex(ctrl.handle);
	external = audiodecIsExternalHandle(ctrl.handle);
	process = ksceKernelGetProcessLocalStorageAddr(audiodecResident.processStorage);
#if FW_DEBUG
	ksceKernelAssert(process != NULL, &workManagerStorage, caller);
#endif
	if (external) {
		if (codecType == process->external.managers[0].codecType)
			index = 0;
		else if (codecType == process->external.managers[1].codecType)
			index = 1;
		else if (codecType == process->external.managers[2].codecType)
			index = 2;
		else if (codecType == process->external.managers[3].codecType)
			index = 3;
		else if (codecType == process->external.managers[4].codecType)
			index = 4;
		else if (codecType == process->external.managers[5].codecType)
			index = 5;
		else
			goto invalid;
		manager = &process->external.managers[index];
	} else {
		if (codecType == process->internal.managers[0].codecType)
			index = 0;
		else if (codecType == process->internal.managers[1].codecType)
			index = 1;
		else if (codecType == process->internal.managers[2].codecType)
			index = 2;
		else if (codecType == process->internal.managers[3].codecType)
			index = 3;
		else if (codecType == process->internal.managers[4].codecType)
			index = 4;
		else if (codecType == process->internal.managers[5].codecType)
			index = 5;
		else
			goto invalid;
		manager = &process->internal.managers[index];
	}
	index = audiodecGetHandleIndex(ctrl.handle);
	if (!manager->totalDecoders) {
		ret = SCE_AUDIODEC_ERROR_NOT_INITIALIZED;
		goto unlock;
	}
	if (index >= manager->totalDecoders)
		goto invalid;
	if (!manager->pDecoders[index].active) {
		ret = SCE_AUDIODEC_ERROR_NOT_HANDLE_IN_USE;
		goto unlock;
	}
	ret = ksceKernelLockFastMutex((SceKernelFastMutex *)manager->pMutex);
	if (ret < 0)
		goto unlock;
	for (;;) {
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&manager->pDecoders[decoderIndex].lockGuard);
		lockResult = ksceKernelSpinlockLowTrylock(&manager->pDecoders[decoderIndex].decodeLock);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&manager->pDecoders[decoderIndex].lockGuard, intr);
		if (lockResult >= 0)
			break;
		sdkVersion = 0;
		if (SceProcessmgrForDriver_D141C076(0, &sdkVersion) < 0 || sdkVersion > 0x30FFFFF) {
			ret = (SceInt32)0xC07F0013;
			goto unlockMutex;
		}
		ksceKernelDelayThread(100);
	}
	ret = _audiodecInternalDeleteDecoder(manager, &ctrl, vaContext);
	ksceKernelSpinlockLowUnlock(&manager->pDecoders[decoderIndex].decodeLock);
unlockMutex:
	ksceKernelUnlockFastMutex((SceKernelFastMutex *)manager->pMutex);
	if (ret >= 0)
		ret = _copyOutSceAudiodecCtrl(userCtrl, (SceAudiodecInfo *)userInfo, &ctrl);
	goto unlock;
invalid:
	ret = SCE_AUDIODEC_ERROR_INVALID_HANDLE;
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}


int sceAudiodecDecodeForUser(SceAudiodecCtrl *userCtrl)
{
	SceAudiodecCtrl ctrl;
	AudiodecInfo info;
	const SceAudiodecInfo *userInfo = NULL;
	AudiodecProcess *process;
	AudiodecWorkManager *manager;
	SceUInt32 syscall = sceSyscallEnter();
	SceUInt32 codecType, decoderIndex, index, sdkVersion;
	SceKernelIntrStatus intr;
	int resident, external, ret, lockResult;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	ctrl.pInfo = &info.publicInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
	ret = _copyInSceAudiodecCtrl(&ctrl, userCtrl, &userInfo, 0);
	if (ret < 0)
		goto unlock;
	if (ctrl.wordLength != SCE_AUDIODEC_WORD_LENGTH_16BITS) {
		ret = SCE_AUDIODEC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	codecType = audiodecGetHandleCodecType(ctrl.handle);
	decoderIndex = audiodecGetHandleIndex(ctrl.handle);
	external = audiodecIsExternalHandle(ctrl.handle);
	resident = audiodecIsResidentHandle(ctrl.handle);
	process = ksceKernelGetProcessLocalStorageAddr(audiodecResident.processStorage);
#if FW_DEBUG
	ksceKernelAssert(process != NULL, &workManagerStorage, caller);
#endif
	if (resident) {
		if (codecType - SCE_AUDIODEC_TYPE_CELP > 1)
			goto invalid;
		manager = &audiodecResident.manager;
	} else {
		if (external) {
			if (codecType == process->external.managers[0].codecType)
				index = 0;
			else if (codecType == process->external.managers[1].codecType)
				index = 1;
			else if (codecType == process->external.managers[2].codecType)
				index = 2;
			else if (codecType == process->external.managers[3].codecType)
				index = 3;
			else if (codecType == process->external.managers[4].codecType)
				index = 4;
			else if (codecType == process->external.managers[5].codecType)
				index = 5;
			else
				goto invalid;
			manager = &process->external.managers[index];
		} else {
			if (codecType == process->internal.managers[0].codecType)
				index = 0;
			else if (codecType == process->internal.managers[1].codecType)
				index = 1;
			else if (codecType == process->internal.managers[2].codecType)
				index = 2;
			else if (codecType == process->internal.managers[3].codecType)
				index = 3;
			else if (codecType == process->internal.managers[4].codecType)
				index = 4;
			else if (codecType == process->internal.managers[5].codecType)
				index = 5;
			else
				goto invalid;
			manager = &process->internal.managers[index];
		}
	}
	index = audiodecGetHandleIndex(ctrl.handle);
	if (!manager->totalDecoders) {
		ret = SCE_AUDIODEC_ERROR_NOT_INITIALIZED;
		goto unlock;
	}
	if (index >= manager->totalDecoders)
		goto invalid;
	if (!manager->pDecoders[index].active) {
		ret = SCE_AUDIODEC_ERROR_NOT_HANDLE_IN_USE;
		goto unlock;
	}
	ret = audiodecLockDecodeBuffers(&ctrl, &manager->pWork, 1, 0, 0);
	if (ret < 0)
		goto unlockBuffers;
	for (;;) {
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&manager->pDecoders[decoderIndex].lockGuard);
		lockResult = ksceKernelSpinlockLowTrylock(&manager->pDecoders[decoderIndex].decodeLock);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&manager->pDecoders[decoderIndex].lockGuard, intr);
		if (lockResult >= 0)
			break;
		sdkVersion = 0;
		if (SceProcessmgrForDriver_D141C076(0, &sdkVersion) < 0 || sdkVersion > 0x30FFFFF) {
			ret = (SceInt32)0xC07F0013;
			goto unlockBuffers;
		}
		ksceKernelDelayThread(100);
	}
	ret = _audiodecInternalDecode(manager, &ctrl, 1, 0, 0);
	ksceKernelSpinlockLowUnlock(&manager->pDecoders[decoderIndex].decodeLock);
unlockBuffers:
	index = audiodecGetHandleIndex(ctrl.handle);
	if (manager->pWork[index].inputEsSize) {
		ksceKernelUnlockRangeWithMode(16, ctrl.pEs, manager->pWork[index].inputEsSize);
		manager->pWork[index].inputEsSize = 0;
	}
	if (manager->pWork[index].outputPcmSize) {
		ksceKernelUnlockRangeWithMode(48, ctrl.pPcm, manager->pWork[index].outputPcmSize);
		manager->pWork[index].outputPcmSize = 0;
	}
	if (ret >= 0)
		ret = _copyOutSceAudiodecCtrl(userCtrl, (SceAudiodecInfo *)userInfo, &ctrl);
	goto unlock;
invalid:
	ret = SCE_AUDIODEC_ERROR_INVALID_HANDLE;
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}


int sceAudiodecDecodeNFramesForUser(SceAudiodecCtrl *userCtrl, SceUInt32 nFrames)
{
	SceAudiodecCtrl ctrl;
	AudiodecInfo info;
	const SceAudiodecInfo *userInfo = NULL;
	AudiodecProcess *process;
	AudiodecWorkManager *manager;
	SceUInt32 syscall = sceSyscallEnter();
	SceUInt32 codecType, decoderIndex, index, sdkVersion;
	SceKernelIntrStatus intr;
	int resident, external, ret, lockResult;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	if (!nFrames) {
		ret = SCE_AUDIODEC_ERROR_INVALID_NFRAMES;
		goto leave;
	}
	ctrl.pInfo = &info.publicInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
	ret = _copyInSceAudiodecCtrl(&ctrl, userCtrl, &userInfo, 0);
	if (ret < 0)
		goto unlock;
	if (ctrl.wordLength != SCE_AUDIODEC_WORD_LENGTH_16BITS) {
		ret = SCE_AUDIODEC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	codecType = audiodecGetHandleCodecType(ctrl.handle);
	decoderIndex = audiodecGetHandleIndex(ctrl.handle);
	external = audiodecIsExternalHandle(ctrl.handle);
	resident = audiodecIsResidentHandle(ctrl.handle);
	process = ksceKernelGetProcessLocalStorageAddr(audiodecResident.processStorage);
#if FW_DEBUG
	ksceKernelAssert(process != NULL, &workManagerStorage, caller);
#endif
	if (resident) {
		if (codecType - SCE_AUDIODEC_TYPE_CELP > 1)
			goto invalid;
		manager = &audiodecResident.manager;
	} else {
		if (external) {
			if (codecType == process->external.managers[0].codecType)
				index = 0;
			else if (codecType == process->external.managers[1].codecType)
				index = 1;
			else if (codecType == process->external.managers[2].codecType)
				index = 2;
			else if (codecType == process->external.managers[3].codecType)
				index = 3;
			else if (codecType == process->external.managers[4].codecType)
				index = 4;
			else if (codecType == process->external.managers[5].codecType)
				index = 5;
			else
				goto invalid;
			manager = &process->external.managers[index];
		} else {
			if (codecType == process->internal.managers[0].codecType)
				index = 0;
			else if (codecType == process->internal.managers[1].codecType)
				index = 1;
			else if (codecType == process->internal.managers[2].codecType)
				index = 2;
			else if (codecType == process->internal.managers[3].codecType)
				index = 3;
			else if (codecType == process->internal.managers[4].codecType)
				index = 4;
			else if (codecType == process->internal.managers[5].codecType)
				index = 5;
			else
				goto invalid;
			manager = &process->internal.managers[index];
		}
	}
	index = audiodecGetHandleIndex(ctrl.handle);
	if (!manager->totalDecoders) {
		ret = SCE_AUDIODEC_ERROR_NOT_INITIALIZED;
		goto unlock;
	}
	if (index >= manager->totalDecoders)
		goto invalid;
	if (!manager->pDecoders[index].active) {
		ret = SCE_AUDIODEC_ERROR_NOT_HANDLE_IN_USE;
		goto unlock;
	}
	ret = audiodecLockDecodeBuffers(&ctrl, &manager->pWork, nFrames, 0, 0);
	if (ret < 0)
		goto unlockBuffers;
	for (;;) {
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&manager->pDecoders[decoderIndex].lockGuard);
		lockResult = ksceKernelSpinlockLowTrylock(&manager->pDecoders[decoderIndex].decodeLock);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&manager->pDecoders[decoderIndex].lockGuard, intr);
		if (lockResult >= 0)
			break;
		sdkVersion = 0;
		if (SceProcessmgrForDriver_D141C076(0, &sdkVersion) < 0 || sdkVersion > 0x30FFFFF) {
			ret = (SceInt32)0xC07F0013;
			goto unlockBuffers;
		}
		ksceKernelDelayThread(100);
	}
	ret = _audiodecInternalDecode(manager, &ctrl, nFrames, 0, 0);
	ksceKernelSpinlockLowUnlock(&manager->pDecoders[decoderIndex].decodeLock);
unlockBuffers:
	index = audiodecGetHandleIndex(ctrl.handle);
	if (manager->pWork[index].inputEsSize) {
		ksceKernelUnlockRangeWithMode(16, ctrl.pEs, manager->pWork[index].inputEsSize);
		manager->pWork[index].inputEsSize = 0;
	}
	if (manager->pWork[index].outputPcmSize) {
		ksceKernelUnlockRangeWithMode(48, ctrl.pPcm, manager->pWork[index].outputPcmSize);
		manager->pWork[index].outputPcmSize = 0;
	}
	if (ret >= 0)
		ret = _copyOutSceAudiodecCtrl(userCtrl, (SceAudiodecInfo *)userInfo, &ctrl);
	goto unlock;
invalid:
	ret = SCE_AUDIODEC_ERROR_INVALID_HANDLE;
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}


int sceAudiodecPartlyDecodeForUser(SceAudiodecCtrl *userCtrl,
				    SceUInt32 startSample, SceUInt32 nSamples)
{
	SceAudiodecCtrl ctrl;
	AudiodecInfo info;
	const SceAudiodecInfo *userInfo = NULL;
	AudiodecProcess *process;
	AudiodecWorkManager *manager;
	SceUInt32 syscall = sceSyscallEnter();
	SceUInt32 codecType, decoderIndex, index, sdkVersion;
	SceKernelIntrStatus intr;
	int resident, external, ret, lockResult;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	ctrl.pInfo = &info.publicInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
	ret = _copyInSceAudiodecCtrl(&ctrl, userCtrl, &userInfo, 0);
	if (ret < 0)
		goto unlock;
	if (ctrl.wordLength != SCE_AUDIODEC_WORD_LENGTH_16BITS) {
		ret = SCE_AUDIODEC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	codecType = audiodecGetHandleCodecType(ctrl.handle);
	decoderIndex = audiodecGetHandleIndex(ctrl.handle);
	external = audiodecIsExternalHandle(ctrl.handle);
	resident = audiodecIsResidentHandle(ctrl.handle);
	process = ksceKernelGetProcessLocalStorageAddr(audiodecResident.processStorage);
#if FW_DEBUG
	ksceKernelAssert(process != NULL, &workManagerStorage, caller);
#endif
	if (resident) {
		if (codecType - SCE_AUDIODEC_TYPE_CELP > 1)
			goto invalid;
		manager = &audiodecResident.manager;
	} else {
		if (external) {
			if (codecType == process->external.managers[0].codecType)
				index = 0;
			else if (codecType == process->external.managers[1].codecType)
				index = 1;
			else if (codecType == process->external.managers[2].codecType)
				index = 2;
			else if (codecType == process->external.managers[3].codecType)
				index = 3;
			else if (codecType == process->external.managers[4].codecType)
				index = 4;
			else if (codecType == process->external.managers[5].codecType)
				index = 5;
			else
				goto invalid;
			manager = &process->external.managers[index];
		} else {
			if (codecType == process->internal.managers[0].codecType)
				index = 0;
			else if (codecType == process->internal.managers[1].codecType)
				index = 1;
			else if (codecType == process->internal.managers[2].codecType)
				index = 2;
			else if (codecType == process->internal.managers[3].codecType)
				index = 3;
			else if (codecType == process->internal.managers[4].codecType)
				index = 4;
			else if (codecType == process->internal.managers[5].codecType)
				index = 5;
			else
				goto invalid;
			manager = &process->internal.managers[index];
		}
	}
	index = audiodecGetHandleIndex(ctrl.handle);
	if (!manager->totalDecoders) {
		ret = SCE_AUDIODEC_ERROR_NOT_INITIALIZED;
		goto unlock;
	}
	if (index >= manager->totalDecoders)
		goto invalid;
	if (!manager->pDecoders[index].active) {
		ret = SCE_AUDIODEC_ERROR_NOT_HANDLE_IN_USE;
		goto unlock;
	}
	ret = audiodecLockDecodeBuffers(&ctrl, &manager->pWork, 0, startSample, nSamples);
	if (ret < 0)
		goto unlockBuffers;
	for (;;) {
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&manager->pDecoders[decoderIndex].lockGuard);
		lockResult = ksceKernelSpinlockLowTrylock(&manager->pDecoders[decoderIndex].decodeLock);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&manager->pDecoders[decoderIndex].lockGuard, intr);
		if (lockResult >= 0)
			break;
		sdkVersion = 0;
		if (SceProcessmgrForDriver_D141C076(0, &sdkVersion) < 0 || sdkVersion > 0x30FFFFF) {
			ret = (SceInt32)0xC07F0013;
			goto unlockBuffers;
		}
		ksceKernelDelayThread(100);
	}
	ret = _audiodecInternalDecode(manager, &ctrl, 0, startSample, nSamples);
	ksceKernelSpinlockLowUnlock(&manager->pDecoders[decoderIndex].decodeLock);
unlockBuffers:
	index = audiodecGetHandleIndex(ctrl.handle);
	if (manager->pWork[index].inputEsSize) {
		ksceKernelUnlockRangeWithMode(16, ctrl.pEs, manager->pWork[index].inputEsSize);
		manager->pWork[index].inputEsSize = 0;
	}
	if (manager->pWork[index].outputPcmSize) {
		ksceKernelUnlockRangeWithMode(48, ctrl.pPcm, manager->pWork[index].outputPcmSize);
		manager->pWork[index].outputPcmSize = 0;
	}
	if (ret >= 0)
		ret = _copyOutSceAudiodecCtrl(userCtrl, (SceAudiodecInfo *)userInfo, &ctrl);
	goto unlock;
invalid:
	ret = SCE_AUDIODEC_ERROR_INVALID_HANDLE;
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}


int sceAudiodecDecodeNStreamsForUser(SceAudiodecCtrl *const *userCtrls,
				     SceUInt32 nStreams)
{
	SceUInt32 syscall = sceSyscallEnter();
	SceAudiodecCtrl *ctrlPtrs[7] = { NULL };
	const SceAudiodecInfo *userInfos[7];
	AudiodecWorkManager *managers[7];
	AudiodecInfo infos[7];
	SceAudiodecCtrl ctrls[7];
	AudiodecProcess *process;
	SceUInt32 codecType, index, i, j, locked, sdkVersion;
	SceKernelIntrStatus intr;
	int external, resident, ret, lockResult;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	ctrls[0].pInfo = &infos[0].publicInfo;
	ctrls[1].pInfo = &infos[1].publicInfo;
	ctrls[2].pInfo = &infos[2].publicInfo;
	ctrls[3].pInfo = &infos[3].publicInfo;
	ctrls[4].pInfo = &infos[4].publicInfo;
	ctrls[5].pInfo = &infos[5].publicInfo;
	ctrls[6].pInfo = &infos[6].publicInfo;
	ret = SCE_AUDIODEC_ERROR_INVALID_PTR;
	if (!userCtrls || ((uintptr_t)userCtrls & 3))
		goto leave;
	ret = SCE_AUDIODEC_ERROR_INVALID_NSTREAMS;
	if (nStreams - 1 > 6)
		goto leave;
	ret = ksceKernelCopyFromUser(ctrlPtrs, userCtrls, sizeof(*ctrlPtrs) * nStreams);
	if (ret < 0)
		goto leave;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
	ret = _copyInSceAudiodecCtrl(&ctrls[0], ctrlPtrs[0], &userInfos[0], 0);
	if (ret < 0)
		goto unlock;
	if (ctrls[0].wordLength != SCE_AUDIODEC_WORD_LENGTH_16BITS) {
		ret = SCE_AUDIODEC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	codecType = audiodecGetHandleCodecType(ctrls[0].handle);
	if (codecType - SCE_AUDIODEC_TYPE_AT9 > 5)
		goto invalid;
	if (nStreams > audiodecMaxStreams[codecType - SCE_AUDIODEC_TYPE_AT9]) {
		ret = SCE_AUDIODEC_ERROR_INVALID_NSTREAMS;
		goto unlock;
	}
	for (i = 1; i < nStreams; i++) {
		ret = _copyInSceAudiodecCtrl(&ctrls[i], ctrlPtrs[i], &userInfos[i], 0);
		if (ret < 0)
			goto unlock;
		if (ctrls[i].wordLength != SCE_AUDIODEC_WORD_LENGTH_16BITS) {
			ret = SCE_AUDIODEC_ERROR_INVALID_WORD_LENGTH;
			goto unlock;
		}
	}
	codecType = audiodecGetHandleCodecType(ctrls[0].handle);
	for (i = 0; i < nStreams; i++) {
		if (audiodecGetHandleCodecType(ctrls[i].handle) != codecType) {
			ret = SCE_AUDIODEC_ERROR_DIFFERENT_TYPES;
			goto unlock;
		}
	}
	for (i = 0; i < nStreams - 1; i++) {
		for (j = i + 1; j < nStreams; j++) {
			if (ctrls[i].handle == ctrls[j].handle) {
				ret = SCE_AUDIODEC_ERROR_SAME_HANDLES;
				goto unlock;
			}
		}
	}
#if FW_DEBUG
	ksceKernelAssert(1, &workManagersDestination, caller);
#endif
	process = ksceKernelGetProcessLocalStorageAddr(audiodecResident.processStorage);
#if FW_DEBUG
	ksceKernelAssert(process != NULL, &workManagersStorage, caller);
#endif
	for (i = 0; i < nStreams; i++) {
		__builtin_prefetch(&ctrls[i + 3]);
		codecType = audiodecGetHandleCodecType(ctrls[i].handle);
		external = audiodecIsExternalHandle(ctrls[i].handle);
		resident = audiodecIsResidentHandle(ctrls[i].handle);
		managers[i] = NULL;
		if (resident) {
			if (codecType - SCE_AUDIODEC_TYPE_CELP > 1)
				goto invalid;
			managers[i] = &audiodecResident.manager;
		} else if (external) {
			if (codecType == process->external.managers[0].codecType)
				index = 0;
			else if (codecType == process->external.managers[1].codecType)
				index = 1;
			else if (codecType == process->external.managers[2].codecType)
				index = 2;
			else if (codecType == process->external.managers[3].codecType)
				index = 3;
			else if (codecType == process->external.managers[4].codecType)
				index = 4;
			else if (codecType == process->external.managers[5].codecType)
				index = 5;
			else
				goto invalid;
			managers[i] = &process->external.managers[index];
		} else {
			if (codecType == process->internal.managers[0].codecType)
				index = 0;
			else if (codecType == process->internal.managers[1].codecType)
				index = 1;
			else if (codecType == process->internal.managers[2].codecType)
				index = 2;
			else if (codecType == process->internal.managers[3].codecType)
				index = 3;
			else if (codecType == process->internal.managers[4].codecType)
				index = 4;
			else if (codecType == process->internal.managers[5].codecType)
				index = 5;
			else
				goto invalid;
			managers[i] = &process->internal.managers[index];
		}
	}
	for (i = 0; i < nStreams; i++) {
		index = audiodecGetHandleIndex(ctrls[i].handle);
		if (!managers[i]->totalDecoders) {
			ret = SCE_AUDIODEC_ERROR_NOT_INITIALIZED;
			goto unlock;
		}
		if (index >= managers[i]->totalDecoders)
			goto invalid;
		if (!managers[i]->pDecoders[index].active) {
			ret = SCE_AUDIODEC_ERROR_NOT_HANDLE_IN_USE;
			goto unlock;
		}
	}
	for (i = 0; i < nStreams; i++) {
		ret = audiodecLockDecodeBuffers(&ctrls[i], &managers[i]->pWork, 1, 0, 0);
		if (ret < 0)
			goto unlockBuffers;
	}
	locked = 0;
	while (locked < nStreams) {
		index = audiodecGetHandleIndex(ctrls[locked].handle);
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&managers[locked]->pDecoders[index].lockGuard);
		lockResult = ksceKernelSpinlockLowTrylock(&managers[locked]->pDecoders[index].decodeLock);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&managers[locked]->pDecoders[index].lockGuard, intr);
		if (lockResult >= 0) {
			locked++;
			continue;
		}
		for (i = 0; i < locked; i++) {
			index = audiodecGetHandleIndex(ctrls[i].handle);
			ksceKernelSpinlockLowUnlock(&managers[i]->pDecoders[index].decodeLock);
		}
		sdkVersion = 0;
		if (SceProcessmgrForDriver_D141C076(0, &sdkVersion) < 0 || sdkVersion > 0x30FFFFF) {
			ret = (SceInt32)0xC07F0013;
			goto unlockBuffers;
		}
		locked = 0;
		ksceKernelDelayThread(100);
	}
	ret = _audiodecInternalDecodeNStreams(managers, ctrls, nStreams);
	for (i = 0; i < nStreams; i++) {
		index = audiodecGetHandleIndex(ctrls[i].handle);
		ksceKernelSpinlockLowUnlock(&managers[i]->pDecoders[index].decodeLock);
	}
unlockBuffers:
	for (i = 0; i < nStreams; i++) {
		index = audiodecGetHandleIndex(ctrls[i].handle);
		if (managers[i]->pWork[index].inputEsSize) {
			ksceKernelUnlockRangeWithMode(16, ctrls[i].pEs, managers[i]->pWork[index].inputEsSize);
			managers[i]->pWork[index].inputEsSize = 0;
		}
		if (managers[i]->pWork[index].outputPcmSize) {
			ksceKernelUnlockRangeWithMode(48, ctrls[i].pPcm, managers[i]->pWork[index].outputPcmSize);
			managers[i]->pWork[index].outputPcmSize = 0;
		}
	}
	if (ret >= 0) {
		for (i = 0; i < nStreams; i++) {
			ret = _copyOutSceAudiodecCtrl(ctrlPtrs[i], (SceAudiodecInfo *)userInfos[i], &ctrls[i]);
			if (ret < 0)
				break;
		}
	}
	goto unlock;
invalid:
	ret = SCE_AUDIODEC_ERROR_INVALID_HANDLE;
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}
int sceAudiodecClearContextForUser(SceAudiodecCtrl *userCtrl)
{
	SceAudiodecCtrl ctrl;
	AudiodecInfo info;
	const SceAudiodecInfo *userInfo = NULL;
	AudiodecProcess *process;
	AudiodecWorkManager *manager;
	SceUInt32 syscall = sceSyscallEnter();
	SceUInt32 codecType, decoderIndex, index, sdkVersion, bitRate, samplingRate;
	SceKernelIntrStatus intr;
	int resident, external, ret, lockResult;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	ctrl.pInfo = &info.publicInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
	ret = _copyInSceAudiodecCtrl(&ctrl, userCtrl, &userInfo, 0);
	if (ret < 0)
		goto unlock;
	if (ctrl.wordLength != SCE_AUDIODEC_WORD_LENGTH_16BITS) {
		ret = SCE_AUDIODEC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	switch (audiodecGetHandleCodecType(ctrl.handle)) {
	case SCE_AUDIODEC_TYPE_AT9:
	case SCE_AUDIODEC_TYPE_MP3:
	case SCE_AUDIODEC_TYPE_AAC:
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		break;
	case SCE_AUDIODEC_TYPE_CELP:
		if (ctrl.pInfo->celp.excitationMode != 0 ||
		    ctrl.pInfo->celp.samplingRate != 8000)
			goto invalidCelp;
		bitRate = ctrl.pInfo->celp.bitRate;
		if (bitRate != 3850 && bitRate != 4650 && bitRate != 5700 &&
		    bitRate != 6600 && bitRate != 7300 && bitRate != 8700 &&
		    bitRate != 9900 && bitRate != 10700 && bitRate != 11800 &&
		    bitRate != 12200) {
invalidCelp:
			ret = SCE_AUDIODEC_CELP_ERROR_INVALID_CONFIG;
			goto unlock;
		}
		break;
	case AUDIODEC_TYPE_SILK:
		samplingRate = ((AudiodecInfo *)ctrl.pInfo)->silk.samplingRate;
		if (samplingRate != 12000 && samplingRate != 8000 &&
		    samplingRate != 16000 && samplingRate != 24000) {
			ret = (SceInt32)0x807F4001;
			goto unlock;
		}
		break;
	default:
		ret = SCE_AUDIODEC_ERROR_INVALID_TYPE;
		goto unlock;
	}
	codecType = audiodecGetHandleCodecType(ctrl.handle);
	decoderIndex = audiodecGetHandleIndex(ctrl.handle);
	external = audiodecIsExternalHandle(ctrl.handle);
	resident = audiodecIsResidentHandle(ctrl.handle);
	process = ksceKernelGetProcessLocalStorageAddr(audiodecResident.processStorage);
#if FW_DEBUG
	ksceKernelAssert(process != NULL, &workManagerStorage, caller);
#endif
	if (resident) {
		if (codecType - SCE_AUDIODEC_TYPE_CELP > 1)
			goto invalid;
		manager = &audiodecResident.manager;
	} else {
		if (external) {
			if (codecType == process->external.managers[0].codecType)
				index = 0;
			else if (codecType == process->external.managers[1].codecType)
				index = 1;
			else if (codecType == process->external.managers[2].codecType)
				index = 2;
			else if (codecType == process->external.managers[3].codecType)
				index = 3;
			else if (codecType == process->external.managers[4].codecType)
				index = 4;
			else if (codecType == process->external.managers[5].codecType)
				index = 5;
			else
				goto invalid;
			manager = &process->external.managers[index];
		} else {
			if (codecType == process->internal.managers[0].codecType)
				index = 0;
			else if (codecType == process->internal.managers[1].codecType)
				index = 1;
			else if (codecType == process->internal.managers[2].codecType)
				index = 2;
			else if (codecType == process->internal.managers[3].codecType)
				index = 3;
			else if (codecType == process->internal.managers[4].codecType)
				index = 4;
			else if (codecType == process->internal.managers[5].codecType)
				index = 5;
			else
				goto invalid;
			manager = &process->internal.managers[index];
		}
	}
	index = audiodecGetHandleIndex(ctrl.handle);
	if (!manager->totalDecoders) {
		ret = SCE_AUDIODEC_ERROR_NOT_INITIALIZED;
		goto unlock;
	}
	if (index >= manager->totalDecoders)
		goto invalid;
	if (!manager->pDecoders[index].active) {
		ret = SCE_AUDIODEC_ERROR_NOT_HANDLE_IN_USE;
		goto unlock;
	}
	for (;;) {
		intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&manager->pDecoders[decoderIndex].lockGuard);
		lockResult = ksceKernelSpinlockLowTrylock(&manager->pDecoders[decoderIndex].decodeLock);
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&manager->pDecoders[decoderIndex].lockGuard, intr);
		if (lockResult >= 0)
			break;
		sdkVersion = 0;
		if (SceProcessmgrForDriver_D141C076(0, &sdkVersion) < 0 || sdkVersion > 0x30FFFFF) {
			ret = (SceInt32)0xC07F0013;
			goto unlock;
		}
		ksceKernelDelayThread(100);
	}
	ret = _audiodecInternalClearContext(manager, &ctrl);
	ksceKernelSpinlockLowUnlock(&manager->pDecoders[decoderIndex].decodeLock);
	if (ret >= 0)
		ret = _copyOutSceAudiodecCtrl(userCtrl, (SceAudiodecInfo *)userInfo, &ctrl);
	goto unlock;
invalid:
	ret = SCE_AUDIODEC_ERROR_INVALID_HANDLE;
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int sceAudiodecGetInternalErrorForUser(SceAudiodecCtrl *userCtrl,
				      int *internalError)
{
	SceAudiodecCtrl ctrl;
	AudiodecInfo info;
	const SceAudiodecInfo *userInfo = NULL;
	AudiodecProcess *process;
	AudiodecWorkManager *manager;
	SceUInt32 syscall = sceSyscallEnter();
	SceUInt32 codecType, index, maxSize, minSize;
	SceAudiodecInfo *copyInfo;
	int invalidSize;
	int resident, external, ret;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	ctrl.pInfo = &info.publicInfo;
	ret = _lockProcessSuspendCore();
	if (ret < 0)
		goto leave;
#if FW_DEBUG
	ksceKernelAssert(1, &copyInCtrlInfo, caller);
#endif
	copyInfo = ctrl.pInfo;
#if FW_DEBUG
	ksceKernelAssert(1, &copyInInfoSource, caller);
#endif
	if (!userCtrl || ((uintptr_t)userCtrl & 3))
		{ ret = SCE_AUDIODEC_ERROR_INVALID_PTR; goto unlock; }
	ret = ksceKernelCopyFromUser(&ctrl, userCtrl, sizeof(ctrl));
	if (ret < 0)
		goto unlock;
	if (ctrl.size != sizeof(ctrl))
		{ ret = SCE_AUDIODEC_ERROR_INVALID_SIZE; goto unlock; }
	userInfo = ctrl.pInfo;
	ctrl.pInfo = copyInfo;
	codecType = audiodecGetHandleCodecType(ctrl.handle);
	ret = SCE_AUDIODEC_ERROR_INVALID_HANDLE;
	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
		maxSize = sizeof(SceAudiodecInfoAt9);
		minSize = maxSize;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
		maxSize = sizeof(SceAudiodecInfoMp3);
		minSize = maxSize;
		break;
	case SCE_AUDIODEC_TYPE_AAC:
	case SCE_AUDIODEC_TYPE_CELP:
		maxSize = sizeof(SceAudiodecInfoAac);
		minSize = 16;
		break;
	case AUDIODEC_TYPE_SILK:
		maxSize = sizeof(AudiodecInfoSilk);
		minSize = maxSize;
		break;
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		maxSize = sizeof(AudiodecInfoAacMultichannel);
		minSize = maxSize;
		break;
	default:
		goto unlock;
	}
	copyInfo = ctrl.pInfo;
#if FW_DEBUG
	ksceKernelAssert(copyInfo != NULL, &copyInInfoSource, caller);
#endif
	if (!userInfo || ((uintptr_t)userInfo & 3))
		{ ret = SCE_AUDIODEC_ERROR_INVALID_PTR; goto unlock; }
	ret = ksceKernelCopyFromUser(copyInfo, userInfo, maxSize);
	if (ret < 0)
		goto unlock;
	invalidSize = maxSize < copyInfo->size;
	if (minSize > copyInfo->size)
		invalidSize |= 1;
	if (invalidSize || (copyInfo->size & 3))
		{ ret = SCE_AUDIODEC_ERROR_INVALID_SIZE; goto unlock; }
	if (ctrl.wordLength != SCE_AUDIODEC_WORD_LENGTH_16BITS) {
		ret = SCE_AUDIODEC_ERROR_INVALID_WORD_LENGTH;
		goto unlock;
	}
	if (!internalError || ((uintptr_t)internalError & 3)) {
		ret = SCE_AUDIODEC_ERROR_INVALID_PTR;
		goto unlock;
	}
	codecType = audiodecGetHandleCodecType(ctrl.handle);
	external = audiodecIsExternalHandle(ctrl.handle);
	resident = audiodecIsResidentHandle(ctrl.handle);
	process = ksceKernelGetProcessLocalStorageAddr(audiodecResident.processStorage);
#if FW_DEBUG
	ksceKernelAssert(process != NULL, &workManagerStorage, caller);
#endif
	if (resident) {
		if (codecType - SCE_AUDIODEC_TYPE_CELP > 1)
			goto invalid;
		manager = &audiodecResident.manager;
	} else {
		if (external) {
			if (codecType == process->external.managers[0].codecType)
				index = 0;
			else if (codecType == process->external.managers[1].codecType)
				index = 1;
			else if (codecType == process->external.managers[2].codecType)
				index = 2;
			else if (codecType == process->external.managers[3].codecType)
				index = 3;
			else if (codecType == process->external.managers[4].codecType)
				index = 4;
			else if (codecType == process->external.managers[5].codecType)
				index = 5;
			else
				goto invalid;
			manager = &process->external.managers[index];
		} else {
			if (codecType == process->internal.managers[0].codecType)
				index = 0;
			else if (codecType == process->internal.managers[1].codecType)
				index = 1;
			else if (codecType == process->internal.managers[2].codecType)
				index = 2;
			else if (codecType == process->internal.managers[3].codecType)
				index = 3;
			else if (codecType == process->internal.managers[4].codecType)
				index = 4;
			else if (codecType == process->internal.managers[5].codecType)
				index = 5;
			else
				goto invalid;
			manager = &process->internal.managers[index];
		}
	}
	index = audiodecGetHandleIndex(ctrl.handle);
	if (!manager->totalDecoders) {
		ret = SCE_AUDIODEC_ERROR_NOT_INITIALIZED;
		goto unlock;
	}
	if (index >= manager->totalDecoders)
		goto invalid;
	if (!manager->pDecoders[index].active) {
		ret = SCE_AUDIODEC_ERROR_NOT_HANDLE_IN_USE;
		goto unlock;
	}
	ret = _audiodecInternalGetInternalError(manager, &ctrl, internalError);
	if (ret >= 0)
		ret = _copyOutSceAudiodecCtrl(userCtrl, (SceAudiodecInfo *)userInfo, &ctrl);
	goto unlock;
invalid:
	ret = SCE_AUDIODEC_ERROR_INVALID_HANDLE;
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return ret;
}

int audiodecPrepareCommand(int operation, AudiodecCommand *command,
			  AudiodecWorkManager *manager, const SceAudiodecCtrl *ctrl)
{
	SceUInt32 codecType = audiodecGetHandleCodecType(ctrl->handle);
	SceUInt32 index = audiodecGetHandleIndex(ctrl->handle);
	AudiodecChannelContext *channel;
	SceUInt32 channels, total, i, count;

	memset(command, 0, sizeof(*command));
	command->pContext = manager->pDecoders[index].pContext;
	command->contextSize = manager->pDecoders[index].contextSize;
	memcpy(&command->info, &manager->pWork[index].info, ctrl->pInfo->size);
	command->inputEsSize = ctrl->inputEsSize;
	command->outputPcmSize = ctrl->outputPcmSize;
	command->handle = ctrl->handle;
	command->pEs = ctrl->pEs;
	command->pPcm = ctrl->pPcm;
	command->wordLength = ctrl->wordLength;
	if (!operation)
		memcpy(&command->info, ctrl->pInfo, ctrl->pInfo->size);
	if (codecType == SCE_AUDIODEC_TYPE_CELP) {
		if (operation == 2)
			command->info.publicInfo.celp.lostCount = ctrl->pInfo->celp.lostCount;
		else if (operation == 3)
			memcpy(&command->info, ctrl->pInfo, ctrl->pInfo->size);
	} else if (codecType == AUDIODEC_TYPE_SILK) {
		if (operation == 2)
			command->info.silk.samplingRate = ((const AudiodecInfo *)ctrl->pInfo)->silk.samplingRate;
	} else if (codecType == SCE_AUDIODEC_TYPE_AT9) {
		channels = manager->pDecoders[index].channels;
		total = manager->totalChannels;
		channel = manager->pChannels;
		if (total) {
			count = 0;
			i = 0;
			do {
				i++;
				if (channel->decoderIndex == index) {
					command->channelContexts[count++] = channel->pContext;
					if (count == channels)
						break;
				}
				channel++;
			} while (i != total);
		}
	}
	return 0;
}
int audiodecPrepareDecodeCommand(AudiodecCommand *command,
			  AudiodecWorkManager *manager, const SceAudiodecCtrl *ctrl)
{
	SceUInt32 codecType = audiodecGetHandleCodecType(ctrl->handle);
	SceUInt32 index = audiodecGetHandleIndex(ctrl->handle);
	AudiodecChannelContext *channel;
	SceUInt32 channels, total, i, count;

	memset(command, 0, sizeof(*command));
	command->pContext = manager->pDecoders[index].pContext;
	command->contextSize = manager->pDecoders[index].contextSize;
	memcpy(&command->info, &manager->pWork[index].info, ctrl->pInfo->size);
	command->inputEsSize = ctrl->inputEsSize;
	command->outputPcmSize = ctrl->outputPcmSize;
	command->handle = ctrl->handle;
	command->pEs = ctrl->pEs;
	command->pPcm = ctrl->pPcm;
	command->wordLength = ctrl->wordLength;
	if (codecType == SCE_AUDIODEC_TYPE_CELP) {
		command->info.publicInfo.celp.lostCount = ctrl->pInfo->celp.lostCount;
	} else if (codecType == AUDIODEC_TYPE_SILK) {
		command->info.silk.samplingRate = ((const AudiodecInfo *)ctrl->pInfo)->silk.samplingRate;
	} else if (codecType == SCE_AUDIODEC_TYPE_AT9) {
		channels = manager->pDecoders[index].channels;
		total = manager->totalChannels;
		channel = manager->pChannels;
		if (total) {
			count = 0;
			i = 0;
			do {
				i++;
				if (channel->decoderIndex == index) {
					command->channelContexts[count++] = channel->pContext;
					if (count == channels)
						break;
				}
				channel++;
			} while (i != total);
		}
	}
	return 0;
}
