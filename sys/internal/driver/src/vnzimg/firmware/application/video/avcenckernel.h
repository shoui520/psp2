/*
 * The AVC encoder's kernels: the state they keep in the IVC2 registers
 */
#ifndef AVCENC_KERNEL_H
#define AVCENC_KERNEL_H

#include <stdint.h>
#include "service.h"

/*
 * the motion plane's downscale, as its kernels keep it in the IVC2 registers.
 * The source's luma is in tiles 16 pixels wide; a band of eight of its lines
 * goes through a 15-tap horizontal filter (every fourth pixel) and a 7-tap
 * vertical one, into two lines of the plane: line 0's every eighth column
 * and line 4's, four columns to the right
 */
typedef struct AvcencDownscale {
	uint8_t pixels[8][24];		/* by line: the last three groups of eight loaded */
	uint8_t columns[8][8];		/* by line: the horizontal pass's, of the block of 32 */
	uint8_t samples[8];		/* line 0's columns 0, 2, 4 and 6, line 4's 1, 3, 5 and 7 */
} AvcencDownscale;

/* the downscale's kernels (avcenckernel.c) */
void avcencDownscaleLoadLeft(AvcencDownscale *downscale, const uint8_t *source);
void avcencDownscaleLoadRight(AvcencDownscale *downscale, const uint8_t *source);
void avcencDownscaleEdgeLeft(AvcencDownscale *downscale);
void avcencDownscaleColumns0(AvcencDownscale *downscale);
void avcencDownscaleColumns2(AvcencDownscale *downscale);
void avcencDownscaleColumns4(AvcencDownscale *downscale);
void avcencDownscaleColumns6(AvcencDownscale *downscale);
void avcencDownscaleEdgeRight(AvcencDownscale *downscale);
void avcencDownscaleEdgeRightTail(AvcencDownscale *downscale);
void avcencDownscaleVertical(AvcencDownscale *downscale, uint8_t *lines, uint32_t y);
void avcencDownscaleVerticalTail(AvcencDownscale *downscale, uint8_t *lines, uint32_t y);

/*
 * the coarse search, as its kernels keep it: an 8x8 block of the current
 * motion plane against nine lines of the previous one, 16 samples of each:
 * the eight left of the block's column and the eight at it. A line's
 * samples sit between the line above's, so a line down moves the odd lines
 * a sample to the right
 */
typedef struct AvcencSearch {
	uint8_t block[8][8];
	uint8_t window[9][16];
} AvcencSearch;

/* the choice of the block's vector: four candidates at a time, one a lane */
typedef struct AvcencChoice {
	int8_t candidates[4][2];	/* x and y */
	int8_t x;			/* the candidates', two more after each */
	int8_t y[4];
	int8_t last[2];			/* the last block's vector */
	int16_t lengthWeight;		/* of the vector's length */
	int16_t changeWeight;		/* of its change from the last one */
	int16_t cost[4];		/* each lane's lowest */
	int16_t sad[4];			/* and its SAD */
	uint16_t vector[4];		/* and its vector, x in the low byte */
} AvcencChoice;

/* a line of the window: 16 samples, or eight with the edge's to one side */
static inline void avcencSearchLine(AvcencSearch *search, uint32_t line, const uint8_t *samples)
{
	uint32_t i;

	for (i = 0; i < 16; i++)
		search->window[line][i] = samples[i];
}

static inline void avcencSearchLineLeft(AvcencSearch *search, uint32_t line,
	const uint8_t *samples)
{
	uint32_t i;

	for (i = 0; i < 8; i++) {
		search->window[line][i] = samples[0];
		search->window[line][8 + i] = samples[i];
	}
}

static inline void avcencSearchLineRight(AvcencSearch *search, uint32_t line,
	const uint8_t *samples)
{
	uint32_t i;

	for (i = 0; i < 8; i++) {
		search->window[line][i] = samples[i];
		search->window[line][8 + i] = samples[7];
	}
}

/* the search's kernels (avcenckernel.c) */
const uint8_t *avcencSearchTop(AvcencSearch *search, const uint8_t *window);
const uint8_t *avcencSearchTopLeft(AvcencSearch *search, const uint8_t *window);
const uint8_t *avcencSearchTopRight(AvcencSearch *search, const uint8_t *window);
void avcencSearchBlock64(AvcencSearch *search, const uint8_t *block);
void avcencSearchBlock128(AvcencSearch *search, const uint8_t *block);
void avcencSearchBlock192(AvcencSearch *search, const uint8_t *block);
void avcencSearchBlock256(AvcencSearch *search, const uint8_t *block);
void avcencSearchShift(AvcencSearch *search);
void avcencSearchWindow64(AvcencSearch *search, const uint8_t *window);
void avcencSearchWindowLeft64(AvcencSearch *search, const uint8_t *window);
void avcencSearchWindowRight64(AvcencSearch *search, const uint8_t *window);
void avcencSearchWindow128(AvcencSearch *search, const uint8_t *window);
void avcencSearchWindowLeft128(AvcencSearch *search, const uint8_t *window);
void avcencSearchWindowRight128(AvcencSearch *search, const uint8_t *window);
void avcencSearchWindow192(AvcencSearch *search, const uint8_t *window);
void avcencSearchWindowLeft192(AvcencSearch *search, const uint8_t *window);
void avcencSearchWindowRight192(AvcencSearch *search, const uint8_t *window);
void avcencSearchWindow256(AvcencSearch *search, const uint8_t *window);
void avcencSearchWindowLeft256(AvcencSearch *search, const uint8_t *window);
void avcencSearchWindowRight256(AvcencSearch *search, const uint8_t *window);
void avcencSearchLast(AvcencSearch *search, uint16_t *sads, int32_t offsets);
void avcencSearchPair(AvcencSearch *search, uint16_t *sads, int32_t offsets);
void avcencSearchChoose(AvcencChoice *choice, const int16_t *top, const int16_t *bottom);

