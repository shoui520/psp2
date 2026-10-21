/* SceNgs voices: their setup inside a rack, and the voice exports. */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <intrmgr/syscall.h>
#include <ngs/error.h>
#include "ngs_rack.h"

unsigned int ngsCountCodecModules(NgsSynSystem *system,
	const struct SceNgsVoiceDefinition *definition)
{
	const NgsVoiceModuleDefinition *modules;
	NgsModuleDefinition *module;
	unsigned int count = 0;
	unsigned int index;

	modules = (const NgsVoiceModuleDefinition *)
		((const unsigned char *)definition + definition->modulesOffset);
	for (index = 0; index < definition->moduleCount; index++) {
		__builtin_prefetch(&modules[index + 7]);
		if (ngsFindSystemModule(system, modules[index].moduleId,
					&module) == 0 &&
		    (module->flags & NGS_MODULE_FLAG_CODEC) != 0)
			count++;
	}
	return count;
}

void ngsInitVoice(NgsSynSystem *system, NgsVoice *voice, NgsSynRack *rack,
		  const SceNgsRackDescription *description,
		  unsigned char **stateCursor, unsigned char **cursor,
		  SceSize stateSize, SceSize paramsSize, int sizeOnly)
{
	const struct SceNgsVoiceDefinition *definition =
		description->pVoiceDefn;
	const NgsVoiceModuleDefinition *modules;
	NgsVoiceModuleParams *moduleParams = NULL;
	unsigned int inputCount, input, maxPatches, patch;
	unsigned int outputCount, output, patchesPerOutput, patchCount;
	unsigned int channels, index, module;
	unsigned char *params;

	if (!sizeOnly) {
		voice->channels = description->nChannelsPerVoice;
		voice->flags = 0;
		voice->patchDirtyMask = 0;
		voice->presetIndex = -1;
		moduleParams = (NgsVoiceModuleParams *)*cursor;
		voice->moduleParams = moduleParams;
		voice->dirtyFlags = 9;
		voice->rack = rack;
		voice->dirtyFlags2 = 9;
		voice->playState.runState = 0;
		voice->callerPlayState.runState = 0;
		voice->playState.pauseState = 0;
		voice->callerPlayState.pauseState = 0;
		*cursor = (unsigned char *)
			(moduleParams + definition->moduleCount);
		params = rack->params + voice->index * paramsSize;
	} else {
		*cursor += sizeof(NgsVoiceModuleParams) *
			   definition->moduleCount;
	}
	for (module = 0; module < description->pVoiceDefn->moduleCount;
	     module++) {
		if (sizeOnly)
			continue;
		if (rack->paramsSizes[module] != 0) {
			moduleParams[module].params =
				params + rack->paramsOffsets[module];
			moduleParams[module].paramsCaller =
				(unsigned char *)moduleParams[module].params +
				((unsigned char *)rack->callerBuffer -
				 (unsigned char *)rack->callbackMemory);
		} else {
			moduleParams[module].params = NULL;
			moduleParams[module].paramsCaller = NULL;
		}
		moduleParams[module].locked = 0;
	}
	if (!sizeOnly)
		voice->paramsDirtyMask = 0;

	modules = (const NgsVoiceModuleDefinition *)
		((const unsigned char *)definition + definition->modulesOffset);
	inputCount = 0;
	for (module = 0; module < definition->moduleCount; module++)
		if (modules[module].moduleId == SCE_NGS_INPUT_MIXER_ID)
			inputCount++;
	maxPatches = description->nMaxPatchesPerInput;
	if (inputCount != 0) {
		if (!sizeOnly)
			voice->inputs = (NgsVoiceInput *)*cursor;
		*cursor += sizeof(NgsVoiceInput) * inputCount;
		for (input = 0; input < inputCount; input++) {
			if (!sizeOnly) {
				NgsVoiceInput *entry = &voice->inputs[input];

				entry->maxPatches = maxPatches;
				entry->mixerContext = NULL;
				entry->mixerContextPhysical = 0;
				entry->patches = (NgsPatch **)*cursor;
			}
			*cursor += sizeof(NgsPatch *) * maxPatches;
			if (!sizeOnly)
				for (patch = 0; patch < maxPatches; patch++)
					voice->inputs[input].patches[patch] =
						NULL;
		}
	} else if (!sizeOnly) {
		voice->inputs = NULL;
	}

	patchesPerOutput = description->nPatchesPerOutput;
	channels = description->nChannelsPerVoice;
	outputCount = description->pVoiceDefn->voiceOutputCount;
	patchCount = outputCount * patchesPerOutput;
	if (!sizeOnly)
		voice->patches = (NgsPatch *)*cursor;
	*cursor += sizeof(NgsPatch) * patchCount;
	index = 0;
	for (output = 0; output < outputCount; output++) {
		for (patch = 0; patch < patchesPerOutput; patch++, index++) {
			NgsPatch *entry;

			if (sizeOnly)
				continue;
			entry = &voice->patches[index];
			entry->destination = NULL;
			entry->reserved23 = 0;
			entry->outputIndex = output;
			entry->channels = channels;
			entry->index = index;
			entry->mixerContext = NULL;
			entry->mixerContextPhysical = 0;
			entry->source = voice;
			entry->signature = NGS_PATCH_SIGNATURE;
			entry->volumes = (SceNgsVolumeMatrix *)
				(voice->rack->patchVolumes + 2 * channels *
				 (index + voice->index * patchCount));
			memset(entry->volumes, 0, 8 * channels);
		}
	}

	if (!sizeOnly && rack != NULL) {
		if (stateSize != 0)
			voice->state = rack->voiceStatesMemory +
				       voice->index * stateSize;
		else
			voice->state = NULL;
	}
}

