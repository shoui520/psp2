/* Built-in biquad filter description and parameter interfaces. */

#include "ngs_module_memory.h"

typedef struct SceNgsFilterParams {
	SceNgsParamsDescriptor desc;
	unsigned int eFilterMode;
	float fFrequency;
	float fResonance;
	float fGain;
} SceNgsFilterParams;

const struct NgsAudioFormat *ngsGetBiquadFilterFormat(void);
int ngsGetBiquadFilterMemoryInfo(const struct NgsModuleInitParams *params,
			       NgsModuleMemoryInfo *info);
extern const char *ngsModuleOutputName;
extern const char *ngsModuleInputName;

const SceNgsFilterParams ngsBiquadFilterPresetParams[2] = {
	{ { 0x01015ce4, 24 }, 9, 400.0f, 1.0f, 0.0f },
	{ { 0x01015ce4, 24 }, 1, 5000.0f, 2.707f, 0.0f },
};

extern struct NgsModuleParamInterface ngsBiquadFilterParamInterfaces[2];
const struct NgsModuleParamInterface *ngsBiquadFilterParamInterfaceList[2] = {
	&ngsBiquadFilterParamInterfaces[0], &ngsBiquadFilterParamInterfaces[1]
};

NgsModuleParamDescription ngsBiquadFilterParamDescriptions[4] = {
	{ "Mode", "", "Selected filter mode", 1 },
	{ "Frequency", "Hz", "Cut-off frequency of the filter.", 2 },
	{ "Resonance", "", "Amount of resonant Q for the filter.This parameter only affects certain filter modes. Typical ranges are 0.2 to 10.0.", 2 },
	{ "Gain", "dB", "Decibel gain or cut-off of the filter. This parameter only affects certain filter modes", 2 },
};

NgsModuleParamDescription ngsBiquadFilterCoefficientDescriptions[5] = {
	{ "B0", "", "CoEfficient B0", 2 },
	{ "B1", "", "CoEfficient B1", 2 },
	{ "B2", "", "CoEfficient B2", 2 },
	{ "A1", "", "CoEfficient A1", 2 },
	{ "A2", "", "CoEfficient A2", 2 },
};

NgsModuleAudioInterface ngsBiquadFilterOutputs[1];
NgsModuleAudioInterface ngsBiquadFilterInputs[1];
struct NgsModuleParamInterface ngsBiquadFilterParamInterfaces[2];
NgsModulePreset ngsBiquadFilterPresets[2];
NgsModuleDefinition ngsBiquadFilterModule;

