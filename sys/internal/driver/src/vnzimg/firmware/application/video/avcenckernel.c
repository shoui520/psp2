/*
 * The AVC encoder's kernels (0x89cfe0-0x8a0000): VLIW code its jobs call,
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
