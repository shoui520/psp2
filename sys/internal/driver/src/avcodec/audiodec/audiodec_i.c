/* SceAudiodec internal library management (Sony audiodec/audiodec_i.c). */
#include "audiodec_internal.h"
#include <sysmem/debug.h>
#include <psp2kern/kernel/threadmgr.h>
#include <string.h>

/* ATRAC9 configuration header: sample-rate index, channel layout, frame bytes,
 * and log2(frames per superframe), in Sony's supported-configuration order. */
#define ATRAC9_CONFIG(sampleRateIndex, channelConfigIndex, frameBytes, superFrameIndex) \
	{ 0xfe, ((sampleRateIndex) << 4) | ((channelConfigIndex) << 1), \
	  ((frameBytes) - 1) >> 3, ((((frameBytes) - 1) & 7) << 5) | ((superFrameIndex) << 3) }

static const SceUInt8 atrac9Configs[][4] __attribute__((aligned(4))) = {
	ATRAC9_CONFIG(1, 0, 24, 2),
	ATRAC9_CONFIG(1, 0, 32, 2),
	ATRAC9_CONFIG(1, 0, 40, 2),
	ATRAC9_CONFIG(1, 0, 48, 2),
	ATRAC9_CONFIG(1, 0, 56, 2),
	ATRAC9_CONFIG(1, 0, 64, 2),
	ATRAC9_CONFIG(1, 1, 48, 2),
	ATRAC9_CONFIG(1, 2, 48, 2),
	ATRAC9_CONFIG(1, 1, 64, 2),
	ATRAC9_CONFIG(1, 2, 64, 2),
	ATRAC9_CONFIG(1, 1, 80, 2),
	ATRAC9_CONFIG(1, 2, 80, 2),
	ATRAC9_CONFIG(1, 1, 96, 2),
	ATRAC9_CONFIG(1, 2, 96, 2),
	ATRAC9_CONFIG(1, 1, 112, 2),
	ATRAC9_CONFIG(1, 2, 112, 2),
	ATRAC9_CONFIG(1, 1, 128, 2),
	ATRAC9_CONFIG(1, 2, 128, 2),
	ATRAC9_CONFIG(1, 0, 24, 0),
	ATRAC9_CONFIG(1, 0, 32, 0),
	ATRAC9_CONFIG(1, 0, 40, 0),
	ATRAC9_CONFIG(1, 0, 48, 0),
	ATRAC9_CONFIG(1, 0, 56, 0),
	ATRAC9_CONFIG(1, 0, 64, 0),
	ATRAC9_CONFIG(1, 1, 48, 0),
	ATRAC9_CONFIG(1, 2, 48, 0),
	ATRAC9_CONFIG(1, 1, 64, 0),
	ATRAC9_CONFIG(1, 2, 64, 0),
	ATRAC9_CONFIG(1, 1, 80, 0),
	ATRAC9_CONFIG(1, 2, 80, 0),
	ATRAC9_CONFIG(1, 1, 96, 0),
	ATRAC9_CONFIG(1, 2, 96, 0),
	ATRAC9_CONFIG(1, 1, 112, 0),
	ATRAC9_CONFIG(1, 2, 112, 0),
	ATRAC9_CONFIG(1, 1, 128, 0),
	ATRAC9_CONFIG(1, 2, 128, 0),
	ATRAC9_CONFIG(1, 0, 64, 1),
	ATRAC9_CONFIG(1, 1, 64, 1),
	ATRAC9_CONFIG(1, 2, 64, 1),
	ATRAC9_CONFIG(1, 1, 72, 0),
	ATRAC9_CONFIG(1, 2, 72, 0),
	ATRAC9_CONFIG(1, 1, 72, 2),
	ATRAC9_CONFIG(1, 2, 72, 2),
	ATRAC9_CONFIG(1, 1, 256, 2),
	ATRAC9_CONFIG(1, 2, 256, 2),
	ATRAC9_CONFIG(4, 0, 24, 2),
	ATRAC9_CONFIG(4, 0, 32, 2),
	ATRAC9_CONFIG(4, 0, 40, 2),
	ATRAC9_CONFIG(4, 0, 48, 2),
	ATRAC9_CONFIG(4, 0, 56, 2),
	ATRAC9_CONFIG(4, 0, 64, 2),
	ATRAC9_CONFIG(4, 1, 48, 2),
	ATRAC9_CONFIG(4, 2, 48, 2),
	ATRAC9_CONFIG(4, 1, 64, 2),
	ATRAC9_CONFIG(4, 2, 64, 2),
	ATRAC9_CONFIG(4, 1, 80, 2),
	ATRAC9_CONFIG(4, 2, 80, 2),
	ATRAC9_CONFIG(4, 1, 96, 2),
	ATRAC9_CONFIG(4, 2, 96, 2),
	ATRAC9_CONFIG(4, 1, 112, 2),
	ATRAC9_CONFIG(4, 2, 112, 2),
	ATRAC9_CONFIG(4, 1, 128, 2),
	ATRAC9_CONFIG(4, 2, 128, 2),
	ATRAC9_CONFIG(4, 0, 24, 0),
	ATRAC9_CONFIG(4, 0, 32, 0),
	ATRAC9_CONFIG(4, 0, 40, 0),
	ATRAC9_CONFIG(4, 0, 48, 0),
	ATRAC9_CONFIG(4, 0, 56, 0),
	ATRAC9_CONFIG(4, 0, 64, 0),
	ATRAC9_CONFIG(4, 1, 48, 0),
	ATRAC9_CONFIG(4, 2, 48, 0),
	ATRAC9_CONFIG(4, 1, 64, 0),
	ATRAC9_CONFIG(4, 2, 64, 0),
	ATRAC9_CONFIG(4, 1, 80, 0),
	ATRAC9_CONFIG(4, 2, 80, 0),
	ATRAC9_CONFIG(4, 1, 96, 0),
	ATRAC9_CONFIG(4, 2, 96, 0),
	ATRAC9_CONFIG(4, 1, 112, 0),
	ATRAC9_CONFIG(4, 2, 112, 0),
	ATRAC9_CONFIG(4, 1, 128, 0),
	ATRAC9_CONFIG(4, 2, 128, 0),
	ATRAC9_CONFIG(4, 0, 80, 2),
	ATRAC9_CONFIG(4, 2, 80, 2),
	ATRAC9_CONFIG(4, 1, 88, 2),
	ATRAC9_CONFIG(4, 0, 128, 0),
	ATRAC9_CONFIG(4, 0, 128, 1),
	ATRAC9_CONFIG(4, 0, 128, 2),
	ATRAC9_CONFIG(4, 1, 128, 1),
	ATRAC9_CONFIG(4, 2, 128, 1),
	ATRAC9_CONFIG(4, 1, 160, 0),
	ATRAC9_CONFIG(4, 1, 160, 2),
	ATRAC9_CONFIG(4, 1, 256, 2),
	ATRAC9_CONFIG(4, 2, 256, 2),
	ATRAC9_CONFIG(7, 0, 24, 2),
	ATRAC9_CONFIG(7, 0, 32, 2),
	ATRAC9_CONFIG(7, 0, 40, 2),
	ATRAC9_CONFIG(7, 0, 48, 2),
	ATRAC9_CONFIG(7, 0, 56, 2),
	ATRAC9_CONFIG(7, 0, 64, 2),
	ATRAC9_CONFIG(7, 1, 48, 2),
	ATRAC9_CONFIG(7, 2, 48, 2),
	ATRAC9_CONFIG(7, 1, 64, 2),
	ATRAC9_CONFIG(7, 2, 64, 2),
	ATRAC9_CONFIG(7, 1, 80, 2),
	ATRAC9_CONFIG(7, 2, 80, 2),
	ATRAC9_CONFIG(7, 1, 96, 2),
	ATRAC9_CONFIG(7, 2, 96, 2),
	ATRAC9_CONFIG(7, 1, 112, 2),
	ATRAC9_CONFIG(7, 2, 112, 2),
	ATRAC9_CONFIG(7, 1, 128, 2),
	ATRAC9_CONFIG(7, 2, 128, 2),
	ATRAC9_CONFIG(7, 0, 24, 0),
	ATRAC9_CONFIG(7, 0, 32, 0),
	ATRAC9_CONFIG(7, 0, 40, 0),
	ATRAC9_CONFIG(7, 0, 48, 0),
	ATRAC9_CONFIG(7, 0, 56, 0),
	ATRAC9_CONFIG(7, 0, 64, 0),
	ATRAC9_CONFIG(7, 1, 48, 0),
	ATRAC9_CONFIG(7, 2, 48, 0),
	ATRAC9_CONFIG(7, 1, 64, 0),
	ATRAC9_CONFIG(7, 2, 64, 0),
	ATRAC9_CONFIG(7, 1, 80, 0),
	ATRAC9_CONFIG(7, 2, 80, 0),
	ATRAC9_CONFIG(7, 1, 96, 0),
	ATRAC9_CONFIG(7, 2, 96, 0),
	ATRAC9_CONFIG(7, 1, 112, 0),
	ATRAC9_CONFIG(7, 2, 112, 0),
	ATRAC9_CONFIG(7, 1, 128, 0),
	ATRAC9_CONFIG(7, 2, 128, 0),
	ATRAC9_CONFIG(7, 0, 128, 2),
	ATRAC9_CONFIG(7, 0, 256, 0),
	ATRAC9_CONFIG(7, 0, 256, 1),
	ATRAC9_CONFIG(7, 0, 256, 2),
	ATRAC9_CONFIG(7, 1, 256, 0),
	ATRAC9_CONFIG(7, 1, 256, 1),
	ATRAC9_CONFIG(7, 1, 256, 2),
	ATRAC9_CONFIG(7, 2, 256, 0),
	ATRAC9_CONFIG(7, 2, 256, 1),
	ATRAC9_CONFIG(7, 2, 256, 2),
};
#undef ATRAC9_CONFIG

