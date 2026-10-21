/* Racks: creation, the processing order of the system's racks, release. */

#include <scetypes.h>
#include <stddef.h>
#include <string.h>
#include <intrmgr/syscall.h>
#include <sysmem/cpu.h>
#include <vnz_wrapper/vnz_wrapper.h>

#include "ngs_platform.h"
#include "ngs_system.h"
#include "ngs_rack.h"

/* The sizes ngsBuildRackDsp hands back, and its coherent memory cursor (name ours). */
typedef struct NgsRackDspSetup {
	int sizeOnly;
	unsigned char *cursor;
	SceSize paramsSize;
	SceSize voiceSize;
	SceSize stateSize;
	SceSize inputMixerSize;
} NgsRackDspSetup;

/*
 * Lays out the rack's DSP block and where each module's parameters, state and
 * context sit in a voice's DSP block. A module the system does not know leaves
 * every size 0, which fails ngsBuildRack's size check.
 */
static inline void ngsBuildRackDsp(NgsSynSystem *system, NgsSynRack *rack,
	const SceNgsRackDescription *description, NgsRackDspSetup *setup)
{
	const struct SceNgsVoiceDefinition *definition = description->pVoiceDefn;
	const NgsVoiceModuleDefinition *module;
	NgsModuleDefinition *systemModule;
	NgsModuleMemoryInfo info;
	NgsDspModuleLayout *layout;
	NgsRackDsp *dsp;
	unsigned char *tables;
	SceSize size;
	SceSize contextSize;
	SceSize contextOffset;
	SceSize inputMixerOffset;
	SceSize mixerContextSize;
	SceSize mixerBufferSize;
	SceSize contextTotal;
	SceSize paramsTotal;
	SceSize stateTotal;
	SceSize inputMixerTotal;
	SceSize sharedWorkSize;
	SceSize patchSize;
	unsigned int index;
	int formatFound;

	size = ngsAlignMemorySize(sizeof(NgsRackDsp) +
		definition->moduleCount * (2 * sizeof(unsigned int) +
		sizeof(NgsDspModuleLayout) + 3));
	dsp = ngsAllocateAlignedMemory(&setup->cursor, size);
	if (!setup->sizeOnly) {
		rack->dsp = dsp;
		rack->dspSize = size;
		tables = (unsigned char *)dsp + sizeof(NgsRackDsp);
		dsp->dispatchIds = (uintptr_t)ngsAllocateMemory(&tables,
			definition->moduleCount * sizeof(unsigned int));
		dsp->dispatchIds2 = (uintptr_t)ngsAllocateMemory(&tables,
			definition->moduleCount * sizeof(unsigned int));
		dsp->layouts = ngsAllocateMemory(&tables,
			definition->moduleCount * sizeof(NgsDspModuleLayout));
		dsp->inputCounts = (uintptr_t)ngsAllocateMemory(&tables,
			definition->moduleCount);
		dsp->outputCounts = (uintptr_t)ngsAllocateMemory(&tables,
			definition->moduleCount);
		dsp->dspControls = (uintptr_t)ngsAllocateMemory(&tables,
			definition->moduleCount);
		dsp->size = rack->dspSize;
		dsp->voiceOutputCount = definition->voiceOutputCount;
		dsp->moduleCount = definition->moduleCount;
		dsp->channelsPerVoice = description->nChannelsPerVoice;
		dsp->patchesPerOutput = description->nPatchesPerOutput;
		dsp->dspModuleCount = ngsCountCodecModules(system, definition);
		dsp->dspConfiguration = definition->dspConfiguration;
		formatFound = 0;
		for (index = 0; index < definition->moduleCount; ++index) {
			module = (const NgsVoiceModuleDefinition *)
				((const unsigned char *)definition +
				 definition->modulesOffset) + index;
			systemModule = NULL;
			if (ngsFindSystemModule(system, module->moduleId,
						&systemModule) != 0)
				return;
			((unsigned int *)dsp->dispatchIds)[index] =
				systemModule->dspDispatchIds[0];
			((unsigned int *)dsp->dispatchIds2)[index] =
				systemModule->dspDispatchIds[1];
			((unsigned char *)dsp->dspControls)[index] =
				systemModule->dspControl;
			((unsigned char *)dsp->inputCounts)[index] =
				module->inputCount;
			((unsigned char *)dsp->outputCounts)[index] =
				module->outputCount;
			if (!formatFound && module->formatOffset != 0) {
				formatFound = 1;
				dsp->format = *(const struct NgsAudioFormat *)
					((const unsigned char *)module +
					 module->formatOffset);
			}
		}
		dsp->dispatchIds = ngsGetPhysicalAddress((void *)dsp->dispatchIds);
		dsp->dispatchIds2 = ngsGetPhysicalAddress((void *)dsp->dispatchIds2);
		dsp->inputCounts = ngsGetPhysicalAddress((void *)dsp->inputCounts);
		dsp->outputCounts = ngsGetPhysicalAddress((void *)dsp->outputCounts);
		dsp->dspControls = ngsGetPhysicalAddress((void *)dsp->dspControls);
		dsp->layoutsPhysical = ngsGetPhysicalAddress(dsp->layouts);
	}
	sharedWorkSize = 0;
	contextTotal = 0;
	paramsTotal = 0;
	stateTotal = 0;
	inputMixerTotal = 0;
	for (index = 0; index < definition->moduleCount; ++index) {
		module = (const NgsVoiceModuleDefinition *)
			((const unsigned char *)definition +
			 definition->modulesOffset) + index;
		if (ngsGetModuleMemoryInfo(system, module->moduleId,
				description->nChannelsPerVoice, &info) != 0)
			return;
		if (sharedWorkSize < info.sharedWorkSize)
			sharedWorkSize = info.sharedWorkSize;
		if (!setup->sizeOnly) {
			dsp->layouts[index].paramsOffset = paramsTotal;
			rack->paramsOffsets[index] = paramsTotal;
			rack->paramsSizes[index] = info.paramsSize;
			rack->stateOffsets[index] = stateTotal;
			rack->stateSizes[index] = info.stateSize;
		}
		paramsTotal += info.paramsSize;
		if (module->moduleId == SCE_NGS_INPUT_MIXER_ID) {
			ngsGetInputMixerMemorySize(system, description,
				&mixerContextSize, &mixerBufferSize);
			mixerContextSize = ngsAlignMemorySize(mixerContextSize);
			mixerBufferSize = ngsAlignMemorySize(mixerBufferSize);
			contextOffset = ngsAlignMemorySize(contextTotal);
			inputMixerOffset = ngsAlignMemorySize(inputMixerTotal);
			if (!setup->sizeOnly) {
				layout = &dsp->layouts[index];
				layout->contextOffset = contextOffset;
				layout->stateOffset = 0;
				layout->stateSize = 0;
				layout->contextSize = mixerContextSize;
			}
			contextTotal = contextOffset + mixerContextSize;
			inputMixerTotal = inputMixerOffset + mixerBufferSize;
		} else {
			contextSize = ngsAlignAllocationSize(info.contextSize, 1);
			if (!setup->sizeOnly) {
				layout = &dsp->layouts[index];
				layout->contextOffset = contextTotal;
				layout->contextSize = contextSize;
				layout->stateOffset = stateTotal;
				layout->stateSize = info.stateSize;
			}
			contextTotal += contextSize;
			stateTotal += info.stateSize;
		}
	}
	/* each output's patches: two words a channel */
	patchSize = contextTotal + description->nChannelsPerVoice * 8 *
		definition->voiceOutputCount * description->nPatchesPerOutput;
	if (!setup->sizeOnly) {
		dsp->voiceSize = patchSize + 8;
		dsp->contextAndPatchSize = patchSize;
		dsp->stateSize = stateTotal;
		dsp->paramsSize = paramsTotal;
		dsp->inputMixerSize = inputMixerTotal;
		dsp->sharedWorkSize = sharedWorkSize;
		dsp->contextSize = contextTotal;
	}
	setup->paramsSize = paramsTotal;
	setup->voiceSize = patchSize + 8;
	setup->stateSize = stateTotal;
	setup->inputMixerSize = inputMixerTotal;
}

/*
 * Builds a rack in the memory ngsAllocateProcessMemory gives it, or, with
 * callbackSize, only measures the four memories it needs (ngsGetRackMemorySize).
 * The same walk does both, so the sizes match what a build uses.
 */
