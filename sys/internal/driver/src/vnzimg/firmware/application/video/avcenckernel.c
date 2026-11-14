/*
 * The AVC encoder's kernels (0x89cfe0-0x8a2208): VLIW code its jobs call,
 * keeping their state in the IVC2 coprocessor's registers (here an
 * AvcencDownscale). Each one is written as what it does to its lanes; the
 * downscale's kernels pass sums from one to the next in the accumulators,
 * which here is each kernel's columns computed whole.
 */
#include "avcenckernel.h"

/* the horizontal filter's taps (2048 is one): a low pass for every fourth pixel */
static const int16_t horizontalTaps[15] = {
	-1, 8, 36, 91, 167, 247, 310, 332, 310, 247, 167, 91, 36, 8, -1,
};

/* the vertical filter's (512 is one) */
static const uint8_t verticalTaps[7] = { 59, 73, 81, 86, 81, 73, 59 };

static inline uint8_t clampByte(int32_t value)
{
	if (value < 0)
		return 0;
	if (value > 255)
		return 255;
	return value;
}

/* eight more pixels of each line, the oldest eight dropped */
static inline void loadGroup(AvcencDownscale *downscale, const uint8_t *source)
{
	uint32_t line, i;

	for (line = 0; line < 8; line++) {
		for (i = 0; i < 16; i++)
			downscale->pixels[line][i] = downscale->pixels[line][i + 8];
		for (i = 0; i < 8; i++)
			downscale->pixels[line][16 + i] = source[line * 16 + i];
	}
}

/* 0x89cfe0: the left half of a tile's eight lines */
void avcencDownscaleLoadLeft(AvcencDownscale *downscale, const uint8_t *source)
{
	loadGroup(downscale, source);
}

/* 0x89d028: the right half */
void avcencDownscaleLoadRight(AvcencDownscale *downscale, const uint8_t *source)
{
	loadGroup(downscale, source);
}

/* 0x89d070: the picture's left edge: the first pixel to the left of it */
void avcencDownscaleEdgeLeft(AvcencDownscale *downscale)
{
	uint32_t line, i;

	for (line = 0; line < 8; line++)
		for (i = 0; i < 8; i++)
			downscale->pixels[line][8 + i] = downscale->pixels[line][16];
}

/* the horizontal filter at a pixel of the loaded groups */
static inline uint8_t horizontalSample(const uint8_t *pixels, uint32_t center)
{
	int32_t sum = 1024;
	uint32_t i;

	for (i = 0; i < 15; i++)
		sum += horizontalTaps[i] * pixels[center - 7 + i];
	return clampByte(sum >> 11);
}

/* two columns, at the middle group's first and fifth pixels */
static inline void columnsPair(AvcencDownscale *downscale, uint32_t column)
{
	uint32_t line;

	for (line = 0; line < 8; line++) {
		downscale->columns[line][column] = horizontalSample(downscale->pixels[line], 8);
		downscale->columns[line][column + 1] = horizontalSample(downscale->pixels[line], 12);
	}
}

/* 0x89d120: the block's columns 0 and 1 (pixels 0 and 4) */
void avcencDownscaleColumns0(AvcencDownscale *downscale)
{
	columnsPair(downscale, 0);
}

/* 0x89d220: columns 2 and 3 */
void avcencDownscaleColumns2(AvcencDownscale *downscale)
{
	columnsPair(downscale, 2);
}

/* 0x89d318: columns 4 and 5 */
void avcencDownscaleColumns4(AvcencDownscale *downscale)
{
	columnsPair(downscale, 4);
}

/* 0x89d410: columns 6 and 7 */
void avcencDownscaleColumns6(AvcencDownscale *downscale)
{
	columnsPair(downscale, 6);
}

/* the picture's right edge: its last pixel to the right of it */
static inline void edgeRight(AvcencDownscale *downscale)
{
	uint32_t line, i;
	uint8_t last;

	for (line = 0; line < 8; line++) {
		last = downscale->pixels[line][23];
		for (i = 0; i < 16; i++)
			downscale->pixels[line][i] = downscale->pixels[line][i + 8];
		for (i = 0; i < 8; i++)
			downscale->pixels[line][16 + i] = last;
	}
}

/* 0x89d4b0: columns 6 and 7 of the picture's last block */
void avcencDownscaleEdgeRight(AvcencDownscale *downscale)
{
	edgeRight(downscale);
	columnsPair(downscale, 6);
}

/* 0x89d520: columns 2 and 3 of a last block of 16 pixels */
void avcencDownscaleEdgeRightTail(AvcencDownscale *downscale)
{
	edgeRight(downscale);
	columnsPair(downscale, 2);
}

/* the vertical filter at a line of the columns, the lines above it the last band's */
static inline uint8_t verticalSample(uint8_t columns[8][8], uint8_t above[3][8],
	int32_t center, uint32_t column)
{
	int32_t sum = 256, line;
	uint32_t i;

	for (i = 0; i < 7; i++) {
		line = center - 3 + i;
		sum += verticalTaps[i] * (line < 0 ? above[line + 3][column] : columns[line][column]);
	}
	return clampByte(sum >> 9);
}

/*
 * the vertical pass over the block's columns: the lines above them are the
 * last band's (the first band's own first line), and its last three lines
 * are kept for the next band
 */
static inline void vertical(AvcencDownscale *downscale, uint8_t columns[8][8], uint8_t *lines,
	uint32_t y)
{
	uint8_t above[3][8];
	uint32_t i, column;

	for (i = 0; i < 3; i++)
		for (column = 0; column < 8; column++)
			above[i][column] = y ? lines[i * 8 + column] : columns[0][column];
	for (i = 0; i < 3; i++)
		for (column = 0; column < 8; column++)
			lines[i * 8 + column] = columns[5 + i][column];
	for (i = 0; i < 4; i++) {
		downscale->samples[i] = verticalSample(columns, above, 0, 2 * i);
		downscale->samples[4 + i] = verticalSample(columns, above, 4, 2 * i + 1);
	}
}

/* 0x89d590: a block of 32 pixels */
void avcencDownscaleVertical(AvcencDownscale *downscale, uint8_t *lines, uint32_t y)
{
	vertical(downscale, downscale->columns, lines, y);
}

/*
 * 0x89d6c0: a last block of 16 pixels, its four columns. The registers hold
 * two lines each, the second's four columns where the others would be
 */
void avcencDownscaleVerticalTail(AvcencDownscale *downscale, uint8_t *lines, uint32_t y)
{
	uint8_t columns[8][8];
	uint32_t line, column;

	for (line = 0; line < 8; line++) {
		for (column = 0; column < 4; column++) {
			columns[line][column] = downscale->columns[line][column];
			columns[line][4 + column] = downscale->columns[line | 1][column];
		}
	}
	vertical(downscale, columns, lines, y);
}

/* 0x89d780: the picture's top: its first line for all eight */
const uint8_t *avcencSearchTop(AvcencSearch *search, const uint8_t *window)
{
	uint32_t line;

	for (line = 0; line < 8; line++)
		avcencSearchLine(search, line, window);
	return window;
}

/* 0x89d7d0: and its left edge */
const uint8_t *avcencSearchTopLeft(AvcencSearch *search, const uint8_t *window)
{
	uint32_t line;

	for (line = 0; line < 8; line++)
		avcencSearchLineLeft(search, line, window);
	return window;
}

/* 0x89d820: and its right edge */
const uint8_t *avcencSearchTopRight(AvcencSearch *search, const uint8_t *window)
{
	uint32_t line;

	for (line = 0; line < 8; line++)
		avcencSearchLineRight(search, line, window);
	return window;
}

/* the block of the current plane, read four samples at a time */
static inline void searchBlock(AvcencSearch *search, const uint8_t *block, uint32_t pitch)
{
	uint32_t line, i;

	for (line = 0; line < 8; line++)
		for (i = 0; i < 8; i++)
			search->block[line][i] = block[line * pitch + i];
}

/* 0x89d870: the plane 64 samples wide */
void avcencSearchBlock64(AvcencSearch *search, const uint8_t *block)
{
	searchBlock(search, block, 64);
}

/* 0x89d900 */
void avcencSearchBlock128(AvcencSearch *search, const uint8_t *block)
{
	searchBlock(search, block, 128);
}

/* 0x89d990 */
void avcencSearchBlock192(AvcencSearch *search, const uint8_t *block)
{
	searchBlock(search, block, 192);
}

/* 0x89da20: and the widest, 256 */
void avcencSearchBlock256(AvcencSearch *search, const uint8_t *block)
{
	searchBlock(search, block, 256);
}

/*
 * 0x89dab0: two lines down: the eighth line comes up to the seventh place
 * and the eighth stays, for the next ones to replace
 */
void avcencSearchShift(AvcencSearch *search)
{
	uint32_t line, i;

	for (line = 0; line < 6; line++)
		for (i = 0; i < 16; i++)
			search->window[line][i] = search->window[line + 2][i];
	for (i = 0; i < 16; i++)
		search->window[6][i] = search->window[8][i];
}

/* the window's first eight lines, of the plane or with an edge */
static inline void searchWindow(AvcencSearch *search, const uint8_t *window, uint32_t pitch)
{
	uint32_t line;

	for (line = 0; line < 8; line++)
		avcencSearchLine(search, line, window + line * pitch);
}

static inline void searchWindowLeft(AvcencSearch *search, const uint8_t *window,
	uint32_t pitch)
{
	uint32_t line;

	for (line = 0; line < 8; line++)
		avcencSearchLineLeft(search, line, window + line * pitch);
}

static inline void searchWindowRight(AvcencSearch *search, const uint8_t *window,
	uint32_t pitch)
{
	uint32_t line;

	for (line = 0; line < 8; line++)
		avcencSearchLineRight(search, line, window + line * pitch);
}

/* 0x89dae8 */
void avcencSearchWindow64(AvcencSearch *search, const uint8_t *window)
{
	searchWindow(search, window, 64);
}

/* 0x89db70 */
void avcencSearchWindowLeft64(AvcencSearch *search, const uint8_t *window)
{
	searchWindowLeft(search, window, 64);
}

/* 0x89dbc8 */
void avcencSearchWindowRight64(AvcencSearch *search, const uint8_t *window)
{
	searchWindowRight(search, window, 64);
}

