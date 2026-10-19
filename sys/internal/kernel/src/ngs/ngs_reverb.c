/* Built-in reverb presets and parameter interfaces. */

#include "ngs_module_memory.h"

typedef struct SceNgsReverbParams {
	SceNgsParamsDescriptor desc;
	float fRoom;
	float fRoomHF;
	float fDecayTime;
	float fDecayHFRatio;
	float fReflections;
	float fReflectionsDelay;
	float fReverb;
	float fReverbDelay;
	float fDiffusion;
	float fDensity;
	float fHFReference;
	int eEarlyReflectionPattern[2];
	float fEarlyReflectionScalar;
	float fLFReference;
	float fRoomLF;
	float fDryMB;
} SceNgsReverbParams;

extern const char *ngsModuleInputName;

const char *const ngsReverbPresetNames[31] = {
	0,
	"default",
	"generic",
	"padded cell",
	"preset room",
	"bathroom",
	"living room",
	"stone room",
	"auditorium",
	"concert hall",
	"cave",
	"arena",
	"hangar",
	"carpeted hallway",
	"hallway",
	"stone corridor",
	"alley",
	"forest",
	"city",
	"mountains",
	"quarry",
	"plain",
	"parking lot",
	"sewer pipe",
	"underwater",
	"small room",
	"medium room",
	"large room",
	"medium hall",
	"large hall",
	"plate",
};