int ngsBuildRack(SceNgsHSynSystem handle, void *callerBuffer,
		 SceSize callerSize, const SceNgsRackDescription *description,
		 SceNgsHRack *rackHandle, SceSize *callbackSize,
		 SceSize *coherentSize, SceSize *dspHeapSize, SceSize *systemSize)
{
	NgsSynSystem *system;
	const struct SceNgsVoiceDefinition *definition;
	const NgsVoiceModuleDefinition *modules;
	const NgsVoiceModuleDefinition *module;
	const NgsVoicePresetDefinition *preset;
	NgsModuleDefinition *systemModule;
	NgsModuleMemoryInfo info;
	NgsRackDspSetup setup;
	NgsSynRack *rack;
	NgsSynRack *other;
	NgsSynRack **slot;
	NgsRackModule *rackModule;
	NgsRackPreset *rackPreset;
	NgsDspRackEntry *entry;
	NgsRackCallerInfo *callerInfo;
	NgsVoiceDsp *dsp;
	NgsDspCodecBuffer *codecBuffer;
	NgsDspModuleLayout *layout;
	NgsVoice *voices;
	NgsRackLink *links;
	int *depthScratch;
	NgsVoiceDsp **voiceDsps;
	unsigned int *stateOffsets;
	unsigned int *stateSizes;
	unsigned int *paramsOffsets;
	unsigned int *paramsSizes;
	unsigned int *playerModules;
	uintptr_t *playerModuleOffsets;
	unsigned int *at9Modules;
	uintptr_t *at9ModuleOffsets;
	unsigned int *codecModules;
	unsigned int *inputMixerModules;
	uintptr_t *inputMixerModuleOffsets;
	unsigned int *updateCount;
	unsigned int *patchUpdateMasks;
	float *patchVolumes;
	unsigned int *paramsUpdateMasks;
	unsigned int *voiceFlags2;
	unsigned char *params;
	SceNgsPatchSetupInfo *patchSetups;
	unsigned int *patchChannels;
	unsigned int *voiceResults;
	unsigned int *voiceRequests;
	unsigned char *voiceStates;
	SceNgsCallbackInfo *callbackInfos;
	SceNgsCallbackInfo *rackCallbackInfo;
	SceNgsCallbackInfo *voiceCallbackInfos;
	uintptr_t *voiceList;
	unsigned int *voiceListCount;
	unsigned char *name;
	unsigned char *callbackMemory;
	unsigned char *coherentMemory;
	unsigned char *dspHeapMemory;
	unsigned char *systemMemory;
	unsigned char *callbackCursor = NULL;
	unsigned char *coherentCursor = NULL;
	unsigned char *dspHeapCursor = NULL;
	unsigned char *systemCursor = NULL;
	SceSize rackCallbackSize = 0;
	SceSize rackCoherentSize = 0;
	SceSize rackDspHeapSize = 0;
	SceSize rackSystemSize = 0;
	SceUID memBlock = 0;
	SceNgsHRack newHandle;
	unsigned int command[4];
	ptrdiff_t callerOffset;
	SceSize nameSize;
	SceSize size;
	SceSize paramsTotal;
	SceSize voiceSize;
	SceSize stateTotal;
	SceSize inputMixerTotal;
	unsigned int voiceCount;
	unsigned int patchesPerVoice;
	unsigned int patchCount;
	unsigned int channels;
	unsigned int codecCount;
	unsigned int codecModuleCount;
	unsigned int playerCount;
	unsigned int at9Count;
	unsigned int inputMixerCount;
	unsigned int player;
	unsigned int at9;
	unsigned int inputMixer;
	unsigned int codec;
	unsigned int capacity;
	unsigned int index;
	unsigned int count;
	unsigned int voice;
	unsigned int failed;
	unsigned int rackIndex;
	int codecHandle;
	int sizeOnly;
	int result;
	int i;

	system = ngsGetSystemFromHandle(handle);
	if (system == NULL)
		return SCE_NGS_ERROR_INVALID_HANDLE;
	if (description == NULL || description->pVoiceDefn == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;
	if (callbackSize != NULL) {
		sizeOnly = 1;
		callbackMemory = NULL;
		coherentMemory = NULL;
		dspHeapMemory = NULL;
		systemMemory = NULL;
	} else if (rackHandle != NULL) {
		result = ngsGetRackMemorySize(handle, description,
			&rackCallbackSize, &rackCoherentSize, &rackDspHeapSize,
			&rackSystemSize);
		if (result != 0)
			return result;
		if (((uintptr_t)callbackCursor & 15) != 0)
			return SCE_NGS_ERROR_INVALID_ALIGNMENT;
		result = ngsAllocateProcessMemory(handle, rackCoherentSize,
			rackCallbackSize, rackDspHeapSize, rackSystemSize,
			callerBuffer, callerSize, (void **)&callbackCursor,
			(void **)&coherentCursor, (void **)&dspHeapCursor,
			(void **)&systemCursor, &memBlock);
		if (result != 0) {
			if (system->compiledSdkVersion < 0x1800000 &&
			    result == SCE_NGS_ERROR_INTERNAL_ALLOC)
				result = SCE_NGS_ERROR_OUT_OF_ASSETS;
			return result;
		}
		sizeOnly = 0;
		callbackMemory = callbackCursor;
		coherentMemory = coherentCursor;
		dspHeapMemory = dspHeapCursor;
		systemMemory = systemCursor;
	} else {
		return SCE_NGS_ERROR_INVALID_PARAM;
	}
	if (callerSize < rackCallbackSize) {
		result = SCE_NGS_ERROR_SIZE_MISMATCH;
		goto fail;
	}
	codecCount = ngsCountCodecModules(system, description->pVoiceDefn);
	if (codecCount > 2) {
		result = SCE_NGS_ERROR_INVALID_PARAM;
		goto fail;
	}

	/* the rack, and its copy of the voice definition's modules and presets */
	if (sizeOnly) {
		rack = NULL;
		ngsAllocateMemory(&systemCursor, sizeof(NgsSynRack));
		definition = description->pVoiceDefn;
		modules = (const NgsVoiceModuleDefinition *)
			((const unsigned char *)definition +
			 definition->modulesOffset);
		systemCursor += definition->moduleCount * sizeof(NgsRackModule) +
			definition->routingIndexCount * sizeof(unsigned int);
		for (module = modules; module != modules + definition->moduleCount;
		     ++module) {
			if (module->inputBindingsOffset != 0)
				systemCursor += module->inputCount *
					sizeof(unsigned int);
			if (module->outputBindingsOffset != 0)
				systemCursor += module->outputCount *
					sizeof(unsigned int);
		}
		if (definition->presetCount != 0 &&
		    definition->presetsOffset != 0) {
			systemCursor += definition->presetCount *
				sizeof(NgsRackPreset);
			for (index = 0; index < definition->presetCount; ++index) {
				preset = (const NgsVoicePresetDefinition *)
					((const unsigned char *)definition +
					 definition->presetsOffset) + index;
				if (preset->nameOffset != 0 &&
				    preset->nameLength != 0)
					systemCursor += ngsAlignSize(
						preset->nameLength + 1, 4);
				if (preset->paramsOffset != 0 &&
				    preset->paramsSize != 0)
					systemCursor += preset->paramsSize;
				if (preset->indicesOffset != 0 &&
				    preset->indicesCount != 0)
					systemCursor += preset->indicesCount *
						sizeof(unsigned int);
			}
		}
	} else {
		memset(callbackMemory, 0, rackCallbackSize);
		rack = ngsAllocateMemory(&systemCursor, sizeof(NgsSynRack));
		if (rack == NULL)
			goto outOfAssets;
		rack->state = 0;
		rack->coherentMemory = coherentMemory;
		rack->dspHeapSize = rackDspHeapSize;
		rack->memBlock = memBlock;
		rack->dspHeapMemory = dspHeapMemory;
		rack->maxPatchesPerInput = description->nMaxPatchesPerInput;
		rack->callerBuffer = callerBuffer;
		rack->callbackMemory = callbackMemory;
		rack->systemMemory = systemMemory;
		rack->coherentSize = rackCoherentSize;
		rack->systemSize = rackSystemSize;
		rack->callbackSize = rackCallbackSize;
		if (system->MaxRacks == 0)
			goto outOfAssets;
		for (rackIndex = 0; system->racks[rackIndex] != NULL;)
			if (++rackIndex == system->MaxRacks)
				goto outOfAssets;
		system->racks[rackIndex] = rack;
		rack->channelsPerVoice = description->nChannelsPerVoice;
		rack->index = rackIndex;
		rack->voiceCount = description->nVoices;
		rack->dspModuleCount = codecCount;
		rack->releaseCallback = NULL;
		rack->paramErrorCallback = NULL;
		rack->userReleaseData = description->pUserReleaseData;
		rack->system = system;
		rack->voices = NULL;
		rack->patchesPerOutput = description->nPatchesPerOutput;
		rack->signature = NGS_RACK_SIGNATURE;
		rack->patchesPerVoice = description->pVoiceDefn->voiceOutputCount *
			description->nPatchesPerOutput;
		rack->voiceOutputCount = description->pVoiceDefn->voiceOutputCount;
		rack->moduleCount8 = description->pVoiceDefn->moduleCount;
		definition = description->pVoiceDefn;
		modules = (const NgsVoiceModuleDefinition *)
			((const unsigned char *)definition +
			 definition->modulesOffset);
		rack->moduleList.moduleCount = definition->moduleCount;
		rack->routingIndexCount = definition->routingIndexCount;
		rack->presetCount = 0;
		rack->presets = NULL;
		rack->moduleList.modules = (NgsRackModule *)systemCursor;
		rack->routingIndices = (unsigned int *)(systemCursor +
			definition->moduleCount * sizeof(NgsRackModule));
		systemCursor = (unsigned char *)(rack->routingIndices +
			definition->routingIndexCount);
		for (index = 0; index < rack->routingIndexCount; ++index)
			rack->routingIndices[index] = ((const unsigned int *)
				((const unsigned char *)definition +
				 definition->routingIndicesOffset))[index];
		rackModule = rack->moduleList.modules;
		for (index = 0; index < rack->moduleList.moduleCount;
		     ++index, ++rackModule) {
			module = &modules[index];
			rackModule->outputBindings = NULL;
			rackModule->moduleId = module->moduleId;
			rackModule->inputCount = module->inputCount;
			rackModule->outputCount = module->outputCount;
			rackModule->inputBindings = NULL;
			if (module->inputBindingsOffset != 0) {
				rackModule->inputBindings =
					(unsigned int *)systemCursor;
				systemCursor += module->inputCount *
					sizeof(unsigned int);
				for (count = 0; count < module->inputCount;
				     ++count)
					rackModule->inputBindings[count] =
						((const unsigned int *)
						((const unsigned char *)module +
						 module->inputBindingsOffset))[count];
			}
			if (module->outputBindingsOffset != 0) {
				rackModule->outputBindings =
					(unsigned int *)systemCursor;
				systemCursor += module->outputCount *
					sizeof(unsigned int);
				for (count = 0; count < module->outputCount;
				     ++count)
					rackModule->outputBindings[count] =
						((const unsigned int *)
						((const unsigned char *)module +
						 module->outputBindingsOffset))[count];
			}
		}
		if (definition->presetCount != 0 &&
		    definition->presetsOffset != 0) {
			rack->presetCount = definition->presetCount;
			rack->presets = (NgsRackPreset *)systemCursor;
			systemCursor += definition->presetCount *
				sizeof(NgsRackPreset);
			for (index = 0; index < rack->presetCount; ++index) {
				preset = (const NgsVoicePresetDefinition *)
					((const unsigned char *)
					 description->pVoiceDefn +
					 description->pVoiceDefn->presetsOffset) +
					index;
				rackPreset = &rack->presets[index];
				rackPreset->nameLength = 0;
				rackPreset->bypassFlagCount = 0;
				rackPreset->paramsSize = 0;
				rackPreset->nameOffset = 0;
				rackPreset->bypassFlagsOffset = 0;
				rackPreset->paramsOffset = 0;
				if (preset->nameOffset != 0 &&
				    preset->nameLength != 0) {
					nameSize = ngsAlignSize(
						preset->nameLength + 1, 4);
					name = systemCursor;
					systemCursor += nameSize;
					memset(name, 0, nameSize);
					rackPreset->nameOffset = (unsigned char *)
						memcpy(name,
						(const unsigned char *)preset +
						preset->nameOffset,
						preset->nameLength) -
						(unsigned char *)rackPreset;
					rackPreset->nameLength =
						preset->nameLength;
				}
				if (preset->paramsOffset != 0 &&
				    preset->paramsSize != 0) {
					name = systemCursor;
					systemCursor += preset->paramsSize;
					rackPreset->paramsOffset = (unsigned char *)
						memcpy(name,
						(const unsigned char *)preset +
						preset->paramsOffset,
						preset->paramsSize) -
						(unsigned char *)rackPreset;
					rackPreset->paramsSize =
						preset->paramsSize;
				}
				if (preset->indicesOffset != 0 &&
				    preset->indicesCount != 0) {
					size = preset->indicesCount *
						sizeof(unsigned int);
					name = systemCursor;
					systemCursor += size;
					rackPreset->bypassFlagsOffset =
						(unsigned char *)memcpy(name,
						(const unsigned char *)preset +
						preset->indicesOffset, size) -
						(unsigned char *)rackPreset;
					rackPreset->bypassFlagCount =
						preset->indicesCount;
				}
			}
		}
	}

	/* the player, ATRAC9 player, codec and input mixer modules */
	definition = description->pVoiceDefn;
	modules = (const NgsVoiceModuleDefinition *)
		((const unsigned char *)definition + definition->modulesOffset);
	playerCount = 0;
	for (index = 0; index < definition->moduleCount; ++index)
		if (modules[index].moduleId == SCE_NGS_PLAYER_ID)
			++playerCount;
	playerModules = NULL;
	playerModuleOffsets = NULL;
	if (playerCount != 0) {
		playerModules = ngsAllocateMemory(&systemCursor,
			playerCount * sizeof(unsigned int));
		playerModuleOffsets = ngsAllocateMemory(&systemCursor,
			playerCount * sizeof(uintptr_t));
	}
	at9Count = 0;
	for (index = 0; index < definition->moduleCount; ++index)
		if (modules[index].moduleId == SCE_NGS_AT9_ID)
			++at9Count;
	at9Modules = NULL;
	at9ModuleOffsets = NULL;
	if (at9Count != 0) {
		at9Modules = ngsAllocateMemory(&systemCursor,
			at9Count * sizeof(unsigned int));
		at9ModuleOffsets = ngsAllocateMemory(&systemCursor,
			at9Count * sizeof(uintptr_t));
	}
	codecModuleCount = 0;
	for (index = 0; index < definition->moduleCount; ++index)
		if (ngsIsCodecModule(system, modules[index].moduleId))
			++codecModuleCount;
	codecModules = NULL;
	if (codecModuleCount != 0)
		codecModules = ngsAllocateMemory(&systemCursor,
			codecModuleCount * sizeof(unsigned int));
	inputMixerCount = 0;
	for (index = 0; index < description->pVoiceDefn->moduleCount; ++index)
		if (modules[index].moduleId == SCE_NGS_INPUT_MIXER_ID)
			++inputMixerCount;
	inputMixerModules = NULL;
	inputMixerModuleOffsets = NULL;
	if (inputMixerCount != 0) {
		inputMixerModules = ngsAllocateMemory(&systemCursor,
			inputMixerCount * sizeof(unsigned int));
		inputMixerModuleOffsets = ngsAllocateMemory(&systemCursor,
			inputMixerCount * sizeof(uintptr_t));
	}
	if (rack != NULL) {
		rack->codecModuleCount = codecModuleCount;
		rack->playerModuleCount = playerCount;
		rack->playerModules = playerModules;
		rack->at9Modules = at9Modules;
		rack->at9ModuleCount = at9Count;
		rack->codecModules = codecModules;
		rack->playerModuleOffsets = playerModuleOffsets;
		rack->at9ModuleOffsets = at9ModuleOffsets;
		rack->inputMixerModules = inputMixerModules;
		rack->inputMixerModuleCount = inputMixerCount;
		rack->inputMixerModuleOffsets = inputMixerModuleOffsets;
		player = 0;
		at9 = 0;
		inputMixer = 0;
		codec = 0;
		for (index = 0; index < description->pVoiceDefn->moduleCount;
		     ++index) {
			switch (modules[index].moduleId) {
			case SCE_NGS_PLAYER_ID:
				rack->playerModules[player++] = index;
				break;
			case SCE_NGS_AT9_ID:
				rack->at9Modules[at9++] = index;
				break;
			case SCE_NGS_INPUT_MIXER_ID:
				rack->inputMixerModules[inputMixer++] = index;
				break;
			}
			if (ngsIsCodecModule(system, modules[index].moduleId))
				rack->codecModules[codec++] = index;
		}
	}

	/* the voices, the links to other racks and the module tables */
	voices = ngsAllocateMemory(&systemCursor,
		description->nVoices * sizeof(NgsVoice));
	links = ngsAllocateMemory(&systemCursor,
		system->MaxRacks * sizeof(NgsRackLink));
	depthScratch = ngsAllocateMemory(&systemCursor,
		system->MaxRacks * sizeof(int));
	voiceDsps = ngsAllocateMemory(&systemCursor,
		description->nVoices * sizeof(NgsVoiceDsp *));
	paramsOffsets = ngsAllocateMemory(&systemCursor,
		description->pVoiceDefn->moduleCount * sizeof(unsigned int));
	paramsSizes = ngsAllocateMemory(&systemCursor,
		description->pVoiceDefn->moduleCount * sizeof(unsigned int));
	stateSizes = ngsAllocateMemory(&systemCursor,
		description->pVoiceDefn->moduleCount * sizeof(unsigned int));
	stateOffsets = ngsAllocateMemory(&systemCursor,
		description->pVoiceDefn->moduleCount * sizeof(unsigned int));
	if (!sizeOnly) {
		rack->voices = voices;
		rack->links = links;
		rack->depthScratch = depthScratch;
		rack->paramsOffsets = paramsOffsets;
		rack->paramsSizes = paramsSizes;
		rack->voiceDsps = voiceDsps;
		rack->stateOffsets = stateOffsets;
		rack->stateSizes = stateSizes;
	}

	/* the rack's entry in the codec engine's rack list */
	capacity = system->MaxVoices - description->nVoices;
	coherentCursor = (unsigned char *)
		ngsAlignMemoryAddress((uintptr_t)coherentCursor);
	dspHeapCursor = (unsigned char *)
		ngsAlignMemoryAddress((uintptr_t)dspHeapCursor);
	entry = ngsAllocateAlignedMemory(&dspHeapCursor,
		sizeof(NgsDspRackEntry) + capacity * sizeof(unsigned int));
	if (!sizeOnly) {
		rack->dspRackList = entry;
		entry->voiceCount = 0;
		entry->processingIndex = -1;
		entry->voiceCapacity = capacity;
		entry->voices = entry->voiceSlots;
		entry->rackIndex = rack->index;
		entry->voicesPhysical = ngsGetPhysicalAddress(entry->voiceSlots);
		entry->size = dspHeapCursor - (unsigned char *)entry;
	}

	setup.sizeOnly = sizeOnly;
	setup.cursor = coherentCursor;
	setup.paramsSize = 0;
	setup.voiceSize = 0;
	setup.stateSize = 0;
	setup.inputMixerSize = 0;
	ngsBuildRackDsp(system, rack, description, &setup);
	paramsTotal = setup.paramsSize;
	voiceSize = setup.voiceSize;
	stateTotal = setup.stateSize;
	inputMixerTotal = setup.inputMixerSize;
	coherentCursor = setup.cursor;
	if (rack != NULL) {
		for (index = 0; index < playerCount; ++index)
			rack->playerModuleOffsets[index] = rack->dsp->layouts[
				rack->playerModules[index]].contextOffset;
		for (index = 0; index < at9Count; ++index)
			rack->at9ModuleOffsets[index] = rack->dsp->layouts[
				rack->at9Modules[index]].contextOffset;
		for (index = 0; index < inputMixerCount; ++index)
			rack->inputMixerModuleOffsets[index] = rack->dsp->layouts[
				rack->inputMixerModules[index]].contextOffset;
	}

	/* what the caller's library reads and writes: the callback memory */
	voiceCount = description->nVoices;
	channels = description->nChannelsPerVoice;
	patchesPerVoice = description->pVoiceDefn->voiceOutputCount *
		description->nPatchesPerOutput;
	patchCount = voiceCount * patchesPerVoice;
	callerInfo = ngsAllocateMemory(&callbackCursor, sizeof(NgsRackCallerInfo));
	updateCount = ngsAllocateMemory(&callbackCursor, sizeof(unsigned int));
	patchUpdateMasks = ngsAllocateMemory(&callbackCursor,
		voiceCount * sizeof(unsigned int));
	patchVolumes = ngsAllocateMemory(&callbackCursor,
		channels * 2 * sizeof(float) * voiceCount * patchesPerVoice);
	paramsUpdateMasks = ngsAllocateMemory(&callbackCursor,
		voiceCount * sizeof(unsigned int));
	voiceFlags2 = ngsAllocateMemory(&callbackCursor,
		voiceCount * sizeof(unsigned int));
	params = ngsAllocateMemory(&callbackCursor, voiceCount * paramsTotal);
	if (rack != NULL) {
		*updateCount = 0;
		callerOffset = (unsigned char *)callerBuffer - callbackMemory;
		memset(patchUpdateMasks, 0, voiceCount * sizeof(unsigned int));
		memset(patchVolumes, 0,
		       channels * 2 * sizeof(float) * voiceCount * patchesPerVoice);
		memset(paramsUpdateMasks, 0, voiceCount * sizeof(unsigned int));
		memset(voiceFlags2, 0, voiceCount * sizeof(unsigned int));
		memset(params, 0, voiceCount * paramsTotal);
		callerInfo->channelsPerVoice = description->nChannelsPerVoice;
		callerInfo->patchUpdateMasks = (unsigned int *)
			((unsigned char *)patchUpdateMasks + callerOffset);
		callerInfo->voiceCount = voiceCount;
		callerInfo->patchVolumes = (float *)
			((unsigned char *)patchVolumes + callerOffset);
		callerInfo->paramsUpdateMasks = (unsigned int *)
			((unsigned char *)paramsUpdateMasks + callerOffset);
		callerInfo->patchesPerVoice = patchesPerVoice;
		callerInfo->voiceFlags2 = (unsigned int *)
			((unsigned char *)voiceFlags2 + callerOffset);
		callerInfo->params = params + callerOffset;
		callerInfo->updateCount = (unsigned int *)
			((unsigned char *)updateCount + callerOffset);
		rack->updateCount = updateCount;
		rack->updateCountCaller = (unsigned int *)
			((unsigned char *)updateCount + callerOffset);
		rack->updateRequested = updateCount;
		rack->paramsCaller = params + callerOffset;
		rack->patchUpdateMasks = patchUpdateMasks;
		rack->patchVolumes = patchVolumes;
		rack->paramsUpdateMasks = paramsUpdateMasks;
		rack->voiceParamsSize = paramsTotal;
		rack->voiceFlags2 = voiceFlags2;
		rack->updateBlockSize = callbackCursor - (unsigned char *)updateCount;
		rack->params = params;
		patchSetups = ngsAllocateMemory(&callbackCursor,
			patchCount * sizeof(SceNgsPatchSetupInfo));
		patchChannels = ngsAllocateMemory(&callbackCursor,
			patchCount * sizeof(unsigned int));
		if (patchCount != 0) {
			memset(patchSetups, 0,
			       patchCount * sizeof(SceNgsPatchSetupInfo));
			memset(patchChannels, 0, patchCount * sizeof(unsigned int));
			rack->patchSetups = patchSetups;
			rack->patchSetupsCaller = (SceNgsPatchSetupInfo *)
				((unsigned char *)patchSetups + callerOffset);
			rack->patchChannels = patchChannels;
			rack->patchChannelsCaller = (unsigned int *)
				((unsigned char *)patchChannels + callerOffset);
			callerInfo->patchSetups = rack->patchSetupsCaller;
			callerInfo->patchChannels = rack->patchChannelsCaller;
		}
	} else {
		ngsAllocateMemory(&callbackCursor,
			patchCount * sizeof(SceNgsPatchSetupInfo));
		ngsAllocateMemory(&callbackCursor,
			patchCount * sizeof(unsigned int));
	}
	voiceResults = ngsAllocateMemory(&callbackCursor,
		voiceCount * sizeof(unsigned int));
	voiceRequests = ngsAllocateMemory(&callbackCursor,
		voiceCount * sizeof(unsigned int));
	voiceStates = NULL;
	if (stateTotal != 0)
		voiceStates = ngsAllocateMemory(&callbackCursor,
			voiceCount * stateTotal);
	if (rack != NULL) {
		memset(voiceRequests, 0, voiceCount * sizeof(unsigned int));
		memset(voiceResults, 0, voiceCount * sizeof(unsigned int));
		if (stateTotal != 0)
			memset(voiceStates, 0, voiceCount * stateTotal);
		rack->requestBlockSize = callbackCursor -
			(unsigned char *)voiceResults;
		rack->voiceResults = voiceResults;
		rack->voiceResultsCaller = (unsigned int *)
			((unsigned char *)voiceResults + callerOffset);
		rack->voiceRequests = voiceRequests;
		rack->voiceRequestsCaller = (unsigned int *)
			((unsigned char *)voiceRequests + callerOffset);
		rack->voiceStatesMemory = voiceStates;
		rack->voiceStatesMemoryCaller = voiceStates + callerOffset;
		rack->requestsDirty = 1;
		callerInfo->voiceResults = rack->voiceResultsCaller;
		callerInfo->voiceRequests = rack->voiceRequestsCaller;
		callerInfo->voiceStates = rack->voiceStatesMemoryCaller;
		callerInfo->inputMixerModuleCount = rack->inputMixerModuleCount;
		callerInfo->moduleCount = rack->moduleCount8;
		callerInfo->patchesPerOutput = description->nPatchesPerOutput;
		callerInfo->voiceStateSize = stateTotal;
		callerInfo->voiceParamsSize = paramsTotal;
		callerInfo->voiceOutputCount = rack->voiceOutputCount;
		for (index = 0; index < rack->moduleCount8; ++index) {
			callerInfo->stateOffsets[index] = rack->stateOffsets[index];
			callerInfo->stateSizes[index] = rack->stateSizes[index];
			callerInfo->paramsOffsets[index] =
				rack->paramsOffsets[index];
			callerInfo->moduleIds[index] =
				rack->moduleList.modules[index].moduleId;
			callerInfo->paramsSizes[index] = rack->paramsSizes[index];
		}
		count = rack->moduleCount8;
		if (count != 32) {
			size = (32 - count) * sizeof(unsigned int);
			memset(&callerInfo->stateOffsets[count], 0, size);
			memset(&callerInfo->stateSizes[count], 0, size);
			memset(&callerInfo->paramsOffsets[count], 0, size);
			memset(&callerInfo->paramsSizes[count], 0, size);
			memset(&callerInfo->moduleIds[count], 0, size);
		}
		callerInfo->routingIndexCount = rack->routingIndexCount;
		for (index = 0; index < rack->routingIndexCount; ++index)
			callerInfo->routingIndices[index] =
				rack->routingIndices[index];
	}

	for (voice = 0; voice < description->nVoices; ++voice) {
		if (rack != NULL)
			rack->voices[voice].index = voice;
		ngsInitVoice(system, rack != NULL ? &rack->voices[voice] : NULL,
			rack, description, &callbackCursor, &systemCursor,
			stateTotal, paramsTotal, sizeOnly);
	}
	systemCursor = (unsigned char *)
		ngsAlignMemorySize((SceSize)systemCursor);
	callbackInfos = ngsAllocateMemory(&callbackCursor,
		codecCount * sizeof(SceNgsCallbackInfo) * description->nVoices);
	rackCallbackInfo = ngsAllocateMemory(&callbackCursor,
		sizeof(SceNgsCallbackInfo));
	voiceCallbackInfos = ngsAllocateMemory(&callbackCursor,
		description->nVoices * sizeof(SceNgsCallbackInfo));
	if (rack != NULL) {
		newHandle = ngsGetRackHandle(rack);
		rack->callbackInfos = callbackInfos;
		rack->rackCallbackInfo = rackCallbackInfo;
		rack->voiceCallbackInfos = voiceCallbackInfos;
		for (i = 0; i < description->nVoices; ++i) {
			rack->voiceCallbackInfos[i].hRackHandle = newHandle;
			rack->voiceCallbackInfos[i].uModuleID = 0;
			rack->voiceCallbackInfos[i].nCallbackData = i;
			rack->voiceCallbackInfos[i].nCallbackData2 = 0;
			rack->voiceCallbackInfos[i].pCallbackPtr = NULL;
			rack->voiceCallbackInfos[i].pUserData = NULL;
			rack->voiceCallbackInfos[i].hVoiceHandle =
				ngsGetVoiceHandle(&rack->voices[i]);
		}
		coherentCursor = (unsigned char *)
			ngsAlignMemoryAddress((uintptr_t)coherentCursor);
		rack->voiceDspMemory = dspHeapCursor;
	} else {
		coherentCursor = (unsigned char *)
			ngsAlignMemoryAddress((uintptr_t)coherentCursor);
	}

	/* each voice's DSP block, input mixers and status */
	for (voice = 0; voice < description->nVoices; ++voice) {
		dsp = ngsAllocateVoiceDsp(system, rack, description,
			&dspHeapCursor, sizeOnly, paramsTotal, codecModuleCount);
		if (rack != NULL) {
			rack->voiceDsps[voice] = dsp;
			rack->voiceDsps[voice]->updatedParams = 0;
		}
	}
	dspHeapCursor = (unsigned char *)
		ngsAlignMemoryAddress((uintptr_t)dspHeapCursor);
	if (rack != NULL) {
		rack->voiceDspMemorySize = dspHeapCursor -
			(unsigned char *)rack->voiceDspMemory;
		rack->voiceDspMemoryEnd = coherentMemory;
	}
	for (voice = 0; voice < description->nVoices; ++voice)
		ngsInitVoiceInputMixers(system,
			rack != NULL ? &rack->voices[voice] : NULL,
			rack != NULL ? rack->voiceDsps[voice] : NULL,
			description, &coherentCursor, sizeOnly, voiceSize,
			stateTotal, inputMixerTotal);
	coherentCursor = (unsigned char *)
		ngsAlignMemoryAddress((uintptr_t)coherentCursor);
	dspHeapCursor = (unsigned char *)
		ngsAlignMemoryAddress((uintptr_t)dspHeapCursor);
	if (rack != NULL) {
		rack->statusMemory = dspHeapCursor;
		rack->statusMemorySize = coherentCursor -
			(unsigned char *)rack->voiceDspMemoryEnd;
	}
	for (voice = 0; voice < description->nVoices; ++voice)
		ngsAllocateVoiceDspStatus(system,
			rack != NULL ? rack->voiceDsps[voice] : NULL,
			description, &dspHeapCursor, sizeOnly);
	voiceList = ngsAllocateMemory(&dspHeapCursor,
		description->nVoices * sizeof(uintptr_t));
	voiceListCount = ngsAllocateMemory(&dspHeapCursor, sizeof(unsigned int));
	dspHeapCursor = (unsigned char *)
		ngsAlignMemoryAddress((uintptr_t)dspHeapCursor);
	if (rack == NULL) {
		if (callbackSize != NULL)
			*callbackSize = callbackCursor - callbackMemory;
		if (coherentSize != NULL)
			*coherentSize = coherentCursor - coherentMemory;
		if (dspHeapSize != NULL)
			*dspHeapSize = dspHeapCursor - dspHeapMemory;
		if (systemSize != NULL)
			*systemSize = systemCursor - systemMemory;
		return 0;
	}
	rack->voiceList = voiceList;
	rack->voiceListCount = voiceListCount;
	*voiceListCount = 0;
	rack->statusMemorySize2 = dspHeapCursor -
		(unsigned char *)rack->statusMemory;
	rack->dsp->voiceListPhysical = ngsGetPhysicalAddress(voiceList);
	rack->dsp->voiceCountPhysical =
		ngsGetPhysicalAddress(rack->voiceListCount);

	/* a codec engine handle for each codec module */
	for (codec = 0; codec < rack->codecModuleCount; ++codec) {
		index = rack->codecModules[codec];
		result = ngsGetModuleMemoryInfo(system,
			rack->moduleList.modules[index].moduleId,
			rack->channelsPerVoice, &info);
		if (result != 0)
			goto release;
		result = ngsFindSystemModule(system,
			rack->moduleList.modules[index].moduleId, &systemModule);
		if (result != 0)
			goto release;
		command[0] = ngsPlatformState.sharedPhysicalAddress;
		command[1] = systemModule->codecResourceId;
		ngsLockCodec(system->context);
		codecHandle = SceCodecEngineWrapperForDriver_391BD4FD(327,
			command, NULL, NULL);
		ngsUnlockCodec(system->context);
		if (codecHandle == 0) {
			result = SCE_NGS_ERROR;
			goto release;
		}
		for (voice = 0; voice < rack->voiceCount; ++voice) {
			dsp = rack->voiceDsps[voice];
			codecBuffer = &dsp->codecBuffers[codec];
			codecBuffer->codecHandle = codecHandle;
			codecBuffer->memoryPhysical = ngsGetPhysicalAddress(
				dsp->moduleMemory +
				dsp->rackDsp->layouts[index].contextOffset);
			codecBuffer->memorySize = info.extraMemorySize;
			codecBuffer->memoryOffset = info.contextSize;
		}
	}

	/* the voices, with their modules' default parameters */
	for (voice = 0; voice < description->nVoices; ++voice) {
		newHandle = ngsGetVoiceHandle(&rack->voices[voice]);
		if (ngsLockVoice(system->context) == 0) {
			rack->voices[voice].signature = NGS_VOICE_SIGNATURE;
			ngsUnlockVoice(system->context);
		}
		result = sceNgsVoiceInitInternal(newHandle, NULL,
			SCE_NGS_VOICE_INIT_ALL);
		if (result != 0) {
			if (ngsLockVoice(system->context) == 0) {
				rack->voices[voice].signature = 0;
				ngsUnlockVoice(system->context);
			}
			goto release;
		}
		params = rack->voiceDsps[voice]->params;
		for (index = 0; index < rack->moduleCount8; ++index)
			if (rack->paramsSizes[index] != 0)
				memcpy(params + rack->paramsOffsets[index],
				       rack->voices[voice].moduleParams[index].params,
				       rack->paramsSizes[index]);
	}

	/* link it with the system's other racks */
	links = rack->links;
	for (index = 0; index != system->MaxRacks; ++index) {
		links[index].rack = NULL;
		links[index].outputPatches = 0;
		links[index].inputPatches = 0;
	}
	for (index = 0; index != system->MaxRacks; ++index) {
		other = system->racks[index];
		if (other == NULL || index == rackIndex)
			continue;
		other->links[rackIndex].rack = rack;
		other->links[rackIndex].outputPatches = 0;
		other->links[rackIndex].inputPatches = 0;
		links[index].rack = system->racks[index];
		links[index].outputPatches = 0;
		links[index].inputPatches = 0;
	}

	if (coherentCursor - coherentMemory != rackCoherentSize ||
	    callbackCursor - callbackMemory != rackCallbackSize ||
	    dspHeapCursor - dspHeapMemory != rackDspHeapSize ||
	    systemCursor - systemMemory != rackSystemSize) {
		result = SCE_NGS_ERROR;
		goto release;
	}

	/* load the rack and its input mixers into the codec engine */
	definition = description->pVoiceDefn;
	modules = (const NgsVoiceModuleDefinition *)
		((const unsigned char *)definition + definition->modulesOffset);
	sceKernelDcacheCleanRange(rack->dsp, rack->dspSize);
	command[0] = ngsPlatformState.sharedPhysicalAddress;
	command[1] = system->systemId;
	command[2] = SceCodecEngineWrapperForDriver_C7C150AE(rack->dsp);
	command[3] = rack->dspSize;
	ngsLockCodec(system->context);
	result = SceCodecEngineWrapperForDriver_391BD4FD(323, command, NULL, NULL);
	if (result != 0) {
		if (system->compiledSdkVersion < 0x1800000 &&
		    result == SCE_NGS_ERROR_INTERNAL_ALLOC)
			result = SCE_NGS_ERROR_OUT_OF_ASSETS;
		system->racks[rack->index] = NULL;
		rack->signature = 0;
		goto unlockCodec;
	}
	for (index = 0; index < description->pVoiceDefn->moduleCount; ++index) {
		if (modules[index].moduleId != SCE_NGS_INPUT_MIXER_ID)
			continue;
		for (voice = 0; voice < description->nVoices; ++voice) {
			dsp = rack->voiceDsps[voice];
			layout = &dsp->rackDsp->layouts[index];
			command[0] = SceCodecEngineWrapperForDriver_C7C150AE(
				dsp->moduleMemory + layout->contextOffset);
			command[1] = layout->contextSize;
			result = SceCodecEngineWrapperForDriver_391BD4FD(325,
				command, NULL, NULL);
			if (result != 0)
				goto unload;
		}
	}
	ngsUnlockCodec(system->context);

	for (voice = 0; voice < description->nVoices; ++voice) {
		dsp = rack->voiceDsps[voice];
		ngsMapVoiceDsp(dsp);
		ngsCleanDcacheRange(dsp, dsp->size);
	}
	ngsCleanDcacheRange(rack->statusMemory, rack->statusMemorySize2);
	ngsLockTransfer(system->context);
	if (system->MaxRacks == 0)
		goto noProcessingSlot;
	slot = system->processingRacks;
	for (index = 0; *slot != NULL; ++slot)
		if (++index == system->MaxRacks)
			goto noProcessingSlot;
	*slot = rack;
	ngsUnlockTransfer(system->context);
	rack->state = NGS_RACK_STATE_ACTIVE;
	newHandle = ngsGetRackHandle(rack);
	result = ngsCopyToCaller(rackHandle, &newHandle, sizeof(newHandle),
		system->copyUserData);
	if (result == 0)
		result = ngsCopyToCaller(rack->callerBuffer, rack->callbackMemory,
			rack->callbackSize, system->copyUserData);
	return result;

noProcessingSlot:
	result = SCE_NGS_ERROR_OUT_OF_ASSETS;
	ngsUnlockTransfer(system->context);
	rack->signature = 0;
	goto free;

unload:
	if (system->compiledSdkVersion < 0x1800000 &&
	    result == SCE_NGS_ERROR_INTERNAL_ALLOC)
		result = SCE_NGS_ERROR_OUT_OF_ASSETS;
	failed = voice;
	for (count = 0; count != index; ++count) {
		if (modules[count].moduleId != SCE_NGS_INPUT_MIXER_ID)
			continue;
		for (voice = 0; voice < description->nVoices; ++voice)
			ngsFlushInputMixer(rack, voice, count);
	}
	for (voice = 0; voice != failed; ++voice)
		ngsFlushInputMixer(rack, voice, index);
	command[0] = 0;
	command[1] = 0;
	command[2] = SceCodecEngineWrapperForDriver_C7C150AE(rack->dsp);
	command[3] = rack->dspSize;
	SceCodecEngineWrapperForDriver_391BD4FD(324, command, NULL, NULL);
	system->racks[rack->index] = NULL;
	rack->signature = 0;
unlockCodec:
	ngsUnlockCodec(system->context);
	goto free;

outOfAssets:
	result = SCE_NGS_ERROR_OUT_OF_ASSETS;
	rack->signature = 0;
	goto free;

release:
	system->racks[rack->index] = NULL;
	rack->signature = 0;
	goto free;

fail:
	if (sizeOnly)
		return result;
free:
	ngsFreeProcessMemory(handle, coherentMemory, rackCoherentSize,
		dspHeapMemory, rackDspHeapSize, systemMemory, rackSystemSize,
		memBlock);
	return result;
}

int ngsGetRackMemorySize(SceNgsHSynSystem handle,
	const SceNgsRackDescription *description, SceSize *callbackSize,
	SceSize *coherentSize, SceSize *dspHeapSize, SceSize *systemSize)
{
	NgsSynSystem *system;
	const struct SceNgsVoiceDefinition *definition;
	const NgsVoiceModuleDefinition *module;
	NgsModuleMemoryInfo info = { 0 };
	SceSize callback = 0;
	SceSize coherent = 0;
	SceSize dspHeap = 0;
	SceSize systemMemory = 0;
	unsigned int voices;
	unsigned int index;
	int result;

	system = ngsGetSystemFromHandle(handle);
	if (system == NULL)
		return SCE_NGS_ERROR_INVALID_HANDLE;
	if (callbackSize == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;
	*callbackSize = 0;
	if (description == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;
	definition = description->pVoiceDefn;
	if (definition == NULL ||
	    (unsigned int)(description->nChannelsPerVoice - 1) > 1 ||
	    description->nVoices <= 0 || description->nPatchesPerOutput < 0 ||
	    description->nMaxPatchesPerInput < 0 ||
	    definition->magic !=
	    NGS_VOICE_DEFINITION_SIGNATURE)
		return SCE_NGS_ERROR_INVALID_PARAM;
	voices = 0;
	for (index = 0; index < system->MaxRacks; ++index) {
		if (system->racks[index] != NULL) {
			voices += system->racks[index]->voiceCount;
			if (voices > 256)
				return SCE_NGS_ERROR;
		}
	}
	if (description->nVoices + voices > system->MaxVoices)
		return SCE_NGS_ERROR_OUT_OF_ASSETS;
	if (definition->voiceOutputCount * description->nPatchesPerOutput > 256 ||
	    definition->moduleCount > 32 || definition->dspConfiguration > 4 ||
	    definition->routingIndexCount > 32)
		return SCE_NGS_ERROR_INVALID_PARAM;
	module = (const NgsVoiceModuleDefinition *)
		((const unsigned char *)definition + definition->modulesOffset);
	for (index = 0; index < description->pVoiceDefn->moduleCount;
	     ++index, ++module) {
		result = ngsGetModuleMemoryInfo(system, module->moduleId,
			description->nChannelsPerVoice, &info);
		if (result != 0)
			return result;
		result = ngsCheckModuleOutputs(system, module->moduleId,
			module->outputCount);
		if (result != 0)
			return result;
	}
	result = ngsBuildRack(handle, NULL, 0, description, NULL, &callback,
		&coherent, &dspHeap, &systemMemory);
	if (result != 0)
		return result;
	if (coherentSize == NULL)
		return ngsGetAlignedMemorySize(callback, coherent, dspHeap,
			systemMemory, callbackSize);
	*coherentSize = coherent;
	*callbackSize = callback;
	*dspHeapSize = dspHeap;
	*systemSize = systemMemory;
	return 0;
}

/* Bring the system's racks into processing order: a rack after those it feeds. */
static inline int ngsSwapDependentRack(NgsSynRack **racks, int index, int end)
{
	NgsSynRack *rack = racks[index];
	NgsSynRack *other;
	int next;

	if (rack == NULL) {
		if (racks[index + 1] == NULL)
			return 0;
		racks[index] = racks[index + 1];
		racks[index + 1] = NULL;
		return 1;
	}
	for (next = index + 1; next < end; ++next) {
		other = racks[next];
		if (other != NULL && rack->depthScratch[other->index] > 0) {
			racks[index] = other;
			racks[next] = rack;
			return 1;
		}
	}
	return 0;
}

int ngsSortRacks(NgsSynSystem *system)
{
	NgsSynRack *rack;
	int *depth;
	int count;
	int index;
	int start;
	int end;
	int sorted;

	ngsLockTransfer(system->context);
	for (index = 0; index < (int)system->MaxRacks; ++index) {
		rack = system->racks[index];
		if (rack == NULL)
			continue;
		count = system->MaxRacks;
		depth = rack->depthScratch;
		if (count != 0)
			memset(depth, 0, count * sizeof(*depth));
		ngsCountRackDepth(&rack->links, count, depth);
	}
	count = system->MaxRacks;
	start = 0;
	end = count - 1;
	for (;;) {
		sorted = 1;
		for (index = start; index < end; ++index)
			if (ngsSwapDependentRack(system->processingRacks, index,
						 end + 1))
				sorted = 0;
		for (index = end - 1; index >= start; --index)
			if (ngsSwapDependentRack(system->processingRacks, index,
						 end))
				sorted = 0;
		if (sorted)
			break;
		++start;
		--end;
	}
	ngsUnlockTransfer(system->context);
	return 0;
}

inline int ngsFlushInputMixer(NgsSynRack *rack, unsigned int voiceIndex,
		       unsigned int moduleIndex)
{
	NgsVoiceDsp *dsp;
	NgsDspModuleLayout *layout;
	unsigned int command[2];
	unsigned char *context;
	SceSize size;

	if (rack == NULL)
		return SCE_NGS_ERROR;
	dsp = rack->voiceDsps[voiceIndex];
	layout = &dsp->rackDsp->layouts[moduleIndex];
	context = dsp->moduleMemory + layout->contextOffset;
	size = layout->contextSize;
	command[0] = SceCodecEngineWrapperForDriver_C7C150AE(context);
	command[1] = size;
	ngsCleanDcacheRange(context, size);
	return SceCodecEngineWrapperForDriver_391BD4FD(326, command, NULL, NULL);
}

void ngsRemoveRack(NgsSynSystem *system, unsigned int index, NgsSynRack *rack)
{
	NgsSynRack *other;
	unsigned int command[4];
	unsigned int count;
	unsigned int link;
	unsigned int module;
	unsigned int voice;

	system->racks[index] = NULL;
	for (index = 0; index != system->MaxRacks; ++index) {
		other = system->racks[index];
		if (other == NULL)
			continue;
		for (link = 0; link != system->MaxRacks; ++link) {
			if (other->links[link].rack == rack) {
				other->links[link].rack = NULL;
				break;
			}
		}
	}
	ngsLockTransfer(system->context);
	count = system->MaxRacks;
	for (index = 0; index != count; ++index)
		if (system->processingRacks[index] == rack)
			break;
	if (index != count) {
		for (; index < count - 1; ++index)
			system->processingRacks[index] =
				system->processingRacks[index + 1];
		system->processingRacks[count - 1] = NULL;
	}
	ngsUnlockTransfer(system->context);
	ngsLockCodec(system->context);
	for (module = 0; module < rack->moduleList.moduleCount; ++module) {
		if (rack->moduleList.modules[module].moduleId != SCE_NGS_INPUT_MIXER_ID)
			continue;
		for (voice = 0; voice < rack->voiceCount; ++voice)
			ngsFlushInputMixer(rack, voice, module);
	}
	command[0] = 0;
	command[1] = 0;
	command[2] = SceCodecEngineWrapperForDriver_C7C150AE(rack->dsp);
	command[3] = rack->dspSize;
	SceCodecEngineWrapperForDriver_391BD4FD(324, command, NULL, NULL);
	ngsUnlockCodec(system->context);
	rack->signature = 0;
	ngsFreeProcessMemory(ngsGetSystemHandle(system), rack->coherentMemory,
		rack->coherentSize, rack->dspHeapMemory, rack->dspHeapSize,
		rack->systemMemory, rack->systemSize, rack->memBlock);
}

int ngsGetRackDepth(NgsSynRack *rack, NgsSynRack *other,
		    unsigned int rackCount, int *depth)
{
	if (rackCount != 0)
		memset(depth, 0, rackCount * sizeof(*depth));
	ngsCountRackDepth(&rack->links, rackCount, depth);
	return depth[other->index];
}

int ngsAddRackPatch(NgsSynRack *source, NgsSynRack *destination)
{
	NgsRackLink *output = &source->links[destination->index];
	NgsRackLink *input = &destination->links[source->index];

	++output->outputPatches;
	++input->inputPatches;
	return 0;
}

int ngsRemoveRackPatch(NgsSynRack *source, NgsSynRack *destination)
{
	NgsRackLink *output = &source->links[destination->index];
	NgsRackLink *input = &destination->links[source->index];

	--output->outputPatches;
	--input->inputPatches;
	return 0;
}

/* The DSP heap below the rack's part of it moved down by releasedSize. */
void ngsRelocateRackMemory(NgsSynRack *rack, void *releasedAddress,
			   SceSize releasedSize)
{
	NgsRackDsp *rackDsp;
	NgsVoiceDsp *dsp;
	NgsDspPatchBuffer *buffer;
	unsigned int voice;
	unsigned int patch;

	if (rack == NULL || (uintptr_t)rack->dspHeapMemory <
	    (uintptr_t)releasedAddress + releasedSize)
		return;
	if (rack->dspRackList->voices != NULL)
		rack->dspRackList->voices = (unsigned int *)
			((unsigned char *)rack->dspRackList->voices - releasedSize);
	rack->dspRackList->voicesPhysical =
		ngsGetPhysicalAddress(rack->dspRackList->voices);
	if (rack->dspRackList != NULL)
		rack->dspRackList = (NgsDspRackEntry *)
			((unsigned char *)rack->dspRackList - releasedSize);
	if (rack->voiceDspMemory != NULL)
		rack->voiceDspMemory = (unsigned char *)rack->voiceDspMemory -
			releasedSize;
	if (rack->statusMemory != NULL)
		rack->statusMemory = (unsigned char *)rack->statusMemory -
			releasedSize;
	if (rack->voiceList != NULL)
		rack->voiceList = (uintptr_t *)
			((unsigned char *)rack->voiceList - releasedSize);
	if (rack->voiceListCount != NULL)
		rack->voiceListCount = (unsigned int *)
			((unsigned char *)rack->voiceListCount - releasedSize);
	rackDsp = rack->dsp;
	rackDsp->voiceListPhysical = ngsGetPhysicalAddress(rack->voiceList);
	rackDsp = rack->dsp;
	rackDsp->voiceCountPhysical = ngsGetPhysicalAddress(rack->voiceListCount);
	for (voice = 0; voice < rack->voiceCount; ++voice) {
		dsp = rack->voiceDsps[voice];
		buffer = dsp->patchBuffers;
		for (patch = 0; patch < rack->patchesPerVoice; ++patch, ++buffer) {
			if (buffer->buffer != NULL)
				buffer->buffer = (unsigned char *)buffer->buffer -
					releasedSize;
			buffer->bufferPhysical =
				ngsGetPhysicalAddress(buffer->buffer);
		}
		if (dsp->patchBuffers != NULL)
			dsp->patchBuffers = (NgsDspPatchBuffer *)
				((unsigned char *)dsp->patchBuffers - releasedSize);
		if (dsp->inputs != NULL)
			dsp->inputs -= releasedSize;
		if (dsp->outputs != NULL)
			dsp->outputs -= releasedSize;
		if (dsp->params != NULL)
			dsp->params -= releasedSize;
		if (dsp->status != NULL)
			dsp->status = (NgsVoiceDspStatus *)
				((unsigned char *)dsp->status - releasedSize);
		if (dsp->codecBuffers != NULL)
			dsp->codecBuffers = (NgsDspCodecBuffer *)
				((unsigned char *)dsp->codecBuffers - releasedSize);
		if (dsp->patchBuffersPhysical != 0)
			dsp->patchBuffersPhysical -= releasedSize;
		if (dsp->inputsPhysical != 0)
			dsp->inputsPhysical -= releasedSize;
		if (dsp->outputsPhysical != 0)
			dsp->outputsPhysical -= releasedSize;
		if (dsp->paramsPhysical != 0)
			dsp->paramsPhysical -= releasedSize;
		if (dsp->statusPhysical != 0)
			dsp->statusPhysical -= releasedSize;
		if (rack->voiceDsps[voice] != NULL)
			rack->voiceDsps[voice] = (NgsVoiceDsp *)
				((unsigned char *)rack->voiceDsps[voice] -
				 releasedSize);
	}
	if (rack->dspHeapMemory != NULL)
		rack->dspHeapMemory = (unsigned char *)rack->dspHeapMemory -
			releasedSize;
}

SceInt32 sceNgsRackGetRequiredMemorySizeInternal(SceNgsHSynSystem handle,
	const SceNgsRackDescription *description, SceUInt32 *size)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system = ngsGetSystemFromHandle(handle);
	SceNgsRackDescription local;
	struct SceNgsVoiceDefinition header;
	const struct SceNgsVoiceDefinition *definition;
	unsigned int encoded;
	SceSize required = 0;
	int result;

	if (system == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto leave;
	}
	if (description == NULL ||
	    ngsCopyFromCaller(&local, description, sizeof(local),
			      system->copyUserData) != 0 ||
	    ngsCopyToCaller(size, &required, sizeof(required),
			    system->copyUserData) != 0)
		goto invalid;
	encoded = (unsigned int)local.pVoiceDefn;
	if (encoded == 0)
		goto invalid;
	definition = (const struct SceNgsVoiceDefinition *)
		(encoded ^ NGS_VOICE_DEFINITION_KEY);
	local.pVoiceDefn = definition;
	if (encoded == NGS_VOICE_DEFINITION_KEY ||
	    !ngsIsRangeReadable(definition, sizeof(header)) ||
	    ngsCopyFromCaller(&header, definition, sizeof(header), 0) != 0 ||
	    header.magic != NGS_VOICE_DEFINITION_SIGNATURE ||
	    header.size == 0 || !ngsIsRangeReadable(local.pVoiceDefn, header.size))
		goto invalid;
	result = ngsGetRackMemorySize(handle, &local, &required, NULL, NULL,
		NULL);
	if (result == 0)
		result = ngsCopyToCaller(size, &required, sizeof(required),
			system->copyUserData);
leave:
	sceSyscallLeave(syscall);
	return result;

invalid:
	result = SCE_NGS_ERROR_INVALID_PARAM;
	goto leave;
}

SceInt32 sceNgsRackInitInternal(SceNgsHSynSystem handle,
	SceNgsBufferInfo *buffer, const SceNgsRackDescription *description,
	SceNgsHRack *rackHandle)
{
	unsigned int syscall = sceSyscallEnter();
	NgsProcessStorage *storage;
	NgsSynSystem *system;
	SceNgsBufferInfo memory;
	SceNgsRackDescription local;
	struct SceNgsVoiceDefinition header;
	const struct SceNgsVoiceDefinition *definition;
	const NgsVoiceModuleDefinition *modules;
	SceNgsHRack none = 0;
	unsigned int encoded;
	unsigned int inputMixers;
	unsigned int index;
	int result;

	result = SceCodecEngineWrapperForDriver_CB8703D8();
	if (result < 0)
		goto leave;
	system = ngsGetSystemFromHandle(handle);
	storage = NULL;
	if (ngsGetProcessStorage(&storage) != 0 || ngsLockProcess(storage) != 0) {
		result = SCE_NGS_ERROR;
		goto unlock;
	}
	if (system == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto unlockProcess;
	}
	if (ngsCopyFromCaller(&memory, buffer, sizeof(memory),
			      system->copyUserData) != 0 ||
	    ngsCopyFromCaller(&local, description, sizeof(local),
			      system->copyUserData) != 0 ||
	    ngsCopyToCaller(rackHandle, &none, sizeof(none),
			    system->copyUserData) != 0)
		goto invalid;
	encoded = (unsigned int)local.pVoiceDefn;
	if (encoded == 0)
		goto invalid;
	definition = (const struct SceNgsVoiceDefinition *)
		(encoded ^ NGS_VOICE_DEFINITION_KEY);
	local.pVoiceDefn = definition;
	if (encoded == NGS_VOICE_DEFINITION_KEY ||
	    !ngsIsRangeReadable(definition, sizeof(header)) ||
	    ngsCopyFromCaller(&header, definition, sizeof(header), 0) != 0 ||
	    header.magic != NGS_VOICE_DEFINITION_SIGNATURE ||
	    header.size == 0 ||
	    !ngsIsCallerRangeAccessible(memory.data, memory.size,
					system->copyUserData))
		goto invalid;
	/* these two keep the process locked, as Sony's do */
	if (!ngsIsRangeReadable(local.pVoiceDefn, header.size)) {
		result = SCE_NGS_ERROR_INVALID_PARAM;
		goto unlock;
	}
	if (system->compiledSdkVersion > 0x3000061) {
		inputMixers = 0;
		modules = (const NgsVoiceModuleDefinition *)
			((const unsigned char *)local.pVoiceDefn +
			 local.pVoiceDefn->modulesOffset);
		for (index = 0; index != local.pVoiceDefn->moduleCount; ++index)
			if (modules[index].moduleId == SCE_NGS_INPUT_MIXER_ID)
				++inputMixers;
		if (inputMixers != 0 && local.nMaxPatchesPerInput == 0) {
			result = SCE_NGS_ERROR_INVALID_PARAM;
			goto unlock;
		}
	}
	result = ngsLockSystemMutex(system->context);
	if (result != 0)
		goto unlockProcess;
	result = ngsBuildRack(handle, memory.data, memory.size, &local,
		rackHandle, NULL, NULL, NULL, NULL);
	ngsUnlockSystemMutex(system->context);
	if (ngsUnlockProcess(storage) != 0 && result == 0)
		result = SCE_NGS_ERROR;
	goto unlock;

invalid:
	result = SCE_NGS_ERROR_INVALID_PARAM;
unlockProcess:
	ngsUnlockProcess(storage);
unlock:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsRackGetVoiceHandleInternal(SceNgsHRack rackHandle,
	const SceUInt32 index, SceNgsHVoice *voiceHandle)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynRack *rack;
	NgsSynSystem *system;
	SceNgsHVoice handle = 0;
	int result = 0;

	rack = ngsGetRackFromHandle(rackHandle);
	if (rack == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	} else {
		system = rack->system;
		if (voiceHandle == NULL || system == NULL) {
			result = SCE_NGS_ERROR_INVALID_PARAM;
		} else {
			if (index < rack->voiceCount &&
			    rack->state == NGS_RACK_STATE_ACTIVE)
				handle = ngsGetVoiceHandle(&rack->voices[index]);
			else
				result = SCE_NGS_ERROR_INVALID_PARAM;
			if (ngsCopyToCaller(voiceHandle, &handle, sizeof(handle),
					    system->copyUserData) != 0)
				result = SCE_NGS_ERROR_INVALID_PARAM;
		}
	}
	sceSyscallLeave(syscall);
	return result;
}

int ngsReleaseRack(SceNgsHRack handle, SceNgsRackReleaseCallbackFunc callback)
{
	NgsSynRack *rack;
	NgsSynRack *other;
	NgsSynSystem *system;
	NgsPatch *patch;
	unsigned int index;
	unsigned int link;
	unsigned int voice;
	unsigned int count;
	unsigned int patchIndex;
	int result;

	rack = ngsGetRackFromHandle(handle);
	if (rack == NULL)
		return SCE_NGS_ERROR_INVALID_HANDLE;
	system = rack->system;
	if (system == NULL || system->signature != 54321 ||
	    system->MaxRacks == 0)
		return SCE_NGS_ERROR_INVALID_PARAM;
	for (index = 0; system->racks[index] != rack; )
		if (++index == system->MaxRacks)
			return SCE_NGS_ERROR_INVALID_PARAM;
	rack = system->racks[index];
	if (rack->state == NGS_RACK_STATE_RELEASING)
		return SCE_NGS_ERROR;
	ngsLockSystemMutex(system->context);
	rack->state = NGS_RACK_STATE_RELEASING;
	for (voice = 0; voice < rack->voiceCount; ++voice) {
		if ((unsigned int)(rack->voices[voice].playState.runState - 1) <= 1)
			sceNgsVoiceKillInternal(ngsGetVoiceHandle(&rack->voices[voice]));
		rack->voices[voice].playState.runState = NGS_VOICE_RUN_STATE_UNLOADING;
		ngsUpdateVoiceFlags(&rack->voices[voice], rack);
	}
	for (link = 0; link < system->MaxRacks; ++link) {
		if (rack->links[link].outputPatches > 0) {
			other = rack->links[link].rack;
			for (voice = 0; voice < rack->voiceCount; ++voice) {
				count = rack->patchesPerOutput * rack->voiceOutputCount;
				for (patchIndex = 0; patchIndex != count; ++patchIndex) {
					patch = &rack->voices[voice].patches[patchIndex];
					if (patch->destination != NULL &&
					    patch->destination->rack == other)
						sceNgsPatchRemoveRoutingInternal(
							ngsGetPatchHandle(patch));
				}
			}
		}
		if (rack->links[link].inputPatches > 0) {
			other = rack->links[link].rack;
			for (voice = 0; voice < other->voiceCount; ++voice) {
				count = other->patchesPerOutput * other->voiceOutputCount;
				for (patchIndex = 0; patchIndex != count; ++patchIndex) {
					patch = &other->voices[voice].patches[patchIndex];
					if (patch->destination != NULL &&
					    patch->destination->rack == rack)
						sceNgsPatchRemoveRoutingInternal(
							ngsGetPatchHandle(patch));
				}
			}
		}
	}
	if (ngsLockVoice(system->context) == 0) {
		for (voice = 0; voice != rack->voiceCount; ++voice)
			rack->voices[voice].signature = 0;
		ngsUnlockVoice(system->context);
	}
	if (callback != NULL) {
		result = ngsLockVoice(system->context);
		if (result == 0) {
			rack->releaseCallback = callback;
			++system->pendingRackReleases;
			rack->signature = 0;
			ngsUnlockVoice(system->context);
		}
	} else {
		result = ngsLockVoice(system->context);
		if (result == 0) {
			ngsRemoveRack(system, index, rack);
			ngsUnlockVoice(system->context);
		}
	}
	ngsUnlockSystemMutex(system->context);
	return result;
}

SceInt32 sceNgsRackReleaseInternal(SceNgsHRack handle,
	const SceNgsRackReleaseCallbackFunc callback)
{
	unsigned int syscall = sceSyscallEnter();
	NgsProcessStorage *storage;
	int result;

	result = SceCodecEngineWrapperForDriver_CB8703D8();
	if (result >= 0) {
		storage = NULL;
		if (ngsGetProcessStorage(&storage) != 0 ||
		    ngsLockProcess(storage) != 0) {
			result = SCE_NGS_ERROR;
		} else {
			result = ngsReleaseRack(handle, callback);
			if (ngsUnlockProcess(storage) != 0 && result == 0)
				result = SCE_NGS_ERROR;
		}
	}
	_unlockProcessSuspendCore();
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsRackSetParamErrorCallbackInternal(SceNgsHRack handle,
	const SceNgsParamsErrorCallbackFunc callback)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynRack *rack = ngsGetRackFromHandle(handle);
	int result;

	if (rack != NULL) {
		rack->paramErrorCallback = callback;
		result = 0;
	} else {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	}
	sceSyscallLeave(syscall);
	return result;
}
