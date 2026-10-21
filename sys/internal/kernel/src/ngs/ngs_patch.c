/* Patches: routes from a voice output into another voice's input mixer. */

#include <scetypes.h>
#include <stddef.h>
#include <string.h>
#include <intrmgr/syscall.h>

#include "ngs_platform.h"
#include "ngs_system.h"
#include "ngs_rack.h"

SceNgsHPatch ngsConnectPatch(NgsVoice *source, unsigned int index,
	NgsVoice *destination, unsigned int inputIndex,
	NgsSynRack *destinationRack, NgsSynRack *sourceRack)
{
	NgsVoiceInput *input = &destination->inputs[inputIndex];
	NgsPatch *patch;

	if (input->patchCount >= input->maxPatches)
		return 0;
	patch = &source->patches[index];
	patch->reserved23 = destination->channels;
	patch->destination = destination;
	patch->inputIndex = inputIndex;
	source->dirtyFlags |= 1;
	source->dirtyFlags2 |= 1;
	source->patchDirtyMask |= 1 << index;
	patch->reserved4 = input->reserved12;
	patch->reserved8 = input->reserved16;
	memset(patch->volumes, 0, 8 * source->channels);
	input->patches[input->patchCount] = patch;
	++input->patchCount;
	ngsAddRackPatch(sourceRack, destinationRack);
	return ngsGetPatchHandle(patch);
}

/* Show the patch to the caller: its setup and its channel count. */
int ngsPublishPatch(unsigned int voiceIndex, NgsSynRack *rack,
	unsigned int patchIndex, unsigned int channels,
	const SceNgsPatchSetupInfo *setup)
{
	unsigned int slot = patchIndex + rack->patchesPerVoice * voiceIndex;
	SceNgsPatchSetupInfo *record = &rack->patchSetups[slot];
	int result;

	*record = *setup;
	if (rack->patchesPerOutput <= 1)
		record->nSourceOutputSubIndex = 0;
	else
		record->nSourceOutputSubIndex =
			patchIndex % rack->patchesPerOutput;
	rack->patchChannels[slot] = channels;
	result = ngsCopyToCaller(&rack->patchSetupsCaller[slot], record,
		sizeof(*record), rack->system->copyUserData);
	if (result == 0)
		result = ngsCopyToCaller(&rack->patchChannelsCaller[slot],
			&rack->patchChannels[slot], sizeof(unsigned int),
			rack->system->copyUserData);
	return result;
}

SceInt32 sceNgsPatchCreateRoutingInternal(const SceNgsPatchSetupInfo *info,
	SceNgsHPatch *handle)
{
	unsigned int syscall = sceSyscallEnter();
	NgsProcessStorage *storage;
	NgsSynSystem *system;
	NgsSynRack *sourceRack;
	NgsSynRack *destinationRack;
	NgsVoice *source;
	NgsVoice *destination;
	SceNgsPatchSetupInfo setup;
	SceNgsHPatch patch = 0;
	int copyUserData;
	int inputIndex;
	int patchesPerOutput;
	int subIndex;
	unsigned int index;
	int result;

	copyUserData = ngsGetSystemInstanceCount();
	storage = NULL;
	if (ngsGetProcessStorage(&storage) != 0 || ngsLockProcess(storage) != 0) {
		result = SCE_NGS_ERROR;
		goto leave;
	}
	if (info == NULL || handle == NULL ||
	    ngsCopyToCaller(handle, &patch, sizeof(patch), copyUserData) != 0 ||
	    ngsCopyFromCaller(&setup, info, sizeof(setup), copyUserData) != 0)
		goto invalid;
	source = ngsGetVoiceFromHandle(setup.hVoiceSource);
	destination = ngsGetVoiceFromHandle(setup.hVoiceDestination);
	inputIndex = setup.nTargetInputIndex;
	if (destination == NULL || source == NULL ||
	    destination->signature != NGS_VOICE_SIGNATURE ||
	    source->signature != NGS_VOICE_SIGNATURE) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto unlock;
	}
	sourceRack = source->rack;
	destinationRack = destination->rack;
	system = destinationRack->system;
	if (system != sourceRack->system) {
		result = SCE_NGS_ERROR_SYSTEM_MISMATCH;
		goto unlock;
	}
	if (inputIndex < 0 || setup.nSourceOutputIndex < 0 ||
	    setup.nSourceOutputSubIndex < -1)
		goto invalid;
	patchesPerOutput = sourceRack->patchesPerOutput;
	if (setup.nSourceOutputSubIndex >= patchesPerOutput)
		goto invalid;
	if (setup.nSourceOutputIndex >= sourceRack->voiceOutputCount ||
	    inputIndex >= destinationRack->inputMixerModuleCount) {
		result = SCE_NGS_ERROR_INVALID_VOICE_TYPE;
		goto unlock;
	}
	if (source->playState.runState == NGS_VOICE_RUN_STATE_UNLOADING ||
	    destination->playState.runState == NGS_VOICE_RUN_STATE_UNLOADING)
		goto error;
	if (setup.nSourceOutputSubIndex == SCE_NGS_VOICE_PATCH_AUTO_SUBINDEX) {
		for (subIndex = 0; subIndex != patchesPerOutput; ++subIndex) {
			index = ngsGetPatchIndex(patchesPerOutput,
				setup.nSourceOutputIndex, subIndex);
			if (source->patches[index].destination == NULL)
				goto found;
		}
		goto outOfAssets;
	}
	index = ngsGetPatchIndex(patchesPerOutput, setup.nSourceOutputIndex,
		setup.nSourceOutputSubIndex);
	if (source->patches[index].destination != NULL)
		goto error;
