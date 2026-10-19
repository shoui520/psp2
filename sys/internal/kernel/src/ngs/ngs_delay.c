/* Built-in modulating delay description and parameter interfaces. */

#include "ngs_module_memory.h"

typedef struct SceNgsDelayTap {
	float fDelayMillisecs;
	float fVolume;
	float fFeedback;
	unsigned int eFilterMode;
	float fCutoff;
	float fPhaseOffsetDeg;
	float fModWidthMillisecs;
} SceNgsDelayTap;

typedef struct SceNgsDelayParams {
	SceNgsParamsDescriptor desc;
	float fDryVol;
	float fModRate;
	SceNgsDelayTap taps[4];
} SceNgsDelayParams;

extern const char *ngsModuleOutputName;
extern const char *ngsModuleInputName;

const float ngsDelayParamMinimum[7] = { 0.0f, -1.0f, -0.95f, 0.0f, 20.0f, -180.0f, 0.0f };

const SceNgsDelayParams ngsDelayPresetParams[6] = {
	{ .desc = { 0x01015ceb, 128 }, .fDryVol = 0.0f,
	  .fModRate = 0.020000000f, .taps = {
			{ 100.0f, 0.40000001f, 0.0f, 1, 6000.0f, 0.0f, 0.0f },
			{ 300.0f, 0.30000001f, 0.0f, 1, 4000.0f, 0.0f, 0.0f },
			{ 500.0f, 0.20000000f, 0.0f, 1, 2000.0f, 0.0f, 0.0f },
			{ 800.0f, 0.10000000f, 0.76999998f, 1, 1000.0f, 0.0f, 0.0099999998f }
	  } },
	{ .desc = { 0x01015ceb, 128 }, .fDryVol = 0.50000000f,
	  .fModRate = 0.69999999f, .taps = {
			{ 40.0f, 0.16666700f, 0.0f, 0, 1000.0f, 0.0f, 1.2000000f },
			{ 40.0f, 0.16666700f, 0.0f, 0, 1000.0f, 120.0f, 1.2000000f },
			{ 40.0f, 0.16666700f, 0.0f, 0, 1000.0f, 240.0f, 1.2000000f },
			{ 0.0f, 0.0f, 0.0f, 0, 1000.0f, 0.0f, 0.0f }
	  } },
	{ .desc = { 0x01015ceb, 128 }, .fDryVol = 0.50000000f,
	  .fModRate = 0.89999998f, .taps = {
			{ 40.0f, 0.33333299f, 0.0f, 0, 1000.0f, 0.0f, 1.2000000f },
			{ 40.0f, 0.16666700f, 0.0f, 0, 1000.0f, 120.0f, 1.2000000f },
			{ 0.0f, 0.0f, 0.0f, 0, 1000.0f, 240.0f, 1.2000000f },
			{ 0.0f, 0.0f, 0.0f, 0, 1000.0f, 0.0f, 0.0f }
	  } },
	{ .desc = { 0x01015ceb, 128 }, .fDryVol = 0.50000000f,
	  .fModRate = 0.89999998f, .taps = {
			{ 0.0f, 0.0f, 0.0f, 0, 1000.0f, 0.0f, 1.2000000f },
			{ 40.0f, 0.16666700f, 0.0f, 0, 1000.0f, 120.0f, 1.2000000f },
			{ 40.0f, 0.33333299f, 0.0f, 0, 1000.0f, 240.0f, 1.2000000f },
			{ 0.0f, 0.0f, 0.0f, 0, 1000.0f, 0.0f, 0.0f }
	  } },
	{ .desc = { 0x01015ceb, 128 }, .fDryVol = 0.0f,
	  .fModRate = 0.10000000f, .taps = {
			{ 10.0f, 0.50000000f, 0.0f, 0, 1000.0f, 0.0f, 9.0f },
			{ 10.0f, 0.50000000f, 0.0f, 0, 1000.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f, 0, 1000.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f, 0, 1000.0f, 0.0f, 0.0f }
	  } },
	{ .desc = { 0x01015ceb, 128 }, .fDryVol = 0.0f,
	  .fModRate = 0.10000000f, .taps = {
			{ 10.0f, 0.50000000f, 0.0f, 0, 1000.0f, 180.0f, 9.0f },
			{ 10.0f, 0.50000000f, 0.0f, 0, 1000.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f, 0, 1000.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f, 0, 1000.0f, 0.0f, 0.0f }
	  } }
};

