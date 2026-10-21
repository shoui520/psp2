/* SceNgs voices: their setup inside a rack, and the voice exports. */

#include <stddef.h>
#include <string.h>
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
	NgsVoiceModuleParams *moduleParams;
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

int ngsUpdateVoiceFlags(NgsVoice *voice, NgsSynRack *rack)
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
