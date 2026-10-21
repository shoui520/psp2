/* Racks: creation, the processing order of the system's racks, release. */

#include <scetypes.h>
#include <stddef.h>
#include <string.h>
#include <intrmgr/syscall.h>
#include <vnz_wrapper/vnz_wrapper.h>

#include "ngs_platform.h"
#include "ngs_system.h"
#include "ngs_rack.h"

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

	if (rack == NULL)
		return SCE_NGS_ERROR;
	dsp = rack->voiceDsps[voiceIndex];
	layout = &dsp->rackDsp->layouts[moduleIndex];
	context = dsp->moduleMemory + layout->contextOffset;
	command[0] = SceCodecEngineWrapperForDriver_C7C150AE(context);
	command[1] = layout->contextSize;
	ngsCleanDcacheRange(context, layout->contextSize);
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
	++source->links[destination->index].outputPatches;
	++destination->links[source->index].inputPatches;
	return 0;
}

int ngsRemoveRackPatch(NgsSynRack *source, NgsSynRack *destination)
{
	--source->links[destination->index].outputPatches;
	--destination->links[source->index].inputPatches;
	return 0;
}

/* The DSP heap below the rack's part of it moved down by releasedSize. */
void ngsRelocateRackMemory(NgsSynRack *rack, void *releasedAddress,
			   SceSize releasedSize)
{
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
	rack->dsp->voiceListPhysical = ngsGetPhysicalAddress(rack->voiceList);
	rack->dsp->voiceCountPhysical =
		ngsGetPhysicalAddress(rack->voiceListCount);
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
			dsp->status = (unsigned char *)dsp->status - releasedSize;
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