/* 0x89dc20 */
void avcencSearchWindow128(AvcencSearch *search, const uint8_t *window)
{
	searchWindow(search, window, 128);
}

/* 0x89dca8 */
void avcencSearchWindowLeft128(AvcencSearch *search, const uint8_t *window)
{
	searchWindowLeft(search, window, 128);
}

/* 0x89dd00 */
void avcencSearchWindowRight128(AvcencSearch *search, const uint8_t *window)
{
	searchWindowRight(search, window, 128);
}

/* 0x89dd58 */
void avcencSearchWindow192(AvcencSearch *search, const uint8_t *window)
{
	searchWindow(search, window, 192);
}

/* 0x89dde0 */
void avcencSearchWindowLeft192(AvcencSearch *search, const uint8_t *window)
{
	searchWindowLeft(search, window, 192);
}

/* 0x89de38 */
void avcencSearchWindowRight192(AvcencSearch *search, const uint8_t *window)
{
	searchWindowRight(search, window, 192);
}

/* 0x89de90 */
void avcencSearchWindow256(AvcencSearch *search, const uint8_t *window)
{
	searchWindow(search, window, 256);
}

/* 0x89df18 */
void avcencSearchWindowLeft256(AvcencSearch *search, const uint8_t *window)
{
	searchWindowLeft(search, window, 256);
}

/* 0x89df70 */
void avcencSearchWindowRight256(AvcencSearch *search, const uint8_t *window)
{
	searchWindowRight(search, window, 256);
}

/*
 * the block's SAD at a place of the window: its lines against the window's
 * from the first, the even ones that many samples in, the odd ones the other
 */
static uint16_t searchSad(const AvcencSearch *search, uint32_t first, uint32_t even,
	uint32_t odd)
{
	uint32_t line, i, offset;
	uint16_t sad = 0;

	for (line = 0; line < 8; line++) {
		offset = line & 1 ? odd : even;
		for (i = 0; i < 8; i++) {
			int32_t difference = search->block[line][i] -
				search->window[first + line][offset + i];
			sad += difference < 0 ? -difference : difference;
		}
	}
	return sad;
}

/*
 * 0x89dfc8: the SADs at the window's first line, from no sample in to the
 * offsets', one row of the map (20 apart) each
 */
void avcencSearchLast(AvcencSearch *search, uint16_t *sads, int32_t offsets)
{
	int32_t offset;

	for (offset = 0; offset <= offsets; offset++)
		sads[offset * 20] = searchSad(search, 0, offset, offset);
}

/*
 * 0x89e0b8: and at the first two lines, the second's four places on. A line
 * down puts the odd lines a sample further in; the shift takes the low three
 * bits of the count, so past the eighth sample they start again at the first
 */
void avcencSearchPair(AvcencSearch *search, uint16_t *sads, int32_t offsets)
{
	int32_t offset;

	for (offset = 0; offset <= offsets; offset++) {
		sads[offset * 20] = searchSad(search, 0, offset, offset);
		sads[offset * 20 + 4] = searchSad(search, 1, offset, offset % 8 + 1);
	}
}

static inline int16_t saturate16(int32_t value)
{
	if (value > 32767)
		return 32767;
	if (value < -32768)
		return -32768;
	return value;
}

/*
 * 0x89e238: a candidate a lane: its cost is the vector's length and its
 * change from the last block's, weighted, plus the SAD of the block's two
 * halves; the lowest is kept, of equal ones the later. The next candidates
 * are two samples to the right
 */
void avcencSearchChoose(AvcencChoice *choice, const int16_t *top, const int16_t *bottom)
{
	int32_t lane, x, y;
	int16_t length, change, cost, sad;

	for (lane = 0; lane < 4; lane++) {
		x = choice->candidates[lane][0];
		y = choice->candidates[lane][1];
		length = (int8_t)(x < 0 ? -x : x) + (int8_t)(y < 0 ? -y : y);
		x -= choice->last[0];
		y -= choice->last[1];
		change = (int8_t)(x < 0 ? -x : x) + (int8_t)(y < 0 ? -y : y);
		cost = length * choice->lengthWeight + change * choice->changeWeight;
		sad = saturate16(top[lane] + bottom[lane]);
		cost = saturate16(cost + sad);
		if (choice->cost[lane] >= cost) {
			choice->cost[lane] = cost;
			choice->sad[lane] = sad;
			choice->vector[lane] = (uint8_t)choice->candidates[lane][0] |
				(uint8_t)choice->candidates[lane][1] << 8;
		}
	}
	choice->x += 2;
	for (lane = 0; lane < 4; lane++) {
		choice->candidates[lane][0] = choice->x;
		choice->candidates[lane][1] = choice->y[lane];
	}
}

/*
 * The intra decision's kernels (0x89e280-0x89f2b0), on an AvcencIntra: a
 * row of four 4x4 blocks of the macroblock's source, a pair of them at a
 * time, the first (columns 0-7) or the second (8-15). Each prediction's
 * lines are four bytes of each block of the pair.
 */

/* the line above's place in the macroblock above, and the macroblocks beside it */
#define MB_SIZE		256

static inline int32_t abs32(int32_t value)
{
	return value < 0 ? -value : value;
}

/* 0x89e280: the first row, with its neighbours above and to the left */
void avcencIntraLoad(AvcencIntra *intra, const uint8_t *lines, const uint8_t *top,
	int32_t topRight)
{
	const uint8_t *aboveLeft = top - MB_SIZE + 8;
	uint32_t line, i;

	for (i = 0; i < 16; i++)
		intra->top[i] = top[i];
	for (i = 0; i < 8; i++)
		intra->topRight[i] = topRight ? top[MB_SIZE + i] : top[15];
	for (i = 0; i < 4; i++) {
		intra->edge[i] = top[8 + i];
		intra->edge[4 + i] = aboveLeft[4 + i];
	}
	for (line = 0; line < 4; line++) {
		for (i = 0; i < 8; i++)
			intra->left[line][i] = lines[line * 16 - MB_SIZE + 8 + i];
		for (i = 0; i < 16; i++)
			intra->lines[line][i] = lines[line * 16 + i];
	}
}

/* 0x89e340: the first row, with its neighbours above only (and above to the right) */
void avcencIntraLoadTop(AvcencIntra *intra, const uint8_t *lines, const uint8_t *top)
{
	uint32_t line, i;

	for (i = 0; i < 16; i++)
		intra->top[i] = top[i];
	for (i = 0; i < 8; i++) {
		intra->topRight[i] = top[MB_SIZE + i];
		intra->edge[i] = top[8 + i];
	}
	for (line = 0; line < 4; line++)
		for (i = 0; i < 16; i++)
			intra->lines[line][i] = lines[line * 16 + i];
}

/*
 * the row below's top: the last line, with nothing to the right of the
 * macroblock; block 1's top right only for the third row
 */
static void loadBelow(AvcencIntra *intra, int32_t topRight)
{
	uint32_t i;

	for (i = 0; i < 16; i++)
		intra->top[i] = intra->lines[3][i];
	for (i = 0; i < 8; i++)
		intra->topRight[i] = intra->lines[3][15];
	for (i = 0; i < 4; i++)
		intra->edge[i] = topRight ? intra->lines[3][8 + i] : intra->lines[3][7];
}

/* 0x89e3a8: the next row, with the left macroblock's */
void avcencIntraLoadRow(AvcencIntra *intra, const uint8_t *lines, int32_t topRight)
{
	uint32_t line, i;

	loadBelow(intra, topRight);
	for (i = 4; i < 8; i++)
		intra->edge[i] = intra->left[3][i];
	for (line = 0; line < 4; line++) {
		for (i = 0; i < 8; i++)
			intra->left[line][i] = lines[line * 16 - MB_SIZE + 8 + i];
		for (i = 0; i < 16; i++)
			intra->lines[line][i] = lines[line * 16 + i];
	}
}

/* 0x89e438: the next row, without a left macroblock */
void avcencIntraLoadRowAlone(AvcencIntra *intra, const uint8_t *lines, int32_t topRight)
{
	uint32_t line, i;

	loadBelow(intra, topRight);
	for (i = 4; i < 8; i++)
		intra->edge[i] = topRight ? intra->lines[3][8 + i] : intra->lines[3][7];
	for (line = 0; line < 4; line++)
		for (i = 0; i < 16; i++)
			intra->lines[line][i] = lines[line * 16 + i];
}

/*
 * a pair's SATDs: the sum of the absolute coefficients of the 4x4
 * Hadamard transform of each block's residual, halved
 */
static void satdPair(const AvcencIntra *intra, uint32_t column, uint16_t *satds)
{
	int32_t residual[4][4], sum, block, line, i, a, b, c, d;

	for (block = 0; block < 2; block++) {
		for (line = 0; line < 4; line++)
			for (i = 0; i < 4; i++)
				residual[line][i] = intra->lines[line][column + block * 4 + i] -
					intra->prediction[line][block * 4 + i];
		for (i = 0; i < 4; i++) {
			a = residual[0][i] + residual[2][i];
			b = residual[1][i] + residual[3][i];
			c = residual[0][i] - residual[2][i];
			d = residual[1][i] - residual[3][i];
			residual[0][i] = a + b;
			residual[1][i] = c + d;
			residual[2][i] = c - d;
			residual[3][i] = a - b;
		}
		sum = 0;
		for (line = 0; line < 4; line++) {
			a = residual[line][0] + residual[line][2];
			b = residual[line][1] + residual[line][3];
			c = residual[line][0] - residual[line][2];
			d = residual[line][1] - residual[line][3];
			sum += abs32(a + b) + abs32(a - b) + abs32(c + d) + abs32(c - d);
		}
		satds[block] = (uint16_t)sum >> 1;
	}
}

/* 0x89e4a0 */
void avcencIntraSatdFirst(const AvcencIntra *intra, uint16_t *satds)
{
	satdPair(intra, 0, satds);
}

/* 0x89e5c8 */
void avcencIntraSatdSecond(const AvcencIntra *intra, uint16_t *satds)
{
	satdPair(intra, 8, satds);
}