#if FW_DEBUG
SCE_DBGINFO_DEFINE(static, initLibraryContext, 138, "_audiodecInternalInitLibrary");
SCE_DBGINFO_DEFINE(static, initLibraryContextSize, 141, "_audiodecInternalInitLibrary");
SCE_DBGINFO_DEFINE(static, deleteDecoderContext, 640, "_audiodecInternalDeleteDecoder");
SCE_DBGINFO_DEFINE(static, createDecoderContext, 453, "_audiodecInternalCreateDecoder");
SCE_DBGINFO_DEFINE(static, createDecoderOwner, 477, "_audiodecInternalCreateDecoder");
SCE_DBGINFO_DEFINE(static, exitLibraryContext, 825, "_audiodecInternalExitLibrary");
#endif

int _audiodecInternalInitLibrary(AudiodecWorkManager *manager,
			       const SceAudiodecInitParam *param, int allocate)
{
	AudiodecCommand command;
	AudiodecDecoderContext *decoder, *decoderEnd;
	AudiodecDecoderWork *work;
	AudiodecChannelContext *channel, *channelEnd;
	SceUInt32 codecType = manager->codecType;
	SceUInt32 decoders, channels, decoderSize, channelSize;
	SceUInt32 decoderBytes, totalSize;
	SceUInt8 *context, *storage;
	int ret;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
		decoders = param->at9.totalCh;
		channels = decoders;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
	case SCE_AUDIODEC_TYPE_AAC:
	case SCE_AUDIODEC_TYPE_CELP:
	case AUDIODEC_TYPE_SILK:
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		decoders = param->mp3.totalStreams;
		channels = 0;
		break;
	default:
		return SCE_AUDIODEC_ERROR_INVALID_TYPE;
	}
	if (manager->totalDecoders)
		return SCE_AUDIODEC_ERROR_ALREADY_INITIALIZED;
	if (decoders > manager->maxDecoders || !decoders)
		return SCE_AUDIODEC_ERROR_INVALID_INIT_PARAM;
	memset(&command, manager->totalDecoders, sizeof(command));
	command.handle = audiodecMakeHandle(0, 0, codecType, 0);
	ret = audiodecQueryLibraryContext(&command);
	if (ret < 0)
		return SCE_AUDIODEC_ERROR_API_FAIL;
	if (manager->codecType == SCE_AUDIODEC_TYPE_AT9) {
		decoderSize = command.contextSize;
		channelSize = command.channelContextSize;
	} else {
		if (manager->codecType < SCE_AUDIODEC_TYPE_AT9 ||
		    manager->codecType > AUDIODEC_TYPE_AAC_MULTICHANNEL)
			return SCE_AUDIODEC_ERROR_INVALID_TYPE;
		decoderSize = command.contextSize;
		channelSize = 0;
	}
	decoderBytes = decoders * decoderSize;
	totalSize = decoderBytes + channels * channelSize;
	decoder = manager->pDecoders;
	channel = manager->pChannels;
	work = manager->pWork;
	if (allocate) {
		storage = SceCodecEngineWrapperForDriver_91535820(1, totalSize);
		if (!storage)
			return SCE_AUDIODEC_ERROR_OUT_OF_MEMORY;
		manager->pContext = storage;
		manager->contextSize = totalSize;
	} else {
		if (totalSize > manager->contextSize) {
#if FW_DEBUG
			ksceKernelAssert(0, &initLibraryContextSize, caller);
#endif
			return SCE_AUDIODEC_ERROR_OUT_OF_MEMORY;
		}
#if FW_DEBUG
		ksceKernelAssert(manager->pContext != NULL, &initLibraryContext, caller);
#endif
		storage = manager->pContext;
	}
	context = storage;
	decoderEnd = decoder + decoders;
	do {
		decoder->pContext = context;
		decoder->contextSize = decoderSize;
		context += decoderSize;
		decoder->channels = 0;
		decoder->active = 0;
		decoder++;
		work->maxEsSize = 0;
		work->maxPcmSize = 0;
		work->internalError = 0;
		work++;
	} while (decoder != decoderEnd);
	manager->totalDecoders = decoders;
	manager->usedDecoders = 0;
	context = storage + decoderBytes;
	if (channels) {
		channelEnd = channel + channels;
		do {
			channel->pContext = context;
			context += channelSize;
			channel->contextSize = channelSize;
			channel->decoderIndex = -1;
			channel++;
		} while (channel != channelEnd);
	}
	manager->totalChannels = channels;
	manager->usedChannels = 0;
	return ret;
}

