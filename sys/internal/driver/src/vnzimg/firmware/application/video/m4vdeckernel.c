/*
 * The MPEG-4 decoder's block kernels (0x8a8cb0-0x8abd50): VLIW code the
 * luma and chroma jobs call, handing them a block in the IVC2 coprocessor's
 * registers (here an M4vdecBlock). Each IVC2 operation is written as what it
 * does to its lanes.
 */
#include "m4vdecdecoder.h"

/* the IDCT's cosines, cos(k pi / 16) * sqrt(2) * 8192 */
#define IDCT_C1		11363
#define IDCT_C2		10703
#define IDCT_C3		9633
#define IDCT_C5		6436
#define IDCT_C6		4433
#define IDCT_C7		2260

/* a lane's 16 bits */
static inline int16_t lane(int32_t value)
{
	return (int16_t)value;
}

/* a lane saturated to 16 bits */
static inline int16_t saturate16(int32_t value)
{
	return value > 32767 ? 32767 : value < -32768 ? -32768 : value;
}

/* cpslai3.h 4 then cpsrai3.h 4: a lane saturated to 12 bits */
static inline int16_t saturate12(int16_t value)
{
	return saturate16(value << 4) >> 4;
}

/* cppacku.b: a lane saturated to a sample */
static inline uint8_t sample(int32_t value)
{
	return value > 255 ? 255 : value < 0 ? 0 : value;
}

/* a block's 64 coefficients */
static inline void blockLoad(M4vdecBlock *block, const int16_t *coefficients)
{
	uint32_t i, j;

	for (i = 0; i < 8; i++)
		for (j = 0; j < 8; j++)
			block->coef[i][j] = coefficients[i * 8 + j];
}

/*
 * a block of its DC alone (lhcp sign-extends it over the first row's
 * lanes, cpperm0 by ccr8 keeps the first)
 */
static inline void blockLoadDc(M4vdecBlock *block, const int16_t *coefficients)
{
	uint32_t i, j;

	for (i = 0; i < 8; i++)
		for (j = 0; j < 8; j++)
			block->coef[i][j] = 0;
	block->coef[0][0] = coefficients[0];
}

/*
 * one dimension of the IDCT: the even part from x0 and x4 (by 8192) and
 * x2, x6, the odd from x1, x3, x5, x7; rounded by `round`, shifted by `shift`
 */
static inline void idct8(const int16_t x[8], int16_t y[8], int32_t round, uint32_t shift)
{
	int32_t even[4], odd[4], k;

	even[0] = ((x[0] + x[4]) << 13) + round + IDCT_C2 * x[2] + IDCT_C6 * x[6];
	even[1] = ((x[0] - x[4]) << 13) + round + IDCT_C6 * x[2] - IDCT_C2 * x[6];
	even[2] = ((x[0] - x[4]) << 13) + round - IDCT_C6 * x[2] + IDCT_C2 * x[6];
	even[3] = ((x[0] + x[4]) << 13) + round - IDCT_C2 * x[2] - IDCT_C6 * x[6];
	odd[0] = IDCT_C1 * x[1] + IDCT_C3 * x[3] + IDCT_C5 * x[5] + IDCT_C7 * x[7];
	odd[1] = IDCT_C3 * x[1] - IDCT_C7 * x[3] - IDCT_C1 * x[5] - IDCT_C5 * x[7];
	odd[2] = IDCT_C5 * x[1] - IDCT_C1 * x[3] + IDCT_C7 * x[5] + IDCT_C3 * x[7];
	odd[3] = IDCT_C7 * x[1] - IDCT_C5 * x[3] + IDCT_C3 * x[5] - IDCT_C1 * x[7];
	for (k = 0; k < 4; k++) {
		y[k] = lane((even[k] + odd[k]) >> shift);
		y[7 - k] = lane((even[k] - odd[k]) >> shift);
	}
}

/*
 * the IDCT's rows, rounded (+512 >> 10), left a column of four
 * rows a register: coef[i][4 h + l] the row 4 (i / 4) + l's column
 * 2 (i % 4) + h, the layout the columns' pass takes
 */
