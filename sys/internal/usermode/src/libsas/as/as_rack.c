/*
 * What every rack shares: the parameter block of its inputs and outputs, and the tables its
 * tasks read them from.
 */

#include <stddef.h>

#include "as_ssp.h"
#include "as_rack.h"

int sceSasAsRackIoParamCreate(SceSasAsAllocator *allocator, SceSasAsConfigEntry **config,
			      SceSasAsHandle *handle, SceSize offset)
{
	SceUInt32 numChannels;
	SceUInt32 numInputs;
	SceUInt32 numOutputs;
	SceSasAsRackIoParam *param;
	SceSasAsRackPort *inputs;
	SceSasAsRackPort *outputs;

	sceSasAsConfigGetUInt(config, "numChannels", 2, &numChannels);
	sceSasAsConfigGetUInt(config, "numInputs", 4, &numInputs);
	sceSasAsConfigGetUInt(config, "numOutputs", 4, &numOutputs);
	inputs = sceSasAsAllocatorAlloc(allocator, numInputs * sizeof(SceSasAsRackPort), 1);
	outputs = sceSasAsAllocatorAlloc(allocator, numOutputs * sizeof(SceSasAsRackPort), 1);
	if (handle != NULL) {
		param = (SceSasAsRackIoParam *)((char *)handle + offset);
		param->numChannels = numChannels;
		param->numInputs = numInputs;
		param->numOutputs = numOutputs;
		param->inputsLocked.handle = handle;
		param->inputsLocked.block = SCE_SAS_AS_RACK_BLOCK_INPUTS;
		param->inputs.count = numInputs;
		param->inputs.ports = inputs;
		param->outputsLocked.handle = handle;
		param->outputsLocked.block = SCE_SAS_AS_RACK_BLOCK_OUTPUTS;
		param->outputs.count = numOutputs;
		param->outputs.ports = outputs;
	}
	return 0;
}

int sceSasAsRackIoTableCreate(SceSasAsAllocator *allocator, SceSasAsConfigEntry **config,
			      void *base, SceSize offset)
{
	SceUInt32 numChannels;
	SceUInt32 numInputs;
	SceUInt32 numOutputs;
	SceSasAsRackIoTable *table;
	SceUInt8 *inputBuss;
	SceUInt8 *outputBuss;
	SceUInt8 *inputMute;
	SceUInt8 *outputMute;
	SceInt16 *inputLevel;
	SceInt16 *outputLevel;

	sceSasAsConfigGetUInt(config, "numChannels", 2, &numChannels);
	sceSasAsConfigGetUInt(config, "numInputs", 4, &numInputs);
	sceSasAsConfigGetUInt(config, "numOutputs", 4, &numOutputs);
	inputBuss = sceSasAsAllocatorAlloc(allocator, numInputs, 1);
	outputBuss = sceSasAsAllocatorAlloc(allocator, numOutputs, 1);
	inputMute = sceSasAsAllocatorAlloc(allocator, numInputs, 1);
	outputMute = sceSasAsAllocatorAlloc(allocator, numOutputs, 1);
	inputLevel = sceSasAsAllocatorAlloc(allocator, numInputs * sizeof(SceInt16), 1);
	outputLevel = sceSasAsAllocatorAlloc(allocator, numOutputs * sizeof(SceInt16), 1);
	if (base != NULL) {
		table = (SceSasAsRackIoTable *)((char *)base + offset);
		table->numChannels = numChannels;
		table->numInputs = numInputs;
		table->numOutputs = numOutputs;
		table->inputBuss = inputBuss;
		table->inputMute = inputMute;
		table->inputLevel = inputLevel;
		table->outputBuss = outputBuss;
		table->outputMute = outputMute;
		table->outputLevel = outputLevel;
		table->inputBussOffset = SCE_SAS_AS_RELATIVE(table->inputBussOffset, inputBuss);
		table->inputMuteOffset = SCE_SAS_AS_RELATIVE(table->inputMuteOffset, inputMute);
		table->inputLevelOffset = SCE_SAS_AS_RELATIVE(table->inputLevelOffset, inputLevel);
		table->outputBussOffset = SCE_SAS_AS_RELATIVE(table->outputBussOffset, outputBuss);
		table->outputMuteOffset = SCE_SAS_AS_RELATIVE(table->outputMuteOffset, outputMute);
		table->outputLevelOffset = SCE_SAS_AS_RELATIVE(table->outputLevelOffset, outputLevel);
	}
	return 0;
}

