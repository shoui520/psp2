/* Module parameter-interface checks. */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "ngs_module_memory.h"
#include "ngs_system.h"

int ngsCheckParamType(uint32_t paramsId, uint32_t expectedParamsId)
{
	if ((paramsId & 0xffffU) != (expectedParamsId & 0xffffU))
		return SCE_NGS_ERROR_PARAM_TYPE_MISMATCH;
	return 0;
}

int ngsGetParamsOutOfRange(NgsSynSystem *system, int channels,
		unsigned int moduleId, const SceNgsParamsDescriptor *params,
		SceSize paramsSize, char *message)
{
	unsigned int setSize = params->size;
	NgsModuleDefinition **modules;
	NgsModuleDefinition *module;
	const struct NgsModuleParamInterface *const *interfaces;
	const struct NgsModuleParamInterface *interface;
	unsigned int index;
	unsigned int paramsId;
	unsigned int count;
	unsigned int channel;
	int availableChannels;

	if (message == NULL || system == NULL || system->modules == NULL)
		return (int)0x804a0002;
	count = system->ModulesLinked;
	if (count == 0)
		return (int)0x804a0007;
	modules = system->modules;
	index = 0;
	do {
		module = *modules++;
		++index;
		if (module != NULL && module->moduleId == moduleId)
			break;
		if (index == count)
			return (int)0x804a0007;
	} while (1);
	count = module->paramInterfaceCount;
	if (count == 0)
		return 0;
	interfaces = module->paramInterfaces;
	paramsId = params->id;
	interface = *interfaces;
	index = 0;
	while (interface->paramsId != paramsId) {
		if (++index == count) {
			snprintf(message, 128, "Module/params mismatch: parameter module ID: 0x%x, actual module ID: 0x%x param interface id 0x%x\n",
				 (unsigned short)paramsId, moduleId, paramsId);
			return 0;
		}
		interface = *++interfaces;
	}
	availableChannels = paramsSize / setSize;
	if (channels >= availableChannels)
		channels = availableChannels;
	if (channels <= 0)
		return 0;
	channel = 0;
	do {
		const unsigned char *value = (const unsigned char *)(params + 1);
		const NgsModuleParamDescription *description = interface->params;
		unsigned int parameter;

		if (params->id != interface->paramsId) {
			snprintf(message, 128, "Set ID (0x%x) set doesn't match module ID (0x%x)\n",
				 interface->paramsId, params->id);
			return (int)0x804a0002;
		}
		if (params->size != interface->size) {
			snprintf(message, 128, "Set size (%d) set doesn't match module size (%d)\n",
				 (int)interface->size, (int)params->size);
			return (int)0x804a0002;
		}
		for (parameter = 0; parameter != interface->paramCount;
		     ++parameter, ++description) {
			int integer, minimum, maximum;

			__builtin_prefetch((const unsigned char *)description + 124);
			switch (description->type) {
			case 0:
			case 3:
			case 4:
				value += 4;
				break;
			case 1:
				integer = *(const int *)value;
				minimum = description->minimum.integer;
				maximum = description->maximum.integer;
				if (integer < minimum || integer > maximum)
					goto integerError;
				value += 4;
				break;
			case 2: {
				float real = *(const float *)value;
				float lower = description->minimum.real;
				float upper = description->maximum.real;
				float realDigits, lowerDigits, upperDigits;

				if (real < lower || real > upper) {
					realDigits = (real - (int)real) * 10.0f;
					lowerDigits = (lower - (int)lower) * 10.0f;
					upperDigits = (upper - (int)upper) * 10.0f;
					snprintf(message, 128, "Param %d (%s) out of range. Value set was %d.%d%d. Range is %d.%d%d <--> %d.%d%d\n",
						 parameter, description->name,
						 (int)real, (int)realDigits,
						 (int)((realDigits - (int)realDigits) * 10.0f),
						 (int)lower, (int)lowerDigits,
						 (int)((lowerDigits - (int)lowerDigits) * 10.0f),
						 (int)upper, (int)upperDigits,
						 (int)((upperDigits - (int)upperDigits) * 10.0f));
					return 0;
				}
				value += 4;
				break;
			}
			case 5:
				integer = *(const unsigned int *)value;
				minimum = description->minimum.unsignedInteger;
				maximum = description->maximum.unsignedInteger;
				if ((unsigned int)integer < (unsigned int)minimum ||
				    (unsigned int)integer > (unsigned int)maximum)
					goto integerError;
				value += 4;
				break;
			case 7:
				integer = *(const signed char *)value;
				minimum = description->minimum.signedByte;
				maximum = description->maximum.signedByte;
				if (integer < minimum || integer > maximum)
					goto integerError;
				++value;
				break;
			case 9:
				integer = *(const short *)value;
				minimum = description->minimum.signedShort;
				maximum = description->maximum.signedShort;
				if (integer < minimum || integer > maximum)
					goto integerError;
				value += 2;
				break;
			default:
				break;
			}
			continue;

		integerError:
			snprintf(message, 128, "Param %d (%s) out of range. Value set was %d. Range is %d <--> %d\n",
				 parameter, description->name, integer, minimum, maximum);
			return 0;
		}
		params = (const SceNgsParamsDescriptor *)
			((const unsigned char *)params + setSize);
	} while (++channel != (unsigned int)channels);
	return 0;
}

