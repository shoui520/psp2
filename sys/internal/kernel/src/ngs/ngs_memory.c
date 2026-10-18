/* SceNgs memory-size alignment. Internal Sony names are unavailable. */

#include <scetypes.h>
#include <stdint.h>
#include "ngs_voice_definition.h"

SceSize ngsAlignMemorySize(SceSize size)
{
	return (size + 255) & ~255U;
}

void *ngsAllocateAlignedMemory(unsigned char **cursor, SceSize size)
{
	uintptr_t address = (uintptr_t)*cursor;

	size = (size + 255) & ~255U;
	address += (unsigned char)(0U - address);
	*cursor = (unsigned char *)address + size;
	return (void *)address;
}

int ngsAlignSize(int size, int alignment)
{
	--alignment;
	return (size + alignment) & ~alignment;
}

void *ngsAllocateMemoryWithAlignment(unsigned char **cursor, SceSize size,
				   SceSize alignment)
{
	unsigned char *address = *cursor;
	unsigned char *next;
	SceSize padding;

	if (alignment > 1) {
		padding = (alignment - (uintptr_t)address % alignment) % alignment;
		--alignment;
		size += alignment;
		address += padding;
		next = address + (size & ~alignment);
	} else {
		next = address + size;
	}
	*cursor = next;
	return address;
}

SceSize ngsAlignAllocationSize(SceSize size, SceSize alignment)
{
	--alignment;
	return (size + alignment) & ~alignment;
}

uintptr_t ngsAlignMemoryAddress(uintptr_t address)
{
	return address + (unsigned char)(0U - address);
}

void *ngsAlignVoiceDefinitionAddress(void *address, SceSize alignment)
{
	--alignment;
	return (void *)(((uintptr_t)address + alignment) & ~alignment);
}

void *ngsAllocateMemory(unsigned char **cursor, SceSize size)
{
	unsigned char *address = *cursor;

	*cursor = address + size;
	return address;
}

unsigned int ngsGetVoiceDefinitionInputCount(
		const struct SceNgsVoiceDefinition *definition)
{
	const NgsVoiceModuleDefinition *module;
	const NgsVoiceModuleDefinition *end;
	unsigned int count = definition->moduleCount;
	unsigned int inputs = 0;

	module = (const NgsVoiceModuleDefinition *)
		((const unsigned char *)definition + definition->modulesOffset);
	if (count == 0)
		return 0;
	end = module + count;
	do {
		inputs += module->inputCount;
		++module;
	} while (module != end);
	return inputs;
}

unsigned int ngsGetVoiceDefinitionOutputCount(
		const struct SceNgsVoiceDefinition *definition)
{
	const NgsVoiceModuleDefinition *module;
	const NgsVoiceModuleDefinition *end;
	unsigned int count = definition->moduleCount;
	unsigned int outputs = 0;

	module = (const NgsVoiceModuleDefinition *)
		((const unsigned char *)definition + definition->modulesOffset);
	if (count == 0)
		return 0;
	end = module + count;
	do {
		outputs += module->outputCount;
		++module;
	} while (module != end);
	return outputs;
}
