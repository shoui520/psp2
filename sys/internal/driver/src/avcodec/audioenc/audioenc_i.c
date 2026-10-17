/* SceAudioenc internal management (Sony audioenc/audioenc_i.c). */
#include "audioenc_internal.h"
#include "../audiodec/audiodec_internal.h"
#include <sysmem/debug.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include <string.h>

SCE_DBGINFO_DEFINE(static, initLibraryContext, 117, "_audioencInternalInitLibrary");
SCE_DBGINFO_DEFINE(static, initLibraryContextSize, 120, "_audioencInternalInitLibrary");
SCE_DBGINFO_DEFINE(static, deleteEncoderContext, 592, "_audioencInternalDeleteEncoder");
SCE_DBGINFO_DEFINE(static, createEncoderContext, 409, "_audioencInternalCreateEncoder");
SCE_DBGINFO_DEFINE(static, createEncoderOwner, 433, "_audioencInternalCreateEncoder");
SCE_DBGINFO_DEFINE(static, exitLibraryContext, 756, "_audioencInternalExitLibrary");

int _audioencInternalInitLibrary(AudioencWorkManager *manager,
			       const SceAudioencInitParam *param, int allocate)
{
	AudioencCommand command;
	AudioencEncoderContext *encoder, *end;
	AudioencEncoderWork *work;
	SceUInt32 codecType = manager->codecType;
	SceUInt32 totalStreams, contextSize, totalSize;
	char *context;
	int ret;

	if (codecType != SCE_AUDIOENC_TYPE_CELP && codecType != AUDIOENC_TYPE_SILK) {
		ret = SCE_AUDIOENC_ERROR_INVALID_TYPE;
		goto done;
	}
	totalStreams = param->celp.totalStreams;
	if (manager->totalEncoders) {
		ret = SCE_AUDIOENC_ERROR_ALREADY_INITIALIZED;
		goto done;
	}
	if (totalStreams > manager->maxEncoders || !totalStreams) {
		ret = SCE_AUDIOENC_ERROR_INVALID_INIT_PARAM;
		goto done;
	}
	memset(&command, manager->totalEncoders, sizeof(command));
	command.handle = audioencMakeHandle(0, 0, codecType, 0);
	ret = audioencQueryLibraryContext(&command);
	if (ret < 0) {
		ret = SCE_AUDIOENC_ERROR_API_FAIL;
		goto done;
	}
	if (manager->codecType - SCE_AUDIOENC_TYPE_CELP > 1) {
		ret = SCE_AUDIOENC_ERROR_INVALID_TYPE;
		goto done;
	}
	contextSize = command.contextSize;
	encoder = manager->pEncoders;
	work = manager->pWork;
	totalSize = contextSize * totalStreams;
	if (allocate) {
		context = SceCodecEngineWrapperForDriver_91535820(1, totalSize);
		if (!context) {
			ret = SCE_AUDIOENC_ERROR_OUT_OF_MEMORY;
			goto done;
		}
		manager->pContext = context;
		manager->contextSize = totalSize;
	} else {
		if (totalSize > manager->contextSize) {
#if FW_DEBUG
			ksceKernelAssert(0, &initLibraryContextSize, __builtin_return_address(0));
#endif
			ret = SCE_AUDIOENC_ERROR_OUT_OF_MEMORY;
			goto done;
		}
#if FW_DEBUG
		ksceKernelAssert(manager->pContext != NULL, &initLibraryContext,
				 __builtin_return_address(0));
#endif
		context = manager->pContext;
	}
	end = encoder + totalStreams;
	do {
		encoder->pContext = context;
		encoder->contextSize = contextSize;
		context += contextSize;
		encoder->channels = 0;
		encoder->active = 0;
		encoder++;
		work->maxPcmSize = 0;
		work->maxEsSize = 0;
		work->internalError = 0;
		work++;
	} while (encoder != end);
	manager->totalEncoders = totalStreams;
	manager->usedEncoders = 0;
	manager->totalChannels = 0;
	manager->usedChannels = 0;
done:
	return ret;
}