const float ngsDelayParamMaximum[7] = { 1000.0f, 1.0f, 0.95f, 3.0f, 23500.0f, 360.0f, 500.0f };
const float ngsDelayParamDefault[7] = { 500.0f, 0.25f, 0.1f, 1.0f, 5000.0f, 0.0f, 10.0f };

const NgsModuleParamDescription ngsDelayControlDescriptions[2] = {
	{ "Dry Volume", "", "Dry volume multiplier.", 2,
	  { .integer = 0 }, { .integer = 0 }, { .integer = 0 } },
	{ "LFO Rate", "Hertz", "Internal LFO rate for tap modulation.", 2,
	  { .integer = 0 }, { .integer = 0 }, { .integer = 0 } },
};

extern struct NgsModuleParamInterface ngsDelayParamInterface;
const struct NgsModuleParamInterface *ngsDelayParamInterfaceList[1] = {
	&ngsDelayParamInterface
};

NgsModuleParamDescription ngsDelayTapDescriptions[7] = {
	{ "Delay Time", "Milliseconds", "Delay time of the tap.", 2,
	  { .integer = 0 }, { .integer = 0 }, { .integer = 0 } },
	{ "Volume", "", "Volume multiplier for tap output.", 2,
	  { .integer = 0 }, { .integer = 0 }, { .integer = 0 } },
	{ "Feedback", "", "Feedback multiplier for tap output.", 2,
	  { .integer = 0 }, { .integer = 0 }, { .integer = 0 } },
	{ "Filter Mode", "", "The Filter type to use on the output tap.", 5,
	  { .integer = 0 }, { .integer = 3 }, { .integer = 0 } },
	{ "Filter Cutoff", "Hertz", "Filter cutoff frequency for the tap.", 2,
	  { .integer = 0 }, { .integer = 0 }, { .integer = 0 } },
	{ "Phase Offset", "Degrees", "Phase offset when using the internal LFO.", 2,
	  { .integer = 0 }, { .integer = 0 }, { .integer = 0 } },
	{ "Mod Width", "Milliseconds", "Width of the modulation.", 2,
	  { .integer = 0 }, { .integer = 0 }, { .integer = 0 } }
};

NgsModuleAudioInterface ngsDelayOutputs[1];
NgsModulePreset ngsDelayPresets[6];
struct NgsModuleParamInterface ngsDelayParamInterface;
NgsModuleAudioInterface ngsDelayInputs[5];
NgsModuleParamDescription ngsDelayParamDescriptions[30];
NgsModuleDefinition ngsDelayModule;