/* a pair's SADs against its prediction's lines, or its first line for all four */
static void sadPair(const AvcencIntra *intra, uint32_t column, int32_t flat, uint16_t *sads)
{
	uint32_t block, line, i, sum;
	int32_t difference;

	for (block = 0; block < 2; block++) {
		sum = 0;
		for (line = 0; line < 4; line++) {
			for (i = 0; i < 4; i++) {
				difference = intra->lines[line][column + block * 4 + i] -
					intra->prediction[flat ? 0 : line][block * 4 + i];
				sum += abs32(difference);
			}
		}
		sads[block] = sum;
	}
}

/* 0x89e6f0 */
void avcencIntraSadDcFirst(const AvcencIntra *intra, uint16_t *sads)
{
	sadPair(intra, 0, 1, sads);
}

/* 0x89e708 */
void avcencIntraSadDcSecond(const AvcencIntra *intra, uint16_t *sads)
{
	sadPair(intra, 8, 1, sads);
}

/* 0x89e720 */
void avcencIntraSadHorizontalFirst(const AvcencIntra *intra, uint16_t *sads)
{
	sadPair(intra, 0, 0, sads);
}

/* 0x89e770 */
void avcencIntraSadHorizontalSecond(const AvcencIntra *intra, uint16_t *sads)
{
	sadPair(intra, 8, 0, sads);
}

/* 0x89e7c0 */
void avcencIntraSadDownLeftFirst(const AvcencIntra *intra, uint16_t *sads)
{
	sadPair(intra, 0, 0, sads);
}

/* 0x89e7d8 */
void avcencIntraSadDownLeftSecond(const AvcencIntra *intra, uint16_t *sads)
{
	sadPair(intra, 8, 0, sads);
}

/* 0x89e7f0 */
void avcencIntraSadVerticalFirst(const AvcencIntra *intra, uint16_t *sads)
{
	sadPair(intra, 0, 1, sads);
}

/* 0x89e840 */
void avcencIntraSadVerticalSecond(const AvcencIntra *intra, uint16_t *sads)
{
	sadPair(intra, 8, 1, sads);
}

/* 0x89e890 */
void avcencIntraSadDownRightFirst(const AvcencIntra *intra, uint16_t *sads)
{
	sadPair(intra, 0, 0, sads);
}

/* 0x89e8c8 */
void avcencIntraSadDownRightSecond(const AvcencIntra *intra, uint16_t *sads)
{
	sadPair(intra, 8, 0, sads);
}

/* a block's prediction again, by its mode, from the pair's predictions kept */
static void chosenPrediction(AvcencIntra *intra, const AvcencIntraPredictions *predictions,
	uint32_t column, uint32_t block, uint8_t mode)
{
	uint32_t line, i, x = column + block * 4;
	uint8_t left;

	for (line = 0; line < 4; line++) {
		left = x ? intra->lines[line][x - 1] : intra->left[line][7];
		for (i = 0; i < 4; i++) {
			if (mode == 0)
				intra->prediction[line][block * 4 + i] = intra->top[x + i];
			else if (mode == 1)
				intra->prediction[line][block * 4 + i] = left;
			else if (mode == 2)
				intra->prediction[line][block * 4 + i] = predictions->dc[block * 4 + i];
			else if (mode == 3)
				intra->prediction[line][block * 4 + i] =
					predictions->downLeft[block][line + i];
			else
				intra->prediction[line][block * 4 + i] =
					predictions->downRight[block][3 - line + i];
		}
	}
}

/*
 * 0x89e900: the row's SATD with the modes chosen (blocks 0 and 1 in the
 * first two, 2 and 3 four further), the pairs' predictions kept in the
 * two rows below
 */
uint32_t avcencIntraChosenCost(AvcencIntra *intra, const AvcencIntraRow *predictions,
	const uint8_t *modes)
{
	uint16_t first[2], second[2];

	chosenPrediction(intra, &predictions[0].predictions, 0, 0, modes[0]);
	chosenPrediction(intra, &predictions[0].predictions, 0, 1, modes[1]);
	avcencIntraSatdFirst(intra, first);
	chosenPrediction(intra, &predictions[1].predictions, 8, 0, modes[4]);
	chosenPrediction(intra, &predictions[1].predictions, 8, 1, modes[5]);
	avcencIntraSatdSecond(intra, second);
	return first[0] + first[1] + second[0] + second[1];
}

static inline void flatPrediction(AvcencIntra *intra, uint8_t first, uint8_t second)
{
	uint32_t i;

	for (i = 0; i < 4; i++) {
		intra->prediction[0][i] = first;
		intra->prediction[0][4 + i] = second;
	}
}

/* the sum of four of the pair's left columns (bottom up), or the top's from a place */
static inline uint32_t sum4(const uint8_t *values)
{
	return values[0] + values[1] + values[2] + values[3];
}

/* a column of the row's lines, bottom up */
static inline void lineColumn(const AvcencIntra *intra, uint32_t x, uint8_t *column)
{
	uint32_t line;

	for (line = 0; line < 4; line++)
		column[line] = intra->lines[3 - line][x];
}

/* 0x89ec08: the first pair's DC, with the top and the left */
void avcencIntraDcFirst(AvcencIntra *intra)
{
	uint32_t line;

	for (line = 0; line < 4; line++)
		intra->columns[line] = intra->left[3 - line][7];
	lineColumn(intra, 3, &intra->columns[4]);
	flatPrediction(intra, (sum4(&intra->top[0]) + sum4(&intra->columns[0]) + 4) >> 3,
		(sum4(&intra->top[4]) + sum4(&intra->columns[4]) + 4) >> 3);
}

/* 0x89ec60: the first pair's DC, without a left macroblock */
void avcencIntraDcFirstTop(AvcencIntra *intra)
{
	lineColumn(intra, 3, &intra->columns[0]);
	lineColumn(intra, 3, &intra->columns[4]);
	flatPrediction(intra, (sum4(&intra->top[0]) + 2) >> 2,
		(sum4(&intra->top[4]) + sum4(&intra->columns[4]) + 4) >> 3);
}

/* 0x89ecb0: the first pair's DC, with nothing above */
void avcencIntraDcFirstLeft(AvcencIntra *intra)
{
	uint8_t left[4], column[4];
	uint32_t line;

	for (line = 0; line < 4; line++)
		left[line] = intra->left[line][7];
	lineColumn(intra, 3, column);
	flatPrediction(intra, (sum4(left) + 2) >> 2, (sum4(column) + 2) >> 2);
}

/* 0x89ecf8: the first pair's DC, with nothing above or to the left */
void avcencIntraDcFirstAlone(AvcencIntra *intra)
{
	uint8_t column[4];

	lineColumn(intra, 3, column);
	flatPrediction(intra, 128, (sum4(column) + 2) >> 2);
}

/* 0x89ed30: the second pair's DC */
void avcencIntraDcSecond(AvcencIntra *intra)
{
	lineColumn(intra, 7, &intra->columns[0]);
	lineColumn(intra, 11, &intra->columns[4]);
	flatPrediction(intra, (sum4(&intra->top[8]) + sum4(&intra->columns[0]) + 4) >> 3,
		(sum4(&intra->top[12]) + sum4(&intra->columns[4]) + 4) >> 3);
}

/* 0x89ed88: the second pair's DC, with nothing above */
void avcencIntraDcSecondLeft(AvcencIntra *intra)
{
	uint8_t first[4], second[4];

	lineColumn(intra, 7, first);
	lineColumn(intra, 11, second);
	flatPrediction(intra, (sum4(first) + 2) >> 2, (sum4(second) + 2) >> 2);
}

/* the horizontal prediction of a pair, from the pixels left of each block */
static void horizontalPair(AvcencIntra *intra, uint32_t column)
{
	uint32_t line, i;
	uint8_t first, second;

	for (line = 0; line < 4; line++) {
		first = column ? intra->lines[line][column - 1] : intra->left[line][7];
		second = intra->lines[line][column + 3];
		for (i = 0; i < 4; i++) {
			intra->prediction[line][i] = first;
			intra->prediction[line][4 + i] = second;
		}
	}
}

/* 0x89edd0 */
void avcencIntraHorizontalFirst(AvcencIntra *intra)
{
	horizontalPair(intra, 0);
}

/* 0x89ede8 */
void avcencIntraHorizontalSecond(AvcencIntra *intra)
{
	horizontalPair(intra, 8);
}

/* 0x89ee00 */
void avcencIntraVerticalFirst(AvcencIntra *intra)
{
	uint32_t i;

	for (i = 0; i < 8; i++)
		intra->prediction[0][i] = intra->top[i];
}

/* 0x89ee08 */
void avcencIntraVerticalSecond(AvcencIntra *intra)
{
	uint32_t i;

	for (i = 0; i < 8; i++)
		intra->prediction[0][i] = intra->top[8 + i];
}

/* the diagonals' filter: (a + 2b + c + 2) / 4 along an edge of ten */
static void filterEdge(const uint8_t *edge, uint8_t *filtered)
{
	uint32_t i;

	for (i = 0; i < 8; i++)
		filtered[i] = (edge[i] + 2 * edge[i + 1] + edge[i + 2] + 2) >> 2;
}

/*
 * the diagonal down left prediction of a pair: each block's top and top
 * right filtered, a line one further along each line down
 */
static void downLeftPair(AvcencIntra *intra, AvcencIntraPredictions *predictions,
	uint8_t (*edges)[10])
{
	uint32_t block, line, i;

	for (block = 0; block < 2; block++) {
		filterEdge(edges[block], predictions->downLeft[block]);
		for (line = 0; line < 4; line++)
			for (i = 0; i < 4; i++)
				intra->prediction[line][block * 4 + i] =
					predictions->downLeft[block][line + i];
	}
}

/* 0x89ee10: past each block's top right, its last pixel and another */
void avcencIntraDownLeftFirst(AvcencIntra *intra, AvcencIntraPredictions *predictions)
{
	uint8_t edges[2][10];
	uint32_t i;

	for (i = 0; i < 8; i++)
		edges[0][i] = intra->top[i];
	edges[0][8] = intra->top[7];
	edges[0][9] = intra->top[0];
	for (i = 0; i < 4; i++) {
		edges[1][i] = intra->top[4 + i];
		edges[1][4 + i] = intra->edge[i];
	}
	edges[1][8] = intra->edge[3];
	edges[1][9] = intra->edge[4];
	downLeftPair(intra, predictions, edges);
}