NgsVoiceDsp *ngsAllocateVoiceDsp(NgsSynSystem *system, NgsSynRack *rack,
				 const SceNgsRackDescription *description,
				 unsigned char **cursor, int sizeOnly,
				 SceSize paramsSize, unsigned int codecCount)
{
	const struct SceNgsVoiceDefinition *definition =
		description->pVoiceDefn;
	unsigned int inputCount = ngsGetVoiceDefinitionInputCount(definition);
	unsigned int outputCount =
		ngsGetVoiceDefinitionOutputCount(definition);
	unsigned int patchCount =
		description->nPatchesPerOutput * definition->voiceOutputCount;
	SceSize size, codecSize;
	NgsVoiceDsp *dsp;
	NgsDspCodecBuffer *codecBuffers;
	unsigned char *blockCursor, *params;
	unsigned int patch;

	size = ngsAlignAllocationSize(inputCount + 8 * patchCount *
		(description->nChannelsPerVoice + 2) + sizeof(NgsVoiceDsp) +
		outputCount, 64);
	codecSize = ngsAlignAllocationSize(sizeof(NgsDspCodecBuffer) *
					   codecCount, 64);
	dsp = ngsAllocateMemoryWithAlignment(cursor, size, 64);
	codecBuffers = ngsAllocateMemoryWithAlignment(cursor, codecSize, 64);
	if (sizeOnly) {
		ngsAllocateMemoryWithAlignment(cursor, paramsSize, 64);
		return dsp;
	}

	dsp->size = size;
	blockCursor = (unsigned char *)(dsp + 1);
	dsp->reserved48 = 0;
	dsp->patchBuffers = ngsAllocateMemory(&blockCursor,
		sizeof(NgsDspPatchBuffer) * patchCount);
	for (patch = 0; patch < patchCount; patch++) {
		dsp->patchBuffers[patch].destination = NULL;
		dsp->patchBuffers[patch].destinationPhysical = 0;
		dsp->patchBuffers[patch].buffer = ngsAllocateMemory(
			&blockCursor, 8 * description->nChannelsPerVoice);
	}
	dsp->inputs = ngsAllocateMemory(&blockCursor, inputCount);
	dsp->outputs = ngsAllocateMemory(&blockCursor, outputCount);
	dsp->codecBuffers = codecBuffers;
	params = ngsAllocateMemoryWithAlignment(cursor, paramsSize, 64);
	dsp->params = params;
	dsp->paramsSize = ngsAlignAllocationSize(paramsSize, 64);
	dsp->heapOffset = params -
			  (unsigned char *)system->context->systemHeap;
	return dsp;
}

void ngsInitVoiceInputMixers(NgsSynSystem *system, NgsVoice *voice,
			     NgsVoiceDsp *dsp,
			     const SceNgsRackDescription *description,
			     unsigned char **cursor, int sizeOnly,
			     SceSize moduleMemorySize,
			     SceSize inputMixerMemorySize, SceSize codecSize)
{
	const struct SceNgsVoiceDefinition *definition =
		description->pVoiceDefn;
	const NgsVoiceModuleDefinition *modules;
	SceSize contextSize, bufferSize;
	unsigned char *moduleMemory, *inputMixerMemory, *codecMemory;
	unsigned int module, input;

	ngsGetInputMixerMemorySize(system, description, &contextSize,
				   &bufferSize);
	contextSize = ngsAlignMemorySize(contextSize);
	bufferSize = ngsAlignMemorySize(bufferSize);
	moduleMemory = ngsAllocateMemoryWithAlignment(cursor,
						      moduleMemorySize, 64);
	inputMixerMemory = ngsAllocateMemoryWithAlignment(cursor,
							  inputMixerMemorySize,
							  64);
	codecMemory = ngsAllocateMemoryWithAlignment(cursor, codecSize, 256);
	if (sizeOnly)
		return;

	dsp->moduleMemory = moduleMemory;
	dsp->moduleMemorySize = ngsAlignSize(moduleMemorySize, 64);
	dsp->inputMixerMemory = inputMixerMemory;
	dsp->inputMixerMemorySize = ngsAlignSize(inputMixerMemorySize, 64);
	voice->outputParams = inputMixerMemorySize != 0 ?
			      inputMixerMemory : NULL;
	voice->outputParamsSize = inputMixerMemorySize;
	dsp->rackDsp = voice->rack->dsp;
	modules = (const NgsVoiceModuleDefinition *)
		((const unsigned char *)definition + definition->modulesOffset);
	input = 0;
	for (module = 0; module < definition->moduleCount; module++) {
		unsigned char *context;

		__builtin_prefetch(&modules[module + 4]);
		if (modules[module].moduleId != SCE_NGS_INPUT_MIXER_ID)
			continue;
		context = dsp->moduleMemory +
			  dsp->rackDsp->layouts[module].contextOffset;
		ngsInitInputMixerContext(system, description, context,
					 codecMemory);
		codecMemory += bufferSize;
		voice->inputs[input].mixerContext = context;
		voice->inputs[input].mixerContextPhysical =
			ngsGetPhysicalAddress(context);
		input++;
		dsp->inputMixerStateSize = contextSize;
	}
}