int _audioencInternalTermLibrary(AudioencWorkManager *manager, int release)
{
	AudioencEncoderContext *encoder, *encoderEnd;
	AudioencEncoderWork *work;
	AudioencChannelContext *channels;
	SceUInt32 totalEncoders = manager->totalEncoders;
	SceUInt32 totalChannels, i;
	int ret = release;

	if (!totalEncoders)
		return SCE_AUDIOENC_ERROR_NOT_INITIALIZED;
	encoder = manager->pEncoders;
	totalChannels = manager->totalChannels;
	channels = manager->pChannels;
	work = manager->pWork;
	for (i = 0; i != totalEncoders; i++) {
		if (encoder[i].active)
			return SCE_AUDIOENC_ERROR_A_HANDLE_IN_USE;
	}
	if (release) {
		ret = SceCodecEngineWrapperForDriver_3A79A935(1, manager->pContext, manager->contextSize);
		manager->pContext = NULL;
		manager->contextSize = 0;
	}
	encoderEnd = encoder + totalEncoders;
	do {
		encoder->pContext = NULL;
		encoder->active = 0;
		encoder->contextSize = 0;
		encoder->channels = 0;
		encoder++;
		work->maxEsSize = 0;
		work->maxPcmSize = 0;
		work->internalError = 0;
		work++;
	} while (encoder != encoderEnd);
	manager->totalEncoders = 0;
	for (i = 0; i < totalChannels; i++) {
		channels[i].pContext = NULL;
		channels[i].contextSize = 0;
		channels[i].encoderIndex = -1;
	}
	manager->totalChannels = 0;
	return ret;
}

int _audioencInternalDeleteEncoder(AudioencWorkManager *manager,
				  SceAudioencCtrl *ctrl, SceUIntVAddr *vaContext)
{
	AudioencCommand command;
	AudioencEncoderContext *encoders, *encoder;
	AudioencEncoderWork *work;
	AudioencChannelContext *channel, *channelEnd;
	SceUInt32 index, encoderSize, channelSize, usedChannels, totalChannels, totalSize;
	SceUIntVAddr virtualContext;
	void *context, *physical;
	int external, resident, ret;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	index = audioencGetHandleIndex(ctrl->handle);
	if (audioencIsResidentHandle(ctrl->handle)) {
		SceUID owner = _audioencGetResidentEncoderOwner(index);

		if (owner != ksceKernelGetProcessId())
			return SCE_AUDIOENC_ERROR_INVALID_HANDLE;
	}
	audioencGetHandleCodecType(ctrl->handle);
	index = audioencGetHandleIndex(ctrl->handle);
	memset(&command, 0, sizeof(command));
	encoder = &manager->pEncoders[index];
	command.pContext = encoder->pContext;
	command.contextSize = encoder->contextSize;
#if FW_DEBUG
	__builtin___memcpy_chk(&command.info, &manager->pWork[index].info,
			     ctrl->pInfo->size, sizeof(command) - offsetof(AudioencCommand, info));
#else
	memcpy(&command.info, &manager->pWork[index].info, ctrl->pInfo->size);
#endif
	command.handle = ctrl->handle;
	command.pInputPcm = ctrl->pInputPcm;
	command.inputPcmSize = ctrl->inputPcmSize;
	command.pOutputEs = ctrl->pOutputEs;
	command.outputEsSize = ctrl->outputEsSize;
	command.wordLength = ctrl->wordLength;
	ret = audioencDeleteCommand(&command);
	if (ret < 0) {
		manager->pWork[audioencGetHandleIndex(command.handle)].internalError = ret;
		ret = SCE_AUDIOENC_ERROR_API_FAIL;
	} else {
		audioencGetHandleCodecType(command.handle);
		audioencGetHandleIndex(command.handle);
		ctrl->inputPcmSize = command.inputPcmSize;
		ctrl->outputEsSize = command.outputEsSize;
		memcpy(ctrl->pInfo, &command.info, command.info.publicInfo.size);
		ret = 0;
	}
	index = audioencGetHandleIndex(ctrl->handle);
	external = audioencIsExternalHandle(ctrl->handle);
	resident = audioencIsResidentHandle(ctrl->handle);
	encoders = manager->pEncoders;
	channel = manager->pChannels;
	totalChannels = manager->totalChannels;
	work = manager->pWork;
	encoderSize = encoders->contextSize;
	encoder = &encoders[index];
	context = encoder->pContext;
	channelSize = channel ? channel->contextSize : 0;
	usedChannels = encoder->channels;
	if (totalChannels) {
		channelEnd = channel + totalChannels;
		if (!external) {
			do {
				if (channel->encoderIndex == index)
					channel->encoderIndex = -1;
				channel++;
			} while (channel != channelEnd);
			manager->usedChannels -= usedChannels;
			encoder->channels = 0;
			encoder->active = 0;
			work[index].maxEsSize = 0;
			work[index].maxPcmSize = 0;
			memset(&work[index].info, 0, sizeof(work[index].info));
			work[index].internalError = 0;
			goto finished;
		}
		do {
			if (channel->encoderIndex == index)
				channel->encoderIndex = -1;
			channel->pContext = NULL;
			channel++;
		} while (channel != channelEnd);
		manager->usedChannels -= usedChannels;
		encoder->channels = 0;
		encoder->active = 0;
		work[index].maxEsSize = 0;
		work[index].maxPcmSize = 0;
		memset(&work[index].info, 0, sizeof(work[index].info));
		work[index].internalError = 0;
		goto releaseExternal;
	}
	manager->usedChannels -= usedChannels;
	encoder->channels = 0;
	encoder->active = 0;
	work[index].maxEsSize = 0;
	work[index].maxPcmSize = 0;
	memset(&work[index].info, 0, sizeof(work[index].info));
	work[index].internalError = 0;
	if (!external)
		goto finished;
releaseExternal:
	ret = SCE_AUDIOENC_ERROR_INVALID_PTR;
	virtualContext = 0;
	totalSize = encoderSize + usedChannels * channelSize;
	physical = SceCodecEngineWrapperForDriver_26FAF6EC(context, totalSize);
	if (!physical)
		return ret;
	ret = SceCodecEngineWrapperForDriver_BCDCEA13(0, physical, &virtualContext);
	if (ret < 0)
		return ret;
#if FW_DEBUG
	ksceKernelAssert(virtualContext != 0, &deleteEncoderContext, caller);
#endif
	ret = SceCodecEngineWrapperForDriver_C5478DD5(0, virtualContext, totalSize);
	if (ret < 0)
		return ret;
	if (vaContext)
		*vaContext = virtualContext;
	encoders[index].pContext = NULL;
finished:
	if (resident)
		_audioencClearResidentEncoderOwner(index);
	manager->usedEncoders--;
	return ret;
}

