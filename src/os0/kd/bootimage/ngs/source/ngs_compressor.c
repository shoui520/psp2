/* Built-in compressor module description and parameter interfaces. */

#include "ngs_module_memory.h"

typedef struct SceNgsCompressorParams {
	SceNgsParamsDescriptor desc;
	float fRatio;
	float fThreshold;
	float fAttack;
	float fRelease;
	float fMakeupGain;
	int nStereoLink;
	int nPeakMode;
	float fSoftKnee;
} SceNgsCompressorParams;

const struct NgsAudioFormat *ngsGetCompressorFormat(void);
int ngsGetCompressorMemoryInfo(const struct NgsModuleInitParams *params,
			     NgsModuleMemoryInfo *info);

const char *ngsModuleOutputName = "Output";
const char *ngsModuleSideChainName = "Side Chain";
const char *ngsModuleInputName = "Input";
const char *ngsModuleInput1Name = "Input1";
const char *ngsModuleInput2Name = "Input2";

extern struct NgsModuleParamInterface ngsCompressorParamInterfaces[2];
const struct NgsModuleParamInterface *ngsCompressorParamInterfaceList[] = {
	&ngsCompressorParamInterfaces[0], &ngsCompressorParamInterfaces[1]
};

NgsModuleParamDescription ngsCompressorParamDescriptions[] = {
	{
		"Ratio",
		"",
		"Amount the volume is reduced in relation to how far above fThreshold the signal is, "
		"5:1 equates to 1/5 = 0.2",
		2,
	},
	{
		"Threshold",
		"dB",
		"Threshold for automatic volume reduction.",
		2,
	},
	{
		"Attack",
		"sec",
		"Rate (in seconds) at which volume is reduced when volume exceeds fThreshold. Value "
		"ranges from 0 to n.",
		2,
	},
	{
		"Release",
		"sec",
		"Rate (in seconds) at which the volume returns when the volume no longer exceeds "
		"fThreshold.",
		2,
	},
	{
		"Makeup Gain",
		"dB",
		"Volume control for the compressed audio signal, expressed in decibels.",
		2,
	},
	{
		"Stereo Link",
		"",
		"Flag to link the two channels of stereo together and get an average from the two channels.",
		1,
		{ .integer = 0 },
		{ .integer = 1 },
		{ .integer = 0 },
	},
	{
		"Peak Mode",
		"",
		"Flag to use peak mode detection (1) or RMS detection (0).",
		1,
		{ .integer = 0 },
		{ .integer = 1 },
		{ .integer = 1 },
	},
	{
		"Soft knee range",
		"dB",
		"Decibel range for soft knee to operate over, 0dB = hard knee. Note this is centered "
		"around the fThreshold.",
		2,
		{ .integer = 0 },
		{ .integer = 1 },
		{ .integer = 1 },
	},
};

NgsModuleAudioInterface ngsCompressorOutputs[1];
NgsModuleAudioInterface ngsCompressorInputs[2];
SceNgsCompressorParams ngsCompressorPresetParams;
NgsModulePreset ngsCompressorPresets[1];
struct NgsModuleParamInterface ngsCompressorParamInterfaces[2];
NgsModuleDefinition ngsCompressorModule;

#define INIT_COMPRESSOR_PARAM(index, intMin, intMax, intDefault, realMin, realMax, realDefault) \
	do { \
		NgsModuleParamDescription *param = &ngsCompressorParamDescriptions[index]; \
		if (param->type == 1) { \
			param->minimum.integer = intMin; \
			param->maximum.integer = intMax; \
			param->defaultValue.integer = intDefault; \
		} else if (param->type == 2) { \
			param->maximum.real = realMax; \
			param->minimum.real = realMin; \
			param->defaultValue.real = realDefault; \
		} \
	} while (0)

