/* Built-in parametric equalizer description and parameter interfaces. */

#include <ngs/ngs_top.h>
#include <ngs/modules/para_eq.h>

#include "ngs_module_memory.h"
#include "ngs_platform.h"
#include "ngs_system.h"

#define NGS_PARAM_EQ_PARAM_COUNT	(4 * SCE_NGS_MAX_EQ_FILTERS)
#define NGS_PARAM_EQ_COEFF_COUNT	(5 * SCE_NGS_MAX_EQ_FILTERS)

extern const char *ngsModuleOutputName;
extern const char *ngsModuleInputName;

/* Mode, frequency, resonance and gain of each filter. */
static const float ngsParametricEqualizerMinimums[NGS_PARAM_EQ_PARAM_COUNT] = {
	0.0f, 20.0f, 0.5f, -90.0f, 0.0f, 20.0f, 0.5f, -90.0f,
	0.0f, 20.0f, 0.5f, -90.0f, 0.0f, 20.0f, 0.5f, -90.0f,
};
static const float ngsParametricEqualizerDefaults[NGS_PARAM_EQ_PARAM_COUNT] = {
	0.0f, 100.0f, 0.5f, 0.0f, 0.0f, 100.0f, 0.5f, 0.0f,
	0.0f, 100.0f, 0.5f, 0.0f, 0.0f, 100.0f, 0.5f, 0.0f,
};
static const float ngsParametricEqualizerMaximums[NGS_PARAM_EQ_PARAM_COUNT] = {
	12.0f, 23500.0f, 10.0f, 24.0f, 12.0f, 23500.0f, 10.0f, 24.0f,
	12.0f, 23500.0f, 10.0f, 24.0f, 12.0f, 23500.0f, 10.0f, 24.0f,
};

const SceNgsParamEqParams ngsParametricEqualizerPresetParams[2] = {
	{ { SCE_NGS_PARAM_EQ_STRUCT_ID, sizeof(SceNgsParamEqParams) }, {
		{ 9, 400.0f, 1.0f, 0.0f }, { 0, 400.0f, 1.0f, 0.0f },
		{ 0, 400.0f, 1.0f, 0.0f }, { 0, 400.0f, 1.0f, 0.0f },
	} },
	{ { SCE_NGS_PARAM_EQ_STRUCT_ID, sizeof(SceNgsParamEqParams) }, {
		{ 8, 80.0f, 1.0f, 0.0f }, { 6, 700.0f, 1.0f, 0.0f },
		{ 6, 2000.0f, 1.0f, 0.0f }, { 7, 8000.0f, 1.0f, 0.0f },
	} },
};

extern struct NgsModuleParamInterface ngsParametricEqualizerParamInterfaces[2];
const struct NgsModuleParamInterface *ngsParametricEqualizerParamInterfaceList[2] = {
	&ngsParametricEqualizerParamInterfaces[0],
	&ngsParametricEqualizerParamInterfaces[1]
};

#define NGS_PARAM_EQ_FILTER_DESCRIPTIONS \
	{ "Mode", "", "Selected filter mode", 1 }, \
	{ "Frequency", "Hz", "Cut-off frequency of the filter.", 2 }, \
	{ "Resonance", "", "Amount of resonant Q for the filter.This parameter only affects certain filter modes. Typical ranges are 0.2 to 10.0.", 2 }, \
	{ "Gain", "dB", "Decibel gain or cut-off of the filter. This parameter only affects certain filter modes", 2 }

NgsModuleParamDescription ngsParametricEqualizerParamDescriptions[NGS_PARAM_EQ_PARAM_COUNT] = {
	NGS_PARAM_EQ_FILTER_DESCRIPTIONS,
	NGS_PARAM_EQ_FILTER_DESCRIPTIONS,
	NGS_PARAM_EQ_FILTER_DESCRIPTIONS,
	NGS_PARAM_EQ_FILTER_DESCRIPTIONS,
};