int _audiodecInternalTermLibrary(AudiodecWorkManager *manager, int release)
{
	AudiodecDecoderContext *decoder, *decoderEnd;
	AudiodecDecoderWork *work;
	AudiodecChannelContext *channels;
	SceUInt32 totalDecoders = manager->totalDecoders;
	SceUInt32 totalChannels, i;
	int ret = release;

	if (!totalDecoders)
		return SCE_AUDIODEC_ERROR_NOT_INITIALIZED;
	decoder = manager->pDecoders;
	totalChannels = manager->totalChannels;
	channels = manager->pChannels;
	work = manager->pWork;
	for (i = 0; i != totalDecoders; i++) {
		if (decoder[i].active)
			return SCE_AUDIODEC_ERROR_A_HANDLE_IN_USE;
	}
	if (release) {
		ret = SceCodecEngineWrapperForDriver_3A79A935(1, manager->pContext, manager->contextSize);
		manager->pContext = NULL;
		manager->contextSize = 0;
	}
	decoderEnd = decoder + totalDecoders;
	do {
		decoder->pContext = NULL;
		decoder->active = 0;
		decoder->contextSize = 0;
		decoder->channels = 0;
		decoder++;
		work->maxEsSize = 0;
		work->maxPcmSize = 0;
		work->internalError = 0;
		work++;
	} while (decoder != decoderEnd);
	manager->totalDecoders = 0;
	for (i = 0; i < totalChannels; i++) {
		channels[i].pContext = NULL;
		channels[i].contextSize = 0;
		channels[i].decoderIndex = -1;
	}
	manager->totalChannels = 0;
	return ret;
}

int _audiodecInternalDeleteDecoder(AudiodecWorkManager *manager,
				  SceAudiodecCtrl *ctrl, SceUIntVAddr *vaContext)
{
	AudiodecCommand command;
	AudiodecDecoderContext *decoders, *decoder;
	AudiodecDecoderWork *work;
	AudiodecChannelContext *channel, *channelEnd;
	SceUInt32 index, codecType, commandIndex, decoderSize, channelSize;
	SceUInt32 usedChannels, totalChannels, totalSize;
	SceUIntVAddr virtualContext;
	void *context, *physical;
	int external, resident, ret;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	index = audiodecGetHandleIndex(ctrl->handle);
	if (audiodecIsResidentHandle(ctrl->handle)) {
		SceUID owner = _audiodecGetResidentDecoderOwner(index);
		if (owner != ksceKernelGetProcessId())
			return SCE_AUDIODEC_ERROR_INVALID_HANDLE;
	}
	ret = audiodecPrepareCommand(1, &command, manager, ctrl);
	if (ret >= 0) {
		ret = audiodecDeleteCommand(&command);
		if (ret < 0) {
			if (((SceUInt32)ret & 0xffff0000U) != 0x807f0000U) {
				manager->pWork[audiodecGetHandleIndex(command.handle)].internalError = ret;
				ret = SCE_AUDIODEC_ERROR_API_FAIL;
			}
		} else {
			codecType = audiodecGetHandleCodecType(command.handle);
			commandIndex = audiodecGetHandleIndex(command.handle);
			ctrl->inputEsSize = command.inputEsSize;
			ctrl->outputPcmSize = command.outputPcmSize;
			memcpy(ctrl->pInfo, &command.info, command.info.publicInfo.size);
			if (codecType == SCE_AUDIODEC_TYPE_AT9) {
				manager->pWork[commandIndex].maxEsSize = command.maxEsSize;
				manager->pWork[commandIndex].bufferedFrames = command.bufferedFrames;
			}
			ret = 0;
		}
	}
	index = audiodecGetHandleIndex(ctrl->handle);
	external = audiodecIsExternalHandle(ctrl->handle);
	resident = audiodecIsResidentHandle(ctrl->handle);
	decoders = manager->pDecoders;
	channel = manager->pChannels;
	totalChannels = manager->totalChannels;
	work = manager->pWork;
	decoderSize = decoders->contextSize;
	decoder = &decoders[index];
	context = decoder->pContext;
	channelSize = channel ? channel->contextSize : 0;
	usedChannels = decoder->channels;
	if (totalChannels) {
		channelEnd = channel + totalChannels;
		if (!external) {
			do {
				if (channel->decoderIndex == index)
					channel->decoderIndex = -1;
				channel++;
			} while (channel != channelEnd);
			manager->usedChannels -= usedChannels;
			decoder->channels = 0;
			decoder->active = 0;
			work[index].maxEsSize = 0;
			work[index].maxPcmSize = 0;
			memset(&work[index].info, 0, sizeof(work[index].info) +
			       sizeof(work[index].samplesPerFrame) + sizeof(work[index].bufferedFrames));
			work[index].internalError = 0;
			goto finished;
		}
		do {
			if (channel->decoderIndex == index)
				channel->decoderIndex = -1;
			channel->pContext = NULL;
			channel++;
		} while (channel != channelEnd);
		manager->usedChannels -= usedChannels;
		decoder->channels = 0;
		decoder->active = 0;
		work[index].maxEsSize = 0;
		work[index].maxPcmSize = 0;
		memset(&work[index].info, 0, sizeof(work[index].info) +
		       sizeof(work[index].samplesPerFrame) + sizeof(work[index].bufferedFrames));
		work[index].internalError = 0;
		goto releaseExternal;
	}
	manager->usedChannels -= usedChannels;
	decoder->channels = 0;
	decoder->active = 0;
	work[index].maxEsSize = 0;
	work[index].maxPcmSize = 0;
	memset(&work[index].info, 0, sizeof(work[index].info) +
	       sizeof(work[index].samplesPerFrame) + sizeof(work[index].bufferedFrames));
	work[index].internalError = 0;
	if (!external)
		goto finished;
releaseExternal:
	ret = SCE_AUDIODEC_ERROR_INVALID_PTR;
	virtualContext = 0;
	totalSize = decoderSize + usedChannels * channelSize;
	physical = SceCodecEngineWrapperForDriver_26FAF6EC(context, totalSize);
	if (!physical)
		return ret;
	ret = SceCodecEngineWrapperForDriver_BCDCEA13(0, physical, &virtualContext);
	if (ret < 0)
		return ret;
#if FW_DEBUG
	ksceKernelAssert(virtualContext != 0, &deleteDecoderContext, caller);
#endif
	ret = SceCodecEngineWrapperForDriver_C5478DD5(0, virtualContext, totalSize);
	if (ret < 0)
		return ret;
	if (vaContext)
		*vaContext = virtualContext;
	decoders[index].pContext = NULL;
finished:
	if (resident)
		_audiodecClearResidentDecoderOwner(index);
	manager->usedDecoders--;
	return ret;
}

