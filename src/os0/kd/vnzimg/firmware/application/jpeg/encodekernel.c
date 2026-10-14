/*
 * The encoder's MCU rows (VLIW, 0x891880): each 8x8 block is level shifted
 * (times 4), transformed by a separable tangent-rotation FDCT in 16-bit
 * saturating arithmetic (rotations Q14), scaled to the DCT's own
 * normalisation (Q14) and quantised by the reciprocals (2^14 / q, rounded
 * half up), then Huffman coded with the tables of ISO/IEC 10918-1 Annex K
 * (luma for the first plane, chroma for the others). The bits go out a word
 * at a time, high first, with 0xFF bytes stuffed.
 */
#include "../ivc2.h"
#include "jpeg.h"

#define TAN_1_16	3259	/* tan(pi/16), Q14 */
#define TAN_2_16	6786	/* tan(pi/8) */
#define TAN_3_16	10947	/* tan(3pi/16) */
#define COS_4_16	11585	/* cos(pi/4) */
#define ROUND_HALF	8192	/* the quantisation's rounding */
#define LUMA		0
#define CHROMA		1
#define ZRL		0xf0	/* sixteen zeros */
#define EOB		0x00

typedef struct JpegHuffmanCode {
	uint16_t code;
	uint8_t length;
} JpegHuffmanCode;

typedef struct BitWriter {
	uint8_t *position;
	uint8_t *end;
	uint32_t buffer;		/* filled from the top */
	int32_t count;			/* the bits in it less 32 */
} BitWriter;

static const JpegHuffmanCode dcCodes[2][12] = {
	{
		{ 0x000,  2 }, { 0x002,  3 }, { 0x003,  3 }, { 0x004,  3 },
		{ 0x005,  3 }, { 0x006,  3 }, { 0x00e,  4 }, { 0x01e,  5 },
		{ 0x03e,  6 }, { 0x07e,  7 }, { 0x0fe,  8 }, { 0x1fe,  9 },
	},
	{
		{ 0x000,  2 }, { 0x001,  2 }, { 0x002,  2 }, { 0x006,  3 },
		{ 0x00e,  4 }, { 0x01e,  5 }, { 0x03e,  6 }, { 0x07e,  7 },
		{ 0x0fe,  8 }, { 0x1fe,  9 }, { 0x3fe, 10 }, { 0x7fe, 11 },
	},
};

