/*
 * The ATRAC9 inverse MDCT (at9Imdct256 0x883ba8, at9Imdct128 0x8849e0,
 * at9Imdct64 0x885318).
 *
 * Each size is two passes of the IVC2 coprocessor over the 256 words of a
 * channel's spectrum (a 16 x 16 word matrix for 256, the same network for
 * the smaller frames with fewer columns):
 *
 *   first pass   a transform along the rows. Every iteration takes two
 *                columns of eight rows, one from the left (L, moving right)
 *                and one from the right (R, moving left), runs a network of
 *                subtractions and Q31 butterflies on them and stores the
 *                result in place.
 *   second pass  a transform along the columns, then the window and the
 *                overlap-add. Every iteration loads 32 words of the spectrum
 *                (X), the twiddle factors (TW) and window coefficients (WA to
 *                WD), runs a network, stores the new overlap (OV) and writes
 *                the windowed samples back to X as pairs of 16-bit values.
 *
 * The networks are the coprocessor's straight-line code, recovered
 * symbolically: a pass is a table of steps over a register file, run once
 * for every iteration. A step is one of
 *
 *   SUB        reg[d] = reg[a] - reg[b]
 *   MULQ       reg[d] = mulQ31(reg[a], reg[b])
 *   BFLY       m = mulQ31(reg[b], k); reg[d] = ave(reg[a], m);
 *              reg[e] = halfDifference(reg[a], m)
 *   BFLY_REG   the same with k = reg[c]
 *
 * The first pass's constants are the cosine table of the twiddle area; the
 * second pass's come from the tables.
 */
#pragma once

#include "at9.h"

/* the memory a network reads and writes; the pass moves each by its advance */
enum {
	AT9_L,			/* spectrum, left column (+8 bytes an iteration) */
	AT9_R,			/* spectrum, right column (-8) */
	AT9_X,			/* spectrum, second pass */
	AT9_OV,			/* overlap of the previous frame */
	AT9_TW,			/* twiddle factors */
	AT9_WA,			/* window coefficients */
	AT9_WB,
	AT9_WC,
	AT9_WD,
	AT9_BASES
};

enum {
	AT9_STEP_SUB,
	AT9_STEP_MULQ,
	AT9_STEP_BFLY,
	AT9_STEP_BFLY_REG
};

#define AT9_REGISTERS			176

/* a word the network reads: the byte offset from a base */
typedef struct At9Source {
	uint8_t base;
	uint16_t offset;
} At9Source;

typedef struct At9Step {
	uint8_t op;
	uint8_t d;
	uint8_t e;
	uint8_t a;
	uint8_t b;
	uint8_t c;
	int32_t k;
} At9Step;

/* a word the network stores */
typedef struct At9Sink {
	uint8_t base;
	uint16_t offset;
	uint8_t reg;
} At9Sink;

/*
 * A windowed sample pair, stored at X + offset. The accumulator holds
 * 2^35 + (reg[a] << 31) plus or minus reg[b] * reg[window]; the pair is its
 * low and high words shifted right by the pass's shift (4, 5 or 6 for frames of
 * 256, 128 or 64 samples) and saturated to 16 bits.
 */
typedef struct At9Sample {
	uint16_t offset;
	uint8_t a;
	uint8_t b;
	uint8_t window;
	int8_t sign;
} At9Sample;

typedef struct At9Pass {
	const At9Source *inputs;
	const At9Step *steps;
	const At9Sink *outputs;
	const At9Sample *samples;
	uint16_t numInputs;
	uint16_t numSteps;
	uint16_t numOutputs;
	uint16_t numSamples;
	uint8_t iterations;
	uint8_t shift;			/* of the samples' accumulator */
	int16_t advance[AT9_BASES];
} At9Pass;

#define AT9_COUNT(table)		(sizeof(table) / sizeof((table)[0]))

/* one frame size: its two passes and the tables of the second */
typedef struct At9Imdct {
	const At9Pass *first;
	const At9Pass *second;
	uint16_t rightColumn;		/* byte offset of the first right column */
	uint16_t tableWords;		/* words of each window table */
	uint16_t twiddleWords;		/* words of a twiddle block */
	const int32_t *tables;		/* WC, WD, TW, WA, WB in turn */
} At9Imdct;

void at9ImdctRun(const At9Imdct *imdct, At9Channel *channel, At9Work *work);