int _audioencInternalCreateEncoder(AudioencWorkManager *manager,
				   SceAudioencCtrl *ctrl, SceUIntVAddr vaContext,
				   SceSize contextSize, int resident)
{
	AudioencCommand command;
	AudioencEncoderContext *encoder;
	AudioencEncoderWork *work;
	AudioencChannelContext *channel;
	SceUInt32 maxPcmSize = ctrl->maxPcmSize, maxEsSize = ctrl->maxEsSize;
	SceUInt32 totalEncoders, totalChannels, usedEncoders, index, i, commandIndex;
	SceUInt8 *context = (SceUInt8 *)vaContext;
	void *physical;
	int channels, external, ret;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	if (manager->codecType - SCE_AUDIOENC_TYPE_CELP > 1)
		return SCE_AUDIOENC_ERROR_INVALID_TYPE;
	totalEncoders = manager->totalEncoders;
	if (!totalEncoders)
		return SCE_AUDIOENC_ERROR_NOT_INITIALIZED;
	totalChannels = manager->totalChannels;
	usedEncoders = manager->usedEncoders + 1;
	channels = totalChannels != 0;
	if (totalEncoders < usedEncoders)
		return SCE_AUDIOENC_ERROR_ALL_HANDLES_IN_USE;
	if (totalChannels < manager->usedChannels + channels)
		return SCE_AUDIOENC_ERROR_CH_SHORTAGE;
	encoder = manager->pEncoders;
	channel = manager->pChannels;
	work = manager->pWork;
	if (vaContext) {
		physical = NULL;
		ret = SceCodecEngineWrapperForDriver_FBD4853E(0, vaContext, contextSize);
		if (ret < 0)
			return ret;
		ret = SceCodecEngineWrapperForDriver_6D41287F(0, vaContext, &physical);
		if (ret >= 0) {
#if FW_DEBUG
			ksceKernelAssert(physical != NULL, &createEncoderContext, caller);
#endif
			context = SceCodecEngineWrapperForDriver_0F235F44(physical, contextSize);
		}
		if (ret < 0 || !context) {
			SceCodecEngineWrapperForDriver_C5478DD5(0, vaContext, contextSize);
			return SCE_AUDIOENC_ERROR_INVALID_PTR;
		}
		usedEncoders = manager->usedEncoders + 1;
	}
	manager->usedEncoders = usedEncoders;
	for (index = 0; index != totalEncoders; index++, encoder++) {
		if (!encoder->active) {
			if (context) {
				encoder->pContext = context;
				context += encoder->contextSize;
			}
			encoder->channels = channels;
			encoder->active = 1;
			work[index].maxPcmSize = maxPcmSize;
			work[index].maxEsSize = maxEsSize;
			memset(&work[index].info, 0, sizeof(work[index].info));
			if (resident) {
#if FW_DEBUG
				ksceKernelAssert(_audioencGetResidentEncoderOwner(index) == -1,
						 &createEncoderOwner, caller);
#endif
				_audioencSetResidentEncoderOwner(index);
			}
			break;
		}
	}
	manager->usedChannels += channels;
	for (i = 0; i != totalChannels; i++, channel++) {
		if (channel->encoderIndex == -1) {
			if (context) {
				channel->pContext = context;
				context += channel->contextSize;
			}
			channel->encoderIndex = index;
			break;
		}
	}
	external = context != NULL;
	ctrl->handle = audioencMakeHandle(external, resident, manager->codecType, index);
	audioencGetHandleCodecType(ctrl->handle);
	commandIndex = audioencGetHandleIndex(ctrl->handle);
	memset(&command, 0, sizeof(command));
	encoder = &manager->pEncoders[commandIndex];
	command.pContext = encoder->pContext;
	command.contextSize = encoder->contextSize;
#if FW_DEBUG
	__builtin___memcpy_chk(&command.info, &manager->pWork[commandIndex].info,
			     ctrl->pInfo->size, sizeof(command) - offsetof(AudioencCommand, info));
#else
	memcpy(&command.info, &manager->pWork[commandIndex].info, ctrl->pInfo->size);
#endif
	command.handle = ctrl->handle;
	command.pInputPcm = ctrl->pInputPcm;
	command.inputPcmSize = ctrl->inputPcmSize;
	command.pOutputEs = ctrl->pOutputEs;
	command.outputEsSize = ctrl->outputEsSize;
	command.wordLength = ctrl->wordLength;
#if FW_DEBUG
	__builtin___memcpy_chk(&command.info, ctrl->pInfo, ctrl->pInfo->size,
			     sizeof(command) - offsetof(AudioencCommand, info));
#else
	memcpy(&command.info, ctrl->pInfo, ctrl->pInfo->size);
#endif
	ret = audioencCreateCommand(&command);
	if (ret < 0) {
		manager->pWork[audioencGetHandleIndex(command.handle)].internalError = ret;
		_audioencInternalDeleteEncoder(manager, ctrl, NULL);
		return SCE_AUDIOENC_ERROR_API_FAIL;
	}
	audioencGetHandleCodecType(command.handle);
	commandIndex = audioencGetHandleIndex(command.handle);
	ctrl->outputEsSize = command.outputEsSize;
	ctrl->inputPcmSize = command.inputPcmSize;
	memcpy(ctrl->pInfo, &command.info, command.info.publicInfo.size);
	memcpy(&manager->pWork[commandIndex].info, &command.info, command.info.publicInfo.size);
	return 0;
}

