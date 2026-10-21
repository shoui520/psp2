/* SceNgs systems: requests applied at an update, player buffers, sizing. */

#include <scetypes.h>
#include <string.h>
#include <kernel/error.h>
#include <sysmem/sysroot.h>
#include <vnz_wrapper/vnz_wrapper.h>
#include <intrmgr/syscall.h>
#include <sysmem/sysmem.h>
#include <ngs/error.h>
#include "ngs_module_memory.h"
#include "ngs_system.h"
#include "ngs_platform.h"
#include "ngs_rack.h"
#include "ngs_player.h"

/* Marks the voices whose patches or parameters the caller changed. */
void ngsApplyRackUpdateRequests(unsigned int *rackCount, NgsSynRack ***racks)
{
	NgsSynRack **list = *racks;
	NgsSynRack *rack;
	NgsVoice *voice;
	unsigned int *requested, *patchMasks, *paramsMasks;
	unsigned int i, j, voiceCount;

	for (i = 0; i < *rackCount; i++) {
		rack = list[i];
		if (rack == NULL || rack->state != NGS_RACK_STATE_ACTIVE)
			continue;
		requested = rack->updateRequested;
		if (*requested == 0)
			continue;
		voiceCount = rack->voiceCount;
		patchMasks = rack->patchUpdateMasks;
		paramsMasks = rack->paramsUpdateMasks;
		for (j = 0; j < voiceCount; j++) {
			if (patchMasks[j] != 0) {
				voice = &rack->voices[j];
				voice->patchDirtyMask |= patchMasks[j];
				voice->dirtyFlags |= 1;
				voice->dirtyFlags2 |= 1;
			}
			if (paramsMasks[j] != 0) {
				voice = &rack->voices[j];
				voice->paramsDirtyMask |= paramsMasks[j];
				voice->dirtyFlags |= 4;
				voice->dirtyFlags2 |= 4;
			}
		}
		*requested = 0;
	}
}

/* Locks the memory block holding a player buffer, once per block. */
static inline int ngsLockPlayerBuffer(NgsSynSystem *system, const void *buffer,
				      SceInt32 size)
{
	const unsigned char *start = buffer;
	const unsigned char *end = start + size;
	NgsLockedMemoryRange *range;
	MemBlockPhyInfo info;
	unsigned int i;
	SceUID uid;
	int result;

	if (start >= end)
		return system->compiledSdkVersion < 0x1800000 ?
			SCE_NGS_ERROR : SCE_NGS_ERROR_INVALID_BUFFER;
	for (i = 0; i < system->lockedRangeCount; i++) {
		range = &system->lockedRanges[i];
		if (start >= (unsigned char *)range->address &&
		    end <= (unsigned char *)range->address + range->size)
			return 0;
	}
	if (i == 1024)
		return SCE_NGS_ERROR_OUT_OF_ASSETS;
	uid = sceKernelFindMemBlockByAddr((void *)start, 0);
	if (uid < 0)
		goto invalid;
	info.ex.size = sizeof(info);
	if (SceSysmemForDriver_24A99FFF(uid, &info) < 0)
		goto invalid;
	range = &system->lockedRanges[system->lockedRangeCount];
	range->address = (void *)info.ex.details.vbase;
	range->size = info.ex.details.size;
	if (info.ex.details.size < (uintptr_t)buffer + size -
	    info.ex.details.vbase)
		return SCE_NGS_ERROR_INVALID_BUFFER;
	result = SceSysmemForDriver_59A4402F((void *)info.ex.details.vbase,
					     info.ex.details.size);
	if (result != 0)
		return result;
	result = sceKernelIsAccessibleRange(0x10, buffer, size);
	if (result != 0) {
		sceKernelUnlockRange((void *)info.ex.details.vbase,
				     info.ex.details.size);
		return result;
	}
	system->lockedRangeCount++;
	return 0;
invalid:
	if (system->compiledSdkVersion < 0x1800000)
		return SCE_NGS_ERROR;
	return SCE_NGS_ERROR_INVALID_BUFFER;
}

int ngsLockAt9PlayerBuffers(NgsSynSystem *system, const SceNgsAT9Params *params)
{
	const SceNgsAT9BufferParams *buffer;
	int i, result;

	for (i = 0; i < SCE_NGS_AT9_MAX_BUFFERS; i++) {
		buffer = &params->buffs[i];
		if (buffer->pBuffer == NULL || buffer->nNumBytes <= 0)
			continue;
		result = ngsLockPlayerBuffer(system, buffer->pBuffer,
					     buffer->nNumBytes);
		if (result != 0)
			return result;
	}
	return 0;
}

int ngsLockPcmPlayerBuffers(NgsSynSystem *system,
			    const SceNgsPlayerParams *params)
{
	const SceNgsPlayerBufferParams *buffer;
	int i, result;

	for (i = 0; i < SCE_NGS_PLAYER_MAX_BUFFERS; i++) {
		buffer = &params->buffs[i];
		if (buffer->pBuffer == NULL || buffer->nNumBytes <= 0)
			continue;
		result = ngsLockPlayerBuffer(system, buffer->pBuffer,
					     buffer->nNumBytes);
		if (result != 0)
			return result;
	}
	return 0;
}

unsigned int ngsGetNextRackId(NgsSynSystem *system)
{
	return system->nextRackId++;
}

int sceNgsSystemGetRequiredMemorySizeInternal(
	const SceNgsSystemInitParams *params, SceUInt32 *size)
{
	SceNgsSystemInitParams init;
	SceSize alignedSize = 0;
	SceSize extraSize;
	SceSize systemSize;
	SceUInt32 syscallState;
	int copyUserData;
	int voiceListSize;
	int rackListSize;
	int unusedSize;
	int nMaxRacks;
	int nMaxVoices;
	int result;

	syscallState = sceSyscallEnter();
	copyUserData = ngsGetSystemInstanceCount();
	if (size == NULL || params == NULL ||
	    ngsCopyFromCaller(&init, params, sizeof(init), copyUserData) != 0 ||
	    ngsIsCallerRangeAccessible(size, sizeof(*size), copyUserData) == 0 ||
	    (unsigned int)(init.nGranularity - 64) > 448 ||
	    ((unsigned int)init.nGranularity << 26) != 0 ||
	    init.nSampleRate != 48000 || init.nMaxModules < 0 ||
	    (unsigned int)((nMaxVoices = init.nMaxVoices) - 1) > 255 ||
	    (unsigned int)((nMaxRacks = init.nMaxRacks) - 1) > 63) {
		result = (int)0x804a0002;
	} else {
		result = ngsGetSystemExtraMemorySize(&extraSize);
		if (result == 0) {
			systemSize = ngsAlignMemorySize(extraSize + 8632 +
				8 * nMaxRacks + 4 * nMaxRacks + 4 * nMaxVoices);
			voiceListSize = ngsAlignSize(
				12 * (init.nMaxRacks + init.nMaxVoices + 33), 256);
			rackListSize = ngsAlignSize(32 * init.nMaxRacks, 256);
			ngsAlignMemorySize(rackListSize + voiceListSize + 256);
			unusedSize = ngsAlignMemorySize(0);
			result = ngsGetAlignedMemorySize(28 * init.nMaxRacks +
				288 + 8 * (2 * init.nMaxRacks + 1 +
				2 * init.nMaxVoices + init.nMaxVoices),
				unusedSize, 0, systemSize, &alignedSize);
			if (result == 0)
				result = ngsCopyToCaller(size, &alignedSize,
					sizeof(*size), copyUserData);
		}
	}
	sceSyscallLeave(syscallState);
	return result;
}