/*
 * the intra decision's 4x4 candidates, as its kernels keep them: a row of
 * four blocks of the macroblock's source and its neighbours, two blocks
 * (a pair, the first or the second) at a time. Its predictions are the
 * standard's: vertical (0), horizontal (1), DC (2), diagonal down left (3)
 * and down right (4), all from the source's pixels
 */
typedef struct AvcencIntra {
	uint8_t lines[4][16];		/* the row's */
	uint8_t left[4][8];		/* the left macroblock's, their right halves */
	uint8_t top[16];		/* the line above the row */
	uint8_t topRight[8];		/* the line above, right of the macroblock */
	uint8_t edge[8];		/* block 1's top right in the first four, block 0's top left last */
	uint8_t columns[8];		/* a pair's left columns, bottom up, four each */
	uint8_t prediction[4][8];	/* a pair's, a line each */
} AvcencIntra;

/*
 * a pair's predictions the choice builds again: the DC and the diagonals'
 * filtered edges, by block, the down right one's corner at its fourth place
 */
typedef struct AvcencIntraPredictions {
	uint8_t dc[8];			/* four of each block's */
	uint8_t downLeft[2][8];
	uint8_t downRight[2][8];
} AvcencIntraPredictions;

/*
 * a row's candidates: the SADs by prediction (DC, horizontal, down left,
 * vertical, down right) and block; the predictions of the row above go
 * in the rows below, until those take their SADs
 */
typedef union AvcencIntraRow {
	uint16_t costs[5][4];
	AvcencIntraPredictions predictions;
	uint16_t chromaCosts[4][2];	/* the chroma decision's, the first row's place: by prediction, U and V */
} AvcencIntraRow;

#define INTRA_DC		0
#define INTRA_HORIZONTAL	1
#define INTRA_DOWN_LEFT		2
#define INTRA_VERTICAL		3
#define INTRA_DOWN_RIGHT	4

/* a cost no SAD reaches: the prediction has no neighbours */
#define INTRA_UNAVAILABLE	16383

/* the intra decision's kernels (avcenckernel.c) */
void avcencIntraLoad(AvcencIntra *intra, const uint8_t *lines, const uint8_t *top,
	int32_t topRight);
void avcencIntraLoadTop(AvcencIntra *intra, const uint8_t *lines, const uint8_t *top);
void avcencIntraLoadRow(AvcencIntra *intra, const uint8_t *lines, int32_t topRight);
void avcencIntraLoadRowAlone(AvcencIntra *intra, const uint8_t *lines, int32_t topRight);
void avcencIntraSatdFirst(const AvcencIntra *intra, uint16_t *satds);
void avcencIntraSatdSecond(const AvcencIntra *intra, uint16_t *satds);
void avcencIntraSadDcFirst(const AvcencIntra *intra, uint16_t *sads);
void avcencIntraSadDcSecond(const AvcencIntra *intra, uint16_t *sads);
void avcencIntraSadHorizontalFirst(const AvcencIntra *intra, uint16_t *sads);
void avcencIntraSadHorizontalSecond(const AvcencIntra *intra, uint16_t *sads);
void avcencIntraSadDownLeftFirst(const AvcencIntra *intra, uint16_t *sads);
void avcencIntraSadDownLeftSecond(const AvcencIntra *intra, uint16_t *sads);
void avcencIntraSadVerticalFirst(const AvcencIntra *intra, uint16_t *sads);
void avcencIntraSadVerticalSecond(const AvcencIntra *intra, uint16_t *sads);
void avcencIntraSadDownRightFirst(const AvcencIntra *intra, uint16_t *sads);
void avcencIntraSadDownRightSecond(const AvcencIntra *intra, uint16_t *sads);
uint32_t avcencIntraChosenCost(AvcencIntra *intra, const AvcencIntraRow *predictions,
	const uint8_t *modes);