NgsModuleParamDescription ngsParametricEqualizerCoefficientDescriptions[NGS_PARAM_EQ_COEFF_COUNT] = {
	{ "F0.B0", "", "CoEfficient B0", 2 },
	{ "F0.B1", "", "CoEfficient B1", 2 },
	{ "F0.B2", "", "CoEfficient B2", 2 },
	{ "F0.A1", "", "CoEfficient A1", 2 },
	{ "F0.A2", "", "CoEfficient A2", 2 },
	{ "F1.B0", "", "CoEfficient B0", 2 },
	{ "F1.B1", "", "CoEfficient B1", 2 },
	{ "F1.B2", "", "CoEfficient B2", 2 },
	{ "F1.A1", "", "CoEfficient A1", 2 },
	{ "F1.A2", "", "CoEfficient A2", 2 },
	{ "F2.B0", "", "CoEfficient B0", 2 },
	{ "F2.B1", "", "CoEfficient B1", 2 },
	{ "F2.B2", "", "CoEfficient B2", 2 },
	{ "F2.A1", "", "CoEfficient A1", 2 },
	{ "F2.A2", "", "CoEfficient A2", 2 },
	{ "F3.B0", "", "CoEfficient B0", 2 },
	{ "F3.B1", "", "CoEfficient B1", 2 },
	{ "F3.B2", "", "CoEfficient B2", 2 },
	{ "F3.A1", "", "CoEfficient A1", 2 },
	{ "F3.A2", "", "CoEfficient A2", 2 },
};

NgsModuleAudioInterface ngsParametricEqualizerOutputs[1];
NgsModuleAudioInterface ngsParametricEqualizerInputs[1];
struct NgsModuleParamInterface ngsParametricEqualizerParamInterfaces[2];
NgsModulePreset ngsParametricEqualizerPresets[2];
NgsModuleDefinition ngsParametricEqualizerModule;