/* The rack entry the batch queues voices on after the rack at position: the
 * nearest later rack that one of the racks from position on patches into. */
static inline NgsDspRackEntry *ngsGetNextRackEntry(NgsSynSystem *system,
						   unsigned int position)
{
	unsigned int count = system->MaxRacks;
	unsigned int next = count;
	NgsDspRackEntry *entry;
	NgsRackLink *links;
	unsigned int index, other;

	for (index = position; index < count &&
	     system->processingRacks[index] != NULL; index++) {
		links = system->processingRacks[index]->links;
		for (other = 0; other < count; other++) {
			if (links[other].outputPatches > 0 &&
			    links[other].rack->processingIndex > position &&
			    links[other].rack->processingIndex < next)
				next = links[other].rack->processingIndex;
		}
	}
	if (next != count) {
		entry = system->processingRacks[next]->dspRackList;
		entry->processingIndex = next;
	} else {
		entry = system->emptyGraph;
	}
	entry->queuedCount = 0;
	entry->voiceCount = 0;
	return entry;
}

/* Queues a callback for the caller (sceNgsSystemGetCallbackList). */
static inline void ngsAddCallback(NgsSynSystem *system, void *callback,
				  SceNgsCallbackInfo *info,
				  SceNgsCallbackInfo *callerInfo)
{
	system->callbacks[system->callbackCount].callback = callback;
	system->callbacks[system->callbackCount].info = callerInfo;
	system->callbackCount++;
	ngsCopyToCaller(callerInfo, info, sizeof(*info), system->copyUserData);
}

/* The caller's address of a rack's callback information. */
static inline SceNgsCallbackInfo *ngsGetRackCallerInfo(NgsSynRack *rack,
						       SceNgsCallbackInfo *info)
{
	return (SceNgsCallbackInfo *)((char *)info +
		((char *)rack->callerBuffer - (char *)rack->callbackMemory));
}

/* The caller's address of a system's callback information. */
static inline SceNgsCallbackInfo *ngsGetSystemCallerInfo(NgsSynSystem *system,
							 SceNgsCallbackInfo *info)
{
	return (SceNgsCallbackInfo *)((char *)info +
		((char *)system->callbackUserMemory -
		 (char *)system->callbackKernelMemory));
}

/* Refreshes a voice's player buffers while the caller's pull was deferred. */
static inline int ngsRefreshVoicePlayers(NgsSynSystem *system, NgsVoice *voice,
					 NgsVoiceDsp *dsp)
{
	NgsSynRack *rack = voice->rack;
	unsigned char params[sizeof(SceNgsAT9Params)];
	unsigned int index, module;
	unsigned char *memory;
	int result;

	if (rack->playerModuleCount != 0) {
		if (dsp->voice != voice)
			dsp->voice = voice;
		for (index = 0; index < rack->playerModuleCount; index++) {
			module = rack->playerModules[index];
			memory = dsp->moduleMemory;
			if (((voice->moduleUpdateMask >> module) & 1) == 0)
				continue;
			memcpy(params, dsp->params + rack->paramsOffsets[module],
			       sizeof(SceNgsPlayerParams));
			result = ngsLockPcmPlayerBuffers(system,
				(const SceNgsPlayerParams *)params);
			if (result != 0)
				return result;
			ngsPreparePcmPlayerBuffers((NgsPcmPlayerContext *)(memory +
				rack->playerModuleOffsets[module]),
				(const SceNgsPlayerParams *)params, 0, 0,
				rack->system->Granularity, 0,
				rack->system->compiledSdkVersion);
		}
	}
	if (rack->at9ModuleCount != 0) {
		for (index = 0; index < voice->rack->at9ModuleCount; index++) {
			module = rack->at9Modules[index];
			memory = dsp->moduleMemory;
			if (((voice->moduleUpdateMask >> module) & 1) == 0)
				continue;
			memcpy(params, dsp->params + rack->paramsOffsets[module],
			       sizeof(SceNgsAT9Params));
			result = ngsLockAt9PlayerBuffers(system,
				(const SceNgsAT9Params *)params);
			if (result != 0)
				return result;
			ngsPrepareAt9PlayerBuffers((NgsAt9PlayerContext *)(memory +
				rack->at9ModuleOffsets[module]),
				(const SceNgsAT9Params *)params, 0, 0,
				voice->rack->system->Granularity, 0);
		}
	}
	return 0;
}