int _audioencInternalEncode(AudioencWorkManager *manager, SceAudioencCtrl *ctrl,
			   int nFrames)
{
	AudioencCommand command;
	AudioencEncoderContext *encoder;
	SceAudioencInfo *info;
	SceUInt32 codecType, index;
	int ret;

	codecType = audioencGetHandleCodecType(ctrl->handle);
	index = audioencGetHandleIndex(ctrl->handle);
	memset(&command, 0, sizeof(command));
	info = ctrl->pInfo;
	encoder = &manager->pEncoders[index];
	command.pContext = encoder->pContext;
	command.contextSize = encoder->contextSize;
#if FW_DEBUG
	__builtin___memcpy_chk(&command.info, &manager->pWork[index].info,
			     info->size, sizeof(command) - offsetof(AudioencCommand, info));
#else
	memcpy(&command.info, &manager->pWork[index].info, info->size);
#endif
	command.handle = ctrl->handle;
	command.pInputPcm = ctrl->pInputPcm;
	command.inputPcmSize = ctrl->inputPcmSize;
	command.pOutputEs = ctrl->pOutputEs;
	command.outputEsSize = ctrl->outputEsSize;
	command.wordLength = ctrl->wordLength;
	if (codecType == AUDIOENC_TYPE_SILK) {
#if FW_DEBUG
		__builtin___memcpy_chk(&command.info, info, info->size,
				     sizeof(command) - offsetof(AudioencCommand, info));
#else
		memcpy(&command.info, info, info->size);
#endif
	}
	ret = audioencEncodeCommand(&command, nFrames);
	if (ret < 0) {
		manager->pWork[audioencGetHandleIndex(command.handle)].internalError = ret;
		return SCE_AUDIOENC_ERROR_API_FAIL;
	}
	audioencGetHandleCodecType(command.handle);
	audioencGetHandleIndex(command.handle);
	ctrl->inputPcmSize = command.inputPcmSize;
	ctrl->outputEsSize = command.outputEsSize;
	memcpy(ctrl->pInfo, &command.info, command.info.publicInfo.size);
	return 0;
}

