/* The SYNTH rack: the SAS voices (as_synth.c). */
#ifndef SCE_SAS_AS_SYNTH_H
#define SCE_SAS_AS_SYNTH_H

#include <stddef.h>

#include "as_rack.h"
#include "as_task.h"

/* a voice's parameter blocks (high half; the voice in the low half) */
#define SCE_SAS_AS_SYNTH_BLOCK_SOURCES		0
#define SCE_SAS_AS_SYNTH_BLOCK_PITCH		1
#define SCE_SAS_AS_SYNTH_BLOCK_ENVELOPE		2
#define SCE_SAS_AS_SYNTH_BLOCK_ADSR		3
#define SCE_SAS_AS_SYNTH_BLOCK_DISTORTION	4
#define SCE_SAS_AS_SYNTH_BLOCK_FILTERS		5
#define SCE_SAS_AS_SYNTH_BLOCK_VOLUMES		6
#define SCE_SAS_AS_SYNTH_BLOCK_SENDS		7

/* the bits a voice sets in updated: an event, or a block unlocked */
#define SCE_SAS_AS_SYNTH_UPDATED_EVENTS		0x1
#define SCE_SAS_AS_SYNTH_UPDATED_SOURCES	0x2	/* taken at the next key on */
#define SCE_SAS_AS_SYNTH_UPDATED_PITCH		0x4
#define SCE_SAS_AS_SYNTH_UPDATED_ADSR		0x8
#define SCE_SAS_AS_SYNTH_UPDATED_ENVELOPE_ON	0x10	/* taken at the next key on */
#define SCE_SAS_AS_SYNTH_UPDATED_ENVELOPE_OFF	0x20	/* taken at the next key off */
#define SCE_SAS_AS_SYNTH_UPDATED_DISTORTION	0x40
#define SCE_SAS_AS_SYNTH_UPDATED_FILTERS	0x80
#define SCE_SAS_AS_SYNTH_UPDATED_VOLUMES	0x100
#define SCE_SAS_AS_SYNTH_UPDATED_SENDS		0x200
/* the bits an update takes now; sources and envelopes wait for their key event */
#define SCE_SAS_AS_SYNTH_UPDATED_NOW		0x3cc

/* the synth's event (high half; the voice in the low half), its values, and its states */
#define SCE_SAS_AS_SYNTH_EVENT			0x9001
#define SCE_SAS_AS_SYNTH_EVENT_KEY_ON		1
#define SCE_SAS_AS_SYNTH_EVENT_KEY_OFF		2
#define SCE_SAS_AS_SYNTH_EVENT_STOP		3
#define SCE_SAS_AS_SYNTH_EVENT_PAUSE		4
#define SCE_SAS_AS_SYNTH_EVENT_RESUME		5
#define SCE_SAS_AS_SYNTH_EVENT_RELOAD		8

#define SCE_SAS_AS_SYNTH_STATE_VOICE		0x3000
#define SCE_SAS_AS_SYNTH_STATE_POSITION		0x3001
#define SCE_SAS_AS_SYNTH_STATE_ENVELOPE		0x3002

/* a filter whose five coefficients are given as they are (4.12); the others are worked out */
#define SCE_SAS_AS_SYNTH_FILTER_COEFFICIENTS	11

/* a voice's event flags */
#define SCE_SAS_AS_SYNTH_KEY_ON			0x2
#define SCE_SAS_AS_SYNTH_KEY_OFF		0x4
#define SCE_SAS_AS_SYNTH_STOP			0x8
#define SCE_SAS_AS_SYNTH_PAUSE			0x10
#define SCE_SAS_AS_SYNTH_RELOAD			0x100

/* a voice's source: what it plays */
typedef struct SceSasAsSynthSource {
	SceUInt8 type;
	SceUInt8 reserved0;
	SceUInt8 next;				/* 0xff: none */
	SceUInt8 reserved1;
	SceUInt32 data[4];
} SceSasAsSynthSource;

/* the ADSR envelope: the four phases' rates and curves, and the sustain level */
typedef struct SceSasAsSynthAdsrModes {
	SceUInt8 attack;
	SceUInt8 decay;
	SceUInt8 sustain;
	SceUInt8 release;
} SceSasAsSynthAdsrModes;

typedef struct SceSasAsSynthAdsr {
	SceUInt8 enable;
	SceUInt8 reserved[3];
	SceSasAsSynthAdsrModes modes;
	SceInt32 attackRate;
	SceInt32 decayRate;
	SceInt32 sustainLevel;
	SceInt32 sustainRate;
	SceInt32 releaseRate;
} SceSasAsSynthAdsr;

typedef struct SceSasAsSynthSources {
	SceUInt8 numSources;
	SceSasAsSynthSource *sources;
} SceSasAsSynthSources;

