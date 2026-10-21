/* Physical address translation and checked kernel/user buffer access. */

#include <stdint.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysclib.h>
#include <intrmgr/syscall.h>

#include "ngs_platform.h"
#include "ngs_system.h"
#include "ngs_voice_definition.h"
#include "ngs_rack.h"

struct SceNgsVoicePreset;

int ngsCopyToCaller(void *destination, const void *source, SceSize size,
		    int copyUserData);
int ngsIsCallerRangeAccessible(const void *address, SceSize size, int copyUserData);

SceInt32 sceNgsVoiceDefinitionGetPresetInternal(
	const struct SceNgsVoiceDefinition *definition, SceUInt32 index,
	const struct SceNgsVoicePreset **presets)
{
	unsigned int syscallState = sceSyscallEnter();
	int copyUserData = ngsGetSystemInstanceCount();
	const NgsVoicePresetDefinition *definitions;
	int result;

	if (definition == NULL || presets == NULL ||
	    ngsIsCallerRangeAccessible(presets, sizeof(*presets), copyUserData) == 0)
		goto invalid;
	definitions = (const NgsVoicePresetDefinition *)
		((const unsigned char *)definition + definition->presetsOffset);
	if (definition->presetCount <= index)
		goto invalidIndex;
	result = ngsCopyToCaller(presets, definitions + index,
					sizeof(*presets), copyUserData);
out:
	sceSyscallLeave(syscallState);
	return result;

invalid:
	result = SCE_NGS_ERROR_INVALID_PARAM;
	sceSyscallLeave(syscallState);
	return result;

invalidIndex:
	result = (int)0x804a0009;
	goto out;
}

uintptr_t ngsGetPhysicalAddress(const void *address)
{
	SceKernelVARange virtualRange;
	SceKernelPARange physicalRange;

	if (address == NULL)
		return 0;
	virtualRange.addr = (uintptr_t)address;
	virtualRange.size = 4;
	if (SceSysmemForDriver_AE36C775(&virtualRange, &physicalRange) != 0)
		return 0;
	return physicalRange.addr;
}

int ngsCopyToCaller(void *destination, const void *source, SceSize size,
		    int copyUserData)
{
	if (size != 0) {
		if ((uintptr_t)destination >= (uintptr_t)destination + size)
			return SCE_NGS_ERROR_INVALID_PARAM;
		if (copyUserData == 1)
			return sceKernelCopyToUser(destination, source, size);
		memcpy(destination, source, size);
	}
	return 0;
}

int ngsCopyFromCaller(void *destination, const void *source, SceSize size,
		      int copyUserData)
{
	if (size == 0)
		return 0;
	if ((uintptr_t)source >= (uintptr_t)source + size)
		return SCE_NGS_ERROR_INVALID_PARAM;
	if (copyUserData == 1)
		return sceKernelCopyFromUser(destination, source, size);
	memcpy(destination, source, size);
	return 0;
}

int ngsIsCallerRangeAccessible(const void *address, SceSize size, int copyUserData)
{
	int result;

	if (copyUserData == 1) {
		result = sceKernelIsAccessibleRange(0x30, address, size);
		if (result != 0)
			goto error;
	} else {
		result = sceKernelIsAccessibleRange(3, address, size);
		if (result != 0)
			goto error;
	}
	return 1;

error:
	SceDebugForDriver_391B74B7("res = %d code = %d\n", result,
			(int)SCE_KERNEL_ERROR_SYSMEM_ADDRESS_SPACE_ERROR);
	return 0;
}

int ngsIsRangeReadable(const void *address, SceSize size)
{
	return sceKernelIsAccessibleRange(1, address, size) == 0;
}

unsigned int ngsGetSystemHandle(NgsSynSystem *system)
{
	NgsProcessStorage *storage = NULL;
	unsigned int result = ngsGetProcessStorage(&storage);

	if (result != 0) {
		result = 0;
	} else if (storage->system == system && system->signature == 54321) {
		if (storage->systemId >= 0)
			result = ((unsigned int)storage->systemId << 29) | 0x10fabfab;
	}
	return result;
}

NgsSynSystem *ngsGetSystemFromHandle(unsigned int handle)
{
	NgsProcessStorage *storage = NULL;
	NgsSynSystem *system;

	if ((handle & 0x1fffffff) != 0x10fabfab ||
	    ngsGetProcessStorage(&storage) != 0 ||
	    (unsigned int)storage->systemId != handle >> 29 ||
	    (system = storage->system) == NULL || system->signature != 54321)
		return NULL;
	return system;
}

SceNgsHRack ngsGetRackHandle(const NgsSynRack *rack)
{
	if (rack == NULL || rack->system == NULL)
		return 0;
	return rack->system->systemId << 29 | rack->index << 23 |
		NGS_RACK_HANDLE_TAG;
}

