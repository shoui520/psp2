/* Built-in AT9 player description and parameter interfaces. */

#include <string.h>
#include <ngs/modules/at9_player.h>

#include "ngs_module_memory.h"

extern const char *ngsModuleOutputName;

const NgsModuleParamDescription ngsAt9PlayerControlDescriptions[9] = {
	{ "Playback Frequency", "Hz", "Frequency to playback data", 2,
	  { .integer = 0 }, { .integer = 0 }, { .integer = 0 } },
	{ "Playback scalar", "SceFloat32", "Scalar multiply for the playback frequency.", 2,
	  { .integer = 0 }, { .integer = 0 }, { .integer = 0 } },
	{ "Lead in Samples", "", "Number of lead in samples.", 1,
	  { .integer = 0 }, { .integer = 2147483647 }, { .integer = 0 } },
	{ "Limit Samples Played", "", "Number of samples to generate.", 1,
	  { .integer = 0 }, { .integer = 2147483647 }, { .integer = 0 } },
	{ "Number of channels", "", "Number of channels in source data.", 7,
	  { .integer = 1 }, { .integer = 2 }, { .integer = 1 } },
	{ "Channel map {0}", "", "Placement of channel {0}", 7,
	  { .integer = -1 }, { .integer = 1 }, { .integer = 0 } },
	{ "Channel map {1}", "", "Placement of channel {1}", 7,
	  { .integer = -1 }, { .integer = 1 }, { .integer = 0 } },
	{ "Reserved", "", "Reserved.", 7,
	  { .integer = 0 }, { .integer = 1 }, { .integer = 0 } },
	{ "Config Data", "", "Config Data.", 5,
	  { .integer = 0 }, { .integer = -1 }, { .integer = 0 } },
};

const NgsModuleParamDescription ngsAt9PlayerBufferDescriptions[6] = {
	{ "Buffer Address", "", "Address of the buffer to play", 5,
	  { .integer = 0 }, { .integer = -1 }, { .integer = 0 } },
	{ "Number of bytes", "", "Number of bytes in the buffer", 1,
	  { .integer = 0 }, { .integer = 2147483647 }, { .integer = 0 } },
	{ "Number of loops", "", "Number of times to loop this buffer. -1 for loop continuously.", 9,
	  { .integer = -1 }, { .integer = 32767 }, { .integer = 0 } },
	{ "Next buffer location", "", "Index of the next buffer. -1 for no next buffer.", 9,
	  { .integer = -1 }, { .integer = 3 }, { .integer = 0 } },
	{ "Start Skip", "", "Number of samples to skip from the first decode", 9,
	  { .integer = 0 }, { .integer = 2048 }, { .integer = 0 } },
	{ "End Skip", "", "Number of samples to skip from the last decode", 9,
	  { .integer = 0 }, { .integer = 2048 }, { .integer = 0 } },
};

const SceNgsAT9Params ngsAt9PlayerPresetParams = {
	.desc = { 0x01015caa, 96 },
	.buffs = { { 0, -1, -1, 1, 0, 0 }, { 0, -1, -1, -1, 0, 0 },
		   { 0, -1, -1, -1, 0, 0 }, { 0, -1, -1, -1, 0, 0 } },
	.fPlaybackFrequency = -1.0f,
	.fPlaybackScalar = -1.0f,
	.nChannels = -1,
	.nChannelMap = { -1, -1 },
};

extern struct NgsModuleParamInterface ngsAt9PlayerParamInterface;
const struct NgsModuleParamInterface *ngsAt9PlayerParamInterfaceList[1] = {
	&ngsAt9PlayerParamInterface
};

NgsModuleAudioInterface ngsAt9PlayerOutputs[1];
struct NgsModuleParamInterface ngsAt9PlayerParamInterface;
NgsModuleParamDescription ngsAt9PlayerParamDescriptions[33];
NgsModulePreset ngsAt9PlayerPresets[1];
NgsModuleDefinition ngsAt9PlayerModule;

int ngsGetAt9PlayerModule(NgsModuleDefinition **module)
{
	const struct NgsAudioFormat *format = ngsGetAt9PlayerFormat();
	NgsModuleParamDescription *description = ngsAt9PlayerParamDescriptions;

	ngsAt9PlayerOutputs[0].supportedFormatCount = 1;
	ngsAt9PlayerOutputs[0].formats = format;
	ngsAt9PlayerOutputs[0].name = ngsModuleOutputName;
	ngsAt9PlayerModule.paramInterfaceCount = 1;
	ngsAt9PlayerModule.inputCount = 0;
	ngsAt9PlayerModule.inputs = 0;
	do {
		description[0] = ngsAt9PlayerBufferDescriptions[0];
		description[1] = ngsAt9PlayerBufferDescriptions[1];
		description[2] = ngsAt9PlayerBufferDescriptions[2];
		description[3] = ngsAt9PlayerBufferDescriptions[3];
		description[4] = ngsAt9PlayerBufferDescriptions[4];
		description[5] = ngsAt9PlayerBufferDescriptions[5];
		description += 6;
	} while (description != ngsAt9PlayerParamDescriptions + 24);
	memcpy(&ngsAt9PlayerParamDescriptions[24], ngsAt9PlayerControlDescriptions,
	       sizeof(ngsAt9PlayerControlDescriptions));
	ngsAt9PlayerParamInterface.paramsId = 0x01015caa;
	ngsAt9PlayerModule.outputs = ngsAt9PlayerOutputs;
	ngsAt9PlayerModule.paramInterfaces = ngsAt9PlayerParamInterfaceList;
	ngsAt9PlayerParamInterface.params = ngsAt9PlayerParamDescriptions;
	ngsAt9PlayerParamInterface.size = 96;
	ngsAt9PlayerParamInterface.paramCount = 33;
	ngsAt9PlayerModule.info = "name = AT9 player\n"
		"version = 1.0.001\n"
		"vendor = Sony Computer Entertainment\n"
		"copyright = (C) 2009 Sony Computer Entertainment Inc. All Rights Reserved.\n"
		"category = Source\n"
		"desc = Re-sampling playback of AT9 data.\n";
	ngsAt9PlayerModule.outputCount = 1;
	ngsAt9PlayerModule.flags = 1;
	ngsAt9PlayerModule.presetCount = 1;
	ngsAt9PlayerModule.version = 0x10002;
	ngsAt9PlayerModule.getMemoryInfo = ngsGetAt9PlayerMemoryInfo;
	ngsAt9PlayerModule.presets = ngsAt9PlayerPresets;
	ngsAt9PlayerPresets[0].name = "None initialized state.";
	ngsAt9PlayerPresets[0].params = &ngsAt9PlayerPresetParams.desc;
	ngsAt9PlayerModule.moduleId = SCE_NGS_AT9_ID;
	ngsAt9PlayerModule.codecResourceId = 0;
	ngsAt9PlayerParamDescriptions[24].maximum.real = 192000.0f;
	ngsAt9PlayerParamDescriptions[24].minimum.real = 0.0f;
	ngsAt9PlayerParamDescriptions[25].minimum.real = 0.0f;
	ngsAt9PlayerParamDescriptions[25].maximum.real = 10.0f;
	ngsAt9PlayerModule.dspDispatchIds[0] = SCE_NGS_AT9_ID;
	ngsAt9PlayerModule.dspDispatchIds[1] = SCE_NGS_AT9_ID;
	ngsAt9PlayerModule.dspControl = 0;
	*module = &ngsAt9PlayerModule;
	return 0;
}

