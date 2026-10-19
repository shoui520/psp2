/* Built-in PCM/VAG player description and parameter interfaces. */

#include <string.h>

#include "ngs_module_memory.h"

typedef struct SceNgsPlayerBufferParams {
	const void *pBuffer;
	int nNumBytes;
	short nLoopCount;
	short nNextBuff;
} SceNgsPlayerBufferParams;

typedef struct SceNgsPlayerParams {
	SceNgsParamsDescriptor desc;
	SceNgsPlayerBufferParams buffs[4];
	float fPlaybackFrequency;
	float fPlaybackScalar;
	int nLeadInSamples;
	int nLimitNumberOfSamplesPlayed;
	int nStartByte;
	signed char nChannels;
	signed char nChannelMap[2];
	signed char nType;
	signed char reserved;
	signed char nStartBuffer;
	signed char pad[2];
} SceNgsPlayerParams;

extern const char *ngsModuleOutputName;

const NgsModuleParamDescription ngsPcmPlayerControlDescriptions[11] = {
	{ "Playback Frequency", "Hz", "Frequency to playback data", 2 },
	{ "Playback scalar", "SceFloat32", "Scalar multiply for the playback frequency.", 2 },
	{ "Lead in Samples", "", "Number of lead in samples.", 1,
	  { .integer = 0 }, { .integer = 0x7fffffff }, { .integer = 0 } },
	{ "Limit Samples Played", "", "Number of samples to generate.", 1,
	  { .integer = 0 }, { .integer = 0x7fffffff }, { .integer = 0 } },
	{ "Start Byte", "", "The byte position to start on at keyon", 1,
	  { .integer = 0 }, { .integer = 0x7fffffff }, { .integer = 0 } },
	{ "Number of channels", "", "Number of channels in source data.", 7,
	  { .integer = 1 }, { .integer = 2 }, { .integer = 1 } },
	{ "Channel map {0}", "", "Placement of channel {0}", 7,
	  { .integer = -1 }, { .integer = 1 }, { .integer = 0 } },
	{ "Channel map {1}", "", "Placement of channel {1}", 7,
	  { .integer = -1 }, { .integer = 1 }, { .integer = 0 } },
	{ "Type", "", "PCM or ADPCM.", 7,
	  { .integer = 0 }, { .integer = 1 }, { .integer = 0 } },
	{ "Reserved", "", "Reserved.", 7,
	  { .integer = 0 }, { .integer = 1 }, { .integer = 0 } },
	{ "Start Buffer", "", "The buffer to start on at keyon.", 7,
	  { .integer = 0 }, { .integer = 3 }, { .integer = 0 } },
};

const NgsModuleParamDescription ngsPcmPlayerBufferDescriptions[4] = {
	{ "Buffer Address", "", "Address of the buffer to play", 5,
	  { .integer = 0 }, { .unsignedInteger = 0xffffffff }, { .integer = 0 } },
	{ "Number of bytes", "", "Number of bytes in the buffer", 1,
	  { .integer = 0 }, { .integer = 0x7fffffff }, { .integer = 0 } },
	{ "Number of loops", "", "Number of times to loop this buffer. -1 for loop continuously.", 9,
	  { .integer = -1 }, { .integer = 32767 }, { .integer = 0 } },
	{ "Next buffer location", "", "Index of the next buffer. -1 for no next buffer.", 9,
	  { .integer = -1 }, { .integer = 3 }, { .integer = 0 } },
};

const SceNgsPlayerParams ngsPcmPlayerPresetParams = {
	.desc = { 0x01015ce6, 84 },
	.buffs = { { NULL, 0, -1, -1 }, { NULL, 0, -1, -1 },
		   { NULL, 0, -1, -1 }, { NULL, 0, -1, -1 } },
	.fPlaybackFrequency = -1.0f,
	.fPlaybackScalar = -1.0f,
	.nChannels = -1,
	.nChannelMap = { -1, -1 },
	.nType = -1,
};

extern struct NgsModuleParamInterface ngsPcmPlayerParamInterface;
const struct NgsModuleParamInterface *ngsPcmPlayerParamInterfaceList[] = {
	&ngsPcmPlayerParamInterface
};

NgsModuleAudioInterface ngsPcmPlayerOutputs[1];
struct NgsModuleParamInterface ngsPcmPlayerParamInterface;
NgsModuleParamDescription ngsPcmPlayerParamDescriptions[27];
NgsModulePreset ngsPcmPlayerPresets[1];
NgsModuleDefinition ngsPcmPlayerModule;