/* Hands a voice's changes since the last update to its DSP block. */
static inline int ngsUpdateVoiceDsp(NgsSynRack *rack, NgsVoice *voice,
				    NgsVoiceDsp *dsp)
{
	unsigned char params[sizeof(SceNgsAT9Params)];
	int volumes[4];
	NgsDspPatchBuffer *patchBuffer;
	NgsInputMixerContext *mixer;
	NgsPatch *patch;
	unsigned int dirty, flags, mask, done, updated, channels, count;
	unsigned int index, module, slot;
	unsigned char *memory;
	int fromCaller, restart, result;

	if (rack->system->compiledSdkVersion < 0x2000000) {
		dirty = voice->dirtyFlags;
		if (dirty != 0)
			voice->dirtyFlags = 0;
		flags = voice->flags;
		fromCaller = 0;
	} else {
		dirty = voice->callerDirtyFlags;
		fromCaller = 1;
		flags = voice->callerFlags;
	}
	restart = 0;
	updated = 0;
	if (dirty == 0)
		goto players;

	if (dirty & 8) {
		if (dsp->initId != voice->initId || dsp->voice != voice) {
			dsp->voice = voice;
			dsp->initId = voice->initId;
		}
		if (voice->playState.pauseState == 1)
			flags |= 8;
		if (voice->playState.runState == NGS_VOICE_RUN_STATE_KEY_OFF)
			flags |= 0x10;
		dsp->controlFlags = flags;
		if (((fromCaller ? voice->callerFlags : voice->flags) & 1) != 0) {
			voice->moduleUpdateMask = 0;
			mask = 0;
			for (module = 0; module < rack->moduleCount8; module++)
				mask |= 1 << module;
			voice->moduleUpdateMask = mask;
			updated = mask;
			for (index = 0; index < voice->rack->inputMixerModuleCount;
			     index++) {
				mixer = (NgsInputMixerContext *)(dsp->moduleMemory +
					rack->inputMixerModuleOffsets[index]);
				for (slot = 0; slot < mixer->patchCount; slot++)
					mixer->restart[slot] = 1;
			}
			restart = 1;
		}
		if (fromCaller) {
			if (voice->callerPlayState.pauseState == 1)
				voice->playState.pauseState = 2;
			if (voice->callerFlags != 0) {
				voice->dirtyFlags |= 8;
				voice->dirtyFlags2 |= 8;
			}
		} else {
			if (voice->playState.pauseState == 1)
				voice->playState.pauseState = 2;
			if (voice->flags != 0) {
				voice->flags = 0;
				voice->dirtyFlags |= 8;
				voice->dirtyFlags2 |= 8;
			}
		}
		ngsUpdateVoiceFlags(voice, rack);
	}

	if (dirty & 2) {
		dsp->bypassFlags = voice->bypassFlags;
		ngsAssignModuleBuffers(dsp->inputs, dsp->outputs, dsp,
				       &rack->moduleList);
	}

	if (dirty & 1) {
		if (rack->patchesPerVoice != 0) {
			mask = voice->patchDirtyMask;
			count = rack->patchesPerVoice;
			for (index = 0; index < count; index++) {
				if (((1 << index) & mask) == 0)
					continue;
				patch = &voice->patches[index];
				channels = rack->channelsPerVoice;
				patchBuffer = &dsp->patchBuffers[index];
				if (patch->destination != NULL) {
					patchBuffer->destination =
						patch->mixerContext;
					patchBuffer->destinationPhysical =
						patch->mixerContextPhysical;
					volumes[0] = (int)(patch->volumes->m[0][0] *
							   16777000.0f);
					volumes[1] = (int)(patch->volumes->m[0][1] *
							   16777000.0f);
					if (channels > 1) {
						volumes[2] = (int)(patch->volumes->m[1][0] *
								   16777000.0f);
						volumes[3] = (int)(patch->volumes->m[1][1] *
								   16777000.0f);
					}
					memcpy(patchBuffer->buffer, volumes,
					       8 * channels);
					mask = voice->patchDirtyMask;
				} else {
					patchBuffer->destination = NULL;
					patchBuffer->destinationPhysical = 0;
				}
				mask &= ~(1 << index);
				voice->patchDirtyMask = mask;
				if (mask == 0)
					break;
			}
		}
		voice->patchDirtyMask = 0;
	}

	if (dirty & 4) {
		mask = voice->paramsDirtyMask;
		memory = dsp->params;
		done = 0;
		count = rack->moduleCount8;
		for (module = 0; module < count; module++) {
			if (((1 << module) & mask) == 0)
				continue;
			if (rack->paramsSizes[module] != 0)
				memcpy(memory + rack->paramsOffsets[module],
				       voice->moduleParams[module].params,
				       rack->paramsSizes[module]);
			done |= 1 << module;
			if (mask == done)
				break;
		}
		voice->paramsDirtyMask = mask & ~done;
		updated |= done;
	}

players:
	for (index = 0; index < rack->playerModuleCount; index++) {
		module = rack->playerModules[index];
		if (((voice->moduleUpdateMask >> module) & 1) == 0)
			continue;
		memory = dsp->moduleMemory;
		memcpy(params, dsp->params + rack->paramsOffsets[module],
		       sizeof(SceNgsPlayerParams));
		result = ngsLockPcmPlayerBuffers(rack->system,
			(const SceNgsPlayerParams *)params);
		if (result != 0)
			return result;
		ngsPreparePcmPlayerBuffers((NgsPcmPlayerContext *)(memory +
			rack->playerModuleOffsets[module]),
			(const SceNgsPlayerParams *)params, 0,
			((1 << module) & updated) != 0,
			rack->system->Granularity, restart,
			rack->system->compiledSdkVersion);
	}
	if (rack->at9ModuleCount != 0) {
		for (index = 0; index < voice->rack->at9ModuleCount; index++) {
			module = rack->at9Modules[index];
			if (((voice->moduleUpdateMask >> module) & 1) == 0)
				continue;
			memory = dsp->moduleMemory;
			memcpy(params, dsp->params + rack->paramsOffsets[module],
			       sizeof(SceNgsAT9Params));
			result = ngsLockAt9PlayerBuffers(rack->system,
				(const SceNgsAT9Params *)params);
			if (result != 0)
				return result;
			ngsPrepareAt9PlayerBuffers((NgsAt9PlayerContext *)(memory +
				rack->at9ModuleOffsets[module]),
				(const SceNgsAT9Params *)params, 0,
				((1 << module) & updated) != 0,
				rack->system->Granularity, restart);
		}
	}
	dsp->updatedParams = updated;
	return 0;
}

/* Hands the codec engine's report on a voice to the caller's callbacks. */
static inline void ngsReportVoice(NgsSynSystem *system, NgsSynRack *rack,
				  NgsVoice *voice)
{
	NgsVoiceDsp *dsp = rack->voiceDsps[voice->index];
	NgsVoiceDspStatus *status;
	NgsVoiceDspCallback *record;
	NgsVoiceModuleParams *params;
	SceNgsParamsErrorCallbackFunc errorCallback;
	SceNgsCallbackInfo *info, *callerInfo;
	NgsSynRack *owner;
	unsigned int event, index;

	if (voice->initId != dsp->initId)
		return;
	status = dsp->status;
	voice->moduleUpdateMask = status->moduleUpdateMask;
	event = status->event;
	if (event == 128)
		return;
	dsp->controlFlags = event;
	if (event == 32) {
		if (voice->finishedCallback != NULL &&
		    system->callbackCount < system->callbackCapacity) {
			info = &rack->voiceCallbackInfos[voice->index];
			ngsAddCallback(system, (void *)voice->finishedCallback,
				       info, ngsGetRackCallerInfo(rack, info));
		}
		if (voice->playState.runState != NGS_VOICE_RUN_STATE_UNLOADING) {
			voice->playState.runState = NGS_VOICE_RUN_STATE_IDLE;
			voice->callerPlayState.runState = NGS_VOICE_RUN_STATE_IDLE;
			voice->dirtyFlags2 |= 8;
			ngsUpdateVoiceFlags(voice, rack);
		}
	} else if (event == 16 &&
		   voice->playState.runState == NGS_VOICE_RUN_STATE_PLAYING) {
		voice->playState.runState = NGS_VOICE_RUN_STATE_KEY_OFF;
		voice->callerPlayState.runState = NGS_VOICE_RUN_STATE_KEY_OFF;
		voice->dirtyFlags |= 8;
		voice->dirtyFlags2 |= 8;
		ngsUpdateVoiceFlags(voice, rack);
	}

	if (status->paramsError != 0) {
		owner = voice->rack;
		errorCallback = owner->paramErrorCallback;
		if (errorCallback != NULL) {
			info = owner->rackCallbackInfo;
			callerInfo = ngsGetRackCallerInfo(owner, info);
		} else {
			errorCallback = system->paramErrorCallback;
			if (errorCallback == NULL)
				goto callbacks;
			info = system->paramErrorInfo;
			callerInfo = ngsGetSystemCallerInfo(system, info);
		}
		if (info != NULL) {
			index = status->paramsErrorModule;
			info->hVoiceHandle = ngsGetVoiceHandle(voice);
			info->hRackHandle = ngsGetRackHandle(voice->rack);
			info->uModuleID =
				voice->rack->moduleList.modules[index].moduleId;
			info->nCallbackData = 0;
			info->nCallbackData2 = 0;
			info->pCallbackPtr = NULL;
			info->pUserData = NULL;
			if (system->callbackCount < system->callbackCapacity)
				ngsAddCallback(system, (void *)errorCallback,
					       info, callerInfo);
		}
	}

callbacks:
	for (index = 0; index < 2; index++) {
		record = &status->callbacks[index];
		if (record->moduleIndex == NGS_VOICE_DSP_NO_MODULE ||
		    voice->moduleParams[record->moduleIndex].callback == NULL)
			continue;
		owner = voice->rack;
		info = &owner->callbackInfos[index +
			voice->index * owner->dspModuleCount];
		info->hVoiceHandle = ngsGetVoiceHandle(voice);
		info->hRackHandle = ngsGetRackHandle(voice->rack);
		params = &voice->moduleParams[record->moduleIndex];
		info->nCallbackData = record->callbackData;
		info->uModuleID =
			owner->moduleList.modules[record->moduleIndex].moduleId;
		info->nCallbackData2 = record->callbackData2;
		info->pUserData = params->callbackUserData;
		info->pCallbackPtr = record->callbackPtr;
		if (system->callbackCount < system->callbackCapacity &&
		    params->callback != NULL)
			ngsAddCallback(system, (void *)params->callback, info,
				       ngsGetRackCallerInfo(owner, info));
	}
}

