/* Built-in serialized voice definitions. */

#include <sce/debug.h>
#include <sce/syscall.h>
#include <psp2kern/kernel/sysclib.h>
#include "ngs_voice_definition.h"

void *ngsAlignVoiceDefinitionAddress(void *address, unsigned int alignment);
void *ngsAllocateMemory(unsigned char **cursor, unsigned int size);

unsigned char ngsDelayVoiceDefinitionStorage[160]
	__attribute__((section(".data"))) = { 0 };
unsigned char ngsCompressorVoiceDefinitionStorage[160]
	__attribute__((section(".data"))) = { 0 };
unsigned char ngsCompressorSideChainVoiceDefinitionStorage[208]
	__attribute__((section(".data"))) = { 0 };
unsigned char ngsDistortionVoiceDefinitionStorage[160]
	__attribute__((section(".data"))) = { 0 };
unsigned char ngsEnvelopeVoiceDefinitionStorage[160]
	__attribute__((section(".data"))) = { 0 };
unsigned char ngsEqVoiceDefinitionStorage[160]
	__attribute__((section(".data"))) = { 0 };
unsigned char ngsMasterVoiceDefinitionStorage[160]
	__attribute__((section(".data"))) = { 0 };
unsigned char ngsMixerVoiceDefinitionStorage[128]
	__attribute__((section(".data"))) = { 0 };
unsigned char ngsPauserVoiceDefinitionStorage[160]
	__attribute__((section(".data"))) = { 0 };
unsigned char ngsReverbVoiceDefinitionStorage[160]
	__attribute__((section(".data"))) = { 0 };
NgsVoicePresetDefinition ngsSimpleVoicePreset
	__attribute__((section(".data"))) = { 0 };
NgsVoicePresetDefinition ngsSimpleAtrac9VoicePreset
	__attribute__((section(".data"))) = { 0 };
NgsVoicePresetDefinition ngsScreamVoicePreset
	__attribute__((section(".data"))) = { 0 };
NgsVoicePresetDefinition ngsScreamAt9VoicePreset
	__attribute__((section(".data"))) = { 0 };
unsigned char ngsSimpleVoiceDefinitionStorage[296]
	__attribute__((section(".data"))) = { 0 };
const uint32_t ngsSimpleVoicePresetIndices[5] = { 1, 2, 3, 4, 5 };
unsigned char ngsSasEmuVoiceDefinitionStorage[184]
	__attribute__((section(".data"))) = { 0 };
unsigned char ngsSimpleAtrac9VoiceDefinitionStorage[296]
	__attribute__((section(".data"))) = { 0 };
const uint32_t ngsSimpleAtrac9VoicePresetIndices[5] = { 1, 2, 3, 4, 5 };
unsigned char ngsScreamVoiceDefinitionStorage[296]
	__attribute__((section(".data"))) = { 0 };
const uint32_t ngsScreamVoicePresetIndices[5] = { 1, 2, 3, 4, 5 };
unsigned char ngsScreamAt9VoiceDefinitionStorage[296]
	__attribute__((section(".data"))) = { 0 };
const uint32_t ngsScreamAt9VoicePresetIndices[5] = { 1, 2, 3, 4, 5 };
NgsVoicePresetDefinition ngsTemplate1VoicePresets[4]
	__attribute__((section(".data"))) = { 0 };
unsigned char ngsTemplate1VoiceDefinitionStorage[408]
	__attribute__((section(".data"))) = { 0 };
const uint32_t ngsTemplate1EnviroIndices[5] = { 1, 5, 3, 8, 9 };
const uint32_t ngsTemplate1PlaybackIndices[8] = { 1, 4, 5, 3, 6, 7, 8, 9 };
const uint32_t ngsTemplate1LibSasIndices[6] = { 5, 3, 6, 7, 8, 9 };
NgsVoicePresetDefinition ngsAtrac9VoicePresets[4]
	__attribute__((section(".data"))) = { 0 };
unsigned char ngsAtrac9VoiceDefinitionStorage[432]
	__attribute__((section(".data"))) = { 0 };
const uint32_t ngsAtrac9LibSasIndices[6] = { 5, 3, 6, 7, 8, 9 };
const uint32_t ngsAtrac9PlaybackIndices[8] = { 1, 4, 5, 3, 6, 7, 8, 9 };
const uint32_t ngsAtrac9FilterInput = 5;
const uint32_t ngsAtrac9EnviroIndices[5] = { 1, 5, 3, 8, 9 };
unsigned char ngsPitchShiftVoiceDefinitionStorage[160]
	__attribute__((section(".data"))) = { 0 };

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetCompressorBussInternal(void)
{
	unsigned int syscallState = sceSyscallEnter();
	unsigned char *cursor = ngsAlignVoiceDefinitionAddress(
		ngsCompressorVoiceDefinitionStorage, 16);
	unsigned char *start = cursor;
	struct SceNgsVoiceDefinition *definition;
	NgsVoiceModuleDefinition *modules;
	struct NgsAudioFormat *format;
	uint32_t *outputBindings;
	uint32_t *inputIndices;
	uint32_t *routingIndices;
	unsigned int size;

	definition = ngsAllocateMemory(&cursor, sizeof(*definition));
	modules = ngsAllocateMemory(&cursor, 2 * sizeof(*modules));
	format = ngsAllocateMemory(&cursor, sizeof(*format));
	outputBindings = ngsAllocateMemory(&cursor, sizeof(*outputBindings));
	inputIndices = ngsAllocateMemory(&cursor, sizeof(*inputIndices));
	routingIndices = ngsAllocateMemory(&cursor, 2 * sizeof(*routingIndices));
	size = cursor - start;
	*outputBindings = 0x80000000U;
	*inputIndices = 0;
	routingIndices[0] = 0;
	routingIndices[1] = 1;
	format->codecFourCC = (uint32_t)'l' | (uint32_t)'p' << 8 |
		(uint32_t)'c' << 16 | (uint32_t)'m' << 24;
	format->sampleFormat = 0x04343271;
	format->formatParameters[0] = 0;
	format->formatParameters[1] = 0;
	modules[0].outputCount = 1;
	modules[0].inputBindingsOffset = 0;
	modules[1].inputCount = 1;
	modules[1].outputCount = 1;
	modules[0].formatOffset = (uintptr_t)format - (uintptr_t)&modules[0];
	modules[0].moduleId = 0x5ce0;
	modules[1].formatOffset = (uintptr_t)format - (uintptr_t)&modules[1];
	modules[1].moduleId = 0x5ce1;
	modules[1].outputBindingsOffset =
		(uintptr_t)outputBindings - (uintptr_t)&modules[1];
	modules[0].inputCount = 0;
	modules[0].outputBindingsOffset = 0;
	modules[1].inputBindingsOffset = 0;
	definition->size = size;
	definition->modulesOffset = (uintptr_t)modules - (uintptr_t)definition;
	definition->reserved = 0;
	definition->signature[0] = 'b';
	definition->signature[1] = 'v';
	definition->signature[2] = 'd';
	definition->signature[3] = 'f';
	definition->version = 0x10001;
	definition->nameOffset = (uintptr_t)"Compressor Buss" - (uintptr_t)definition;
	definition->moduleCount = 2;
	definition->dspConfiguration = 2;
	definition->nameLength = strnlen("Compressor Buss", 32);
	definition->descriptionOffset = (uintptr_t)
		"Compressor Buss: Input (mix) -> Compressor -> Output.\n" -
		(uintptr_t)definition;
	size = strnlen("Compressor Buss: Input (mix) -> Compressor -> Output.\n", 128);
	definition->presetsOffset = 0;
	definition->presetCount = 0;
	definition->descriptionLength = size;
	definition->routingIndicesOffset =
		(uintptr_t)routingIndices - (uintptr_t)definition;
	definition->voiceOutputCount = 1;
	definition->routingIndexCount = 2;
	sceSyscallLeave(syscallState);
	return (const struct SceNgsVoiceDefinition *)
		((uintptr_t)definition ^ 0x9e28dcce);
}