int _audiodecInternalCreateDecoder(AudiodecWorkManager *manager,
				  SceAudiodecCtrl *ctrl, SceUIntVAddr vaContext,
				  SceSize contextSize, int resident)
{
	AudiodecCommand command;
	AudiodecDecoderContext *decoders, *decoder;
	AudiodecDecoderWork *work;
	AudiodecChannelContext *channels, *channel;
	SceUInt32 maxEsSize = ctrl->maxEsSize, maxPcmSize = ctrl->maxPcmSize;
	SceUInt32 totalDecoders, totalChannels, usedDecoders, index, i;
	SceUInt32 codecType, commandIndex;
	SceUInt8 *context = (SceUInt8 *)vaContext;
	void *physical;
	int nChannels, ret;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	switch (manager->codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
	case SCE_AUDIODEC_TYPE_AAC:
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		nChannels = ctrl->pInfo->at9.ch;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
		nChannels = ctrl->pInfo->mp3.ch;
		break;
	case SCE_AUDIODEC_TYPE_CELP:
	case AUDIODEC_TYPE_SILK:
		nChannels = 1;
		break;
	default:
		return SCE_AUDIODEC_ERROR_INVALID_TYPE;
	}
	totalDecoders = manager->totalDecoders;
	if (!totalDecoders)
		return SCE_AUDIODEC_ERROR_NOT_INITIALIZED;
	totalChannels = manager->totalChannels;
	usedDecoders = manager->usedDecoders + 1;
	if (!totalChannels)
		nChannels = 0;
	if (totalDecoders < usedDecoders)
		return SCE_AUDIODEC_ERROR_ALL_HANDLES_IN_USE;
	if (totalChannels < manager->usedChannels + nChannels)
		return SCE_AUDIODEC_ERROR_CH_SHORTAGE;
	decoders = manager->pDecoders;
	channels = manager->pChannels;
	work = manager->pWork;
	if (vaContext) {
		physical = NULL;
		ret = SceCodecEngineWrapperForDriver_FBD4853E(0, vaContext, contextSize);
		if (ret < 0)
			return ret;
		ret = SceCodecEngineWrapperForDriver_6D41287F(0, vaContext, &physical);
		if (ret < 0)
			goto mappingFailed;
#if FW_DEBUG
		ksceKernelAssert(physical != NULL, &createDecoderContext, caller);
#endif
		context = SceCodecEngineWrapperForDriver_0F235F44(physical, contextSize);
		if (!context)
			goto mappingFailed;
		usedDecoders = manager->usedDecoders + 1;
	}
	decoder = decoders;
	index = 0;
	manager->usedDecoders = usedDecoders;
	do {
		if (!decoder->active) {
			if (context) {
				decoder->pContext = context;
				context += decoder->contextSize;
			}
			decoder->channels = nChannels;
			decoder->active = 1;
			work[index].maxEsSize = maxEsSize;
			work[index].maxPcmSize = maxPcmSize;
			memset(&work[index].info, 0, sizeof(work[index].info) +
			       sizeof(work[index].samplesPerFrame) + sizeof(work[index].bufferedFrames));
			if (resident) {
#if FW_DEBUG
				ksceKernelAssert(_audiodecGetResidentDecoderOwner(index) == -1,
						 &createDecoderOwner, caller);
#endif
				_audiodecSetResidentDecoderOwner(index);
			}
			break;
		}
		index++;
		decoder++;
	} while (index != totalDecoders);
	manager->usedChannels += nChannels;
	if (totalChannels && nChannels) {
		channel = channels;
		i = 0;
		do {
			if (channel->decoderIndex == -1) {
				if (context) {
					channel->pContext = context;
					context += channel->contextSize;
				}
				nChannels--;
				channel->decoderIndex = index;
			}
			i++;
			channel++;
		} while (i != totalChannels && nChannels);
	}
	ctrl->handle = audiodecMakeHandle(context != NULL, resident, manager->codecType, index);
	ret = audiodecPrepareCommand(0, &command, manager, ctrl);
	if (ret < 0)
		goto deleteDecoder;
	ret = audiodecCreateCommand(&command);
	if (ret < 0) {
		if (((SceUInt32)ret & 0xffff0000U) != 0x807f0000U) {
			manager->pWork[audiodecGetHandleIndex(command.handle)].internalError = ret;
			ret = SCE_AUDIODEC_ERROR_API_FAIL;
		}
		goto deleteDecoder;
	}
	codecType = audiodecGetHandleCodecType(command.handle);
	commandIndex = audiodecGetHandleIndex(command.handle);
	ctrl->outputPcmSize = command.outputPcmSize;
	ctrl->inputEsSize = command.inputEsSize;
	memcpy(ctrl->pInfo, &command.info, command.info.publicInfo.size);
	work = &manager->pWork[commandIndex];
	memcpy(&work->info, &command.info, command.info.publicInfo.size);
	if (codecType == SCE_AUDIODEC_TYPE_AT9) {
		work->maxEsSize = command.maxEsSize;
		work->bufferedFrames = command.bufferedFrames;
		work->samplesPerFrame = work->maxPcmSize / (2 * command.info.publicInfo.at9.ch);
	}
	return 0;
deleteDecoder:
	_audiodecInternalDeleteDecoder(manager, ctrl, NULL);
	return ret;
mappingFailed:
	SceCodecEngineWrapperForDriver_C5478DD5(0, vaContext, contextSize);
	return SCE_AUDIODEC_ERROR_INVALID_PTR;
}