void avcencIntraDcFirst(AvcencIntra *intra);
void avcencIntraDcFirstTop(AvcencIntra *intra);
void avcencIntraDcFirstLeft(AvcencIntra *intra);
void avcencIntraDcFirstAlone(AvcencIntra *intra);
void avcencIntraDcSecond(AvcencIntra *intra);
void avcencIntraDcSecondLeft(AvcencIntra *intra);
void avcencIntraHorizontalFirst(AvcencIntra *intra);
void avcencIntraHorizontalSecond(AvcencIntra *intra);
void avcencIntraVerticalFirst(AvcencIntra *intra);
void avcencIntraVerticalSecond(AvcencIntra *intra);
void avcencIntraDownLeftFirst(AvcencIntra *intra, AvcencIntraPredictions *predictions);
void avcencIntraDownLeftSecond(AvcencIntra *intra, AvcencIntraPredictions *predictions);
void avcencIntraDownRightFirst(AvcencIntra *intra, AvcencIntraPredictions *predictions);
void avcencIntraDownRightSecond(AvcencIntra *intra, AvcencIntraPredictions *predictions);
uint32_t avcencIntraChoose(AvcencIntra *intra, AvcencIntraRow *row, uint8_t *modes);
uint32_t avcencIntraChooseTwo(AvcencIntra *intra, AvcencIntraRow *row, uint8_t *modes);
uint8_t avcencIntraLuma(const AvcencIntraRow *rows);
uint8_t avcencIntraLumaTop(const AvcencIntraRow *rows);
uint8_t avcencIntraLumaLeft(const AvcencIntraRow *rows);
uint32_t avcencIntraVariance(const uint8_t *luma);

/*
 * the chroma intra decision's, as its kernels keep it: one 8x8 part (U or
 * V) of the macroblock's source and its neighbours. The DC and vertical
 * predictions are two lines, one for each half of the part, in the first
 * and fifth lines
 */
typedef struct AvcencChroma {
	uint8_t lines[8][8];
	uint8_t left[8];		/* the left macroblock's last column */
	uint8_t top[8];			/* the line above */
	uint8_t topLeft[8];		/* the line above to the left, its last the corner */
	uint8_t prediction[8][8];
} AvcencChroma;

/* the chroma candidates: the chroma modes are DC (0), horizontal (1), vertical (2) and plane (3) */
#define CHROMA_HORIZONTAL	0
#define CHROMA_DC		1
#define CHROMA_PLANE		2
#define CHROMA_VERTICAL		3

/* the chroma decision's kernels (avcenckernel.c) */
void avcencChromaLoad(AvcencChroma *chroma, const uint8_t *lines, const uint8_t *top);
void avcencChromaLoadTop(AvcencChroma *chroma, const uint8_t *lines, const uint8_t *top);
void avcencChromaLoadLeft(AvcencChroma *chroma, const uint8_t *lines);
uint16_t avcencChromaSad(const AvcencChroma *chroma);
uint16_t avcencChromaSadHalves(const AvcencChroma *chroma);
void avcencChromaDc(AvcencChroma *chroma);
void avcencChromaDcTop(AvcencChroma *chroma);
void avcencChromaDcLeft(AvcencChroma *chroma);
void avcencChromaVertical(AvcencChroma *chroma);
void avcencChromaPlane(AvcencChroma *chroma);
uint8_t avcencChromaChoose(const AvcencIntraRow *row);
uint8_t avcencChromaChooseTop(const AvcencIntraRow *row);
uint8_t avcencChromaChooseLeft(const AvcencIntraRow *row);

/*
 * the sub-pel refinement's, as its kernels keep it: an 8x8 block of the
 * source, the reference around its whole pixel position (from two pixels
 * left of and above it, its edges repeated outside the picture) and the half
 * pixels the candidates are made of
 */
typedef struct AvcencRefine {
	uint8_t window[12][16];
	uint8_t source[8][8];
	uint8_t half[9][9];		/* a row of half pixels, the left of the block's first */
	uint8_t vertical[9][12];	/* the half pixels above the window's lines 2-10 */
	uint16_t candidates[9][4];	/* the candidates' SADs, by block or partition */
} AvcencRefine;

/*
 * the candidates, around each block's whole pixel position: below left,
 * left, above left, above, above right, right, below right, below, and the
 * position itself. One out of the reference's reach costs REFINE_UNAVAILABLE
 */
#define REFINE_BELOW_LEFT	0
#define REFINE_LEFT		1
#define REFINE_ABOVE_LEFT	2
#define REFINE_ABOVE		3
#define REFINE_ABOVE_RIGHT	4
#define REFINE_RIGHT		5
#define REFINE_BELOW_RIGHT	6
#define REFINE_BELOW		7
#define REFINE_WHOLE		8
#define REFINE_CANDIDATES	9
#define REFINE_UNAVAILABLE	32767

/* the refinement's kernels (avcenckernel.c) */
void avcencRefineHorizontalRight(AvcencRefine *refine);
void avcencRefineDiagonalRight(AvcencRefine *refine);
void avcencRefineDiagonal(AvcencRefine *refine);
uint16_t avcencRefineDiagonalSadAbove(const AvcencRefine *refine);
uint16_t avcencRefineDiagonalSadBelow(const AvcencRefine *refine);
void avcencRefineVertical(AvcencRefine *refine, const uint8_t *saved);
uint16_t avcencRefineVerticalSadAbove(const AvcencRefine *refine);
uint16_t avcencRefineVerticalSadBelow(const AvcencRefine *refine);
void avcencRefineHorizontal(AvcencRefine *refine);
uint16_t avcencRefineHorizontalSad(const AvcencRefine *refine);
void avcencRefineEdgeLeft(AvcencRefine *refine, uint32_t count);
void avcencRefineEdgeLeftFar(AvcencRefine *refine, uint32_t count);
void avcencRefineEdgeRight(AvcencRefine *refine, uint32_t count);
void avcencRefineEdgeRightFar(AvcencRefine *refine, uint32_t count);
void avcencRefineEdgeTop(AvcencRefine *refine, int32_t line);
void avcencRefineEdgeBottom(AvcencRefine *refine, int32_t count);
void avcencRefineLoad(AvcencRefine *refine, const uint8_t *first, const uint8_t *second,
	const uint8_t *third, uint32_t offset, uint32_t line, int32_t nextRow);