const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetCompressorSideChainBussInternal(void)
{
	unsigned int syscallState = sceSyscallEnter();
	unsigned char *cursor = ngsAlignVoiceDefinitionAddress(
		ngsCompressorSideChainVoiceDefinitionStorage, 16);
	unsigned char *start = cursor;
	struct SceNgsVoiceDefinition *definition;
	NgsVoiceModuleDefinition *modules;
	struct NgsAudioFormat *format;
	uint32_t *outputBindings;
	uint32_t *inputIndices;
	uint32_t *routingIndices;
	uint32_t *sideChainIndices;
	uint32_t *inputBindings;
	unsigned int size;

	definition = ngsAllocateMemory(&cursor, sizeof(*definition));
	modules = ngsAllocateMemory(&cursor, 3 * sizeof(*modules));
	format = ngsAllocateMemory(&cursor, sizeof(*format));
	outputBindings = ngsAllocateMemory(&cursor, sizeof(*outputBindings));
	inputIndices = ngsAllocateMemory(&cursor, 2 * sizeof(*inputIndices));
	routingIndices = ngsAllocateMemory(&cursor, 3 * sizeof(*routingIndices));
	sideChainIndices = ngsAllocateMemory(&cursor, 2 * sizeof(*sideChainIndices));
	inputBindings = ngsAllocateMemory(&cursor, 2 * sizeof(*inputBindings));
	size = cursor - start;
	*outputBindings = 0x80000000U;
	inputIndices[0] = 0;
	inputIndices[1] = 1;
	routingIndices[1] = 1;
	routingIndices[2] = 2;
	routingIndices[0] = 0;
	sideChainIndices[0] = 0;
	sideChainIndices[1] = 1;
	inputBindings[0] = 0;
	inputBindings[1] = 1;
	format->codecFourCC = (uint32_t)'l' | (uint32_t)'p' << 8 |
		(uint32_t)'c' << 16 | (uint32_t)'m' << 24;
	format->sampleFormat = 0x04343271;
	format->formatParameters[0] = 0;
	format->formatParameters[1] = 0;
	modules[0].inputCount = 0;
	modules[0].outputCount = 1;
	modules[0].inputBindingsOffset = 0;
	modules[0].outputBindingsOffset = 0;
	modules[1].inputCount = 0;
	modules[1].outputCount = 1;
	modules[1].inputBindingsOffset = 0;
	modules[1].outputBindingsOffset = 0;
	modules[2].inputCount = 2;
	modules[2].outputCount = 1;
	modules[0].formatOffset = (uintptr_t)format - (uintptr_t)&modules[0];
	modules[1].formatOffset = (uintptr_t)format - (uintptr_t)&modules[1];
	modules[2].moduleId = 0x5ce1;
	modules[2].formatOffset = (uintptr_t)format - (uintptr_t)&modules[2];
	modules[2].inputBindingsOffset =
		(uintptr_t)inputBindings - (uintptr_t)&modules[2];
	modules[0].moduleId = 0x5ce0;
	modules[1].moduleId = 0x5ce0;
	modules[2].outputBindingsOffset =
		(uintptr_t)outputBindings - (uintptr_t)&modules[2];
	definition->version = 0x10001;
	definition->size = size;
	definition->modulesOffset = (uintptr_t)modules - (uintptr_t)definition;
	definition->signature[1] = 'v';
	definition->signature[3] = 'f';
	definition->moduleCount = 3;
	definition->dspConfiguration = 3;
	definition->signature[0] = 'b';
	definition->reserved = 0;
	definition->signature[2] = 'd';
	definition->nameOffset = (uintptr_t)"Compressor SideChain Buss" -
		(uintptr_t)definition;
	definition->nameLength = strnlen("Compressor SideChain Buss", 32);
	definition->descriptionOffset = (uintptr_t)
		"Compressor Side Chain. In:0 Data To Process, In:1 Control Data" -
		(uintptr_t)definition;
	size = strnlen("Compressor Side Chain. In:0 Data To Process, In:1 Control Data", 128);
	definition->presetsOffset = 0;
	definition->presetCount = 0;
	definition->routingIndicesOffset =
		(uintptr_t)routingIndices - (uintptr_t)definition;
	definition->descriptionLength = size;
	definition->voiceOutputCount = 1;
	definition->routingIndexCount = 3;
	sceSyscallLeave(syscallState);
	return (const struct SceNgsVoiceDefinition *)
		((uintptr_t)definition ^ 0x9e28dcce);
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetDistortionBussInternal(void)
{
	unsigned int syscallState = sceSyscallEnter();
	unsigned char *cursor = ngsAlignVoiceDefinitionAddress(
		ngsDistortionVoiceDefinitionStorage, 16);
	unsigned char *start = cursor;
	struct SceNgsVoiceDefinition *definition;
	NgsVoiceModuleDefinition *modules;
	struct NgsAudioFormat *format;
	uint32_t *outputBindings;
	uint32_t *inputIndices;
	uint32_t *routingIndices;
	unsigned int size;

	definition = ngsAllocateMemory(&cursor, sizeof(*definition));
	modules = ngsAllocateMemory(&cursor, 2 * sizeof(*modules));
	format = ngsAllocateMemory(&cursor, sizeof(*format));
	outputBindings = ngsAllocateMemory(&cursor, sizeof(*outputBindings));
	inputIndices = ngsAllocateMemory(&cursor, sizeof(*inputIndices));
	routingIndices = ngsAllocateMemory(&cursor, 2 * sizeof(*routingIndices));
	size = cursor - start;
	*outputBindings = 0x80000000U;
	*inputIndices = 0;
	routingIndices[0] = 0;
	routingIndices[1] = 1;
	format->codecFourCC = (uint32_t)'l' | (uint32_t)'p' << 8 |
		(uint32_t)'c' << 16 | (uint32_t)'m' << 24;
	format->sampleFormat = 0x04343271;
	format->formatParameters[0] = 0;
	format->formatParameters[1] = 0;
	modules[0].outputCount = 1;
	modules[0].inputBindingsOffset = 0;
	modules[1].inputCount = 1;
	modules[1].outputCount = 1;
	modules[0].formatOffset = (uintptr_t)format - (uintptr_t)&modules[0];
	modules[0].moduleId = 0x5ce0;
	modules[1].formatOffset = (uintptr_t)format - (uintptr_t)&modules[1];
	modules[1].moduleId = 0x5ce2;
	modules[1].outputBindingsOffset =
		(uintptr_t)outputBindings - (uintptr_t)&modules[1];
	modules[0].inputCount = 0;
	modules[0].outputBindingsOffset = 0;
	modules[1].inputBindingsOffset = 0;
	definition->size = size;
	definition->modulesOffset = (uintptr_t)modules - (uintptr_t)definition;
	definition->reserved = 0;
	definition->signature[0] = 'b';
	definition->signature[1] = 'v';
	definition->signature[2] = 'd';
	definition->signature[3] = 'f';
	definition->version = 0x10001;
	definition->nameOffset = (uintptr_t)"Distortion Buss" - (uintptr_t)definition;
	definition->moduleCount = 2;
	definition->dspConfiguration = 2;
	definition->nameLength = strnlen("Distortion Buss", 32);
	definition->descriptionOffset = (uintptr_t)
		"Distortion Buss: Input (mix) -> Distortion -> Output.\n" -
		(uintptr_t)definition;
	size = strnlen("Distortion Buss: Input (mix) -> Distortion -> Output.\n", 128);
	definition->presetsOffset = 0;
	definition->presetCount = 0;
	definition->descriptionLength = size;
	definition->routingIndicesOffset =
		(uintptr_t)routingIndices - (uintptr_t)definition;
	definition->voiceOutputCount = 1;
	definition->routingIndexCount = 2;
	sceSyscallLeave(syscallState);
	return (const struct SceNgsVoiceDefinition *)
		((uintptr_t)definition ^ 0x9e28dcce);
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetEnvelopeBussInternal(void)
{
	unsigned int syscallState = sceSyscallEnter();
	unsigned char *cursor = ngsAlignVoiceDefinitionAddress(
		ngsEnvelopeVoiceDefinitionStorage, 16);
	unsigned char *start = cursor;
	struct SceNgsVoiceDefinition *definition;
	NgsVoiceModuleDefinition *modules;
	struct NgsAudioFormat *format;
	uint32_t *outputBindings;
	uint32_t *inputIndices;
	uint32_t *routingIndices;
	unsigned int size;

	definition = ngsAllocateMemory(&cursor, sizeof(*definition));
	modules = ngsAllocateMemory(&cursor, 2 * sizeof(*modules));
	format = ngsAllocateMemory(&cursor, sizeof(*format));
	outputBindings = ngsAllocateMemory(&cursor, sizeof(*outputBindings));
	inputIndices = ngsAllocateMemory(&cursor, sizeof(*inputIndices));
	routingIndices = ngsAllocateMemory(&cursor, 2 * sizeof(*routingIndices));
	size = cursor - start;
	*outputBindings = 0x80000000U;
	*inputIndices = 0;
	routingIndices[0] = 0;
	routingIndices[1] = 1;
	format->codecFourCC = (uint32_t)'l' | (uint32_t)'p' << 8 |
		(uint32_t)'c' << 16 | (uint32_t)'m' << 24;
	format->sampleFormat = 0x04343271;
	format->formatParameters[0] = 0;
	format->formatParameters[1] = 0;
	modules[0].outputCount = 1;
	modules[0].inputBindingsOffset = 0;
	modules[1].inputCount = 1;
	modules[1].outputCount = 1;
	modules[1].formatOffset = (uintptr_t)format - (uintptr_t)&modules[1];
	modules[0].moduleId = 0x5ce0;
	modules[0].inputCount = 0;
	modules[0].outputBindingsOffset = 0;
	modules[1].inputBindingsOffset = 0;
	modules[0].formatOffset = (uintptr_t)format - (uintptr_t)&modules[0];
	modules[1].moduleId = 0x5ce3;
	modules[1].outputBindingsOffset =
		(uintptr_t)outputBindings - (uintptr_t)&modules[1];
	definition->size = size;
	definition->modulesOffset = (uintptr_t)modules - (uintptr_t)definition;
	definition->reserved = 0;
	definition->signature[0] = 'b';
	definition->signature[1] = 'v';
	definition->signature[2] = 'd';
	definition->signature[3] = 'f';
	definition->version = 0x10001;
	definition->nameOffset = (uintptr_t)"Envelope Buss" - (uintptr_t)definition;
	definition->moduleCount = 2;
	definition->dspConfiguration = 2;
	definition->nameLength = strnlen("Envelope Buss", 32);
	definition->descriptionOffset = (uintptr_t)
		"Envelope Buss: Input (mix) -> Envelope -> Output.\n" -
		(uintptr_t)definition;
	size = strnlen("Envelope Buss", 128);
	definition->presetsOffset = 0;
	definition->presetCount = 0;
	definition->descriptionLength = size;
	definition->routingIndicesOffset =
		(uintptr_t)routingIndices - (uintptr_t)definition;
	definition->voiceOutputCount = 1;
	definition->routingIndexCount = 2;
	sceSyscallLeave(syscallState);
	return (const struct SceNgsVoiceDefinition *)
		((uintptr_t)definition ^ 0x9e28dcce);
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetEqBussInternal(void)
{
	unsigned int syscallState = sceSyscallEnter();
	unsigned char *cursor = ngsAlignVoiceDefinitionAddress(
		ngsEqVoiceDefinitionStorage, 16);
	unsigned char *start = cursor;
	struct SceNgsVoiceDefinition *definition;
	NgsVoiceModuleDefinition *modules;
	struct NgsAudioFormat *format;
	uint32_t *outputBindings;
	uint32_t *inputIndices;
	uint32_t *routingIndices;
	unsigned int size;

	definition = ngsAllocateMemory(&cursor, sizeof(*definition));
	modules = ngsAllocateMemory(&cursor, 2 * sizeof(*modules));
	format = ngsAllocateMemory(&cursor, sizeof(*format));
	outputBindings = ngsAllocateMemory(&cursor, sizeof(*outputBindings));
	inputIndices = ngsAllocateMemory(&cursor, sizeof(*inputIndices));
	routingIndices = ngsAllocateMemory(&cursor, 2 * sizeof(*routingIndices));
	size = cursor - start;
	*outputBindings = 0x80000000U;
	*inputIndices = 0;
	routingIndices[0] = 0;
	routingIndices[1] = 1;
	format->codecFourCC = (uint32_t)'l' | (uint32_t)'p' << 8 |
		(uint32_t)'c' << 16 | (uint32_t)'m' << 24;
	format->sampleFormat = 0x04343271;
	format->formatParameters[0] = 0;
	format->formatParameters[1] = 0;
	modules[0].outputCount = 1;
	modules[0].inputBindingsOffset = 0;
	modules[1].inputCount = 1;
	modules[1].outputCount = 1;
	modules[0].formatOffset = (uintptr_t)format - (uintptr_t)&modules[0];
	modules[0].moduleId = 0x5ce0;
	modules[1].formatOffset = (uintptr_t)format - (uintptr_t)&modules[1];
	modules[1].moduleId = 0x5cec;
	modules[1].outputBindingsOffset =
		(uintptr_t)outputBindings - (uintptr_t)&modules[1];
	modules[0].inputCount = 0;
	modules[0].outputBindingsOffset = 0;
	modules[1].inputBindingsOffset = 0;
	definition->size = size;
	definition->modulesOffset = (uintptr_t)modules - (uintptr_t)definition;
	definition->reserved = 0;
	definition->signature[0] = 'b';
	definition->signature[1] = 'v';
	definition->signature[2] = 'd';
	definition->signature[3] = 'f';
	definition->version = 0x10001;
	definition->nameOffset = (uintptr_t)"EQ Buss" - (uintptr_t)definition;
	definition->moduleCount = 2;
	definition->dspConfiguration = 2;
	definition->nameLength = strnlen("EQ Buss", 32);
	definition->descriptionOffset = (uintptr_t)
		"EQ Buss Def: In (mix) -> Para EQ (4 * 2-pole filters) -> Out" -
		(uintptr_t)definition;
	size = strnlen("EQ Buss Def: In (mix) -> Para EQ (4 * 2-pole filters) -> Out", 128);
	definition->presetsOffset = 0;
	definition->presetCount = 0;
	definition->descriptionLength = size;
	definition->routingIndicesOffset =
		(uintptr_t)routingIndices - (uintptr_t)definition;
	definition->voiceOutputCount = 1;
	definition->routingIndexCount = 2;
	sceSyscallLeave(syscallState);
	return (const struct SceNgsVoiceDefinition *)
		((uintptr_t)definition ^ 0x9e28dcce);
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetMasterBussInternal(void)
{
	unsigned int syscallState = sceSyscallEnter();
	unsigned char *cursor = ngsAlignVoiceDefinitionAddress(
		ngsMasterVoiceDefinitionStorage, 16);
	unsigned char *start = cursor;
	struct SceNgsVoiceDefinition *definition;
	NgsVoiceModuleDefinition *modules;
	struct NgsAudioFormat *format;
	uint32_t *outputBindings;
	uint32_t *inputIndices;
	uint32_t *routingIndices;
	unsigned int size;

	definition = ngsAllocateMemory(&cursor, sizeof(*definition));
	modules = ngsAllocateMemory(&cursor, 2 * sizeof(*modules));
	format = ngsAllocateMemory(&cursor, sizeof(*format));
	outputBindings = ngsAllocateMemory(&cursor, sizeof(*outputBindings));
	inputIndices = ngsAllocateMemory(&cursor, sizeof(*inputIndices));
	routingIndices = ngsAllocateMemory(&cursor, 2 * sizeof(*routingIndices));
	size = cursor - start;
	*outputBindings = 0x80000000U;
	routingIndices[0] = 0;
	routingIndices[1] = 1;
	*inputIndices = 0;
	format->sampleFormat = 0x04343271;
	format->formatParameters[0] = 0;
	format->formatParameters[1] = 0;
	format->codecFourCC = (uint32_t)'l' | (uint32_t)'p' << 8 |
		(uint32_t)'c' << 16 | (uint32_t)'m' << 24;
	modules[0].outputCount = 1;
	modules[0].inputBindingsOffset = 0;
	modules[1].inputCount = 1;
	modules[1].outputCount = 0;
	modules[0].inputCount = 0;
	modules[0].outputBindingsOffset = 0;
	modules[1].formatOffset = 0;
	modules[1].inputBindingsOffset = 0;
	modules[1].outputBindingsOffset = 0;
	modules[0].formatOffset = (uintptr_t)format - (uintptr_t)&modules[0];
	modules[0].moduleId = 0x5ce0;
	modules[1].moduleId = 0x5ced;
	definition->dspConfiguration = 1;
	definition->signature[0] = 'b';
	definition->signature[2] = 'd';
	definition->signature[3] = 'f';
	definition->reserved = 0;
	definition->nameOffset = (uintptr_t)"Master Buss" - (uintptr_t)definition;
	definition->modulesOffset = (uintptr_t)modules - (uintptr_t)definition;
	definition->signature[1] = 'v';
	definition->version = 0x10001;
	definition->size = sizeof(*definition);
	definition->moduleCount = 2;
	definition->nameLength = strnlen("Master Buss", 32);
	definition->descriptionOffset = (uintptr_t)
		"Master Buss, handles returning PCM data to user" - (uintptr_t)definition;
	definition->descriptionLength = strnlen(
		"Master Buss, handles returning PCM data to user", 128);
	definition->routingIndicesOffset =
		(uintptr_t)routingIndices - (uintptr_t)definition;
	definition->size = size;
	definition->presetsOffset = 0;
	definition->presetCount = 0;
	definition->voiceOutputCount = 0;
	definition->routingIndexCount = 2;

	sceSyscallLeave(syscallState);
	return (const struct SceNgsVoiceDefinition *)
		((uintptr_t)definition ^ 0x9e28dcce);
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetMixerBussInternal(void)
{
	unsigned int syscallState = sceSyscallEnter();
	unsigned char *cursor = ngsAlignVoiceDefinitionAddress(
		ngsMixerVoiceDefinitionStorage, 16);
	unsigned char *start = cursor;
	struct SceNgsVoiceDefinition *definition;
	NgsVoiceModuleDefinition *module;
	struct NgsAudioFormat *format;
	uint32_t *outputBindings;
	uint32_t *routingIndices;
	unsigned int size;

	definition = ngsAllocateMemory(&cursor, sizeof(*definition));
	module = ngsAllocateMemory(&cursor, sizeof(*module));
	format = ngsAllocateMemory(&cursor, sizeof(*format));
	outputBindings = ngsAllocateMemory(&cursor, sizeof(*outputBindings));
	routingIndices = ngsAllocateMemory(&cursor, sizeof(*routingIndices));
	*outputBindings = 0x80000000U;
	size = cursor - start;
	*routingIndices = 0;
	format->formatParameters[0] = 0;
	format->formatParameters[1] = 0;
	format->codecFourCC = (uint32_t)'l' | (uint32_t)'p' << 8 |
		(uint32_t)'c' << 16 | (uint32_t)'m' << 24;
	format->sampleFormat = 0x04343271;
	module->outputBindingsOffset = (uintptr_t)outputBindings - (uintptr_t)module;
	module->inputCount = 0;
	module->inputBindingsOffset = 0;
	module->formatOffset = (uintptr_t)format - (uintptr_t)module;
	module->moduleId = 0x5ce0;
	module->outputCount = 1;
	definition->dspConfiguration = 1;
	definition->reserved = 0;
	definition->signature[0] = 'b';
	definition->signature[2] = 'd';
	definition->modulesOffset = (uintptr_t)module - (uintptr_t)definition;
	definition->signature[3] = 'f';
	definition->version = 0x10001;
	definition->size = sizeof(*definition);
	definition->moduleCount = 1;
	definition->signature[1] = 'v';
	definition->nameOffset = (uintptr_t)"Mixer Buss" - (uintptr_t)definition;
	definition->nameLength = strnlen("Mixer Buss", 32);
	definition->descriptionOffset = (uintptr_t)
		"Mixer Buss, acts as a mixer for multiple input voices" -
		(uintptr_t)definition;
	definition->descriptionLength = strnlen(
		"Mixer Buss, acts as a mixer for multiple input voices", 128);
	definition->routingIndicesOffset =
		(uintptr_t)routingIndices - (uintptr_t)definition;
	definition->size = size;
	definition->presetsOffset = 0;
	definition->presetCount = 0;
	definition->voiceOutputCount = 1;
	definition->routingIndexCount = 1;

	sceSyscallLeave(syscallState);
	return (const struct SceNgsVoiceDefinition *)
		((uintptr_t)definition ^ 0x9e28dcce);
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetPauserBussInternal(void)
{
	unsigned int syscallState = sceSyscallEnter();
	unsigned char *cursor = ngsAlignVoiceDefinitionAddress(
		ngsPauserVoiceDefinitionStorage, 16);
	unsigned char *start = cursor;
	struct SceNgsVoiceDefinition *definition;
	NgsVoiceModuleDefinition *modules;
	struct NgsAudioFormat *format;
	uint32_t *outputBindings;
	uint32_t *inputIndices;
	uint32_t *routingIndices;
	unsigned int size;

	definition = ngsAllocateMemory(&cursor, sizeof(*definition));
	modules = ngsAllocateMemory(&cursor, 2 * sizeof(*modules));
	format = ngsAllocateMemory(&cursor, sizeof(*format));
	outputBindings = ngsAllocateMemory(&cursor, sizeof(*outputBindings));
	inputIndices = ngsAllocateMemory(&cursor, sizeof(*inputIndices));
	routingIndices = ngsAllocateMemory(&cursor, 2 * sizeof(*routingIndices));
	size = cursor - start;
	*outputBindings = 0x80000000U;
	*inputIndices = 0;
	routingIndices[0] = 0;
	routingIndices[1] = 1;
	format->codecFourCC = (uint32_t)'l' | (uint32_t)'p' << 8 |
		(uint32_t)'c' << 16 | (uint32_t)'m' << 24;
	format->sampleFormat = 0x04343271;
	format->formatParameters[0] = 0;
	format->formatParameters[1] = 0;
	modules[0].outputCount = 1;
	modules[0].inputBindingsOffset = 0;
	modules[1].inputCount = 1;
	modules[1].outputCount = 1;
	modules[1].formatOffset = (uintptr_t)format - (uintptr_t)&modules[1];
	modules[0].moduleId = 0x5ce0;
	modules[0].inputCount = 0;
	modules[0].outputBindingsOffset = 0;
	modules[1].inputBindingsOffset = 0;
	modules[0].formatOffset = (uintptr_t)format - (uintptr_t)&modules[0];
	modules[1].moduleId = 0x5ce5;
	modules[1].outputBindingsOffset =
		(uintptr_t)outputBindings - (uintptr_t)&modules[1];
	definition->size = size;
	definition->modulesOffset = (uintptr_t)modules - (uintptr_t)definition;
	definition->reserved = 0;
	definition->signature[0] = 'b';
	definition->signature[1] = 'v';
	definition->signature[2] = 'd';
	definition->signature[3] = 'f';
	definition->version = 0x10001;
	definition->nameOffset = (uintptr_t)"Pauser Buss" - (uintptr_t)definition;
	definition->moduleCount = 2;
	definition->dspConfiguration = 2;
	definition->nameLength = strnlen("Pauser Buss", 32);
	definition->descriptionOffset = (uintptr_t)
		"Pauser Buss: Input (mix) -> Pauser -> Output.\n" -
		(uintptr_t)definition;
	size = strnlen("Pauser Buss", 128);
	definition->presetsOffset = 0;
	definition->presetCount = 0;
	definition->descriptionLength = size;
	definition->routingIndicesOffset =
		(uintptr_t)routingIndices - (uintptr_t)definition;
	definition->voiceOutputCount = 1;
	definition->routingIndexCount = 2;
	sceSyscallLeave(syscallState);
	return (const struct SceNgsVoiceDefinition *)
		((uintptr_t)definition ^ 0x9e28dcce);
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetReverbBussInternal(void)
{
	unsigned int syscallState = sceSyscallEnter();
	unsigned char *cursor = ngsAlignVoiceDefinitionAddress(
		ngsReverbVoiceDefinitionStorage, 16);
	unsigned char *start = cursor;
	struct SceNgsVoiceDefinition *definition;
	NgsVoiceModuleDefinition *modules;
	struct NgsAudioFormat *format;
	uint32_t *outputBindings;
	uint32_t *inputIndices;
	uint32_t *routingIndices;
	unsigned int size;

	definition = ngsAllocateMemory(&cursor, sizeof(*definition));
	modules = ngsAllocateMemory(&cursor, 2 * sizeof(*modules));
	format = ngsAllocateMemory(&cursor, sizeof(*format));
	outputBindings = ngsAllocateMemory(&cursor, sizeof(*outputBindings));
	inputIndices = ngsAllocateMemory(&cursor, sizeof(*inputIndices));
	routingIndices = ngsAllocateMemory(&cursor, 2 * sizeof(*routingIndices));
	size = cursor - start;
	*outputBindings = 0x80000000U;
	*inputIndices = 0;
	routingIndices[0] = 0;
	routingIndices[1] = 1;
	format->codecFourCC = (uint32_t)'l' | (uint32_t)'p' << 8 |
		(uint32_t)'c' << 16 | (uint32_t)'m' << 24;
	format->sampleFormat = 0x04343271;
	format->formatParameters[0] = 0;
	format->formatParameters[1] = 0;
	modules[0].outputCount = 1;
	modules[0].inputBindingsOffset = 0;
	modules[1].inputCount = 1;
	modules[1].outputCount = 1;
	modules[0].formatOffset = (uintptr_t)format - (uintptr_t)&modules[0];
	modules[0].moduleId = 0x5ce0;
	modules[1].formatOffset = (uintptr_t)format - (uintptr_t)&modules[1];
	modules[1].moduleId = 0x5ce7;
	modules[1].outputBindingsOffset =
		(uintptr_t)outputBindings - (uintptr_t)&modules[1];
	modules[0].inputCount = 0;
	modules[0].outputBindingsOffset = 0;
	modules[1].inputBindingsOffset = 0;
	definition->size = size;
	definition->modulesOffset = (uintptr_t)modules - (uintptr_t)definition;
	definition->reserved = 0;
	definition->signature[0] = 'b';
	definition->signature[1] = 'v';
	definition->signature[2] = 'd';
	definition->signature[3] = 'f';
	definition->version = 0x10001;
	definition->nameOffset = (uintptr_t)"Reverb Buss" - (uintptr_t)definition;
	definition->moduleCount = 2;
	definition->dspConfiguration = 2;
	definition->nameLength = strnlen("Reverb Buss", 32);
	definition->descriptionOffset = (uintptr_t)
		"Reverb Buss: in(mix) -> reverb -> out" -
		(uintptr_t)definition;
	size = strnlen("Reverb Buss: in(mix) -> reverb -> out", 128);
	definition->presetsOffset = 0;
	definition->presetCount = 0;
	definition->descriptionLength = size;
	definition->routingIndicesOffset =
		(uintptr_t)routingIndices - (uintptr_t)definition;
	definition->voiceOutputCount = 1;
	definition->routingIndexCount = 2;
	sceSyscallLeave(syscallState);
	return (const struct SceNgsVoiceDefinition *)
		((uintptr_t)definition ^ 0x9e28dcce);
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetSimpleVoiceInternal(void)
{
	unsigned int syscallState = sceSyscallEnter();
	unsigned char *cursor = ngsAlignVoiceDefinitionAddress(
		ngsSimpleVoiceDefinitionStorage, 16);
	unsigned char *start = cursor;
	struct SceNgsVoiceDefinition *definition;
	NgsVoiceModuleDefinition *modules;
	struct NgsAudioFormat *format;
	uint32_t *firstOutput;
	uint32_t *secondOutput;
	uint32_t *firstInput;
	uint32_t *secondInput;
	uint32_t *routingIndices;
	uint32_t *unusedInput;
	uint32_t *pauserOutput;
	uint32_t *filterInput;
	unsigned int size;

	definition = ngsAllocateMemory(&cursor, sizeof(*definition));
	modules = ngsAllocateMemory(&cursor, 6 * sizeof(*modules));
	format = ngsAllocateMemory(&cursor, sizeof(*format));
	firstOutput = ngsAllocateMemory(&cursor, sizeof(*firstOutput));
	secondOutput = ngsAllocateMemory(&cursor, sizeof(*secondOutput));
	firstInput = ngsAllocateMemory(&cursor, sizeof(*firstInput));
	secondInput = ngsAllocateMemory(&cursor, sizeof(*secondInput));
	routingIndices = ngsAllocateMemory(&cursor, 6 * sizeof(*routingIndices));
	unusedInput = ngsAllocateMemory(&cursor, sizeof(*unusedInput));
	pauserOutput = ngsAllocateMemory(&cursor, sizeof(*pauserOutput));
	filterInput = ngsAllocateMemory(&cursor, sizeof(*filterInput));
	size = cursor - start;
	*firstOutput = 0x80000000U;
	*secondOutput = 0x80000001U;
	*firstInput = 0;
	*secondInput = 1;
	routingIndices[4] = 4;
	routingIndices[5] = 5;
	routingIndices[2] = 2;
	routingIndices[3] = 3;
	routingIndices[0] = 0;
	routingIndices[1] = 1;
	*unusedInput = 3;
	*pauserOutput = 2;
	*filterInput = 3;
	format->formatParameters[0] = 0;
	format->formatParameters[1] = 0;
	format->sampleFormat = 0x04343271;
	format->codecFourCC = (uint32_t)'l' | (uint32_t)'p' << 8 |
		(uint32_t)'c' << 16 | (uint32_t)'m' << 24;
	modules[0].inputCount = 0;
	modules[0].outputCount = 1;
	modules[0].inputBindingsOffset = 0;
	modules[0].outputBindingsOffset = 0;
	modules[1].inputCount = 1;
	modules[1].outputCount = 1;
	modules[1].inputBindingsOffset = 0;
	modules[1].outputBindingsOffset = 0;
	modules[2].inputCount = 1;
	modules[2].outputCount = 1;
	modules[0].moduleId = 0x5ce6;
	modules[0].formatOffset = (uintptr_t)format - (uintptr_t)&modules[0];
	modules[1].moduleId = 0x5cec;
	modules[2].inputBindingsOffset = 0;
	modules[2].outputBindingsOffset = 0;
	modules[3].inputCount = 1;
	modules[3].outputCount = 1;
	modules[4].inputCount = 1;
	modules[4].outputCount = 1;
	modules[5].inputCount = 1;
	modules[5].outputCount = 1;
	modules[2].formatOffset = (uintptr_t)format - (uintptr_t)&modules[2];
	modules[1].formatOffset = (uintptr_t)format - (uintptr_t)&modules[1];
	modules[2].moduleId = 0x5ce3;
	modules[3].formatOffset = (uintptr_t)format - (uintptr_t)&modules[3];
	modules[3].moduleId = 0x5ce5;
	modules[3].outputBindingsOffset =
		(uintptr_t)pauserOutput - (uintptr_t)&modules[3];
	modules[4].outputBindingsOffset =
		(uintptr_t)firstOutput - (uintptr_t)&modules[4];
	modules[4].inputBindingsOffset =
		(uintptr_t)filterInput - (uintptr_t)&modules[4];
	modules[3].inputBindingsOffset = 0;
	modules[4].formatOffset = (uintptr_t)format - (uintptr_t)&modules[4];
	modules[5].formatOffset = (uintptr_t)format - (uintptr_t)&modules[5];
	modules[5].inputBindingsOffset =
		(uintptr_t)filterInput - (uintptr_t)&modules[5];
	modules[5].outputBindingsOffset =
		(uintptr_t)secondOutput - (uintptr_t)&modules[5];
	modules[4].moduleId = 0x5ce4;
	modules[5].moduleId = 0x5ce4;
	ngsSimpleVoicePreset.nameOffset =
		(uintptr_t)"Basic Playback" - (uintptr_t)&ngsSimpleVoicePreset;
	ngsSimpleVoicePreset.nameLength = strnlen("Basic Playback", 32);
	ngsSimpleVoicePreset.indicesCount = 5;
	ngsSimpleVoicePreset.paramsOffset = 0;
	ngsSimpleVoicePreset.paramsSize = 0;
	definition->dspConfiguration = 2;
	ngsSimpleVoicePreset.indicesOffset =
		(uintptr_t)ngsSimpleVoicePresetIndices - (uintptr_t)&ngsSimpleVoicePreset;
	definition->size = size;
	definition->signature[1] = 'v';
	definition->signature[3] = 'f';
	definition->modulesOffset = (uintptr_t)modules - (uintptr_t)definition;
	definition->version = 0x10001;
	definition->reserved = 0;
	definition->nameOffset = (uintptr_t)"Simple Voice Def" - (uintptr_t)definition;
	definition->signature[0] = 'b';
	definition->signature[2] = 'd';
	definition->moduleCount = 6;
	definition->nameLength = strnlen("Simple Voice Def", 32);
	definition->descriptionOffset = (uintptr_t)
		"Basic voice. 2 sends with independent filter per send.\n" -
		(uintptr_t)definition;
	definition->descriptionLength = strnlen(
		"Basic voice. 2 sends with independent filter per send.\n", 128);
	definition->presetsOffset =
		(uintptr_t)&ngsSimpleVoicePreset - (uintptr_t)definition;
	definition->routingIndicesOffset =
		(uintptr_t)routingIndices - (uintptr_t)definition;
	definition->presetCount = 1;
	definition->voiceOutputCount = 2;
	definition->routingIndexCount = 6;

	sceSyscallLeave(syscallState);
	return (const struct SceNgsVoiceDefinition *)
		((uintptr_t)definition ^ 0x9e28dcce);
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetTemplate1Internal(void)
{
	unsigned int syscallState = sceSyscallEnter();
	unsigned char *cursor = ngsAlignVoiceDefinitionAddress(
		ngsTemplate1VoiceDefinitionStorage, 16);
	unsigned char *start = cursor;
	struct SceNgsVoiceDefinition *definition;
	NgsVoiceModuleDefinition *modules;
	struct NgsAudioFormat *format;
	uint32_t *firstOutput;
	uint32_t *secondOutput;
	uint32_t *thirdOutput;
	uint32_t *fourthOutput;
	uint32_t *routingIndices;
	uint32_t *distortionOutput;
	uint32_t *mixerInputs;
	uint32_t *filterInput;
	unsigned int size;
	unsigned int nameLength;

	definition = ngsAllocateMemory(&cursor, sizeof(*definition));
	modules = ngsAllocateMemory(&cursor, 10 * sizeof(*modules));
	format = ngsAllocateMemory(&cursor, sizeof(*format));
	firstOutput = ngsAllocateMemory(&cursor, sizeof(*firstOutput));
	secondOutput = ngsAllocateMemory(&cursor, sizeof(*secondOutput));
	thirdOutput = ngsAllocateMemory(&cursor, sizeof(*thirdOutput));
	fourthOutput = ngsAllocateMemory(&cursor, sizeof(*fourthOutput));
	routingIndices = ngsAllocateMemory(&cursor, 10 * sizeof(*routingIndices));
	distortionOutput = ngsAllocateMemory(&cursor, sizeof(*distortionOutput));
	mixerInputs = ngsAllocateMemory(&cursor, 2 * sizeof(*mixerInputs));
	filterInput = ngsAllocateMemory(&cursor, sizeof(*filterInput));
	size = cursor - start;
	format->formatParameters[0] = 0;
	format->formatParameters[1] = 0;
	format->codecFourCC = (uint32_t)'l' | (uint32_t)'p' << 8 |
		(uint32_t)'c' << 16 | (uint32_t)'m' << 24;
	format->sampleFormat = 0x04343271;
	*firstOutput = 0x80000000U;
	*secondOutput = 0x80000001U;
	*thirdOutput = 0x80000002U;
	*fourthOutput = 0x80000003U;
	routingIndices[9] = 9;
	routingIndices[7] = 7;
	routingIndices[3] = 3;
	routingIndices[4] = 4;
	routingIndices[5] = 5;
	routingIndices[6] = 6;
	routingIndices[8] = 8;
	routingIndices[0] = 0;
	routingIndices[1] = 1;
	routingIndices[2] = 2;
	*distortionOutput = 4;
	mixerInputs[0] = 0;
	mixerInputs[1] = 1;
	*filterInput = 5;
	modules[0].moduleId = 0x5ce6;
	modules[0].formatOffset = (uintptr_t)format - (uintptr_t)&modules[0];
	modules[0].inputBindingsOffset = 0;
	modules[0].outputBindingsOffset = 0;
	modules[1].moduleId = 0x5ce8;
	modules[3].inputCount = 1;
	modules[3].outputCount = 1;
	modules[4].inputCount = 1;
	modules[4].outputCount = 1;
	modules[5].inputCount = 1;
	modules[5].outputCount = 1;
	modules[6].inputCount = 1;
	modules[6].outputCount = 1;
	modules[3].inputBindingsOffset = 0;
	modules[3].outputBindingsOffset = 0;
	modules[4].inputBindingsOffset = 0;
	modules[4].outputBindingsOffset = 0;
	modules[2].inputCount = 2;
	modules[2].moduleId = 0x5ce9;
	modules[0].outputCount = 1;
	modules[1].outputCount = 1;
	modules[2].outputCount = 1;
	modules[7].inputCount = 1;
	modules[0].inputCount = 0;
	modules[1].formatOffset = (uintptr_t)format - (uintptr_t)&modules[1];
	modules[1].inputCount = 0;
	modules[1].inputBindingsOffset = 0;
	modules[1].outputBindingsOffset = 0;
	modules[2].outputBindingsOffset = 0;
	modules[5].inputBindingsOffset = 0;
	modules[6].outputBindingsOffset =
		(uintptr_t)firstOutput - (uintptr_t)&modules[6];
	modules[2].formatOffset = (uintptr_t)format - (uintptr_t)&modules[2];
	modules[5].outputBindingsOffset =
		(uintptr_t)distortionOutput - (uintptr_t)&modules[5];
	modules[5].formatOffset = (uintptr_t)format - (uintptr_t)&modules[5];
	modules[2].inputBindingsOffset =
		(uintptr_t)mixerInputs - (uintptr_t)&modules[2];
	modules[7].outputBindingsOffset =
		(uintptr_t)secondOutput - (uintptr_t)&modules[7];
	modules[7].outputCount = 1;
	modules[3].formatOffset = (uintptr_t)format - (uintptr_t)&modules[3];
	modules[4].moduleId = 0x5ce3;
	modules[4].formatOffset = (uintptr_t)format - (uintptr_t)&modules[4];
	modules[5].moduleId = 0x5ce2;
	modules[9].outputBindingsOffset =
		(uintptr_t)fourthOutput - (uintptr_t)&modules[9];
	modules[6].formatOffset = (uintptr_t)format - (uintptr_t)&modules[6];
	modules[8].outputBindingsOffset =
		(uintptr_t)thirdOutput - (uintptr_t)&modules[8];
	modules[7].formatOffset = (uintptr_t)format - (uintptr_t)&modules[7];
	modules[7].inputBindingsOffset =
		(uintptr_t)filterInput - (uintptr_t)&modules[7];
	modules[6].inputBindingsOffset =
		(uintptr_t)filterInput - (uintptr_t)&modules[6];
	modules[9].formatOffset = (uintptr_t)format - (uintptr_t)&modules[9];
	modules[8].inputBindingsOffset =
		(uintptr_t)filterInput - (uintptr_t)&modules[8];
	modules[9].inputBindingsOffset =
		(uintptr_t)filterInput - (uintptr_t)&modules[9];
	modules[3].moduleId = 0x5cec;
	modules[6].moduleId = 0x5cec;
	modules[7].moduleId = 0x5cec;
	modules[8].moduleId = 0x5cec;
	modules[8].inputCount = 1;
	modules[8].outputCount = 1;
	modules[9].moduleId = 0x5cec;
	modules[9].inputCount = 1;
	modules[9].outputCount = 1;
	modules[8].formatOffset = (uintptr_t)format - (uintptr_t)&modules[8];
	ngsTemplate1VoicePresets[0].nameOffset =
		(uintptr_t)"Basic Playback" - (uintptr_t)&ngsTemplate1VoicePresets[0];
	ngsTemplate1VoicePresets[0].nameLength = strnlen("Basic Playback", 32);
	ngsTemplate1VoicePresets[0].indicesOffset = 0;
	ngsTemplate1VoicePresets[0].indicesCount = 0;
	ngsTemplate1VoicePresets[0].paramsOffset = 0;
	ngsTemplate1VoicePresets[0].paramsSize = 0;
	ngsTemplate1VoicePresets[1].nameOffset =
		(uintptr_t)"All Mods On" - (uintptr_t)&ngsTemplate1VoicePresets[1];
	ngsTemplate1VoicePresets[1].nameLength = strnlen("All Mods On", 32);
	ngsTemplate1VoicePresets[1].indicesOffset = 0;
	ngsTemplate1VoicePresets[1].indicesCount = 0;
	ngsTemplate1VoicePresets[1].paramsOffset = 0;
	ngsTemplate1VoicePresets[1].paramsSize = 0;
	ngsTemplate1VoicePresets[2].nameOffset =
		(uintptr_t)"LibSas Voice" - (uintptr_t)&ngsTemplate1VoicePresets[2];
	ngsTemplate1VoicePresets[2].nameLength = strnlen("LibSas Voice", 32);
	ngsTemplate1VoicePresets[2].indicesOffset = 0;
	ngsTemplate1VoicePresets[2].indicesCount = 0;
	ngsTemplate1VoicePresets[2].paramsOffset = 0;
	ngsTemplate1VoicePresets[2].paramsSize = 0;
	ngsTemplate1VoicePresets[3].nameOffset =
		(uintptr_t)"Enviro Voice" - (uintptr_t)&ngsTemplate1VoicePresets[3];
	nameLength = strnlen("Enviro Voice", 32);
	ngsTemplate1VoicePresets[3].paramsOffset = 0;
	ngsTemplate1VoicePresets[3].paramsSize = 0;
	ngsTemplate1VoicePresets[0].indicesCount = 8;
	ngsTemplate1VoicePresets[3].nameLength = nameLength;
	ngsTemplate1VoicePresets[2].indicesCount = 6;
	ngsTemplate1VoicePresets[2].indicesOffset =
		(uintptr_t)ngsTemplate1LibSasIndices - (uintptr_t)&ngsTemplate1VoicePresets[2];
	ngsTemplate1VoicePresets[3].indicesCount = 5;
	ngsTemplate1VoicePresets[3].indicesOffset =
		(uintptr_t)ngsTemplate1EnviroIndices - (uintptr_t)&ngsTemplate1VoicePresets[3];
	ngsTemplate1VoicePresets[0].indicesOffset =
		(uintptr_t)ngsTemplate1PlaybackIndices - (uintptr_t)&ngsTemplate1VoicePresets[0];
	definition->dspConfiguration = 3;
	definition->reserved = 0;
	definition->version = 0x10001;
	definition->signature[1] = 'v';
	definition->modulesOffset = (uintptr_t)modules - (uintptr_t)definition;
	definition->signature[2] = 'd';
	definition->signature[3] = 'f';
	definition->nameOffset = (uintptr_t)"T1 Voice Def" - (uintptr_t)definition;
	definition->moduleCount = 10;
	definition->signature[0] = 'b';
	definition->nameLength = strnlen("T1 Voice Def", 32);
	definition->descriptionOffset = (uintptr_t)
		"T1 Voice Definition, a complex voice with waveform playback, signal generation and 4 separate sends." -
		(uintptr_t)definition;
	definition->descriptionLength = strnlen(
		"T1 Voice Definition, a complex voice with waveform playback, signal generation and 4 separate sends.", 128);
	definition->presetsOffset =
		(uintptr_t)ngsTemplate1VoicePresets - (uintptr_t)definition;
	definition->size = size;
	definition->routingIndicesOffset =
		(uintptr_t)routingIndices - (uintptr_t)definition;
	definition->presetCount = 4;
	definition->voiceOutputCount = 4;
	definition->routingIndexCount = 10;

	sceSyscallLeave(syscallState);
	return (const struct SceNgsVoiceDefinition *)
		((uintptr_t)definition ^ 0x9e28dcce);
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetAtrac9VoiceInternal(void)
{
	unsigned int syscallState = sceSyscallEnter();
	unsigned char *cursor = ngsAlignVoiceDefinitionAddress(
		ngsAtrac9VoiceDefinitionStorage, 16);
	unsigned char *start = cursor;
	struct SceNgsVoiceDefinition *definition;
	NgsVoiceModuleDefinition *modules;
	struct NgsAudioFormat *format;
	uint32_t *firstOutput;
	uint32_t *secondOutput;
	uint32_t *thirdOutput;
	uint32_t *fourthOutput;
	uint32_t *unusedChannelIndices;
	uint32_t *unusedFirstInput;
	uint32_t *unusedSecondInput;
	uint32_t *unusedThirdInput;
	uint32_t *routingIndices;
	uint32_t *distortionOutput;
	uint32_t *mixerInputs;
	uint32_t *filterInput;
	unsigned int size;
	unsigned int nameLength;

	definition = ngsAllocateMemory(&cursor, sizeof(*definition));
	modules = ngsAllocateMemory(&cursor, 10 * sizeof(*modules));
	format = ngsAllocateMemory(&cursor, sizeof(*format));
	firstOutput = ngsAllocateMemory(&cursor, sizeof(*firstOutput));
	secondOutput = ngsAllocateMemory(&cursor, sizeof(*secondOutput));
	thirdOutput = ngsAllocateMemory(&cursor, sizeof(*thirdOutput));
	fourthOutput = ngsAllocateMemory(&cursor, sizeof(*fourthOutput));
	unusedChannelIndices = ngsAllocateMemory(&cursor, 2 * sizeof(*unusedChannelIndices));
	unusedFirstInput = ngsAllocateMemory(&cursor, sizeof(*unusedFirstInput));
	unusedSecondInput = ngsAllocateMemory(&cursor, sizeof(*unusedSecondInput));
	unusedThirdInput = ngsAllocateMemory(&cursor, sizeof(*unusedThirdInput));
	routingIndices = ngsAllocateMemory(&cursor, 10 * sizeof(*routingIndices));
	filterInput = ngsAllocateMemory(&cursor, sizeof(*filterInput));
	distortionOutput = ngsAllocateMemory(&cursor, sizeof(*distortionOutput));
	mixerInputs = ngsAllocateMemory(&cursor, 2 * sizeof(*mixerInputs));
	size = cursor - start;
	format->formatParameters[0] = 0;
	format->formatParameters[1] = 0;
	format->codecFourCC = (uint32_t)'l' | (uint32_t)'p' << 8 |
		(uint32_t)'c' << 16 | (uint32_t)'m' << 24;
	format->sampleFormat = 0x04343271;
	*firstOutput = 0x80000000U;
	*secondOutput = 0x80000001U;
	*thirdOutput = 0x80000002U;
	*fourthOutput = 0x80000003U;
	unusedChannelIndices[0] = 0;
	unusedChannelIndices[1] = 1;
	*unusedFirstInput = 0;
	*unusedSecondInput = 1;
	*unusedThirdInput = 2;
	routingIndices[7] = 7;
	routingIndices[3] = 3;
	routingIndices[4] = 4;
	routingIndices[5] = 5;
	routingIndices[6] = 6;
	routingIndices[8] = 8;
	routingIndices[0] = 0;
	routingIndices[1] = 1;
	routingIndices[2] = 2;
	routingIndices[9] = 9;
	*distortionOutput = 4;
	mixerInputs[0] = 0;
	mixerInputs[1] = 1;
	*filterInput = 5;
	modules[0].moduleId = 0x5caa;
	modules[0].formatOffset = (uintptr_t)format - (uintptr_t)&modules[0];
	modules[0].inputBindingsOffset = 0;
	modules[0].outputBindingsOffset = 0;
	modules[1].moduleId = 0x5ce8;
	modules[3].inputCount = 1;
	modules[3].outputCount = 1;
	modules[4].inputCount = 1;
	modules[4].outputCount = 1;
	modules[5].inputCount = 1;
	modules[5].outputCount = 1;
	modules[6].inputCount = 1;
	modules[6].outputCount = 1;
	modules[3].inputBindingsOffset = 0;
	modules[3].outputBindingsOffset = 0;
	modules[4].inputBindingsOffset = 0;
	modules[4].outputBindingsOffset = 0;
	modules[2].inputCount = 2;
	modules[2].moduleId = 0x5ce9;
	modules[0].outputCount = 1;
	modules[1].outputCount = 1;
	modules[2].outputCount = 1;
	modules[7].inputCount = 1;
	modules[0].inputCount = 0;
	modules[1].formatOffset = (uintptr_t)format - (uintptr_t)&modules[1];
	modules[1].inputCount = 0;
	modules[1].inputBindingsOffset = 0;
	modules[1].outputBindingsOffset = 0;
	modules[2].outputBindingsOffset = 0;
	modules[5].inputBindingsOffset = 0;
	modules[6].outputBindingsOffset =
		(uintptr_t)firstOutput - (uintptr_t)&modules[6];
	modules[2].formatOffset = (uintptr_t)format - (uintptr_t)&modules[2];
	modules[5].outputBindingsOffset =
		(uintptr_t)distortionOutput - (uintptr_t)&modules[5];
	modules[5].formatOffset = (uintptr_t)format - (uintptr_t)&modules[5];
	modules[2].inputBindingsOffset =
		(uintptr_t)mixerInputs - (uintptr_t)&modules[2];
	modules[7].outputBindingsOffset =
		(uintptr_t)secondOutput - (uintptr_t)&modules[7];
	modules[7].outputCount = 1;
	modules[3].formatOffset = (uintptr_t)format - (uintptr_t)&modules[3];
	modules[4].moduleId = 0x5ce3;
	modules[4].formatOffset = (uintptr_t)format - (uintptr_t)&modules[4];
	modules[5].moduleId = 0x5ce2;
	modules[9].outputBindingsOffset =
		(uintptr_t)fourthOutput - (uintptr_t)&modules[9];
	modules[6].formatOffset = (uintptr_t)format - (uintptr_t)&modules[6];
	modules[8].outputBindingsOffset =
		(uintptr_t)thirdOutput - (uintptr_t)&modules[8];
	modules[7].formatOffset = (uintptr_t)format - (uintptr_t)&modules[7];
	modules[7].inputBindingsOffset =
		(uintptr_t)&ngsAtrac9FilterInput - (uintptr_t)&modules[7];
	modules[6].inputBindingsOffset =
		(uintptr_t)&ngsAtrac9FilterInput - (uintptr_t)&modules[6];
	modules[9].formatOffset = (uintptr_t)format - (uintptr_t)&modules[9];
	modules[8].inputBindingsOffset =
		(uintptr_t)&ngsAtrac9FilterInput - (uintptr_t)&modules[8];
	modules[9].inputBindingsOffset =
		(uintptr_t)&ngsAtrac9FilterInput - (uintptr_t)&modules[9];
	modules[3].moduleId = 0x5cec;
	modules[6].moduleId = 0x5cec;
	modules[7].moduleId = 0x5cec;
	modules[8].moduleId = 0x5cec;
	modules[8].inputCount = 1;
	modules[8].outputCount = 1;
	modules[9].moduleId = 0x5cec;
	modules[9].inputCount = 1;
	modules[9].outputCount = 1;
	modules[8].formatOffset = (uintptr_t)format - (uintptr_t)&modules[8];
	ngsAtrac9VoicePresets[0].nameOffset =
		(uintptr_t)"Basic Playback" - (uintptr_t)&ngsAtrac9VoicePresets[0];
	ngsAtrac9VoicePresets[0].nameLength = strnlen("Basic Playback", 32);
	ngsAtrac9VoicePresets[0].indicesOffset = 0;
	ngsAtrac9VoicePresets[0].indicesCount = 0;
	ngsAtrac9VoicePresets[0].paramsOffset = 0;
	ngsAtrac9VoicePresets[0].paramsSize = 0;
	ngsAtrac9VoicePresets[1].nameOffset =
		(uintptr_t)"All Mods On" - (uintptr_t)&ngsAtrac9VoicePresets[1];
	ngsAtrac9VoicePresets[1].nameLength = strnlen("All Mods On", 32);
	ngsAtrac9VoicePresets[1].indicesOffset = 0;
	ngsAtrac9VoicePresets[1].indicesCount = 0;
	ngsAtrac9VoicePresets[1].paramsOffset = 0;
	ngsAtrac9VoicePresets[1].paramsSize = 0;
	ngsAtrac9VoicePresets[2].nameOffset =
		(uintptr_t)"LibSas Voice" - (uintptr_t)&ngsAtrac9VoicePresets[2];
	ngsAtrac9VoicePresets[2].nameLength = strnlen("LibSas Voice", 32);
	ngsAtrac9VoicePresets[2].indicesOffset = 0;
	ngsAtrac9VoicePresets[2].indicesCount = 0;
	ngsAtrac9VoicePresets[2].paramsOffset = 0;
	ngsAtrac9VoicePresets[2].paramsSize = 0;
	ngsAtrac9VoicePresets[3].nameOffset =
		(uintptr_t)"Enviro Voice" - (uintptr_t)&ngsAtrac9VoicePresets[3];
	nameLength = strnlen("Enviro Voice", 32);
	ngsAtrac9VoicePresets[3].paramsOffset = 0;
	ngsAtrac9VoicePresets[3].paramsSize = 0;
	ngsAtrac9VoicePresets[0].indicesCount = 8;
	ngsAtrac9VoicePresets[3].nameLength = nameLength;
	ngsAtrac9VoicePresets[2].indicesCount = 6;
	ngsAtrac9VoicePresets[2].indicesOffset =
		(uintptr_t)ngsAtrac9LibSasIndices - (uintptr_t)&ngsAtrac9VoicePresets[2];
	ngsAtrac9VoicePresets[3].indicesCount = 5;
	ngsAtrac9VoicePresets[3].indicesOffset =
		(uintptr_t)ngsAtrac9EnviroIndices - (uintptr_t)&ngsAtrac9VoicePresets[3];
	ngsAtrac9VoicePresets[0].indicesOffset =
		(uintptr_t)ngsAtrac9PlaybackIndices - (uintptr_t)&ngsAtrac9VoicePresets[0];
	definition->dspConfiguration = 3;
	definition->reserved = 0;
	definition->version = 0x10001;
	definition->size = sizeof(*definition);
	definition->signature[1] = 'v';
	definition->modulesOffset = (uintptr_t)modules - (uintptr_t)definition;
	definition->signature[2] = 'd';
	definition->signature[3] = 'f';
	definition->nameOffset = (uintptr_t)"ATRAC9 Voice Def" - (uintptr_t)definition;
	definition->moduleCount = 10;
	definition->signature[0] = 'b';
	definition->nameLength = strnlen("ATRAC9 Voice Def", 32);
	definition->descriptionOffset = (uintptr_t)
		"ATRAC9 Voice Definition, a complex voice with AT9 playback, signal generation and 4 separate sends." -
		(uintptr_t)definition;
	definition->descriptionLength = strnlen(
		"ATRAC9 Voice Definition, a complex voice with AT9 playback, signal generation and 4 separate sends.", 128);
	definition->presetsOffset =
		(uintptr_t)ngsAtrac9VoicePresets - (uintptr_t)definition;
	definition->size = size;
	definition->routingIndicesOffset =
		(uintptr_t)routingIndices - (uintptr_t)definition;
	definition->presetCount = 4;
	definition->voiceOutputCount = 4;
	definition->routingIndexCount = 10;

	sceSyscallLeave(syscallState);
	return (const struct SceNgsVoiceDefinition *)
		((uintptr_t)definition ^ 0x9e28dcce);
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetDelayBussInternal(void)
{
	unsigned int syscallState = sceSyscallEnter();
	unsigned char *cursor = ngsAlignVoiceDefinitionAddress(
		ngsDelayVoiceDefinitionStorage, 16);
	unsigned char *start = cursor;
	struct SceNgsVoiceDefinition *definition;
	NgsVoiceModuleDefinition *modules;
	struct NgsAudioFormat *format;
	uint32_t *outputBindings;
	uint32_t *inputIndices;
	uint32_t *routingIndices;
	unsigned int size;

	definition = ngsAllocateMemory(&cursor, sizeof(*definition));
	modules = ngsAllocateMemory(&cursor, 2 * sizeof(*modules));
	format = ngsAllocateMemory(&cursor, sizeof(*format));
	outputBindings = ngsAllocateMemory(&cursor, sizeof(*outputBindings));
	inputIndices = ngsAllocateMemory(&cursor, sizeof(*inputIndices));
	routingIndices = ngsAllocateMemory(&cursor, 2 * sizeof(*routingIndices));
	size = cursor - start;
	*outputBindings = 0x80000000U;
	*inputIndices = 0;
	routingIndices[0] = 0;
	routingIndices[1] = 1;
	format->codecFourCC = (uint32_t)'l' | (uint32_t)'p' << 8 |
		(uint32_t)'c' << 16 | (uint32_t)'m' << 24;
	format->sampleFormat = 0x04343271;
	format->formatParameters[0] = 0;
	format->formatParameters[1] = 0;
	modules[0].outputCount = 1;
	modules[0].inputBindingsOffset = 0;
	modules[1].inputCount = 1;
	modules[1].outputCount = 1;
	modules[1].formatOffset = (uintptr_t)format - (uintptr_t)&modules[1];
	modules[0].moduleId = 0x5ce0;
	modules[0].inputCount = 0;
	modules[0].outputBindingsOffset = 0;
	modules[1].inputBindingsOffset = 0;
	modules[0].formatOffset = (uintptr_t)format - (uintptr_t)&modules[0];
	modules[1].moduleId = 0x5ceb;
	modules[1].outputBindingsOffset =
		(uintptr_t)outputBindings - (uintptr_t)&modules[1];
	definition->size = size;
	definition->modulesOffset = (uintptr_t)modules - (uintptr_t)definition;
	definition->reserved = 0;
	definition->signature[0] = 'b';
	definition->signature[1] = 'v';
	definition->signature[2] = 'd';
	definition->signature[3] = 'f';
	definition->version = 0x10001;
	definition->nameOffset = (uintptr_t)"Delay Buss" - (uintptr_t)definition;
	definition->moduleCount = 2;
	definition->dspConfiguration = 2;
	definition->nameLength = strnlen("Delay Buss", 32);
	definition->descriptionOffset = (uintptr_t)
		"Delay Buss: Input (mix) -> Delay -> Output.\n" -
		(uintptr_t)definition;
	size = strnlen("Delay Buss", 128);
	definition->presetsOffset = 0;
	definition->presetCount = 0;
	definition->descriptionLength = size;
	definition->routingIndicesOffset =
		(uintptr_t)routingIndices - (uintptr_t)definition;
	definition->voiceOutputCount = 1;
	definition->routingIndexCount = 2;
	sceSyscallLeave(syscallState);
	return (const struct SceNgsVoiceDefinition *)
		((uintptr_t)definition ^ 0x9e28dcce);
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetSasEmuVoiceInternal(void)
{
	unsigned int syscallState = sceSyscallEnter();
	unsigned char *cursor = ngsAlignVoiceDefinitionAddress(
		ngsSasEmuVoiceDefinitionStorage, 16);
	unsigned char *start = cursor;
	struct SceNgsVoiceDefinition *definition;
	NgsVoiceModuleDefinition *modules;
	struct NgsAudioFormat *format;
	uint32_t *outputBindings;
	uint32_t *inputIndices;
	uint32_t *routingIndices;
	uint32_t *unusedOutputBinding;
	uint32_t *unusedSecondInput;
	uint32_t *unusedPauserOutput;
	uint32_t *unusedFirstFilterInput;
	uint32_t *unusedSecondFilterInput;
	unsigned int size;

	definition = ngsAllocateMemory(&cursor, sizeof(*definition));
	modules = ngsAllocateMemory(&cursor, 2 * sizeof(*modules));
	format = ngsAllocateMemory(&cursor, sizeof(*format));
	outputBindings = ngsAllocateMemory(&cursor, sizeof(*outputBindings));
	unusedOutputBinding = ngsAllocateMemory(&cursor, sizeof(*unusedOutputBinding));
	inputIndices = ngsAllocateMemory(&cursor, sizeof(*inputIndices));
	unusedSecondInput = ngsAllocateMemory(&cursor, sizeof(*unusedSecondInput));
	routingIndices = ngsAllocateMemory(&cursor, 2 * sizeof(*routingIndices));
	unusedPauserOutput = ngsAllocateMemory(&cursor, sizeof(*unusedPauserOutput));
	unusedFirstFilterInput = ngsAllocateMemory(&cursor, sizeof(*unusedFirstFilterInput));
	unusedSecondFilterInput = ngsAllocateMemory(&cursor, sizeof(*unusedSecondFilterInput));
	size = cursor - start;
	*outputBindings = 0x80000000U;
	*unusedOutputBinding = 0x80000001U;
	*inputIndices = 0;
	*unusedSecondInput = 1;
	routingIndices[0] = 0;
	routingIndices[1] = 1;
	*unusedPauserOutput = 2;
	*unusedFirstFilterInput = 2;
	*unusedSecondFilterInput = 2;
	format->codecFourCC = (uint32_t)'l' | (uint32_t)'p' << 8 |
		(uint32_t)'c' << 16 | (uint32_t)'m' << 24;
	format->sampleFormat = 0x04343271;
	format->formatParameters[0] = 0;
	format->formatParameters[1] = 0;
	modules[0].outputCount = 1;
	modules[0].inputBindingsOffset = 0;
	modules[1].inputCount = 1;
	modules[1].outputCount = 1;
	modules[1].formatOffset = (uintptr_t)format - (uintptr_t)&modules[1];
	modules[0].moduleId = 0x5ce6;
	modules[0].inputCount = 0;
	modules[0].outputBindingsOffset = 0;
	modules[1].inputBindingsOffset = 0;
	modules[0].formatOffset = (uintptr_t)format - (uintptr_t)&modules[0];
	modules[1].moduleId = 0x5ce3;
	modules[1].outputBindingsOffset =
		(uintptr_t)outputBindings - (uintptr_t)&modules[1];
	definition->size = size;
	definition->modulesOffset = (uintptr_t)modules - (uintptr_t)definition;
	definition->reserved = 0;
	definition->signature[0] = 'b';
	definition->signature[1] = 'v';
	definition->signature[2] = 'd';
	definition->signature[3] = 'f';
	definition->version = 0x10001;
	definition->nameOffset = (uintptr_t)"Sas Emu Voice" - (uintptr_t)definition;
	definition->moduleCount = 2;
	definition->dspConfiguration = 2;
	definition->nameLength = strnlen("Sas Emu Voice", 32);
	definition->descriptionOffset = (uintptr_t)
		"libSas emulation voice.\n" -
		(uintptr_t)definition;
	size = strnlen("libSas emulation voice.\n", 128);
	definition->presetsOffset = 0;
	definition->presetCount = 0;
	definition->descriptionLength = size;
	definition->routingIndicesOffset =
		(uintptr_t)routingIndices - (uintptr_t)definition;
	definition->voiceOutputCount = 2;
	definition->routingIndexCount = 2;
	sceSyscallLeave(syscallState);
	return (const struct SceNgsVoiceDefinition *)
		((uintptr_t)definition ^ 0x9e28dcce);
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetSimpleAtrac9VoiceInternal(void)
{
	unsigned int syscallState = sceSyscallEnter();
	unsigned char *cursor = ngsAlignVoiceDefinitionAddress(
		ngsSimpleAtrac9VoiceDefinitionStorage, 16);
	unsigned char *start = cursor;
	struct SceNgsVoiceDefinition *definition;
	NgsVoiceModuleDefinition *modules;
	struct NgsAudioFormat *format;
	uint32_t *firstOutput;
	uint32_t *secondOutput;
	uint32_t *firstInput;
	uint32_t *secondInput;
	uint32_t *routingIndices;
	uint32_t *unusedInput;
	uint32_t *pauserOutput;
	uint32_t *filterInput;
	unsigned int size;

	definition = ngsAllocateMemory(&cursor, sizeof(*definition));
	modules = ngsAllocateMemory(&cursor, 6 * sizeof(*modules));
	format = ngsAllocateMemory(&cursor, sizeof(*format));
	firstOutput = ngsAllocateMemory(&cursor, sizeof(*firstOutput));
	secondOutput = ngsAllocateMemory(&cursor, sizeof(*secondOutput));
	firstInput = ngsAllocateMemory(&cursor, sizeof(*firstInput));
	secondInput = ngsAllocateMemory(&cursor, sizeof(*secondInput));
	routingIndices = ngsAllocateMemory(&cursor, 6 * sizeof(*routingIndices));
	unusedInput = ngsAllocateMemory(&cursor, sizeof(*unusedInput));
	pauserOutput = ngsAllocateMemory(&cursor, sizeof(*pauserOutput));
	filterInput = ngsAllocateMemory(&cursor, sizeof(*filterInput));
	size = cursor - start;
	*firstOutput = 0x80000000U;
	*secondOutput = 0x80000001U;
	*firstInput = 0;
	*secondInput = 1;
	routingIndices[4] = 4;
	routingIndices[5] = 5;
	routingIndices[2] = 2;
	routingIndices[3] = 3;
	routingIndices[0] = 0;
	routingIndices[1] = 1;
	*unusedInput = 3;
	*pauserOutput = 2;
	*filterInput = 3;
	format->formatParameters[0] = 0;
	format->formatParameters[1] = 0;
	format->sampleFormat = 0x04343271;
	format->codecFourCC = (uint32_t)'l' | (uint32_t)'p' << 8 |
		(uint32_t)'c' << 16 | (uint32_t)'m' << 24;
	modules[0].inputCount = 0;
	modules[0].outputCount = 1;
	modules[0].inputBindingsOffset = 0;
	modules[0].outputBindingsOffset = 0;
	modules[1].inputCount = 1;
	modules[1].outputCount = 1;
	modules[1].inputBindingsOffset = 0;
	modules[1].outputBindingsOffset = 0;
	modules[2].inputCount = 1;
	modules[2].outputCount = 1;
	modules[0].moduleId = 0x5caa;
	modules[0].formatOffset = (uintptr_t)format - (uintptr_t)&modules[0];
	modules[1].moduleId = 0x5cec;
	modules[2].inputBindingsOffset = 0;
	modules[2].outputBindingsOffset = 0;
	modules[3].inputCount = 1;
	modules[3].outputCount = 1;
	modules[4].inputCount = 1;
	modules[4].outputCount = 1;
	modules[5].inputCount = 1;
	modules[5].outputCount = 1;
	modules[2].formatOffset = (uintptr_t)format - (uintptr_t)&modules[2];
	modules[1].formatOffset = (uintptr_t)format - (uintptr_t)&modules[1];
	modules[2].moduleId = 0x5ce3;
	modules[3].formatOffset = (uintptr_t)format - (uintptr_t)&modules[3];
	modules[3].moduleId = 0x5ce5;
	modules[3].outputBindingsOffset =
		(uintptr_t)pauserOutput - (uintptr_t)&modules[3];
	modules[4].outputBindingsOffset =
		(uintptr_t)firstOutput - (uintptr_t)&modules[4];
	modules[4].inputBindingsOffset =
		(uintptr_t)filterInput - (uintptr_t)&modules[4];
	modules[3].inputBindingsOffset = 0;
	modules[4].formatOffset = (uintptr_t)format - (uintptr_t)&modules[4];
	modules[5].formatOffset = (uintptr_t)format - (uintptr_t)&modules[5];
	modules[5].inputBindingsOffset =
		(uintptr_t)filterInput - (uintptr_t)&modules[5];
	modules[5].outputBindingsOffset =
		(uintptr_t)secondOutput - (uintptr_t)&modules[5];
	modules[4].moduleId = 0x5ce4;
	modules[5].moduleId = 0x5ce4;
	ngsSimpleAtrac9VoicePreset.nameOffset =
		(uintptr_t)"Basic Playback" - (uintptr_t)&ngsSimpleAtrac9VoicePreset;
	ngsSimpleAtrac9VoicePreset.nameLength = strnlen("Basic Playback", 32);
	ngsSimpleAtrac9VoicePreset.indicesCount = 5;
	ngsSimpleAtrac9VoicePreset.paramsOffset = 0;
	ngsSimpleAtrac9VoicePreset.paramsSize = 0;
	definition->dspConfiguration = 2;
	ngsSimpleAtrac9VoicePreset.indicesOffset =
		(uintptr_t)ngsSimpleAtrac9VoicePresetIndices - (uintptr_t)&ngsSimpleAtrac9VoicePreset;
	definition->size = size;
	definition->signature[1] = 'v';
	definition->signature[3] = 'f';
	definition->modulesOffset = (uintptr_t)modules - (uintptr_t)definition;
	definition->version = 0x10001;
	definition->reserved = 0;
	definition->nameOffset = (uintptr_t)"Simple Voice Def At9" - (uintptr_t)definition;
	definition->signature[0] = 'b';
	definition->signature[2] = 'd';
	definition->moduleCount = 6;
	definition->nameLength = strnlen("Simple Voice Def At9", 32);
	definition->descriptionOffset = (uintptr_t)
		"Basic Atrac9 voice. 2 sends with independent filter per send.\n" -
		(uintptr_t)definition;
	definition->descriptionLength = strnlen(
		"Basic Atrac9 voice. 2 sends with independent filter per send.\n", 128);
	definition->presetsOffset =
		(uintptr_t)&ngsSimpleAtrac9VoicePreset - (uintptr_t)definition;
	definition->routingIndicesOffset =
		(uintptr_t)routingIndices - (uintptr_t)definition;
	definition->presetCount = 1;
	definition->voiceOutputCount = 2;
	definition->routingIndexCount = 6;

	sceSyscallLeave(syscallState);
	return (const struct SceNgsVoiceDefinition *)
		((uintptr_t)definition ^ 0x9e28dcce);
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetScreamVoiceInternal(void)
{
	unsigned int syscallState = sceSyscallEnter();
	unsigned char *cursor = ngsAlignVoiceDefinitionAddress(
		ngsScreamVoiceDefinitionStorage, 16);
	unsigned char *start = cursor;
	struct SceNgsVoiceDefinition *definition;
	NgsVoiceModuleDefinition *modules;
	struct NgsAudioFormat *format;
	uint32_t *firstOutput;
	uint32_t *secondOutput;
	uint32_t *firstInput;
	uint32_t *secondInput;
	uint32_t *routingIndices;
	uint32_t *equalizerOutput;
	uint32_t *filterInput;
	unsigned int size;

	definition = ngsAllocateMemory(&cursor, sizeof(*definition));
	modules = ngsAllocateMemory(&cursor, 6 * sizeof(*modules));
	format = ngsAllocateMemory(&cursor, sizeof(*format));
	firstOutput = ngsAllocateMemory(&cursor, sizeof(*firstOutput));
	secondOutput = ngsAllocateMemory(&cursor, sizeof(*secondOutput));
	firstInput = ngsAllocateMemory(&cursor, sizeof(*firstInput));
	secondInput = ngsAllocateMemory(&cursor, sizeof(*secondInput));
	routingIndices = ngsAllocateMemory(&cursor, 6 * sizeof(*routingIndices));
	equalizerOutput = ngsAllocateMemory(&cursor, sizeof(*equalizerOutput));
	filterInput = ngsAllocateMemory(&cursor, sizeof(*filterInput));
	size = cursor - start;
	*firstOutput = 0x80000000U;
	*secondOutput = 0x80000001U;
	*firstInput = 0;
	*secondInput = 1;
	routingIndices[4] = 4;
	routingIndices[5] = 5;
	routingIndices[2] = 2;
	routingIndices[3] = 3;
	routingIndices[0] = 0;
	routingIndices[1] = 1;
	*equalizerOutput = 2;
	*filterInput = 3;
	format->formatParameters[0] = 0;
	format->formatParameters[1] = 0;
	format->sampleFormat = 0x04343271;
	format->codecFourCC = (uint32_t)'l' | (uint32_t)'p' << 8 |
		(uint32_t)'c' << 16 | (uint32_t)'m' << 24;
	modules[0].inputCount = 0;
	modules[0].outputCount = 1;
	modules[0].inputBindingsOffset = 0;
	modules[0].outputBindingsOffset = 0;
	modules[1].inputCount = 1;
	modules[1].outputCount = 1;
	modules[1].inputBindingsOffset = 0;
	modules[1].outputBindingsOffset = 0;
	modules[2].inputCount = 1;
	modules[2].outputCount = 1;
	modules[0].moduleId = 0x5ce6;
	modules[0].formatOffset = (uintptr_t)format - (uintptr_t)&modules[0];
	modules[1].moduleId = 0x5ce3;
	modules[2].inputBindingsOffset = 0;
	modules[2].outputBindingsOffset = 0;
	modules[3].inputCount = 1;
	modules[3].outputCount = 1;
	modules[4].inputCount = 1;
	modules[4].outputCount = 1;
	modules[5].inputCount = 1;
	modules[5].outputCount = 1;
	modules[2].formatOffset = (uintptr_t)format - (uintptr_t)&modules[2];
	modules[1].formatOffset = (uintptr_t)format - (uintptr_t)&modules[1];
	modules[2].moduleId = 0x5ce2;
	modules[3].formatOffset = (uintptr_t)format - (uintptr_t)&modules[3];
	modules[3].moduleId = 0x5cec;
	modules[3].outputBindingsOffset =
		(uintptr_t)equalizerOutput - (uintptr_t)&modules[3];
	modules[4].outputBindingsOffset =
		(uintptr_t)firstOutput - (uintptr_t)&modules[4];
	modules[4].inputBindingsOffset =
		(uintptr_t)filterInput - (uintptr_t)&modules[4];
	modules[3].inputBindingsOffset = 0;
	modules[4].formatOffset = (uintptr_t)format - (uintptr_t)&modules[4];
	modules[5].formatOffset = (uintptr_t)format - (uintptr_t)&modules[5];
	modules[5].inputBindingsOffset =
		(uintptr_t)filterInput - (uintptr_t)&modules[5];
	modules[5].outputBindingsOffset =
		(uintptr_t)secondOutput - (uintptr_t)&modules[5];
	modules[4].moduleId = 0x5ce4;
	modules[5].moduleId = 0x5ce5;
	ngsScreamVoicePreset.nameOffset =
		(uintptr_t)"Basic Playback" - (uintptr_t)&ngsScreamVoicePreset;
	ngsScreamVoicePreset.nameLength = strnlen("Basic Playback", 32);
	ngsScreamVoicePreset.indicesCount = 5;
	ngsScreamVoicePreset.paramsOffset = 0;
	ngsScreamVoicePreset.paramsSize = 0;
	definition->dspConfiguration = 2;
	ngsScreamVoicePreset.indicesOffset =
		(uintptr_t)ngsScreamVoicePresetIndices - (uintptr_t)&ngsScreamVoicePreset;
	definition->size = size;
	definition->signature[1] = 'v';
	definition->signature[3] = 'f';
	definition->modulesOffset = (uintptr_t)modules - (uintptr_t)definition;
	definition->version = 0x10001;
	definition->reserved = 0;
	definition->nameOffset = (uintptr_t)"Scream Voice Def" - (uintptr_t)definition;
	definition->signature[0] = 'b';
	definition->signature[2] = 'd';
	definition->moduleCount = 6;
	definition->nameLength = strnlen("Scream Voice Def", 32);
	definition->descriptionOffset = (uintptr_t)
		"Scream voice.\n" -
		(uintptr_t)definition;
	definition->descriptionLength = strnlen(
		"Scream voice.\n", 128);
	definition->presetsOffset =
		(uintptr_t)&ngsScreamVoicePreset - (uintptr_t)definition;
	definition->routingIndicesOffset =
		(uintptr_t)routingIndices - (uintptr_t)definition;
	definition->presetCount = 1;
	definition->voiceOutputCount = 2;
	definition->routingIndexCount = 6;

	sceSyscallLeave(syscallState);
	return (const struct SceNgsVoiceDefinition *)
		((uintptr_t)definition ^ 0x9e28dcce);
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetScreamVoiceAT9Internal(void)
{
	unsigned int syscallState = sceSyscallEnter();
	unsigned char *cursor = ngsAlignVoiceDefinitionAddress(
		ngsScreamAt9VoiceDefinitionStorage, 16);
	unsigned char *start = cursor;
	struct SceNgsVoiceDefinition *definition;
	NgsVoiceModuleDefinition *modules;
	struct NgsAudioFormat *format;
	uint32_t *firstOutput;
	uint32_t *secondOutput;
	uint32_t *firstInput;
	uint32_t *secondInput;
	uint32_t *routingIndices;
	uint32_t *equalizerOutput;
	uint32_t *filterInput;
	unsigned int size;

	definition = ngsAllocateMemory(&cursor, sizeof(*definition));
	modules = ngsAllocateMemory(&cursor, 6 * sizeof(*modules));
	format = ngsAllocateMemory(&cursor, sizeof(*format));
	firstOutput = ngsAllocateMemory(&cursor, sizeof(*firstOutput));
	secondOutput = ngsAllocateMemory(&cursor, sizeof(*secondOutput));
	firstInput = ngsAllocateMemory(&cursor, sizeof(*firstInput));
	secondInput = ngsAllocateMemory(&cursor, sizeof(*secondInput));
	routingIndices = ngsAllocateMemory(&cursor, 6 * sizeof(*routingIndices));
	equalizerOutput = ngsAllocateMemory(&cursor, sizeof(*equalizerOutput));
	filterInput = ngsAllocateMemory(&cursor, sizeof(*filterInput));
	size = cursor - start;
	*firstOutput = 0x80000000U;
	*secondOutput = 0x80000001U;
	*firstInput = 0;
	*secondInput = 1;
	routingIndices[4] = 4;
	routingIndices[5] = 5;
	routingIndices[2] = 2;
	routingIndices[3] = 3;
	routingIndices[0] = 0;
	routingIndices[1] = 1;
	*equalizerOutput = 2;
	*filterInput = 3;
	format->formatParameters[0] = 0;
	format->formatParameters[1] = 0;
	format->sampleFormat = 0x04343271;
	format->codecFourCC = (uint32_t)'l' | (uint32_t)'p' << 8 |
		(uint32_t)'c' << 16 | (uint32_t)'m' << 24;
	modules[0].inputCount = 0;
	modules[0].outputCount = 1;
	modules[0].inputBindingsOffset = 0;
	modules[0].outputBindingsOffset = 0;
	modules[1].inputCount = 1;
	modules[1].outputCount = 1;
	modules[1].inputBindingsOffset = 0;
	modules[1].outputBindingsOffset = 0;
	modules[2].inputCount = 1;
	modules[2].outputCount = 1;
	modules[0].moduleId = 0x5caa;
	modules[0].formatOffset = (uintptr_t)format - (uintptr_t)&modules[0];
	modules[1].moduleId = 0x5ce3;
	modules[2].inputBindingsOffset = 0;
	modules[2].outputBindingsOffset = 0;
	modules[3].inputCount = 1;
	modules[3].outputCount = 1;
	modules[4].inputCount = 1;
	modules[4].outputCount = 1;
	modules[5].inputCount = 1;
	modules[5].outputCount = 1;
	modules[2].formatOffset = (uintptr_t)format - (uintptr_t)&modules[2];
	modules[1].formatOffset = (uintptr_t)format - (uintptr_t)&modules[1];
	modules[2].moduleId = 0x5ce2;
	modules[3].formatOffset = (uintptr_t)format - (uintptr_t)&modules[3];
	modules[3].moduleId = 0x5cec;
	modules[3].outputBindingsOffset =
		(uintptr_t)equalizerOutput - (uintptr_t)&modules[3];
	modules[4].outputBindingsOffset =
		(uintptr_t)firstOutput - (uintptr_t)&modules[4];
	modules[4].inputBindingsOffset =
		(uintptr_t)filterInput - (uintptr_t)&modules[4];
	modules[3].inputBindingsOffset = 0;
	modules[4].formatOffset = (uintptr_t)format - (uintptr_t)&modules[4];
	modules[5].formatOffset = (uintptr_t)format - (uintptr_t)&modules[5];
	modules[5].inputBindingsOffset =
		(uintptr_t)filterInput - (uintptr_t)&modules[5];
	modules[5].outputBindingsOffset =
		(uintptr_t)secondOutput - (uintptr_t)&modules[5];
	modules[4].moduleId = 0x5ce4;
	modules[5].moduleId = 0x5ce5;
	ngsScreamAt9VoicePreset.nameOffset =
		(uintptr_t)"Basic Playback" - (uintptr_t)&ngsScreamAt9VoicePreset;
	ngsScreamAt9VoicePreset.nameLength = strnlen("Basic Playback", 32);
	ngsScreamAt9VoicePreset.indicesCount = 5;
	ngsScreamAt9VoicePreset.paramsOffset = 0;
	ngsScreamAt9VoicePreset.paramsSize = 0;
	definition->dspConfiguration = 2;
	ngsScreamAt9VoicePreset.indicesOffset =
		(uintptr_t)ngsScreamAt9VoicePresetIndices - (uintptr_t)&ngsScreamAt9VoicePreset;
	definition->size = size;
	definition->signature[1] = 'v';
	definition->signature[3] = 'f';
	definition->modulesOffset = (uintptr_t)modules - (uintptr_t)definition;
	definition->version = 0x10001;
	definition->reserved = 0;
	definition->nameOffset = (uintptr_t)"Scream Voice AT9 Def" - (uintptr_t)definition;
	definition->signature[0] = 'b';
	definition->signature[2] = 'd';
	definition->moduleCount = 6;
	definition->nameLength = strnlen("Scream Voice AT9 Def", 32);
	definition->descriptionOffset = (uintptr_t)
		"Scream voice AT9.\n" -
		(uintptr_t)definition;
	definition->descriptionLength = strnlen(
		"Scream voice AT9.\n", 128);
	definition->presetsOffset =
		(uintptr_t)&ngsScreamAt9VoicePreset - (uintptr_t)definition;
	definition->routingIndicesOffset =
		(uintptr_t)routingIndices - (uintptr_t)definition;
	definition->presetCount = 1;
	definition->voiceOutputCount = 2;
	definition->routingIndexCount = 6;

	sceSyscallLeave(syscallState);
	return (const struct SceNgsVoiceDefinition *)
		((uintptr_t)definition ^ 0x9e28dcce);
}

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetPitchshiftBussInternal(void)
{
	unsigned int syscallState = sceSyscallEnter();
	unsigned char *cursor = ngsAlignVoiceDefinitionAddress(
		ngsPitchShiftVoiceDefinitionStorage, 16);
	unsigned char *start = cursor;
	struct SceNgsVoiceDefinition *definition;
	NgsVoiceModuleDefinition *modules;
	struct NgsAudioFormat *format;
	uint32_t *outputBindings;
	uint32_t *inputIndices;
	uint32_t *routingIndices;
	unsigned int size;

	definition = ngsAllocateMemory(&cursor, sizeof(*definition));
	modules = ngsAllocateMemory(&cursor, 2 * sizeof(*modules));
	format = ngsAllocateMemory(&cursor, sizeof(*format));
	outputBindings = ngsAllocateMemory(&cursor, sizeof(*outputBindings));
	inputIndices = ngsAllocateMemory(&cursor, sizeof(*inputIndices));
	routingIndices = ngsAllocateMemory(&cursor, 2 * sizeof(*routingIndices));
	size = cursor - start;
	*outputBindings = 0x80000000U;
	*inputIndices = 0;
	routingIndices[0] = 0;
	routingIndices[1] = 1;
	format->codecFourCC = (uint32_t)'l' | (uint32_t)'p' << 8 |
		(uint32_t)'c' << 16 | (uint32_t)'m' << 24;
	format->sampleFormat = 0x04343271;
	format->formatParameters[0] = 0;
	format->formatParameters[1] = 0;
	modules[0].outputCount = 1;
	modules[0].inputBindingsOffset = 0;
	modules[1].inputCount = 1;
	modules[1].outputCount = 1;
	modules[1].formatOffset = (uintptr_t)format - (uintptr_t)&modules[1];
	modules[0].moduleId = 0x5ce0;
	modules[0].inputCount = 0;
	modules[0].outputBindingsOffset = 0;
	modules[1].inputBindingsOffset = 0;
	modules[0].formatOffset = (uintptr_t)format - (uintptr_t)&modules[0];
	modules[1].moduleId = 0x5cea;
	modules[1].outputBindingsOffset =
		(uintptr_t)outputBindings - (uintptr_t)&modules[1];
	definition->size = size;
	definition->modulesOffset = (uintptr_t)modules - (uintptr_t)definition;
	definition->reserved = 0;
	definition->signature[0] = 'b';
	definition->signature[1] = 'v';
	definition->signature[2] = 'd';
	definition->signature[3] = 'f';
	definition->version = 0x10001;
	definition->nameOffset = (uintptr_t)"Pitch shift Buss" - (uintptr_t)definition;
	definition->moduleCount = 2;
	definition->dspConfiguration = 2;
	definition->nameLength = strnlen("Pitch shift Buss", 32);
	definition->descriptionOffset = (uintptr_t)
		"Pitch shift Buss Def: In (mix) -> Pitch shift -> Out" -
		(uintptr_t)definition;
	size = strnlen("Pitch shift Buss Def: In (mix) -> Pitch shift -> Out", 128);
	definition->presetsOffset = 0;
	definition->presetCount = 0;
	definition->descriptionLength = size;
	definition->routingIndicesOffset =
		(uintptr_t)routingIndices - (uintptr_t)definition;
	definition->voiceOutputCount = 1;
	definition->routingIndexCount = 2;
	sceSyscallLeave(syscallState);
	return (const struct SceNgsVoiceDefinition *)
		((uintptr_t)definition ^ 0x9e28dcce);
}