void ngsAllocateVoiceDspStatus(NgsSynSystem *system, NgsVoiceDsp *dsp,
			       const SceNgsRackDescription *description,
			       unsigned char **cursor, int sizeOnly)
{
	SceSize size = ngsAlignSize(sizeof(NgsVoiceDspStatus), 64);
	NgsVoiceDspStatus *status;

	status = ngsAllocateMemoryWithAlignment(cursor, size, 64);
	if (sizeOnly)
		return;
	dsp->status = status;
	dsp->statusSize = size;
	status->paramsError = 0;
	status->callbacks[0].moduleIndex = NGS_VOICE_DSP_NO_MODULE;
	status->callbacks[1].moduleIndex = NGS_VOICE_DSP_NO_MODULE;
}

void ngsMapVoiceDsp(NgsVoiceDsp *dsp)
{
	unsigned int patch;

	for (patch = 0; patch < dsp->rackDsp->voiceOutputCount *
				dsp->rackDsp->patchesPerOutput; patch++) {
		dsp->patchBuffers[patch].destinationPhysical =
			ngsGetPhysicalAddress(
				dsp->patchBuffers[patch].destination);
		dsp->patchBuffers[patch].bufferPhysical =
			ngsGetPhysicalAddress(dsp->patchBuffers[patch].buffer);
	}
	dsp->moduleMemoryPhysical = ngsGetPhysicalAddress(dsp->moduleMemory);
	dsp->inputMixerMemoryPhysical =
		ngsGetPhysicalAddress(dsp->inputMixerMemory);
	dsp->paramsPhysical = ngsGetPhysicalAddress(dsp->params);
	dsp->inputsPhysical = ngsGetPhysicalAddress(dsp->inputs);
	dsp->outputsPhysical = ngsGetPhysicalAddress(dsp->outputs);
	dsp->reserved48Physical =
		ngsGetPhysicalAddress((const void *)dsp->reserved48);
	dsp->rackDspPhysical = ngsGetPhysicalAddress(dsp->rackDsp);
	dsp->patchBuffersPhysical = ngsGetPhysicalAddress(dsp->patchBuffers);
	dsp->statusPhysical = ngsGetPhysicalAddress(dsp->status);
}

/* The command for each run state, from NGS_VOICE_RUN_STATE 1 to 3. */
static const unsigned int ngsVoiceRunCommands[3] = { 1, 4, 8 };

inline int ngsUpdateVoiceFlags(NgsVoice *voice, NgsSynRack *rack)
{
	unsigned int command;

	if (voice->playState.runState - 1 > 2)
		command = 0;
	else
		command = ngsVoiceRunCommands[voice->playState.runState - 1];
	if (voice->flags & 1)
		command |= 0x10;
	if (voice->flags & 2)
		command |= 0x40;
	if (voice->playState.pauseState != 0)
		command |= 0x20;
	rack->voiceRequests[voice->index] = command;
	rack->requestsDirty = 1;
	return 1;
}

SceInt32 sceNgsVoiceGetOutputPatchInternal(SceNgsHVoice handle,
					   const SceInt32 nOutputIndex,
					   const SceInt32 nSubIndex,
					   SceNgsHPatch *pPatchHandle)
{
	unsigned int syscall = sceSyscallEnter();
	NgsVoice *voice = ngsGetVoiceFromHandle(handle);
	SceNgsHPatch patchHandle = 0;
	int copyUserData = ngsGetSystemInstanceCount();
	SceInt32 result;

	if (voice == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	} else if (pPatchHandle == NULL) {
		result = SCE_NGS_ERROR_INVALID_PARAM;
	} else {
		NgsSynRack *rack = voice->rack;

		if (nOutputIndex < 0 ||
		    nOutputIndex >= rack->voiceOutputCount ||
		    nSubIndex < 0 || nSubIndex >= rack->patchesPerOutput) {
			result = SCE_NGS_ERROR_INVALID_PARAM;
		} else {
			NgsPatch *patch = &voice->patches[nSubIndex +
				rack->patchesPerOutput * nOutputIndex];

			if (patch->destination != NULL) {
				result = 0;
				patchHandle = ngsGetPatchHandle(patch);
			} else {
				result = SCE_NGS_ERROR_PATCH_NOT_AVAIL;
			}
		}
		if (ngsCopyToCaller(pPatchHandle, &patchHandle,
				    sizeof(patchHandle), copyUserData) != 0)
			result = SCE_NGS_ERROR_INVALID_PARAM;
	}
	sceSyscallLeave(syscall);
	return result;
}