int _audioencInternalClearContext(AudioencWorkManager *manager, SceAudioencCtrl *ctrl)
{
	AudioencCommand command;
	AudioencEncoderContext *encoder;
	SceAudioencInfo *info;
	SceUInt32 codecType, index;
	int ret;

	codecType = audioencGetHandleCodecType(ctrl->handle);
	index = audioencGetHandleIndex(ctrl->handle);
	memset(&command, 0, sizeof(command));
	info = ctrl->pInfo;
	encoder = &manager->pEncoders[index];
	command.pContext = encoder->pContext;
	command.contextSize = encoder->contextSize;
#if FW_DEBUG
	__builtin___memcpy_chk(&command.info, &manager->pWork[index].info,
			     info->size, sizeof(command) - offsetof(AudioencCommand, info));
#else
	memcpy(&command.info, &manager->pWork[index].info, info->size);
#endif
	command.handle = ctrl->handle;
	command.pInputPcm = ctrl->pInputPcm;
	command.inputPcmSize = ctrl->inputPcmSize;
	command.pOutputEs = ctrl->pOutputEs;
	command.outputEsSize = ctrl->outputEsSize;
	command.wordLength = ctrl->wordLength;
	if (codecType == SCE_AUDIOENC_TYPE_CELP) {
#if FW_DEBUG
		__builtin___memcpy_chk(&command.info, info, info->size,
				     sizeof(command) - offsetof(AudioencCommand, info));
#else
		memcpy(&command.info, info, info->size);
#endif
	}
	ret = audioencClearContextCommand(&command);
	if (ret < 0) {
		manager->pWork[audioencGetHandleIndex(command.handle)].internalError = ret;
		return SCE_AUDIOENC_ERROR_API_FAIL;
	}
	codecType = audioencGetHandleCodecType(command.handle);
	index = audioencGetHandleIndex(command.handle);
	ctrl->inputPcmSize = command.inputPcmSize;
	ctrl->outputEsSize = command.outputEsSize;
	memcpy(ctrl->pInfo, &command.info, command.info.publicInfo.size);
	if (codecType == SCE_AUDIOENC_TYPE_CELP)
		memcpy(&manager->pWork[index].info, &command.info, command.info.publicInfo.size);
	return 0;
}