int ngsGetPcmVagPlayerModule(NgsModuleDefinition **module)
{
	ngsPcmPlayerOutputs[0].formats = ngsGetPcmVagPlayerFormat();
	ngsPcmPlayerParamDescriptions[0] = ngsPcmPlayerBufferDescriptions[0];
	ngsPcmPlayerOutputs[0].name = ngsModuleOutputName;
	ngsPcmPlayerOutputs[0].supportedFormatCount = 1;
	ngsPcmPlayerModule.paramInterfaceCount = 1;
	ngsPcmPlayerModule.inputCount = 0;
	ngsPcmPlayerModule.inputs = NULL;
	ngsPcmPlayerParamDescriptions[1] = ngsPcmPlayerBufferDescriptions[1];
	ngsPcmPlayerParamDescriptions[2] = ngsPcmPlayerBufferDescriptions[2];
	ngsPcmPlayerParamDescriptions[3] = ngsPcmPlayerBufferDescriptions[3];
	ngsPcmPlayerParamDescriptions[4] = ngsPcmPlayerBufferDescriptions[0];
	ngsPcmPlayerParamDescriptions[5] = ngsPcmPlayerBufferDescriptions[1];
	ngsPcmPlayerParamDescriptions[6] = ngsPcmPlayerBufferDescriptions[2];
	ngsPcmPlayerParamDescriptions[7] = ngsPcmPlayerBufferDescriptions[3];
	ngsPcmPlayerParamDescriptions[8] = ngsPcmPlayerBufferDescriptions[0];
	ngsPcmPlayerParamDescriptions[9] = ngsPcmPlayerBufferDescriptions[1];
	ngsPcmPlayerParamDescriptions[10] = ngsPcmPlayerBufferDescriptions[2];
	ngsPcmPlayerParamDescriptions[11] = ngsPcmPlayerBufferDescriptions[3];
	ngsPcmPlayerParamDescriptions[12] = ngsPcmPlayerBufferDescriptions[0];
	ngsPcmPlayerParamDescriptions[13] = ngsPcmPlayerBufferDescriptions[1];
	ngsPcmPlayerParamDescriptions[14] = ngsPcmPlayerBufferDescriptions[2];
	ngsPcmPlayerParamDescriptions[15] = ngsPcmPlayerBufferDescriptions[3];
	memcpy(&ngsPcmPlayerParamDescriptions[16], ngsPcmPlayerControlDescriptions,
	       sizeof(ngsPcmPlayerControlDescriptions));
	ngsPcmPlayerModule.outputCount = 1;
	ngsPcmPlayerParamDescriptions[16].maximum.real = 192000.0f;
	ngsPcmPlayerModule.codecResourceId = 0;
	ngsPcmPlayerModule.dspControl = 0;
	ngsPcmPlayerModule.outputs = ngsPcmPlayerOutputs;
	ngsPcmPlayerModule.flags = 1;
	ngsPcmPlayerModule.presetCount = 1;
	ngsPcmPlayerParamDescriptions[17].maximum.real = 10.0f;
	ngsPcmPlayerParamDescriptions[16].minimum.real = 0.0f;
	ngsPcmPlayerParamDescriptions[17].minimum.real = 0.0f;
	ngsPcmPlayerModule.paramInterfaces = ngsPcmPlayerParamInterfaceList;
	ngsPcmPlayerParamInterface.params = ngsPcmPlayerParamDescriptions;
	ngsPcmPlayerModule.info = "name = PCM/VAG player\n"
		"version = 1.0.001\n"
		"vendor = Sony Computer Entertainment\n"
		"copyright = (C) 2009 Sony Computer Entertainment Inc. All Rights Reserved.\n"
		"category = Source\n"
		"desc = Re-sampling playback of PCM or VAG data.\n";
	ngsPcmPlayerModule.getMemoryInfo = ngsGetPcmVagPlayerMemoryInfo;
	ngsPcmPlayerModule.version = 0x10002;
	ngsPcmPlayerParamInterface.paramsId = 0x01015ce6;
	ngsPcmPlayerParamInterface.size = 84;
	ngsPcmPlayerParamInterface.paramCount = 27;
	ngsPcmPlayerModule.dspDispatchIds[0] = 0x5ce6;
	ngsPcmPlayerModule.dspDispatchIds[1] = 0x5ce6;
	ngsPcmPlayerModule.moduleId = 0x5ce6;
	ngsPcmPlayerModule.presets = ngsPcmPlayerPresets;
	ngsPcmPlayerPresets[0].name = "None initialized state.";
	ngsPcmPlayerPresets[0].params = &ngsPcmPlayerPresetParams.desc;
	*module = &ngsPcmPlayerModule;
	return 0;
}