/* 0x89ee60 */
void avcencIntraDownLeftSecond(AvcencIntra *intra, AvcencIntraPredictions *predictions)
{
	uint8_t edges[2][10];
	uint32_t i;

	for (i = 0; i < 8; i++)
		edges[0][i] = intra->top[8 + i];
	edges[0][8] = intra->top[15];
	edges[0][9] = intra->top[8];
	for (i = 0; i < 4; i++) {
		edges[1][i] = intra->top[12 + i];
		edges[1][4 + i] = intra->topRight[i];
	}
	edges[1][8] = intra->topRight[3];
	edges[1][9] = intra->topRight[4];
	downLeftPair(intra, predictions, edges);
}

/*
 * the diagonal down right prediction of a pair: each block's left column
 * (bottom up), its top left and its top filtered, a line one back each
 * line down
 */
static void downRightPair(AvcencIntra *intra, AvcencIntraPredictions *predictions,
	uint8_t (*edges)[10])
{
	uint32_t block, line, i;

	for (block = 0; block < 2; block++) {
		filterEdge(edges[block], predictions->downRight[block]);
		for (line = 0; line < 4; line++)
			for (i = 0; i < 4; i++)
				intra->prediction[line][block * 4 + i] =
					predictions->downRight[block][3 - line + i];
	}
}

/* a pair's edges: block 0's left, top left and five above; block 1's, past its top */
static void downRightEdges(const AvcencIntra *intra, uint32_t column, uint8_t corner,
	uint8_t (*edges)[10])
{
	uint32_t i;

	for (i = 0; i < 4; i++) {
		edges[0][i] = intra->columns[i];
		edges[1][i] = intra->columns[4 + i];
		edges[1][4 + i] = intra->top[column + 3 + i];
	}
	edges[0][4] = corner;
	for (i = 0; i < 5; i++)
		edges[0][5 + i] = intra->top[column + i];
	edges[1][8] = intra->top[column + 7];
	edges[1][9] = intra->top[column];
}

/* 0x89eeb0 */
void avcencIntraDownRightFirst(AvcencIntra *intra, AvcencIntraPredictions *predictions)
{
	uint8_t edges[2][10];

	downRightEdges(intra, 0, intra->edge[7], edges);
	downRightPair(intra, predictions, edges);
}

/* 0x89ef08 */
void avcencIntraDownRightSecond(AvcencIntra *intra, AvcencIntraPredictions *predictions)
{
	uint8_t edges[2][10];

	downRightEdges(intra, 8, intra->top[7], edges);
	downRightPair(intra, predictions, edges);
}

/*
 * 0x89ef60: each block's mode, the lowest SAD's (DC on a tie, then
 * vertical, horizontal, down left and down right), into the record's
 * places of the row's blocks (0 and 1, then 2 and 3 four further); the
 * row's SATD with them
 */
uint32_t avcencIntraChoose(AvcencIntra *intra, AvcencIntraRow *row, uint8_t *modes)
{
	static const uint8_t order[5] = {
		INTRA_DC, INTRA_VERTICAL, INTRA_HORIZONTAL, INTRA_DOWN_LEFT, INTRA_DOWN_RIGHT,
	};
	static const uint8_t mode[5] = { 2, 1, 3, 0, 4 };	/* by candidate */
	static const uint8_t place[4] = { 0, 1, 4, 5 };
	uint32_t block, i;
	int16_t best;

	for (block = 0; block < 4; block++) {
		best = row->costs[INTRA_DC][block];
		modes[place[block]] = mode[INTRA_DC];
		for (i = 1; i < 5; i++) {
			if ((int16_t)row->costs[order[i]][block] < best) {
				best = row->costs[order[i]][block];
				modes[place[block]] = mode[order[i]];
			}
		}
	}
	return avcencIntraChosenCost(intra, row + 1, modes);
}

/* 0x89f020: DC or horizontal only, for a row with nothing above */
uint32_t avcencIntraChooseTwo(AvcencIntra *intra, AvcencIntraRow *row, uint8_t *modes)
{
	static const uint8_t place[4] = { 0, 1, 4, 5 };
	uint32_t block;

	for (block = 0; block < 4; block++)
		modes[place[block]] = (int16_t)row->costs[INTRA_HORIZONTAL][block] <
			(int16_t)row->costs[INTRA_DC][block] ? 1 : 2;
	return avcencIntraChosenCost(intra, row + 1, modes);
}

/* the sum of a candidate's SADs over the macroblock's sixteen blocks */
static int32_t candidateSum(const AvcencIntraRow *rows, uint32_t candidate)
{
	int32_t sum = 0;
	uint32_t row, block;

	for (row = 0; row < 4; row++)
		for (block = 0; block < 4; block++)
			sum += rows[row].costs[candidate][block];
	return sum;
}

/*
 * 0x89f0a0: the 16x16 prediction, by its 4x4 blocks' SADs: vertical (0),
 * horizontal (1) or DC (2), DC on a tie
 */
uint8_t avcencIntraLuma(const AvcencIntraRow *rows)
{
	int32_t best, sum;
	uint8_t mode;

	best = candidateSum(rows, INTRA_DC);
	mode = 2;
	sum = candidateSum(rows, INTRA_VERTICAL);
	if (sum < best) {
		best = sum;
		mode = 0;
	}
	if (candidateSum(rows, INTRA_HORIZONTAL) < best)
		mode = 1;
	return mode;
}

/* 0x89f128: vertical or DC, without a left macroblock */
uint8_t avcencIntraLumaTop(const AvcencIntraRow *rows)
{
	return candidateSum(rows, INTRA_VERTICAL) < candidateSum(rows, INTRA_DC) ? 0 : 2;
}

/* 0x89f188: horizontal or DC, with nothing above */
uint8_t avcencIntraLumaLeft(const AvcencIntraRow *rows)
{
	return candidateSum(rows, INTRA_HORIZONTAL) < candidateSum(rows, INTRA_DC) ? 1 : 2;
}

/* 0x89f1e8: the macroblock's luma variance, (256 * sum of squares - sum squared) / 65536 */
uint32_t avcencIntraVariance(const uint8_t *luma)
{
	uint32_t sum = 0, squares = 0, i;

	for (i = 0; i < 256; i++) {
		sum += luma[i];
		squares += luma[i] * luma[i];
	}
	return (squares * 256 - sum * sum) >> 16;
}

/*
 * The chroma intra decision's kernels (0x89f2b8-0x89f760), on an
 * AvcencChroma: an 8x8 part of the source, U or V. The left macroblock's
 * part is 128 bytes back, the one above 'top' less its last line.
 */
#define CHROMA_MB_SIZE	128

static void chromaLines(AvcencChroma *chroma, const uint8_t *lines)
{
	uint32_t line, i;

	for (line = 0; line < 8; line++)
		for (i = 0; i < 8; i++)
			chroma->lines[line][i] = lines[line * 8 + i];
}

/* the left column, and the horizontal prediction from it */
static void chromaLeft(AvcencChroma *chroma, const uint8_t *lines)
{
	uint32_t line, i;

	for (line = 0; line < 8; line++) {
		chroma->left[line] = lines[line * 8 - CHROMA_MB_SIZE + 7];
		for (i = 0; i < 8; i++)
			chroma->prediction[line][i] = chroma->left[line];
	}
}

/* 0x89f2b8: with the neighbours above and to the left */
void avcencChromaLoad(AvcencChroma *chroma, const uint8_t *lines, const uint8_t *top)
{
	uint32_t i;

	for (i = 0; i < 8; i++) {
		chroma->topLeft[i] = top[i - CHROMA_MB_SIZE];
		chroma->top[i] = top[i];
	}
	chromaLeft(chroma, lines);
	chromaLines(chroma, lines);
}

/* 0x89f360: with the line above only */
void avcencChromaLoadTop(AvcencChroma *chroma, const uint8_t *lines, const uint8_t *top)
{
	uint32_t i;

	for (i = 0; i < 8; i++)
		chroma->top[i] = top[i];
	chromaLines(chroma, lines);
}

/* 0x89f3b0: with the left macroblock's only */
void avcencChromaLoadLeft(AvcencChroma *chroma, const uint8_t *lines)
{
	chromaLeft(chroma, lines);
	chromaLines(chroma, lines);
}

/* the part's SAD against the prediction's lines, or its halves' first lines */
static uint16_t chromaSad(const AvcencChroma *chroma, int32_t halves)
{
	uint32_t line, i, sum = 0;
	int32_t difference;

	for (line = 0; line < 8; line++) {
		for (i = 0; i < 8; i++) {
			difference = chroma->lines[line][i] -
				chroma->prediction[halves ? line & 4 : line][i];
			sum += abs32(difference);
		}
	}
	return sum;
}

/* 0x89f440 */
uint16_t avcencChromaSad(const AvcencChroma *chroma)
{
	return chromaSad(chroma, 0);
}

/* 0x89f468 */
uint16_t avcencChromaSadHalves(const AvcencChroma *chroma)
{
	return chromaSad(chroma, 1);
}

/* the DC prediction's halves: four of each 4x4 block's value */
static void chromaDcLines(AvcencChroma *chroma, uint8_t topLeft, uint8_t topRight,
	uint8_t bottomLeft, uint8_t bottomRight)
{
	uint32_t i;

	for (i = 0; i < 4; i++) {
		chroma->prediction[0][i] = topLeft;
		chroma->prediction[0][4 + i] = topRight;
		chroma->prediction[4][i] = bottomLeft;
		chroma->prediction[4][4 + i] = bottomRight;
	}
}

/*
 * 0x89f4c8: the standard's DC: each 4x4 block's from the top and the left,
 * the top right one's from the top only, the bottom left one's from the
 * left only
 */
void avcencChromaDc(AvcencChroma *chroma)
{
	chromaDcLines(chroma,
		(sum4(&chroma->top[0]) + sum4(&chroma->left[0]) + 4) >> 3,
		(sum4(&chroma->top[4]) + 2) >> 2,
		(sum4(&chroma->left[4]) + 2) >> 2,
		(sum4(&chroma->top[4]) + sum4(&chroma->left[4]) + 4) >> 3);
}

