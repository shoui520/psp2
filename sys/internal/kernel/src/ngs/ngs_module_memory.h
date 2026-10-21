#ifndef NGS_MODULE_MEMORY_H
#define NGS_MODULE_MEMORY_H

#include <scetypes.h>
#include <stdint.h>
#include <ngs/ngs_top.h>

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

/* ngs_memory.c */
SceSize ngsAlignMemorySize(SceSize size);
int ngsAlignSize(int size, int alignment);
void *ngsAlignVoiceDefinitionAddress(void *address, SceSize alignment);
void *ngsAllocateMemory(unsigned char **cursor, SceSize size);
void *ngsAllocateAlignedMemory(unsigned char **cursor, SceSize size);
void *ngsAllocateMemoryWithAlignment(unsigned char **cursor, SceSize size,
				     SceSize alignment);
SceSize ngsAlignAllocationSize(SceSize size, SceSize alignment);
uintptr_t ngsAlignMemoryAddress(uintptr_t address);

/* ngs_module_memory.c */
int ngsGetCompressorMemoryInfo(const struct NgsModuleInitParams *params, NgsModuleMemoryInfo *info);
const struct NgsAudioFormat *ngsGetCompressorFormat(void);
int ngsGetDistortionMemoryInfo(const struct NgsModuleInitParams *params, NgsModuleMemoryInfo *info);
const struct NgsAudioFormat *ngsGetDistortionFormat(void);
int ngsGetEnvelopeMemoryInfo(const struct NgsModuleInitParams *params, NgsModuleMemoryInfo *info);
const struct NgsAudioFormat *ngsGetEnvelopeFormat(void);
int ngsGetPauserMemoryInfo(const struct NgsModuleInitParams *params, NgsModuleMemoryInfo *info);
const struct NgsAudioFormat *ngsGetPauserFormat(void);
int ngsGetPcmVagPlayerMemoryInfo(const struct NgsModuleInitParams *params,
			       NgsModuleMemoryInfo *info);
const struct NgsAudioFormat *ngsGetPcmVagPlayerFormat(void);
int ngsGetSignalGeneratorMemoryInfo(const struct NgsModuleInitParams *params,
				  NgsModuleMemoryInfo *info);
const struct NgsAudioFormat *ngsGetSignalGeneratorFormat(void);
int ngsGetReverbMemoryInfo(const struct NgsModuleInitParams *params, NgsModuleMemoryInfo *info);
const struct NgsAudioFormat *ngsGetReverbFormat(void);
int ngsGetMixerMemoryInfo(const struct NgsModuleInitParams *params, NgsModuleMemoryInfo *info);
const struct NgsAudioFormat *ngsGetMixerFormat(void);
int ngsGetBiquadFilterMemoryInfo(const struct NgsModuleInitParams *params,
			       NgsModuleMemoryInfo *info);
const struct NgsAudioFormat *ngsGetBiquadFilterFormat(void);
int ngsGetInputMemoryInfo(const struct NgsModuleInitParams *params, NgsModuleMemoryInfo *info);
const struct NgsAudioFormat *ngsGetInputFormat(void);
int ngsGetOutputMemoryInfo(const struct NgsModuleInitParams *params, NgsModuleMemoryInfo *info);
const struct NgsAudioFormat *ngsGetOutputFormat(void);
int ngsGetAt9PlayerMemoryInfo(const struct NgsModuleInitParams *params, NgsModuleMemoryInfo *info);
const struct NgsAudioFormat *ngsGetAt9PlayerFormat(void);
int ngsGetDelayMemoryInfo(const struct NgsModuleInitParams *params, NgsModuleMemoryInfo *info);
const struct NgsAudioFormat *ngsGetDelayFormat(void);
int ngsGetPitchShiftMemoryInfo(const struct NgsModuleInitParams *params, NgsModuleMemoryInfo *info);
const struct NgsAudioFormat *ngsGetPitchShiftFormat(void);

#endif
