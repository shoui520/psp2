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

#endif
