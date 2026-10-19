/* Built-in distortion module description and parameter interfaces. */

#include "ngs_module_memory.h"

typedef struct SceNgsDistortionParams {
	SceNgsParamsDescriptor desc;
	float fA;
	float fB;
	float fClip;
	float fGate;
	float fWetGain;
	float fDryGain;
} SceNgsDistortionParams;

extern const char *ngsModuleOutputName;
extern const char *ngsModuleInputName;

extern struct NgsModuleParamInterface ngsDistortionParamInterface;
const struct NgsModuleParamInterface *ngsDistortionParamInterfaceList[] = {
	&ngsDistortionParamInterface
};

NgsModuleParamDescription ngsDistortionParamDescriptions[] = {
	{ "A", "", "Distortion control A, out = in * A - in * B * B", 2 },
	{ "B", "", "Distortion control B, out = in * A - in * B * B", 2 },
	{ "Clip", "", "Linear clip threshold, post distortion.", 2 },
	{ "Gate", "", "Linear gate threshold, post distortion.", 2 },
	{ "Wet Gain", "", "Linear volume control for wet output. Value ranges from 0 to n, "
		"where 2 = double amplitude.", 2 },
	{ "Dry Gain", "", "Linear volume control for wet output. Value ranges from 0 to n, "
		"where 2 = double amplitude.", 2 },
};

NgsModuleAudioInterface ngsDistortionOutputs[1];
SceNgsDistortionParams ngsDistortionPresetParams;
NgsModulePreset ngsDistortionPresets[1];
struct NgsModuleParamInterface ngsDistortionParamInterface;
NgsModuleDefinition ngsDistortionModule;
NgsModuleAudioInterface ngsDistortionInputs[1];