SceInt32 sceNgsSystemUpdateInternal(SceNgsHSynSystem handle)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system;
	NgsDspBatch *batch;
	NgsDspBatchEntry *entry, *entries;
	NgsDspBatchRack *batchRack;
	NgsDspRackEntry *node, *previous;
	NgsSynRack *rack;
	NgsVoice *voice;
	NgsVoiceDsp *dsp;
	SceNgsCallbackInfo *info;
	SceNgsRackReleaseCallbackFunc releaseCallback;
	void *systemHeap;
	unsigned int position, index, count, codec, statusCount, voiceCount;
	unsigned int queued, entryCount;
	int command, capacity, rackAdded, dependent;
	int result, lockResult, playerResult;

	result = SceCodecEngineWrapperForDriver_CB8703D8();
	if (result < 0)
		goto leave;
	system = ngsGetSystemFromHandle(handle);
	if (system == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto unlock;
	}
	if (system->state != NGS_SYSTEM_STATE_ACTIVE) {
		result = SCE_NGS_ERROR;
		goto unlock;
	}
	batch = system->dspBatch;
	systemHeap = system->context->systemHeap;
	ngsLockSystemMutex(system->context);
	if (system->compiledSdkVersion < 0x2000000)
		ngsApplyRackUpdateRequests(&system->MaxRacks, &system->racks);
	ngsLockTransfer(system->context);

	for (index = 0; index < system->MaxRacks; index++) {
		if (system->processingRacks[index] != NULL)
			system->processingRacks[index]->processingIndex = index;
	}
	batch->entryCount = 0;
	batch->rackCount = 0;
	capacity = batch->capacity;
	node = ngsGetNextRackEntry(system, 0);

	command = NGS_DSP_BATCH_STATUS_SLOTS;
	statusCount = 0;
	voiceCount = 0;
	playerResult = 0;
	previous = NULL;
	for (position = 0; position < system->MaxRacks; position++) {
		if (node->processingIndex == position) {
			if (node->queuedCount > 0) {
				if (command < capacity) {
					entry = &batch->entries[command];
					entry->type = NGS_DSP_BATCH_RACK;
					entry->heapOffset = ngsGetPhysicalAddress(node);
					entry->size = node->queuedCount;
					node->commandIndex = command++;
				}
				previous = node;
			}
			node = ngsGetNextRackEntry(system, position);
		}
		rack = system->processingRacks[position];
		if (rack == NULL || rack->state != NGS_RACK_STATE_ACTIVE ||
		    (rack->inputMixerModuleCount != 0 &&
		     rack->maxPatchesPerInput == 0))
			continue;
		*rack->voiceListCount = 0;
		result = ngsLockParams(system->context);
		if (result != 0) {
			ngsUnlockTransfer(system->context);
			ngsUnlockSystemMutex(system->context);
			goto unlock;
		}
		rackAdded = 0;
		for (index = 0; index < rack->voiceCount; index++) {
			voice = &rack->voices[index];
			if (voice->callerPlayState.pauseState == 2 ||
			    voice->callerPlayState.runState - 1 > 1)
				continue;
			dsp = rack->voiceDsps[index];
			if (system->pullDataDeferred != 0)
				result = ngsRefreshVoicePlayers(system, voice,
								dsp);
			else
				result = ngsUpdateVoiceDsp(rack, voice, dsp);
			if (result != 0) {
				playerResult = result;
				continue;
			}

			dsp = rack->voiceDsps[index];
			entries = batch->entries;
			if (previous != NULL) {
				if ((int)previous->voiceCount >=
				    (int)previous->voiceCapacity)
					break;
				previous->voices[previous->voiceCount] = command;
				previous->voiceCount++;
				dependent = 1;
			} else {
				dependent = 0;
			}
			dsp->rackEntry = node;
			node->queuedCount++;
			if (command >= capacity)
				break;
			entries[command].type = NGS_DSP_BATCH_VOICE;
			entries[command].heapOffset =
				(unsigned char *)dsp - (unsigned char *)systemHeap;
			entries[command].size = dependent;
			system->updateVoices[voiceCount++] = voice;
			rack->voiceResults[index]++;
			rack->requestsDirty = 1;
			if (rack->codecModuleCount != 0) {
				if (statusCount >= NGS_DSP_BATCH_STATUS_SLOTS) {
					command++;
					break;
				}
				for (codec = 0; ; ) {
					entry = &batch->entries[statusCount++];
					entry->type = NGS_DSP_BATCH_STATUS;
					entry->size = command;
					entry->heapOffset = ngsGetPhysicalAddress(
						&rack->voiceDsps[index]->codecBuffers[codec]);
					batch->entries[command].size++;
					if (++codec >= rack->codecModuleCount)
						break;
					if (statusCount == NGS_DSP_BATCH_STATUS_SLOTS)
						break;
				}
				if (codec < rack->codecModuleCount) {
					command++;
					break;
				}
			}
			command++;
			if (rackAdded)
				continue;
			if (batch->rackCount >= batch->rackCapacity)
				break;
			batchRack = &batch->racks[batch->rackCount];
			batchRack->voiceDspMemory = (uintptr_t)rack->voiceDspMemory;
			batchRack->voiceDspMemorySize = rack->voiceDspMemorySize;
			batchRack->voiceDspMemoryEnd =
				(uintptr_t)rack->voiceDspMemoryEnd;
			batchRack->statusMemorySize = rack->statusMemorySize;
			batchRack->statusMemory = rack->statusMemory;
			batchRack->statusMemorySize2 = rack->statusMemorySize2;
			batchRack->voiceCountPhysical =
				rack->dsp->voiceCountPhysical;
			batch->rackCount++;
			rackAdded = 1;
		}
		ngsUnlockParams(system->context);
	}

	if (command < capacity) {
		entry = &batch->entries[command];
		entry->type = NGS_DSP_BATCH_RACK;
		entry->heapOffset = ngsGetPhysicalAddress(node);
		entry->size = node->queuedCount;
		node->commandIndex = command++;
	}
	batch->entryCount = command;
	for (; statusCount < NGS_DSP_BATCH_STATUS_SLOTS; statusCount++)
		batch->entries[statusCount].type = NGS_DSP_BATCH_UNUSED;
	ngsUnlockTransfer(system->context);
	entries = batch->entries;
	entryCount = batch->entryCount;
	ngsSulphaTraceUpdate(system, entries, entryCount, &system->debugBuffer);
	result = ngsRunDspBatch(system, entries, entryCount);
	if (system->compiledSdkVersion >= 0x1800000 && result != 0) {
		ngsUnlockSystemMutex(system->context);
		goto unlock;
	}

	lockResult = ngsLockUpdate(system->context);
	if (lockResult == 0) {
		for (index = 0; index != voiceCount; index++) {
			voice = system->updateVoices[index];
			if (voice->outputParamsSize != 0)
				memcpy(voice->state, voice->outputParams,
				       voice->outputParamsSize);
		}
		ngsUnlockUpdate(system->context);
		lockResult = ngsLockVoice(system->context);
		if (lockResult == 0) {
			for (position = 0; position < system->MaxRacks;
			     position++) {
				rack = system->racks[position];
				if (rack == NULL || rack->state == 0)
					continue;
				queued = *rack->voiceListCount;
				*rack->voiceListCount = 0;
				for (index = 0; (int)index < (int)queued; index++) {
					voice = (NgsVoice *)rack->voiceList[index];
					if (voice != NULL)
						ngsReportVoice(system, rack, voice);
				}
			}
			lockResult = 0;
			ngsUnlockVoice(system->context);
		}
	}
	ngsSulphaEndUpdate(system, entries, entryCount);
	system->CycleCount++;

	while (system->pendingRackReleases > 0) {
		count = system->MaxRacks;
		/* Sony's waits here for a releasing rack, however long. */
		for (;;) {
			for (index = 0; index < count; index++) {
				rack = system->racks[index];
				if (rack != NULL &&
				    rack->state == NGS_RACK_STATE_RELEASING)
					goto release;
			}
		}
release:
		releaseCallback = rack->releaseCallback;
		info = &system->rackReleaseInfo[index];
		info->pUserData = rack->userReleaseData;
		info->hRackHandle = ngsGetRackHandle(rack);
		if (releaseCallback != NULL) {
			lockResult = ngsLockVoice(system->context);
			if (lockResult == 0) {
				ngsRemoveRack(system, index, rack);
				ngsUnlockVoice(system->context);
			}
			if (system->callbackCount < system->callbackCapacity) {
				info = &system->rackReleaseInfo[index];
				ngsAddCallback(system, (void *)releaseCallback,
					info, ngsGetSystemCallerInfo(system, info));
			}
		}
		system->pendingRackReleases--;
	}
	for (index = 0; index < system->lockedRangeCount; index++)
		sceKernelUnlockRange(system->lockedRanges[index].address,
				     system->lockedRanges[index].size);
	system->lockedRangeCount = 0;
	ngsUnlockSystemMutex(system->context);
	_unlockProcessSuspendCore();
	sceSyscallLeave(syscall);
	return lockResult != 0 ? lockResult : playerResult;

unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsSystemReleaseInternal(SceNgsHSynSystem handle)
{
	unsigned int syscall = sceSyscallEnter();
	NgsProcessStorage *storage = NULL;
	NgsSynSystem *system;
	SceNgsCallbackInfo *info, *callerInfo;
	SceNgsRackReleaseCallbackFunc callback;
	NgsSynRack *rack;
	unsigned int index;
	int result;

	result = SceCodecEngineWrapperForDriver_CB8703D8();
	if (result < 0)
		goto leave;
	if (ngsGetProcessStorage(&storage) != 0 ||
	    ngsLockProcess(storage) != 0) {
		result = SCE_NGS_ERROR;
		goto unlock;
	}
	system = ngsGetSystemFromHandle(handle);
	if (system == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto unlockProcess;
	}
	system->state = NGS_SYSTEM_STATE_RELEASING;
	ngsLockSystemMutex(system->context);
	ngsUnlockSystemMutex(system->context);
	result = ngsCheckContextUnlocked(system->context);
	if (result != 0)
		goto unlockProcess;

	for (index = 0; index < system->MaxRacks; index++) {
		rack = system->racks[index];
		if (rack == NULL)
			continue;
		callback = rack->releaseCallback;
		info = &system->rackReleaseInfo[index];
		info->pUserData = rack->userReleaseData;
		info->hRackHandle = ngsGetRackHandle(system->racks[index]);
		result = ngsReleaseRack(ngsGetRackHandle(system->racks[index]),
			NULL);
		if (result == SCE_KERNEL_ERROR_THREAD_STOPPED ||
		    result == SCE_KERNEL_ERROR_THREAD_SUSPENDED)
			goto unlockProcess;
		if (callback != NULL &&
		    system->callbackCount < system->callbackCapacity) {
			info = &system->rackReleaseInfo[index];
			callerInfo = (SceNgsCallbackInfo *)((char *)info +
				((char *)system->callbackUserMemory -
				 (char *)system->callbackKernelMemory));
			system->callbacks[system->callbackCount].callback =
				(void *)callback;
			system->callbacks[system->callbackCount].info = callerInfo;
			system->callbackCount++;
			ngsCopyToCaller(callerInfo, info, sizeof(*info),
				system->copyUserData);
		}
	}
	system->signature = 0;
	result = ngsReleaseSystem(system);
	system->state = NGS_SYSTEM_STATE_NONE;
	ngsReleaseDebugBuffer(&system->debugBuffer, system->debugMemBlock);
	ngsFreeProcessMemory(handle, system->coherentAllocation,
		system->coherentAllocationSize, system->dspHeapAllocation,
		system->dspHeapAllocationSize, system->systemAllocation,
		system->systemAllocationSize, system->systemMemBlock);
	if (ngsUnlockProcess(storage) != 0 && result == 0)
		result = SCE_NGS_ERROR;
	goto unlock;
unlockProcess:
	ngsUnlockProcess(storage);
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsSystemLockInternal(SceNgsHSynSystem handle)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemFromHandle(handle);
	SceInt32 result;

	if (system == NULL)
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	else if (ngsLockSystem(system->context) != 0)
		result = SCE_NGS_ERROR;
	else
		result = 0;
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsSystemUnlockInternal(SceNgsHSynSystem handle)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemFromHandle(handle);
	SceInt32 result;

	if (system == NULL)
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	else if (ngsUnlockSystem(system->context) != 0)
		result = SCE_NGS_ERROR;
	else
		result = 0;
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsSystemSetParamErrorCallbackInternal(SceNgsHSynSystem handle,
	const SceNgsParamsErrorCallbackFunc callback)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemFromHandle(handle);
	SceInt32 result;

	if (system != NULL) {
		system->paramErrorCallback = callback;
		result = 0;
	} else {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	}
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsSystemGetCallbackListInternal(SceNgsHSynSystem handle,
	NgsCallbackList **array, SceUInt32 *count)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemFromHandle(handle);
	SceInt32 result;

	if (system == NULL || array == NULL) {
		result = system != NULL ? 0 : SCE_NGS_ERROR_INVALID_HANDLE;
		sceSyscallLeave(syscall);
		return result;
	}
	result = system->callbackCount;
	if (result != 0) {
		if (ngsCopyToCaller(system->callerCallbacks, system->callbacks,
			sizeof(NgsCallbackList) * result,
			system->copyUserData) != 0 ||
		    ngsCopyToCaller(array, &system->callerCallbacks,
			sizeof(*array), system->copyUserData) != 0)
			result = 0;
		else
			result = system->callbackCount;
	}
	system->callbackCount = 0;
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsSystemSetFlagsInternal(SceNgsHSynSystem handle,
	const SceUInt32 flags)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemFromHandle(handle);
	SceInt32 result;

	if (system != NULL) {
		system->flags = flags;
		result = 0;
	} else {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	}
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsSulphaSetSynthUpdateCallbackInternal(SceNgsHSynSystem handle,
	void *callback, const SceNgsBufferInfo *info)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemFromHandle(handle);
	SceNgsBufferInfo buffer;
	SceInt32 result = SCE_NGS_ERROR_INVALID_HANDLE;

	if (system != NULL) {
		system->synthUpdateCallback = callback;
		if (info != NULL) {
			result = SCE_NGS_ERROR_INVALID_PARAM;
			if (ngsCopyFromCaller(&buffer, info, sizeof(buffer),
				system->copyUserData) == 0) {
				system->callerDebugBuffer = buffer;
				ngsReleaseDebugBuffer(&system->debugBuffer,
					system->debugMemBlock);
				ngsAllocateDebugBuffer(&system->debugBuffer,
					&system->debugMemBlock, buffer.size);
				result = 0;
			}
		} else {
			system->callerDebugBuffer.data = NULL;
			system->callerDebugBuffer.size = 0;
			ngsReleaseDebugBuffer(&system->debugBuffer,
				system->debugMemBlock);
			result = 0;
		}
	}
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsSulphaGetSynthUpdateCallbackInternal(SceNgsHSynSystem handle,
	void **callback, SceNgsBufferInfo *info)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemFromHandle(handle);
	SceNgsBufferInfo buffer;
	SceInt32 result;

	if (system == NULL || callback == NULL) {
		if (system == NULL) {
			result = SCE_NGS_ERROR_INVALID_HANDLE;
			goto leave;
		}
	} else if (info != NULL) {
		result = SCE_NGS_ERROR;
		if (system->synthUpdateCallback == NULL)
			goto leave;
		buffer = system->callerDebugBuffer;
		if (ngsCopyToCaller(callback, &system->synthUpdateCallback,
			sizeof(*callback), system->copyUserData) == 0 &&
		    ngsCopyToCaller(info, &buffer, sizeof(buffer),
			system->copyUserData) == 0 &&
		    ngsCopyToCaller(buffer.data, system->debugBuffer.data,
			buffer.size, system->copyUserData) == 0) {
			result = system->synthUpdateCallback != NULL ?
				0 : SCE_NGS_ERROR;
			goto leave;
		}
	}
	result = SCE_NGS_ERROR_INVALID_PARAM;
leave:
	sceSyscallLeave(syscall);
	return result;
}

