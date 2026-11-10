/* The REVERB rack: the SAS effect (as_reverb.c). */
#ifndef SCE_SAS_AS_REVERB_H
#define SCE_SAS_AS_REVERB_H

#include "as_rack.h"
#include "as_task.h"

/* its parameter block, and the bit each block sets in updated */
#define SCE_SAS_AS_REVERB_BLOCK_EFFECT		0

#define SCE_SAS_AS_REVERB_UPDATED_INPUTS	0x1
#define SCE_SAS_AS_REVERB_UPDATED_OUTPUTS	0x2
#define SCE_SAS_AS_REVERB_UPDATED_EFFECT	0x4

/* the effect off (SCE_SAS_FX_TYPE_OFF as a byte) */
#define SCE_SAS_AS_REVERB_TYPE_OFF		0xff

/* the work area the effect keeps between renders */
#define SCE_SAS_AS_REVERB_WORK_SIZE		0xacb0

/* what the API sets (sceSasSetEffectType, sceSasSetEffectParam, sceSasSetEffectVolume) */
typedef struct SceSasAsReverbRackEffect {
	SceInt32 type;				/* SCE_SAS_FX_TYPE_* */
	float feedback;				/* 0..1 */
	float delay;				/* 0..1 */
	float levels[4];			/* the volume's two last: left and right */
} SceSasAsReverbRackEffect;

/* what the task reads: the effect in fixed point, then the port table */
typedef struct SceSasAsReverbRackState {
	SceUInt8 type;
	SceUInt8 feedback;			/* 0..128 */
	SceUInt8 delay;				/* 0..128 */
	SceInt16 levels[4];			/* 4.12 */
	SceSasAsRackIoTable io;
} SceSasAsReverbRackState;

/* what the task keeps between renders: the type it last ran, the port levels, the effect's memory */
typedef struct SceSasAsReverbRackWork {
	SceUInt8 type;
	SceUInt8 data0[0xb];
	SceSasAsRackIoTask io;
	SceUInt8 data[SCE_SAS_AS_REVERB_WORK_SIZE - 0x1c];
} SceSasAsReverbRackWork;

typedef struct SceSasAsReverbRack {
	SceSasAsHandle handle;
	volatile SceUInt32 lock;		/* one bit a block */
	volatile SceUInt32 updated;		/* the blocks unlocked since the last update */
	SceSasAsRackIoParam io;
	SceSasAsLockedBlock effectLocked;
	SceSasAsReverbRackEffect effect;
	SceSasAsReverbRackState *state;
	SceSasAsReverbRackWork *work;
	SceSasAsTask task;
	SceUInt8 reserved[0x400];
} SceSasAsReverbRack;

#endif