int ngsCheckParamsInRange(NgsSynSystem *system, unsigned int channels,
		unsigned int moduleId, const SceNgsParamsDescriptor *params,
		SceSize paramsSize)
{
	unsigned int setSize = params->size;
	unsigned int availableChannels = paramsSize / setSize;
	unsigned int count, index, channel;
	NgsModuleDefinition **modules;
	NgsModuleDefinition *module;
	const struct NgsModuleParamInterface *const *interfaces;
	const struct NgsModuleParamInterface *interface;

	if (channels >= availableChannels)
		channels = availableChannels;
	if (channels == 0)
		return (int)0x804a0001;
	if (system == NULL || system->modules == NULL)
		return (int)0x804a0002;
	count = system->ModulesLinked;
	if (count == 0)
		return (int)0x804a0007;
	modules = system->modules;
	index = 0;
	do {
		module = *modules++;
		++index;
		if (module != NULL && module->moduleId == moduleId)
			break;
		if (index == count)
			return (int)0x804a0007;
	} while (1);
	count = module->paramInterfaceCount;
	if (count == 0)
		return 0;
	interfaces = module->paramInterfaces;
	interface = *interfaces;
	index = 0;
	while (interface->paramsId != params->id) {
		if (++index == count)
			return (int)0x804a0009;
		interface = *++interfaces;
	}
	channel = 0;
	do {
		const unsigned char *value = (const unsigned char *)(params + 1);
		const NgsModuleParamDescription *description = interface->params;
		unsigned int parameter;

		if (params->id != interface->paramsId)
			return (int)0x804a0002;
		if (params->size != setSize || setSize != interface->size)
			return (int)0x804a000d;
		for (parameter = 0; parameter != interface->paramCount;
		     ++parameter, ++description) {
			__builtin_prefetch((const unsigned char *)description + 124);
			switch (description->type) {
			case 0:
			case 3:
			case 4:
				value += 4;
				break;
			case 1:
				if (*(const int *)value < description->minimum.integer ||
				    *(const int *)value > description->maximum.integer)
					return (int)0x804a0009;
				value += 4;
				break;
			case 2:
				if (*(const float *)value < description->minimum.real ||
				    *(const float *)value > description->maximum.real)
					return (int)0x804a0009;
				value += 4;
				break;
			case 5:
				if (*(const unsigned int *)value < description->minimum.unsignedInteger ||
				    *(const unsigned int *)value > description->maximum.unsignedInteger)
					return (int)0x804a0009;
				value += 4;
				break;
			case 7:
				if (description->minimum.signedByte > *(const signed char *)value ||
				    description->maximum.signedByte < *(const signed char *)value)
					return (int)0x804a0009;
				++value;
				break;
			case 9:
				if (description->minimum.signedShort > *(const short *)value ||
				    description->maximum.signedShort < *(const short *)value)
					return (int)0x804a0009;
				value += 2;
				break;
			default:
				return (int)0x804a0001;
			}
		}
		params = (const SceNgsParamsDescriptor *)
			((const unsigned char *)params + setSize);
	} while (++channel < channels);
	return 0;
}

