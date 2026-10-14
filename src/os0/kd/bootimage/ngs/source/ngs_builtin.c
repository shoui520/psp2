/* Registration order of the built-in NGS audio modules. */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sce/syscall.h>

#include "ngs_module_memory.h"
#include "ngs_system.h"

#define SCE_NGS_ERROR ((int)0x804a0001)

struct NgsSynSystem;
struct NgsModuleDefinition;

uint32_t ngsGetSystemHandle(struct NgsSynSystem *system);
int ngsRegisterModule(uint32_t handle, struct NgsModuleDefinition *module);
int ngsGetCompressorModule(struct NgsModuleDefinition **module);
int ngsGetDistortionModule(struct NgsModuleDefinition **module);
int ngsGetEnvelopeModule(struct NgsModuleDefinition **module);
int ngsGetBiquadFilterModule(struct NgsModuleDefinition **module);
int ngsGetInputModule(struct NgsModuleDefinition **module);
int ngsGetMixerModule(struct NgsModuleDefinition **module);
int ngsGetPauserModule(struct NgsModuleDefinition **module);
int ngsGetPcmVagPlayerModule(struct NgsModuleDefinition **module);
int ngsGetSignalGeneratorModule(struct NgsModuleDefinition **module);
int ngsGetReverbModule(struct NgsModuleDefinition **module);
int ngsGetParametricEqualizerModule(struct NgsModuleDefinition **module);
int ngsGetOutputModule(struct NgsModuleDefinition **module);
int ngsGetAt9PlayerModule(struct NgsModuleDefinition **module);
int ngsGetPitchShiftModule(struct NgsModuleDefinition **module);
int ngsGetDelayModule(struct NgsModuleDefinition **module);

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

struct NgsSynSystem *ngsGetSystemFromHandle(unsigned int handle);
int ngsAllocateDebugBuffer(SceNgsBufferInfo *buffer, SceUID *memBlock, int size);
int ngsReleaseDebugBuffer(SceNgsBufferInfo *buffer, SceUID memBlock);
int ngsCopyToCaller(void *destination, const void *source, SceSize size,
		    int copyUserData);

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
