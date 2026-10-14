/*
 * The audio-in echo canceller (RPCs 416-424, os0 audioin's aec_rpc): a
 * frequency-domain canceller over 512-sample half frames. The far-end
 * signal and the microphone are windowed (1024 samples, Hamming) and
 * transformed; each of the 512 bins tracks the two levels and the delay
 * between them, and the microphone's bin is scaled down by an echo and a
 * noise suppression gain. The result is resynthesized by overlap-add, then
 * an automatic gain and a look-ahead limiter run on it.
 */
#pragma once

#include "../application.h"

#define AEC_BINS		512
#define AEC_HALF		512	/* samples a call, half a frame */
#define AEC_FRAME		1024
#define AEC_STATE_SIZE		(AEC_BINS * 56)
#define AEC_WORK_SIZE		0xb708
#define AEC_MEMORY_SIZE		0xb740	/* what the ARM allocates */
#define AEC_CURVE_POINTS	16
#define AEC_LIMITER_HISTORY	32

/* the work's flags */
#define AEC_FLAG_BYPASS		0x10	/* only the limiter, on the microphone */
#define AEC_FLAG_CLEAR		0x20	/* the signal histories cleared once */

/* a bin's state (56 bytes): levels in Q9, logs in Q12 */
typedef struct AecBin {
	int32_t farLevel;		/* |far|, averaged */
	int32_t farPower;		/* its slow average, Q9 */
	int32_t nearLevel;		/* |near|, averaged */
	int32_t nearPower;
	int32_t noise;			/* the near floor, Q7 */
	int32_t delay;			/* log near - log far - delay, the last */
	int32_t delayMean;
	int32_t delayMoment;		/* the delay's mean, Q16 */
	uint32_t delaySquare;		/* its mean square */
	int32_t noiseGain;
	int32_t echoGain;
	int32_t reserved;
	int32_t delaySpread;
	int32_t delayAverage;
} AecBin;

typedef struct AecComplex {
	int32_t re;
	int32_t im;
} AecComplex;

/* the output's automatic gain (24 bytes) */
typedef struct AecAgc {
	uint32_t nearPeak;		/* the output's peaks, averaged */
	uint32_t inputPeak;		/* the microphone's */
	uint16_t farPeak;		/* the far end's */
	uint16_t reserved;
	uint32_t level;			/* the far end's level, Q7 */
	uint32_t reserved2;
	uint32_t gain;			/* Q13 */
} AecAgc;

/* the look-ahead limiter (152 bytes) */
typedef struct AecLimiter {
	int32_t count;			/* samples a call */
	uint16_t gain;			/* the input's, Q12 */
	uint16_t reserved;
	int32_t envelope;		/* Q9 */
	int32_t hold;			/* negative: samples the peak is held */
	int32_t peak;
	int32_t history[AEC_LIMITER_HISTORY];
	int32_t index;
} AecLimiter;

/* the canceller's memory, in what the ARM gives (0xb708 bytes) */
typedef struct AecWork {
	AecBin bins[AEC_BINS];		/* the state RPC 424 copies out */
	AecAgc agc;
	AecLimiter limiter;
	uint32_t reserved[2];
	uint32_t agcGain;		/* the last call's */
	int32_t echoSuppression;
	int32_t noiseSuppression;
	int32_t agcEnabled;
	int32_t echoScale;
	int32_t noiseScale;
	int32_t highPass;
	int32_t reserved2;
	int32_t delay;			/* the far end's, in the bins' log units */
	int16_t farHistory[AEC_FRAME];
	int16_t farPrevious[AEC_HALF];
	int16_t nearPrevious[AEC_HALF];
	int16_t outputPrevious[AEC_HALF];
	uint32_t reserved3;
	AecComplex far[AEC_BINS];
	AecComplex near[AEC_BINS];
	AecComplex output[AEC_BINS];
	int32_t highPassState[3];	/* the filter's input, then its two delays */
	uint32_t farFill;		/* far samples kept from the last call */
	uint32_t flags;
	int32_t frames;			/* since the parameters changed, to 100 */
	uint8_t curve[AEC_BINS];	/* the microphone's gain by bin, (x + 1) / 256 */
	uint32_t size;			/* bytes a call */
	VkVThreadContext *context;
	int32_t result;
	uint32_t reserved4;
} AecWork;

/* aecengine.c */
void aecSetCurve(AecWork *work, const uint8_t *points);
void aecApplyCurve(const uint8_t *curve, AecComplex *spectrum);
void aecSetDelay(AecWork *work, int16_t delay, uint32_t flags);
uint32_t aecSetFarFill(AecWork *work, uint32_t fill);
void aecReset(AecWork *work);
int32_t aecProcess(AecWork *work, int16_t *near, const int16_t *far);

/* aecmath.c: fixed point, logs and exponents in Q12 */
uint16_t aecSqrt(uint32_t value);
uint16_t aecLog2Fraction(int16_t value);
int16_t aecExp2Fraction(uint16_t value);
int32_t aecLog2(int32_t value);
int32_t aecExp2(uint32_t value);
int32_t aecLog2Long(uint64_t *value);

/* VLIW: the two frames' transforms, and the inverse (aectransform.c) */
int32_t aecTransform(uint32_t mode, const int16_t *frame, const int16_t *frame2,
	AecComplex *spectrum, AecComplex *spectrum2);			/* 0x8dfc38 */
int32_t aecTransformReal(uint32_t mode, const int16_t *frame, AecComplex *spectrum,
	uint32_t inverse);						/* 0x8dfe98 */
int32_t aecInverse(uint32_t mode, const AecComplex *spectrum, int16_t *frame,
	uint32_t inverse);						/* 0x8dffa8 */
/* not yet written: the 1024-point radix-4 FFT in place, inverse when asked */
void aecFft(uint32_t mode, AecComplex *z, uint32_t inverse);		/* 0x8df640 */
