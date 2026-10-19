/* Built-in mixer description and parameter interfaces. */

#include "ngs_module_memory.h"

typedef struct SceNgsMixerParams {
	SceNgsParamsDescriptor desc;
	float fGainIn[2];
	unsigned int uPauseLeadIn;
	unsigned int uPauseLeadOut;
} SceNgsMixerParams;

extern const char *ngsModuleOutputName;
extern const char *ngsModuleInput1Name;
extern const char *ngsModuleInput2Name;

const SceNgsMixerParams ngsMixerPresetParams = {
	.desc = { 0x01015ce9, 24 },
	.fGainIn = { 1.0f, 1.0f },
	.uPauseLeadIn = 32,
	.uPauseLeadOut = 32,
};

extern struct NgsModuleParamInterface ngsMixerParamInterface;
const struct NgsModuleParamInterface *ngsMixerParamInterfaceList[1] = {
	&ngsMixerParamInterface
};

NgsModuleParamDescription ngsMixerParamDescriptions[4] = {
	{ "Port 0 gain", "SceFloat32", "Input gain of signal 0", 2 },
	{ "Port 1 gain", "SceFloat32", "Input gain of signal 1", 2 },
	{ "Pause lead in", "Samples", "Number of samples to linearly smooth in when a voice is unpaused.", 5 },
	{ "Pause lead out", "Samples", "Number of samples to linearly smooth in when a voice is paused.", 5 },
};

NgsModuleAudioInterface ngsMixerOutputs[1];
NgsModuleAudioInterface ngsMixerInputs[2];
struct NgsModuleParamInterface ngsMixerParamInterface;
NgsModulePreset ngsMixerPresets[1];
NgsModuleDefinition ngsMixerModule;

int ngsGetMixerModule(NgsModuleDefinition **module)
{
	ngsMixerOutputs[0].formats = ngsGetMixerFormat();
	ngsMixerOutputs[0].name = ngsModuleOutputName;
	ngsMixerInputs[0].formats = ngsMixerOutputs[0].formats;
	ngsMixerInputs[0].name = ngsModuleInput1Name;
	ngsMixerInputs[1].formats = ngsMixerOutputs[0].formats;
	ngsMixerInputs[1].name = ngsModuleInput2Name;
	ngsMixerInputs[0].supportedFormatCount = 1;
	ngsMixerOutputs[0].supportedFormatCount = 1;
	ngsMixerInputs[1].supportedFormatCount = 1;
	ngsMixerModule.inputs = ngsMixerInputs;
	ngsMixerModule.paramInterfaceCount = 1;
	ngsMixerModule.inputCount = 2;
	if (ngsMixerParamDescriptions[0].type == 2) {
		ngsMixerParamDescriptions[0].minimum.real = 0.0f;
		ngsMixerParamDescriptions[0].maximum.real = 8.0f;
		ngsMixerParamDescriptions[0].defaultValue.real = 1.0f;
	} else if (ngsMixerParamDescriptions[0].type == 5) {
		ngsMixerParamDescriptions[0].defaultValue.unsignedInteger = 1;
		ngsMixerParamDescriptions[0].minimum.unsignedInteger = 0;
		ngsMixerParamDescriptions[0].maximum.unsignedInteger = 8;
	}
	if (ngsMixerParamDescriptions[1].type == 2) {
		ngsMixerParamDescriptions[1].minimum.real = 0.0f;
		ngsMixerParamDescriptions[1].maximum.real = 8.0f;
		ngsMixerParamDescriptions[1].defaultValue.real = 1.0f;
	} else if (ngsMixerParamDescriptions[1].type == 5) {
		ngsMixerParamDescriptions[1].defaultValue.unsignedInteger = 1;
		ngsMixerParamDescriptions[1].minimum.unsignedInteger = 0;
		ngsMixerParamDescriptions[1].maximum.unsignedInteger = 8;
	}
	if (ngsMixerParamDescriptions[2].type == 2) {
		ngsMixerParamDescriptions[2].minimum.real = 0.0f;
		ngsMixerParamDescriptions[2].maximum.real = 256.0f;
		ngsMixerParamDescriptions[2].defaultValue.real = 32.0f;
	} else if (ngsMixerParamDescriptions[2].type == 5) {
		ngsMixerParamDescriptions[2].defaultValue.unsignedInteger = 32;
		ngsMixerParamDescriptions[2].minimum.unsignedInteger = 0;
		ngsMixerParamDescriptions[2].maximum.unsignedInteger = 256;
	}
	if (ngsMixerParamDescriptions[3].type == 2) {
		ngsMixerParamDescriptions[3].minimum.real = 0.0f;
		ngsMixerParamDescriptions[3].maximum.real = 256.0f;
		ngsMixerParamDescriptions[3].defaultValue.real = 32.0f;
	} else if (ngsMixerParamDescriptions[3].type == 5) {
		ngsMixerParamDescriptions[3].minimum.unsignedInteger = 0;
		ngsMixerParamDescriptions[3].maximum.unsignedInteger = 256;
		ngsMixerParamDescriptions[3].defaultValue.unsignedInteger = 32;
	}
	ngsMixerModule.outputs = ngsMixerOutputs;
	ngsMixerParamInterface.params = ngsMixerParamDescriptions;
	ngsMixerParamInterface.paramsId = 0x01015ce9;
	ngsMixerParamInterface.size = 24;
	ngsMixerModule.paramInterfaces = ngsMixerParamInterfaceList;
	ngsMixerParamInterface.paramCount = 4;
	ngsMixerModule.info = "name = Mixer\n"
		"version = 1.0.001\n"
		"vendor = Sony Computer Entertainment\n"
		"copyright = (C) 2009 Sony Computer Entertainment Inc. All Rights Reserved.\n"
		"category = Mixer\n"
		"desc = Mixes 2 audio input signals into a single signal.\n";
	ngsMixerModule.outputCount = 1;
	ngsMixerModule.presetCount = 1;
	ngsMixerModule.version = 0x10002;
	ngsMixerModule.getMemoryInfo = ngsGetMixerMemoryInfo;
	ngsMixerModule.presets = ngsMixerPresets;
	ngsMixerPresets[0].name = "Full volume mix";
	ngsMixerPresets[0].params = &ngsMixerPresetParams.desc;
	ngsMixerModule.moduleId = 0x5ce9;
	ngsMixerModule.dspDispatchIds[0] = 0x5ce9;
	ngsMixerModule.dspDispatchIds[1] = 0x5ce9;
	ngsMixerModule.codecResourceId = 0;
	ngsMixerModule.dspControl = 0;
	ngsMixerModule.flags = 0;
	*module = &ngsMixerModule;
	return 0;
}