/* an envelope of points: the key-on ones, then the key-off ones */
typedef struct SceSasAsSynthEnvelopePoint {
	SceUInt32 time;
	float level;				/* 0..1 */
} SceSasAsSynthEnvelopePoint;

typedef struct SceSasAsSynthEnvelope {
	SceUInt8 reserved;
	SceUInt8 numOnPoints;
	SceUInt8 numOffPoints;
	SceUInt8 numPoints;
	SceSasAsSynthEnvelopePoint *points;
} SceSasAsSynthEnvelope;

typedef struct SceSasAsSynthDistortion {
	SceUInt8 enable;
	float gain;
	float level;
} SceSasAsSynthDistortion;

typedef struct SceSasAsSynthFilter {
	SceUInt8 type;
	SceUInt8 mode;
	float coefficients[5];
} SceSasAsSynthFilter;

typedef struct SceSasAsSynthFilters {
	SceUInt32 mask;				/* the filters in use */
	SceUInt32 reserved;
	SceUInt8 numFilters;
	SceSasAsSynthFilter *filters;
} SceSasAsSynthFilters;

typedef struct SceSasAsSynthVolumes {
	SceUInt8 numVolumes;
	SceUInt8 numChannels;
	float *levels;				/* numVolumes * numChannels */
} SceSasAsSynthVolumes;

typedef struct SceSasAsSynthSend {
	SceUInt8 buss;
	SceUInt8 volume;
	float level;
} SceSasAsSynthSend;

typedef struct SceSasAsSynthSends {
	SceUInt32 mask;				/* the sends in use */
	SceUInt8 numSends;
	SceSasAsSynthSend *sends;
} SceSasAsSynthSends;

/* a voice's parameters as the API sets them, block by block */
typedef struct SceSasAsSynthVoice {
	volatile SceUInt32 lock;		/* one bit a block */
	volatile SceUInt32 updated;
	SceUInt16 events;
	SceUInt16 keyOnCount;
	SceUInt32 reserved;
	SceSasAsLockedBlock sourcesLocked;
	SceSasAsSynthSources sources;
	SceSasAsLockedBlock pitchLocked;
	float pitch;				/* 0..4 */
	SceSasAsLockedBlock adsrLocked;
	SceSasAsSynthAdsr adsr;
	SceSasAsLockedBlock envelopeLocked;
	SceSasAsSynthEnvelope envelope;
	SceSasAsLockedBlock distortionLocked;
	SceSasAsSynthDistortion distortion;
	SceSasAsLockedBlock volumesLocked;
	SceSasAsSynthVolumes volumes;
	SceSasAsLockedBlock filtersLocked;
	SceSasAsSynthFilters filters;
	SceSasAsLockedBlock sendsLocked;
	SceSasAsSynthSends sends;
} SceSasAsSynthVoice;

/* a voice's parameters as its task reads them, in fixed point, arrays by offset */
typedef struct SceSasAsSynthVoiceParam {
	SceUInt16 updated;
	SceUInt16 events;
	SceUInt16 keyOnCount;
	SceInt32 pitch;				/* 2.14 */
	SceInt32 attackRate;
	SceInt32 decayRate;
	SceInt32 sustainRate;
	SceInt32 releaseRate;
	SceInt32 sustainLevel;
	SceSasAsSynthAdsrModes adsrModes;
	SceUInt8 adsrEnable;
	SceUInt8 numSources;
	SceUInt8 numEnvelopePoints;
	SceUInt8 distortionEnable;
	SceInt16 distortionGain;		/* 6.10 */
	SceInt16 distortionLevel;		/* 4.12 */
	SceUInt32 filterMask;
	SceUInt32 sendMask;
	SceUInt8 maxSources;
	SceUInt8 numVolumes;
	SceUInt8 numFilters;
	SceUInt8 numSends;
	SceSasAsSynthSource *sources;
	SceInt16 *envelopeLevels;		/* 2.14 */
	SceUInt32 *envelopeTimes;
	SceUInt8 *filterTypes;			/* type and mode, a filter */
	SceInt16 *filterCoefficients;		/* five a filter */
	SceInt16 *volumes;			/* 4.12 */
	SceUInt8 *sendBusses;			/* buss and volume, a send */
	SceInt16 *sendLevels;			/* 4.12 */
	SceUInt16 sourcesOffset;
	SceUInt16 envelopeLevelsOffset;
	SceUInt16 envelopeTimesOffset;
	SceUInt16 filterTypesOffset;
	SceUInt16 filterCoefficientsOffset;
	SceUInt16 volumesOffset;
	SceUInt16 sendBussesOffset;
	SceUInt16 sendLevelsOffset;
} SceSasAsSynthVoiceParam;

/* what a voice reports after a render */
typedef struct SceSasAsSynthVoiceStatus {
	SceUInt16 state;			/* 0: nothing to report */
	SceUInt16 events;
	SceUInt32 position;
	SceUInt8 source;
	SceUInt16 keyOnCount;
	SceUInt16 envelopeLeft;			/* 1.15 */
	SceUInt16 envelopeRight;
	SceUInt64 time;
} SceSasAsSynthVoiceStatus;