/* by run << 4 | size */
static const JpegHuffmanCode acCodes[2][256] = {
	{
		[0x00] = { 0x000a,  4 }, [0x01] = { 0x0000,  2 }, [0x02] = { 0x0001,  2 },
		[0x03] = { 0x0004,  3 }, [0x04] = { 0x000b,  4 }, [0x05] = { 0x001a,  5 },
		[0x06] = { 0x0078,  7 }, [0x07] = { 0x00f8,  8 }, [0x08] = { 0x03f6, 10 },
		[0x09] = { 0xff82, 16 }, [0x0a] = { 0xff83, 16 }, [0x11] = { 0x000c,  4 },
		[0x12] = { 0x001b,  5 }, [0x13] = { 0x0079,  7 }, [0x14] = { 0x01f6,  9 },
		[0x15] = { 0x07f6, 11 }, [0x16] = { 0xff84, 16 }, [0x17] = { 0xff85, 16 },
		[0x18] = { 0xff86, 16 }, [0x19] = { 0xff87, 16 }, [0x1a] = { 0xff88, 16 },
		[0x21] = { 0x001c,  5 }, [0x22] = { 0x00f9,  8 }, [0x23] = { 0x03f7, 10 },
		[0x24] = { 0x0ff4, 12 }, [0x25] = { 0xff89, 16 }, [0x26] = { 0xff8a, 16 },
		[0x27] = { 0xff8b, 16 }, [0x28] = { 0xff8c, 16 }, [0x29] = { 0xff8d, 16 },
		[0x2a] = { 0xff8e, 16 }, [0x31] = { 0x003a,  6 }, [0x32] = { 0x01f7,  9 },
		[0x33] = { 0x0ff5, 12 }, [0x34] = { 0xff8f, 16 }, [0x35] = { 0xff90, 16 },
		[0x36] = { 0xff91, 16 }, [0x37] = { 0xff92, 16 }, [0x38] = { 0xff93, 16 },
		[0x39] = { 0xff94, 16 }, [0x3a] = { 0xff95, 16 }, [0x41] = { 0x003b,  6 },
		[0x42] = { 0x03f8, 10 }, [0x43] = { 0xff96, 16 }, [0x44] = { 0xff97, 16 },
		[0x45] = { 0xff98, 16 }, [0x46] = { 0xff99, 16 }, [0x47] = { 0xff9a, 16 },
		[0x48] = { 0xff9b, 16 }, [0x49] = { 0xff9c, 16 }, [0x4a] = { 0xff9d, 16 },
		[0x51] = { 0x007a,  7 }, [0x52] = { 0x07f7, 11 }, [0x53] = { 0xff9e, 16 },
		[0x54] = { 0xff9f, 16 }, [0x55] = { 0xffa0, 16 }, [0x56] = { 0xffa1, 16 },
		[0x57] = { 0xffa2, 16 }, [0x58] = { 0xffa3, 16 }, [0x59] = { 0xffa4, 16 },
		[0x5a] = { 0xffa5, 16 }, [0x61] = { 0x007b,  7 }, [0x62] = { 0x0ff6, 12 },
		[0x63] = { 0xffa6, 16 }, [0x64] = { 0xffa7, 16 }, [0x65] = { 0xffa8, 16 },
		[0x66] = { 0xffa9, 16 }, [0x67] = { 0xffaa, 16 }, [0x68] = { 0xffab, 16 },
		[0x69] = { 0xffac, 16 }, [0x6a] = { 0xffad, 16 }, [0x71] = { 0x00fa,  8 },
		[0x72] = { 0x0ff7, 12 }, [0x73] = { 0xffae, 16 }, [0x74] = { 0xffaf, 16 },
		[0x75] = { 0xffb0, 16 }, [0x76] = { 0xffb1, 16 }, [0x77] = { 0xffb2, 16 },
		[0x78] = { 0xffb3, 16 }, [0x79] = { 0xffb4, 16 }, [0x7a] = { 0xffb5, 16 },
		[0x81] = { 0x01f8,  9 }, [0x82] = { 0x7fc0, 15 }, [0x83] = { 0xffb6, 16 },
		[0x84] = { 0xffb7, 16 }, [0x85] = { 0xffb8, 16 }, [0x86] = { 0xffb9, 16 },
		[0x87] = { 0xffba, 16 }, [0x88] = { 0xffbb, 16 }, [0x89] = { 0xffbc, 16 },
		[0x8a] = { 0xffbd, 16 }, [0x91] = { 0x01f9,  9 }, [0x92] = { 0xffbe, 16 },
		[0x93] = { 0xffbf, 16 }, [0x94] = { 0xffc0, 16 }, [0x95] = { 0xffc1, 16 },
		[0x96] = { 0xffc2, 16 }, [0x97] = { 0xffc3, 16 }, [0x98] = { 0xffc4, 16 },
		[0x99] = { 0xffc5, 16 }, [0x9a] = { 0xffc6, 16 }, [0xa1] = { 0x01fa,  9 },
		[0xa2] = { 0xffc7, 16 }, [0xa3] = { 0xffc8, 16 }, [0xa4] = { 0xffc9, 16 },
		[0xa5] = { 0xffca, 16 }, [0xa6] = { 0xffcb, 16 }, [0xa7] = { 0xffcc, 16 },
		[0xa8] = { 0xffcd, 16 }, [0xa9] = { 0xffce, 16 }, [0xaa] = { 0xffcf, 16 },
		[0xb1] = { 0x03f9, 10 }, [0xb2] = { 0xffd0, 16 }, [0xb3] = { 0xffd1, 16 },
		[0xb4] = { 0xffd2, 16 }, [0xb5] = { 0xffd3, 16 }, [0xb6] = { 0xffd4, 16 },
		[0xb7] = { 0xffd5, 16 }, [0xb8] = { 0xffd6, 16 }, [0xb9] = { 0xffd7, 16 },
		[0xba] = { 0xffd8, 16 }, [0xc1] = { 0x03fa, 10 }, [0xc2] = { 0xffd9, 16 },
		[0xc3] = { 0xffda, 16 }, [0xc4] = { 0xffdb, 16 }, [0xc5] = { 0xffdc, 16 },
		[0xc6] = { 0xffdd, 16 }, [0xc7] = { 0xffde, 16 }, [0xc8] = { 0xffdf, 16 },
		[0xc9] = { 0xffe0, 16 }, [0xca] = { 0xffe1, 16 }, [0xd1] = { 0x07f8, 11 },
		[0xd2] = { 0xffe2, 16 }, [0xd3] = { 0xffe3, 16 }, [0xd4] = { 0xffe4, 16 },
		[0xd5] = { 0xffe5, 16 }, [0xd6] = { 0xffe6, 16 }, [0xd7] = { 0xffe7, 16 },
		[0xd8] = { 0xffe8, 16 }, [0xd9] = { 0xffe9, 16 }, [0xda] = { 0xffea, 16 },
		[0xe1] = { 0xffeb, 16 }, [0xe2] = { 0xffec, 16 }, [0xe3] = { 0xffed, 16 },
		[0xe4] = { 0xffee, 16 }, [0xe5] = { 0xffef, 16 }, [0xe6] = { 0xfff0, 16 },
		[0xe7] = { 0xfff1, 16 }, [0xe8] = { 0xfff2, 16 }, [0xe9] = { 0xfff3, 16 },
		[0xea] = { 0xfff4, 16 }, [0xf0] = { 0x07f9, 11 }, [0xf1] = { 0xfff5, 16 },
		[0xf2] = { 0xfff6, 16 }, [0xf3] = { 0xfff7, 16 }, [0xf4] = { 0xfff8, 16 },
		[0xf5] = { 0xfff9, 16 }, [0xf6] = { 0xfffa, 16 }, [0xf7] = { 0xfffb, 16 },
		[0xf8] = { 0xfffc, 16 }, [0xf9] = { 0xfffd, 16 }, [0xfa] = { 0xfffe, 16 },
	},
	{
		[0x00] = { 0x0000,  2 }, [0x01] = { 0x0001,  2 }, [0x02] = { 0x0004,  3 },
		[0x03] = { 0x000a,  4 }, [0x04] = { 0x0018,  5 }, [0x05] = { 0x0019,  5 },
		[0x06] = { 0x0038,  6 }, [0x07] = { 0x0078,  7 }, [0x08] = { 0x01f4,  9 },
		[0x09] = { 0x03f6, 10 }, [0x0a] = { 0x0ff4, 12 }, [0x11] = { 0x000b,  4 },
		[0x12] = { 0x0039,  6 }, [0x13] = { 0x00f6,  8 }, [0x14] = { 0x01f5,  9 },
		[0x15] = { 0x07f6, 11 }, [0x16] = { 0x0ff5, 12 }, [0x17] = { 0xff88, 16 },
		[0x18] = { 0xff89, 16 }, [0x19] = { 0xff8a, 16 }, [0x1a] = { 0xff8b, 16 },
		[0x21] = { 0x001a,  5 }, [0x22] = { 0x00f7,  8 }, [0x23] = { 0x03f7, 10 },
		[0x24] = { 0x0ff6, 12 }, [0x25] = { 0x7fc2, 15 }, [0x26] = { 0xff8c, 16 },
		[0x27] = { 0xff8d, 16 }, [0x28] = { 0xff8e, 16 }, [0x29] = { 0xff8f, 16 },
		[0x2a] = { 0xff90, 16 }, [0x31] = { 0x001b,  5 }, [0x32] = { 0x00f8,  8 },
		[0x33] = { 0x03f8, 10 }, [0x34] = { 0x0ff7, 12 }, [0x35] = { 0xff91, 16 },
		[0x36] = { 0xff92, 16 }, [0x37] = { 0xff93, 16 }, [0x38] = { 0xff94, 16 },
		[0x39] = { 0xff95, 16 }, [0x3a] = { 0xff96, 16 }, [0x41] = { 0x003a,  6 },
		[0x42] = { 0x01f6,  9 }, [0x43] = { 0xff97, 16 }, [0x44] = { 0xff98, 16 },
		[0x45] = { 0xff99, 16 }, [0x46] = { 0xff9a, 16 }, [0x47] = { 0xff9b, 16 },
		[0x48] = { 0xff9c, 16 }, [0x49] = { 0xff9d, 16 }, [0x4a] = { 0xff9e, 16 },
		[0x51] = { 0x003b,  6 }, [0x52] = { 0x03f9, 10 }, [0x53] = { 0xff9f, 16 },
		[0x54] = { 0xffa0, 16 }, [0x55] = { 0xffa1, 16 }, [0x56] = { 0xffa2, 16 },
		[0x57] = { 0xffa3, 16 }, [0x58] = { 0xffa4, 16 }, [0x59] = { 0xffa5, 16 },
		[0x5a] = { 0xffa6, 16 }, [0x61] = { 0x0079,  7 }, [0x62] = { 0x07f7, 11 },
		[0x63] = { 0xffa7, 16 }, [0x64] = { 0xffa8, 16 }, [0x65] = { 0xffa9, 16 },
		[0x66] = { 0xffaa, 16 }, [0x67] = { 0xffab, 16 }, [0x68] = { 0xffac, 16 },
		[0x69] = { 0xffad, 16 }, [0x6a] = { 0xffae, 16 }, [0x71] = { 0x007a,  7 },
		[0x72] = { 0x07f8, 11 }, [0x73] = { 0xffaf, 16 }, [0x74] = { 0xffb0, 16 },
		[0x75] = { 0xffb1, 16 }, [0x76] = { 0xffb2, 16 }, [0x77] = { 0xffb3, 16 },
		[0x78] = { 0xffb4, 16 }, [0x79] = { 0xffb5, 16 }, [0x7a] = { 0xffb6, 16 },
		[0x81] = { 0x00f9,  8 }, [0x82] = { 0xffb7, 16 }, [0x83] = { 0xffb8, 16 },
		[0x84] = { 0xffb9, 16 }, [0x85] = { 0xffba, 16 }, [0x86] = { 0xffbb, 16 },
		[0x87] = { 0xffbc, 16 }, [0x88] = { 0xffbd, 16 }, [0x89] = { 0xffbe, 16 },
		[0x8a] = { 0xffbf, 16 }, [0x91] = { 0x01f7,  9 }, [0x92] = { 0xffc0, 16 },
		[0x93] = { 0xffc1, 16 }, [0x94] = { 0xffc2, 16 }, [0x95] = { 0xffc3, 16 },
		[0x96] = { 0xffc4, 16 }, [0x97] = { 0xffc5, 16 }, [0x98] = { 0xffc6, 16 },
		[0x99] = { 0xffc7, 16 }, [0x9a] = { 0xffc8, 16 }, [0xa1] = { 0x01f8,  9 },
		[0xa2] = { 0xffc9, 16 }, [0xa3] = { 0xffca, 16 }, [0xa4] = { 0xffcb, 16 },
		[0xa5] = { 0xffcc, 16 }, [0xa6] = { 0xffcd, 16 }, [0xa7] = { 0xffce, 16 },
		[0xa8] = { 0xffcf, 16 }, [0xa9] = { 0xffd0, 16 }, [0xaa] = { 0xffd1, 16 },
		[0xb1] = { 0x01f9,  9 }, [0xb2] = { 0xffd2, 16 }, [0xb3] = { 0xffd3, 16 },
		[0xb4] = { 0xffd4, 16 }, [0xb5] = { 0xffd5, 16 }, [0xb6] = { 0xffd6, 16 },
		[0xb7] = { 0xffd7, 16 }, [0xb8] = { 0xffd8, 16 }, [0xb9] = { 0xffd9, 16 },
		[0xba] = { 0xffda, 16 }, [0xc1] = { 0x01fa,  9 }, [0xc2] = { 0xffdb, 16 },
		[0xc3] = { 0xffdc, 16 }, [0xc4] = { 0xffdd, 16 }, [0xc5] = { 0xffde, 16 },
		[0xc6] = { 0xffdf, 16 }, [0xc7] = { 0xffe0, 16 }, [0xc8] = { 0xffe1, 16 },
		[0xc9] = { 0xffe2, 16 }, [0xca] = { 0xffe3, 16 }, [0xd1] = { 0x07f9, 11 },
		[0xd2] = { 0xffe4, 16 }, [0xd3] = { 0xffe5, 16 }, [0xd4] = { 0xffe6, 16 },
		[0xd5] = { 0xffe7, 16 }, [0xd6] = { 0xffe8, 16 }, [0xd7] = { 0xffe9, 16 },
		[0xd8] = { 0xffea, 16 }, [0xd9] = { 0xffeb, 16 }, [0xda] = { 0xffec, 16 },
		[0xe1] = { 0x3fe0, 14 }, [0xe2] = { 0xffed, 16 }, [0xe3] = { 0xffee, 16 },
		[0xe4] = { 0xffef, 16 }, [0xe5] = { 0xfff0, 16 }, [0xe6] = { 0xfff1, 16 },
		[0xe7] = { 0xfff2, 16 }, [0xe8] = { 0xfff3, 16 }, [0xe9] = { 0xfff4, 16 },
		[0xea] = { 0xfff5, 16 }, [0xf0] = { 0x03fa, 10 }, [0xf1] = { 0x7fc3, 15 },
		[0xf2] = { 0xfff6, 16 }, [0xf3] = { 0xfff7, 16 }, [0xf4] = { 0xfff8, 16 },
		[0xf5] = { 0xfff9, 16 }, [0xf6] = { 0xfffa, 16 }, [0xf7] = { 0xfffb, 16 },
		[0xf8] = { 0xfffc, 16 }, [0xf9] = { 0xfffd, 16 }, [0xfa] = { 0xfffe, 16 },
	},
};