/* 0x89f520: from the top only */
void avcencChromaDcTop(AvcencChroma *chroma)
{
	uint8_t left = (sum4(&chroma->top[0]) + 2) >> 2;
	uint8_t right = (sum4(&chroma->top[4]) + 2) >> 2;

	chromaDcLines(chroma, left, right, left, right);
}

/* 0x89f550: from the left only */
void avcencChromaDcLeft(AvcencChroma *chroma)
{
	uint8_t top = (sum4(&chroma->left[0]) + 2) >> 2;
	uint8_t bottom = (sum4(&chroma->left[4]) + 2) >> 2;

	chromaDcLines(chroma, top, top, bottom, bottom);
}

/* 0x89f580 */
void avcencChromaVertical(AvcencChroma *chroma)
{
	uint32_t i;

	for (i = 0; i < 8; i++) {
		chroma->prediction[0][i] = chroma->top[i];
		chroma->prediction[4][i] = chroma->top[i];
	}
}

/*
 * 0x89f588: the standard's plane prediction: the gradients H and V from
 * the line above and the column left about the corner, b = (34 H + 32) / 64,
 * c likewise, a = 16 (the top's last + the left's last)
 */
void avcencChromaPlane(AvcencChroma *chroma)
{
	uint8_t corner = chroma->topLeft[7];
	int32_t h = 0, v = 0, a, b, c, i, line;

	for (i = 0; i < 4; i++) {
		h += (i + 1) * (chroma->top[4 + i] - (i < 3 ? chroma->top[2 - i] : corner));
		v += (i + 1) * (chroma->left[4 + i] - (i < 3 ? chroma->left[2 - i] : corner));
	}
	a = 16 * (chroma->top[7] + chroma->left[7]);
	b = (17 * h + 16) >> 5;
	c = (17 * v + 16) >> 5;
	for (line = 0; line < 8; line++)
		for (i = 0; i < 8; i++)
			chroma->prediction[line][i] = clampByte((a + b * (i - 3) +
				c * (line - 3) + 16) >> 5);
}

/* a candidate's SADs, U and V together */
static inline int16_t chromaCost(const AvcencIntraRow *row, uint32_t candidate)
{
	return row->chromaCosts[candidate][0] + row->chromaCosts[candidate][1];
}

/* 0x89f6d8: the lowest of the four, DC on a tie, then horizontal, vertical and plane */
uint8_t avcencChromaChoose(const AvcencIntraRow *row)
{
	int16_t best = chromaCost(row, CHROMA_DC), cost;
	uint8_t mode = 0;

	cost = chromaCost(row, CHROMA_HORIZONTAL);
	if (cost < best) {
		best = cost;
		mode = 1;
	}
	cost = chromaCost(row, CHROMA_VERTICAL);
	if (cost < best) {
		best = cost;
		mode = 2;
	}
	if (chromaCost(row, CHROMA_PLANE) < best)
		mode = 3;
	return mode;
}

/* 0x89f728: DC or vertical, without a left macroblock */
uint8_t avcencChromaChooseTop(const AvcencIntraRow *row)
{
	return chromaCost(row, CHROMA_VERTICAL) < chromaCost(row, CHROMA_DC) ? 2 : 0;
}

/* 0x89f760: DC or horizontal, with nothing above */
uint8_t avcencChromaChooseLeft(const AvcencIntraRow *row)
{
	return chromaCost(row, CHROMA_HORIZONTAL) < chromaCost(row, CHROMA_DC) ? 1 : 0;
}

/*
 * The sub-pel refinement's kernels (0x89f818-0x8a1480). A half pixel is the
 * four tap filter (-1, 5, 5, -1) / 8 of the pixels around it, rounded and
 * clamped; a diagonal one is the filter across the vertical half pixels.
 */
static inline void copyPixels(uint8_t *to, const uint8_t *from, uint32_t count)
{
	uint32_t i;

	for (i = 0; i < count; i++)
		to[i] = from[i];
}

static inline uint8_t halfPixel(int32_t a, int32_t b, int32_t c, int32_t d)
{
	return clampByte((-a + 5 * b + 5 * c - d + 4) >> 3);
}

/* the next half pixels across: the plane's right of the block's pixels */
static void halfShift(AvcencRefine *refine, uint32_t lines)
{
	uint32_t line, i;

	for (line = 0; line < lines; line++)
		for (i = 0; i < 8; i++)
			refine->half[line][i] = refine->half[line][i + 1];
}

/* 0x89f818: the horizontal half pixels right of the block's */
void avcencRefineHorizontalRight(AvcencRefine *refine)
{
	halfShift(refine, 8);
}

/* 0x89f858: the diagonal ones */
void avcencRefineDiagonalRight(AvcencRefine *refine)
{
	halfShift(refine, 9);
}

/* 0x89f8a0: the diagonal half pixels, across the vertical ones, left of the block's */
void avcencRefineDiagonal(AvcencRefine *refine)
{
	uint32_t line, i;

	for (line = 0; line < 9; line++)
		for (i = 0; i < 9; i++)
			refine->half[line][i] = halfPixel(refine->vertical[line][i],
				refine->vertical[line][i + 1], refine->vertical[line][i + 2],
				refine->vertical[line][i + 3]);
}

/* the block's SAD against eight lines of the half pixels, from a line */
static uint16_t halfSad(const AvcencRefine *refine, uint32_t first)
{
	uint32_t line, i, sum = 0;

	for (line = 0; line < 8; line++)
		for (i = 0; i < 8; i++)
			sum += abs32(refine->source[line][i] - refine->half[first + line][i]);
	return sum;
}

/* 0x89fa68: above the block's lines */
uint16_t avcencRefineDiagonalSadAbove(const AvcencRefine *refine)
{
	return halfSad(refine, 0);
}

/* 0x89fb48: below them */
uint16_t avcencRefineDiagonalSadBelow(const AvcencRefine *refine)
{
	return halfSad(refine, 1);
}

/* 0x89fc28: the vertical half pixels, from the window as it was stored */
void avcencRefineVertical(AvcencRefine *refine, const uint8_t *saved)
{
	uint32_t line, i;
	const uint8_t *left = saved, *right = saved + 96;
	uint8_t pixels[12][12];

	for (line = 0; line < 12; line++)
		for (i = 0; i < 12; i++)
			pixels[line][i] = i < 8 ? left[line * 8 + i] : right[line * 8 + i - 8];
	for (line = 0; line < 9; line++)
		for (i = 0; i < 12; i++)
			refine->vertical[line][i] = halfPixel(pixels[line][i], pixels[line + 1][i],
				pixels[line + 2][i], pixels[line + 3][i]);
}

/* the block's SAD against the vertical half pixels, from a line */
static uint16_t verticalSad(const AvcencRefine *refine, uint32_t first)
{
	uint32_t line, i, sum = 0;

	for (line = 0; line < 8; line++)
		for (i = 0; i < 8; i++)
			sum += abs32(refine->source[line][i] - refine->vertical[first + line][2 + i]);
	return sum;
}

/* 0x89fee8 */
uint16_t avcencRefineVerticalSadAbove(const AvcencRefine *refine)
{
	return verticalSad(refine, 0);
}

/* 0x8a0040 */
uint16_t avcencRefineVerticalSadBelow(const AvcencRefine *refine)
{
	return verticalSad(refine, 1);
}

/* 0x8a01a0: the horizontal half pixels of the block's lines, left of its pixels */
void avcencRefineHorizontal(AvcencRefine *refine)
{
	uint32_t line, i;
	const uint8_t *pixels;

	for (line = 0; line < 8; line++) {
		pixels = refine->window[2 + line];
		for (i = 0; i < 9; i++)
			refine->half[line][i] = halfPixel(pixels[i], pixels[i + 1], pixels[i + 2],
				pixels[i + 3]);
	}
}

/* 0x8a0328 */
uint16_t avcencRefineHorizontalSad(const AvcencRefine *refine)
{
	return halfSad(refine, 0);
}

/* the window moved right by a count of pixels, the first repeated */
static void windowRight(AvcencRefine *refine, uint32_t count)
{
	uint32_t line;
	int32_t i;

	for (line = 0; line < 12; line++)
		for (i = 15; i >= 0; i--)
			refine->window[line][i] = refine->window[line][i < (int32_t)count ? 0 :
				i - count];
}

/* 0x8a0408: the block left of the picture by less than eight */
void avcencRefineEdgeLeft(AvcencRefine *refine, uint32_t count)
{
	windowRight(refine, count);
}

/* 0x8a0498: by eight to twelve */
void avcencRefineEdgeLeftFar(AvcencRefine *refine, uint32_t count)
{
	windowRight(refine, count);
}

/* the window moved left, the last pixel repeated */
static void windowLeft(AvcencRefine *refine, uint32_t count)
{
	uint32_t line, i;

	for (line = 0; line < 12; line++)
		for (i = 0; i < 16; i++)
			refine->window[line][i] = refine->window[line][i + count > 15 ? 15 :
				i + count];
}

/* 0x8a0528: right of it by up to eight */
void avcencRefineEdgeRight(AvcencRefine *refine, uint32_t count)
{
	windowLeft(refine, count);
}

/* 0x8a05b8: by nine to fifteen */
void avcencRefineEdgeRightFar(AvcencRefine *refine, uint32_t count)
{
	windowLeft(refine, count);
}

/* 0x8a0648: above the picture: the lines moved down, the first repeated, until the line is 0 */
void avcencRefineEdgeTop(AvcencRefine *refine, int32_t line)
{
	int32_t i;

	for (; line < 0; line++)
		for (i = 11; i > 0; i--)
			copyPixels(refine->window[i], refine->window[i - 1], 16);
}

/* 0x8a06b8: below it: moved up a count of lines, the last repeated */
void avcencRefineEdgeBottom(AvcencRefine *refine, int32_t count)
{
	int32_t i;

	for (; count > 0; count--)
		for (i = 0; i < 11; i++)
			copyPixels(refine->window[i], refine->window[i + 1], 16);
}

/*
 * 0x8a0728: the window's twelve lines from the reference's tiles: sixteen
 * pixels of each from the three eight pixel columns given, from an offset
 * (1 to 8) into the first. The lines go down the tile from a line of it and
 * on into the tile below
 */