int _audioencInternalGetOptInfo(AudioencWorkManager *manager, SceAudioencCtrl *ctrl)
{
	AudioencCommand command;
	AudioencEncoderContext *encoder;
	SceUInt32 index;
	int ret;

	audioencGetHandleCodecType(ctrl->handle);
	index = audioencGetHandleIndex(ctrl->handle);
	memset(&command, 0, sizeof(command));
	encoder = &manager->pEncoders[index];
	command.pContext = encoder->pContext;
	command.contextSize = encoder->contextSize;
#if FW_DEBUG
	__builtin___memcpy_chk(&command.info, &manager->pWork[index].info,
			     ctrl->pInfo->size, sizeof(command) - offsetof(AudioencCommand, info));
#else
	memcpy(&command.info, &manager->pWork[index].info, ctrl->pInfo->size);
#endif
	command.handle = ctrl->handle;
	command.pInputPcm = ctrl->pInputPcm;
	command.inputPcmSize = ctrl->inputPcmSize;
	command.pOutputEs = ctrl->pOutputEs;
	command.outputEsSize = ctrl->outputEsSize;
	command.wordLength = ctrl->wordLength;
	command.optInfo.size = ctrl->pOptInfo->size;
	ret = audioencGetOptInfoCommand(&command);
	if (ret < 0) {
		manager->pWork[audioencGetHandleIndex(command.handle)].internalError = ret;
		return SCE_AUDIOENC_ERROR_API_FAIL;
	}
	audioencGetHandleCodecType(command.handle);
	audioencGetHandleIndex(command.handle);
	ctrl->inputPcmSize = command.inputPcmSize;
	ctrl->outputEsSize = command.outputEsSize;
	memcpy(ctrl->pInfo, &command.info, command.info.publicInfo.size);
	memcpy(ctrl->pOptInfo, &command.optInfo, command.optInfo.size);
	return 0;
}

int _audioencInternalGetInternalError(AudioencWorkManager *manager,
				    SceAudioencCtrl *ctrl, int *userInternalError)
{
	SceUInt32 index = audioencGetHandleIndex(ctrl->handle);

	*userInternalError = manager->pWork[index].internalError;
	return 0;
}

int _audioencInternalExitLibrary(SceUID pid, AudioencWorkManager *manager,
			       int external, int resident)
{
	AudioencEncoderContext *encoders = manager->pEncoders;
	AudioencChannelContext *channels = manager->pChannels;
	AudioencEncoderWork *work = manager->pWork;
	AudioencEncoderContext *encoder;
	AudioencChannelContext *channel, *channelEnd;
	SceUInt32 totalEncoders = manager->totalEncoders;
	SceUInt32 encoderSize = encoders ? encoders->contextSize : 0;
	SceUInt32 channelSize = channels ? channels->contextSize : 0;
	SceUInt32 totalChannels, size, i;
	SceUIntVAddr virtualContext;
	void *context, *physical;
	int ret, result = 0;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	if (external) {
		encoder = encoders;
		for (i = 0; i != totalEncoders; i++, encoder++) {
			size = encoderSize + encoder->channels * channelSize;
			context = encoder->pContext;
#if FW_DEBUG
			__builtin_prefetch(encoder + 3);
#else
			__builtin_prefetch(encoder + 4);
#endif
			if (!size || !context)
				continue;
			virtualContext = 0;
			physical = SceCodecEngineWrapperForDriver_26FAF6EC(context, size);
			if (!physical) {
				result = SCE_AUDIOENC_ERROR_INVALID_PTR;
				continue;
			}
			ret = SceCodecEngineWrapperForDriver_BCDCEA13(pid, physical, &virtualContext);
			if (ret < 0) {
				result = ret;
				continue;
			}
#if FW_DEBUG
			ksceKernelAssert(virtualContext != 0, &exitLibraryContext, caller);
#endif
			ret = SceCodecEngineWrapperForDriver_C5478DD5(pid, virtualContext, size);
			result &= ~(ret >> 31);
			if (ret < 0)
				result = ret;
		}
		return result;
	}
	if (resident) {
		totalChannels = manager->totalChannels;
		channelEnd = channels + totalChannels;
		encoder = encoders;
		for (i = 0; i != totalEncoders; i++, encoder++, work++) {
			if (_audioencGetResidentEncoderOwner(i) != pid)
				continue;
			channel = channels;
			if (totalChannels) {
				do {
					if (channel->encoderIndex == i)
						channel->encoderIndex = -1;
					channel++;
				} while (channel != channelEnd);
			}
			__builtin_prefetch(&work[4].maxPcmSize);
			manager->usedChannels -= encoder->channels;
			encoder->channels = 0;
			encoder->active = 0;
			work->maxPcmSize = 0;
			work->maxEsSize = 0;
			work->internalError = 0;
			_audioencClearResidentEncoderOwner(i);
			manager->usedEncoders--;
		}
		return 0;
	}
	if (!manager->pContext || !manager->contextSize)
		return 0;
	ret = SceCodecEngineWrapperForDriver_3A79A935(1, manager->pContext, manager->contextSize);
	return ret < 0 ? ret : 0;
}