/* The module parameters of a preset: SceNgsModuleParamHeader, then the params. */
typedef struct NgsPresetParamsHeader {
	SceNgsModuleParamHeader header;
	SceNgsParamsDescriptor descriptor;
} NgsPresetParamsHeader;

SceInt32 sceNgsVoiceSetPresetInternal(SceNgsHVoice handle,
				      const SceNgsVoicePreset *preset)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system;
	NgsVoice *voice;
	NgsSynRack *rack;
	NgsModuleDefinition *module;
	SceNgsVoicePreset copy;
	SceInt32 result, error;
	unsigned int index;

	voice = ngsGetVoiceFromHandle(handle);
	if (voice == NULL || (system = ngsGetVoiceSystem(voice)) == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto leave;
	}
	rack = voice->rack;
	result = ngsLockParams(rack->system->context);
	if (result != 0)
		goto leave;

	/* Every module's parameters back to the module's first preset. */
	voice->bypassFlags = 0;
	voice->dirtyFlags |= 2;
	voice->dirtyFlags2 |= 2;
	error = 0;
	if (rack->moduleList.moduleCount == 0)
		error = result;
	for (index = 0; index < rack->moduleList.moduleCount; index++) {
		NgsVoiceModuleParams *params;
		const SceNgsParamsDescriptor *defaults;
		SceSize size;
		int count, channel;
		int found;

		found = ngsFindSystemModule(system,
			rack->moduleList.modules[index].moduleId, &module);
		if (found != 0) {
			error = found;
			continue;
		}
		params = &voice->moduleParams[index];
		if (module->presetCount == 0) {
			if (params->locked == 1) {
				error = SCE_NGS_ERROR_RESOURCE_LOCKED;
				continue;
			}
			if (params->params != NULL &&
			    voice->rack->paramsSizes[index] != 0)
				memset(params->params, 0,
				       voice->rack->paramsSizes[index]);
			voice->paramsDirtyMask |= 1 << index;
			continue;
		}
		defaults = module->presets[0].params;
		if (params->locked == 1) {
			error = SCE_NGS_ERROR_RESOURCE_LOCKED;
			continue;
		}
		if (ngsCheckParamType(
			voice->rack->moduleList.modules[index].moduleId,
			defaults->id) != 0) {
			error = SCE_NGS_ERROR_INVALID_PARAM;
			continue;
		}
		size = defaults->size;
		if (size == 0) {
			error = SCE_NGS_ERROR;
			continue;
		}
		count = voice->rack->paramsSizes[index] / size;
		if (count > voice->channels)
			count = voice->channels;
		for (channel = 1; channel <= count; channel++) {
			if (size != 0 &&
			    size * channel <= voice->rack->paramsSizes[index]) {
				memcpy((unsigned char *)params->params +
				       size * (channel - 1), defaults, size);
				voice->paramsDirtyMask |= 1 << index;
			}
			size = defaults->size;
		}
	}

	if (preset == NULL) {
		result = error;
		goto publish;
	}
	if (!ngsIsRangeReadable(preset, sizeof(*preset))) {
		/* The caller's preset: copy it and what it points to. */
		copy = (SceNgsVoicePreset){ 0 };
		if (ngsCopyFromCaller(&copy, preset, sizeof(copy),
				      system->copyUserData) != 0) {
			result = error;
			goto publish;
		}
		if (copy.nBypassFlagsOffset != 0) {
			unsigned int *bypassFlags = __builtin_alloca(
				sizeof(*bypassFlags) * copy.uNumBypassFlags);

			if (ngsCopyFromCaller(bypassFlags,
				(const unsigned char *)preset +
					copy.nBypassFlagsOffset,
				sizeof(*bypassFlags) * copy.uNumBypassFlags,
				system->copyUserData) == 0)
				copy.nBypassFlagsOffset =
					(unsigned char *)bypassFlags -
					(unsigned char *)&copy;
		}
		if (copy.nPresetDataOffset != 0) {
			unsigned char *data =
				__builtin_alloca(copy.uSizePresetData);

			if (ngsCopyFromCaller(data,
				(const unsigned char *)preset +
					copy.nPresetDataOffset,
				copy.uSizePresetData,
				system->copyUserData) == 0)
				copy.nPresetDataOffset =
					data - (unsigned char *)&copy;
		}
		preset = &copy;
	}

	if (preset->nPresetDataOffset != 0) {
		const unsigned char *data = (const unsigned char *)preset +
					    preset->nPresetDataOffset;
		SceSize dataSize = preset->uSizePresetData;
		NgsSynSystem *presetSystem;
		NgsVoice *target;
		SceSize offset;
		int checkRanges;
		SceInt32 status, check;

		target = ngsGetVoiceFromHandle(handle);
		if (target == NULL) {
			result = SCE_NGS_ERROR_INVALID_HANDLE;
			goto publish;
		}
		if (dataSize == 0 || data == NULL) {
			result = SCE_NGS_ERROR_INVALID_PARAM;
			goto publish;
		}
		presetSystem = target->rack->system;
		if (presetSystem == NULL) {
			result = SCE_NGS_ERROR_INVALID_HANDLE;
			goto publish;
		}
		checkRanges = presetSystem->flags & 1;
		offset = 0;
		status = 0;
		do {
			NgsPresetParamsHeader header;
			NgsSynRack *targetRack;
			NgsVoiceModuleParams *params;
			SceSize total, next;
			int module, channel, last;

			if (ngsCopyFromCaller(&header, data + offset,
					      sizeof(header),
					      presetSystem->copyUserData) != 0) {
				result = SCE_NGS_ERROR_INVALID_PARAM;
				goto publish;
			}
			targetRack = target->rack;
			next = offset + sizeof(header.header);
			if ((SceUInt32)header.header.moduleId >=
				    targetRack->routingIndexCount ||
			    (module = targetRack->routingIndices[
				    header.header.moduleId]) >
				    (int)targetRack->moduleList.moduleCount ||
			    module < 0) {
				status = SCE_NGS_ERROR_INVALID_PARAM;
				offset = next + header.descriptor.size;
				continue;
			}
			params = &target->moduleParams[module];
			channel = header.header.chan;
			if (params->locked == 1) {
				status = SCE_NGS_ERROR_RESOURCE_LOCKED;
				offset = next + header.descriptor.size;
				continue;
			}
			check = ngsCheckParamType(
				targetRack->moduleList.modules[module].moduleId,
				header.descriptor.id);
			if (check != 0) {
				status = check;
				offset = next + header.descriptor.size;
				continue;
			}
			if (channel != SCE_NGS_MODULE_ALL_CHANNELS &&
			    (channel < 0 || channel >= target->channels)) {
				status = SCE_NGS_ERROR_INVALID_PARAM;
				offset = next + header.descriptor.size;
				continue;
			}
			if (header.descriptor.size == 0) {
				status = SCE_NGS_ERROR;
				offset = next;
				continue;
			}
			total = target->rack->paramsSizes[module];
			if (channel == SCE_NGS_MODULE_ALL_CHANNELS) {
				SceSize count = total / header.descriptor.size;

				channel = 0;
				last = count >= target->channels ?
				       target->channels : count;
			} else {
				last = channel + 1;
				if ((SceUInt32)channel >=
				    total / header.descriptor.size) {
					status = SCE_NGS_ERROR;
					offset = next + header.descriptor.size;
					continue;
				}
			}
			check = 0;
			while (channel < last) {
				SceSize start;

				channel++;
				if (channel * header.descriptor.size <= total) {
					start = header.descriptor.size *
						(channel - 1);
					check = ngsCopyFromCaller(
						(unsigned char *)params->params +
							start,
						data + next,
						header.descriptor.size,
						presetSystem->copyUserData);
					if (check != 0)
						break;
					check = ngsCopyToCaller(
						(unsigned char *)params->paramsCaller +
							start,
						(unsigned char *)params->params +
							start,
						header.descriptor.size,
						presetSystem->copyUserData);
					if (check != 0)
						break;
					target->paramsDirtyMask |= 1 << module;
				}
				total = target->rack->paramsSizes[module];
			}
			offset = next + header.descriptor.size;
			if (check != 0) {
				status = check;
				continue;
			}
			if (checkRanges &&
			    (check = ngsCheckParamsInRange(
				    target->rack->system, target->channels,
				    target->rack->moduleList.modules[module].moduleId,
				    params->params,
				    target->rack->paramsSizes[module])) != 0) {
				status = check;
				target->paramsDirtyMask &= ~(1 << module);
			} else {
				target->dirtyFlags |= 4;
				target->dirtyFlags2 |= 4;
			}
		} while (offset < dataSize);
		error = status;
	}
	if (error != 0) {
		result = error;
		goto publish;
	}

	if (preset->nBypassFlagsOffset != 0) {
		const SceUInt32 *bypassFlags = (const SceUInt32 *)
			((const unsigned char *)preset +
			 preset->nBypassFlagsOffset);
		unsigned int flag;
		int bit;

		for (flag = 0; flag < preset->uNumBypassFlags; flag++) {
			if (bypassFlags[flag] >= rack->routingIndexCount ||
			    (bit = rack->routingIndices[bypassFlags[flag]]) >
				    (int)rack->moduleList.moduleCount ||
			    bit < 0)
				result = SCE_NGS_ERROR_INVALID_PARAM;
			else
				voice->bypassFlags |= 1 << bit;
		}
		voice->dirtyFlags |= 2;
		voice->dirtyFlags2 |= 2;
	}