void avcencRefineStore(const AvcencRefine *refine, uint8_t *saved);
void avcencRefineLoadSource(AvcencRefine *refine, const uint8_t *source);
uint16_t avcencRefineSad(const AvcencRefine *refine);
uint16_t avcencRefineSatd(const AvcencRefine *refine);
void avcencRefineMerge16x16(AvcencRefine *refine);
void avcencRefineMerge16x8(AvcencRefine *refine);
void avcencRefineMerge8x16(AvcencRefine *refine);
void avcencRefineChoose(const AvcencRefine *refine, const int8_t (*deltas)[2],
	int16_t (*vectors)[2]);

/*
 * the motion job's, as its kernels keep it: a macroblock's vectors, their
 * predictions and differences, each as four lanes (two blocks' x and y), the
 * neighbours the predictions are made of, and for the P_Skip check a band of
 * the reference's prediction and the transform's thresholds. A neighbour
 * loaded from a record has its vector's sign in its upper lanes
 */
typedef struct AvcencMotion {
	int16_t vectors[2][4];		/* blocks 0 and 1, 2 and 3, in quarter pixels */
	int16_t predictions[2][4];
	int16_t differences[2][4];	/* the second the last 8x8 macroblock's but for 8x8 */
	int16_t neighbours[2][3][4];	/* the first and the second block's left, above and corner */
	int16_t leftBelow[4];		/* the left macroblock's block 3, for block 2 */
	uint8_t unavailable;		/* by MOTION_LEFT, MOTION_ABOVE, MOTION_CORNER */
	uint16_t satd;			/* the blocks' costs */
	uint16_t bits[2];		/* the vectors' bits, of lanes 0 and 1, of lanes 2 and 3 */
	uint8_t luma[4][16];		/* a band of the prediction */
	uint8_t chroma[2][4][8];	/* U's and V's */
	int16_t thresholds[2][4];	/* the coefficients': rows 0 and 2, rows 1 and 3 */
} AvcencMotion;

#define MOTION_LEFT		0
#define MOTION_ABOVE		1
#define MOTION_CORNER		2

/* the motion job's kernels (avcenckernel.c) */
uint32_t avcencMotionModes(const uint8_t *above, const uint8_t *left, const uint8_t *modes,
	uint32_t first, uint32_t second, int8_t *codes);
void avcencMotionBits(AvcencMotion *motion);
void avcencMotionBits8x8(AvcencMotion *motion);
void avcencMotionCodeBits(AvcencMotion *motion);
void avcencMotionCodeBits8x8(AvcencMotion *motion);
void avcencMotionVectors(AvcencMotion *motion, const int16_t *vector, const int16_t (*vectors)[2],
	const uint16_t *costs);
void avcencMotionMedian(AvcencMotion *motion);
void avcencMotionMedianPairs(AvcencMotion *motion);
void avcencMotionMedianLower(AvcencMotion *motion);
void avcencMotionSkipMedian(AvcencMotion *motion, int16_t *vector);
uint32_t avcencMotionTransform(const AvcencMotion *motion, int32_t (*block)[4]);
void avcencMotionLoadLuma(AvcencMotion *motion, const uint8_t *first, const uint8_t *second,
	int32_t pitch, uint32_t line);
void avcencMotionLoadLumaShifted(AvcencMotion *motion, const uint8_t *first,
	const uint8_t *second, int32_t pitch, uint32_t line, uint32_t offset);
void avcencMotionEdgeLuma(AvcencMotion *motion, int32_t x, int32_t y, int32_t width,
	int32_t height);
uint32_t avcencMotionSkipLuma(const AvcencMotion *motion, const uint8_t *source);
void avcencMotionLoadChroma(AvcencMotion *motion, const uint8_t *first, int32_t pitch,
	uint32_t line);
void avcencMotionLoadChromaShifted(AvcencMotion *motion, const uint8_t *first,
	const uint8_t *second, int32_t pitch, uint32_t line, uint32_t offset);
void avcencMotionEdgeChroma(AvcencMotion *motion, int32_t x, int32_t y, int32_t width,
	int32_t height);
uint32_t avcencMotionSkipChroma(const AvcencMotion *motion, const uint8_t *source,
	uint32_t first);

/*
 * the deblocking's kernels (0x8a2208-0x8a3c00): the decoder's built again,
 * the same code (avcdeckernel.c) but the luma's last columns kept
 */