int _audiodecInternalDecode(AudiodecWorkManager *manager, SceAudiodecCtrl *ctrl,
			   SceUInt32 nFrames, SceUInt32 startSample,
			   SceUInt32 nSamples)
{
	AudiodecCommand command;
	AudiodecDecoderWork *work;
	SceUInt32 codecType, index;
	int ret;

	ret = audiodecPrepareDecodeCommand(&command, manager, ctrl);
	if (ret >= 0) {
		ret = audiodecDecodeCommand(&command, nFrames, startSample, nSamples);
		if (ret < 0) {
			if (((SceUInt32)ret & 0xffff0000U) != 0x807f0000U) {
				manager->pWork[audiodecGetHandleIndex(command.handle)].internalError = ret;
				ret = SCE_AUDIODEC_ERROR_API_FAIL;
			}
		} else {
			codecType = audiodecGetHandleCodecType(command.handle);
			index = audiodecGetHandleIndex(command.handle);
			ctrl->inputEsSize = command.inputEsSize;
			ctrl->outputPcmSize = command.outputPcmSize;
			memcpy(ctrl->pInfo, &command.info, command.info.publicInfo.size);
			work = manager->pWork;
			if (codecType == SCE_AUDIODEC_TYPE_AT9) {
				work[index].maxEsSize = command.maxEsSize;
				work[index].bufferedFrames = command.bufferedFrames;
			}
			ret = 0;
		}
	}
	return ret;
}

int _audiodecInternalDecodeNStreams(AudiodecWorkManager *const *managers,
				  SceAudiodecCtrl *ctrls, SceUInt32 nStreams)
{
	AudiodecCommand commands[7];
	AudiodecCommand *command = commands;
	AudiodecWorkManager *const *currentManager = managers;
	AudiodecWorkManager *manager;
	AudiodecDecoderWork *work;
	SceAudiodecCtrl *ctrl = ctrls;
	SceUInt32 i, codecType, index;
	int ret;

	if (nStreams) {
		i = 0;
		do {
			ret = audiodecPrepareDecodeCommand(command, *currentManager++, ctrl);
			if (ret < 0)
				return ret;
			i++;
			command++;
			ctrl++;
		} while (i != nStreams);
	}
	ret = audiodecDecodeNStreamsCommand(commands, nStreams);
	if (ret < 0) {
		if (((SceUInt32)ret & 0xffff0000U) != 0x807f0000U) {
			currentManager = managers;
			for (i = 0; i != nStreams; i++) {
				manager = *currentManager++;
				index = audiodecGetHandleIndex(commands[i].handle);
				manager->pWork[index].internalError = ret;
			}
			ret = SCE_AUDIODEC_ERROR_API_FAIL;
		}
		return ret;
	}
	if (nStreams) {
		currentManager = managers;
		command = commands;
		ctrl = ctrls;
		i = 0;
		do {
			manager = *currentManager++;
			codecType = audiodecGetHandleCodecType(command->handle);
			index = audiodecGetHandleIndex(command->handle);
			ctrl->inputEsSize = command->inputEsSize;
			ctrl->outputPcmSize = command->outputPcmSize;
			memcpy(ctrl->pInfo, &command->info, command->info.publicInfo.size);
			work = manager->pWork;
			if (codecType == SCE_AUDIODEC_TYPE_AT9) {
				work[index].maxEsSize = command->maxEsSize;
				work[index].bufferedFrames = command->bufferedFrames;
			}
			i++;
			ctrl++;
			command++;
		} while (i != nStreams);
		ret = 0;
	}
	return ret;
}

int _audiodecInternalClearContext(AudiodecWorkManager *manager, SceAudiodecCtrl *ctrl)
{
	AudiodecCommand command;
	AudiodecDecoderContext *decoder;
	AudiodecDecoderWork *work;
	AudiodecChannelContext *channel;
	SceAudiodecInfo *info;
	SceUInt32 codecType, index, channels, total, i, count;
	int ret;

	codecType = audiodecGetHandleCodecType(ctrl->handle);
	index = audiodecGetHandleIndex(ctrl->handle);
	memset(&command, 0, sizeof(command));
	info = ctrl->pInfo;
	decoder = &manager->pDecoders[index];
	command.pContext = decoder->pContext;
	command.contextSize = decoder->contextSize;
#if FW_DEBUG
	__builtin___memcpy_chk(&command.info, &manager->pWork[index].info,
			     info->size, sizeof(command) - offsetof(AudiodecCommand, info));
#else
	memcpy(&command.info, &manager->pWork[index].info, info->size);
#endif
	command.handle = ctrl->handle;
	command.pEs = ctrl->pEs;
	command.inputEsSize = ctrl->inputEsSize;
	command.pPcm = ctrl->pPcm;
	command.wordLength = ctrl->wordLength;
	command.outputPcmSize = ctrl->outputPcmSize;
	if (codecType == SCE_AUDIODEC_TYPE_CELP) {
#if FW_DEBUG
		__builtin___memcpy_chk(&command.info, info, info->size,
				     sizeof(command) - offsetof(AudiodecCommand, info));
#else
		memcpy(&command.info, info, info->size);
#endif
	} else if (codecType == SCE_AUDIODEC_TYPE_AT9) {
		total = manager->totalChannels;
		channels = decoder->channels;
		channel = manager->pChannels;
		if (total) {
			count = 0;
			i = 0;
			do {
				if (channel->decoderIndex == index) {
					command.channelContexts[count++] = channel->pContext;
					if (count == channels)
						break;
				}
				i++;
				channel++;
			} while (i != total);
		}
	}
	ret = audiodecClearContextCommand(&command);
	if (ret < 0) {
		if (((SceUInt32)ret & 0xffff0000U) != 0x807f0000U) {
			index = audiodecGetHandleIndex(command.handle);
			manager->pWork[index].internalError = ret;
			ret = SCE_AUDIODEC_ERROR_API_FAIL;
		}
		return ret;
	}
	codecType = audiodecGetHandleCodecType(command.handle);
	index = audiodecGetHandleIndex(command.handle);
	ctrl->outputPcmSize = command.outputPcmSize;
	ctrl->inputEsSize = command.inputEsSize;
	memcpy(ctrl->pInfo, &command.info, command.info.publicInfo.size);
	work = &manager->pWork[index];
	if (codecType == SCE_AUDIODEC_TYPE_AT9) {
		work->maxEsSize = command.maxEsSize;
		work->bufferedFrames = command.bufferedFrames;
	} else if (codecType == SCE_AUDIODEC_TYPE_CELP) {
		memcpy(&work->info, &command.info, command.info.publicInfo.size);
	}
	return 0;
}