void audioencFinishEncodeCommand(VeneziaRpcThunk *thunk)
{
	AudioencCommand *command = thunk->memories[0].virtualBase;

	SceCodecEngineWrapperForDriver_07C53B4E(thunk);
	SceCodecEngineWrapperForDriver_D1F94D9D(thunk);
	SceCodecEngineWrapperForDriver_1C2BC9F7(thunk);
	command->pInputPcm = thunk->memories[1].virtualBase;
	command->pOutputEs = thunk->memories[2].virtualBase;
}

int audioencPackEncodeCommand(VeneziaRpcThunk *thunk, const unsigned int *input,
			     unsigned int *packed)
{
	AudioencCommand *command = (AudioencCommand *)input[0];
	VeneziaMemoryOption option;
	SceUInt32 outputEsSize;
	int ret;

	option.flags = 1;
	option.allocationSize = sizeof(*command);
	SceCodecEngineWrapperForDriver_47AED6FF(thunk, command, sizeof(*command), &option);
	outputEsSize = command->outputEsSize;
	option.flags = 21;
	option.allocationSize = command->inputPcmSize;
	SceCodecEngineWrapperForDriver_47AED6FF(thunk, command->pInputPcm,
					      command->inputPcmSize, &option);
	option.flags = 5;
	option.allocationSize = outputEsSize;
	SceCodecEngineWrapperForDriver_47AED6FF(thunk, command->pOutputEs,
					      command->outputEsSize, &option);
	SceCodecEngineWrapperForDriver_03EA6C2A(thunk);
	command->pInputPcm = (void *)thunk->memories[1].physicalBase;
	command->pOutputEs = (void *)thunk->memories[2].physicalBase;
	ret = SceCodecEngineWrapperForDriver_D1F94D9D(thunk);
	packed[0] = thunk->memories[0].physicalBase;
	packed[1] = input[1];
	packed[2] = input[2];
	packed[3] = input[3];
	return ret;
}

int audioencQueryLibraryContext(AudioencCommand *command)
{
	unsigned int input[5];
	unsigned int operation;
	SceUInt32 codecType = ((SceUInt32)command->handle >> 16) & 0x3fff;

	switch (codecType) {
	case SCE_AUDIOENC_TYPE_CELP:
		operation = 264;
		break;
	case AUDIOENC_TYPE_SILK:
		operation = 280;
		break;
	default:
		return SCE_AUDIOENC_ERROR_INVALID_TYPE;
	}
	input[0] = (unsigned int)command;
	return SceCodecEngineWrapperForDriver_391BD4FD(operation, input,
				 audiodecPackRpcMemory, (VeneziaRpcUnpack)audiodecFinishRpcMemory);
}

int audioencCreateCommand(AudioencCommand *command)
{
	unsigned int input[5];
	unsigned int operation;
	SceUInt32 codecType = ((SceUInt32)command->handle >> 16) & 0x3fff;

	switch (codecType) {
	case SCE_AUDIOENC_TYPE_CELP:
		operation = 265;
		break;
	case AUDIOENC_TYPE_SILK:
		operation = 281;
		break;
	default:
		return SCE_AUDIOENC_ERROR_INVALID_TYPE;
	}
	input[0] = (unsigned int)command;
	input[1] = SceCodecEngineWrapperForDriver_6A1184EA(0);
	return SceCodecEngineWrapperForDriver_391BD4FD(operation, input,
				 audiodecPackRpcMemory, (VeneziaRpcUnpack)audiodecFinishRpcMemory);
}