int sceSasAsRackIoTaskCreate(SceSasAsAllocator *allocator, SceSasAsConfigEntry **config,
			     void *base, SceSize offset)
{
	SceUInt32 numChannels;
	SceUInt32 numInputs;
	SceUInt32 numOutputs;
	SceSasAsRackIoTask *task;
	SceUInt8 *inputBuss;
	SceUInt8 *outputBuss;
	SceUInt8 *inputMute;
	SceUInt8 *outputMute;
	SceInt16 *inputLevel;
	SceInt16 *outputLevel;

	sceSasAsConfigGetUInt(config, "numChannels", 2, &numChannels);
	sceSasAsConfigGetUInt(config, "numInputs", 4, &numInputs);
	sceSasAsConfigGetUInt(config, "numOutputs", 4, &numOutputs);
	inputBuss = sceSasAsAllocatorAlloc(allocator, numInputs, 1);
	outputBuss = sceSasAsAllocatorAlloc(allocator, numOutputs, 1);
	inputMute = sceSasAsAllocatorAlloc(allocator, numInputs, 1);
	outputMute = sceSasAsAllocatorAlloc(allocator, numOutputs, 1);
	inputLevel = sceSasAsAllocatorAlloc(allocator, numInputs * sizeof(SceInt16), 1);
	outputLevel = sceSasAsAllocatorAlloc(allocator, numOutputs * sizeof(SceInt16), 1);
	if (base != NULL) {
		task = (SceSasAsRackIoTask *)((char *)base + offset);
		task->numChannels = numChannels;
		task->numInputs = numInputs;
		task->numOutputs = numOutputs;
		task->inputBussOffset = SCE_SAS_AS_RELATIVE(task->inputBussOffset, inputBuss);
		task->inputMuteOffset = SCE_SAS_AS_RELATIVE(task->inputMuteOffset, inputMute);
		task->inputLevelOffset = SCE_SAS_AS_RELATIVE(task->inputLevelOffset, inputLevel);
		task->outputBussOffset = SCE_SAS_AS_RELATIVE(task->outputBussOffset, outputBuss);
		task->outputMuteOffset = SCE_SAS_AS_RELATIVE(task->outputMuteOffset, outputMute);
		task->outputLevelOffset = SCE_SAS_AS_RELATIVE(task->outputLevelOffset, outputLevel);
	}
	return 0;
}

/* a level as 4.12 fixed point, saturated */
static inline SceInt16 sceSasAsLevelToFixed(float level)
{
	float value = level * 4096.0f;

	if (value < -32768.0f)
		return -32768;
	if (value > 32767.0f)
		return 32767;
	return (SceInt16)value;
}

int sceSasAsRackIoTableSetInputs(const SceSasAsRackIoParam *param, SceSasAsRackIoTable *table)
{
	const SceSasAsRackPort *port = param->inputs.ports;
	SceUInt32 mask = param->inputs.mask;
	SceUInt32 i;

	table->inputMask = mask;
	for (i = 0; mask != 0; mask >>= 1, i++, port++) {
		if (mask & 1) {
			table->inputBuss[i] = port->buss;
			table->inputMute[i] = port->mute;
			table->inputLevel[i] = sceSasAsLevelToFixed(port->level);
		}
	}
	return 0;
}

int sceSasAsRackIoTableSetOutputs(const SceSasAsRackIoParam *param, SceSasAsRackIoTable *table)
{
	const SceSasAsRackPort *port = param->outputs.ports;
	SceUInt32 mask = param->outputs.mask;
	SceUInt32 flags = 0;
	SceUInt32 i;

	table->outputMask = mask;
	for (i = 0; mask != 0; mask >>= 1, i++, port++) {
		if (mask & 1) {
			table->outputBuss[i] = port->buss;
			table->outputMute[i] = port->mute;
			table->outputLevel[i] = sceSasAsLevelToFixed(port->level);
			if (port->flag)
				flags |= 1u << i;
		}
	}
	table->outputFlags = flags;
	return 0;
}