static inline void idctRows(M4vdecBlock *block)
{
	int16_t rows[8][8];
	uint32_t i, j;

	for (i = 0; i < 8; i++)
		idct8(block->coef[i], rows[i], 512, 10);
	for (i = 0; i < 8; i++)
		for (j = 0; j < 8; j++)
			block->coef[i][j] = rows[4 * (i / 4) + j % 4][2 * (i % 4) + j / 4];
}

/* the IDCT's columns (+32 << 13 >> 19), from the rows' layout back to rows */
static inline void idctColumns(M4vdecBlock *block)
{
	int16_t rows[8][8], column[8], out[8];
	uint32_t i, j;

	for (i = 0; i < 8; i++)
		for (j = 0; j < 8; j++)
			rows[4 * (i / 4) + j % 4][2 * (i % 4) + j / 4] = block->coef[i][j];
	for (j = 0; j < 8; j++) {
		for (i = 0; i < 8; i++)
			column[i] = rows[i][j];
		column[0] += 32;
		idct8(column, out, 0, 19);
		for (i = 0; i < 8; i++)
			block->coef[i][j] = out[i];
	}
}

/*
 * the block dequantised (the second method, H.263's): (2 |level| + 1) *
 * quant, less one for an even quant; in 12 bits
 */
static inline void dequantize(M4vdecBlock *block, uint32_t even, uint32_t quant)
{
	int16_t level, value;
	uint32_t i, j;

	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			level = block->coef[i][j];
			value = lane(saturate16(lane(level < 0 ? -level : level) << 1) + 1);
			value = lane(lane(value * (int16_t)quant) - (int16_t)even);
			if (level < 0)
				value = lane(-value);
			block->coef[i][j] = level ? saturate12(value) : 0;
		}
	}
}

/* the block's samples stored, `stride` a row */
static inline void blockStore(const M4vdecBlock *block, uint8_t *samples, uint32_t stride)
{
	uint32_t i, j;

	for (i = 0; i < 8; i++)
		for (j = 0; j < 8; j++)
			samples[i * stride + j] = sample(block->coef[i][j]);
}

/* the block's residual added to the prediction there */
static inline void blockAdd(const M4vdecBlock *block, uint8_t *samples, uint32_t stride)
{
	uint32_t i, j;

	for (i = 0; i < 8; i++)
		for (j = 0; j < 8; j++)
			samples[i * stride + j] = sample(samples[i * stride + j] + block->coef[i][j]);
}

/* the block's first column and first row kept, for the blocks right of and below it */
static inline void predictorsStore(const M4vdecBlock *block, int16_t *column, int16_t *row)
{
	uint32_t i;

	for (i = 0; i < 8; i++)
		row[i] = block->coef[0][i];
	for (i = 0; i < 8; i++)
		column[i] = block->coef[i][0];
}

/* 0x8a8cb0: a luma block's coefficients */
void m4vdecBlockLoad(M4vdecBlock *block, const int16_t *coefficients)
{
	blockLoad(block, coefficients);
}

/* 0x8a8d40 */
void m4vdecBlockLoadDc(M4vdecBlock *block, const int16_t *coefficients)
{
	blockLoadDc(block, coefficients);
}

/* 0x8a8d90 */
void m4vdecIdctRows(M4vdecBlock *block)
{
	idctRows(block);
}

/* 0x8a91d0 */
void m4vdecIdctColumns(M4vdecBlock *block)
{
	idctColumns(block);
}

/* 0x8a9620 */
void m4vdecDequantize(M4vdecBlock *block, uint32_t even, uint32_t quant)
{
	dequantize(block, even, quant);
}

/* 0x8a9998: the samples in a luma tile, 16 a row */
void m4vdecBlockStore(const M4vdecBlock *block, uint8_t *samples)
{
	blockStore(block, samples, 16);
}

/* 0x8a99e8 */
void m4vdecBlockAdd(const M4vdecBlock *block, uint8_t *samples)
{
	blockAdd(block, samples, 16);
}

/* 0x8a9af8 */
void m4vdecPredictorsStore(const M4vdecBlock *block, int16_t *column, int16_t *row)
{
	predictorsStore(block, column, row);
}

/* 0x8a9b38-0x8aa9c0: the chroma's copies; a chroma tile is 8 samples a row */
void m4vdecChromaBlockLoad(M4vdecBlock *block, const int16_t *coefficients)	/* 0x8a9b38 */
{
	blockLoad(block, coefficients);
}