int ngsGetDistortionModule(NgsModuleDefinition **module)
{
	const struct NgsAudioFormat *format = ngsGetDistortionFormat();

	ngsDistortionOutputs[0].name = ngsModuleOutputName;
	ngsDistortionInputs[0].formats = format;
	ngsDistortionInputs[0].supportedFormatCount = 1;
	ngsDistortionOutputs[0].formats = format;
	ngsDistortionInputs[0].name = ngsModuleInputName;
	ngsDistortionOutputs[0].supportedFormatCount = 1;
	ngsDistortionModule.presets = ngsDistortionPresets;
	ngsDistortionModule.paramInterfaceCount = 1;
	ngsDistortionModule.presetCount = 1;
	ngsDistortionModule.inputCount = 1;
	ngsDistortionModule.inputs = ngsDistortionInputs;
	ngsDistortionPresets[0].name = "Overdrive";
	ngsDistortionPresets[0].params = &ngsDistortionPresetParams.desc;
	ngsDistortionPresetParams.desc.id = 0x01015ce2;
	ngsDistortionPresetParams.desc.size = sizeof(ngsDistortionPresetParams);
	ngsDistortionPresetParams.fA = 2.0f;
	ngsDistortionPresetParams.fB = 1.0f;
	ngsDistortionPresetParams.fClip = 1.0f;
	ngsDistortionPresetParams.fDryGain = 1.0f;
	ngsDistortionPresetParams.fGate = 0.0f;
	ngsDistortionPresetParams.fWetGain = 0.0f;

	if (ngsDistortionParamDescriptions[0].type == 1) {
		ngsDistortionParamDescriptions[0].minimum.integer = 0;
		ngsDistortionParamDescriptions[0].maximum.integer = 10;
		ngsDistortionParamDescriptions[0].defaultValue.integer = 2;
	} else if (ngsDistortionParamDescriptions[0].type == 2) {
		ngsDistortionParamDescriptions[0].minimum.real = 0.0f;
		ngsDistortionParamDescriptions[0].maximum.real = 10.0f;
		ngsDistortionParamDescriptions[0].defaultValue.real = 2.0f;
	}
	if (ngsDistortionParamDescriptions[1].type == 1) {
		ngsDistortionParamDescriptions[1].defaultValue.integer = 1;
		ngsDistortionParamDescriptions[1].minimum.integer = 0;
		ngsDistortionParamDescriptions[1].maximum.integer = 10;
	} else if (ngsDistortionParamDescriptions[1].type == 2) {
		ngsDistortionParamDescriptions[1].defaultValue.real = 1.0f;
		ngsDistortionParamDescriptions[1].minimum.real = 0.0f;
		ngsDistortionParamDescriptions[1].maximum.real = 10.0f;
	}
	if (ngsDistortionParamDescriptions[2].type == 1) {
		ngsDistortionParamDescriptions[2].defaultValue.integer = 1;
		ngsDistortionParamDescriptions[2].minimum.integer = 0;
		ngsDistortionParamDescriptions[2].maximum.integer = 4;
	} else if (ngsDistortionParamDescriptions[2].type == 2) {
		ngsDistortionParamDescriptions[2].defaultValue.real = 1.0f;
		ngsDistortionParamDescriptions[2].minimum.real = 0.0f;
		ngsDistortionParamDescriptions[2].maximum.real = 4.0f;
	}
	if (ngsDistortionParamDescriptions[3].type == 1) {
		ngsDistortionParamDescriptions[3].minimum.integer = 0;
		ngsDistortionParamDescriptions[3].maximum.integer = 4;
		ngsDistortionParamDescriptions[3].defaultValue.integer = 0;
	} else if (ngsDistortionParamDescriptions[3].type == 2) {
		ngsDistortionParamDescriptions[3].minimum.real = 0.0f;
		ngsDistortionParamDescriptions[3].defaultValue.real = 0.0f;
		ngsDistortionParamDescriptions[3].maximum.real = 4.0f;
	}
	if (ngsDistortionParamDescriptions[4].type == 1) {
		ngsDistortionParamDescriptions[4].minimum.integer = 0;
		ngsDistortionParamDescriptions[4].maximum.integer = 4;
		ngsDistortionParamDescriptions[4].defaultValue.integer = 0;
	} else if (ngsDistortionParamDescriptions[4].type == 2) {
		ngsDistortionParamDescriptions[4].minimum.real = 0.0f;
		ngsDistortionParamDescriptions[4].defaultValue.real = 0.0f;
		ngsDistortionParamDescriptions[4].maximum.real = 4.0f;
	}
	if (ngsDistortionParamDescriptions[5].type == 1) {
		ngsDistortionParamDescriptions[5].defaultValue.integer = 1;
		ngsDistortionParamDescriptions[5].minimum.integer = 0;
		ngsDistortionParamDescriptions[5].maximum.integer = 4;
	} else if (ngsDistortionParamDescriptions[5].type == 2) {
		ngsDistortionParamDescriptions[5].minimum.real = 0.0f;
		ngsDistortionParamDescriptions[5].maximum.real = 4.0f;
		ngsDistortionParamDescriptions[5].defaultValue.real = 1.0f;
	}

	ngsDistortionModule.outputs = ngsDistortionOutputs;
	ngsDistortionParamInterface.params = ngsDistortionParamDescriptions;
	ngsDistortionParamInterface.paramsId = 0x01015ce2;
	ngsDistortionParamInterface.size = 32;
	ngsDistortionParamInterface.paramCount = 6;
	ngsDistortionModule.paramInterfaces = ngsDistortionParamInterfaceList;
	ngsDistortionModule.info = "name = Distortion\n"
		"version = 1.0.001\n"
		"vendor = Sony Computer Entertainment\n"
		"copyright = (C) 2009 Sony Computer Entertainment Inc. All Rights Reserved.\n"
		"category = Distortion\n"
		"desc = Polynomial distortion.\n";
	ngsDistortionModule.outputCount = 1;
	ngsDistortionModule.version = 0x10002;
	ngsDistortionModule.getMemoryInfo = ngsGetDistortionMemoryInfo;
	ngsDistortionModule.moduleId = 0x5ce2;
	ngsDistortionModule.dspDispatchIds[0] = 0x5ce2;
	ngsDistortionModule.dspDispatchIds[1] = 0x5ce2;
	ngsDistortionModule.codecResourceId = 0;
	ngsDistortionModule.dspControl = 0;
	ngsDistortionModule.flags = 0;
	*module = &ngsDistortionModule;
	return 0;
}