publish:
	if (result == 0) {
		NgsSynRack *voiceRack = voice->rack;
		SceSize size = voiceRack->voiceParamsSize;
		SceSize start = size * voice->index;

		voice->dirtyFlags |= 4;
		voice->dirtyFlags2 |= 4;
		result = ngsCopyToCaller(voiceRack->paramsCaller + start,
					 voiceRack->params + start, size,
					 system->copyUserData);
	}
	ngsUnlockParams(voice->rack->system->context);
leave:
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsVoiceInitInternal(SceNgsHVoice handle,
				 const SceNgsVoicePreset *preset,
				 const SceUInt32 flags)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemForVoiceHandle(handle);
	NgsVoice *voice;
	NgsSynRack *rack;
	SceInt32 result;

	if (system == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto leave;
	}
	result = ngsLockVoice(system->context);
	if (result != 0)
		goto leave;
	voice = ngsGetVoiceFromHandle(handle);
	if (voice == NULL || (system = ngsGetVoiceSystem(voice)) == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto unlock;
	}
	if (voice->playState.runState != 0) {
		result = SCE_NGS_ERROR_INVALID_STATE;
		goto unlock;
	}
	voice->initId = ngsGetNextRackId(system);
	rack = voice->rack;
	rack->voiceResults[voice->index] = 0;
	voice->flags = 0;
	voice->playState.pauseState = 0;
	voice->playState.runState = 0;
	voice->dirtyFlags2 |= 8;
	voice->dirtyFlags |= 8;
	rack->voiceRequests[voice->index] = 0;
	rack->requestsDirty = 1;
	if (flags & SCE_NGS_VOICE_INIT_ROUTING) {
		unsigned int patch, count, input, index;

		count = rack->patchesPerOutput * rack->voiceOutputCount;
		for (patch = 0; patch < count; patch++)
			if (voice->patches[patch].destination != NULL)
				sceNgsPatchRemoveRoutingInternal(
					ngsGetPatchHandle(&voice->patches[patch]));
		for (input = 0; input < voice->rack->inputMixerModuleCount;
		     input++)
			for (index = 0; index < voice->inputs[input].patchCount;
			     index++)
				sceNgsPatchRemoveRoutingInternal(
					ngsGetPatchHandle(
					voice->inputs[input].patches[index]));
		rack = voice->rack;
	}
	if (flags & SCE_NGS_VOICE_INIT_CALLBACKS) {
		unsigned int module;

		for (module = 0; module < rack->moduleList.moduleCount;
		     module++) {
			voice->moduleParams[module].callback = NULL;
			voice->moduleParams[module].callbackUserData = NULL;
		}
		voice->finishedCallback = NULL;
	}
	if (flags & SCE_NGS_VOICE_INIT_PRESET) {
		if (preset == NULL)
			preset = rack->presetCount != 0 ?
				 (const SceNgsVoicePreset *)rack->presets :
				 NULL;
		result = sceNgsVoiceSetPresetInternal(handle, preset);
	}
