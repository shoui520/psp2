/* The REVERB rack: the SAS effect (as_reverb.c). */
#ifndef SCE_SAS_AS_REVERB_H
#define SCE_SAS_AS_REVERB_H

#include <stddef.h>
#include <sas.h>

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

/* an effect type's parameters (the presets of SCE_SAS_FX_TYPE_ROOM to _PIPE) */
typedef struct SceSasAsReverbPreset {
	SceUInt16 size;				/* the reflection ring's entries, a power of two */
	SceUInt16 echoDelay;			/* the echo's and the delay's taps, from their delay */
	SceUInt16 apf2Delay;			/* the second all-pass filter's samples */
	SceUInt16 iirLevel;
	SceUInt16 combLevel[4];
	SceUInt16 wallLevel;
	SceUInt16 apf1Level;
	SceUInt16 apf2Level;
	SceUInt16 leftSame;			/* the ring's taps */
	SceUInt16 rightSame;
	SceUInt16 leftComb1;
	SceUInt16 rightComb1;
	SceUInt16 leftComb2;
	SceUInt16 rightComb2;
	SceUInt16 leftSameDelay;
	SceUInt16 rightSameDelay;
	SceUInt16 leftDiff;
	SceUInt16 rightDiff;
	SceUInt16 leftComb3;
	SceUInt16 rightComb3;
	SceUInt16 leftComb4;
	SceUInt16 rightComb4;
	SceUInt16 leftDiffDelay;
	SceUInt16 rightDiffDelay;
	SceUInt16 apf1LeftDelay;		/* the first all-pass filter's samples, left and right */
	SceUInt16 apf1RightDelay;
	SceUInt16 echoApfDelay;			/* the echo's and the delay's all-pass filters */
} SceSasAsReverbPreset;

/*
 * The reflection ring keeps four samples a position: the left same-side and different-side
 * reflections, then the right different-side and same-side ones.
 */
#define SCE_SAS_AS_REVERB_RING_SIZE \
	((SCE_SAS_AS_REVERB_WORK_SIZE - 0x70) / (4 * sizeof(SceInt16)))

/* what the task keeps between renders: the effect it last set up, the port levels, its memory */
typedef struct SceSasAsReverbRackWork {
	SceUInt8 type;
	SceUInt8 feedback;
	SceUInt8 delay;
	SceInt16 levels[4];
	SceSasAsRackIoTask io;
	SceInt32 lastLeft;			/* the last outputs, to interpolate from */
	SceInt32 lastRight;
	SceInt16 *apf1Left;			/* the all-pass filters' lines, after the ring */
	SceInt16 *apf1Right;
	SceInt16 *apf2;				/* left and right interleaved */
	SceUInt16 position;			/* the ring's */
	SceUInt16 mask;
	SceUInt16 apf1LeftPosition;
	SceUInt16 apf1RightPosition;
	SceUInt16 apf2Position;
	SceUInt16 feedbackTap[4];		/* each reflection's taps back from position */
	SceUInt16 apf1LeftLength;
	SceUInt16 apf1RightLength;
	SceUInt16 apf2Length;
	SceInt16 apf1Level;
	SceInt16 apf2Level;
	SceInt16 iirLevel;
	SceInt16 combLevel[4];
	SceInt16 wallLevel;
	SceUInt16 combTap[2][4];
	SceUInt16 reflectionTap[4];
	SceInt16 ring[SCE_SAS_AS_REVERB_RING_SIZE][4];
} SceSasAsReverbRackWork;

_Static_assert(offsetof(SceSasAsReverbRackWork, lastLeft) == 0x1c, "reverb work last outputs");
_Static_assert(offsetof(SceSasAsReverbRackWork, position) == 0x30, "reverb work positions");
_Static_assert(offsetof(SceSasAsReverbRackWork, combTap) == 0x58, "reverb work taps");
_Static_assert(offsetof(SceSasAsReverbRackWork, ring) == 0x70, "reverb work ring");
_Static_assert(sizeof(SceSasAsReverbRackWork) == SCE_SAS_AS_REVERB_WORK_SIZE, "reverb work size");

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