int _audiodecInternalGetInternalError(AudiodecWorkManager *manager,
				    SceAudiodecCtrl *ctrl, int *internalError)
{
	SceUInt32 index = audiodecGetHandleIndex(ctrl->handle);

	*internalError = manager->pWork[index].internalError;
	return 0;
}

int _audiodecInternalExitLibrary(SceUID pid, AudiodecWorkManager *manager,
			       int external, int resident)
{
	AudiodecDecoderContext *decoders = manager->pDecoders;
	AudiodecChannelContext *channels = manager->pChannels;
	AudiodecDecoderWork *work = manager->pWork;
	AudiodecDecoderContext *decoder;
	AudiodecChannelContext *channel, *channelEnd;
	SceUInt32 totalDecoders = manager->totalDecoders;
	SceUInt32 decoderSize = decoders ? decoders->contextSize : 0;
	SceUInt32 channelSize = channels ? channels->contextSize : 0;
	SceUInt32 totalChannels, size, i;
	SceUIntVAddr virtualContext;
	void *context, *physical;
	int ret, result = 0;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	if (external) {
		decoder = decoders;
		for (i = 0; i != totalDecoders; i++, decoder++) {
			size = decoderSize + decoder->channels * channelSize;
			context = decoder->pContext;
#if FW_DEBUG
			__builtin_prefetch(decoder + 3);
#else
			__builtin_prefetch(decoder + 4);
#endif
			if (!size || !context)
				continue;
			virtualContext = 0;
			physical = SceCodecEngineWrapperForDriver_26FAF6EC(context, size);
			if (!physical) {
				result = SCE_AUDIODEC_ERROR_INVALID_PTR;
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
			if (ret < 0)
				result = ret;
		}
		return result;
	}
	if (resident) {
		totalChannels = manager->totalChannels;
		channelEnd = channels + totalChannels;
		decoder = decoders;
		for (i = 0; i != totalDecoders; i++, decoder++, work++) {
			if (_audiodecGetResidentDecoderOwner(i) != pid)
				continue;
			channel = channels;
			if (totalChannels) {
				do {
					if (channel->decoderIndex == i)
						channel->decoderIndex = -1;
					channel++;
				} while (channel != channelEnd);
			}
			__builtin_prefetch(&work[4].maxEsSize);
			manager->usedChannels -= decoder->channels;
			decoder->channels = 0;
			decoder->active = 0;
			work->maxEsSize = 0;
			work->maxPcmSize = 0;
			work->internalError = 0;
			_audiodecClearResidentDecoderOwner(i);
			manager->usedDecoders--;
		}
		return 0;
	}
	if (!manager->pContext || !manager->contextSize)
		return 0;
	ret = SceCodecEngineWrapperForDriver_3A79A935(1, manager->pContext, manager->contextSize);
	return ret < 0 ? ret : 0;
}

int audiodecCalculateEsSize(SceUInt32 codecType, AudiodecDecoderWork *work,
			  SceUInt32 nFrames)
{
	SceUInt32 size = work->maxEsSize;
	SceUInt32 bufferedFrames;

	if (codecType == SCE_AUDIODEC_TYPE_AT9) {
		bufferedFrames = work->bufferedFrames;
		if (bufferedFrames < nFrames)
			size += work->info.publicInfo.at9.superFrameSize *
				((work->info.publicInfo.at9.framesInSuperFrame - 1 -
				  bufferedFrames + nFrames) / work->info.publicInfo.at9.framesInSuperFrame);
	} else {
		size *= nFrames;
	}
	return size;
}

int audiodecCalculatePcmSize(SceUInt32 codecType, AudiodecDecoderWork *work,
			   SceUInt32 nFrames, SceUInt32 startSample,
			   SceUInt32 nSamples)
{
	SceUInt32 size = work->maxPcmSize;

	if (!(startSample | nSamples))
		return size * nFrames;
	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
	case SCE_AUDIODEC_TYPE_AAC:
		size = nSamples * (2 * work->info.publicInfo.at9.ch);
		break;
	case SCE_AUDIODEC_TYPE_MP3:
		size = nSamples * (2 * work->info.publicInfo.mp3.ch);
		break;
	case SCE_AUDIODEC_TYPE_CELP:
	case AUDIODEC_TYPE_SILK:
		size = 2 * nSamples;
		break;
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		if (work->info.aacMultichannel.downmix)
			size = 4 * nSamples;
		else
			size = nSamples * (2 * work->info.aacMultichannel.aac.ch);
		break;
	default:
		size = 0;
		break;
	}
	return size;
}

int atrac9ValidateConfig(const SceAudiodecCtrl *ctrl)
{
	const SceUInt8 (*config)[4] = atrac9Configs;
	const SceUInt8 *input = ctrl->pInfo->at9.configData;

	for (; config != atrac9Configs + sizeof(atrac9Configs) / sizeof(atrac9Configs[0]); config++) {
		if ((*config)[0] == input[0] && input[1] == (*config)[1] &&
		    input[2] == (*config)[2] && input[3] == (*config)[3])
			return 0;
	}
	return SCE_AUDIODEC_AT9_ERROR_INVALID_CONFIG;
}

int atrac9GetChannelCount(const SceUInt8 configData[4])
{
	SceUInt32 channelConfig = (configData[1] >> 1) & 7;
	int channels = SCE_AUDIODEC_AT9_ERROR_INVALID_CONFIG;

	if (!channelConfig)
		return 1;
	if (channelConfig <= 2)
		channels = 2;
	return channels;
}

int atrac9GetSamplesPerFrame(const SceUInt8 configData[4])
{
	SceUInt32 sampleRate = 1U << ((configData[1] >> 4) & 7);
	int samples = SCE_AUDIODEC_AT9_ERROR_INVALID_CONFIG;

	if (sampleRate & 3)
		return 64;
	if (sampleRate & 0xe0)
		return 256;
	if (sampleRate & 0x1c)
		samples = 128;
	return samples;
}