void avcencRefineLoad(AvcencRefine *refine, const uint8_t *first, const uint8_t *second,
	const uint8_t *third, uint32_t offset, uint32_t line, int32_t nextRow)
{
	uint32_t i, j, n;
	uint8_t pixels[24];

	for (i = 0; i < 12; i++) {
		if (line + i == 16) {
			first += nextRow;
			second += nextRow;
			third += nextRow;
		}
		for (j = 0; j < 8; j++) {
			pixels[j] = first[j];
			pixels[8 + j] = second[j];
			pixels[16 + j] = third[j];
		}
		for (n = 0; n < 16; n++)
			refine->window[i][n] = pixels[offset + n];
		first += 16;
		second += 16;
		third += 16;
	}
}

/* 0x8a0f20: the window's left halves, then its right halves */
void avcencRefineStore(const AvcencRefine *refine, uint8_t *saved)
{
	uint32_t line;

	for (line = 0; line < 12; line++) {
		copyPixels(saved + line * 8, refine->window[line], 8);
		copyPixels(saved + 96 + line * 8, refine->window[line] + 8, 8);
	}
}

/* 0x8a0ff0: the source's block, from its macroblock's lines */
void avcencRefineLoadSource(AvcencRefine *refine, const uint8_t *source)
{
	uint32_t line;

	for (line = 0; line < 8; line++)
		copyPixels(refine->source[line], source + line * 16, 8);
}

/* 0x8a1040: the SAD at the whole pixel position */
uint16_t avcencRefineSad(const AvcencRefine *refine)
{
	uint32_t line, i, sum = 0;

	for (line = 0; line < 8; line++)
		for (i = 0; i < 8; i++)
			sum += abs32(refine->source[line][i] - refine->window[2 + line][2 + i]);
	return sum;
}

/* 0x8a10b0: its SATD: the 4x4 blocks' Hadamard sums, saturated, halved */
uint16_t avcencRefineSatd(const AvcencRefine *refine)
{
	int32_t residual[4][4], sum, block, line, i, a, b, c, d;

	sum = 0;
	for (block = 0; block < 4; block++) {
		for (line = 0; line < 4; line++)
			for (i = 0; i < 4; i++)
				residual[line][i] = refine->source[(block & 2) * 2 + line]
					[(block & 1) * 4 + i] -
					refine->window[2 + (block & 2) * 2 + line]
					[2 + (block & 1) * 4 + i];
		for (i = 0; i < 4; i++) {
			a = residual[0][i] + residual[2][i];
			b = residual[1][i] + residual[3][i];
			c = residual[0][i] - residual[2][i];
			d = residual[1][i] - residual[3][i];
			residual[0][i] = a + b;
			residual[1][i] = c + d;
			residual[2][i] = c - d;
			residual[3][i] = a - b;
		}
		for (line = 0; line < 4; line++) {
			a = residual[line][0] + residual[line][2];
			b = residual[line][1] + residual[line][3];
			c = residual[line][0] - residual[line][2];
			d = residual[line][1] - residual[line][3];
			sum += abs32(a + b) + abs32(a - b) + abs32(c + d) + abs32(c - d);
		}
	}
	if (sum > 32767)
		sum = 32767;
	return sum >> 1;
}

/* two costs added, saturating */
static inline uint16_t addCosts(uint32_t a, uint32_t b)
{
	return a + b > REFINE_UNAVAILABLE ? REFINE_UNAVAILABLE : a + b;
}

/* each candidate's costs merged into the partitions': the two blocks of each into the first */
static void mergeCandidates(AvcencRefine *refine, uint32_t first0, uint32_t second0,
	uint32_t first1, uint32_t second1)
{
	uint32_t i;
	uint16_t *costs;

	for (i = 0; i < REFINE_CANDIDATES; i++) {
		costs = refine->candidates[i];
		costs[0] = addCosts(costs[first0], costs[second0]);
		costs[1] = addCosts(costs[first1], costs[second1]);
		costs[2] = 0;
		costs[3] = 0;
	}
}

/* 0x8a1188: one 16x16 partition: each candidate's four blocks' costs */
void avcencRefineMerge16x16(AvcencRefine *refine)
{
	uint32_t i;
	uint16_t *costs;

	for (i = 0; i < REFINE_CANDIDATES; i++) {
		costs = refine->candidates[i];
		costs[0] = addCosts(addCosts(costs[0], costs[1]), addCosts(costs[2], costs[3]));
		costs[1] = 0;
		costs[2] = 0;
		costs[3] = 0;
	}
}

/* 0x8a1248: two 16x8, the top blocks and the bottom ones */
void avcencRefineMerge16x8(AvcencRefine *refine)
{
	mergeCandidates(refine, 0, 1, 2, 3);
}

/* 0x8a1320: two 8x16, the left blocks and the right ones */
void avcencRefineMerge8x16(AvcencRefine *refine)
{
	mergeCandidates(refine, 0, 2, 1, 3);
}

/* the candidates' offsets, in quarter pixels */
static const int8_t refineOffsets[REFINE_CANDIDATES][2] = {
	{ -2, 2 }, { -2, 0 }, { -2, -2 }, { 0, -2 }, { 2, -2 }, { 2, 0 }, { 2, 2 }, { 0, 2 },
	{ 0, 0 },
};

/*
 * 0x8a13f8: each partition's cheapest candidate, the later on a tie, and
 * its vector: its whole pixel offset in quarter pixels and the candidate's
 */
void avcencRefineChoose(const AvcencRefine *refine, const int8_t (*deltas)[2],
	int16_t (*vectors)[2])
{
	uint32_t part, i, best;

	for (part = 0; part < 4; part++) {
		best = 0;
		for (i = 1; i < REFINE_CANDIDATES; i++)
			if (refine->candidates[best][part] >= refine->candidates[i][part])
				best = i;
		vectors[part][0] = deltas[part][0] * 4 + refineOffsets[best][0];
		vectors[part][1] = deltas[part][1] * 4 + refineOffsets[best][1];
	}
}

/*
 * the 4x4 blocks' neighbours, in decoding order: the block above each and
 * the one left of it, by its index in the macroblock, less 16 for the
 * macroblock above's or the left one's
 */
static const int8_t aboveBlocks[16] = {
	-6, -5, 0, 1, -2, -1, 4, 5, 2, 3, 8, 9, 6, 7, 12, 13,
};
static const int8_t leftBlocks[16] = {
	-11, 0, -9, 2, 1, 4, 3, 6, -3, 8, -1, 10, 9, 12, 11, 14,
};

/*
 * 0x8a14b8: the 4x4 blocks' modes coded: each one's prediction is the
 * smaller of its neighbours', DC (2) for one whose bit is set in the masks
 * (the first eight blocks', the last eight's). A mode equal to it is coded
 * -128, one above it less one. The modes predicted are counted
 */
uint32_t avcencMotionModes(const uint8_t *above, const uint8_t *left, const uint8_t *modes,
	uint32_t first, uint32_t second, int8_t *codes)
{
	uint32_t block, matches = 0, mask, upper, beside, predicted, mode;

	for (block = 0; block < 16; block++) {
		mask = block < 8 ? first >> block : second >> (block - 8);
		upper = aboveBlocks[block] < 0 ? above[16 + aboveBlocks[block]] :
			modes[aboveBlocks[block]];
		beside = leftBlocks[block] < 0 ? left[16 + leftBlocks[block]] :
			modes[leftBlocks[block]];
		predicted = mask & 1 ? 2 : upper < beside ? upper : beside;
		mode = modes[block];
		if (mode == predicted) {
			matches++;
			codes[block] = -128;
		} else {
			codes[block] = mode > predicted ? mode - 1 : mode;
		}
	}
	return matches;
}

/* the leading zeros of a half */
static inline uint32_t leadingZeros(uint16_t value)
{
	uint32_t zeros = 0;

	while (zeros < 16 && !(value & 0x8000 >> zeros))
		zeros++;
	return zeros;
}

/* a vector difference's bits, roughly: 1 for none, then its size plus 2, past 8 its log's */
static inline int16_t vectorBits(int16_t difference)
{
	uint16_t size = difference < 0 ? -difference : difference;

	if (size == 0)
		return 1;
	if ((int16_t)size > 8)
		return (15 - leadingZeros(size - 1)) * 2 + 8;
	return size + 2;
}

/* a vector difference's Exp-Golomb code: se(v)'s length */
static inline int16_t codeBits(int16_t difference)
{
	int16_t twice = difference * 2;
	uint16_t number = twice > 0 ? twice - 1 : -twice;

	return (15 - leadingZeros(number + 1)) * 2 + 1;
}

/* the lanes' bits, by pair */
static void sumBits(AvcencMotion *motion, const int16_t *lanes)
{
	motion->bits[0] = lanes[0] + lanes[1];
	motion->bits[1] = lanes[2] + lanes[3];
}

/* 0x8a1588: the first differences' bits, roughly, the inter decision's weight */
void avcencMotionBits(AvcencMotion *motion)
{
	int16_t lanes[4];
	uint32_t i;

	for (i = 0; i < 4; i++)
		lanes[i] = vectorBits(motion->differences[0][i]);
	sumBits(motion, lanes);
}

/* 0x8a15c8: both differences' */
void avcencMotionBits8x8(AvcencMotion *motion)
{
	int16_t lanes[4];
	uint32_t i;

	for (i = 0; i < 4; i++)
		lanes[i] = vectorBits(motion->differences[1][i]) +
			vectorBits(motion->differences[0][i]);
	sumBits(motion, lanes);
}

/* 0x8a1638: the first differences' bits as coded */
void avcencMotionCodeBits(AvcencMotion *motion)
{
	int16_t lanes[4];
	uint32_t i;

	for (i = 0; i < 4; i++)
		lanes[i] = codeBits(motion->differences[0][i]);
	sumBits(motion, lanes);
}

/* 0x8a1688: both differences' */
void avcencMotionCodeBits8x8(AvcencMotion *motion)
{
	int16_t lanes[4];
	uint32_t i;

	for (i = 0; i < 4; i++)
		lanes[i] = codeBits(motion->differences[0][i]) +
			codeBits(motion->differences[1][i]);
	sumBits(motion, lanes);
}

/*
 * 0x8a16f0: the blocks' vectors whole: the inter decision's offsets from
 * the macroblock's vector (in whole pixels) added to it, and its costs summed
 */
