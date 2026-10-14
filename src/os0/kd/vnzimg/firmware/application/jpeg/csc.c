/*
 * sceJpegMJpegCsc's colour conversion (VLIW, 0x891778 and 0x8915f0):
 * planar YCbCr, one chroma sample a pixel, to RGBA with alpha 0xff, eight
 * pixels at a time; the output rows are iFrameWidth pixels apart. The
 * coefficients are 10-bit fixed point and the results saturate to bytes.
 */
#include "jpeg.h"

#define CSC_COLOR_OPTION	5
#define FIXED_ONE		1024	/* 1.0 */
#define CR_TO_R			1436	/* 1.402 */
#define CB_TO_G			(-352)	/* -0.344 */
#define CR_TO_G			(-731)	/* -0.714 */
#define CB_TO_B			1815	/* 1.772 */

static uint32_t saturate(int32_t value)
{
	value >>= 10;
	return value < 0 ? 0 : value > 255 ? 255 : value;
}

/* 0x8915f0 */
static void convert(uint32_t *rgba, const uint8_t *y, const uint8_t *cb, const uint8_t *cr,
	int32_t width, int32_t height, int32_t skip)
{
	int32_t rows = height, groups, i;

	do {
		groups = width >> 3;
		do {
			for (i = 0; i < 8; i++) {
				int32_t luma = y[i] * FIXED_ONE, u = cb[i] - 128, v = cr[i] - 128;

				*rgba++ = saturate(luma + CR_TO_R * v) |
					saturate(luma + CB_TO_G * u + CR_TO_G * v) << 8 |
					saturate(luma + CB_TO_B * u) << 16 | 0xffu << 24;
			}
			y += 8;
			cb += 8;
			cr += 8;
		} while (--groups >= 1);
		rgba += skip;
	} while (--rows >= 1);
}

/* 0x891778 */
int32_t jpegMJpegCsc(void *pRGBA, const uint8_t *y, const uint8_t *cb, const uint8_t *cr,
	int32_t colorOption, uint32_t xysize, uint32_t iFrameWidth)
{
	uint32_t width = xysize >> 16, height = xysize & 0xffff;

	if (colorOption != CSC_COLOR_OPTION)
		return SCE_JPEG_ERROR_UNSUPPORT_SAMPLING;
	if ((width & 7) || width > iFrameWidth)
		return SCE_JPEG_ERROR_UNSUPPORT_IMAGE_SIZE;
	convert(pRGBA, y, cb, cr, width, height, iFrameWidth - width);
	return 0;
}