int audioencDeleteCommand(AudioencCommand *command)
{
	unsigned int input[5];
	unsigned int operation;
	SceUInt32 codecType = ((SceUInt32)command->handle >> 16) & 0x3fff;

	switch (codecType) {
	case SCE_AUDIOENC_TYPE_CELP:
		operation = 266;
		break;
	case AUDIOENC_TYPE_SILK:
		operation = 282;
		break;
	default:
		return SCE_AUDIOENC_ERROR_INVALID_TYPE;
	}
	input[0] = (unsigned int)command;
	return SceCodecEngineWrapperForDriver_391BD4FD(operation, input,
				 audiodecPackRpcMemory, (VeneziaRpcUnpack)audiodecFinishRpcMemory);
}

int audioencEncodeCommand(AudioencCommand *command, int nFrames)
{
	unsigned int input[5];
	unsigned int operation;
	SceUInt32 codecType = ((SceUInt32)command->handle >> 16) & 0x3fff;

	switch (codecType) {
	case SCE_AUDIOENC_TYPE_CELP:
		operation = 267;
		break;
	case AUDIOENC_TYPE_SILK:
		operation = 283;
		break;
	default:
		return SCE_AUDIOENC_ERROR_INVALID_TYPE;
	}
	input[0] = (unsigned int)command;
	input[1] = nFrames;
	return SceCodecEngineWrapperForDriver_391BD4FD(operation, input,
						     audioencPackEncodeCommand, audioencFinishEncodeCommand);
}

int audioencClearContextCommand(AudioencCommand *command)
{
	unsigned int input[5];
	unsigned int operation;
	SceUInt32 codecType = ((SceUInt32)command->handle >> 16) & 0x3fff;

	switch (codecType) {
	case SCE_AUDIOENC_TYPE_CELP:
		operation = 268;
		break;
	case AUDIOENC_TYPE_SILK:
		operation = 284;
		break;
	default:
		return SCE_AUDIOENC_ERROR_INVALID_TYPE;
	}
	input[0] = (unsigned int)command;
	return SceCodecEngineWrapperForDriver_391BD4FD(operation, input,
				 audiodecPackRpcMemory, (VeneziaRpcUnpack)audiodecFinishRpcMemory);
}

int audioencGetOptInfoCommand(AudioencCommand *command)
{
	unsigned int input[5];
	SceUInt32 codecType = ((SceUInt32)command->handle >> 16) & 0x3fff;

	if (codecType == SCE_AUDIOENC_TYPE_CELP) {
		input[0] = (unsigned int)command;
		return SceCodecEngineWrapperForDriver_391BD4FD(269, input,
				 audiodecPackRpcMemory, (VeneziaRpcUnpack)audiodecFinishRpcMemory);
	}
	return codecType == AUDIOENC_TYPE_SILK ?
		SCE_AUDIOENC_ERROR_UNSUPPORTED : SCE_AUDIOENC_ERROR_INVALID_TYPE;
}

__attribute__((noinline)) int audioencIsExternalHandle(SceUInt32 handle)
{
	return handle >> 31;
}

__attribute__((noinline)) int audioencIsResidentHandle(SceUInt32 handle)
{
	return (handle >> 30) & 1;
}

__attribute__((noinline)) SceUInt32 audioencGetHandleCodecType(SceUInt32 handle)
{
	return (handle >> 16) & 0x3fff;
}

__attribute__((noinline)) SceUInt32 audioencGetHandleIndex(SceUInt32 handle)
{
	return handle & 0xffff;
}

__attribute__((noinline)) SceUInt32 audioencMakeHandle(int external, int resident,
						   SceUInt32 codecType, SceUInt32 index)
{
	return ((SceUInt32)external << 31) | ((SceUInt32)(resident & 1) << 30) |
	       ((codecType & 0x3fff) << 16) | (index & 0xffff);
}
