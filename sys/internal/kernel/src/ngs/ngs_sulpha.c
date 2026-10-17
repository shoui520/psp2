/* Built-in module descriptions for Sulpha. */

#include <stddef.h>
#include <string.h>
#include <intrmgr/syscall.h>

#include "ngs_module_memory.h"

typedef union SulphaNgsModuleQuery {
	struct {
		unsigned int version;
		unsigned int moduleId;
		unsigned int inputCount;
		unsigned int outputCount;
		char info[256];
		unsigned int paramInterfaceCount;
		unsigned int dspControl;
		unsigned int flags;
		SceSize paramsSize;
	} module;
	struct {
		unsigned int paramsId;
		SceSize size;
		unsigned int paramCount;
	} interface;
	struct {
		char name[64];
		char units[32];
		char description[128];
		unsigned int type;
		NgsParamValue minimum;
		NgsParamValue maximum;
		NgsParamValue defaultValue;
	} parameter;
} SulphaNgsModuleQuery;

typedef char SulphaNgsModuleQuerySize[(sizeof(SulphaNgsModuleQuery) == 288) ? 1 : -1];

int ngsFindBuiltinModule(unsigned int moduleId, NgsModuleDefinition **module);
int ngsCopyToCaller(void *destination, const void *source, SceSize size,
		    int copyUserData);

int sceNgsSulphaQueryModuleInternal(unsigned int type, SulphaNgsModuleQuery *debug,
				   unsigned int moduleId, unsigned int index)
{
	NgsModuleDefinition *module;
	struct NgsModuleInitParams params;
	NgsModuleMemoryInfo memory;
	SulphaNgsModuleQuery query;
	const struct NgsModuleParamInterface *interface;
	const NgsModuleParamDescription *parameter;
	unsigned int syscall = sceSyscallEnter();
	unsigned int interfaceIndex = index >> 16;
	int result;

	module = NULL;
	result = ngsFindBuiltinModule(moduleId, &module);
	if (module == NULL) {
		result = (int)0x804a0002;
		goto leave;
	}
	if (result < 0)
		goto leave;
	switch (type) {
	case 1:
		if (module->paramInterfaces == NULL)
			return (int)0x804a0002;
		if (interfaceIndex >= module->paramInterfaceCount)
			goto leave;
		interface = module->paramInterfaces[interfaceIndex];
		memset(&query, 0, sizeof(query));
		if (interface != NULL) {
			query.interface.paramsId = interface->paramsId;
			query.interface.size = interface->size;
			query.interface.paramCount = interface->paramCount;
		}
		break;
	case 0:
		memset(&query, 0, sizeof(query));
		query.module.version = module->version;
		query.module.moduleId = module->moduleId;
		query.module.inputCount = module->inputCount;
		query.module.outputCount = module->outputCount;
		query.module.paramInterfaceCount = module->paramInterfaceCount;
		query.module.dspControl = module->dspControl;
		query.module.flags = module->flags & 1;
		strncpy(query.module.info, module->info, sizeof(query.module.info));
		if (module->getMemoryInfo != NULL) {
			params.paramInterfaceCount = module->paramInterfaceCount;
			params.paramInterfaces = module->paramInterfaces;
			params.nGranularity = 256;
			params.nSampleRate = 48000;
			params.nChannelsPerVoice = 2;
			if (module->getMemoryInfo(&params, &memory) == 0)
				query.module.paramsSize = memory.paramsSize;
		}
		break;
	case 2:
		if (interfaceIndex >= module->paramInterfaceCount)
			goto leave;
		interface = module->paramInterfaces[interfaceIndex];
		memset(&query, 0, sizeof(query));
		if (interface == NULL || (unsigned short)index >= interface->paramCount)
			goto leave;
		parameter = &interface->params[(unsigned short)index];
		if (parameter->name != NULL)
			strncpy(query.parameter.name, parameter->name,
				sizeof(query.parameter.name));
		if (parameter->units != NULL)
			strncpy(query.parameter.units, parameter->units,
				sizeof(query.parameter.units));
		if (parameter->description != NULL)
			strncpy(query.parameter.description, parameter->description,
				sizeof(query.parameter.description));
		query.parameter.minimum = parameter->minimum;
		query.parameter.maximum = parameter->maximum;
		query.parameter.defaultValue = parameter->defaultValue;
		query.parameter.type = parameter->type;
		result = ngsCopyToCaller(debug, &query, sizeof(query), 1);
		goto leave;
	default:
		goto leave;
	}
	result = ngsCopyToCaller(debug, &query, sizeof(query), 1);
leave:
	sceSyscallLeave(syscall);
	return result;
}