unlock:
	ngsUnlockVoice(system->context);
leave:
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsVoicePlayInternal(SceNgsHVoice handle)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemForVoiceHandle(handle);
	NgsVoice *voice;
	SceInt32 result;

	if (system == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto leave;
	}
	result = ngsLockVoice(system->context);
	if (result != 0)
		goto leave;
	voice = ngsGetVoiceFromHandle(handle);
	if (voice == NULL || (system = ngsGetVoiceSystem(voice)) == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	} else if (voice->playState.runState != 0) {
		result = SCE_NGS_ERROR_INVALID_STATE;
	} else {
		NgsSynRack *rack = voice->rack;

		voice->flags = 1;
		voice->dirtyFlags2 |= 8;
		voice->dirtyFlags |= 8;
		rack->voiceResults[voice->index] = 0;
		voice->playState.runState = 1;
		ngsUpdateVoiceFlags(voice, rack);
	}
	ngsUnlockVoice(system->context);
leave:
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsVoiceKeyOffInternal(SceNgsHVoice handle)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemForVoiceHandle(handle);
	NgsVoice *voice;
	SceInt32 result;

	if (system == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto leave;
	}
	result = ngsLockVoice(system->context);
	if (result != 0)
		goto leave;
	voice = ngsGetVoiceFromHandle(handle);
	if (voice == NULL || (system = ngsGetVoiceSystem(voice)) == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	} else if (voice->playState.runState == 1) {
		NgsSynRack *rack = voice->rack;

		voice->flags |= 2;
		voice->dirtyFlags |= 8;
		voice->dirtyFlags2 |= 8;
		voice->playState.runState = 2;
		ngsUpdateVoiceFlags(voice, rack);
	} else if (system->compiledSdkVersion < 0x2100000) {
		result = 0;
	} else {
		result = SCE_NGS_ERROR_INVALID_STATE;
	}
	ngsUnlockVoice(system->context);
leave:
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsVoiceKillInternal(SceNgsHVoice handle)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemForVoiceHandle(handle);
	NgsVoice *voice;
	SceInt32 result;

	if (system == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto leave;
	}
	result = ngsLockVoice(system->context);
	if (result != 0)
		goto leave;
	voice = ngsGetVoiceFromHandle(handle);
	if (voice == NULL || (system = ngsGetVoiceSystem(voice)) == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	} else if (voice->playState.runState == 1 ||
		   voice->playState.runState == 2) {
		voice->playState.runState = 0;
		if (system->compiledSdkVersion >= 0x1800000)
			voice->flags = 0;
		voice->dirtyFlags2 |= 8;
		ngsUpdateVoiceFlags(voice, voice->rack);
	}
	ngsUnlockVoice(system->context);