void avcencDeblockLoad(AvcDeblock *deblock, const uint8_t *samples);
void avcencDeblockStore(const AvcDeblock *deblock, uint8_t *samples);
void avcencDeblockStoreLeft(const AvcDeblock *deblock, uint8_t *samples);
void avcencDeblockKeepLeft(const AvcDeblock *deblock, const uint8_t *luma,
	uint8_t (*columns)[4][8]);
void avcencDeblockTurnLeft(AvcDeblock *deblock);
void avcencDeblockTurnRight(AvcDeblock *deblock);
uint32_t avcencDeblockEdge(AvcDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths);
uint32_t avcencDeblockEdgeRight(AvcDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths);
void avcencDeblockInner(AvcDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths);
void avcencDeblockInnerRight(AvcDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths);
void avcencDeblockMiddle(AvcDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths);
void avcencDeblockInnerLast(AvcDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths);
void avcencChromaDeblockLoad(AvcChromaDeblock *deblock, const uint8_t *samples);
void avcencChromaDeblockStore(const AvcChromaDeblock *deblock, uint8_t *samples);
void avcencChromaDeblockStoreLeft(const AvcChromaDeblock *deblock, uint8_t *samples);
void avcencChromaDeblockKeepLeft(const AvcChromaDeblock *deblock, uint8_t (*columns)[2][8]);
uint32_t avcencChromaDeblockTurn(AvcChromaDeblock *deblock);
uint32_t avcencChromaDeblockMask(const AvcChromaDeblock *deblock, uint32_t indexA,
	uint32_t indexB, uint32_t strengths, uint32_t inner);
uint32_t avcencChromaDeblockEdge(AvcChromaDeblock *deblock, uint32_t indexA,
	uint32_t indexB, uint32_t strengths);
uint32_t avcencChromaDeblockInner(AvcChromaDeblock *deblock, uint32_t indexA,
	uint32_t indexB, uint32_t strengths);

/* the luma prediction's kernels (0x8a3c30-0x8a4d18+): the decoder's, copied */
void avcencLoad8(AvcInter *inter, const uint8_t *address, const uint8_t *next, uint32_t y,
	uint32_t step, uint32_t shift);
void avcencLoad8Tall(AvcInter *inter, const uint8_t *address, const uint8_t *next,
	uint32_t y, uint32_t step, uint32_t shift);
void avcencLoad8Wide(AvcInter *inter, const uint8_t *address, const uint8_t *next,
	const uint8_t *third, uint32_t y, uint32_t step, uint32_t shift);
void avcencLoad8WideTall(AvcInter *inter, const uint8_t *address, const uint8_t *next,
	const uint8_t *third, uint32_t y, uint32_t step, uint32_t shift);
void avcencEdgeLeft8(AvcInter *inter, int32_t shift);
void avcencEdgeLeft8Tall(AvcInter *inter, int32_t shift);
void avcencEdgeLeft8Wide(AvcInter *inter, int32_t shift);
void avcencEdgeLeft8WideFar(AvcInter *inter, int32_t shift);
void avcencEdgeLeft8WideTall(AvcInter *inter, int32_t shift);
void avcencEdgeLeft8WideFarTall(AvcInter *inter, int32_t shift);
void avcencEdgeRight8(AvcInter *inter, int32_t shift);
void avcencEdgeRight8Tall(AvcInter *inter, int32_t shift);
void avcencEdgeRight8Wide(AvcInter *inter, int32_t shift);
void avcencEdgeRight8WideFar(AvcInter *inter, int32_t shift);
void avcencEdgeRight8WideTall(AvcInter *inter, int32_t shift);
void avcencEdgeRight8WideFarTall(AvcInter *inter, int32_t shift);
void avcencEdgeTop8(AvcInter *inter, int32_t y);
void avcencEdgeTop8Tall(AvcInter *inter, int32_t y);
void avcencEdgeBottom8(AvcInter *inter, int32_t below);
void avcencEdgeBottom8Tall(AvcInter *inter, int32_t below);
void avcencLumaVertical8(AvcInter *inter, uint32_t mode);
void avcencLumaHorizontal8(AvcInter *inter, uint32_t mode);
void avcencLumaCentre8(AvcInter *inter, uint32_t mode);

/*
 * The residual in the coprocessor: the quantisation's MF by row (c8 rows 0
 * and 2, c9 rows 1 and 3), its f (c22, c4) and qbits (c5); the
 * dequantisation's v (c17, c18) and shift (c16); a 4x4 block's levels or
 * residual (c12-c15); two 4x4 blocks side by side, the source (c24-c27) and
 * the prediction, then reconstruction (c28-c31).
 */
typedef struct AvcencResidual {
	int16_t quantScale[2][4];
	int32_t rounding;
	uint32_t quantShift;
	int16_t levelScale[2][4];
	uint32_t levelShift;
	int16_t block[4][4];
	uint8_t source[2][4][4];	/* the left block, the right */
	uint8_t predicted[2][4][4];
} AvcencResidual;

