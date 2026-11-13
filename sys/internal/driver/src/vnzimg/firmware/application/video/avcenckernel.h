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

#endif
