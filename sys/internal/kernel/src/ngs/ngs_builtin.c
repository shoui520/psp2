/* Registration order of the built-in NGS audio modules. */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <intrmgr/syscall.h>

#include "ngs_module_memory.h"
#include "ngs_system.h"
#include "ngs_rack.h"

struct NgsSynSystem;
struct NgsModuleDefinition;

int ngsRegisterModule(uint32_t handle, struct NgsModuleDefinition *module);
int ngsGetParametricEqualizerModule(struct NgsModuleDefinition **module);

typedef int (*NgsModuleGetter)(NgsModuleDefinition **module);

const NgsModuleGetter ngsBuiltinModuleGetters[] = {
	ngsGetCompressorModule,
	ngsGetDelayModule,
	ngsGetDistortionModule,
	ngsGetEnvelopeModule,
	ngsGetBiquadFilterModule,
	ngsGetInputModule,
	ngsGetMixerModule,
	ngsGetOutputModule,
	ngsGetParametricEqualizerModule,
	ngsGetPauserModule,
	ngsGetPcmVagPlayerModule,
	ngsGetReverbModule,
	ngsGetSignalGeneratorModule,
	ngsGetAt9PlayerModule,
	ngsGetPitchShiftModule,
};

int ngsRegisterModule(uint32_t handle, struct NgsModuleDefinition *module)
{
	NgsSynSystem *system = ngsGetSystemFromHandle(handle);
	NgsSynSystem *lookup;
	NgsModuleDefinition **modules;
	NgsModuleDefinition *entry;
	unsigned int moduleId;
	unsigned int count;
	unsigned int index;

	if (system == NULL)
		return (int)0x804a000c;
	if (module == NULL)
		return (int)0x804a0002;
	if (system->ModulesLinked >= system->MaxModules)
		return (int)0x804a0006;
	moduleId = module->moduleId;
	lookup = ngsGetSystemFromHandle(handle);
	if (lookup != NULL && (modules = lookup->modules) != NULL &&
	    (count = lookup->ModulesLinked) != 0) {
		index = 0;
		do {
			entry = *modules++;
			++index;
			if (entry != NULL && entry->moduleId == moduleId)
				return SCE_NGS_ERROR;
		} while (index != count);
	}
	index = system->ModulesLinked;
	system->modules[index] = module;
	system->ModulesLinked = index + 1;
	return 0;
}

