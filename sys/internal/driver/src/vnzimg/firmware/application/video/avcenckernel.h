/*
 * The AVC encoder's kernels: the state they keep in the IVC2 registers
 */
#ifndef AVCENC_KERNEL_H
#define AVCENC_KERNEL_H

#include <stdint.h>

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

#endif