/* the DCT's normalisation by [u][v] frequency, Q14 * 32 */
static const int16_t scale[8][8] = {
	{ 512, 710, 669, 602, 512, 602, 669, 710 },
	{ 710, 985, 928, 835, 710, 835, 928, 985 },
	{ 669, 928, 874, 787, 669, 787, 874, 928 },
	{ 602, 835, 787, 708, 602, 708, 787, 835 },
	{ 512, 710, 669, 602, 512, 602, 669, 710 },
	{ 602, 835, 787, 708, 602, 708, 787, 835 },
	{ 669, 928, 874, 787, 669, 787, 874, 928 },
	{ 710, 985, 928, 835, 710, 835, 928, 985 },
};

static const uint8_t zigzag[64] = {
	 0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
};

static int32_t add(int32_t a, int32_t b)
{
	return ivc2Clip(a + b, 16);
}

static int32_t subtract(int32_t a, int32_t b)
{
	return ivc2Clip(a - b, 16);
}

/* (c x) >> 14, saturated */
static int32_t rotate(int32_t c, int32_t x)
{
	return ivc2Clip((c * x) >> 14, 16);
}

/* one dimension, eight values `step` apart, in place */
static void fdct(int16_t *x, int32_t step)
{
	int32_t s07 = add(x[0], x[7 * step]), d07 = subtract(x[0], x[7 * step]);
	int32_t s16 = add(x[step], x[6 * step]), d16 = subtract(x[step], x[6 * step]);
	int32_t s25 = add(x[2 * step], x[5 * step]), d25 = subtract(x[2 * step], x[5 * step]);
	int32_t s34 = add(x[3 * step], x[4 * step]), d34 = subtract(x[3 * step], x[4 * step]);
	int32_t even0 = add(s07, s34), even1 = add(s16, s25);
	int32_t even2 = subtract(s07, s34), even3 = subtract(s16, s25);
	int32_t odd0 = rotate(COS_4_16, add(d16, d25));
	int32_t odd1 = rotate(COS_4_16, subtract(d16, d25));
	int32_t u = add(odd0, d07), w = add(odd1, d34);
	int32_t f = subtract(d07, odd0), g = subtract(d34, odd1);

	x[0] = add(even0, even1);
	x[4 * step] = subtract(even0, even1);
	x[2 * step] = add(rotate(TAN_2_16, even3), even2);
	x[6 * step] = subtract(rotate(TAN_2_16, even2), even3);
	x[step] = add(rotate(TAN_1_16, w), u);
	x[7 * step] = subtract(rotate(TAN_1_16, u), w);
	x[5 * step] = add(rotate(TAN_3_16, f), g);
	x[3 * step] = subtract(f, rotate(TAN_3_16, g));
}