void m4vdecChromaBlockLoadDc(M4vdecBlock *block, const int16_t *coefficients)	/* 0x8a9bc8 */
{
	blockLoadDc(block, coefficients);
}

void m4vdecChromaIdctRows(M4vdecBlock *block)				/* 0x8a9c18 */
{
	idctRows(block);
}

void m4vdecChromaIdctColumns(M4vdecBlock *block)				/* 0x8aa058 */
{
	idctColumns(block);
}

void m4vdecChromaDequantize(M4vdecBlock *block, uint32_t even, uint32_t quant)	/* 0x8aa4a8 */
{
	dequantize(block, even, quant);
}

void m4vdecChromaBlockStore(const M4vdecBlock *block, uint8_t *samples)	/* 0x8aa820 */
{
	blockStore(block, samples, 8);
}

void m4vdecChromaBlockAdd(const M4vdecBlock *block, uint8_t *samples)	/* 0x8aa870 */
{
	blockAdd(block, samples, 8);
}

/* 0x8aa980: its row first */
void m4vdecChromaPredictorsStore(const M4vdecBlock *block, int16_t *row, int16_t *column)
{
	predictorsStore(block, column, row);
}

/* the quantisers' reciprocals, 2^18 / quant (above 32767 less 65536) */
static const int16_t g_m4vdecQuantReciprocals[32] = {			/* 0x8fc1f8 */
	-1, 1, 1, 21845, 0, -13107, -21845, -28087,
	-32768, 29127, 26214, 23831, 21845, 20165, 18725, 17476,
	16384, 15420, 14564, 13797, 13107, 12483, 11916, 11398,
	10923, 10486, 10082, 9709, 9362, 9039, 8738, 8456,
};

/*
 * 0x8aa9c0: a neighbour's AC predictors rescaled to the block's quantiser:
 * by its quantiser, in 12 bits, then divided by the block's, rounded away
 * from 0
 */
static void predictorScale(M4vdecBlock *block, int32_t quant, int32_t predictorQuant)
{
	int32_t reciprocal, magnitude, value, sign;
	uint32_t i;

	for (i = 0; i < 8; i++) {
		value = saturate12(lane(block->predictor[i] * (int16_t)predictorQuant));
		if (quant >= 3) {
			sign = value < 0 ? -1 : 1;
			magnitude = lane(value * sign);
			reciprocal = g_m4vdecQuantReciprocals[quant];
			if (quant >= 9)
				value = (magnitude * reciprocal + 133120) >> 18;
			else
				value = ((magnitude << 16) + magnitude * reciprocal + 133120) >> 18;
			value = lane(lane(value) * sign);
		} else if (quant == 2) {
			value = (value + (value >= 0)) >> 1;
		}
		block->predictor[i] = value;
	}
}

/* 0x8aab00: the DC predicted, the first column's AC from the block to the left */
void m4vdecAcPredictLeft(M4vdecBlock *block)
{
	uint32_t i;

	for (i = 1; i < 8; i++)
		block->coef[i][0] = lane(block->coef[i][0] + block->predictor[i]);
	for (i = 0; i < 4; i++)
		block->coef[0][i] = lane(block->coef[0][i] + block->dc[i]);
	for (i = 0; i < 8; i++) {
		block->coef[i][0] = saturate12(block->coef[i][0]);
		block->coef[i][1] = saturate12(block->coef[i][1]);
		block->coef[i][2] = saturate12(block->coef[i][2]);
		block->coef[i][3] = saturate12(block->coef[i][3]);
	}
}

/* 0x8aaba0: and with the left block's quantiser another */
void m4vdecAcPredictLeftScaled(M4vdecBlock *block, int32_t quant, int32_t predictorQuant)
{
	predictorScale(block, quant, predictorQuant);
	m4vdecAcPredictLeft(block);
}

/* 0x8aac60: the DC predicted, the first row's AC from the block above */
void m4vdecAcPredictTop(M4vdecBlock *block)
{
	uint32_t i;

	block->predictor[0] = block->dc[0];
	for (i = 0; i < 8; i++)
		block->coef[0][i] = saturate12(lane(block->coef[0][i] + block->predictor[i]));
}