/* Titles released before the fix that rely on it too. */
void ngsCheckFixForBugzilla89940(NgsSynSystem *system)
{
	char titleId[32];

	system->fixForBugzilla89940 = 0;
	if (SceSysrootForKernel_EC3124A3(0, titleId, sizeof(titleId)) < 0)
		return;
	if (strncmp(titleId, "PCSG00140", sizeof(titleId)) == 0 ||
	    strncmp(titleId, "PCSH00044", sizeof(titleId)) == 0 ||
	    strncmp(titleId, "PCSH00073", sizeof(titleId)) == 0 ||
	    strncmp(titleId, "PCSE00300", sizeof(titleId)) == 0 ||
	    strncmp(titleId, "PCSB00471", sizeof(titleId)) == 0 ||
	    strncmp(titleId, "PCSG90080", sizeof(titleId)) == 0 ||
	    strncmp(titleId, "PCSG90123", sizeof(titleId)) == 0 ||
	    strncmp(titleId, "PCSH00046", sizeof(titleId)) == 0 ||
	    strncmp(titleId, "PCSE00375", sizeof(titleId)) == 0 ||
	    strncmp(titleId, "PCSB00493", sizeof(titleId)) == 0)
		system->fixForBugzilla89940 = 1;
}

SceInt32 sceNgsSystemInitInternal(SceNgsBufferInfo *bufferInfo,
	const SceUInt32 compiledSdkVersion,
	const SceNgsSystemInitParams *params, SceNgsHSynSystem *handle)
{
	unsigned int syscall;
	NgsProcessStorage *storage;
	SceNgsBufferInfo buffer;
	SceNgsSystemInitParams init;
	unsigned char header[260];
	NgsSynSystem *system;
	NgsDspBatch *batch;
	NgsDspRackEntry *emptyGraph;
	unsigned char *callbackCursor, *cursor, *batchCursor;
	void *callbackMemory, *coherentMemory, *dspHeapMemory, *headerMemory;
	unsigned int systemHandle, index, callbackCount, entryCapacity;
	SceSize alignedSize, extraSize, systemSize, coherentSize, callbackSize;
	SceSize batchSize, graphSize, used;
	SceUID memBlock;
	int result, status;

	alignedSize = 0;
	storage = NULL;
	systemHandle = 0;
	syscall = sceSyscallEnter();
	status = SceCodecEngineWrapperForDriver_CB8703D8();
	result = status;
	if (status < 0)
		goto leave;
	if (params == NULL || handle == NULL ||
	    (status = ngsGetProcessStorage(&storage)) != 0 ||
	    (status = ngsCopyFromCaller(&buffer, bufferInfo, sizeof(buffer),
					storage->copyUserData)) != 0 ||
	    buffer.data == NULL) {
		result = SCE_NGS_ERROR_INVALID_PARAM;
		goto unlock;
	}
	status = ngsLockProcess(storage);
	result = status;
	if (status != 0)
		goto unlock;

	if (ngsIsCallerRangeAccessible(buffer.data, buffer.size,
				       storage->copyUserData) == 0 ||
	    ngsCopyFromCaller(&init, params, sizeof(init),
			      storage->copyUserData) != 0 ||
	    ngsCopyToCaller(handle, &systemHandle, sizeof(*handle),
			    storage->copyUserData) != 0 ||
	    (unsigned int)(init.nGranularity - 64) > 448 ||
	    ((unsigned int)init.nGranularity << 26) != 0 ||
	    init.nSampleRate != 48000 || init.nMaxModules < 0 ||
	    (unsigned int)(init.nMaxVoices - 1) > 255 ||
	    (unsigned int)(init.nMaxRacks - 1) > 63) {
		result = SCE_NGS_ERROR_INVALID_PARAM;
		goto unlockProcess;
	}
	result = ngsGetSystemExtraMemorySize(&extraSize);
	if (result != 0)
		goto unlockProcess;
	systemSize = ngsAlignMemorySize(extraSize + 8632 +
		8 * init.nMaxRacks + 4 * init.nMaxRacks +
		4 * init.nMaxVoices);
	ngsAlignMemorySize(ngsAlignSize(32 * init.nMaxRacks, 256) +
		ngsAlignSize(12 * (init.nMaxRacks + init.nMaxVoices + 33), 256) +
		256);
	coherentSize = ngsAlignMemorySize(0);
	callbackSize = 28 * init.nMaxRacks + 288 + 8 * (init.nMaxVoices +
		2 * init.nMaxRacks + 1 + 2 * init.nMaxVoices);
	result = ngsGetAlignedMemorySize(callbackSize, coherentSize, 0,
					 systemSize, &alignedSize);
	if (result != 0)
		goto unlockProcess;
	if (buffer.size < alignedSize) {
		result = SCE_NGS_ERROR_SIZE_MISMATCH;
		goto unlockProcess;
	}
	if (((uintptr_t)buffer.data << 28) != 0) {
		result = SCE_NGS_ERROR_INVALID_ALIGNMENT;
		goto unlockProcess;
	}
	result = ngsAllocateProcessMemory(0, coherentSize, callbackSize, 0,
		systemSize, buffer.data, buffer.size, &callbackMemory,
		&coherentMemory, &dspHeapMemory, (void **)&cursor, &memBlock);
	if (result != 0) {
		if (result == SCE_NGS_ERROR_INTERNAL_ALLOC &&
		    compiledSdkVersion < 0x1800000)
			result = SCE_NGS_ERROR_OUT_OF_ASSETS;
		goto unlockProcess;
	}

	system = (NgsSynSystem *)cursor;
	system->state = NGS_SYSTEM_STATE_NONE;
	system->dspHeapAllocationSize = 0;
	system->lockedRangeCount = 0;
	system->systemAllocation = system;
	system->coherentAllocation = coherentMemory;
	cursor = (unsigned char *)(system + 1);
	system->dspHeapAllocation = dspHeapMemory;
	system->coherentAllocationSize = coherentSize;
	system->systemAllocationSize = systemSize;
	system->systemMemBlock = memBlock;
	system->pullDataDeferred = 0;
	system->compiledSdkVersion = compiledSdkVersion;
	system->callbackUserMemory = buffer.data;
	system->callbackMemorySize = callbackSize;
	callbackCursor = callbackMemory;
	system->callbackKernelMemory = callbackMemory;
	headerMemory = ngsAllocateMemory(&callbackCursor, sizeof(header));
	memset(header, 0, sizeof(header));
	memcpy(headerMemory, header, sizeof(header));
	system->rackReleaseInfo = ngsAllocateMemory(&callbackCursor,
		sizeof(SceNgsCallbackInfo) * init.nMaxRacks);
	system->paramErrorInfo = ngsAllocateMemory(&callbackCursor,
		sizeof(SceNgsCallbackInfo));
	callbackCount = 2 * init.nMaxRacks + 1 + 2 * init.nMaxVoices +
		init.nMaxVoices;
	system->callbacks = ngsAllocateMemory(&callbackCursor,
		sizeof(NgsCallbackList) * callbackCount);
	system->callbackCapacity = callbackCount;
	system->Granularity = init.nGranularity;
	system->callerCallbacks = (NgsCallbackList *)((char *)system->callbacks +
		((char *)system->callbackUserMemory -
		 (char *)system->callbackKernelMemory));
	system->callbackCount = 0;
	system->SampleRate = init.nSampleRate;
	system->MaxRacks = init.nMaxRacks;
	system->MaxVoices = init.nMaxVoices;
	system->copyUserData = ngsGetSystemInstanceCount();
	system->debugBuffer.data = NULL;
	system->debugBuffer.size = 0;
	system->callerDebugBuffer.data = NULL;
	system->callerDebugBuffer.size = 0;
	system->racks = (NgsSynRack **)cursor;
	system->processingRacks = (NgsSynRack **)(cursor +
		4 * init.nMaxRacks);
	system->nextRackId = 0;
	system->paramErrorCallback = NULL;
	system->pendingRackReleases = 0;
	system->synthUpdateCallback = NULL;
	system->CycleCount = 0;
	system->flags = 1;
	system->rackDepths = (int *)(cursor + 8 * init.nMaxRacks +
		4 * init.nMaxVoices);
	system->updateVoices = (NgsVoice **)(cursor + 8 * init.nMaxRacks);
	cursor += 12 * init.nMaxRacks + 4 * init.nMaxVoices;
	for (index = 0; index < system->MaxRacks; index++) {
		system->racks[index] = NULL;
		system->processingRacks[index] = NULL;
		system->rackDepths[index] = 0;
	}
	system->ModulesLinked = 0;
	system->MaxModules = 32;
	system->modules = (NgsModuleDefinition **)cursor;
	cursor += 32 * sizeof(NgsModuleDefinition *);
	ngsCheckFixForBugzilla89940(system);
	system->context = ngsAllocateMemory(&cursor,
					    sizeof(NgsPlatformContext));
	result = ngsCreateSystem(system);
	if (result != 0) {
		ngsFreeProcessMemory(ngsGetSystemHandle(system), coherentMemory,
			coherentSize, dspHeapMemory, 0, system, systemSize,
			memBlock);
		goto unlockProcess;
	}

	cursor = (unsigned char *)ngsAlignMemorySize((SceSize)cursor);
	entryCapacity = system->MaxRacks + system->MaxVoices + 33;
	system->dspHeap = system->context->systemHeap;
	system->dspHeapUsed = 0;
	batchSize = ngsAlignMemorySize(ngsAlignSize(32 * system->MaxRacks, 256) +
		ngsAlignSize(12 * entryCapacity, 256) + 256);
	batch = system->context->systemHeap;
	system->dspHeapUsed += batchSize;
	system->dspBatch = batch;
	batchCursor = (unsigned char *)batch + 256;
	batch->entries = ngsAllocateMemory(&batchCursor,
		ngsAlignSize(12 * entryCapacity, 256));
	system->dspBatch->entriesPhysical =
		ngsGetPhysicalAddress(system->dspBatch->entries);
	batch->entryCount = 0;
	batch->capacity = entryCapacity;
	batch->size = batchSize;
	batch->racks = ngsAllocateMemory(&batchCursor,
		ngsAlignSize(32 * system->MaxRacks, 256));
	system->dspBatch->racksPhysical =
		ngsGetPhysicalAddress(system->dspBatch->racks);
	used = system->dspHeapUsed;
	system->dspBatch->rackCount = 0;
	system->dspBatch->rackCapacity = system->MaxRacks;
	system->emptyGraph = (NgsDspRackEntry *)
		((unsigned char *)system->context->systemHeap + used);
	graphSize = ngsAlignMemorySize(sizeof(NgsDspRackEntry));
	emptyGraph = system->emptyGraph;
	system->dspHeapUsed = graphSize + used;
	emptyGraph->queuedCount = 0;
	emptyGraph->processingIndex = system->MaxRacks;
	emptyGraph->voices = NULL;
	emptyGraph->voiceCapacity = 0;
	emptyGraph->voicesPhysical = 0;
	emptyGraph->voiceCount = 0;
	emptyGraph->rackIndex = -1;
	emptyGraph->size = ngsAlignMemorySize(sizeof(NgsDspRackEntry));
	system->signature = NGS_SYSTEM_SIGNATURE;
	result = ngsRegisterBuiltinModules(system);
	system->state = NGS_SYSTEM_STATE_ACTIVE;
	system->initialCoherentAllocationSize = coherentSize;
	systemHandle = ngsGetSystemHandle(system);
	if (result != 0)
		goto unlockProcess;
	result = ngsCopyToCaller(handle, &systemHandle, sizeof(*handle),
				 storage->copyUserData);
	if (result != 0)
		goto unlockProcess;
	result = ngsCopyToCaller(system->callbackUserMemory,
		system->callbackKernelMemory, system->callbackMemorySize,
		storage->copyUserData);
	status = ngsUnlockProcess(storage);
	if (status != 0 && result == 0)
		result = SCE_NGS_ERROR;
	goto unlock;

unlockProcess:
	ngsUnlockProcess(storage);
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return result;
}