int ngsGetDelayModule(NgsModuleDefinition **module)
{
	const struct NgsAudioFormat *format = ngsGetDelayFormat();
	NgsModuleParamDescription *description;
	const float *minimum = ngsDelayParamMinimum;
	const float *maximum = ngsDelayParamMaximum;
	const float *defaults = ngsDelayParamDefault;

	ngsDelayOutputs[0].name = ngsModuleOutputName;
	ngsDelayOutputs[0].formats = format;
	ngsDelayInputs[0].formats = format;
	ngsDelayInputs[1].formats = format;
	ngsDelayInputs[2].formats = format;
	ngsDelayInputs[3].formats = format;
	ngsDelayInputs[4].formats = format;
	ngsDelayInputs[2].name = "Mod Input 2";
	ngsDelayInputs[3].name = "Mod Input 3";
	ngsDelayInputs[4].name = "Mod Input 4";
	ngsDelayInputs[0].name = ngsModuleInputName;
	ngsDelayInputs[1].name = "Mod Input 1";
	ngsDelayPresets[0].name = "Default Delay";
	ngsDelayPresets[0].params = &ngsDelayPresetParams[0].desc;
	ngsDelayPresets[1].params = &ngsDelayPresetParams[1].desc;
	ngsDelayPresets[1].name = "Mono Chorus";
	ngsDelayPresets[2].name = "Stereo Chorus Left";
	ngsDelayPresets[2].params = &ngsDelayPresetParams[2].desc;
	ngsDelayPresets[3].name = "Stereo Chorus Right";
	ngsDelayPresets[5].params = &ngsDelayPresetParams[5].desc;
	ngsDelayPresets[4].name = "Flanger Mono/Left";
	ngsDelayPresets[5].name = "Flanger Right";
	ngsDelayPresets[3].params = &ngsDelayPresetParams[3].desc;
	ngsDelayPresets[4].params = &ngsDelayPresetParams[4].desc;
	ngsDelayOutputs[0].supportedFormatCount = 1;
	ngsDelayInputs[0].supportedFormatCount = 1;
	ngsDelayInputs[1].supportedFormatCount = 1;
	ngsDelayInputs[2].supportedFormatCount = 1;
	ngsDelayInputs[3].supportedFormatCount = 1;
	ngsDelayInputs[4].supportedFormatCount = 1;
	ngsDelayModule.presetCount = 6;
	ngsDelayModule.presets = ngsDelayPresets;
	ngsDelayModule.inputs = ngsDelayInputs;
	ngsDelayModule.paramInterfaceCount = 1;
	ngsDelayModule.inputCount = 5;
	description = ngsDelayTapDescriptions;
	do {
		while (description->type == 2) {
			description->minimum.real = *minimum;
			description->maximum.real = *maximum;
			description->defaultValue.real = *defaults;
			description++;
			maximum++;
			defaults++;
			if (++minimum == ngsDelayParamMinimum + 7)
				goto boundsDone;
		}
		if (description->type == 5) {
			description->minimum.unsignedInteger = (unsigned int)*minimum;
			description->maximum.unsignedInteger = (unsigned int)*maximum;
			description->defaultValue.unsignedInteger = (unsigned int)*defaults;
		} else if (description->type == 1) {
			description->minimum.integer = (int)*minimum;
			description->maximum.integer = (int)*maximum;
			description->defaultValue.integer = (int)*defaults;
		}
		description++;
		maximum++;
		defaults++;
	} while (++minimum != ngsDelayParamMinimum + 7);
boundsDone:
	ngsDelayParamDescriptions[0] = ngsDelayControlDescriptions[0];
	ngsDelayParamDescriptions[0].defaultValue.real = 0.5f;
	ngsDelayParamDescriptions[0].minimum.real = 0.0f;
	ngsDelayParamDescriptions[1] = ngsDelayControlDescriptions[1];
	ngsDelayParamDescriptions[0].maximum.real = 1.0f;
	ngsDelayParamDescriptions[1].maximum.real = 20.0f;
	ngsDelayParamDescriptions[1].minimum.real = 0.0f;
	ngsDelayParamDescriptions[1].defaultValue.real = 1.0f;
	description = ngsDelayParamDescriptions;
	do {
		description[2] = ngsDelayTapDescriptions[0];
		description[8] = ngsDelayTapDescriptions[6];
		description[3] = ngsDelayTapDescriptions[1];
		description[4] = ngsDelayTapDescriptions[2];
		description[5] = ngsDelayTapDescriptions[3];
		description[6] = ngsDelayTapDescriptions[4];
		description[7] = ngsDelayTapDescriptions[5];
		description += 7;
	} while (description != ngsDelayParamDescriptions + 28);
	ngsDelayModule.outputs = ngsDelayOutputs;
	ngsDelayParamInterface.paramsId = 0x01015ceb;
	ngsDelayParamInterface.params = ngsDelayParamDescriptions;
	ngsDelayParamInterface.size = 128;
	ngsDelayParamInterface.paramCount = 30;
	ngsDelayModule.dspDispatchIds[0] = 0x5ceb;
	ngsDelayModule.dspDispatchIds[1] = 0x5ceb;
	ngsDelayModule.dspControl = 3;
	ngsDelayModule.flags = 2;
	ngsDelayModule.paramInterfaces = ngsDelayParamInterfaceList;
	ngsDelayModule.info = "name = Delay\n"
		"version = 1.0.001\n"
		"vendor = Sony Computer Entertainment\n"
		"copyright = (C) 2009 Sony Computer Entertainment Inc. All Rights Reserved.\n"
		"category = Delay / Modulation \n"
		"desc = Modulating delay DSP.\n";
	ngsDelayModule.outputCount = 1;
	ngsDelayModule.version = 0x10002;
	ngsDelayModule.getMemoryInfo = ngsGetDelayMemoryInfo;
	ngsDelayModule.moduleId = 0x5ceb;
	ngsDelayModule.codecResourceId = 0;
	*module = &ngsDelayModule;
	return 0;
}

