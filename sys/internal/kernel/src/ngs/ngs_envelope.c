/* Built-in envelope module description and parameter interfaces. */

#include <string.h>

#include "ngs_module_memory.h"

typedef struct SceNgsEnvelopePoint {
	unsigned int uMsecsToNextPoint;
	float fAmplitude;
	unsigned int eCurveType;
} SceNgsEnvelopePoint;

typedef struct SceNgsEnvelopeParams {
	SceNgsParamsDescriptor desc;
	SceNgsEnvelopePoint envelopePoints[4];
	unsigned int uReleaseMsecs;
	unsigned int uNumPoints;
	unsigned int uLoopStart;
	int nLoopEnd;
} SceNgsEnvelopeParams;

extern const char *ngsModuleOutputName;
extern const char *ngsModuleInputName;

const SceNgsEnvelopeParams ngsEnvelopePresetParams = {
	.desc = { 0x01015ce3, 72 },
	.envelopePoints = {
		{ 256, 1.0f, 1 }, { 256, 1.0f, 1 },
		{ 256, 1.0f, 1 }, { 256, 1.0f, 1 },
	},
	.uReleaseMsecs = 64,
	.uNumPoints = 1,
	.uLoopStart = 0,
	.nLoopEnd = -1,
};

const NgsModuleParamDescription ngsEnvelopeControlParamDescriptions[] = {
	{ "Release Time", "", "Release time, post key-off.", 5,
		{ .unsignedInteger = 0 }, { .unsignedInteger = ~0U }, { .unsignedInteger = 0 } },
	{ "NumPoints", "", "Number of active points in the envelope.", 5,
		{ .unsignedInteger = 0 }, { .unsignedInteger = 4 }, { .unsignedInteger = 4 } },
	{ "Loop Start", "", "Index of the envelope point to start looping from.", 5,
		{ .unsignedInteger = 0 }, { .unsignedInteger = 3 }, { .unsignedInteger = 0 } },
	{ "Loop End", "", "Index of the envelope point, specifying the end of the loop.", 1,
		{ .integer = -3 }, { .integer = 3 }, { .integer = -1 } },
};

extern struct NgsModuleParamInterface ngsEnvelopeParamInterface;
const struct NgsModuleParamInterface *ngsEnvelopeParamInterfaceList[] = {
	&ngsEnvelopeParamInterface
};

NgsModuleParamDescription ngsEnvelopePointParamDescriptions[] = {
	{ "Samples To Next Point", "", "Samples to Next Point in Envelope.", 5,
		{ .unsignedInteger = 0 }, { .unsignedInteger = ~0U }, { .unsignedInteger = 0 } },
	{ "Amplitude", "", "Amplitude of envelope point as linear scalar.", 2 },
	{ "Curve Type", "", "Curve type enumeration.", 5,
		{ .unsignedInteger = 0 }, { .unsignedInteger = 1 }, { .unsignedInteger = 0 } },
};

NgsModuleAudioInterface ngsEnvelopeOutputs[1];
struct NgsModuleParamInterface ngsEnvelopeParamInterface;
NgsModuleAudioInterface ngsEnvelopeInputs[1];
NgsModuleDefinition ngsEnvelopeModule;
NgsModuleParamDescription ngsEnvelopeParamDescriptions[16];
NgsModulePreset ngsEnvelopePresets[1];

int ngsGetEnvelopeModule(NgsModuleDefinition **module)
{
	const struct NgsAudioFormat *format = ngsGetEnvelopeFormat();

	ngsEnvelopePointParamDescriptions[1].minimum.real = 0.0f;
	ngsEnvelopePointParamDescriptions[1].maximum.real = 100.0f;
	ngsEnvelopePointParamDescriptions[1].defaultValue.real = 1.0f;
	ngsEnvelopeInputs[0].formats = format;
	ngsEnvelopeParamDescriptions[0] = ngsEnvelopePointParamDescriptions[0];
	ngsEnvelopeInputs[0].name = ngsModuleInputName;
	ngsEnvelopeOutputs[0].name = ngsModuleOutputName;
	ngsEnvelopeOutputs[0].formats = format;
	ngsEnvelopeOutputs[0].supportedFormatCount = 1;
	ngsEnvelopeInputs[0].supportedFormatCount = 1;
	ngsEnvelopeParamDescriptions[1] = ngsEnvelopePointParamDescriptions[1];
	ngsEnvelopeModule.inputCount = 1;
	ngsEnvelopeModule.paramInterfaceCount = 1;
	ngsEnvelopeModule.inputs = ngsEnvelopeInputs;
	ngsEnvelopeParamDescriptions[2] = ngsEnvelopePointParamDescriptions[2];
	ngsEnvelopeParamDescriptions[3] = ngsEnvelopePointParamDescriptions[0];
	ngsEnvelopeParamDescriptions[4] = ngsEnvelopePointParamDescriptions[1];
	ngsEnvelopeParamDescriptions[5] = ngsEnvelopePointParamDescriptions[2];
	ngsEnvelopeParamDescriptions[6] = ngsEnvelopePointParamDescriptions[0];
	ngsEnvelopeParamDescriptions[7] = ngsEnvelopePointParamDescriptions[1];
	ngsEnvelopeParamDescriptions[8] = ngsEnvelopePointParamDescriptions[2];
	ngsEnvelopeParamDescriptions[9] = ngsEnvelopePointParamDescriptions[0];
	ngsEnvelopeParamDescriptions[10] = ngsEnvelopePointParamDescriptions[1];
	ngsEnvelopeParamDescriptions[11] = ngsEnvelopePointParamDescriptions[2];
	memcpy(&ngsEnvelopeParamDescriptions[12], ngsEnvelopeControlParamDescriptions,
	       sizeof(ngsEnvelopeControlParamDescriptions));
	ngsEnvelopeModule.outputCount = 1;
	ngsEnvelopeParamInterface.params = ngsEnvelopeParamDescriptions;
	ngsEnvelopeModule.outputs = ngsEnvelopeOutputs;
	ngsEnvelopeModule.paramInterfaces = ngsEnvelopeParamInterfaceList;
	ngsEnvelopeModule.presetCount = 1;
	ngsEnvelopeParamInterface.paramsId = 0x01015ce3;
	ngsEnvelopeParamInterface.size = 72;
	ngsEnvelopeParamInterface.paramCount = 16;
	ngsEnvelopeModule.dspDispatchIds[0] = 0x5ce3;
	ngsEnvelopeModule.dspDispatchIds[1] = 0x5ce3;
	ngsEnvelopeModule.dspControl = 0;
	ngsEnvelopeModule.flags = 0;
	ngsEnvelopePresets[0].name = "Basic Envelope";
	ngsEnvelopePresets[0].params = &ngsEnvelopePresetParams.desc;
	ngsEnvelopeModule.info = "name = Envelope\n"
		"version = 1.0.001\n"
		"vendor = Sony Computer Entertainment\n"
		"copyright = (C) 2011 Sony Computer Entertainment Inc. All Rights Reserved.\n"
		"category = Envelope\n"
		"desc = Applies an amplitude envelope to the signal.\n";
	ngsEnvelopeModule.getMemoryInfo = ngsGetEnvelopeMemoryInfo;
	ngsEnvelopeModule.version = 0x10002;
	ngsEnvelopeModule.moduleId = 0x5ce3;
	ngsEnvelopeModule.codecResourceId = 0;
	ngsEnvelopeModule.presets = ngsEnvelopePresets;
	*module = &ngsEnvelopeModule;
	return 0;
}