leave:
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsVoicePauseInternal(SceNgsHVoice handle)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemForVoiceHandle(handle);
	NgsVoice *voice;
	SceInt32 result;

	if (system == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto leave;
	}
	result = ngsLockVoice(system->context);
	if (result != 0)
		goto leave;
	voice = ngsGetVoiceFromHandle(handle);
	if (voice == NULL || (system = ngsGetVoiceSystem(voice)) == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	} else if (voice->playState.pauseState == 0) {
		if (voice->flags == 1 || voice->playState.runState == 0)
			voice->playState.pauseState = 2;
		else
			voice->playState.pauseState = 1;
		voice->dirtyFlags2 |= 8;
		voice->dirtyFlags |= 8;
		ngsUpdateVoiceFlags(voice, voice->rack);
	}
	ngsUnlockVoice(system->context);
leave:
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsVoiceResumeInternal(SceNgsHVoice handle)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemForVoiceHandle(handle);
	NgsVoice *voice;
	SceInt32 result;

	if (system == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto leave;
	}
	result = ngsLockVoice(system->context);
	if (result != 0)
		goto leave;
	voice = ngsGetVoiceFromHandle(handle);
	if (voice == NULL || (system = ngsGetVoiceSystem(voice)) == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	} else if (voice->playState.pauseState != 0) {
		voice->playState.pauseState = 0;
		voice->dirtyFlags |= 8;
		voice->dirtyFlags2 |= 8;
		if (system->compiledSdkVersion >= 0x1800000)
			ngsUpdateVoiceFlags(voice, voice->rack);
	}
	ngsUnlockVoice(system->context);
leave:
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsVoiceBypassModuleInternal(SceNgsHVoice handle,
					 const SceUInt32 module,
					 const SceUInt32 flag)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemForVoiceHandle(handle);
	NgsVoice *voice;
	NgsSynRack *rack;
	SceInt32 result;
	int index;

	if (system == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto leave;
	}
	result = ngsLockVoice(system->context);
	if (result != 0 || (voice = ngsGetVoiceFromHandle(handle)) == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto unlock;
	}
	system = ngsGetVoiceSystem(voice);
	if (system == NULL)
		goto invalid;
	rack = voice->rack;
	if (module >= rack->routingIndexCount)
		goto invalid;
	index = rack->routingIndices[module];
	if (index > (int)rack->moduleList.moduleCount || index < 0)
		goto invalid;
	if (flag == SCE_NGS_MODULE_FLAG_NOT_BYPASSED) {
		if (voice->bypassFlags & (1 << index)) {
			voice->bypassFlags &= ~(1 << index);
			voice->dirtyFlags |= 2;
			voice->dirtyFlags2 |= 2;
		}
		goto unlock;
	}
	if (flag == SCE_NGS_MODULE_FLAG_BYPASSED) {
		if (!(voice->bypassFlags & (1 << index))) {
			voice->bypassFlags |= 1 << index;
			voice->dirtyFlags |= 2;
			voice->dirtyFlags2 |= 2;
		}
		goto unlock;
	}
invalid:
	result = SCE_NGS_ERROR_INVALID_PARAM;
unlock:
	ngsUnlockVoice(system->context);
leave:
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsVoiceSetModuleCallbackInternal(SceNgsHVoice handle,
	const SceUInt32 module, const SceNgsModuleCallbackFunc callback,
	void *callback_userdata)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemForVoiceHandle(handle);
	NgsVoice *voice;
	NgsSynRack *rack;
	SceInt32 result;
	int index;

	if (system == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto leave;
	}
	result = ngsLockVoice(system->context);
	if (result != 0)
		goto leave;
	voice = ngsGetVoiceFromHandle(handle);
	if (voice != NULL && (system = ngsGetVoiceSystem(voice)) != NULL &&
	    module < (rack = voice->rack)->routingIndexCount &&
	    (index = rack->routingIndices[module]) <=
		    (int)rack->moduleList.moduleCount &&
	    index >= 0) {
		voice->moduleParams[index].callback = callback;
		voice->moduleParams[index].callbackUserData = callback_userdata;
	} else {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	}
	ngsUnlockVoice(system->context);
leave:
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsVoiceSetFinishedCallbackInternal(SceNgsHVoice handle,
	const SceNgsCallbackFunc callback, void *userdata)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemForVoiceHandle(handle);
	NgsVoice *voice;
	SceInt32 result;

	if (system == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto leave;
	}
	result = ngsLockVoice(system->context);
	if (result != 0)
		goto leave;
	voice = ngsGetVoiceFromHandle(handle);
	if (voice != NULL && (system = ngsGetVoiceSystem(voice)) != NULL) {
		voice->finishedCallback = callback;
		voice->rack->voiceCallbackInfos[voice->index].pUserData =
			userdata;
	} else {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	}
	ngsUnlockVoice(system->context);