NgsSynRack *ngsGetRackFromHandle(SceNgsHRack handle)
{
	NgsProcessStorage *storage = NULL;
	NgsSynSystem *system;
	NgsSynRack *rack;
	unsigned int index;

	ngsGetProcessStorage(&storage);
	if ((handle & 0x7fffff) != NGS_RACK_HANDLE_TAG || storage == NULL)
		return NULL;
	system = storage->system;
	if (system == NULL || (unsigned int)storage->systemId != handle >> 29)
		return NULL;
	index = handle >> 23 & 0x3f;
	if (index >= system->MaxRacks)
		return NULL;
	rack = system->racks[index];
	if (rack != NULL && rack->signature != NGS_RACK_SIGNATURE)
		return NULL;
	return rack;
}

SceNgsHVoice ngsGetVoiceHandle(const NgsVoice *voice)
{
	NgsSynRack *rack;

	if (voice == NULL)
		return 0;
	rack = voice->rack;
	if (rack == NULL || rack->system == NULL)
		return 0;
	return rack->index << 23 | voice->index << 15 | NGS_VOICE_HANDLE_TAG |
		rack->system->systemId << 29;
}

NgsVoice *ngsGetVoiceFromHandle(SceNgsHVoice handle)
{
	NgsProcessStorage *storage = NULL;
	NgsSynSystem *system;
	NgsSynRack *rack;
	NgsVoice *voice;
	unsigned int systemId = handle >> 29;
	unsigned int tag = handle & 0x7fff;
	unsigned int index;

	ngsGetProcessStorage(&storage);
	if (storage == NULL)
		return NULL;
	if ((unsigned int)storage->systemId != systemId ||
	    tag != NGS_VOICE_HANDLE_TAG)
		return NULL;
	system = storage->system;
	if (system == NULL)
		return NULL;
	index = handle >> 23 & 0x3f;
	if (index >= system->MaxRacks)
		return NULL;
	rack = system->racks[index];
	if (rack == NULL)
		return NULL;
	index = (unsigned char)(handle >> 15);
	if (rack->signature != NGS_RACK_SIGNATURE || index >= rack->voiceCount)
		return NULL;
	if (rack->voices == NULL)
		return NULL;
	voice = &rack->voices[index];
	if (voice->signature != NGS_VOICE_SIGNATURE)
		return NULL;
	return voice;
}

SceNgsHPatch ngsGetPatchHandle(const NgsPatch *patch)
{
	NgsVoice *voice;
	NgsSynRack *rack;

	if (patch == NULL)
		return 0;
	voice = patch->source;
	if (voice == NULL)
		return 0;
	rack = voice->rack;
	if (rack == NULL || rack->system == NULL)
		return 0;
	return voice->index << 15 | patch->index << 7 | NGS_PATCH_HANDLE_TAG |
		rack->index << 23 | rack->system->systemId << 29;
}

NgsPatch *ngsGetPatchFromHandle(SceNgsHPatch handle, NgsVoice **source)
{
	NgsProcessStorage *storage = NULL;
	NgsSynSystem *system;
	NgsSynRack *rack;
	NgsVoice *voice;
	NgsPatch *patch;
	unsigned int systemId = handle >> 29;
	unsigned int tag = handle & 0x7f;
	unsigned int index;

	ngsGetProcessStorage(&storage);
	if (storage == NULL)
		return NULL;
	if ((unsigned int)storage->systemId != systemId ||
	    tag != NGS_PATCH_HANDLE_TAG)
		return NULL;
	system = storage->system;
	if (system == NULL)
		return NULL;
	index = handle >> 23 & 0x3f;
	if (index >= system->MaxRacks)
		return NULL;
	rack = system->racks[index];
	if (rack == NULL)
		return NULL;
	index = (unsigned char)(handle >> 15);
	if (index >= rack->voiceCount)
		return NULL;
	voice = &rack->voices[index];
	index = (unsigned char)(handle >> 7);
	if (index >= rack->patchesPerVoice)
		return NULL;
	patch = &voice->patches[index];
	if (patch->signature != NGS_PATCH_SIGNATURE)
		return NULL;
	if (source != NULL)
		*source = voice;
	return patch;
}

NgsSynSystem *ngsGetVoiceSystem(const NgsVoice *voice)
{
	NgsSynRack *rack;

	if (voice == NULL)
		return NULL;
	rack = voice->rack;
	if (rack == NULL || rack->signature != NGS_RACK_SIGNATURE ||
	    rack->system == NULL || rack->system->signature != 54321)
		return NULL;
	return rack->system;
}

struct NgsSynSystem *ngsGetSystemForVoiceHandle(unsigned int handle)
{
	unsigned int systemId = handle >> 29;
	NgsProcessStorage *storage = NULL;

	ngsGetProcessStorage(&storage);
	if (storage != NULL) {
		if (storage->systemId == systemId)
			return storage->system;
		return NULL;
	}
	return NULL;
}