/* what getState gives for SCE_SAS_AS_SYNTH_STATE_VOICE (state and events) and _POSITION */
typedef struct SceSasAsSynthVoiceState {
	SceUInt16 state;
	SceUInt16 events;
	SceUInt32 source;
	SceUInt32 position;
	SceUInt64 time;
} SceSasAsSynthVoiceState;

/* the voices' work areas (the task's, between renders): a fixed part, then its arrays */
typedef struct SceSasAsSynthVoiceWork {
	SceUInt8 state[0xac];
	SceUInt16 sourceStateOffset;		/* 10 bytes */
	SceUInt16 envelopeStateOffset;		/* 8 bytes */
	SceUInt16 volumesOffset;		/* the volumes it last ramped to */
	SceUInt16 sendLevelsOffset;		/* the send levels it last ramped to */
	SceUInt8 reserved[4];
} SceSasAsSynthVoiceWork;

/* the task's first buffer: where the voices are */
typedef struct SceSasAsSynthTaskInfo {
	SceUInt32 numVoices;
	SceUInt32 numSends;
	SceUInt32 numFilters;
	SceUInt32 numVolumes;
	SceSasAsSynthVoiceParam *params;
	SceSize paramStride;
	SceSasAsSynthVoiceWork *works;
	SceSize workStride;
	SceSasAsSynthVoiceStatus *status;	/* the render's */
} SceSasAsSynthTaskInfo;

typedef struct SceSasAsSynthRack {
	SceSasAsHandle handle;
	SceUInt32 numProcessors;
	SceUInt32 numChannels;
	SceUInt32 numVoices;
	SceUInt32 numVoiceWords;		/* the bitmaps' words */
	volatile SceUInt32 *updatedVoices;	/* a bit a voice unlocked or given an event */
	SceSasAsSynthVoice *voices;
	SceSize voiceStride;			/* the voices' allocations, a voice */
	SceUInt32 *changedVoices;		/* a bit a voice the last update changed */
	SceUInt8 numSources;
	SceUInt8 numVolumes;
	SceUInt8 numFilters;
	SceUInt8 numSends;
	SceSasAsSynthVoiceParam *params;
	SceSize paramStride;
	SceSasAsSynthVoiceWork *works;
	SceSize workStride;
	SceSasAsSynthVoiceStatus *status[2];	/* the render's and the one before */
	SceUInt32 statusIndex;
	SceSasAsSynthTaskInfo *info;
	volatile SceUInt32 lock;		/* bit 0 guards the status */
	SceSasAsTask task;
	SceUInt8 reserved[0x400];
} SceSasAsSynthRack;

_Static_assert(sizeof(SceSasAsSynthVoice) == 0xb0, "synth voice size");
_Static_assert(offsetof(SceSasAsSynthVoice, adsr) == 0x34, "synth voice adsr");
_Static_assert(offsetof(SceSasAsSynthVoice, adsr.modes) == 0x38, "synth voice adsr modes");
_Static_assert(offsetof(SceSasAsSynthVoice, envelope) == 0x58, "synth voice envelope");
_Static_assert(offsetof(SceSasAsSynthVoice, volumes) == 0x7c, "synth voice volumes");
_Static_assert(offsetof(SceSasAsSynthVoice, filters) == 0x8c, "synth voice filters");
_Static_assert(offsetof(SceSasAsSynthVoice, sends) == 0xa4, "synth voice sends");
_Static_assert(sizeof(SceSasAsSynthVoiceParam) == 0x68, "synth voice param size");
_Static_assert(offsetof(SceSasAsSynthVoiceParam, adsrModes) == 0x20, "synth voice param adsr modes");
_Static_assert(offsetof(SceSasAsSynthVoiceParam, maxSources) == 0x34, "synth voice param counts");
_Static_assert(offsetof(SceSasAsSynthVoiceParam, sources) == 0x38, "synth voice param arrays");
_Static_assert(sizeof(SceSasAsSynthVoiceState) == 0x18, "synth voice state size");
_Static_assert(sizeof(SceSasAsSynthVoiceStatus) == 0x18, "synth voice status size");
_Static_assert(sizeof(SceSasAsSynthVoiceWork) == 0xb8, "synth voice work size");
_Static_assert(offsetof(SceSasAsSynthRack, numProcessors) == 0x58, "synth rack counts");
_Static_assert(offsetof(SceSasAsSynthRack, statusIndex) == 0x94, "synth rack status index");
_Static_assert(offsetof(SceSasAsSynthRack, task) == 0xa0, "synth rack task");
_Static_assert(sizeof(SceSasAsSynthRack) == 0x510, "synth rack size");

#endif
