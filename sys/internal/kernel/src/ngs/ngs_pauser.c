/* Built-in pauser module description and parameter interfaces. */

#include "ngs_module_memory.h"

typedef struct SceNgsPauserParams {
	SceNgsParamsDescriptor desc;
	unsigned int uPauseLeadIn;
	unsigned int uPauseLeadOut;
} SceNgsPauserParams;

extern const char *ngsModuleOutputName;
extern const char *ngsModuleInputName;

const SceNgsPauserParams ngsPauserPresetParams = {
	.desc = { 0x01015ce5, 16 },
	.uPauseLeadIn = 32,
	.uPauseLeadOut = 32,
};

extern struct NgsModuleParamInterface ngsPauserParamInterface;
const struct NgsModuleParamInterface *ngsPauserParamInterfaceList[] = {
	&ngsPauserParamInterface
};

NgsModuleParamDescription ngsPauserParamDescriptions[] = {
	{ "Lead in", "Samples", "Number of samples to linearly smooth in when a voice is unpaused.", 1 },
	{ "Lead out", "Samples", "Number of samples to linearly smooth in when a voice is paused.", 1 },
};

NgsModuleAudioInterface ngsPauserOutputs[1];
struct NgsModuleParamInterface ngsPauserParamInterface;
NgsModuleAudioInterface ngsPauserInputs[1];
NgsModuleDefinition ngsPauserModule;
NgsModulePreset ngsPauserPresets[1];

int ngsGetPauserModule(NgsModuleDefinition **module)
{
	ngsPauserOutputs[0].formats = ngsGetPauserFormat();
	ngsPauserOutputs[0].name = ngsModuleOutputName;
	ngsPauserInputs[0].formats = ngsPauserOutputs[0].formats;
	ngsPauserInputs[0].name = ngsModuleInputName;
	ngsPauserInputs[0].supportedFormatCount = 1;
	ngsPauserOutputs[0].supportedFormatCount = 1;
	ngsPauserModule.paramInterfaceCount = 1;
	ngsPauserModule.inputCount = 1;
	ngsPauserModule.inputs = ngsPauserInputs;

	if (ngsPauserParamDescriptions[0].type == 1) {
		ngsPauserParamDescriptions[0].minimum.integer = 0;
		ngsPauserParamDescriptions[0].maximum.integer = 256;
		ngsPauserParamDescriptions[0].defaultValue.integer = 32;
	} else if (ngsPauserParamDescriptions[0].type == 2) {
		ngsPauserParamDescriptions[0].maximum.real = 256.0f;
		ngsPauserParamDescriptions[0].defaultValue.real = 32.0f;
		ngsPauserParamDescriptions[0].minimum.real = 0.0f;
	}
	if (ngsPauserParamDescriptions[1].type == 1) {
		ngsPauserParamDescriptions[1].minimum.integer = 0;
		ngsPauserParamDescriptions[1].maximum.integer = 256;
		ngsPauserParamDescriptions[1].defaultValue.integer = 32;
	} else if (ngsPauserParamDescriptions[1].type == 2) {
		ngsPauserParamDescriptions[1].minimum.real = 0.0f;
		ngsPauserParamDescriptions[1].maximum.real = 256.0f;
		ngsPauserParamDescriptions[1].defaultValue.real = 32.0f;
	}

	ngsPauserModule.outputs = ngsPauserOutputs;
	ngsPauserParamInterface.paramCount = 2;
	ngsPauserParamInterface.params = ngsPauserParamDescriptions;
	ngsPauserParamInterface.paramsId = 0x01015ce5;
	ngsPauserModule.version = 0x10002;
	ngsPauserParamInterface.size = 16;
	ngsPauserModule.outputCount = 1;
	ngsPauserModule.presetCount = 1;
	ngsPauserModule.paramInterfaces = ngsPauserParamInterfaceList;
	ngsPauserModule.info = "name = Pauser\n"
		"version = 1.0.001\n"
		"vendor = Sony Computer Entertainment\n"
		"copyright = (C) 2009 Sony Computer Entertainment Inc. All Rights Reserved.\n"
		"category = Pauser\n"
		"desc = Allows soft pause/resume without introducing clicks.\n";
	ngsPauserModule.getMemoryInfo = ngsGetPauserMemoryInfo;
	ngsPauserModule.presets = ngsPauserPresets;
	ngsPauserPresets[0].name = "32 sample lead in/out";
	ngsPauserPresets[0].params = &ngsPauserPresetParams.desc;
	ngsPauserModule.moduleId = 0x5ce5;
	ngsPauserModule.dspDispatchIds[0] = 0x5ce5;
	ngsPauserModule.dspDispatchIds[1] = 0x5ce5;
	ngsPauserModule.codecResourceId = 0;
	ngsPauserModule.dspControl = 0;
	ngsPauserModule.flags = 0;
	*module = &ngsPauserModule;
	return 0;
}
