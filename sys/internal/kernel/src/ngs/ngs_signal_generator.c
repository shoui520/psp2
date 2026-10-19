/* Built-in signal generator description and parameter interfaces. */

#include "ngs_module_memory.h"

typedef struct SceNgsGeneratorSettings {
	unsigned int eGeneratorMode;
	int nFrequency;
	float fAmplitude;
	float fPulseWidth;
	unsigned int uSampleOffset;
	unsigned int uPhaseAngle;
} SceNgsGeneratorSettings;

typedef struct SceNgsGeneratorParams {
	SceNgsParamsDescriptor desc;
	SceNgsGeneratorSettings generator;
} SceNgsGeneratorParams;

extern const char *ngsModuleOutputName;

const SceNgsGeneratorParams ngsSignalGeneratorPresetParams = {
	.desc = { 0x01015ce8, 32 },
};

extern struct NgsModuleParamInterface ngsSignalGeneratorParamInterface;
const struct NgsModuleParamInterface *ngsSignalGeneratorParamInterfaceList[] = {
	&ngsSignalGeneratorParamInterface
};

NgsModuleParamDescription ngsSignalGeneratorParamDescriptions[6] = {
	{ "Mode", "", "Signal Generating mode", 5,
	  { .integer = 0 }, { .integer = 5 }, { .integer = 0 } },
	{ "Frequency", "", "Frequency of the signal generated.", 1,
	  { .integer = 0 }, { .integer = 12000 }, { .integer = 0 } },
	{ "Amplitude", "", "Amplitude of the signal generated.", 2 },
	{ "PulseWidth", "", "Pulse width if the mode is Pulse.", 2 },
	{ "Sample Offset", "", "Sample offset of generated signal.", 5,
	  { .integer = 0 }, { .unsignedInteger = 0xffffffff }, { .integer = 0 } },
	{ "Phase", "Degrees", "Start phase of the signal generated", 5,
	  { .integer = 0 }, { .integer = 360 }, { .integer = 0 } },
};

NgsModuleAudioInterface ngsSignalGeneratorOutputs[1];
struct NgsModuleParamInterface ngsSignalGeneratorParamInterface;
NgsModuleDefinition ngsSignalGeneratorModule;
NgsModulePreset ngsSignalGeneratorPresets[1];

int ngsGetSignalGeneratorModule(NgsModuleDefinition **module)
{
	const struct NgsAudioFormat *format = ngsGetSignalGeneratorFormat();

	ngsSignalGeneratorOutputs[0].supportedFormatCount = 1;
	ngsSignalGeneratorOutputs[0].formats = format;
	ngsSignalGeneratorOutputs[0].name = ngsModuleOutputName;
	ngsSignalGeneratorModule.outputs = ngsSignalGeneratorOutputs;
	ngsSignalGeneratorModule.inputCount = 0;
	ngsSignalGeneratorModule.inputs = 0;
	ngsSignalGeneratorModule.dspControl = 0;
	ngsSignalGeneratorModule.flags = 0;
	ngsSignalGeneratorModule.paramInterfaceCount = 1;
	ngsSignalGeneratorModule.outputCount = 1;
	ngsSignalGeneratorModule.presetCount = 1;
	ngsSignalGeneratorModule.paramInterfaces = ngsSignalGeneratorParamInterfaceList;
	ngsSignalGeneratorModule.info = "name = Signal Generator\n"
		"version = 1.0.001\n"
		"vendor = Sony Computer Entertainment\n"
		"copyright = (C) 2009 Sony Computer Entertainment Inc. All Rights Reserved.\n"
		"category = Source\n"
		"desc = Creates basic signal shapes, 8 parallel signal generators (sine, saw, triangle, pulse, noise, etc.).\n";
	ngsSignalGeneratorModule.codecResourceId = 0;
	ngsSignalGeneratorModule.dspDispatchIds[0] = 0x5ce8;
	ngsSignalGeneratorModule.dspDispatchIds[1] = 0x5ce8;
	ngsSignalGeneratorModule.getMemoryInfo = ngsGetSignalGeneratorMemoryInfo;
	ngsSignalGeneratorModule.version = 0x10002;
	ngsSignalGeneratorModule.moduleId = 0x5ce8;
	ngsSignalGeneratorModule.presets = ngsSignalGeneratorPresets;
	ngsSignalGeneratorParamDescriptions[2].maximum.real = 1.0f;
	ngsSignalGeneratorPresets[0].name = "All Off";
	ngsSignalGeneratorPresets[0].params = &ngsSignalGeneratorPresetParams.desc;
	ngsSignalGeneratorParamDescriptions[3].maximum.real = 1.0f;
	ngsSignalGeneratorParamInterface.params = ngsSignalGeneratorParamDescriptions;
	ngsSignalGeneratorParamInterface.paramsId = 0x01015ce8;
	ngsSignalGeneratorParamInterface.size = 32;
	ngsSignalGeneratorParamInterface.paramCount = 6;
	*module = &ngsSignalGeneratorModule;
	return 0;
}