/* the residual's kernels (0x8a5170-0x8a6080) */
void avcencReconstructLeft(AvcencResidual *residual);
void avcencReconstructRight(AvcencResidual *residual);
void avcencStorePair(const AvcencResidual *residual, uint8_t *samples);
void avcencKeepRight(const uint8_t *samples, uint8_t *left);
uint32_t avcencLevelsPastOne(const AvcencResidual *residual);
uint32_t avcencResidualPair(AvcencResidual *residual, const uint8_t *predicted,
	const uint8_t *source, uint8_t *saved);
uint32_t avcencResidualRight(AvcencResidual *residual);
void avcencInverseLevels(AvcencResidual *residual);

/* an intra macroblock's kernels (avcenckernel.c) */
void avcencQuantise(AvcencResidual *residual);
void avcencDcTransform(int16_t (*dc)[4]);
void avcencIntra16Pair(AvcencResidual *residual, const uint8_t *source,
	const uint8_t *predicted);
void avcencIntra16Right(AvcencResidual *residual);
uint32_t avcencLevelsAny(const AvcencResidual *residual);
uint32_t avcencDcAny(const int16_t *dc);
void avcencDcIntra16(AvcRecon *recon, const int16_t *levels, uint32_t qp);
void avcencIdctAc(AvcRecon *recon, const int16_t *levels);
void avcencIdctDc(AvcRecon *recon);
void avcencIdct(AvcRecon *recon, const int16_t *levels);
void avcencIntraInverse(AvcRecon *recon, uint32_t levelShift, const int16_t *levelScale);
uint32_t avcencIntraResidual(AvcRecon *recon, const uint8_t *source, uint32_t quantShift,
	int32_t rounding, const int16_t *quantScale, uint32_t unused);
void avcencDcQuantSetup(AvcencResidual *residual, uint32_t quantShift, int32_t rounding,
	const int16_t *quantScale, uint32_t unused);
void avcencQuantiseDc(const AvcencResidual *residual, int16_t (*dc)[4]);
void avcencAddIntra0(AvcRecon *recon);
void avcencAddIntra1(AvcRecon *recon);
void avcencAddIntra2(AvcRecon *recon);
void avcencAddIntra3(AvcRecon *recon);
void avcencStoreIntra(const AvcRecon *recon, uint8_t *samples);
void avcencIntraNeighbours(AvcRecon *recon, const AvcEdges *edges, uint32_t aboveRight);
void avcencIntraNext(AvcRecon *recon, uint32_t block, const AvcEdges *edges);
void avcencIntra4x4Block0(AvcRecon *recon, uint32_t mode, uint32_t left, uint32_t above);
void avcencIntra4x4Block1(AvcRecon *recon, uint32_t mode, uint32_t above);
void avcencIntra4x4Block2(AvcRecon *recon, uint32_t mode, uint32_t left);
void avcencIntra4x4Block3(AvcRecon *recon, uint32_t mode);
void avcencIntraKeep(const AvcRecon *recon, const uint8_t *samples, AvcEdges *edges);
void avcencLoadIntra16(AvcRecon *recon, const uint8_t *samples);
void avcencAddIntra16Block0(AvcRecon *recon);
void avcencAddIntra16Block1(AvcRecon *recon);
void avcencAddIntra16Block2(AvcRecon *recon);
void avcencAddIntra16Block3(AvcRecon *recon);
void avcencStoreIntra16(const AvcRecon *recon, uint8_t *samples);
void avcencKeepRightIntra16(const AvcRecon *recon, uint8_t **right);
void avcencIntra16Vertical(const AvcEdges *edges, uint8_t *samples);
void avcencIntra16Horizontal(const uint8_t *left, uint8_t *samples);
void avcencIntra16Dc(const uint8_t *left, const AvcEdges *edges, uint32_t leftAvailable,
	uint32_t aboveAvailable, uint8_t *samples);
void avcencIntra16Plane(const uint8_t *left, const AvcEdges *edges, uint32_t aboveLeft,
	uint8_t *samples);

/*
 * The chroma residual in the coprocessor: the quantisation's MF by row
 * (c24 rows 0 and 2, c25 rows 1 and 3), f (c20) and qbits (c18), the DC
 * levels' MF(0, 0), 2f and qbits + 1 (c22, c21, c19); four rows of the
 * source (c8-c11) and of the prediction (c12-c15), Cb's or Cr's; a 4x4
 * block's coefficients or levels (c28-c31); the component's DC levels (c4).
 */
typedef struct AvcencChromaResidual {
	int16_t quantScale[2][4];
	int32_t rounding;
	uint32_t quantShift;
	int16_t dcScale;
	int32_t dcRounding;
	uint32_t dcShift;
	uint8_t source[4][8];
	uint8_t predicted[4][8];
	int16_t block[4][4];
	int16_t dc[4];
} AvcencChromaResidual;

/* the chroma's kernels (0x8a7308-0x8a8cb0) */
void avcencChromaMcLoad(AvcChromaInter *inter, const uint8_t *address, const uint8_t *next,
	uint32_t y, uint32_t step, uint32_t shift);