const SceNgsReverbParams ngsReverbPresetParams[30] = {
	{ /* default */
		{ 0x01015ce7, 76 },
		0.0f, 0.0f, 1.0f, 0.5f, -500.0f, 0.02f,
		0.0f, 0.04f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* generic */
		{ 0x01015ce7, 76 },
		0.0f, -100.0f, 1.49f, 0.83f, -2602.0f, 0.007f,
		200.0f, 0.011f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* padded cell */
		{ 0x01015ce7, 76 },
		0.0f, -6000.0f, 0.17f, 0.1f, -1204.0f, 0.001f,
		207.0f, 0.002f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* preset room */
		{ 0x01015ce7, 76 },
		0.0f, -454.0f, 0.4f, 0.83f, -1646.0f, 0.002f,
		53.0f, 0.003f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* bathroom */
		{ 0x01015ce7, 76 },
		0.0f, -1200.0f, 1.49f, 0.54f, -370.0f, 0.007f,
		1030.0f, 0.011f, 100.0f, 60.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* living room */
		{ 0x01015ce7, 76 },
		0.0f, -6000.0f, 0.5f, 0.1f, -1376.0f, 0.003f,
		-1104.0f, 0.004f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* stone room */
		{ 0x01015ce7, 76 },
		0.0f, -300.0f, 2.31f, 0.64f, -711.0f, 0.012f,
		83.0f, 0.017f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* auditorium */
		{ 0x01015ce7, 76 },
		0.0f, -476.0f, 4.32f, 0.59f, -789.0f, 0.02f,
		-289.0f, 0.03f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* concert hall */
		{ 0x01015ce7, 76 },
		0.0f, -500.0f, 3.92f, 0.7f, -1230.0f, 0.02f,
		-2.0f, 0.029f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* cave */
		{ 0x01015ce7, 76 },
		0.0f, 0.0f, 2.91f, 1.3f, -602.0f, 0.015f,
		-302.0f, 0.022f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* arena */
		{ 0x01015ce7, 76 },
		0.0f, -698.0f, 7.24f, 0.33f, -1166.0f, 0.02f,
		16.0f, 0.03f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* hangar */
		{ 0x01015ce7, 76 },
		0.0f, -1000.0f, 10.05f, 0.23f, -602.0f, 0.02f,
		198.0f, 0.03f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* carpeted hallway */
		{ 0x01015ce7, 76 },
		0.0f, -4000.0f, 0.3f, 0.1f, -1831.0f, 0.002f,
		-1630.0f, 0.03f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* hallway */
		{ 0x01015ce7, 76 },
		-1000.0f, -300.0f, 1.49f, 0.59f, -1219.0f, 0.007f,
		411.0f, 0.011f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* stone corridor */
		{ 0x01015ce7, 76 },
		-1000.0f, -237.0f, 2.7f, 0.79f, -1214.0f, 0.013f,
		395.0f, 0.02f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* alley */
		{ 0x01015ce7, 76 },
		-1000.0f, -270.0f, 1.49f, 0.86f, -1204.0f, 0.007f,
		-4.0f, 0.011f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* forest */
		{ 0x01015ce7, 76 },
		-1000.0f, -3300.0f, 1.49f, 0.54f, -2560.0f, 0.162f,
		-613.0f, 0.088f, 79.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* city */
		{ 0x01015ce7, 76 },
		-1000.0f, -800.0f, 1.49f, 0.67f, -2273.0f, 0.007f,
		-2217.0f, 0.011f, 50.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* mountains */
		{ 0x01015ce7, 76 },
		-1000.0f, -2500.0f, 1.49f, 0.21f, -2780.0f, 0.3f,
		-2014.0f, 0.1f, 27.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* quarry */
		{ 0x01015ce7, 76 },
		-1000.0f, -1000.0f, 1.49f, 0.83f, -10000.0f, 0.061f,
		500.0f, 0.025f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* plain */
		{ 0x01015ce7, 76 },
		-1000.0f, -2000.0f, 1.49f, 0.5f, -2466.0f, 0.179f,
		-2514.0f, 0.1f, 21.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* parking lot */
		{ 0x01015ce7, 76 },
		-1000.0f, 0.0f, 1.65f, 1.5f, -1363.0f, 0.008f,
		-1153.0f, 0.012f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* sewer pipe */
		{ 0x01015ce7, 76 },
		-1000.0f, -1000.0f, 2.81f, 0.14f, 429.0f, 0.014f,
		648.0f, 0.021f, 80.0f, 60.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* underwater */
		{ 0x01015ce7, 76 },
		-1000.0f, -4000.0f, 1.49f, 0.1f, -449.0f, 0.007f,
		1700.0f, 0.011f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* small room */
		{ 0x01015ce7, 76 },
		-1000.0f, -600.0f, 1.1f, 0.83f, -400.0f, 0.005f,
		500.0f, 0.01f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* medium room */
		{ 0x01015ce7, 76 },
		-1000.0f, -600.0f, 1.3f, 0.83f, -1000.0f, 0.01f,
		-200.0f, 0.02f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* large room */
		{ 0x01015ce7, 76 },
		-1000.0f, -600.0f, 1.5f, 0.83f, -1600.0f, 0.02f,
		-1000.0f, 0.04f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* medium hall */
		{ 0x01015ce7, 76 },
		-1000.0f, -600.0f, 1.8f, 0.7f, -1300.0f, 0.015f,
		-800.0f, 0.03f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* large hall */
		{ 0x01015ce7, 76 },
		-1000.0f, -600.0f, 1.8f, 0.7f, -2000.0f, 0.03f,
		-1400.0f, 0.06f, 100.0f, 100.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
	{ /* plate */
		{ 0x01015ce7, 76 },
		-1000.0f, -200.0f, 1.3f, 0.9f, 0.0f, 0.002f,
		0.0f, 0.01f, 100.0f, 75.0f, 5000.0f, { 0, 1 },
		100.0f, 80.0f, 0.0f, -10000.0f,
	},
};

const float ngsReverbMinimumValues[17] = {
	-10000.0f, -10000.0f, 0.1f, 0.1f, -10000.0f, 0.0f,
	-10000.0f, 0.0f, 0.0f, 0.0f, 20.0f, 0.0f,
	0.0f, 0.0f, 20.0f, -10000.0f, -10000.0f
};

const float ngsReverbMaximumValues[17] = {
	0.0f, 0.0f, 20.0f, 2.0f, 1000.0f, 0.3f,
	2000.0f, 0.1f, 100.0f, 100.0f, 20000.0f, 5.0f,
	5.0f, 100.0f, 20000.0f, 0.0f, 0.0f
};

const float ngsReverbDefaultValues[17] = {
	0.0f, -300.0f, 4.0f, 0.9f, -400.0f, 0.05f,
	-300.0f, 0.05f, 100.0f, 100.0f, 5000.0f, 0.0f,
	1.0f, 100.0f, 80.0f, 0.0f, -300.0f
};

extern struct NgsModuleParamInterface ngsReverbParamInterfaces[2];
const struct NgsModuleParamInterface *ngsReverbParamInterfaceList[2] = {
	&ngsReverbParamInterfaces[0], &ngsReverbParamInterfaces[1]
};

NgsModuleParamDescription ngsReverbParamDescriptions[17] = {
	{ "Room", "mB", "Room (main) volume control.", 2 },
	{ "Room HF", "mB", "High frequency content input at HF_reference.", 2 },
	{ "Decay Time", "Seconds", "Decay time of late reverberation.", 2 },
	{ "Decay HF Ratio", "", "Ratio of high frequency decay in late reverb.", 2 },
	{ "Reflections", "mB", "Volume of early reflections.", 2 },
	{ "Reflections Delay", "Seconds", "Delay from input signal until first reflection.", 2 },
	{ "Reverb", "mB", "Volume of late reverb in mB.", 2 },
	{ "Reverb Delay", "Seconds", "Delay from first reflection until late reverb.", 2 },
	{ "Diffusion", "Percent", "Echo density in late reverberation.", 2 },
	{ "Density", "Percent", "Modal density in late reverberation.", 2 },
	{ "HF Reference", "Hz", "Reference frequency for Room_HF and Decay_HF_ratio.", 2 },
	{ "Early Reflection Pattern Left", "", "Early reflection pattern", 1 },
	{ "Early Reflection Pattern Right", "", "Early reflection pattern", 1 },
	{ "Early Reflection Scalar", "Percent", "Scaler 0 -> 100% for early reflections. 0 == single reflection 100 == widely spread reflections.", 2 },
	{ "LF Reference", "Hz", "Reference low frequency .", 2 },
	{ "Room LF", "mB", "Low frequency content input in mB, at LF_reference.", 2 },
	{ "Dry MB", "mB", "Dry gain.", 2 },
};

NgsModuleAudioInterface ngsReverbOutputs[1];
NgsModuleAudioInterface ngsReverbInputs[1];
NgsModulePreset ngsReverbPresets[30];
struct NgsModuleParamInterface ngsReverbParamInterfaces[2];
NgsModuleDefinition ngsReverbModule;

int ngsGetReverbModule(NgsModuleDefinition **module)
{
	const struct NgsAudioFormat *format = ngsGetReverbFormat();
	const SceNgsReverbParams *preset = ngsReverbPresetParams;
	unsigned int index = 0;
	const float *minimum = ngsReverbMinimumValues;
	const float *maximum = ngsReverbMaximumValues;
	const float *defaultValue = ngsReverbDefaultValues;
	NgsModuleParamDescription *description = ngsReverbParamDescriptions;

	ngsReverbOutputs[0].formats = format;
	ngsReverbOutputs[0].name = ngsModuleInputName;
	ngsReverbOutputs[0].supportedFormatCount = 1;
	ngsReverbInputs[0].supportedFormatCount = 1;
	ngsReverbInputs[0].formats = format;
	do {
		ngsReverbPresets[index].params = &preset->desc;
		ngsReverbPresets[index].name = ngsReverbPresetNames[index + 1];
		++preset;
		++index;
	} while (preset != ngsReverbPresetParams + 30);
	ngsReverbModule.inputCount = 1;
	ngsReverbModule.inputs = ngsReverbInputs;
	ngsReverbModule.paramInterfaceCount = 2;
	do {
		if (description->type == 1) {
			description->minimum.integer = (int)*minimum;
			description->maximum.integer = (int)*maximum;
			description->defaultValue.integer = (int)*defaultValue;
		} else if (description->type == 2) {
			description->minimum.real = *minimum;
			description->maximum.real = *maximum;
			description->defaultValue.real = *defaultValue;
		}
		++minimum;
		++maximum;
		++defaultValue;
		++description;
	} while (minimum != ngsReverbMinimumValues + 17);
	ngsReverbModule.outputs = ngsReverbOutputs;
	ngsReverbParamInterfaces[0].paramsId = 0x01015ce7;
	ngsReverbParamInterfaces[1].paramsId = 0x01025ce7;
	ngsReverbParamInterfaces[0].paramCount = 17;
	ngsReverbParamInterfaces[1].paramCount = 17;
	ngsReverbParamInterfaces[0].size = 76;
	ngsReverbParamInterfaces[1].size = 76;
	ngsReverbParamInterfaces[0].params = ngsReverbParamDescriptions;
	ngsReverbParamInterfaces[1].params = ngsReverbParamDescriptions;
	ngsReverbModule.presets = ngsReverbPresets;
	ngsReverbModule.paramInterfaces = ngsReverbParamInterfaceList;
	ngsReverbModule.info = "name = Reverb\n"
		"version = 1.0.001\n"
		"vendor = Sony Computer Entertainment\n"
		"copyright = (C) 2009 Sony Computer Entertainment Inc. All Rights Reserved.\n"
		"category = Reverb\n"
		"desc = Applies an environmental reverberation to the sound\n";
	ngsReverbModule.outputCount = 1;
	ngsReverbModule.version = 0x10002;
	ngsReverbModule.getMemoryInfo = ngsGetReverbMemoryInfo;
	ngsReverbModule.moduleId = 0x5ce7;
	ngsReverbModule.dspDispatchIds[0] = 0x5ce7;
	ngsReverbModule.dspDispatchIds[1] = 0x5ce7;
	ngsReverbModule.codecResourceId = 0x5ce7;
	ngsReverbModule.presetCount = 30;
	ngsReverbModule.dspControl = 3;
	ngsReverbModule.flags = 0;
	*module = &ngsReverbModule;
	return 0;
}

