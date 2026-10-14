#ifndef NGS_MODULE_MEMORY_H
#define NGS_MODULE_MEMORY_H

#include <psp2kern/types.h>

typedef union NgsParamValue {
	int integer;
	unsigned int unsignedInteger;
	float real;
	signed char signedByte;
	short signedShort;
} NgsParamValue;

typedef struct NgsModuleParamDescription {
	const char *name;
	const char *units;
	const char *description;
	unsigned int type;
	NgsParamValue minimum;
	NgsParamValue maximum;
	NgsParamValue defaultValue;
} NgsModuleParamDescription;

struct NgsModuleParamInterface {
	unsigned int paramsId;
	SceSize size;
	unsigned int paramCount;
	const NgsModuleParamDescription *params;
};

typedef char NgsModuleParamDescriptionSize[(sizeof(NgsModuleParamDescription) == 28) ? 1 : -1];
typedef char NgsModuleParamInterfaceSize[(sizeof(struct NgsModuleParamInterface) == 16) ? 1 : -1];

struct NgsModuleInitParams {
	int nSampleRate;
	int nGranularity;
	int nChannelsPerVoice;
	unsigned int paramInterfaceCount;
	const struct NgsModuleParamInterface *const *paramInterfaces;
};

typedef char NgsModuleInitParamsSize[(sizeof(struct NgsModuleInitParams) == 20) ? 1 : -1];

typedef struct NgsModuleMemoryInfo {
	SceSize stateSize;
	SceSize sharedWorkSize;
	SceSize contextSize;
	SceSize paramsSize;
	SceSize extraMemorySize;
} NgsModuleMemoryInfo;

typedef char NgsModuleMemoryInfoSize[(sizeof(NgsModuleMemoryInfo) == 20) ? 1 : -1];

struct NgsAudioFormat;

typedef struct NgsModuleAudioInterface {
	unsigned int supportedFormatCount;
	const struct NgsAudioFormat *formats;
	const char *name;
} NgsModuleAudioInterface;

typedef struct SceNgsParamsDescriptor {
	unsigned int id;
	unsigned int size;
} SceNgsParamsDescriptor;

typedef struct NgsModulePreset {
	const char *name;
	const SceNgsParamsDescriptor *params;
} NgsModulePreset;

typedef struct NgsModuleDefinition {
	unsigned int version;
	unsigned int moduleId;
	unsigned int inputCount;
	const struct NgsModuleAudioInterface *inputs;
	unsigned int outputCount;
	const struct NgsModuleAudioInterface *outputs;
	int (*getMemoryInfo)(const struct NgsModuleInitParams *,
			     NgsModuleMemoryInfo *);
	unsigned int dspDispatchIds[2];
	unsigned int codecResourceId;
	const char *info;
	unsigned int presetCount;
	const struct NgsModulePreset *presets;
	unsigned int paramInterfaceCount;
	const struct NgsModuleParamInterface *const *paramInterfaces;
	unsigned int dspControl;
	unsigned int flags;
} NgsModuleDefinition;

typedef char NgsModuleDefinitionSize[(sizeof(NgsModuleDefinition) == 68) ? 1 : -1];

#endif