/* a block into quantised coefficients, by [u][v]; the reciprocals are by [v][u] */
static void transform(const uint8_t *block, uint32_t pitch, const uint16_t *reciprocal,
	int16_t coefficient[8][8])
{
	int32_t u, v;

	for (u = 0; u < 8; u++, block += pitch)
		for (v = 0; v < 8; v++)
			coefficient[u][v] = (block[v] - 128) << 2;
	for (v = 0; v < 8; v++)
		fdct(&coefficient[0][v], 8);
	for (u = 0; u < 8; u++)
		fdct(coefficient[u], 1);
	for (u = 0; u < 8; u++) {
		for (v = 0; v < 8; v++) {
			int32_t value = rotate(scale[u][v], coefficient[u][v]);

			coefficient[u][v] = ivc2Clip((ROUND_HALF +
				(int16_t)reciprocal[v * 8 + u] * value) >> 14, 16);
		}
	}
}

/* length bits of code, high first: 0 when the output is full */
static int32_t putBits(BitWriter *writer, uint32_t code, int32_t length)
{
	int32_t filled = writer->count + 32, shift;
	uint32_t value, word;

	if (length == 0)
		return 1;
	value = code << (32 - length);
	word = writer->buffer | (value >> filled);
	writer->count += length;
	if (writer->count < 0) {
		writer->buffer = word;
		return 1;
	}
	if (writer->position >= writer->end)
		return 0;
	for (shift = 24; shift >= 0; shift -= 8) {
		*writer->position++ = word >> shift;
		if ((uint8_t)(word >> shift) == 0xff)
			*writer->position++ = 0;
	}
	writer->buffer = value << (32 - filled);
	writer->count -= 32;
	return 1;
}

