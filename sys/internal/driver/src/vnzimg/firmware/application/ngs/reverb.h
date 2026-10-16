/*
 * The reverb's engine state. The ARM allocates it in the module's work
 * memory (the settings, then the engine and the buffers Reset lays out
 * after it); the voice job (reverb.c) and the RPC 327 entry (the engine's
 * block processing, reverbengine.c) both work on it.
 */
#pragma once

#include "ngs.h"

/* the reverb is an FDN of 16 delay lines, early reflections and a late filter chain */
#define NGS_REVERB_LINES		16
#define NGS_REVERB_PATHS		8
#define NGS_REVERB_TAPS			8
#define NGS_REVERB_ALLPASSES		4
#define NGS_REVERB_ALLPASS_SIZE		605
/* the FDN works on 128 samples at a time */
#define NGS_REVERB_CHUNK		128
/* each path's ring of 16-bit samples (the input, for the pre-delay and the taps) */
#define NGS_REVERB_RING			14912
/* the part of the ring the early taps and the late input read from: 7200 + 512 samples */
#define NGS_REVERB_HISTORY		7712
#define NGS_REVERB_MAX_DELAY		7200

/* SceNgsReverbParams, version 1 of the structure: no DC blocker on the output */
#define NGS_REVERB_PARAMS_ID_V1		0x01015ce7

/* a Schroeder allpass: y = d + g (x - g d) on a line of `length` samples */
typedef struct NgsReverbAllpass {
	int32_t position;
	int32_t length;
	int32_t gain;
	int32_t line[NGS_REVERB_ALLPASS_SIZE];
} NgsReverbAllpass;

/* a path's late filters: a first-order FIR, two biquads and four allpasses */
typedef struct NgsReverbFilters {
	int32_t b0;			/* the FIR, y = b0 x + b1 x1 (times the late gain) */
	int32_t b1;
	int32_t reserved;		/* 0 */
	int32_t previous;		/* x1, the last input of the last block */
	int32_t reserved2;		/* cleared by the bypass */
	int32_t gain;			/* the late gain */
	NgsBiquad highCut;		/* the hf damping, designed by reverbHighShelf */
	NgsBiquad lowShelf;		/* the lf shelf */
	NgsReverbAllpass allpass[NGS_REVERB_ALLPASSES];
} NgsReverbFilters;

/* what one output path keeps: the ring and the early reflection taps */
typedef struct NgsReverbPath {
	int32_t dry;			/* the dry gain */
	int16_t *ring;			/* NGS_REVERB_RING samples */
	int32_t writePosition;
	int32_t reflectionsDelay;	/* samples the input waits in the ring, a multiple of 8 */
	int32_t reverbDelay;		/* samples the late reverb's input is delayed */
	int32_t reserved[3];
	int32_t tapDelay[NGS_REVERB_TAPS];	/* early reflection taps, samples before the end of the history */
	int32_t tapGain[NGS_REVERB_TAPS];
} NgsReverbPath;

typedef struct NgsReverbEngine {
	int16_t paths;			/* wet output paths */
	int16_t channels;		/* the channel layout Reset was given */
	int32_t lfoCosine;		/* the chorus's rotating phasor */
	int32_t lfoSine;
	uint32_t parameterId;		/* the id of the SceNgsReverbParams it was configured from */
	int32_t lineLength[NGS_REVERB_LINES];
	int32_t linePosition[NGS_REVERB_LINES];
	int32_t lineLast[NGS_REVERB_LINES];	/* the line's absorbed output, one sample on */
	int32_t lineOverrun[NGS_REVERB_LINES];	/* the line sample 3 past the block */
	int32_t lineDamping[NGS_REVERB_LINES];	/* y = gain x + damping y1 */
	int32_t lineGain[NGS_REVERB_LINES];
	uint32_t dcAccumulatorLow[2];	/* the output DC blocker's 64-bit accumulator, per channel */
	uint32_t dcAccumulatorHigh[2];
	int32_t dcInput[2];
	int32_t dcOutput[2];
	int32_t reserved[8];
	int32_t inputGain[NGS_REVERB_LINES][NGS_REVERB_PATHS];
	int32_t outputSign[NGS_REVERB_LINES][NGS_REVERB_PATHS];	/* +-1.0, 0 */
	NgsReverbFilters *filters[NGS_REVERB_PATHS];
	int16_t *lines[NGS_REVERB_LINES];	/* the delay lines, 0 without an FDN */
	NgsReverbPath path[NGS_REVERB_PATHS];
	int32_t *buffers[NGS_REVERB_PATHS];	/* the input on its way in, the wet output on its way out */
} NgsReverbEngine;

typedef char ngsReverbEngineSize[sizeof(NgsReverbEngine) == 2384 ? 1 : -1];
typedef char ngsReverbPathSize[sizeof(NgsReverbPath) == 96 ? 1 : -1];
typedef char ngsReverbFiltersSize[sizeof(NgsReverbFilters) == 9824 ? 1 : -1];

typedef struct NgsReverbSettings {
	uint32_t granularity;
	int32_t tail;			/* blocks run since the voice was keyed off */
	int32_t maxTail;		/* blocks for the reverb to die away */
	int32_t bypassed;
	NgsReverbEngine engine;		/* followed by what Reset lays out */
} NgsReverbSettings;

/* SceNgsReverbParams in 8.24 (levels in mB / 2000, percentages / 100), for the engine */
typedef struct NgsReverbConfig {
	SceNgsParamsDescriptor desc;
	int32_t room;
	int32_t roomHF;
	int32_t decayTime;
	int32_t decayHFRatio;
	int32_t reflections;
	int32_t reflectionsDelay;	/* less one block, 0 to 0.15 s */
	int32_t reverb;
	int32_t reverbDelay;
	int32_t diffusion;
	int32_t density;
	int32_t hfReference;		/* Hz */
	int32_t earlyReflectionPattern[SCE_NGS_MAX_SYSTEM_CHANNELS];
	int32_t earlyReflectionScalar;
	int32_t lfReference;		/* Hz */
	int32_t roomLF;
	int32_t dry;
} NgsReverbConfig;


/* custom early reflection taps the ARM can give Configure */
typedef struct NgsReverbTap {
	float gain;
	int32_t delay;			/* samples */
} NgsReverbTap;

/* reverbengine.c */
void ngsReverbRun(int32_t **inputs, int32_t **outputs, int32_t channels, uint32_t samples,
	NgsReverbEngine *engine);					/* 0x84be00 */
void ngsReverbBypass(int32_t **inputs, int32_t **outputs, int32_t channels, uint32_t samples,
	NgsReverbEngine *engine, int32_t reset);			/* 0x84be14 */
void ngsReverbEngine(NgsReverbSettings *settings, void *buffers, uint8_t *scratch); /* 0x84be2c */
int32_t ngsReverbReset(NgsReverbEngine *engine, const NgsReverbConfig *config, int32_t channels,
	int32_t unused, int32_t full);					/* 0x84ce30 */
int32_t ngsReverbConfigure(NgsReverbEngine *engine, const NgsReverbConfig *config,
	int32_t channels, const NgsReverbTap *taps);			/* 0x84d150 */
