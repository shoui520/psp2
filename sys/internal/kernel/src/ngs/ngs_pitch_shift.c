/* Built-in pitch shift description and parameter interface. */

#include "ngs_module_memory.h"

typedef struct SceNgsPitchShiftParams {
	SceNgsParamsDescriptor desc;
	float fPitchOffsetInCents;
} SceNgsPitchShiftParams;

extern const char *ngsModuleOutputName;
extern const char *ngsModuleInputName;

const SceNgsPitchShiftParams ngsPitchShiftPresetParams = {
	.desc = { 0x01015cea, 12 },
	.fPitchOffsetInCents = 0.0f,
};

extern struct NgsModuleParamInterface ngsPitchShiftParamInterface;
const struct NgsModuleParamInterface *ngsPitchShiftParamInterfaceList[1] = {
	&ngsPitchShiftParamInterface
};

NgsModuleParamDescription ngsPitchShiftParamDescriptions[1] = {
	{ "PitchOffsetInCents", "",
	  "Selected playback rate. 0 = no pitch shift, -1200.0f = 1 octave down, 1200.0f = 1 octave up", 2,
	  { .integer = 0 }, { .integer = 0 }, { .integer = 0 } },
};

NgsModuleAudioInterface ngsPitchShiftOutputs[1];
NgsModuleAudioInterface ngsPitchShiftInputs[1];
struct NgsModuleParamInterface ngsPitchShiftParamInterface;
NgsModulePreset ngsPitchShiftPresets[1];
NgsModuleDefinition ngsPitchShiftModule;

int ngsGetPitchShiftModule(NgsModuleDefinition **module)
{
	const struct NgsAudioFormat *format = ngsGetPitchShiftFormat();

	ngsPitchShiftOutputs[0].formats = format;
	ngsPitchShiftOutputs[0].name = ngsModuleOutputName;
	ngsPitchShiftInputs[0].formats = format;
	ngsPitchShiftInputs[0].name = ngsModuleInputName;
	ngsPitchShiftInputs[0].supportedFormatCount = 1;
	ngsPitchShiftOutputs[0].supportedFormatCount = 1;
	ngsPitchShiftModule.paramInterfaceCount = 1;
	ngsPitchShiftModule.inputCount = 1;
	ngsPitchShiftModule.inputs = ngsPitchShiftInputs;
	if (ngsPitchShiftParamDescriptions[0].type == 2) {
		ngsPitchShiftParamDescriptions[0].minimum.real = -1200.0f;
		ngsPitchShiftParamDescriptions[0].maximum.real = 1200.0f;
		ngsPitchShiftParamDescriptions[0].defaultValue.real = 0.0f;
	}
	ngsPitchShiftModule.outputs = ngsPitchShiftOutputs;
	ngsPitchShiftParamInterface.params = ngsPitchShiftParamDescriptions;
	ngsPitchShiftParamInterface.paramsId = 0x01015cea;
	ngsPitchShiftParamInterface.size = 12;
	ngsPitchShiftParamInterface.paramCount = 1;
	ngsPitchShiftModule.outputCount = 1;
	ngsPitchShiftModule.presetCount = 1;
	ngsPitchShiftModule.version = 0x10002;
	ngsPitchShiftModule.paramInterfaces = ngsPitchShiftParamInterfaceList;
	ngsPitchShiftModule.info = "name = Pitch shift\n"
		"version = 1.0.001\n"
		"vendor = Sony Computer Entertainment\n"
		"copyright = (C) 2011 Sony Computer Entertainment Inc. All Rights Reserved.\n"
		"category = Pitch shift\n"
		"desc = Performs a pitch shift on an audio signal\n";
	ngsPitchShiftModule.getMemoryInfo = ngsGetPitchShiftMemoryInfo;
	ngsPitchShiftModule.presets = ngsPitchShiftPresets;
	ngsPitchShiftPresets[0].name = "Default";
	ngsPitchShiftPresets[0].params = &ngsPitchShiftPresetParams.desc;
	ngsPitchShiftModule.moduleId = 0x5cea;
	ngsPitchShiftModule.dspDispatchIds[0] = 0x5cea;
	ngsPitchShiftModule.dspDispatchIds[1] = 0x5cea;
	ngsPitchShiftModule.codecResourceId = 0;
	ngsPitchShiftModule.dspControl = 0;
	ngsPitchShiftModule.flags = 2;
	*module = &ngsPitchShiftModule;
	return 0;
}
