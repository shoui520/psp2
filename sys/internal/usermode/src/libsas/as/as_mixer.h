/* The MIXER rack: its inputs mixed into its outputs through a channel matrix (as_mixer.c). */
#ifndef SCE_SAS_AS_MIXER_H
#define SCE_SAS_AS_MIXER_H

#include "as_rack.h"
#include "as_task.h"

/* its parameter block, and the bit each block sets in updated */
#define SCE_SAS_AS_MIXER_BLOCK_MATRIX		0

#define SCE_SAS_AS_MIXER_UPDATED_INPUTS		0x1
#define SCE_SAS_AS_MIXER_UPDATED_OUTPUTS	0x2
#define SCE_SAS_AS_MIXER_UPDATED_MATRIX		0x4

/* what the API sets: the matrix (numChannels by numChannels, row by input channel) */
typedef struct SceSasAsMixerRackMatrix {
	SceUInt8 numChannels;
	SceUInt8 enable;
	float *levels;
} SceSasAsMixerRackMatrix;

/* what the task reads: the port table and the matrix in 4.12 fixed point */
typedef struct SceSasAsMixerRackState {
	SceUInt8 enable;
	SceSasAsRackIoTable io;
	SceInt16 *levels;
	SceUInt16 levelsOffset;
} SceSasAsMixerRackState;

/* what the task keeps between renders: the levels last applied, to ramp from */
typedef struct SceSasAsMixerRackIo {
	SceSasAsRackIoTask io;
	SceUInt16 levelsOffset;
} SceSasAsMixerRackIo;

typedef struct SceSasAsMixerRack {
	SceSasAsHandle handle;
	volatile SceUInt32 lock;		/* one bit a block */
	volatile SceUInt32 updated;		/* the blocks unlocked since the last update */
	SceUInt32 numChannels;
	SceSasAsRackIoParam io;
	SceSasAsLockedBlock matrixLocked;
	SceSasAsMixerRackMatrix matrix;
	SceSasAsMixerRackState *state;
	SceSasAsMixerRackIo *taskIo;
	SceSasAsTask task;
	SceUInt8 reserved[0x400];
} SceSasAsMixerRack;

#endif