leave:
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsVoiceGetModuleBypassInternal(SceNgsHVoice handle,
					    const SceUInt32 module,
					    SceUInt32 *flag)
{
	unsigned int syscall = sceSyscallEnter();
	SceUInt32 bypass = SCE_NGS_MODULE_FLAG_NOT_BYPASSED;
	int copyUserData = ngsGetSystemInstanceCount();
	NgsSynSystem *system = ngsGetSystemForVoiceHandle(handle);
	NgsVoice *voice;
	NgsSynRack *rack;
	SceInt32 result;
	int index;

	if (system == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto leave;
	}
	result = ngsLockVoice(system->context);
	if (result != 0)
		goto leave;
	voice = ngsGetVoiceFromHandle(handle);
	if (voice == NULL || (system = ngsGetVoiceSystem(voice)) == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	} else if (flag == NULL) {
		result = SCE_NGS_ERROR_INVALID_PARAM;
	} else {
		rack = voice->rack;
		if (module >= rack->routingIndexCount ||
		    (index = rack->routingIndices[module]) >
			    (int)rack->moduleList.moduleCount ||
		    index < 0)
			result = SCE_NGS_ERROR_INVALID_PARAM;
		else if (voice->bypassFlags & (1 << index))
			bypass = SCE_NGS_MODULE_FLAG_BYPASSED;
		else
			bypass = SCE_NGS_MODULE_FLAG_NOT_BYPASSED;
		if (ngsCopyToCaller(flag, &bypass, sizeof(bypass),
				    copyUserData) != 0)
			result = SCE_NGS_ERROR_INVALID_PARAM;
	}
	ngsUnlockVoice(system->context);
leave:
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsVoiceGetParamsOutOfRangeBufferedInternal(SceNgsHVoice handle,
	const SceUInt32 module, char *message_buffer)
{
	unsigned int syscall = sceSyscallEnter();
	int copyUserData = ngsGetSystemInstanceCount();
	unsigned char params[320];
	char message[128];
	NgsSynSystem *system;
	NgsVoice *voice;
	NgsSynRack *rack;
	SceInt32 result;
	int index;

	message[0] = '\0';
	system = ngsGetSystemForVoiceHandle(handle);
	if (system == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto leave;
	}
	result = ngsLockVoice(system->context);
	if (result != 0)
		goto leave;
	voice = ngsGetVoiceFromHandle(handle);
	if (voice == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	} else if (message_buffer == NULL ||
		   ngsIsCallerRangeAccessible(message_buffer, sizeof(message),
					      copyUserData) == 0) {
		result = SCE_NGS_ERROR_INVALID_PARAM;
	} else {
		rack = voice->rack;
		if (module >= rack->routingIndexCount ||
		    (index = rack->routingIndices[module]) >
			    (int)rack->moduleList.moduleCount ||
		    index < 0) {
			result = SCE_NGS_ERROR_INVALID_PARAM;
		} else {
			unsigned int moduleId;
			unsigned short paramsId;

			__builtin_strcpy(message, "No value was out of range\n");
			moduleId = rack->moduleList.modules[index].moduleId;
			paramsId = *(const unsigned short *)
				voice->moduleParams[index].params;
			if (moduleId != paramsId) {
				snprintf(message, sizeof(message),
					 "Voice Module ID (0x%x) mismatch with Param ID (0x%x)\n",
					 moduleId, paramsId);
			} else if (rack->paramsSizes[index] >= 0x144 ||
				   ngsCopyFromCaller(params,
					voice->moduleParams[index].paramsCaller,
					rack->paramsSizes[index],
					copyUserData) != 0) {
				result = SCE_NGS_ERROR;
			} else {
				result = ngsGetParamsOutOfRange(
					voice->rack->system, voice->channels,
					voice->rack->moduleList.modules[index].moduleId,
					(const SceNgsParamsDescriptor *)params,
					voice->rack->paramsSizes[index],
					message);
			}
		}
		if (ngsCopyToCaller(message_buffer, message, sizeof(message),
				    copyUserData) != 0)
			result = SCE_NGS_ERROR_INVALID_PARAM;
	}
	ngsUnlockVoice(system->context);
leave:
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsVoiceSetAllBypassesInternal(SceNgsHVoice handle,
					   const SceUInt32 bitflag)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemForVoiceHandle(handle);
	NgsVoice *voice;
	SceInt32 result;

	if (system == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto leave;
	}
	result = ngsLockVoice(system->context);
	if (result != 0)
		goto leave;
	voice = ngsGetVoiceFromHandle(handle);
	if (voice != NULL) {
		system = ngsGetVoiceSystem(voice);
		if (system != NULL) {
			voice->bypassFlags = bitflag;
			voice->dirtyFlags |= 2;
			voice->dirtyFlags2 |= 2;
		}
	} else {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	}
	ngsUnlockVoice(system->context);
leave:
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsVoiceClearDirtyFlagInternal(SceNgsHVoice handle,
					   const SceUInt32 param_bit_flag)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemForVoiceHandle(handle);
	NgsVoice *voice;
	SceInt32 result;

	if (system == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto leave;
	}
	result = ngsLockVoice(system->context);
	if (result != 0)
		goto leave;
	voice = ngsGetVoiceFromHandle(handle);
	if (voice == NULL || (system = ngsGetVoiceSystem(voice)) == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	} else {
		result = ngsLockParams(system->context);
		if (result == 0) {
			voice->dirtyFlags &= ~param_bit_flag;
			voice->dirtyFlags2 &= ~param_bit_flag;
			ngsUnlockParams(system->context);
		}
	}
	ngsUnlockVoice(system->context);
leave:
	sceSyscallLeave(syscall);
	return result;
}