unsigned int sceNgsSystemGetSysHandleFromRack(SceNgsHRack handle)
{
	NgsSynRack *rack = ngsGetRackFromHandle(handle);

	if (rack == NULL)
		return 0;
	return ngsGetSystemHandle(rack->system);
}

int sceNgsSystemIsFixForBugzilla89940(SceNgsHSynSystem handle)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system;
	int result;

	/* Sony's returns these without leaving the syscall state. */
	if (handle == 0)
		return 0;
	system = ngsGetSystemFromHandle(handle);
	if (system == NULL)
		return 0;
	result = system->compiledSdkVersion >= 0x3550000 ||
		system->fixForBugzilla89940 == 1;
	sceSyscallLeave(syscall);
	return result;
}

/* Copies the voices' states for the caller, unless the system is updating. */
static inline void ngsPullVoiceStates(NgsSynSystem *system, int clearFlags)
{
	NgsSynRack *rack;
	unsigned int index, voice;

	if (ngsTryLockSystem(system->context) != 0) {
		system->pullDataDeferred = 1;
		return;
	}
	ngsUnlockSystem(system->context);
	system->pullDataDeferred = 0;
	for (index = 0; index < system->MaxRacks; index++) {
		rack = system->racks[index];
		if (rack == NULL || rack->state != NGS_RACK_STATE_ACTIVE)
			continue;
		for (voice = 0; voice < rack->voiceCount; voice++) {
			if (!clearFlags) {
				rack->voices[voice].callerPlayState =
					rack->voices[voice].playState;
				continue;
			}
			rack->voices[voice].callerPlayState =
				rack->voices[voice].playState;
			rack->voices[voice].callerDirtyFlags =
				rack->voices[voice].dirtyFlags;
			rack->voices[voice].callerFlags =
				rack->voices[voice].flags;
			if (rack->voices[voice].callerPlayState.pauseState != 2 &&
			    rack->voices[voice].callerPlayState.runState - 1 <= 1) {
				rack->voices[voice].flags = 0;
				rack->voices[voice].dirtyFlags = 0;
			}
		}
	}
}