/* 0x8aac80: and with the block above's quantiser another */
void m4vdecAcPredictTopScaled(M4vdecBlock *block, int32_t quant, int32_t predictorQuant)
{
	predictorScale(block, quant, predictorQuant);
	m4vdecAcPredictTop(block);
}

/*
 * Motion compensation's kernels: a block's 9 x 9 reference samples read
 * from the frame's tiles, the picture's edges repeated, then interpolated.
 * The luma's (0x8aacc0-0x8ab518) and the chroma's (0x8ab518-0x8abd50)
 * differ in their tiles: 16 samples a row and 16 rows for the luma, 8 and
 * 8 for the chroma, a tile's Cb then its Cr.
 */

/*
 * the left edge inside the samples: the rows moved on by `shift` (mod 8)
 * samples, the first repeated in; `next` each row turned (its first the
 * ninth sample)
 */
static inline void referenceLeftEdge(M4vdecReference *reference, uint32_t shift)
{
	uint8_t row[8];
	uint32_t r, c;

	shift &= 7;
	for (r = 0; r < 9; r++) {
		for (c = 0; c < 8; c++)
			reference->next[r][c] = reference->rows[r][(c + 8 - shift) & 7];
		for (c = 0; c < 8; c++)
			row[c] = c >= shift ? reference->rows[r][c - shift] : reference->rows[r][0];
		for (c = 0; c < 8; c++)
			reference->rows[r][c] = row[c];
	}
}

/* the left edge past the samples: each row its first sample; `next` the row it was */
static inline void referenceLeftOutside(M4vdecReference *reference)
{
	uint32_t r, c;

	for (r = 0; r < 9; r++) {
		for (c = 0; c < 8; c++)
			reference->next[r][c] = reference->rows[r][c];
		for (c = 1; c < 8; c++)
			reference->rows[r][c] = reference->rows[r][0];
	}
}

/*
 * the right edge inside the samples: the rows moved back by 8 - (shift
 * mod 8) samples (all 8 for 0), the ninth repeated in
 */
static inline void referenceRightEdge(M4vdecReference *reference, uint32_t shift)
{
	uint8_t row[8];
	uint32_t r, c;

	shift &= 7;
	for (r = 0; r < 9; r++) {
		for (c = 0; c < 8; c++)
			row[c] = shift && c < shift ? reference->rows[r][8 - shift + c] :
				reference->next[r][0];
		for (c = 0; c < 8; c++)
			reference->rows[r][c] = row[c];
	}
}

/* the right edge past the samples: each row its ninth sample */
static inline void referenceRightOutside(M4vdecReference *reference)
{
	uint32_t r, c;

	for (r = 0; r < 9; r++)
		for (c = 0; c < 8; c++)
			reference->rows[r][c] = reference->next[r][0];
}

/* the top edge: the rows moved down by -y, the first repeated in */
static inline void referenceTopEdge(M4vdecReference *reference, int32_t y)
{
	uint32_t r, c;

	for (; y < 0; y++) {
		for (r = 8; r > 0; r--) {
			for (c = 0; c < 8; c++) {
				reference->rows[r][c] = reference->rows[r - 1][c];
				reference->next[r][c] = reference->next[r - 1][c];
			}
		}
	}
}

/* the bottom edge: the rows moved up by `past`, the last repeated in */
static inline void referenceBottomEdge(M4vdecReference *reference, int32_t past)
{
	uint32_t r, c;

	for (; past >= 1; past--) {
		for (r = 0; r < 8; r++) {
			for (c = 0; c < 8; c++) {
				reference->rows[r][c] = reference->rows[r + 1][c];
				reference->next[r][c] = reference->next[r + 1][c];
			}
		}
	}
}

/* each row one sample on, its ninth sample last */
static inline void referenceNext(M4vdecReference *reference)
{
	uint32_t r, c;
	uint8_t ninth;

	for (r = 0; r < 9; r++) {
		ninth = reference->next[r][0];
		for (c = 0; c < 7; c++)
			reference->next[r][c] = reference->rows[r][c + 1];
		reference->next[r][7] = ninth;
	}
}

