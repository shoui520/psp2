/* The RACK classes: the synth, the master, the reverb and the mixer. */
#ifndef SCE_SAS_AS_RACK_H
#define SCE_SAS_AS_RACK_H

#include "as.h"

/* the parameter blocks of a rack's inputs and outputs (high half 0x81xx: a rack's own block) */
#define SCE_SAS_AS_RACK_BLOCK_INPUTS	0x81010000
#define SCE_SAS_AS_RACK_BLOCK_OUTPUTS	0x81020000

/* one input or output of a rack: the buss it reads or writes, and its level */
typedef struct SceSasAsRackPort {
	SceUInt8 buss;
	SceUInt8 mute;
	SceUInt8 flag;			/* outputs only: collected into the table's outputFlags */
	SceUInt8 reserved;
	float level;
} SceSasAsRackPort;

/* the ports in use (bit i: port i) */
typedef struct SceSasAsRackPorts {
	SceUInt32 mask;
	SceUInt8 count;
	SceSasAsRackPort *ports;
} SceSasAsRackPorts;

/* what the API locks and sets: the inputs and the outputs, each its own block */
typedef struct SceSasAsRackIoParam {
	SceUInt8 numChannels;
	SceUInt8 numInputs;
	SceUInt8 numOutputs;
	SceSasAsLockedBlock inputsLocked;
	SceSasAsRackPorts inputs;
	SceSasAsLockedBlock outputsLocked;
	SceSasAsRackPorts outputs;
} SceSasAsRackIoParam;

/*
 * The ports as the rack's tasks read them: per port its buss, mute and level (4.12 fixed),
 * by pointer and again as offsets from the offset's own place, which hold where the table
 * is copied.
 */
typedef struct SceSasAsRackIoTable {
	SceUInt8 numChannels;
	SceUInt8 numInputs;
	SceUInt8 numOutputs;
	SceUInt32 inputMask;
	SceUInt32 outputMask;
	SceUInt32 outputFlags;
	SceUInt8 *inputBuss;
	SceUInt8 *inputMute;
	SceInt16 *inputLevel;
	SceUInt8 *outputBuss;
	SceUInt8 *outputMute;
	SceInt16 *outputLevel;
	SceUInt16 inputBussOffset;
	SceUInt16 inputMuteOffset;
	SceUInt16 inputLevelOffset;
	SceUInt16 outputBussOffset;
	SceUInt16 outputMuteOffset;
	SceUInt16 outputLevelOffset;
} SceSasAsRackIoTable;

/* the same arrays by offset only */
typedef struct SceSasAsRackIoTask {
	SceUInt8 numChannels;
	SceUInt8 numInputs;
	SceUInt8 numOutputs;
	SceUInt16 inputBussOffset;
	SceUInt16 inputMuteOffset;
	SceUInt16 inputLevelOffset;
	SceUInt16 outputBussOffset;
	SceUInt16 outputMuteOffset;
	SceUInt16 outputLevelOffset;
} SceSasAsRackIoTask;

/* the offset from where it is kept to what it points to, and back */
#define SCE_SAS_AS_RELATIVE(field, pointer) \
	((SceUInt16)((SceUIntPtr)(pointer) - (SceUIntPtr)&(field)))
#define SCE_SAS_AS_AT(field) \
	((void *)((SceUIntPtr)&(field) + (field)))

/*
 * One channel into a buffer at a level (4.12), replacing what the buffer held or adding to it;
 * a level that changed since the last render (delta: their difference, 16.16) ramps over the
 * render. The racks' tasks mix their inputs, matrices and outputs with it.
 */
static inline void sceSasAsRackMix(SceInt32 *buffer, const SceInt32 *input, SceUInt32 granularity,
				   SceUInt32 add, SceInt32 from, SceInt32 to, SceInt32 delta)
{
	SceInt32 level, step;
	SceUInt32 i;

	if (add) {
		if (to != from) {
			step = delta / (SceInt32)granularity;
			level = from << 16;
			for (i = 0; i < granularity; i++) {
				buffer[i] += (input[i] * (level >> 16)) >> 12;
				level += step;
			}
		} else if (to != 0) {
			if (to == 0x1000) {
				for (i = 0; i < granularity; i++)
					buffer[i] += input[i];
			} else {
				for (i = 0; i < granularity; i++)
					buffer[i] += (SceInt32)(((SceInt64)input[i] * to) >> 12);
			}
		}
	} else {
		if (to != from) {
			step = delta / (SceInt32)granularity;
			level = from << 16;
			for (i = 0; i < granularity; i++) {
				buffer[i] = (SceInt32)(((SceInt64)input[i] * (level >> 16)) >> 12);
				level += step;
			}
		} else if (to == 0) {
			for (i = 0; i < granularity; i++)
				buffer[i] = 0;
		} else if (to == 0x1000) {
			if (buffer != input) {
				for (i = 0; i < granularity; i++)
					buffer[i] = input[i];
			}
		} else {
			for (i = 0; i < granularity; i++)
				buffer[i] = (SceInt32)(((SceInt64)input[i] * to) >> 12);
		}
	}
}

int sceSasAsRackIoParamCreate(SceSasAsAllocator *allocator, SceSasAsConfigEntry **config,
			      SceSasAsHandle *handle, SceSize offset);
int sceSasAsRackIoTableCreate(SceSasAsAllocator *allocator, SceSasAsConfigEntry **config,
			      void *base, SceSize offset);
int sceSasAsRackIoTaskCreate(SceSasAsAllocator *allocator, SceSasAsConfigEntry **config,
			     void *base, SceSize offset);
int sceSasAsRackIoTableSetInputs(const SceSasAsRackIoParam *param, SceSasAsRackIoTable *table);
int sceSasAsRackIoTableSetOutputs(const SceSasAsRackIoParam *param, SceSasAsRackIoTable *table);

extern const SceSasAsClass sceSasAsSynthRackClass;
extern const SceSasAsClass sceSasAsMasterRackClass;
extern const SceSasAsClass sceSasAsReverbRackClass;
extern const SceSasAsClass sceSasAsMixerRackClass;

#endif