found:
	if (sourceRack->links[destinationRack->index].outputPatches <= 0) {
		if (ngsGetRackDepth(sourceRack, destinationRack, system->MaxRacks,
				    system->rackDepths) != 0 ||
		    destinationRack == sourceRack) {
			result = SCE_NGS_ERROR_DEPENDENCY;
			goto unlock;
		}
		patch = ngsConnectPatch(source, index, destination, inputIndex,
			destinationRack, sourceRack);
		result = ngsCopyToCaller(handle, &patch, sizeof(patch),
			system->copyUserData);
		if (result != 0)
			goto unlock;
		if (patch == 0)
			goto outOfAssets;
		result = ngsSortRacks(system);
		if (result != 0)
			goto unlock;
	} else {
		patch = ngsConnectPatch(source, index, destination, inputIndex,
			destinationRack, sourceRack);
		result = ngsCopyToCaller(handle, &patch, sizeof(patch),
			system->copyUserData);
		if (result != 0)
			goto unlock;
		if (patch == 0)
			goto outOfAssets;
	}
	result = ngsPublishPatch(source->index, source->rack, index,
		destination->channels, &setup);
	if (ngsUnlockProcess(storage) != 0 && result == 0)
		result = SCE_NGS_ERROR;
	goto leave;

invalid:
	result = SCE_NGS_ERROR_INVALID_PARAM;
	goto unlock;
error:
	result = SCE_NGS_ERROR;
	goto unlock;
outOfAssets:
	result = SCE_NGS_ERROR_OUT_OF_ASSETS;
unlock:
	ngsUnlockProcess(storage);
leave:
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsPatchRemoveRoutingInternal(SceNgsHPatch handle)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynRack *rack;
	NgsVoice *source = NULL;
	NgsVoice *destination;
	NgsVoiceInput *input;
	NgsPatch *patch;
	unsigned int index;
	unsigned int count;
	unsigned int last;
	unsigned int slot;
	int result;

	patch = ngsGetPatchFromHandle(handle, &source);
	if (patch == NULL || source == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto leave;
	}
	rack = source->rack;
	count = rack->patchesPerOutput * rack->voiceOutputCount;
	for (index = 0; index != count; ++index)
		if (patch == &source->patches[index] && patch->destination != NULL)
			break;
	if (index == count) {
		result = SCE_NGS_ERROR;
		goto leave;
	}
	destination = patch->destination;
	input = &destination->inputs[patch->inputIndex];
	for (last = 0; last != input->patchCount; ++last) {
		if (input->patches[last] != patch)
			continue;
		input->patches[last] = input->patches[input->patchCount - 1];
		destination->inputs[patch->inputIndex].patches[input->patchCount - 1] =
			NULL;
		--destination->inputs[patch->inputIndex].patchCount;
		patch->destination = NULL;
		patch->reserved4 = 0;
		patch->reserved8 = 0;
		source->patchDirtyMask |= 1 << index;
		source->dirtyFlags |= 1;
		source->dirtyFlags2 |= 1;
		ngsRemoveRackPatch(source->rack, destination->rack);
		rack = source->rack;
		break;
	}
	source->dirtyFlags |= 1;
	source->dirtyFlags2 |= 1;
	source->patchDirtyMask |= 1 << patch->index;
	slot = patch->index + rack->patchesPerVoice * source->index;
	rack->patchChannels[slot] = 0;
	result = ngsCopyToCaller(&rack->patchChannelsCaller[slot],
		&rack->patchChannels[slot], sizeof(unsigned int),
		rack->system->copyUserData);
leave:
	sceSyscallLeave(syscall);
	return result;
}

/*
 * Count, for every rack, how many chains of patches lead from it into the
 * racks that feed this one.
 */
inline void ngsCountRackDepth(NgsRackLink **links, unsigned int rackCount, int *depth)
{
	unsigned int index;

	for (index = 0; index != rackCount; ++index) {
		if ((*links)[index].inputPatches > 0) {
			++depth[index];
			ngsCountRackDepth(&(*links)[index].rack->links, rackCount,
				depth);
		}
	}
}