void avcencChromaMcEdgeLeft(AvcChromaInter *inter, int32_t shift);
void avcencChromaMcEdgeRight(AvcChromaInter *inter, int32_t shift);
void avcencChromaMcEdgeTop(AvcChromaInter *inter, int32_t y);
void avcencChromaMcEdgeBottom(AvcChromaInter *inter, int32_t below);
void avcencChromaMcPredict(AvcChromaInter *inter, uint32_t fraction);
void avcencChromaMcStore(const AvcChromaInter *inter, uint8_t *cb, uint8_t *cr);
void avcencChromaLoadRows(AvcChromaRecon *recon, const uint8_t *samples);
void avcencChromaLoadBottom(AvcChromaRecon *recon, const uint8_t *samples);
void avcencChromaResidual0(AvcencChromaResidual *residual, const uint8_t *source, const uint8_t *predicted);
void avcencChromaResidual2(AvcencChromaResidual *residual, const uint8_t *source, const uint8_t *predicted);
void avcencChromaResidual1(AvcencChromaResidual *residual);
void avcencChromaResidual3(AvcencChromaResidual *residual);
uint32_t avcencChromaLevelsAny(const AvcencChromaResidual *residual);
uint32_t avcencChromaDcAny(const AvcencChromaResidual *residual);
void avcencChromaQuantise(AvcencChromaResidual *residual);
void avcencChromaQuantiseDc(AvcencChromaResidual *residual);
void avcencChromaDcTransform(AvcencChromaResidual *residual);
void avcencChromaQuantSetup(AvcencChromaResidual *residual, const int16_t *quantScale, uint32_t quantShift,
	int32_t rounding);
uint32_t avcencChromaLevelsPastOne(const AvcencChromaResidual *residual);
void avcencChromaStoreLevels(const AvcencChromaResidual *residual, int16_t *levels);
void avcencChromaDcInverse(AvcChromaRecon *recon, const int16_t *levels, uint32_t qp);
void avcencChromaIdctDc(AvcChromaRecon *recon);
void avcencChromaIdct(AvcChromaRecon *recon, const int16_t *levels);
void avcencChromaStoreCb(const AvcChromaRecon *recon, uint8_t *samples);
void avcencChromaStoreCr(const AvcChromaRecon *recon, uint8_t *samples);
void avcencChromaStoreRows(const AvcChromaRecon *recon, uint8_t *samples);
void avcencChromaAddDcCb(AvcChromaRecon *recon, uint8_t *samples);
void avcencChromaAddDcCr(AvcChromaRecon *recon, uint8_t *samples);
void avcencChromaBlocksCb(AvcChromaRecon *recon);
void avcencChromaBlocksCr(AvcChromaRecon *recon);
void avcencChromaAddCb0(AvcChromaRecon *recon);
void avcencChromaAddCb1(AvcChromaRecon *recon);
void avcencChromaAddCb2(AvcChromaRecon *recon);
void avcencChromaAddCb3(AvcChromaRecon *recon);
void avcencChromaAddCr0(AvcChromaRecon *recon);
void avcencChromaAddCr1(AvcChromaRecon *recon);
void avcencChromaAddCr2(AvcChromaRecon *recon);
void avcencChromaAddCr3(AvcChromaRecon *recon);
void avcencChromaRowsCb(AvcChromaRecon *recon, uint8_t *samples);
void avcencChromaRowsCr(AvcChromaRecon *recon, uint8_t *samples);
void avcencChromaKeepBottom(const AvcChromaRecon *recon, AvcChromaEdges *edges);
void avcencChromaKeepRight(const AvcChromaRecon *recon, uint8_t (*left)[8]);
void avcencChromaIntraDc(AvcChromaRecon *recon, const uint8_t (*left)[8],
	const AvcChromaEdges *edges, uint32_t leftAvailable, uint32_t aboveAvailable);
void avcencChromaIntraHorizontal(AvcChromaRecon *recon, const uint8_t (*left)[8],
	const AvcChromaEdges *edges);
void avcencChromaIntraVertical(AvcChromaRecon *recon, const AvcChromaEdges *edges);
void avcencChromaIntraPlane(AvcChromaRecon *recon, const uint8_t (*left)[8],
	const AvcChromaEdges *edges);

/*
 * the inter decision's search result, as its kernels keep it: the lowest
 * cost (SAD and the vector's length) of each 8x8 block, of the 8x16
 * halves (left, right) and the 16x8 ones (top, bottom), and of the whole
 * macroblock, and the candidates that have them. The decision keeps them
 * in its info too
 */
typedef struct AvcencInterBest {
	uint16_t costs[8];		/* the blocks, then left, right, top, bottom */
	uint32_t whole[2];		/* the macroblock's, then the least length's */
	int8_t deltas[12][2];		/* the same order, in whole pixels, x and y */
} AvcencInterBest;

#define INTER_HALF_LEFT		4	/* the deltas' and costs' places */
#define INTER_HALF_RIGHT	5
#define INTER_HALF_TOP		6
#define INTER_HALF_BOTTOM	7
#define INTER_WHOLE		8

/* a cost no SAD reaches: the neighbour is out of the window */
#define INTER_UNAVAILABLE	16383

/*
 * the window's bands, as their kernels keep them while they bring them in:
 * eight lines of the reference, 24 pixels each
 */
