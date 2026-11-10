/* The MASTER rack: a compressor and the master volume on the way out (as_master.c). */
#ifndef SCE_SAS_AS_MASTER_H
#define SCE_SAS_AS_MASTER_H

#include "as_rack.h"
#include "as_task.h"

/* its parameter blocks (high half), and the bit each sets in updated */
#define SCE_SAS_AS_MASTER_BLOCK_COMPRESSOR	0
#define SCE_SAS_AS_MASTER_BLOCK_BUFFERS		1
#define SCE_SAS_AS_MASTER_BLOCK_VOLUME		2

#define SCE_SAS_AS_MASTER_UPDATED_INPUTS	0x1
#define SCE_SAS_AS_MASTER_UPDATED_COMPRESSOR	0x2
#define SCE_SAS_AS_MASTER_UPDATED_BUFFERS	0x4
#define SCE_SAS_AS_MASTER_UPDATED_VOLUME	0x8

/* the levels below which a gain is 0 */
#define SCE_SAS_AS_MASTER_MIN_DB		-96.0f

typedef struct SceSasAsMasterRackCompressor {
	SceUInt8 enable;
	SceUInt8 mode;
	float attack;
	float release;
	float threshold;			/* dB */
	float ratio;				/* 50 and above: a limiter */
	float gain;				/* dB, after the compressor */
} SceSasAsMasterRackCompressor;

/* what the task reads: the input table, then the compressor and volume in fixed point */
typedef struct SceSasAsMasterRackState {
	SceSasAsRackIoTable io;
	SceUInt8 enable;
	SceUInt8 mode;
	SceInt32 gain;				/* 2.14 */
	SceInt32 attack;			/* 2.30 per sample */
	SceInt32 release;			/* 2.30 per sample */
	SceInt32 threshold;			/* 1.15 */
	SceInt32 ratio;				/* 1 / ratio, 2.14 */
	SceUInt16 maxBuffers;
	SceUInt16 numBuffers;
	SceInt32 volume;			/* 2.14, saturated */
} SceSasAsMasterRackState;

typedef struct SceSasAsMasterRackIo {
	SceSasAsRackIoTask io;
	SceUInt32 mode;
} SceSasAsMasterRackIo;

typedef struct SceSasAsMasterRack {
	SceSasAsHandle handle;
	volatile SceUInt32 lock;		/* one bit a block */
	volatile SceUInt32 updated;		/* the blocks unlocked since the last update */
	SceSasAsRackIoParam io;
	SceSasAsLockedBlock compressorLocked;
	SceSasAsMasterRackCompressor compressor;
	SceSasAsLockedBlock buffersLocked;
	SceInt32 numBuffers;
	SceSasAsLockedBlock volumeLocked;
	float volume;				/* dB */
	SceSasAsMasterRackState *state;
	SceSasAsMasterRackIo *taskIo;
	SceUInt32 maxBuffers;
	SceSasAsTask task;
	SceUInt8 reserved[0x400];
} SceSasAsMasterRack;

#endif
