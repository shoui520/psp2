#ifndef NGS_VOICE_DEFINITION_H
#define NGS_VOICE_DEFINITION_H

#include <stdint.h>

/* "bvdf", and the key the voice definition getters encode their result with */
#define NGS_VOICE_DEFINITION_SIGNATURE	0x66647662
#define NGS_VOICE_DEFINITION_KEY	0x9e28dcce

/* The packed format descriptor is forwarded intact to the DSP. */
struct NgsAudioFormat {
	uint32_t codecFourCC;
	uint32_t sampleFormat;
	uint32_t formatParameters[2];
};

/* Offsets in a voice definition are relative to their containing object. */
typedef struct NgsVoiceModuleDefinition {
	uint32_t moduleId;
	uint32_t formatOffset;
	uint32_t inputCount;
	uint32_t outputCount;
	uint32_t inputBindingsOffset;
	uint32_t outputBindingsOffset;
} NgsVoiceModuleDefinition;

typedef struct NgsVoicePresetDefinition {
	uint32_t nameOffset;
	uint32_t nameLength;
	uint32_t paramsOffset;
	uint32_t paramsSize;
	uint32_t indicesOffset;
	uint32_t indicesCount;
} NgsVoicePresetDefinition;

struct SceNgsVoiceDefinition {
	union {
		char signature[4];
		uint32_t magic;
	};
	uint32_t version;
	uint32_t size;
	uint32_t modulesOffset;
	uint32_t moduleCount;
	uint32_t voiceOutputCount;
	uint32_t dspConfiguration;
	uint32_t reserved;
	uint32_t nameOffset;
	uint32_t nameLength;
	uint32_t descriptionOffset;
	uint32_t descriptionLength;
	uint32_t presetsOffset;
	uint32_t presetCount;
	uint32_t routingIndexCount;
	uint32_t routingIndicesOffset;
};

_Static_assert(sizeof(NgsVoiceModuleDefinition) == 24,
	       "voice module definition size");
_Static_assert(sizeof(struct NgsAudioFormat) == 16, "audio format size");
_Static_assert(sizeof(NgsVoicePresetDefinition) == 24,
	       "voice preset definition size");
_Static_assert(sizeof(struct SceNgsVoiceDefinition) == 64,
	       "voice definition size");

#endif