typedef struct AvcencInterBand {
	uint8_t lines[8][24];
} AvcencInterBand;

/*
 * the search, as its kernels keep it: eight lines of a half of the
 * macroblock, a parity's pixels of each, alternating (the even pixels of
 * the even lines, or the odd ones), and ten lines of the window, the first
 * 16 bytes of each
 */
typedef struct AvcencInterSearch {
	uint8_t source[8][8];
	uint8_t lines[10][16];
} AvcencInterSearch;

/*
 * the skip check's: four lines of the reference at the vector, 16 pixels,
 * or four of each chroma part's, eight; and the thresholds of the residual's
 * coefficients, the first and third lines' then the second and fourth's
 */
typedef struct AvcencInterSkip {
	uint8_t luma[4][16];
	uint8_t chroma[2][4][8];	/* U, then V */
	int16_t thresholds[2][4];
} AvcencInterSkip;

/* the inter decision's kernels (0x8dd958-0x8df4d0) */
void avcencInterBest(const uint16_t *sads, AvcencInterBest *best, int8_t (*deltas)[2]);
void avcencInterPartitionCosts(const AvcencInterBest *best, uint32_t weight, uint16_t *costs,
	uint16_t *sads);
void avcencInterPosition(const uint8_t *vector, uint32_t x, uint32_t y, uint32_t width,
	uint32_t height, int16_t *position, int16_t *origin);
void avcencInterBandStore(const AvcencInterBand *band, uint8_t (*window)[24]);
void avcencInterSkipLoad(AvcencInterSkip *skip, const uint8_t (*lines)[24], uint32_t offset,
	uint32_t parity);
int32_t avcencInterSkipTransform(int16_t (*lines)[4], const int16_t *thresholds);
int32_t avcencInterSkipLuma(const AvcencInterSkip *skip, const uint8_t *source);
void avcencInterSkipChroma(AvcencInterSkip *skip, const uint8_t *tile, uint32_t pitch,
	uint32_t line);
void avcencInterSkipChromaShifted(AvcencInterSkip *skip, const uint8_t *tile,
	const uint8_t *next, uint32_t pitch, uint32_t line, uint32_t shift);
void avcencInterSkipChromaEdges(AvcencInterSkip *skip, int32_t x, int32_t y, int32_t width,
	int32_t height);
int32_t avcencInterSkipChromaCheck(const AvcencInterSkip *skip, const uint8_t *source,
	int32_t left);
void avcencInterBandLeft4(AvcencInterBand *band);
void avcencInterBandLeft8(AvcencInterBand *band);
void avcencInterBandLeft12(AvcencInterBand *band);
void avcencInterBandRight4(AvcencInterBand *band);
void avcencInterBandRight8(AvcencInterBand *band);
void avcencInterBandRight12(AvcencInterBand *band);
void avcencInterBandTop4(AvcencInterBand *band);
void avcencInterBandTop8(AvcencInterBand *band);
void avcencInterBandBottom4(AvcencInterBand *band);
void avcencInterBandBottom8(AvcencInterBand *band);
void avcencInterBandLoad(AvcencInterBand *band, const uint8_t *first, const uint8_t *second,
	const uint8_t *third, const uint8_t *fourth);
void avcencInterBandLoadShifted(AvcencInterBand *band, const uint8_t *first,
	const uint8_t *second, const uint8_t *third, const uint8_t *fourth);
void avcencInterSearchSource(AvcencInterSearch *search, const uint8_t *luma);
void avcencInterSearchSourceOther(AvcencInterSearch *search, const uint8_t *luma);
void avcencInterSearchLines(AvcencInterSearch *search, const uint8_t *window);
void avcencInterSearchShift(AvcencInterSearch *search, const uint8_t *window);
void avcencInterSearchNext(AvcencInterSearch *search, const uint8_t *window);
void avcencInterNeighboursEven(const AvcencInterSearch *search, uint16_t *sads, uint32_t offset);
void avcencInterNeighboursOdd(const AvcencInterSearch *search, uint16_t *sads, uint32_t offset);
void avcencInterSearchRows(const AvcencInterSearch *search, uint16_t *sads);
void avcencInterSearchLastRow(const AvcencInterSearch *search, uint16_t *sads);

/*
 * a macroblock's bS as its kernels hand them on, by register: two edges of
 * four 4-sample groups each, the vertical edges 0-1 and 2-3, then the
 * horizontal ones
 */
typedef struct AvcencStrengthLanes {
	uint8_t coded[4][8];		/* where coefficients are, and next to intra (c16-c19) */
	uint8_t edges[4][8];		/* and where the vectors differ (c12-c15) */
} AvcencStrengthLanes;

/* the strengths' kernels (0x8df4d0-0x8df640) */
void avcencStrengthsCoded(AvcencStrengthLanes *lanes, uint32_t vertical, uint32_t horizontal,
	uint32_t leftIntra, uint32_t topIntra);
void avcencStrengthsVectors(AvcencStrengthLanes *lanes, const int16_t (*current)[2],
	const int16_t (*left)[2], const int16_t (*above)[2]);

#endif
