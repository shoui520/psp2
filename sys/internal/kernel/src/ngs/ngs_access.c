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