void avcencMotionVectors(AvcencMotion *motion, const int16_t *vector, const int16_t (*vectors)[2],
	const uint16_t *costs)
{
	uint32_t i;

	for (i = 0; i < 4; i++) {
		motion->vectors[0][i] = vectors[i >> 1][i & 1] + vector[i & 1] * 4;
		motion->vectors[1][i] = vectors[2 + (i >> 1)][i & 1] + vector[i & 1] * 4;
	}
	motion->satd = (uint16_t)(costs[0] + costs[1]) + (uint16_t)(costs[2] + costs[3]);
}

/* the median of three, lane by lane */
static inline int16_t median3(int16_t a, int16_t b, int16_t c)
{
	int16_t low = a < b ? a : b, high = a < b ? b : a;

	if (high > c)
		high = c;
	return low > high ? low : high;
}

/*
 * 0x8a1730: the first block's prediction, the median of its neighbours,
 * those unavailable zero; one alone available is taken
 */
void avcencMotionMedian(AvcencMotion *motion)
{
	int16_t (*neighbours)[4] = motion->neighbours[0];
	uint32_t i, n, available = 0;

	for (n = 0; n < 3; n++) {
		if (motion->unavailable & 1 << n)
			for (i = 0; i < 4; i++)
				neighbours[n][i] = 0;
		else
			available++;
	}
	for (i = 0; i < 4; i++) {
		if (available != 1)
			motion->predictions[0][i] = median3(neighbours[MOTION_LEFT][i],
				neighbours[MOTION_ABOVE][i], neighbours[MOTION_CORNER][i]);
		else if (!(motion->unavailable & 1 << MOTION_CORNER))
			motion->predictions[0][i] = neighbours[MOTION_CORNER][i];
		else if (!(motion->unavailable & 1 << MOTION_ABOVE))
			motion->predictions[0][i] = neighbours[MOTION_ABOVE][i];
		else
			motion->predictions[0][i] = neighbours[MOTION_LEFT][i];
	}
}

/* the medians of two blocks' neighbours, their vectors in the lanes' two halves */
static void medianPairs(int16_t *prediction, const int16_t *left0, const int16_t *left1,
	const int16_t *above0, const int16_t *above1, const int16_t *corner0,
	const int16_t *corner1)
{
	uint32_t i;

	for (i = 0; i < 2; i++) {
		prediction[i] = median3(left0[i], above0[i], corner0[i]);
		prediction[2 + i] = median3(left1[i], above1[i], corner1[i]);
	}
}

/* 0x8a1790: the first two blocks' predictions, the medians of their neighbours */
void avcencMotionMedianPairs(AvcencMotion *motion)
{
	int16_t (*first)[4] = motion->neighbours[0], (*second)[4] = motion->neighbours[1];

	medianPairs(motion->predictions[0], first[MOTION_LEFT], second[MOTION_LEFT],
		first[MOTION_ABOVE], second[MOTION_ABOVE], first[MOTION_CORNER],
		second[MOTION_CORNER]);
}

/*
 * 0x8a17b0: the last two blocks' (8x8): block 2's are the left macroblock's
 * block 3 (zero when it is unavailable) and blocks 0 and 1, block 3's
 * blocks 2, 1 and 0
 */
void avcencMotionMedianLower(AvcencMotion *motion)
{
	static const int16_t none[4];
	const int16_t *left = motion->unavailable & 1 << MOTION_LEFT ? none : motion->leftBelow;

	medianPairs(motion->predictions[1], left, motion->vectors[1], motion->vectors[0],
		&motion->vectors[0][2], &motion->vectors[0][2], motion->vectors[0]);
}

/*
 * 0x8a17e0: P_Skip's prediction, the median of the neighbours (each
 * unavailable one already zero); one alone available is taken
 */
void avcencMotionSkipMedian(AvcencMotion *motion, int16_t *vector)
{
	int16_t (*neighbours)[4] = motion->neighbours[0];
	uint32_t i, n, available = 0;

	for (n = 0; n < 3; n++)
		if (!(motion->unavailable & 1 << n))
			available++;
	for (i = 0; i < 2; i++) {
		if (available != 1)
			vector[i] = median3(neighbours[MOTION_LEFT][i], neighbours[MOTION_ABOVE][i],
				neighbours[MOTION_CORNER][i]);
		else if (!(motion->unavailable & 1 << MOTION_CORNER))
			vector[i] = neighbours[MOTION_CORNER][i];
		else if (!(motion->unavailable & 1 << MOTION_ABOVE))
			vector[i] = neighbours[MOTION_ABOVE][i];
		else
			vector[i] = neighbours[MOTION_LEFT][i];
	}
}

/* one pass of the 4x4 forward transform, over the rows' lanes */
static void transformPass(int32_t (*block)[4])
{
	int32_t sum0, sum1, difference0, difference1;
	uint32_t i;

	for (i = 0; i < 4; i++) {
		sum0 = block[0][i] + block[3][i];
		sum1 = block[1][i] + block[2][i];
		difference0 = block[0][i] - block[3][i];
		difference1 = block[1][i] - block[2][i];
		block[0][i] = sum0 + sum1;
		block[2][i] = sum0 - sum1;
		block[1][i] = difference1 + difference0 * 2;
		block[3][i] = difference0 - difference1 * 2;
	}
}

/*
 * 0x8a1828: a 4x4 block's forward integer transform, its coefficients'
 * sizes; whether rows 0 and 2 are below their thresholds
 */
uint32_t avcencMotionTransform(const AvcencMotion *motion, int32_t (*block)[4])
{
	int32_t column[4][4];
	uint32_t i, j, below = 1;

	transformPass(block);
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
			column[j][i] = block[i][j];
	transformPass(column);
	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++)
			block[i][j] = abs32(column[i][j]);
		if (i == 0 || i == 2)
			for (j = 0; j < 4; j++)
				if (motion->thresholds[0][j] <= block[i][j])
					below = 0;
	}
	return below;
}

/*
 * 0x8a18a8: a band of the prediction, four lines of the reference: their
 * left eight pixels from one column of a tile, the right ones from the next
 */
void avcencMotionLoadLuma(AvcencMotion *motion, const uint8_t *first, const uint8_t *second,
	int32_t pitch, uint32_t line)
{
	uint32_t i, j;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 8; j++) {
			motion->luma[i][j] = first[j];
			motion->luma[i][8 + j] = second[j];
		}
		first += 16;
		second += 16;
		if (line + i == 15) {
			first += pitch * 16 - 256;
			second += pitch * 16 - 256;
		}
	}
}

/* 0x8a1950: from three columns, the pixels from the offset into the first */
void avcencMotionLoadLumaShifted(AvcencMotion *motion, const uint8_t *first,
	const uint8_t *second, int32_t pitch, uint32_t line, uint32_t offset)
{
	const uint8_t *third = first + 256;
	uint8_t pixels[24];
	uint32_t i, j;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 8; j++) {
			pixels[j] = first[j];
			pixels[8 + j] = second[j];
			pixels[16 + j] = third[j];
		}
		for (j = 0; j < 16; j++)
			motion->luma[i][j] = pixels[offset + j];
		first += 16;
		second += 16;
		third += 16;
		if (line + i == 15) {
			first += pitch * 16 - 256;
			second += pitch * 16 - 256;
			third += pitch * 16 - 256;
		}
	}
}

/*
 * a band's lines, loaded within the picture, moved to where the vector
 * puts them: past the left or the right edge, its pixel is repeated, past
 * the top or the bottom, its line
 */
static void moveBand(uint8_t *band, uint32_t pitch, uint32_t size, int32_t x, int32_t y,
	int32_t width, int32_t height)
{
	int32_t shift, line, i, from;
	uint8_t pixels[16], fill;

	shift = x < 0 ? x : x > width - (int32_t)size ? x - (width - (int32_t)size) : 0;
	if (shift != 0) {
		for (line = 0; line < 4; line++) {
			fill = band[line * pitch + (shift < 0 ? 0 : size - 1)];
			for (i = 0; i < (int32_t)size; i++) {
				from = i + shift;
				pixels[i] = from < 0 || from >= (int32_t)size ? fill :
					band[line * pitch + from];
			}
			for (i = 0; i < (int32_t)size; i++)
				band[line * pitch + i] = pixels[i];
		}
	}
	for (; y < 0; y++)
		for (line = 3; line > 0; line--)
			for (i = 0; i < (int32_t)size; i++)
				band[line * pitch + i] = band[(line - 1) * pitch + i];
	for (; y > height - 4; y--)
		for (line = 0; line < 3; line++)
			for (i = 0; i < (int32_t)size; i++)
				band[line * pitch + i] = band[(line + 1) * pitch + i];
}

/* 0x8a1a38: the luma band moved to the vector's position */
void avcencMotionEdgeLuma(AvcencMotion *motion, int32_t x, int32_t y, int32_t width,
	int32_t height)
{
	moveBand(motion->luma[0], 16, 16, x, y, width, height);
}

/* whether a 4x4 block of the source, less the prediction, is below the thresholds once transformed */
static uint32_t blockBelow(const AvcencMotion *motion, const uint8_t *prediction,
	uint32_t predictionPitch, const uint8_t *source, uint32_t sourcePitch)
{
	int32_t block[4][4];
	uint32_t i, j;

	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
			block[i][j] = prediction[i * predictionPitch + j] - source[i * sourcePitch + j];
	if (!avcencMotionTransform(motion, block))
		return 0;
	for (j = 0; j < 4; j++)
		if (motion->thresholds[1][j] <= block[1][j] || motion->thresholds[1][j] <= block[3][j])
			return 0;
	return 1;
}

/*
 * 0x8a1c08: P_Skip's check of a band of the luma: each 4x4 block of the
 * source's four lines less the prediction, transformed, below the thresholds
 */
uint32_t avcencMotionSkipLuma(const AvcencMotion *motion, const uint8_t *source)
{
	uint32_t block;

	for (block = 0; block < 4; block++)
		if (!blockBelow(motion, &motion->luma[0][block * 4], 16, source + block * 4, 16))
			return 0;
	return 1;
}