int ngsGetBiquadFilterModule(NgsModuleDefinition **module)
{
	ngsBiquadFilterOutputs[0].formats = ngsGetBiquadFilterFormat();
	ngsBiquadFilterOutputs[0].name = ngsModuleOutputName;
	ngsBiquadFilterInputs[0].formats = ngsBiquadFilterOutputs[0].formats;
	ngsBiquadFilterInputs[0].name = ngsModuleInputName;
	ngsBiquadFilterInputs[0].supportedFormatCount = 1;
	ngsBiquadFilterOutputs[0].supportedFormatCount = 1;
	ngsBiquadFilterModule.paramInterfaceCount = 2;
	ngsBiquadFilterModule.inputCount = 1;
	ngsBiquadFilterModule.inputs = ngsBiquadFilterInputs;
	if (ngsBiquadFilterParamDescriptions[0].type == 1) {
		ngsBiquadFilterParamDescriptions[0].minimum.integer = 0;
		ngsBiquadFilterParamDescriptions[0].maximum.integer = 12;
		ngsBiquadFilterParamDescriptions[0].defaultValue.integer = 0;
	} else if (ngsBiquadFilterParamDescriptions[0].type == 2) {
		ngsBiquadFilterParamDescriptions[0].minimum.real = 0.0f;
		ngsBiquadFilterParamDescriptions[0].maximum.real = 12.0f;
		ngsBiquadFilterParamDescriptions[0].defaultValue.real = 0.0f;
	}
	if (ngsBiquadFilterParamDescriptions[1].type == 1) {
		ngsBiquadFilterParamDescriptions[1].defaultValue.integer = 1;
		ngsBiquadFilterParamDescriptions[1].minimum.integer = 20;
		ngsBiquadFilterParamDescriptions[1].maximum.integer = 23500;
	} else if (ngsBiquadFilterParamDescriptions[1].type == 2) {
		ngsBiquadFilterParamDescriptions[1].defaultValue.real = 1.0f;
		ngsBiquadFilterParamDescriptions[1].maximum.real = 23500.0f;
		ngsBiquadFilterParamDescriptions[1].minimum.real = 20.0f;
	}
	if (ngsBiquadFilterParamDescriptions[2].type == 1) {
		ngsBiquadFilterParamDescriptions[2].minimum.integer = 0;
		ngsBiquadFilterParamDescriptions[2].maximum.integer = 10;
		ngsBiquadFilterParamDescriptions[2].defaultValue.integer = 0;
	} else if (ngsBiquadFilterParamDescriptions[2].type == 2) {
		ngsBiquadFilterParamDescriptions[2].minimum.real = 0.5f;
		ngsBiquadFilterParamDescriptions[2].defaultValue.real = 0.5f;
		ngsBiquadFilterParamDescriptions[2].maximum.real = 10.0f;
	}
	if (ngsBiquadFilterParamDescriptions[3].type == 1) {
		ngsBiquadFilterParamDescriptions[3].minimum.integer = -90;
		ngsBiquadFilterParamDescriptions[3].maximum.integer = 24;
		ngsBiquadFilterParamDescriptions[3].defaultValue.integer = 0;
	} else if (ngsBiquadFilterParamDescriptions[3].type == 2) {
		ngsBiquadFilterParamDescriptions[3].minimum.real = -90.0f;
		ngsBiquadFilterParamDescriptions[3].defaultValue.real = 0.0f;
		ngsBiquadFilterParamDescriptions[3].maximum.real = 24.0f;
	}
	ngsBiquadFilterParamInterfaces[0].paramsId = 0x01015ce4;
	ngsBiquadFilterParamInterfaces[0].size = 24;
	ngsBiquadFilterParamInterfaces[0].params = ngsBiquadFilterParamDescriptions;
	ngsBiquadFilterParamInterfaces[1].paramsId = 0x02015ce4;
	ngsBiquadFilterParamInterfaces[0].paramCount = 4;
	ngsBiquadFilterModule.outputs = ngsBiquadFilterOutputs;
	ngsBiquadFilterParamInterfaces[1].size = 28;
	ngsBiquadFilterParamInterfaces[1].params = ngsBiquadFilterCoefficientDescriptions;
	ngsBiquadFilterCoefficientDescriptions[0].defaultValue.real = 1.0f;
	ngsBiquadFilterCoefficientDescriptions[0].minimum.real = -127.0f;
	ngsBiquadFilterCoefficientDescriptions[1].minimum.real = -127.0f;
	ngsBiquadFilterCoefficientDescriptions[2].minimum.real = -127.0f;
	ngsBiquadFilterCoefficientDescriptions[3].minimum.real = -127.0f;
	ngsBiquadFilterCoefficientDescriptions[4].minimum.real = -127.0f;
	ngsBiquadFilterCoefficientDescriptions[0].maximum.real = 127.0f;
	ngsBiquadFilterCoefficientDescriptions[1].maximum.real = 127.0f;
	ngsBiquadFilterCoefficientDescriptions[2].maximum.real = 127.0f;
	ngsBiquadFilterCoefficientDescriptions[3].maximum.real = 127.0f;
	ngsBiquadFilterCoefficientDescriptions[4].maximum.real = 127.0f;
	ngsBiquadFilterCoefficientDescriptions[1].defaultValue.real = 0.0f;
	ngsBiquadFilterCoefficientDescriptions[2].defaultValue.real = 0.0f;
	ngsBiquadFilterCoefficientDescriptions[3].defaultValue.real = 0.0f;
	ngsBiquadFilterCoefficientDescriptions[4].defaultValue.real = 0.0f;
	ngsBiquadFilterParamInterfaces[1].paramCount = 5;
	ngsBiquadFilterModule.paramInterfaces = ngsBiquadFilterParamInterfaceList;
	ngsBiquadFilterModule.codecResourceId = 0;
	ngsBiquadFilterModule.info = "name = Bi-quad Filter\n"
		"version = 1.0.001\n"
		"vendor = Sony Computer Entertainment\n"
		"copyright = (C) 2011 Sony Computer Entertainment Inc. All Rights Reserved.\n"
		"category = Filter\n"
		"desc = Performs filtering on an audio signal\n";
	ngsBiquadFilterModule.outputCount = 1;
	ngsBiquadFilterModule.version = 0x10002;
	ngsBiquadFilterModule.getMemoryInfo = ngsGetBiquadFilterMemoryInfo;
	ngsBiquadFilterModule.moduleId = 0x5ce4;
	ngsBiquadFilterModule.dspDispatchIds[0] = 0x5ce4;
	ngsBiquadFilterModule.dspDispatchIds[1] = 0x5ce4;
	ngsBiquadFilterModule.dspControl = 0;
	ngsBiquadFilterPresets[1].params = &ngsBiquadFilterPresetParams[1].desc;
	ngsBiquadFilterPresets[0].params = &ngsBiquadFilterPresetParams[0].desc;
	ngsBiquadFilterModule.presets = ngsBiquadFilterPresets;
	ngsBiquadFilterPresets[0].name = "Low pass 1 pole";
	ngsBiquadFilterPresets[1].name = "Low pass 2 pole";
	ngsBiquadFilterModule.flags = 2;
	ngsBiquadFilterModule.presetCount = 2;
	*module = &ngsBiquadFilterModule;
	return 0;
}