inline int ngsFindSystemModule(NgsSynSystem *system, unsigned int moduleId,
			NgsModuleDefinition **module)
{
	unsigned int index;

	if (module == NULL || system == NULL || system->modules == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;
	*module = NULL;
	for (index = 0; index < system->ModulesLinked; ++index) {
		if (system->modules[index] != NULL &&
		    system->modules[index]->moduleId == moduleId) {
			*module = system->modules[index];
			return 0;
		}
	}
	return SCE_NGS_ERROR_MODULE_NOT_AVAIL;
}

int ngsGetModuleMemoryInfo(NgsSynSystem *system, unsigned int moduleId,
			   int channels, NgsModuleMemoryInfo *info)
{
	NgsModuleDefinition *module;
	struct NgsModuleInitParams params;
	int result;

	result = ngsFindSystemModule(system, moduleId, &module);
	if (result != 0)
		return result;
	params.nSampleRate = system->SampleRate;
	params.nGranularity = system->Granularity;
	params.nChannelsPerVoice = channels;
	params.paramInterfaceCount = module->paramInterfaceCount;
	params.paramInterfaces = module->paramInterfaces;
	if (module->getMemoryInfo == NULL)
		return SCE_NGS_ERROR;
	return module->getMemoryInfo(&params, info);
}

int ngsIsCodecModule(NgsSynSystem *system, unsigned int moduleId)
{
	NgsModuleDefinition *module;

	if (ngsFindSystemModule(system, moduleId, &module) != 0)
		return 0;
	return module->codecResourceId != 0;
}

int ngsCheckModuleOutputs(NgsSynSystem *system, unsigned int moduleId,
			  unsigned int outputCount)
{
	NgsModuleDefinition *module;
	int result;

	result = ngsFindSystemModule(system, moduleId, &module);
	if (result != 0)
		return result;
	if (module->outputCount < outputCount)
		return SCE_NGS_ERROR_INVALID_PARAM;
	return 0;
}

SceInt32 sceNgsSulphaGetModuleListInternal(SceUInt32 *module_ids,
		SceUInt32 in_array_count, SceUInt32 *count)
{
	unsigned int syscall = sceSyscallEnter();
	int result;

	if (module_ids == NULL || count == NULL) {
		result = SCE_NGS_ERROR;
	} else {
		SceSize size = in_array_count * sizeof(*module_ids);
		SceNgsBufferInfo buffer = { NULL, 0 };
		SceUID memBlock;
		unsigned int sourceCount;
		NgsModuleDefinition *module;
		unsigned int *data;
		bool more;

		result = ngsAllocateDebugBuffer(&buffer, &memBlock, size);
		if (result >= 0) {
			sourceCount = 0;
			data = buffer.data;
			if (in_array_count != 0) {
				do {
					data[sourceCount] = 0;
					if (ngsBuiltinModuleGetters[sourceCount](&module) == 0)
						data[sourceCount] = module->moduleId;
					++sourceCount;
					more = in_array_count > sourceCount;
					if (sourceCount > 14)
						more = false;
				} while (more);
			}
			result = ngsCopyToCaller(module_ids, data, size, 1);
			if (result == 0)
				result = ngsCopyToCaller(count, &sourceCount,
						 sizeof(sourceCount), 1);
			ngsReleaseDebugBuffer(&buffer, memBlock);
		}
	}
	sceSyscallLeave(syscall);
	return result;
}

int ngsFindBuiltinModule(unsigned int moduleId, NgsModuleDefinition **module)
{
	const NgsModuleGetter *getter;
	int result;

	if (module == NULL)
		return SCE_NGS_ERROR;
	*module = NULL;
	getter = ngsBuiltinModuleGetters;
	do {
		result = (*getter++)(module);
		if (result == 0 && (*module)->moduleId == moduleId)
			return result;
	} while (getter != ngsBuiltinModuleGetters + 15);
	return SCE_NGS_ERROR;
}

int ngsRegisterBuiltinModules(struct NgsSynSystem *system)
{
	struct NgsModuleDefinition *module;
	uint32_t handle = ngsGetSystemHandle(system);
	int moduleResult;
	int result;

	if (handle == 0)
		return SCE_NGS_ERROR;

	moduleResult = ngsGetCompressorModule(&module);
	result = ngsRegisterModule(handle, module);
	if ((moduleResult | result) != 0)
		return result;
	moduleResult = ngsGetDistortionModule(&module);
	result = ngsRegisterModule(handle, module);
	if ((moduleResult | result) != 0)
		return result;
	moduleResult = ngsGetEnvelopeModule(&module);
	result = ngsRegisterModule(handle, module);
	if ((moduleResult | result) != 0)
		return result;
	moduleResult = ngsGetBiquadFilterModule(&module);
	result = ngsRegisterModule(handle, module);
	if ((moduleResult | result) != 0)
		return result;
	moduleResult = ngsGetInputModule(&module);
	result = ngsRegisterModule(handle, module);
	if ((moduleResult | result) != 0)
		return result;
	moduleResult = ngsGetMixerModule(&module);
	result = ngsRegisterModule(handle, module);
	if ((moduleResult | result) != 0)
		return result;
	moduleResult = ngsGetPauserModule(&module);
	result = ngsRegisterModule(handle, module);
	if ((moduleResult | result) != 0)
		return result;
	moduleResult = ngsGetPcmVagPlayerModule(&module);
	result = ngsRegisterModule(handle, module);
	if ((moduleResult | result) != 0)
		return result;
	moduleResult = ngsGetSignalGeneratorModule(&module);
	result = ngsRegisterModule(handle, module);
	if ((moduleResult | result) != 0)
		return result;
	moduleResult = ngsGetReverbModule(&module);
	result = ngsRegisterModule(handle, module);
	if ((moduleResult | result) != 0)
		return result;
	moduleResult = ngsGetParametricEqualizerModule(&module);
	result = ngsRegisterModule(handle, module);
	if ((moduleResult | result) != 0)
		return result;
	moduleResult = ngsGetOutputModule(&module);
	result = ngsRegisterModule(handle, module);
	if ((moduleResult | result) != 0)
		return result;
	moduleResult = ngsGetAt9PlayerModule(&module);
	result = ngsRegisterModule(handle, module);
	if ((moduleResult | result) != 0)
		return result;
	moduleResult = ngsGetPitchShiftModule(&module);
	result = ngsRegisterModule(handle, module);
	if ((moduleResult | result) != 0)
		return result;
	moduleResult = ngsGetDelayModule(&module);
	result = ngsRegisterModule(handle, module);
	if ((moduleResult | result) != 0)
		return result;
	return 0;
}

SceInt32 sceNgsModuleGetNumPresetsInternal(SceNgsHSynSystem handle,
	const SceNgsModuleID moduleId, SceUInt32 *count)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system;
	NgsModuleDefinition *module;
	SceUInt32 presets = 0;
	int result;

	system = ngsGetSystemFromHandle(handle);
	if (system == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	} else {
		result = ngsFindSystemModule(system, moduleId, &module);
		if (result == 0) {
			if (count == NULL) {
				result = SCE_NGS_ERROR_INVALID_PARAM;
			} else {
				presets = module->presetCount;
				result = ngsCopyToCaller(count, &presets,
					sizeof(presets), system->copyUserData);
			}
		}
	}
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsModuleGetPresetInternal(SceNgsHSynSystem handle,
	const SceNgsModuleID moduleId, const SceUInt32 presetIndex,
	SceNgsBufferInfo *info)
{
	unsigned int syscall = sceSyscallEnter();
	NgsSynSystem *system;
	NgsModuleDefinition *module;
	const SceNgsParamsDescriptor *params;
	SceNgsBufferInfo buffer;
	int result;

	system = ngsGetSystemFromHandle(handle);
	buffer.data = NULL;
	buffer.size = 0;
	if (system == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
		goto leave;
	}
	result = ngsFindSystemModule(system, moduleId, &module);
	if (result != 0)
		goto leave;
	if (info == NULL) {
		result = SCE_NGS_ERROR_INVALID_PARAM;
		goto leave;
	}
	result = ngsCopyFromCaller(&buffer, info, sizeof(buffer),
		system->copyUserData);
	if (result != 0)
		goto leave;
	if (buffer.data != NULL && presetIndex < module->presetCount &&
	    buffer.size >= (params = module->presets[presetIndex].params)->size)
		result = ngsCopyToCaller(buffer.data, params, params->size,
			system->copyUserData);
	else
		result = SCE_NGS_ERROR_INVALID_PARAM;
leave:
	sceSyscallLeave(syscall);
	return result;
}

SceInt32 sceNgsModuleCheckParamsInRangeInternal(SceNgsHVoice handle,
	const SceNgsModuleID moduleId, const SceNgsParamsDescriptor *params,
	const SceUInt32 size)
{
	unsigned int syscall = sceSyscallEnter();
	SceUInt32 buffer[80];
	NgsSynSystem *system;
	NgsVoice *voice;
	int result;

	voice = ngsGetVoiceFromHandle(handle);
	if (voice == NULL) {
		result = SCE_NGS_ERROR_INVALID_HANDLE;
	} else if (size >= 0x144) {
		result = SCE_NGS_ERROR;
	} else {
		system = ngsGetVoiceSystem(voice);
		if (system == NULL ||
		    ngsCopyFromCaller(buffer, params, size,
				      system->copyUserData) != 0)
			result = SCE_NGS_ERROR_INVALID_PARAM;
		else
			result = ngsCheckParamsInRange(system,
				voice->rack->channelsPerVoice, moduleId,
				(const SceNgsParamsDescriptor *)buffer, size);
	}
	sceSyscallLeave(syscall);
	return result;
}