int ngsGetCompressorModule(NgsModuleDefinition **module)
{
	ngsCompressorOutputs[0].formats = ngsGetCompressorFormat();
	ngsCompressorOutputs[0].name = ngsModuleOutputName;
	ngsCompressorInputs[0].formats = ngsCompressorOutputs[0].formats;
	ngsCompressorInputs[0].name = ngsModuleInputName;
	ngsCompressorInputs[1].formats = ngsCompressorOutputs[0].formats;
	ngsCompressorInputs[1].name = ngsModuleSideChainName;
	ngsCompressorInputs[0].supportedFormatCount = 1;
	ngsCompressorInputs[1].supportedFormatCount = 1;
	ngsCompressorOutputs[0].supportedFormatCount = 1;
	ngsCompressorModule.presetCount = 1;
	ngsCompressorModule.presets = ngsCompressorPresets;
	ngsCompressorModule.inputs = ngsCompressorInputs;
	ngsCompressorPresets[0].name = "-6dB 5:1 peak compression";
	ngsCompressorPresets[0].params = &ngsCompressorPresetParams.desc;
	ngsCompressorPresetParams.nStereoLink = 1;
	ngsCompressorPresetParams.nPeakMode = 1;
	ngsCompressorPresetParams.fThreshold = -6.0f;
	ngsCompressorPresetParams.desc.id = 0x01015ce1;
	ngsCompressorPresetParams.desc.size = sizeof(ngsCompressorPresetParams);
	ngsCompressorPresetParams.fAttack = 0.05f;
	ngsCompressorPresetParams.fSoftKnee = 3.0f;
	ngsCompressorPresetParams.fRatio = 0.2f;
	ngsCompressorPresetParams.fRelease = 0.2f;
	ngsCompressorPresetParams.fMakeupGain = 0.0f;
	ngsCompressorModule.inputCount = 2;
	ngsCompressorModule.paramInterfaceCount = 2;

	INIT_COMPRESSOR_PARAM(0, 0, 1, 0, 0.0f, 1.0f, 0.2f);
	INIT_COMPRESSOR_PARAM(1, -90, 0, -6, -90.0f, 0.0f, -6.0f);
	INIT_COMPRESSOR_PARAM(2, 0, 10, 0, 0.001f, 10.0f, 0.05f);
	INIT_COMPRESSOR_PARAM(3, 0, 20, 0, 0.001f, 20.0f, 0.2f);
	INIT_COMPRESSOR_PARAM(4, -90, 24, 0, -90.0f, 24.0f, 0.0f);
	INIT_COMPRESSOR_PARAM(5, 0, 1, 1, 0.0f, 1.0f, 1.0f);
	INIT_COMPRESSOR_PARAM(6, 0, 1, 1, 0.0f, 1.0f, 1.0f);
	INIT_COMPRESSOR_PARAM(7, 0, 10, 3, 0.0f, 10.0f, 3.0f);

	ngsCompressorParamInterfaces[0].params = ngsCompressorParamDescriptions;
	ngsCompressorParamInterfaces[1].params = ngsCompressorParamDescriptions;
	ngsCompressorModule.outputs = ngsCompressorOutputs;
	ngsCompressorParamInterfaces[0].paramsId = 0x01015ce1;
	ngsCompressorParamInterfaces[1].paramsId = 0x01025ce1;
	ngsCompressorParamInterfaces[0].size = 40;
	ngsCompressorParamInterfaces[1].size = 40;
	ngsCompressorParamInterfaces[0].paramCount = 8;
	ngsCompressorParamInterfaces[1].paramCount = 8;
	ngsCompressorModule.paramInterfaces = ngsCompressorParamInterfaceList;
	ngsCompressorModule.info = "name = Compressor\n"
		"version = 1.0.001\n"
		"vendor = Sony Computer Entertainment\n"
		"copyright = (C) 2009 Sony Computer Entertainment Inc. All Rights Reserved.\n"
		"category = Dynamics\n"
		"desc = Limits dynamic range of the signal by reducing the amplitude based on signal level relative to fThreshold and an envelope. Can be used as a side-chain compressor\n";
	ngsCompressorModule.outputCount = 1;
	ngsCompressorModule.version = 0x10002;
	ngsCompressorModule.getMemoryInfo = ngsGetCompressorMemoryInfo;
	ngsCompressorModule.moduleId = 0x5ce1;
	ngsCompressorModule.dspDispatchIds[0] = 0x5ce1;
	ngsCompressorModule.dspDispatchIds[1] = 0x5ce1;
	ngsCompressorModule.codecResourceId = 0;
	ngsCompressorModule.dspControl = 0;
	ngsCompressorModule.flags = 0;
	*module = &ngsCompressorModule;
	return 0;
}