static int32_t putCode(BitWriter *writer, const JpegHuffmanCode *code)
{
	return putBits(writer, code->code, code->length);
}

/* the bits of |x| */
static int32_t bitSize(int32_t x)
{
	uint32_t magnitude = x < 0 ? -x : x;
	int32_t size = 0;

	while (magnitude >> size)
		size++;
	return size;
}

/* x in size bits, negative values one less */
static int32_t putValue(BitWriter *writer, int32_t x, int32_t size)
{
	return putBits(writer, (x < 0 ? x - 1 : x) & ((1u << size) - 1), size);
}

/*
 * One block. When the output fills up, the rest of the block is dropped and
 * the bit buffer stays as it was before the block. Sizes beyond the tables'
 * (only from extreme coefficients at q = 1) read past them in the original.
 */
static void encodeBlock(JpegEncoderContext *context, uint32_t class,
	int16_t coefficient[8][8], int16_t *predictor)
{
	BitWriter writer = { context->position, context->end, context->bitBuffer,
		context->bitCount };
	const int16_t *natural = coefficient[0];
	int32_t difference = natural[0] - *predictor, run = 0, size, i;

	*predictor = natural[0];
	size = bitSize(difference);
	if (!putCode(&writer, &dcCodes[class][size]) || !putValue(&writer, difference, size))
		goto full;
	for (i = 1; i < 64; i++) {
		int32_t x = natural[zigzag[i]];

		if (x == 0) {
			run++;
			continue;
		}
		for (; run >= 16; run -= 16)
			if (!putCode(&writer, &acCodes[class][ZRL]))
				goto full;
		size = bitSize(x);
		if (!putCode(&writer, &acCodes[class][run << 4 | size]) ||
		    !putValue(&writer, x, size))
			goto full;
		run = 0;
	}
	if (run && !putCode(&writer, &acCodes[class][EOB]))
		goto full;
	context->bitBuffer = writer.buffer;
	context->bitCount = writer.count;
full:
	context->position = writer.position;
}

void jpegEncodeMcuRow(JpegEncoderContext *context, uint32_t columns, JpegEncoderPlane *planes)
{
	int16_t coefficient[8][8];
	JpegEncoderPlane *plane;
	uint32_t class, block;

	for (; (int32_t)columns >= 1; columns--) {
		for (plane = planes, class = LUMA; plane->blocks; plane++, class = CHROMA) {
			block = 0;
			do {
				transform(plane->block, plane->pitch,
					context->quantReciprocal[class], coefficient);
				plane->block += plane->blockStep[block];
				encodeBlock(context, class, coefficient,
					&context->dcPredictor[plane->component]);
			} while (++block < plane->blocks);
		}
	}
}