int ngsGetParametricEqualizerModule(NgsModuleDefinition **module)
{
	const struct NgsAudioFormat *format = ngsGetParametricEqualizerFormat();
	NgsProcessStorage *storage = NULL;
	NgsModuleParamDescription *description;
	unsigned int sdkVersion, index;
	int count, coefficient;

	ngsParametricEqualizerOutputs[0].name = ngsModuleOutputName;
	ngsParametricEqualizerOutputs[0].formats = format;
	ngsParametricEqualizerInputs[0].formats = format;
	ngsParametricEqualizerInputs[0].name = ngsModuleInputName;
	ngsParametricEqualizerInputs[0].supportedFormatCount = 1;
	ngsParametricEqualizerOutputs[0].supportedFormatCount = 1;
	ngsParametricEqualizerModule.paramInterfaceCount = 2;
	ngsParametricEqualizerModule.inputCount = 1;
	ngsParametricEqualizerModule.inputs = ngsParametricEqualizerInputs;
	if (ngsGetProcessStorage(&storage) == 0 && storage != NULL &&
	    storage->system != NULL)
		sdkVersion = storage->system->compiledSdkVersion;
	else
		sdkVersion = 0x1800000;

	for (index = 0; index < NGS_PARAM_EQ_PARAM_COUNT; index++) {
		description = &ngsParametricEqualizerParamDescriptions[index];
		if (description->type == 1) {
			description->minimum.integer =
				(int)ngsParametricEqualizerMinimums[index];
			description->maximum.integer =
				(int)ngsParametricEqualizerMaximums[index];
			description->defaultValue.integer =
				(int)ngsParametricEqualizerDefaults[index];
		} else if (description->type == 2) {
			description->minimum.real =
				ngsParametricEqualizerMinimums[index];
			description->maximum.real =
				ngsParametricEqualizerMaximums[index];
			description->defaultValue.real =
				ngsParametricEqualizerDefaults[index];
		}
	}

	/* Titles built before SDK 1.80 see the first 16 coefficients only. */
	count = sdkVersion < 0x1800000 ? 16 : NGS_PARAM_EQ_COEFF_COUNT;
	ngsParametricEqualizerParamInterfaces[0].paramsId =
		SCE_NGS_PARAM_EQ_STRUCT_ID;
	ngsParametricEqualizerParamInterfaces[0].size =
		sizeof(SceNgsParamEqParams);
	ngsParametricEqualizerParamInterfaces[0].paramCount =
		NGS_PARAM_EQ_PARAM_COUNT;
	ngsParametricEqualizerParamInterfaces[0].params =
		ngsParametricEqualizerParamDescriptions;
	for (coefficient = 0; coefficient < count; coefficient++) {
		ngsParametricEqualizerCoefficientDescriptions[coefficient].minimum.real =
			-127.0f;
		ngsParametricEqualizerCoefficientDescriptions[coefficient].maximum.real =
			127.0f;
		ngsParametricEqualizerCoefficientDescriptions[coefficient].defaultValue.real =
			0.0f;
	}
	ngsParametricEqualizerModule.outputs = ngsParametricEqualizerOutputs;
	ngsParametricEqualizerPresets[0].params =
		&ngsParametricEqualizerPresetParams[0].desc;
	ngsParametricEqualizerPresets[1].params =
		&ngsParametricEqualizerPresetParams[1].desc;
	ngsParametricEqualizerPresets[0].name = "Single low pass filter";
	ngsParametricEqualizerPresets[1].name = "4 band EQ";
	ngsParametricEqualizerParamInterfaces[1].params =
		ngsParametricEqualizerCoefficientDescriptions;
	ngsParametricEqualizerCoefficientDescriptions[0].defaultValue.real = 1.0f;
	ngsParametricEqualizerCoefficientDescriptions[5].defaultValue.real = 1.0f;
	ngsParametricEqualizerCoefficientDescriptions[10].defaultValue.real = 1.0f;
	ngsParametricEqualizerCoefficientDescriptions[15].defaultValue.real = 1.0f;
	ngsParametricEqualizerParamInterfaces[1].paramsId =
		SCE_NGS_PARAM_EQ_COEFF_STRUCT_ID;
	ngsParametricEqualizerModule.presets = ngsParametricEqualizerPresets;
	ngsParametricEqualizerModule.version = 0x10002;
	ngsParametricEqualizerParamInterfaces[1].size =
		sizeof(SceNgsParamEqParamsCoEff);
	ngsParametricEqualizerParamInterfaces[1].paramCount =
		NGS_PARAM_EQ_COEFF_COUNT;
	ngsParametricEqualizerModule.codecResourceId = 0;
	ngsParametricEqualizerModule.dspControl = 0;
	ngsParametricEqualizerModule.flags = 2;
	ngsParametricEqualizerModule.paramInterfaces =
		ngsParametricEqualizerParamInterfaceList;
	ngsParametricEqualizerModule.info = "name = Parametric Equalizer\n"
		"version = 1.0.001\n"
		"vendor = Sony Computer Entertainment\n"
		"copyright = (C) 2011 Sony Computer Entertainment Inc. All Rights Reserved.\n"
		"category = EQ / Filter\n"
		"desc = Performs equalization and filtering on an audio signal\n";
	ngsParametricEqualizerModule.outputCount = 1;
	ngsParametricEqualizerModule.getMemoryInfo =
		ngsGetParametricEqualizerMemoryInfo;
	ngsParametricEqualizerModule.moduleId = SCE_NGS_PARAM_EQ_ID;
	ngsParametricEqualizerModule.dspDispatchIds[0] = SCE_NGS_PARAM_EQ_ID;
	ngsParametricEqualizerModule.dspDispatchIds[1] = SCE_NGS_PARAM_EQ_ID;
	ngsParametricEqualizerModule.presetCount = 2;
	*module = &ngsParametricEqualizerModule;
	return 0;
}