int atrac9GetMaxEsSize(const SceUInt8 configData[4])
{
	SceUInt32 frames = 1U << ((configData[3] >> 3) & 3);
	SceUInt32 frameSize = ((SceUInt32)configData[3] << 8) |
			     ((SceUInt32)configData[2] << 16);

	return frames + (frameSize >> 13) * frames;
}

void audiodecFinishCommand(struct VeneziaRpcThunk *thunk)
{
	SceCodecEngineWrapperForDriver_07C53B4E(thunk);
	SceCodecEngineWrapperForDriver_D1F94D9D(thunk);
	SceCodecEngineWrapperForDriver_1C2BC9F7(thunk);
}

int audiodecFinishRpcMemory(struct VeneziaRpcThunk *thunk)
{
	SceCodecEngineWrapperForDriver_07C53B4E(thunk);
	SceCodecEngineWrapperForDriver_D1F94D9D(thunk);
	return SceCodecEngineWrapperForDriver_1C2BC9F7(thunk);
}

void audiodecFinishDecodeCommand(VeneziaRpcThunk *thunk)
{
	SceUInt32 nStreams = thunk->memories[0].size / sizeof(AudiodecCommand);
	AudiodecCommand *commands = thunk->memories[0].virtualBase;
	AudiodecCommand *command;
	SceUInt32 transferIndex;

	SceCodecEngineWrapperForDriver_07C53B4E(thunk);
	SceCodecEngineWrapperForDriver_D1F94D9D(thunk);
	SceCodecEngineWrapperForDriver_1C2BC9F7(thunk);
	if (!nStreams)
		return;
	command = commands;
	transferIndex = 1;
	do {
		SceUInt32 next = transferIndex + 1;

		command->pEs = thunk->memories[transferIndex].virtualBase;
		if (command->outputPcmSize) {
			command->pPcm = thunk->memories[transferIndex + 1].virtualBase;
			next = transferIndex + 2;
		}
		command++;
		transferIndex = next;
	} while (command != commands + nStreams);
}

int audiodecPackCommand(VeneziaRpcThunk *thunk, const unsigned int *input,
		       unsigned int *packed)
{
	VeneziaMemoryOption option;
	int ret;

	option.flags = 1;
	option.allocationSize = sizeof(AudiodecCommand);
	SceCodecEngineWrapperForDriver_47AED6FF(thunk, (void *)input[0],
					      sizeof(AudiodecCommand), &option);
	SceCodecEngineWrapperForDriver_03EA6C2A(thunk);
	ret = SceCodecEngineWrapperForDriver_D1F94D9D(thunk);
	packed[0] = thunk->memories[0].physicalBase;
	packed[1] = input[1];
	packed[2] = input[2];
	packed[3] = input[3];
	return ret;
}

int audiodecPackRpcMemory(VeneziaRpcThunk *thunk, const unsigned int *input,
			 unsigned int *packed)
{
	VeneziaMemoryOption option;
	int ret;

	option.flags = 1;
	option.allocationSize = 192;
	SceCodecEngineWrapperForDriver_47AED6FF(thunk, (void *)input[0],
					      192, &option);
	SceCodecEngineWrapperForDriver_03EA6C2A(thunk);
	ret = SceCodecEngineWrapperForDriver_D1F94D9D(thunk);
	packed[0] = thunk->memories[0].physicalBase;
	packed[1] = input[1];
	packed[2] = input[2];
	packed[3] = input[3];
	return ret;
}

int audiodecPackDecodeCommand(VeneziaRpcThunk *thunk, const unsigned int *input,
			     unsigned int *packed)
{
	SceUInt32 nStreams = input[3];
	AudiodecCommand *commands = (AudiodecCommand *)input[0];
	AudiodecCommand *command;
	VeneziaMemoryOption option;
	SceUInt32 i, codecType, allocationSize, pcmFlags, channels;
	SceUInt32 transferIndex, next;

	option.flags = nStreams > 1 ? 3 : 1;
	option.allocationSize = nStreams * sizeof(AudiodecCommand);
	SceCodecEngineWrapperForDriver_47AED6FF(thunk, commands,
					      option.allocationSize, &option);
	if (nStreams) {
		command = commands;
		i = 0;
		do {
			codecType = ((SceUInt32)command->handle >> 16) & 0x3fff;
			__builtin_prefetch(command + 3);
			switch (codecType) {
			case SCE_AUDIODEC_TYPE_AAC:
				pcmFlags = nStreams > 1 ? 39 : 37;
				channels = command->info.publicInfo.aac.isSbr ?
					(command->info.publicInfo.aac.ch == 1 ? 2 : 1) : 1;
				allocationSize = channels * 2 * command->outputPcmSize;
				break;
			case AUDIODEC_TYPE_AAC_MULTICHANNEL:
				pcmFlags = nStreams > 1 ? 39 : 37;
				allocationSize = 49152;
				break;
			case SCE_AUDIODEC_TYPE_MP3:
				pcmFlags = nStreams > 1 ? 7 : 5;
				channels = command->info.publicInfo.mp3.ch == 1 ? 2 : 1;
				allocationSize = command->outputPcmSize * channels;
				break;
			default:
				allocationSize = command->outputPcmSize;
				pcmFlags = nStreams > 1 ? 7 : 5;
				break;
			}
			option.flags = 20;
			option.allocationSize = 0;
			SceCodecEngineWrapperForDriver_47AED6FF(thunk, command->pEs,
							      command->inputEsSize, &option);
			if (command->outputPcmSize) {
				option.flags = pcmFlags;
				option.allocationSize = allocationSize;
				SceCodecEngineWrapperForDriver_47AED6FF(thunk, command->pPcm,
								      command->outputPcmSize, &option);
				command->outputPcmSize = allocationSize;
			}
			command++;
			i++;
		} while (i != nStreams);
	}
	SceCodecEngineWrapperForDriver_03EA6C2A(thunk);
	if (nStreams) {
		command = commands;
		transferIndex = 1;
		do {
			next = transferIndex + 1;
			command->pEs = (void *)thunk->memories[transferIndex].physicalBase;
			if (command->outputPcmSize) {
				command->pPcm = (void *)thunk->memories[transferIndex + 1].physicalBase;
				next = transferIndex + 2;
			}
			command++;
			transferIndex = next;
		} while (command != commands + nStreams);
	}
	SceCodecEngineWrapperForDriver_D1F94D9D(thunk);
	packed[0] = thunk->memories[0].physicalBase;
	packed[1] = input[1];
	packed[2] = input[2];
	packed[3] = input[3];
	return (int)packed;
}