/*
 * 9 rows of the frame's tiles read: from `first`, the 8 bytes the first
 * sample is in, and `second`, the 8 after it (in the tile on where the row
 * goes past its tile), `shift` samples in; `stride` on to the tile row
 * below after a tile's last row (y the first row's). `next` each row's
 * second 8 bytes turned to the ninth sample.
 */
static inline void referenceLoad(M4vdecReference *reference, const uint8_t *first,
	const uint8_t *second, int32_t stride, uint32_t y, uint32_t shift, uint32_t pitch,
	uint32_t tileRows)
{
	uint32_t r, c;

	for (r = 0; r < 9; r++) {
		for (c = 0; c < 8; c++) {
			reference->rows[r][c] = shift + c < 8 ? first[shift + c] : second[shift + c - 8];
			reference->next[r][c] = second[(shift + c) & 7];
		}
		first += pitch;
		second += pitch;
		if (((y + r) & (tileRows - 1)) == tileRows - 1) {
			first += stride;
			second += stride;
		}
	}
}

/* a block predicted from whole samples, `stride` a row */
static inline void predictFull(const M4vdecReference *reference, uint8_t *samples, uint32_t stride)
{
	uint32_t r, c;

	for (r = 0; r < 8; r++)
		for (c = 0; c < 8; c++)
			samples[r * stride + c] = reference->rows[r][c];
}

/* half a sample across, `round` (0 for vop_rounding_type 1, else 1) added before halving */
static inline void predictAcross(const M4vdecReference *reference, uint8_t *samples,
	uint32_t stride, uint32_t round)
{
	uint32_t r, c;

	for (r = 0; r < 8; r++)
		for (c = 0; c < 8; c++)
			samples[r * stride + c] = (reference->rows[r][c] + reference->next[r][c] +
				round) >> 1;
}

/* half a sample down */
static inline void predictDown(const M4vdecReference *reference, uint8_t *samples,
	uint32_t stride, uint32_t round)
{
	uint32_t r, c;

	for (r = 0; r < 8; r++)
		for (c = 0; c < 8; c++)
			samples[r * stride + c] = (reference->rows[r][c] + reference->rows[r + 1][c] +
				round) >> 1;
}

/* half a sample across and down: the four samples' sum, `round` added, by 4 */
static inline void predictBoth(const M4vdecReference *reference, uint8_t *samples,
	uint32_t stride, uint32_t round)
{
	uint32_t r, c;

	for (r = 0; r < 8; r++)
		for (c = 0; c < 8; c++)
			samples[r * stride + c] = (round + reference->rows[r][c] +
				reference->next[r][c] + reference->rows[r + 1][c] +
				reference->next[r + 1][c]) >> 2;
}

/* the luma's */
void m4vdecReferenceLeftEdge(M4vdecReference *reference, uint32_t shift)	/* 0x8aacc0 */
{
	referenceLeftEdge(reference, shift);
}

void m4vdecReferenceLeftOutside(M4vdecReference *reference)		/* 0x8aad30 */
{
	referenceLeftOutside(reference);
}

void m4vdecReferenceRightEdge(M4vdecReference *reference, uint32_t shift)	/* 0x8aad78 */
{
	referenceRightEdge(reference, shift);
}

void m4vdecReferenceRightOutside(M4vdecReference *reference)		/* 0x8aadc0 */
{
	referenceRightOutside(reference);
}

void m4vdecReferenceTopEdge(M4vdecReference *reference, int32_t y)	/* 0x8aade8 */
{
	referenceTopEdge(reference, y);
}

void m4vdecReferenceBottomEdge(M4vdecReference *reference, int32_t past)	/* 0x8aae40 */
{
	referenceBottomEdge(reference, past);
}

void m4vdecReferenceNext(M4vdecReference *reference)			/* 0x8aae98 */
{
	referenceNext(reference);
}

/* 0x8aaec0: a luma tile's rows 16 bytes apart, its tile row 16 rows */
void m4vdecReferenceLoad(M4vdecReference *reference, const uint8_t *first,
	const uint8_t *second, int32_t stride, uint32_t y, uint32_t shift)
{
	referenceLoad(reference, first, second, stride, y, shift, 16, 16);
}

void m4vdecPredictFull(const M4vdecReference *reference, uint8_t *samples)	/* 0x8ab290 */
{
	predictFull(reference, samples, 16);
}

