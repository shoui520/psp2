/* Memory requirements returned by the built-in audio-module callbacks. */

#include <stddef.h>
#include <arm_neon.h>
#include <string.h>
#include "ngs_module_memory.h"
#include "ngs_rack.h"
#include "ngs_voice_definition.h"

extern const int ngsReverbDelayLengths[16];

struct NgsAudioFormat ngsCompressorFormat
	__attribute__((section(".data"))) = { 0 };
struct NgsAudioFormat ngsDistortionFormat
	__attribute__((section(".data"))) = { 0 };
struct NgsAudioFormat ngsEnvelopeFormat
	__attribute__((section(".data"))) = { 0 };
struct NgsAudioFormat ngsPauserFormat
	__attribute__((section(".data"))) = { 0 };
struct NgsAudioFormat ngsPcmVagPlayerFormat
	__attribute__((section(".data"))) = { 0 };
struct NgsAudioFormat ngsSignalGeneratorFormat
	__attribute__((section(".data"))) = { 0 };
struct NgsAudioFormat ngsReverbFormat
	__attribute__((section(".data"))) = { 0 };
struct NgsAudioFormat ngsMixerFormat
	__attribute__((section(".data"))) = { 0 };
struct NgsAudioFormat ngsBiquadFilterFormat
	__attribute__((section(".data"))) = { 0 };
struct NgsAudioFormat ngsParametricEqualizerFormat
	__attribute__((section(".data"))) = { 0 };
struct NgsAudioFormat ngsInputFormat
	__attribute__((section(".data"))) = { 0 };
struct NgsAudioFormat ngsOutputFormat
	__attribute__((section(".data"))) = { 0 };
struct NgsAudioFormat ngsAt9PlayerFormat
	__attribute__((section(".data"))) = { 0 };
struct NgsAudioFormat ngsDelayFormat
	__attribute__((section(".data"))) = { 0 };
struct NgsAudioFormat ngsPitchShiftFormat
	__attribute__((section(".data"))) = { 0 };