/* 0x8a1d80: a band of the chroma, U's eight pixels of each line and V's, from one column of a tile */
void avcencMotionLoadChroma(AvcencMotion *motion, const uint8_t *first, int32_t pitch,
	uint32_t line)
{
	const uint8_t *second = first + 64;
	uint32_t i, j;

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 8; j++) {
			motion->chroma[0][i][j] = first[j];
			motion->chroma[1][i][j] = second[j];
		}
		first += 8;
		second += 8;
		if (line + i == 7) {
			first += pitch * 8 - 64;
			second += pitch * 8 - 64;
		}
	}
}

/* 0x8a1e30: from two columns, the pixels from the offset into the first */
void avcencMotionLoadChromaShifted(AvcencMotion *motion, const uint8_t *first,
	const uint8_t *second, int32_t pitch, uint32_t line, uint32_t offset)
{
	const uint8_t *columns[4] = { first, second, first + 64, second + 64 };
	uint8_t pixels[16];
	uint32_t i, j, plane;

	for (i = 0; i < 4; i++) {
		for (plane = 0; plane < 2; plane++) {
			for (j = 0; j < 8; j++) {
				pixels[j] = columns[plane * 2][j];
				pixels[8 + j] = columns[plane * 2 + 1][j];
			}
			for (j = 0; j < 8; j++)
				motion->chroma[plane][i][j] = pixels[offset + j];
		}
		for (j = 0; j < 4; j++) {
			columns[j] += 8;
			if (line + i == 7)
				columns[j] += pitch * 8 - 64;
		}
	}
}

/* 0x8a1f58: the chroma band moved to the vector's position */
void avcencMotionEdgeChroma(AvcencMotion *motion, int32_t x, int32_t y, int32_t width,
	int32_t height)
{
	moveBand(motion->chroma[0][0], 8, 8, x, y, width, height);
	moveBand(motion->chroma[1][0], 8, 8, x, y, width, height);
}

/*
 * 0x8a2090: P_Skip's check of a band of the chroma: one 4x4 block of U and
 * one of V, the first band U's left one and V's right one, the second the
 * others
 */
uint32_t avcencMotionSkipChroma(const AvcencMotion *motion, const uint8_t *source,
	uint32_t first)
{
	uint32_t u = first ? 0 : 4, v = first ? 4 : 0;

	if (!blockBelow(motion, &motion->chroma[0][0][u], 8, source + u, 8))
		return 0;
	return blockBelow(motion, &motion->chroma[1][0][v], 8, source + 64 + v, 8);
}

/*
 * The deblocking's kernels (0x8a2208-0x8a3c00): the decoder's (0x897dd8-
 * 0x898f80, 0x89b650-0x89bed8) built again for the encoder, the same code
 * but for the luma's last columns: each calls the decoder's. An edge's
 * thresholds and lanes (0x8a2510, as the decoder's 0x8980e8) are part of
 * the edges'.
 */

/* 0x8a2208 */
void avcencDeblockLoad(AvcDeblock *deblock, const uint8_t *samples)
{
	avcdecDeblockLoad(deblock, samples);
}

/* 0x8a2290 */
void avcencDeblockStore(const AvcDeblock *deblock, uint8_t *samples)
{
	avcdecDeblockStore(deblock, samples);
}

/* 0x8a2320 */
void avcencDeblockStoreLeft(const AvcDeblock *deblock, uint8_t *samples)
{
	avcdecDeblockStoreLeft(deblock, samples);
}

/*
 * 0x8a2380: the macroblock's right four columns, turned, for the next one's
 * left edge: the upper half's from its samples stored, the lower half's as
 * they are still in the coprocessor
 */
void avcencDeblockKeepLeft(const AvcDeblock *deblock, const uint8_t *luma,
	uint8_t (*columns)[4][8])
{
	uint32_t column, row;

	for (column = 0; column < 4; column++)
		for (row = 0; row < 8; row++) {
			columns[0][column][row] = luma[row * 16 + 12 + column];
			columns[1][column][row] = deblock->rows[row][12 + column];
		}
}

/* 0x8a2450 */
void avcencDeblockTurnLeft(AvcDeblock *deblock)
{
	avcdecDeblockTurnLeft(deblock);
}

/* 0x8a24b0 */
void avcencDeblockTurnRight(AvcDeblock *deblock)
{
	avcdecDeblockTurnRight(deblock);
}

/* 0x8a2628 */
uint32_t avcencDeblockEdge(AvcDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths)
{
	return avcdecDeblockEdge(deblock, indexA, indexB, strengths);
}

/* 0x8a2948 */
uint32_t avcencDeblockEdgeRight(AvcDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths)
{
	return avcdecDeblockEdgeRight(deblock, indexA, indexB, strengths);
}

/* 0x8a2c68 */
void avcencDeblockInner(AvcDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths)
{
	avcdecDeblockInner(deblock, indexA, indexB, strengths);
}

/* 0x8a2e38 */
void avcencDeblockInnerRight(AvcDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths)
{
	avcdecDeblockInnerRight(deblock, indexA, indexB, strengths);
}

/* 0x8a3008 */
void avcencDeblockMiddle(AvcDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths)
{
	avcdecDeblockMiddle(deblock, indexA, indexB, strengths);
}

/* 0x8a31d8 */
void avcencDeblockInnerLast(AvcDeblock *deblock, uint32_t indexA, uint32_t indexB,
	uint32_t strengths)
{
	avcdecDeblockInnerLast(deblock, indexA, indexB, strengths);
}

/* 0x8a33a8 */
void avcencChromaDeblockLoad(AvcChromaDeblock *deblock, const uint8_t *samples)
{
	avcdecChromaDeblockLoad(deblock, samples);
}

/* 0x8a3430 */
void avcencChromaDeblockStore(const AvcChromaDeblock *deblock, uint8_t *samples)
{
	avcdecChromaDeblockStore(deblock, samples);
}

/* 0x8a34c0 */
void avcencChromaDeblockStoreLeft(const AvcChromaDeblock *deblock, uint8_t *samples)
{
	avcdecChromaDeblockStoreLeft(deblock, samples);
}

/* 0x8a3550 */
void avcencChromaDeblockKeepLeft(const AvcChromaDeblock *deblock, uint8_t (*columns)[2][8])
{
	avcdecChromaDeblockKeepLeft(deblock, columns);
}

/* 0x8a35b0 */
uint32_t avcencChromaDeblockTurn(AvcChromaDeblock *deblock)
{
	return avcdecChromaDeblockTurn(deblock);
}

/* 0x8a3670 */
uint32_t avcencChromaDeblockMask(const AvcChromaDeblock *deblock, uint32_t indexA,
	uint32_t indexB, uint32_t strengths, uint32_t inner)
{
	return avcdecChromaDeblockMask(deblock, indexA, indexB, strengths, inner);
}

/* 0x8a3770 */
uint32_t avcencChromaDeblockEdge(AvcChromaDeblock *deblock, uint32_t indexA,
	uint32_t indexB, uint32_t strengths)
{
	return avcdecChromaDeblockEdge(deblock, indexA, indexB, strengths);
}

/* 0x8a39f0 */
uint32_t avcencChromaDeblockInner(AvcChromaDeblock *deblock, uint32_t indexA,
	uint32_t indexB, uint32_t strengths)
{
	return avcdecChromaDeblockInner(deblock, indexA, indexB, strengths);
}

/*
 * The strengths' kernels (0x8df4d0-0x8df640): a P macroblock's bS, two edges
 * a register: 2 where either 4x4 block has coefficients, 4 next to an intra
 * macroblock, 1 where the 8x8 blocks' vectors are a sample apart
 */

/* the edges' groups: by edge pair, the 4x4 blocks (z order) either side of each */
static const uint8_t verticalGroups[16] = {
	0, 2, 8, 10, 1, 3, 9, 11, 4, 6, 12, 14, 5, 7, 13, 15,
};

static const uint8_t horizontalGroups[16] = {
	0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15,
};

/*
 * 0x8df4d0: 2 where the blocks have coefficients, by the edges' masks; 4 on
 * the macroblock's edges next to an intra one (by group)
 */
void avcencStrengthsCoded(AvcencStrengthLanes *lanes, uint32_t vertical, uint32_t horizontal,
	uint32_t leftIntra, uint32_t topIntra)
{
	uint32_t i;

	for (i = 0; i < 16; i++) {
		lanes->coded[i / 8][i % 8] = (vertical >> verticalGroups[i]) & 1 ? 2 : 0;
		lanes->coded[2 + i / 8][i % 8] = (horizontal >> horizontalGroups[i]) & 1 ? 2 : 0;
	}
	for (i = 0; i < 8; i++) {
		if (leftIntra & (1 << i))
			lanes->coded[0][i] = 4;
		if (topIntra & (1 << i))
			lanes->coded[2][i] = 4;
	}
}

/* a component four quarter samples apart or more, the difference in 16 bits */
static inline uint32_t componentFar(int16_t a, int16_t b)
{
	int32_t difference = a - b;

	return (int16_t)(difference < 0 ? -difference : difference) >= 4;
}

static inline uint32_t vectorsFar(const int16_t *a, const int16_t *b)
{
	return componentFar(a[0], b[0]) || componentFar(a[1], b[1]);
}

/*
 * 0x8df578: and 1 where the vectors differ, over the coded ones: on the
 * left and top edges against the neighbours' 8x8 blocks there, in the
 * middle between the macroblock's own; none inside an 8x8 block
 */
void avcencStrengthsVectors(AvcencStrengthLanes *lanes, const int16_t (*current)[2],
	const int16_t (*left)[2], const int16_t (*above)[2])
{
	uint8_t far[4][8];
	uint32_t edge, i, half;

	for (i = 0; i < 4; i++) {
		half = i >> 1;
		far[0][i] = vectorsFar(current[2 * half], left[2 * half + 1]);
		far[0][4 + i] = 0;
		far[1][i] = vectorsFar(current[2 * half + 1], current[2 * half]);
		far[1][4 + i] = 0;
		far[2][i] = vectorsFar(current[half], above[2 + half]);
		far[2][4 + i] = 0;
		far[3][i] = vectorsFar(current[2 + half], current[half]);
		far[3][4 + i] = 0;
	}
	for (edge = 0; edge < 4; edge++)
		for (i = 0; i < 8; i++)
			lanes->edges[edge][i] = (int8_t)lanes->coded[edge][i] > (int8_t)far[edge][i] ?
				lanes->coded[edge][i] : far[edge][i];
}