void m4vdecPredictAcross(const M4vdecReference *reference, uint8_t *samples)	/* 0x8ab2e0 */
{
	predictAcross(reference, samples, 16, 0);
}

void m4vdecPredictAcrossRounded(const M4vdecReference *reference, uint8_t *samples) /* 0x8ab330 */
{
	predictAcross(reference, samples, 16, 1);
}

void m4vdecPredictDown(const M4vdecReference *reference, uint8_t *samples)	/* 0x8ab380 */
{
	predictDown(reference, samples, 16, 0);
}

void m4vdecPredictDownRounded(const M4vdecReference *reference, uint8_t *samples) /* 0x8ab3d0 */
{
	predictDown(reference, samples, 16, 1);
}

void m4vdecPredictBoth(const M4vdecReference *reference, uint8_t *samples,
	uint32_t round)							/* 0x8ab420 */
{
	predictBoth(reference, samples, 16, round);
}

/* the chroma's */
void m4vdecChromaReferenceLeftEdge(M4vdecReference *reference, uint32_t shift)	/* 0x8ab518 */
{
	referenceLeftEdge(reference, shift);
}

void m4vdecChromaReferenceLeftOutside(M4vdecReference *reference)	/* 0x8ab588 */
{
	referenceLeftOutside(reference);
}

void m4vdecChromaReferenceRightEdge(M4vdecReference *reference, uint32_t shift) /* 0x8ab5d0 */
{
	referenceRightEdge(reference, shift);
}

void m4vdecChromaReferenceRightOutside(M4vdecReference *reference)	/* 0x8ab618 */
{
	referenceRightOutside(reference);
}

void m4vdecChromaReferenceTopEdge(M4vdecReference *reference, int32_t y)	/* 0x8ab640 */
{
	referenceTopEdge(reference, y);
}

void m4vdecChromaReferenceBottomEdge(M4vdecReference *reference, int32_t past) /* 0x8ab698 */
{
	referenceBottomEdge(reference, past);
}

/* 0x8ab6f0: a picture of one tile across: each row turned by one sample */
void m4vdecChromaReferenceNarrow(M4vdecReference *reference)
{
	uint8_t rows, next;
	uint32_t r, c;

	for (r = 0; r < 9; r++) {
		rows = reference->rows[r][7];
		next = reference->next[r][7];
		for (c = 7; c > 0; c--) {
			reference->rows[r][c] = reference->rows[r][c - 1];
			reference->next[r][c] = reference->next[r][c - 1];
		}
		reference->rows[r][0] = rows;
		reference->next[r][0] = next;
	}
}

void m4vdecChromaReferenceNext(M4vdecReference *reference)		/* 0x8ab740 */
{
	referenceNext(reference);
}

/* 0x8ab768: a chroma tile's rows 8 bytes apart, its tile row 8 rows */
void m4vdecChromaReferenceLoad(M4vdecReference *reference, const uint8_t *first,
	const uint8_t *second, int32_t stride, uint32_t y, uint32_t shift)
{
	referenceLoad(reference, first, second, stride, y, shift, 8, 8);
}

void m4vdecChromaPredictFull(const M4vdecReference *reference, uint8_t *samples) /* 0x8abad0 */
{
	predictFull(reference, samples, 8);
}

void m4vdecChromaPredictAcross(const M4vdecReference *reference, uint8_t *samples) /* 0x8abb20 */
{
	predictAcross(reference, samples, 8, 0);
}

void m4vdecChromaPredictAcrossRounded(const M4vdecReference *reference,
	uint8_t *samples)						/* 0x8abb70 */
{
	predictAcross(reference, samples, 8, 1);
}

void m4vdecChromaPredictDown(const M4vdecReference *reference, uint8_t *samples) /* 0x8abbc0 */
{
	predictDown(reference, samples, 8, 0);
}

void m4vdecChromaPredictDownRounded(const M4vdecReference *reference,
	uint8_t *samples)						/* 0x8abc10 */
{
	predictDown(reference, samples, 8, 1);
}

void m4vdecChromaPredictBoth(const M4vdecReference *reference, uint8_t *samples,
	uint32_t round)							/* 0x8abc60 */
{
	predictBoth(reference, samples, 8, round);
}