int ngsGetCompressorMemoryInfo(const struct NgsModuleInitParams *params,
			     NgsModuleMemoryInfo *info)
{
	if (params == NULL || info == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;
	info->stateSize = 16;
	info->sharedWorkSize = 0;
	info->extraMemorySize = 0;
	info->contextSize = 56;
	info->paramsSize = 40;
	return 0;
}

const struct NgsAudioFormat *ngsGetCompressorFormat(void)
{
	uint32x4_t format = {
		(uint32_t)'l' | (uint32_t)'p' << 8 |
			(uint32_t)'c' << 16 | (uint32_t)'m' << 24,
		0x04343271, 0, 0,
	};

	vst1q_u32(&ngsCompressorFormat.codecFourCC, format);
	return &ngsCompressorFormat;
}

int ngsGetDistortionMemoryInfo(const struct NgsModuleInitParams *params,
			     NgsModuleMemoryInfo *info)
{
	if (params == NULL || info == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;
	info->paramsSize = 32;
	info->extraMemorySize = 0;
	info->stateSize = 0;
	info->sharedWorkSize = 0;
	info->contextSize = 24;
	return 0;
}

const struct NgsAudioFormat *ngsGetDistortionFormat(void)
{
	uint32x4_t format = {
		(uint32_t)'l' | (uint32_t)'p' << 8 |
			(uint32_t)'c' << 16 | (uint32_t)'m' << 24,
		0x04343271, 0, 0,
	};

	vst1q_u32(&ngsDistortionFormat.codecFourCC, format);
	return &ngsDistortionFormat;
}

int ngsGetEnvelopeMemoryInfo(const struct NgsModuleInitParams *params,
			   NgsModuleMemoryInfo *info)
{
	if (params == NULL || info == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;
	info->stateSize = 20;
	info->sharedWorkSize = 0;
	info->extraMemorySize = 0;
	info->contextSize = 84;
	info->paramsSize = 72;
	return 0;
}

const struct NgsAudioFormat *ngsGetEnvelopeFormat(void)
{
	uint32x4_t format = {
		(uint32_t)'l' | (uint32_t)'p' << 8 |
			(uint32_t)'c' << 16 | (uint32_t)'m' << 24,
		0x04343271, 0, 0,
	};

	vst1q_u32(&ngsEnvelopeFormat.codecFourCC, format);
	return &ngsEnvelopeFormat;
}

int ngsGetPauserMemoryInfo(const struct NgsModuleInitParams *params,
			 NgsModuleMemoryInfo *info)
{
	if (params == NULL || info == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;
	info->paramsSize = 16;
	info->extraMemorySize = 0;
	info->stateSize = 0;
	info->sharedWorkSize = 0;
	info->contextSize = 20;
	return 0;
}

const struct NgsAudioFormat *ngsGetPauserFormat(void)
{
	uint32x4_t format = {
		(uint32_t)'l' | (uint32_t)'p' << 8 |
			(uint32_t)'c' << 16 | (uint32_t)'m' << 24,
		0x04343271, 0, 0,
	};

	vst1q_u32(&ngsPauserFormat.codecFourCC, format);
	return &ngsPauserFormat;
}

int ngsGetPcmVagPlayerMemoryInfo(const struct NgsModuleInitParams *params,
			       NgsModuleMemoryInfo *info)
{
	if (params == NULL || info == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;
	info->stateSize = 24;
	info->contextSize = 256;
	info->paramsSize = 84;
	info->extraMemorySize = 0;
	info->sharedWorkSize = 15616;
	return 0;
}

const struct NgsAudioFormat *ngsGetPcmVagPlayerFormat(void)
{
	uint32x4_t format = {
		(uint32_t)'l' | (uint32_t)'p' << 8 |
			(uint32_t)'c' << 16 | (uint32_t)'m' << 24,
		0x04343271, 0, 0,
	};

	vst1q_u32(&ngsPcmVagPlayerFormat.codecFourCC, format);
	return &ngsPcmVagPlayerFormat;
}

int ngsGetSignalGeneratorMemoryInfo(const struct NgsModuleInitParams *params,
				  NgsModuleMemoryInfo *info)
{
	if (params == NULL || info == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;
	info->paramsSize = 32;
	info->extraMemorySize = 0;
	info->stateSize = 0;
	info->sharedWorkSize = 0;
	info->contextSize = 48;
	return 0;
}

const struct NgsAudioFormat *ngsGetSignalGeneratorFormat(void)
{
	uint32x4_t format = {
		(uint32_t)'l' | (uint32_t)'p' << 8 |
			(uint32_t)'c' << 16 | (uint32_t)'m' << 24,
		0x04343271, 0, 0,
	};

	vst1q_u32(&ngsSignalGeneratorFormat.codecFourCC, format);
	return &ngsSignalGeneratorFormat;
}

int ngsGetReverbMemoryInfo(const struct NgsModuleInitParams *params,
			 NgsModuleMemoryInfo *info)
{
	int contextSize = 85808;
	int delayIndex;

	for (delayIndex = 0; delayIndex != 16; delayIndex++) {
		int length = ngsReverbDelayLengths[delayIndex];
		int alignedLength = 8 - length % 8 + length;

		if (length & 7)
			length = alignedLength;
		if (delayIndex <= 2)
			length += 8;
		contextSize += 2 * (length + 8);
	}
	contextSize += 16;
	if (params == NULL || info == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;
	info->stateSize = 0;
	info->sharedWorkSize = 0;
	info->contextSize = contextSize;
	info->paramsSize = 76;
	info->extraMemorySize = (params->nChannelsPerVoice + 11) << 11;
	return 0;
}

const struct NgsAudioFormat *ngsGetReverbFormat(void)
{
	uint32x4_t format = {
		(uint32_t)'l' | (uint32_t)'p' << 8 |
			(uint32_t)'c' << 16 | (uint32_t)'m' << 24,
		0x04323366, 0, 0,
	};

	vst1q_u32(&ngsReverbFormat.codecFourCC, format);
	return &ngsReverbFormat;
}

int ngsGetMixerMemoryInfo(const struct NgsModuleInitParams *params,
			NgsModuleMemoryInfo *info)
{
	if (params == NULL || info == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;
	info->paramsSize = 24;
	info->extraMemorySize = 0;
	info->stateSize = 0;
	info->sharedWorkSize = 0;
	info->contextSize = 20;
	return 0;
}

const struct NgsAudioFormat *ngsGetMixerFormat(void)
{
	uint32x4_t format = {
		(uint32_t)'l' | (uint32_t)'p' << 8 |
			(uint32_t)'c' << 16 | (uint32_t)'m' << 24,
		0x04343271, 0, 0,
	};

	vst1q_u32(&ngsMixerFormat.codecFourCC, format);
	return &ngsMixerFormat;
}

int ngsGetBiquadFilterMemoryInfo(const struct NgsModuleInitParams *params,
			       NgsModuleMemoryInfo *info)
{
	int channels;
	int paramsSize;
	int secondaryParamsSize;

	if (params == NULL || info == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;
	channels = params->nChannelsPerVoice;
	info->stateSize = 0;
	info->sharedWorkSize = 0;
	info->extraMemorySize = 0;
	paramsSize = 24 * channels;
	secondaryParamsSize = 28 * channels;
	if (secondaryParamsSize >= paramsSize)
		paramsSize = secondaryParamsSize;
	info->contextSize = 36 * channels;
	info->paramsSize = paramsSize;
	return 0;
}

const struct NgsAudioFormat *ngsGetBiquadFilterFormat(void)
{
	uint32x4_t format = {
		(uint32_t)'l' | (uint32_t)'p' << 8 |
			(uint32_t)'c' << 16 | (uint32_t)'m' << 24,
		0x04343271, 0, 0,
	};

	vst1q_u32(&ngsBiquadFilterFormat.codecFourCC, format);
	return &ngsBiquadFilterFormat;
}

int ngsGetParametricEqualizerMemoryInfo(const struct NgsModuleInitParams *params,
				     NgsModuleMemoryInfo *info)
{
	int channels;
	int paramsSize;
	int secondaryParamsSize;

	if (params == NULL || info == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;
	channels = params->nChannelsPerVoice;
	info->stateSize = 0;
	info->sharedWorkSize = 0;
	info->extraMemorySize = 0;
	paramsSize = 88 * channels;
	secondaryParamsSize = 72 * channels;
	if (paramsSize < secondaryParamsSize)
		paramsSize = secondaryParamsSize;
	info->contextSize = 144 * channels;
	info->paramsSize = paramsSize;
	return 0;
}

const struct NgsAudioFormat *ngsGetParametricEqualizerFormat(void)
{
	uint32x4_t format = {
		(uint32_t)'l' | (uint32_t)'p' << 8 |
			(uint32_t)'c' << 16 | (uint32_t)'m' << 24,
		0x04343271, 0, 0,
	};

	vst1q_u32(&ngsParametricEqualizerFormat.codecFourCC, format);
	return &ngsParametricEqualizerFormat;
}

int ngsGetInputMemoryInfo(const struct NgsModuleInitParams *params,
			NgsModuleMemoryInfo *info)
{
	if (params == NULL || info == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;
	info->stateSize = 0;
	info->contextSize = 0;
	info->paramsSize = 0;
	info->sharedWorkSize = 0;
	info->extraMemorySize = 0;
	return 0;
}

void ngsInitInputMixerContext(NgsSynSystem *system,
			      const SceNgsRackDescription *description,
			      void *memory, unsigned char *buffers)
{
	unsigned char *cursor = memory;
	int patchCount = description->nMaxPatchesPerInput;
	SceSize bufferSize = 4 * system->Granularity;
	NgsInputMixerContext *context;
	int slot;
	unsigned int channel;

	if (patchCount > NGS_INPUT_MIXER_MAX_PATCHES)
		patchCount = NGS_INPUT_MIXER_MAX_PATCHES;
	context = ngsAllocateMemory(&cursor, sizeof(*context));
	context->patchCount = patchCount;
	context->channels = description->nChannelsPerVoice;
	memset(context->reserved, 0, sizeof(context->reserved));
	for (slot = 0; slot < patchCount; slot++) {
		for (channel = 0; channel < context->channels; channel++) {
			void *buffer = ngsAllocateMemory(&buffers, bufferSize);

			context->buffers[slot][channel] = (uintptr_t)buffer;
			context->buffers[slot][channel] =
				ngsGetPhysicalAddress(buffer);
		}
		for (; channel < 2; channel++)
			context->buffers[slot][channel] = 0;
	}
	for (; slot < NGS_INPUT_MIXER_BUFFER_SLOTS; slot++) {
		context->buffers[slot][0] = 0;
		context->buffers[slot][1] = 0;
	}
	ngsCleanDcacheRange(context, ngsAlignMemorySize(sizeof(*context)));
}

int ngsGetInputMixerMemorySize(NgsSynSystem *system,
			       const SceNgsRackDescription *description,
			       SceSize *contextSize, SceSize *bufferSize)
{
	SceSize channelSize = 4 * system->Granularity *
			      description->nChannelsPerVoice;
	int patchCount = description->nMaxPatchesPerInput;

	*contextSize = sizeof(NgsInputMixerContext);
	if (patchCount > NGS_INPUT_MIXER_MAX_PATCHES)
		patchCount = NGS_INPUT_MIXER_MAX_PATCHES;
	*bufferSize = channelSize * patchCount;
	return 0;
}

const struct NgsAudioFormat *ngsGetInputFormat(void)
{
	uint32x4_t format = {
		(uint32_t)'l' | (uint32_t)'p' << 8 |
			(uint32_t)'c' << 16 | (uint32_t)'m' << 24,
		0x04343271, 0, 0,
	};

	vst1q_u32(&ngsInputFormat.codecFourCC, format);
	return &ngsInputFormat;
}

int ngsGetOutputMemoryInfo(const struct NgsModuleInitParams *params,
			 NgsModuleMemoryInfo *info)
{
	int granularity;
	int channels;

	if (params == NULL || info == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;
	granularity = params->nGranularity;
	channels = params->nChannelsPerVoice;
	info->paramsSize = 0;
	info->sharedWorkSize = 0;
	info->extraMemorySize = 0;
	info->contextSize = 4;
	info->stateSize = (channels * granularity) * 2;
	return 0;
}

const struct NgsAudioFormat *ngsGetOutputFormat(void)
{
	uint32x4_t format = {
		(uint32_t)'l' | (uint32_t)'p' << 8 |
			(uint32_t)'c' << 16 | (uint32_t)'m' << 24,
		0x04323366, 0, 0,
	};

	vst1q_u32(&ngsOutputFormat.codecFourCC, format);
	return &ngsOutputFormat;
}

int ngsGetAt9PlayerMemoryInfo(const struct NgsModuleInitParams *params,
			    NgsModuleMemoryInfo *info)
{
	if (params == NULL || info == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;
	info->stateSize = 24;
	info->contextSize = 2704;
	info->paramsSize = 96;
	info->extraMemorySize = 0;
	info->sharedWorkSize = 11520;
	return 0;
}

const struct NgsAudioFormat *ngsGetAt9PlayerFormat(void)
{
	uint32x4_t format = {
		(uint32_t)'l' | (uint32_t)'p' << 8 |
			(uint32_t)'c' << 16 | (uint32_t)'m' << 24,
		0x04343271, 0, 0,
	};

	vst1q_u32(&ngsAt9PlayerFormat.codecFourCC, format);
	return &ngsAt9PlayerFormat;
}

int ngsGetDelayMemoryInfo(const struct NgsModuleInitParams *params,
			NgsModuleMemoryInfo *info)
{
	int channels;

	if (params == NULL || info == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;
	channels = params->nChannelsPerVoice;
	info->stateSize = 0;
	info->extraMemorySize = 0;
	info->sharedWorkSize = 10240;
	info->contextSize = 192252 * channels;
	info->paramsSize = channels << 7;
	return 0;
}

const struct NgsAudioFormat *ngsGetDelayFormat(void)
{
	uint32x4_t format = {
		(uint32_t)'l' | (uint32_t)'p' << 8 |
			(uint32_t)'c' << 16 | (uint32_t)'m' << 24,
		0x04343271, 0, 0,
	};

	vst1q_u32(&ngsDelayFormat.codecFourCC, format);
	return &ngsDelayFormat;
}

int ngsGetPitchShiftMemoryInfo(const struct NgsModuleInitParams *params,
			     NgsModuleMemoryInfo *info)
{
	if (params == NULL || info == NULL)
		return SCE_NGS_ERROR_INVALID_PARAM;
	info->paramsSize = 12;
	info->extraMemorySize = 0;
	info->stateSize = 0;
	info->sharedWorkSize = 0;
	info->contextSize = 8220;
	return 0;
}

const struct NgsAudioFormat *ngsGetPitchShiftFormat(void)
{
	uint32x4_t format = {
		(uint32_t)'l' | (uint32_t)'p' << 8 |
			(uint32_t)'c' << 16 | (uint32_t)'m' << 24,
		0x04343271, 0, 0,
	};

	vst1q_u32(&ngsPitchShiftFormat.codecFourCC, format);
	return &ngsPitchShiftFormat;
}

const int ngsReverbDelayLengths[16] = {
	2617, 1999, 1801, 3407, 2179, 1597, 4063, 3019,
	4001, 3821, 2411, 3593, 1409, 2797, 1187, 3191,
};