int audiodecQueryLibraryContext(AudiodecCommand *command)
{
	unsigned int input[5];
	unsigned int operation;
	SceUInt32 codecType = ((SceUInt32)command->handle >> 16) & 0x3fff;

	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
		operation = 128;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
		operation = 176;
		break;
	case SCE_AUDIODEC_TYPE_AAC:
		operation = 192;
		input[1] = 0;
		break;
	case SCE_AUDIODEC_TYPE_CELP:
		operation = 256;
		break;
	case AUDIODEC_TYPE_SILK:
		operation = 272;
		break;
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		operation = 192;
		input[1] = 1;
		break;
	default:
		return SCE_AUDIODEC_ERROR_INVALID_TYPE;
	}
	input[0] = (unsigned int)command;
	return SceCodecEngineWrapperForDriver_391BD4FD(operation, input,
						     audiodecPackCommand, audiodecFinishCommand);
}

int audiodecCreateCommand(AudiodecCommand *command)
{
	unsigned int input[5];
	unsigned int operation;
	SceUInt32 codecType = ((SceUInt32)command->handle >> 16) & 0x3fff;

	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
		operation = 129;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
		operation = 177;
		break;
	case SCE_AUDIODEC_TYPE_AAC:
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		operation = 193;
		break;
	case SCE_AUDIODEC_TYPE_CELP:
		operation = 257;
		break;
	case AUDIODEC_TYPE_SILK:
		operation = 273;
		break;
	default:
		return SCE_AUDIODEC_ERROR_INVALID_TYPE;
	}
	input[0] = (unsigned int)command;
	input[1] = SceCodecEngineWrapperForDriver_6A1184EA(0);
	return SceCodecEngineWrapperForDriver_391BD4FD(operation, input,
						     audiodecPackCommand, audiodecFinishCommand);
}

int audiodecDeleteCommand(AudiodecCommand *command)
{
	unsigned int input[5];
	unsigned int operation;
	SceUInt32 codecType = ((SceUInt32)command->handle >> 16) & 0x3fff;

	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
		operation = 130;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
		operation = 178;
		break;
	case SCE_AUDIODEC_TYPE_AAC:
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		operation = 194;
		break;
	case SCE_AUDIODEC_TYPE_CELP:
		operation = 258;
		break;
	case AUDIODEC_TYPE_SILK:
		operation = 274;
		break;
	default:
		return SCE_AUDIODEC_ERROR_INVALID_TYPE;
	}
	input[0] = (unsigned int)command;
	return SceCodecEngineWrapperForDriver_391BD4FD(operation, input,
						     audiodecPackCommand, audiodecFinishCommand);
}

int audiodecDecodeCommand(AudiodecCommand *command, SceUInt32 nFrames,
			 SceUInt32 startSample, SceUInt32 nSamples)
{
	unsigned int input[5];
	unsigned int operation;
	SceUInt32 codecType = ((SceUInt32)command->handle >> 16) & 0x3fff;

	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
		if (nFrames > 1)
			operation = 135;
		else if (startSample | nSamples)
			operation = 137;
		else
			operation = 132;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
		operation = 179;
		break;
	case SCE_AUDIODEC_TYPE_AAC:
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		operation = 195;
		break;
	case SCE_AUDIODEC_TYPE_CELP:
		operation = 259;
		break;
	case AUDIODEC_TYPE_SILK:
		operation = 275;
		break;
	default:
		return SCE_AUDIODEC_ERROR_INVALID_TYPE;
	}
	input[0] = (unsigned int)command;
	if (startSample | nSamples) {
		input[1] = startSample;
		input[2] = nSamples;
	} else {
		input[1] = nFrames;
	}
	input[3] = 1;
	return SceCodecEngineWrapperForDriver_391BD4FD(operation, input,
						     audiodecPackDecodeCommand,
						     audiodecFinishDecodeCommand);
}

int audiodecDecodeNStreamsCommand(AudiodecCommand *commands, SceUInt32 nStreams)
{
	unsigned int input[5];
	unsigned int operation;
	SceUInt32 codecType = ((SceUInt32)commands->handle >> 16) & 0x3fff;

	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
		operation = 136;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
		operation = 180;
		break;
	case SCE_AUDIODEC_TYPE_AAC:
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		operation = 196;
		break;
	case SCE_AUDIODEC_TYPE_CELP:
		operation = 260;
		break;
	case AUDIODEC_TYPE_SILK:
		operation = 276;
		break;
	default:
		return SCE_AUDIODEC_ERROR_INVALID_TYPE;
	}
	input[0] = (unsigned int)commands;
	input[1] = nStreams;
	input[3] = nStreams;
	return SceCodecEngineWrapperForDriver_391BD4FD(operation, input,
						     audiodecPackDecodeCommand,
						     audiodecFinishDecodeCommand);
}

int audiodecClearContextCommand(AudiodecCommand *command)
{
	unsigned int input[5];
	unsigned int operation;
	SceUInt32 codecType = ((SceUInt32)command->handle >> 16) & 0x3fff;

	switch (codecType) {
	case SCE_AUDIODEC_TYPE_AT9:
		operation = 134;
		break;
	case SCE_AUDIODEC_TYPE_MP3:
		operation = 181;
		break;
	case SCE_AUDIODEC_TYPE_AAC:
	case AUDIODEC_TYPE_AAC_MULTICHANNEL:
		operation = 197;
		break;
	case SCE_AUDIODEC_TYPE_CELP:
		operation = 261;
		break;
	case AUDIODEC_TYPE_SILK:
		operation = 277;
		break;
	default:
		return SCE_AUDIODEC_ERROR_INVALID_TYPE;
	}
	input[0] = (unsigned int)command;
	return SceCodecEngineWrapperForDriver_391BD4FD(operation, input,
						     audiodecPackCommand, audiodecFinishCommand);
}

__attribute__((noinline)) int audiodecIsExternalHandle(SceUInt32 handle)
{
	return handle >> 31;
}

__attribute__((noinline)) int audiodecIsResidentHandle(SceUInt32 handle)
{
	return (handle >> 30) & 1;
}

__attribute__((noinline)) SceUInt32 audiodecGetHandleCodecType(SceUInt32 handle)
{
	return (handle >> 16) & 0x3fff;
}

__attribute__((noinline)) SceUInt32 audiodecGetHandleIndex(SceUInt32 handle)
{
	return handle & 0xffff;
}

__attribute__((noinline)) SceInt32 audiodecMakeHandle(int external, int resident,
						   SceUInt32 codecType, SceUInt32 index)
{
	return ((SceUInt32)external << 31) | ((SceUInt32)(resident & 1) << 30) |
	       ((codecType & 0x3fff) << 16) | (index & 0xffff);
}