/* Reads the caller's update block of a rack. */
static inline int ngsPullRackUpdates(NgsSynRack *rack)
{
	return ngsCopyFromCaller(rack->updateCount, rack->updateCountCaller,
		rack->updateBlockSize, rack->system->copyUserData);
}

SceInt32 sceNgsSystemPullDataInternal(SceNgsHSynSystem handle,
	const SceUInt32 dirtyFlagsA, const SceUInt32 dirtyFlagsB)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemFromHandle(handle);
	NgsSynRack *rack;
	unsigned int count, index;
	SceInt32 result, status;

	if (system == NULL) {
		status = SCE_NGS_ERROR_INVALID_HANDLE;
		goto leave;
	}
	count = system->MaxRacks;
	if (system->compiledSdkVersion >= 0x2000000) {
		result = ngsLockVoice(system->context);
	} else {
		result = ngsLockVoice(system->context);
		if (result != 0) {
			status = result;
			goto leave;
		}
		ngsPullVoiceStates(system, 0);
		result = ngsUnlockVoice(system->context);
	}
	if (result != 0) {
		status = result;
		goto leave;
	}

	if (count >= 32)
		count = 32;
	status = 0;
	for (index = 0; index != count; index++) {
		rack = system->racks[index];
		if (rack != NULL && ((dirtyFlagsA >> index) & 1) != 0 &&
		    rack->state == NGS_RACK_STATE_ACTIVE &&
		    ngsPullRackUpdates(rack) != 0)
			status = SCE_NGS_ERROR;
	}
	for (index = count; index < system->MaxRacks; index++) {
		rack = system->racks[index];
		/* Sony's shifts by the rack index here too. */
		if (rack != NULL && ((dirtyFlagsB >> index) & 1) != 0 &&
		    rack->state == NGS_RACK_STATE_ACTIVE &&
		    ngsPullRackUpdates(rack) != 0)
			status = SCE_NGS_ERROR;
	}

	if (system->compiledSdkVersion >= 0x2000000) {
		ngsApplyRackUpdateRequests(&system->MaxRacks, &system->racks);
		ngsPullVoiceStates(system, 1);
		status = 0;
		ngsUnlockVoice(system->context);
	}
leave:
	sceSyscallLeave(syscall);
	return status;
}

SceInt32 sceNgsSystemPushDataInternal(SceNgsHSynSystem handle)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemFromHandle(handle);
	NgsSynRack *rack;
	unsigned int index;
	SceInt32 result;
	int copied;

	if (system == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto leave;
	}
	result = 0;
	if (system->MaxRacks == 0)
		goto leave;
	index = 0;
	do {
		rack = system->racks[index++];
		if (rack == NULL || rack->state != NGS_RACK_STATE_ACTIVE ||
		    rack->requestsDirty == 0)
			continue;
		copied = ngsCopyToCaller(rack->voiceResultsCaller,
			rack->voiceResults, rack->requestBlockSize,
			system->copyUserData);
		rack->requestsDirty = 0;
		if (copied != 0)
			result = SCE_NGS_ERROR;
	} while (index < system->MaxRacks);
leave:
	sceSyscallLeave(syscall);
	return result;
}
