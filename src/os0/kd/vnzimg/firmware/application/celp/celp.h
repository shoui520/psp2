/*
 * The MPEG-4 CELP codecs (narrowband, 8 kHz): the decoder at 0x8546e4-0x854b3c
 * with its library at 0x8c4000-0x8c8758, and the encoder at 0x854b82-0x857932
 * with its library at 0x8c8758-0x8d1000. The ARM's RPC layer is in celpdec.c
 * and celpenc.c.
 *
 * Values are fixed point: LSP frequencies and LPC coefficients Q24 and Q15
 * (as noted), samples and gains in the formats of the 32-bit words below.
 */
#pragma once

#include "../application.h"

#ifndef max
#define max(a, b)			((a) > (b) ? (a) : (b))
#define min(a, b)			((a) < (b) ? (a) : (b))
#endif

#define CELP_ORDER			10
#define CELP_MAX_ORDER			20
#define CELP_MAX_SUBFRAMES		8
#define CELP_MAX_FRAME			320
#define CELP_MAX_SUBFRAME		80
#define CELP_MAX_PULSES			12
#define CELP_HISTORY_WORDS		384

/* errors of the engine entry points (SCE-style, 0x80000000 + n) */
#define CELP_ERROR_STATE		((int32_t)0x80000001)
#define CELP_ERROR_SIZE			((int32_t)0x80000003)
#define CELP_ERROR_POINTER		((int32_t)0x80000004)

/* the decoder's state, 8 bytes into the memory the ARM gives */
typedef struct CelpDecoderState {
	const uint8_t *stream;			/* the frame's bytes */
	int16_t *pcm;
	uint32_t lostCount;			/* frames lost before this one */
	int32_t align;				/* the frame ends on a byte */
	int32_t invalid;			/* the bit rate is not one the stream may have */
	int32_t lostFrames;			/* in a row */
	const uint8_t *bitBase;			/* the bit reader */
	int32_t bitPosition;
	int32_t frameBits;			/* bits of a frame at this bit rate */
	int32_t frameLength;			/* samples of a frame */
	int32_t bitRateIndex;
	int32_t gainMode;			/* header: 2 bits */
	int32_t gainIndex;			/* 6 bits */
	int32_t lspIndex[4];			/* 4, 4, 7 and 6 bits */
	int32_t lspPredicted;			/* 1 bit */
	int32_t headerSpare[6];
	int32_t positionIndex[CELP_MAX_SUBFRAMES];	/* per subframe: the pulse positions */
	int32_t signIndex[CELP_MAX_SUBFRAMES];		/* the pulse signs */
	int32_t gainCode[CELP_MAX_SUBFRAMES];		/* the gain codebook entry */
	int32_t lagIndex[CELP_MAX_SUBFRAMES];		/* the pitch lag */
	int32_t reserved[2];
	int32_t order;				/* of the LPC */
	int32_t frameLengthCopy;
	int32_t subframes;
	int32_t subframeLength;
	int32_t lpc[(CELP_MAX_SUBFRAMES + 2) * CELP_MAX_ORDER];	/* Q15, a subframe after another */
	int32_t previousLsp[CELP_MAX_ORDER];	/* Q24 */
	int32_t lsp[CELP_MAX_ORDER];
	int32_t excitation[CELP_MAX_FRAME];
	int32_t gainLevel[CELP_MAX_SUBFRAMES];
	int32_t reserved2[2];
	int32_t previousGainLevel;
	int32_t pulseCount;
	int32_t pulseWidth[CELP_MAX_PULSES];	/* bits of a pulse's position */
	int32_t pulsePosition[CELP_MAX_PULSES][CELP_MAX_SUBFRAME];
	int32_t history[CELP_HISTORY_WORDS];	/* the past excitation */
	int32_t reserved3[2];			/* cleared with the history */
	int32_t output[CELP_MAX_FRAME];
	int32_t spare[CELP_MAX_FRAME];
	int32_t filterState[CELP_MAX_ORDER];	/* of the synthesis filter */
	int32_t frameCounter;
} CelpDecoderState;

typedef char celpDecoderStateSize[sizeof(CelpDecoderState) == 0x2974 ? 1 : -1];
typedef char celpDecoderStateLpc[offsetof(CelpDecoderState, lpc) == 0xf8 ? 1 : -1];
typedef char celpDecoderStateLsp[offsetof(CelpDecoderState, lsp) == 0x468 ? 1 : -1];
typedef char celpDecoderStateHistory[offsetof(CelpDecoderState, history) == 0x1918 ? 1 : -1];

/* the state inside the engine's memory */
static inline CelpDecoderState *celpDecoderState(void *engine)
{
	return (CelpDecoderState *)(((uint32_t)engine & ~7u) + 8);
}

/* tables (celptables.c) */
extern const int32_t celpModeIndices[10];
extern const int32_t celpLevelScale[4];
extern const int32_t celpGainScale[4];
extern const int32_t celpGainExponent[4];
extern const int32_t celpLevelFactor[4];
extern const int32_t celpSharpening[4];
extern const int8_t celpPositionBits[64];
extern const int8_t celpSignBits[64];
extern const int32_t celpFrameLengths[32];
extern const int32_t celpSubframeCounts[32];
extern const int32_t celpSubframeLengths[32];
extern const int32_t celpLagInteger[768];
extern const int32_t celpLagFraction[768];
extern const int32_t celpLimits[4];
extern const int32_t celpLspFirst[16][5];
extern const int32_t celpLspSecond[16][5];
extern const int32_t celpLspThird[64][5];
extern const int32_t celpLspFourth[32][5];
extern const int32_t celpLspThirdPredicted[64][5];
extern const int32_t celpLspFourthPredicted[32][5];
extern const int32_t celpGainBookLong[64][4][2];
extern const int32_t celpGainBookShort[64][4][2];
extern const int32_t celpInterpolation[37];
extern const int32_t celpSubframeWeights[6];
extern const int32_t celpBitRates[10];
extern const int32_t celpBitRateIndices[10];

/* fixed-point math shared by the codecs (celpmath.c) */
int32_t celpLog10(int32_t x);
int32_t celpLn(int32_t x);
int32_t celpExp(int32_t x);
int32_t celpPow(int32_t x, int32_t y);
uint32_t celpInvSqrt(int32_t x);
int32_t celpSqrt(int32_t x);
int32_t celpSin(int32_t t);
int32_t celpCos(int32_t t);

/* the bit reader (celpbits.c) */
void celpBitsReset(CelpDecoderState *state);
uint32_t celpBitsGet(CelpDecoderState *state, uint32_t count);
uint32_t celpBitsGetWide(CelpDecoderState *state, uint32_t count);
void celpBitsAlign(CelpDecoderState *state);

/* the frame (celpdecframe.c, celplsp.c) */
void celpDecodeHeader(CelpDecoderState *state);
void celpDecodeLsp(CelpDecoderState *state);
void celpDecodeExcitation(CelpDecoderState *state);
void celpSynthesize(CelpDecoderState *state);
void celpBuildPulses(CelpDecoderState *state, int32_t subframeLength, int32_t pulses, int32_t *widths,
	int32_t (*positions)[CELP_MAX_SUBFRAME]);
int32_t celpFrameBits(const CelpDecoderState *state);
