/*
 * The AVC decoder's CABAC slices (0x82a424-0x82cd90). The unit decodes
 * CAVLC slices itself; a CABAC slice is decoded here, a run of macroblocks
 * at a time, into the run's buffers the unit then reads: each macroblock's
 * type, prediction modes, reference indices, motion vector differences and
 * coefficients. The arithmetic decoder reads its bits from the bitstream
 * unit; the neighbours' syntax elements the contexts depend on are kept in
 * a row buffer of the helper's.
 */
#include "service.h"

#define ROW_BUFFER_SIZE		4096	/* 128 entries */

/* mb_type values */
#define I_NXN			0
#define I_PCM			25
#define I_16X16_CBP_LUMA	13	/* the I_16x16 types from here have the luma blocks coded */
#define P_8X8			3
#define P_8X8_REF0		4
#define B_DIRECT_16X16		0
#define B_8X8			22
#define B_TYPES			23	/* the I types follow */
#define P_TYPES			5

/* the sub-macroblock shapes, for their motion vector differences */
#define SUB_8X8			0
#define SUB_8X4			1
#define SUB_4X8			2
#define SUB_4X4			3

/* 9.3.1.1: m (and n below) for contexts 0-399, by cabac_init_idc, then I slices (0x8f61d8) */
static const int8_t cabacInitM[4][400] = {
	{	/* cabac_init_idc 0 */
		20, 2, 3, 20, 2, 3, -28, -23, -6, -1, 7, 23, 23, 21, 1, 0, -37, 5, -13, -11,
		1, 12, -4, 17, 18, 9, 29, 26, 16, 9, -46, -20, 1, -13, -11, 1, -6, -17, -6, 9,
		-3, -6, -11, 6, 7, -5, 2, 0, -3, -10, 5, 4, -3, 0, -7, -5, -4, -5, -7, 1,
		0, 0, 0, 0, -9, 4, 0, -7, 13, 3, 0, -4, -3, -27, -28, -25, -23, -28, -20, -16,
		-22, -21, -18, -13, -29, -7, -5, -7, -13, -3, -1, -1, -9, -3, -9, -8, -23, 5, 6, 6,
		6, -1, 0, -4, -8, -2, -6, -1, -7, 2, 5, -3, 1, 6, -4, 1, -4, 0, 2, -2,
		11, 4, 1, 11, 18, 12, 13, 13, -10, -7, -2, 13, 9, -7, 9, 2, 5, -2, 0, 0,
		-13, -5, -1, 4, -6, 4, 14, 4, 13, 3, 1, 9, 7, 16, 5, 4, 11, -5, -1, 0,
		22, 5, 14, -1, 0, 9, 11, 2, 3, 0, 0, 2, 2, 0, 4, 2, 6, 0, 3, 2,
		4, 6, 6, 7, 6, 6, 11, 14, 8, -1, 7, -3, 15, 22, -1, 25, 30, 28, 28, 32,
		34, 30, 30, 32, 31, 26, 26, 37, 28, 17, 1, 5, 9, 16, 18, 18, 22, 24, 23, 18,
		20, 11, 9, 9, -1, -2, -9, -6, -2, 0, 0, -3, -2, -4, -4, -8, -17, -9, 3, 0,
		0, 0, -6, -7, -12, -11, -30, 1, -3, -1, 1, 2, -6, 0, 0, -3, -10, 0, -4, 5,
		7, 1, -2, -3, -3, -11, 0, 8, 10, 14, 13, 2, 0, -3, -6, -8, -1, -13, -16, -10,
		-21, -18, -14, -22, -21, -18, -21, -23, -26, -10, -12, -5, -9, -22, -5, 9, -4, -10, -1, 7,
		9, 5, 12, 15, 18, 17, 10, 7, -1, 7, 8, 9, 6, 2, 13, 10, 6, 5, 13, 4,
		6, -2, -2, 6, 10, 9, 12, 3, 14, 10, -3, 13, 17, 7, 7, 13, 10, 26, 14, 11,
		9, 18, 21, 23, 32, 32, 34, 39, 42, 41, 46, 38, 21, 45, 53, 48, 65, 43, 39, 30,
		18, 20, 0, -14, -5, -19, -35, 27, 28, 31, 27, 34, 30, 24, 33, 22, 26, 21, 26, 23,
		16, 14, 8, 6, 17, 21, 23, 26, 27, 28, 28, 23, 24, 28, 23, 19, 22, 22, 11, 0,
	},
	{	/* cabac_init_idc 1 */
		20, 2, 3, 20, 2, 3, -28, -23, -6, -1, 7, 22, 34, 16, -2, 4, -29, 2, -6, -13,
		5, 9, -3, 10, 26, 19, 40, 57, 41, 26, -45, -15, -4, -6, -13, 5, 6, -13, 0, 8,
		-2, -5, -10, 2, 2, -3, -3, 1, -3, -6, 0, -3, -7, -5, -1, -1, 1, -2, -5, 0,
		0, 0, 0, 0, -9, 4, 0, -7, 13, 3, 13, 7, 2, -39, -18, -17, -26, -35, -24, -23,
		-27, -24, -21, -18, -36, 0, -5, -7, -4, 0, 0, -15, -35, -2, -12, -9, -31, 3, 7, 7,
		8, -3, 0, -7, -9, -13, -13, -9, -14, -8, -12, -23, -24, -10, -20, -17, -78, -70, -50, -46,
		-4, -5, -4, -8, 2, -1, -7, -6, -8, -34, -3, 32, 30, -44, 0, -5, 0, -1, -3, -8,
		-25, -14, -5, 5, 2, 0, -9, -11, 18, -4, 0, 7, 9, 18, 9, 5, 9, 0, 0, 2,
		19, -4, 15, 12, 9, 0, 4, 10, 10, 33, 52, 18, 28, 35, 38, 34, 39, 32, 102, 0,
		56, 33, 29, 37, 51, 39, 52, 69, 67, 44, 32, 55, 32, 0, 27, 33, 34, 36, 38, 38,
		34, 35, 34, 32, 37, 35, 30, 28, 26, 29, 0, 2, 8, 14, 18, 17, 21, 17, 20, 18,
		27, 16, 7, 16, 11, 10, -10, -23, -15, -7, 0, -5, -11, -9, -9, -10, -34, -21, -3, -5,
		-7, -11, -15, -17, -25, -25, -28, -11, -10, -10, -10, -9, -16, -7, -4, -5, -9, 2, -9, 1,
		11, 5, -2, -2, 0, -8, 3, 7, 10, 17, 16, 3, -1, -5, -1, -4, -1, -21, -23, -20,
		-26, -25, -17, -27, -27, -17, -26, -27, -33, -10, -14, -8, -17, -28, -6, -2, -4, -9, -8, -1,
		5, 1, 9, 0, 1, 7, -7, -6, -16, -2, 2, -6, -3, 2, -3, -3, 0, 9, -1, -2,
		-2, -1, -9, 14, 16, 0, 18, 11, 12, 10, 2, 12, 13, 0, 3, 19, 3, 18, 19, 18,
		14, 26, 31, 33, 33, 37, 39, 42, 47, 45, 49, 41, 32, 69, 63, 66, 77, 54, 52, 41,
		36, 40, 30, 28, 23, 12, 11, 37, 39, 40, 38, 46, 42, 40, 49, 38, 40, 38, 46, 31,
		29, 25, 12, 11, 26, 22, 23, 27, 33, 26, 30, 27, 18, 25, 18, 12, 21, 14, 11, 0,
	},
	{	/* cabac_init_idc 2 */
		20, 2, 3, 20, 2, 3, -28, -23, -6, -1, 7, 29, 25, 14, -10, -3, -27, 26, -4, -24,
		5, 6, -17, 14, 20, 20, 29, 54, 37, 12, -32, -22, -2, -4, -24, 5, -6, -14, -6, 4,
		-11, -15, -21, 19, 20, 4, 6, 1, -5, -13, 5, 6, -3, -1, 3, -4, -2, -12, -7, 1,
		0, 0, 0, 0, -9, 4, 0, -7, 13, 3, 7, -9, -20, -36, -17, -14, -25, -25, -12, -17,
		-31, -14, -18, -13, -37, 11, 5, 2, 5, -6, 4, -14, -37, -5, -11, -11, -30, 0, -2, 0,
		-4, -6, 3, -8, -13, -4, -12, -5, -3, -4, -8, -16, -9, -1, 5, 4, -4, -2, 2, -1,
		-4, -1, 0, -7, -4, -6, -3, -6, 8, -9, -11, 9, 0, -5, 1, -15, -5, -8, -21, -21,
		-13, -25, -29, 9, 17, -8, -5, -2, 13, 3, -7, 8, -10, 3, -3, -20, 0, 1, -3, -21,
		16, -23, 17, 44, 50, -22, 4, 0, 7, 11, 8, 6, 7, 3, 8, 13, 13, 4, 3, 2,
		6, 8, 11, 14, 7, 4, 4, 13, 9, 19, 10, 12, 0, 20, 8, 35, 33, 28, 24, 27,
		34, 52, 39, 19, 31, 36, 24, 34, 30, 22, 20, 19, 27, 19, 15, 15, 21, 25, 30, 31,
		27, 24, 0, 14, 15, 26, -24, -24, -22, -9, 0, 0, -14, -13, -13, -11, -29, -21, -14, -12,
		-11, -10, -21, -16, -23, -15, -37, -10, -8, -8, -8, -7, -14, -10, -9, -12, -18, -4, -22, -16,
		-2, 1, -13, -9, -4, -13, -13, -6, -13, -6, -2, -16, -10, -13, -9, -10, -1, -22, -25, -25,
		-27, -19, -23, -25, -26, -24, -28, -31, -37, -10, -15, -10, -13, -50, -5, 17, -5, -13, -12, -2,
		0, -1, 4, -7, 5, 15, 1, 0, -10, 1, 0, 2, 0, -5, 7, 5, 2, 14, 15, 5,
		2, -2, -18, 12, 5, -12, 11, 5, 0, 2, -6, 5, 7, -6, -11, -2, -2, 25, 17, 16,
		17, 27, 37, 41, 42, 48, 39, 46, 52, 46, 52, 43, 32, 61, 56, 62, 81, 45, 35, 28,
		34, 39, 30, 20, 18, 15, 0, 36, 37, 37, 32, 34, 29, 24, 34, 31, 35, 31, 33, 36,
		27, 21, 18, 19, 36, 24, 27, 24, 31, 22, 22, 16, 15, 14, 3, -16, 21, 22, 25, 0,
	},
	{	/* I slices */
		20, 2, 3, 20, 2, 3, -28, -23, -6, -1, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		0, 0, 0, 0, -9, 4, 0, -7, 13, 3, 0, 1, 0, -17, -13, 0, -7, -21, -27, -31,
		-24, -18, -27, -21, -30, -17, -12, -16, -11, -12, -2, -15, -13, -3, -8, -10, -30, -1, -6, -7,
		-20, -4, -5, -7, -22, -7, -11, -3, -5, -4, -4, -12, -7, -7, 8, 5, -2, 1, 0, -2,
		1, 7, 10, 0, 11, 1, 0, 5, 31, 1, 7, 28, 16, 14, -13, -15, -13, -13, -12, -10,
		-16, -10, -7, -13, -19, 1, 0, -5, 18, -8, -15, 0, -4, 2, -11, -3, 15, -13, 0, 0,
		21, -15, 9, 16, 0, 12, 24, 15, 8, 13, 15, 13, 10, 12, 6, 20, 15, 4, 1, 0,
		7, 12, 11, 15, 11, 13, 16, 12, 10, 30, 18, 10, 17, 17, 0, 26, 22, 26, 30, 28,
		33, 37, 33, 40, 38, 33, 40, 41, 38, 41, 30, 27, 26, 37, 35, 38, 38, 37, 38, 42,
		35, 39, 14, 27, 21, 12, 2, -3, -6, -5, -3, -2, 0, 1, -2, -1, -9, -5, -5, -3,
		-2, 0, -16, -8, -10, -6, -10, -12, -15, -10, -6, -4, -12, -8, -7, -9, -17, -11, -20, -11,
		-6, -4, -13, -13, -11, -19, -8, -5, -4, -2, -3, -13, -10, -12, -13, -14, -1, -6, -6, -8,
		0, -1, 0, -2, -2, -5, -3, -4, -9, -1, 0, 3, 10, -7, 15, 14, 16, 12, 1, 20,
		18, 5, 1, 10, 17, 9, -12, -11, -16, -7, -8, -7, -9, -13, 4, -3, -3, -6, 10, -1,
		-1, -7, -14, 2, 0, -5, 0, -11, 1, 0, -14, 3, 4, -1, -13, 11, 5, 12, 15, 6,
		7, 12, 18, 13, 13, 15, 12, 13, 15, 14, 14, 17, 17, 24, 21, 25, 31, 22, 19, 14,
		10, 7, -2, -4, -3, 9, -12, 36, 36, 32, 37, 44, 34, 34, 40, 33, 35, 33, 38, 33,
		23, 13, 29, 26, 22, 31, 35, 34, 34, 36, 34, 32, 35, 34, 39, 30, 34, 29, 19, 0,
	},
};


static const int8_t cabacInitN[4][400] = {
	{	/* cabac_init_idc 0 */
		-15, 54, 74, -15, 54, 74, 127, 104, 53, 54, 51, 33, 2, 0, 9, 49, 118, 57, 78, 65,
		62, 49, 73, 50, 64, 43, 0, 67, 90, 104, 127, 104, 67, 78, 65, 62, 86, 95, 61, 45,
		69, 81, 96, 55, 67, 86, 88, 58, 76, 94, 54, 69, 81, 88, 67, 74, 74, 80, 72, 58,
		41, 63, 63, 63, 83, 86, 97, 72, 41, 62, 45, 78, 96, 126, 98, 101, 67, 82, 94, 83,
		110, 91, 102, 93, 127, 92, 89, 96, 108, 46, 65, 57, 93, 74, 92, 87, 126, 54, 60, 59,
		69, 48, 68, 69, 88, 85, 78, 75, 77, 54, 50, 68, 50, 42, 81, 63, 70, 67, 57, 76,
		35, 64, 61, 35, 25, 24, 29, 36, 93, 73, 73, 46, 49, 100, 53, 53, 53, 61, 56, 56,
		63, 60, 62, 57, 69, 57, 39, 51, 68, 64, 61, 63, 50, 39, 44, 52, 48, 60, 59, 59,
		33, 44, 43, 78, 60, 69, 28, 40, 44, 49, 46, 44, 51, 47, 39, 62, 46, 54, 54, 58,
		63, 51, 57, 53, 52, 55, 45, 36, 53, 82, 55, 78, 46, 31, 84, 7, -7, 3, 4, 0,
		-1, 6, 6, 9, 19, 27, 30, 20, 34, 70, 67, 59, 67, 30, 32, 35, 29, 31, 38, 43,
		41, 63, 59, 64, 94, 89, 108, 76, 44, 45, 52, 64, 59, 70, 75, 82, 102, 77, 24, 42,
		48, 55, 59, 71, 83, 87, 119, 58, 29, 36, 38, 43, 55, 58, 64, 74, 90, 70, 29, 31,
		42, 59, 58, 72, 81, 97, 58, 5, 14, 18, 27, 40, 58, 70, 79, 85, -1, 106, 106, 87,
		114, 110, 98, 110, 106, 103, 107, 108, 112, 96, 95, 91, 93, 94, 86, 67, 80, 85, 70, 60,
		58, 61, 50, 50, 49, 54, 41, 46, 51, 49, 52, 41, 47, 55, 41, 44, 50, 53, 49, 63,
		64, 69, 59, 70, 44, 31, 43, 53, 34, 38, 52, 40, 32, 44, 38, 50, 57, 43, 11, 14,
		11, 11, 9, -2, -15, -15, -21, -23, -33, -31, -28, -12, 29, -24, -45, -26, -43, -19, -10, 9,
		26, 27, 57, 82, 75, 97, 125, 0, 0, -4, 6, 8, 10, 22, 19, 32, 31, 41, 44, 47,
		65, 71, 60, 63, 65, 24, 20, 23, 32, 23, 24, 40, 32, 29, 42, 57, 53, 61, 86, 0,
	},
	{	/* cabac_init_idc 1 */
		-15, 54, 74, -15, 54, 74, 127, 104, 53, 54, 51, 25, 0, 0, 9, 41, 118, 65, 71, 79,
		52, 50, 70, 54, 34, 22, 0, 2, 36, 69, 127, 101, 76, 71, 79, 52, 69, 90, 52, 43,
		69, 82, 96, 59, 75, 87, 100, 56, 74, 85, 59, 81, 86, 95, 66, 77, 70, 86, 72, 61,
		41, 63, 63, 63, 83, 86, 97, 72, 41, 62, 15, 51, 80, 127, 91, 96, 81, 98, 102, 97,
		119, 99, 110, 102, 127, 80, 89, 94, 92, 39, 65, 84, 127, 73, 104, 91, 127, 55, 56, 55,
		61, 53, 68, 74, 88, 103, 91, 89, 92, 76, 87, 110, 105, 78, 112, 99, 127, 127, 127, 127,
		66, 78, 71, 72, 59, 55, 70, 75, 89, 119, 75, 20, 22, 127, 54, 61, 58, 60, 61, 67,
		84, 74, 65, 52, 57, 61, 69, 70, 55, 71, 58, 61, 41, 25, 32, 43, 47, 44, 51, 46,
		38, 66, 38, 42, 34, 89, 45, 28, 31, -11, -43, 15, 0, -22, -25, 0, -18, -12, -94, 0,
		-15, -4, 10, -5, -29, -9, -34, -58, -63, -5, 7, -29, 1, 0, 36, -25, -30, -28, -28, -27,
		-18, -16, -14, -8, -6, 0, 10, 18, 25, 41, 75, 72, 77, 35, 31, 35, 30, 45, 42, 45,
		26, 54, 66, 56, 73, 67, 116, 112, 71, 61, 53, 66, 77, 80, 84, 87, 127, 101, 39, 53,
		61, 75, 77, 91, 107, 111, 122, 76, 44, 52, 57, 58, 72, 69, 69, 74, 86, 66, 34, 32,
		31, 52, 55, 67, 73, 89, 52, 4, 8, 8, 19, 37, 61, 73, 70, 78, -1, 126, 124, 110,
		126, 124, 105, 121, 117, 102, 117, 116, 122, 95, 100, 95, 111, 114, 89, 80, 82, 85, 81, 72,
		64, 67, 56, 69, 69, 69, 69, 67, 77, 64, 61, 67, 64, 57, 65, 66, 62, 51, 66, 71,
		75, 70, 72, 60, 37, 47, 35, 37, 41, 41, 48, 41, 41, 59, 50, 40, 66, 50, -6, -6,
		0, -12, -16, -25, -22, -28, -30, -30, -42, -36, -34, -17, 9, -71, -63, -64, -74, -39, -35, -10,
		0, -1, 14, 26, 37, 55, 65, -33, -36, -37, -30, -33, -30, -24, -29, -12, -10, -3, -5, 20,
		30, 44, 48, 49, 45, 22, 22, 21, 20, 28, 24, 34, 42, 39, 50, 70, 54, 71, 83, 0,
	},
	{	/* cabac_init_idc 2 */
		-15, 54, 74, -15, 54, 74, 127, 104, 53, 54, 51, 16, 0, 0, 51, 62, 99, 16, 85, 102,
		57, 57, 73, 57, 40, 10, 0, 0, 42, 97, 127, 117, 74, 85, 102, 57, 93, 88, 44, 55,
		89, 103, 116, 57, 58, 84, 96, 63, 85, 106, 63, 75, 90, 101, 55, 79, 75, 97, 50, 60,
		41, 63, 63, 63, 83, 86, 97, 72, 41, 62, 34, 88, 127, 127, 91, 95, 84, 86, 89, 91,
		127, 76, 103, 90, 127, 80, 76, 84, 78, 55, 61, 83, 127, 79, 104, 91, 127, 65, 79, 72,
		92, 56, 68, 71, 98, 86, 88, 82, 72, 67, 72, 89, 69, 59, 66, 57, 71, 71, 58, 74,
		44, 69, 62, 51, 47, 42, 41, 53, 76, 78, 83, 52, 67, 90, 67, 72, 75, 80, 83, 64,
		31, 64, 94, 75, 63, 74, 35, 27, 91, 65, 69, 77, 66, 62, 68, 81, 30, 7, 23, 74,
		66, 124, 37, -18, -34, 127, 39, 42, 34, 29, 31, 37, 42, 40, 33, 43, 36, 47, 55, 58,
		60, 44, 44, 42, 48, 56, 52, 37, 49, 58, 48, 45, 69, 33, 63, -18, -25, -3, 10, 0,
		-14, -44, -24, 17, 25, 29, 33, 15, 20, 73, 34, 31, 44, 16, 36, 36, 28, 21, 20, 12,
		16, 42, 93, 56, 57, 38, 127, 115, 82, 62, 53, 59, 85, 89, 94, 92, 127, 100, 57, 67,
		71, 77, 85, 88, 104, 98, 127, 82, 48, 61, 66, 70, 75, 79, 83, 92, 108, 79, 69, 75,
		58, 58, 78, 83, 81, 99, 81, 38, 62, 58, 59, 73, 76, 86, 83, 87, -1, 127, 127, 120,
		127, 114, 117, 118, 117, 113, 118, 120, 124, 94, 102, 99, 106, 127, 92, 57, 86, 94, 91, 77,
		71, 73, 64, 81, 64, 57, 67, 68, 67, 68, 77, 64, 68, 78, 55, 59, 65, 54, 44, 60,
		70, 76, 86, 70, 64, 70, 55, 56, 69, 65, 74, 54, 54, 76, 82, 77, 77, 42, -13, -9,
		-12, -21, -30, -40, -41, -47, -32, -40, -51, -41, -39, -19, 11, -55, -46, -50, -67, -20, -2, 15,
		1, 1, 17, 38, 45, 54, 79, -16, -14, -17, 1, 15, 15, 25, 22, 16, 18, 28, 41, 28,
		47, 62, 31, 26, 24, 23, 16, 30, 29, 41, 42, 60, 52, 60, 78, 123, 53, 56, 61, 0,
	},
	{	/* I slices */
		-15, 54, 74, -15, 54, 74, 127, 104, 53, 54, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		41, 63, 63, 63, 83, 86, 97, 72, 41, 62, 11, 55, 69, 127, 102, 82, 74, 107, 127, 127,
		127, 95, 127, 114, 127, 123, 115, 122, 115, 63, 68, 84, 104, 70, 93, 90, 127, 74, 97, 91,
		127, 56, 82, 76, 125, 93, 87, 77, 71, 63, 68, 84, 62, 65, 61, 56, 66, 64, 61, 78,
		50, 52, 35, 44, 38, 45, 46, 44, 17, 51, 50, 19, 33, 62, 108, 100, 101, 91, 94, 88,
		84, 86, 83, 87, 94, 70, 72, 74, 59, 102, 100, 95, 75, 72, 75, 71, 46, 69, 62, 65,
		37, 72, 57, 54, 62, 72, 0, 9, 25, 18, 9, 19, 37, 18, 29, 33, 30, 45, 58, 62,
		61, 38, 45, 39, 42, 44, 45, 41, 49, 34, 42, 55, 51, 46, 89, -19, -17, -17, -25, -20,
		-23, -27, -23, -28, -17, -11, -15, -6, 1, 17, -6, 3, 22, -16, -4, -8, -3, 3, 5, 0,
		16, 22, 48, 37, 60, 68, 97, 71, 42, 50, 54, 62, 58, 63, 72, 74, 91, 67, 27, 39,
		44, 46, 64, 68, 78, 77, 86, 92, 55, 60, 62, 65, 73, 76, 80, 88, 110, 97, 84, 79,
		73, 74, 86, 96, 97, 117, 78, 33, 48, 53, 62, 71, 79, 86, 90, 97, -1, 93, 84, 79,
		66, 71, 62, 60, 59, 75, 62, 58, 66, 79, 71, 68, 44, 62, 36, 40, 27, 29, 44, 36,
		32, 42, 48, 62, 46, 64, 104, 97, 96, 88, 85, 85, 85, 88, 66, 77, 76, 76, 58, 76,
		83, 99, 95, 95, 76, 74, 70, 75, 68, 65, 73, 62, 62, 68, 75, 55, 64, 70, 6, 19,
		16, 14, 13, 11, 15, 16, 23, 23, 20, 26, 44, 40, 47, 17, 21, 22, 27, 29, 35, 50,
		57, 63, 77, 82, 94, 69, 109, -35, -34, -26, -30, -32, -18, -15, -15, -7, -5, 0, 2, 13,
		35, 58, -3, 0, 30, -7, -15, -3, 3, -1, 5, 11, 5, 12, 11, 29, 26, 39, 66, 0,
	},
};

/* mb_type of an I macroblock in a P or B slice, by its bins after the second */
static const uint8_t intraMbTypes[32] = {
	1, 1, 2, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9, 10, 11, 12,
	13, 13, 14, 14, 15, 15, 16, 16, 17, 18, 19, 20, 21, 22, 23, 24,
};

/* P mb_type by its second and third bins */
static const uint8_t pMbTypes[4] = { 0, 3, 2, 1 };

/*
 * B mb_type (0: an I type follows) and its bins, by the bins from the third,
 * shifted left: 6 bins take one more
 */
static const uint8_t bMbTypes[32][2] = {
	{ 3, 5 }, { 3, 5 }, { 4, 5 }, { 4, 5 }, { 5, 5 }, { 5, 5 }, { 6, 5 }, { 6, 5 },
	{ 7, 5 }, { 7, 5 }, { 8, 5 }, { 8, 5 }, { 9, 5 }, { 9, 5 }, { 10, 5 }, { 10, 5 },
	{ 12, 6 }, { 13, 6 }, { 14, 6 }, { 15, 6 }, { 16, 6 }, { 17, 6 }, { 18, 6 }, { 19, 6 },
	{ 20, 6 }, { 21, 6 }, { 0, 5 }, { 0, 5 }, { 11, 5 }, { 11, 5 }, { 22, 5 }, { 22, 5 },
};

/*
 * B sub_mb_type and its bins, by the bins from the third, shifted left: 5
 * bins take one more
 */
static const uint8_t bSubMbTypes[16][2] = {
	{ 3, 4 }, { 3, 4 }, { 4, 4 }, { 4, 4 }, { 5, 4 }, { 5, 4 }, { 6, 4 }, { 6, 4 },
	{ 7, 5 }, { 8, 5 }, { 9, 5 }, { 10, 5 }, { 11, 4 }, { 11, 4 }, { 12, 4 }, { 12, 4 },
};

/* 9.3.3.2.1.1: rangeTabLPS, by pStateIdx and qCodIRangeIdx */
static const uint8_t rangeTabLps[64][4] = {
	{ 128, 176, 208, 240 }, { 128, 167, 197, 227 },
	{ 128, 158, 187, 216 }, { 123, 150, 178, 205 },
	{ 116, 142, 169, 195 }, { 111, 135, 160, 185 },
	{ 105, 128, 152, 175 }, { 100, 122, 144, 166 },
	{ 95, 116, 137, 158 }, { 90, 110, 130, 150 },
	{ 85, 104, 123, 142 }, { 81, 99, 117, 135 },
	{ 77, 94, 111, 128 }, { 73, 89, 105, 122 },
	{ 69, 85, 100, 116 }, { 66, 80, 95, 110 },
	{ 62, 76, 90, 104 }, { 59, 72, 86, 99 },
	{ 56, 69, 81, 94 }, { 53, 65, 77, 89 },
	{ 51, 62, 73, 85 }, { 48, 59, 69, 80 },
	{ 46, 56, 66, 76 }, { 43, 53, 63, 72 },
	{ 41, 50, 59, 69 }, { 39, 48, 56, 65 },
	{ 37, 45, 54, 62 }, { 35, 43, 51, 59 },
	{ 33, 41, 48, 56 }, { 32, 39, 46, 53 },
	{ 30, 37, 43, 50 }, { 29, 35, 41, 48 },
	{ 27, 33, 39, 45 }, { 26, 31, 37, 43 },
	{ 24, 30, 35, 41 }, { 23, 28, 33, 39 },
	{ 22, 27, 32, 37 }, { 21, 26, 30, 35 },
	{ 20, 24, 29, 33 }, { 19, 23, 27, 31 },
	{ 18, 22, 26, 30 }, { 17, 21, 25, 28 },
	{ 16, 20, 23, 27 }, { 15, 19, 22, 25 },
	{ 14, 18, 21, 24 }, { 14, 17, 20, 23 },
	{ 13, 16, 19, 22 }, { 12, 15, 18, 21 },
	{ 12, 14, 17, 20 }, { 11, 14, 16, 19 },
	{ 11, 13, 15, 18 }, { 10, 12, 15, 17 },
	{ 10, 12, 14, 16 }, { 9, 11, 13, 15 },
	{ 9, 11, 12, 14 }, { 8, 10, 12, 14 },
	{ 8, 9, 11, 13 }, { 7, 9, 11, 12 },
	{ 7, 9, 10, 12 }, { 7, 8, 10, 11 },
	{ 6, 8, 9, 11 }, { 6, 7, 9, 10 },
	{ 6, 7, 8, 9 }, { 2, 2, 2, 2 },
};

/* 9.3.3.2.1.2: transIdxLPS and transIdxMPS, by pStateIdx */
static const uint8_t transIdxLps[64] = {
	0, 0, 1, 2, 2, 4, 4, 5, 6, 7, 8, 9, 9, 11, 11, 12,
	13, 13, 15, 15, 16, 16, 18, 18, 19, 19, 21, 21, 22, 22, 23, 24,
	24, 25, 26, 26, 27, 27, 28, 29, 29, 30, 30, 30, 31, 32, 32, 33,
	33, 33, 34, 34, 35, 35, 35, 36, 36, 36, 37, 37, 37, 38, 38, 63,
};
static const uint8_t transIdxMps[64] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
	33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
	49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 62, 63,
};

/* the 4x4 blocks' zig-zag scan */
static const uint8_t zigzag4x4[16] = {
	0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15,
};

/* the B partitions using a list: by list, partition and mb_type */
static const uint8_t bPartitionLists[2][2][23] = {
	{
		{ 0, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0 },
		{ 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0 },
	},
	{
		{ 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0 },
		{ 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0 },
	},
};

/* the motion vectors of a B sub-macroblock in a list: by list and sub_mb_type */
static const uint8_t bSubMbLists[2][13] = {
	{ 0, 1, 0, 1, 2, 2, 0, 0, 2, 2, 4, 0, 4 },
	{ 0, 0, 1, 1, 0, 0, 2, 2, 2, 2, 0, 4, 4 },
};

static uint32_t decodeDecision(VideoAvcCabac *cabac, uint32_t ctxIdx);
static uint32_t decodeBypass(VideoAvcCabac *cabac);
static uint32_t decodeTerminate(VideoAvcCabac *cabac);
static void engineInit(VideoAvcCabac *cabac);
static void contextsInit(VideoAvcCabac *cabac, uint32_t cabacInitIdc, uint32_t qp);
static void macroblockI(VideoSliceJob *job);
static void macroblockP(VideoSliceJob *job);
static void macroblockB(VideoSliceJob *job);
static void macroblockSkip(VideoSliceJob *job);
static uint32_t residual16x16(VideoSliceJob *job, uint32_t lumaCbp);
static uint32_t residual4x4(VideoSliceJob *job, uint32_t lumaCbp);
static uint32_t residualChroma(VideoSliceJob *job, uint32_t chromaCbp);

/* the left macroblock's absMvd column, as no macroblock left of the row's first */
static inline void leftAbsMvdClear(VideoAvcCabacMb *left)
{
	uint32_t list, i;

	for (list = 0; list < 2; list++)
		for (i = 0; i < 4; i++)
			left->absMvd[list][i] = 0;
}

/* the cbf words of a neighbour: all coded, or none */
static inline void cbfAllCoded(VideoAvcCabacMb *mb)
{
	mb->lumaCbf = 0xffff;
	mb->chromaDcCbf[0] = 1;
	mb->chromaDcCbf[1] = 1;
	mb->chromaAcCbf[0] = 0xff;
	mb->chromaAcCbf[1] = 0xff;
	mb->reserved[0] = 1;
	mb->reserved[1] = 1;
}

static inline void cbfNoneCoded(VideoAvcCabacMb *mb)
{
	mb->lumaCbf = 0;
	mb->chromaDcCbf[0] = 0;
	mb->chromaDcCbf[1] = 0;
	mb->chromaAcCbf[0] = 0;
	mb->chromaAcCbf[1] = 0;
	mb->reserved[0] = 0;
	mb->reserved[1] = 0;
}

/* the absMvd a neighbour keeps of a motion vector difference (x | y << 16) */
static inline uint32_t absMvdOf(uint32_t mvd)
{
	int32_t x = (int16_t)mvd, y = (int16_t)(mvd >> 16);

	x = x < 0 ? -x : x;
	y = y < 0 ? -y : y;
	x = x < 33 ? x : 33;
	y = y < 33 ? y : 33;
	return (uint32_t)x | (uint32_t)y << 8;
}

/*
 * The row buffer of a slice that starts here: its entries as macroblocks
 * outside the slice, with the cbf of the I slices' unavailable neighbours.
 */
static void neighboursInit(VideoSliceJob *job)
{
	const VideoNal *nal = job->parser.nal;
	VideoAvcCabacMb *row = job->rowBuffer;
	uint32_t intra = (nal->sliceHeader & AVC_SLICE_TYPE) == AVC_SLICE_I;
	uint32_t coded = intra ? 0xffff : 0;
	int32_t i;

	leftAbsMvdClear(row);
	row->chromaDcCbf[0] = intra;
	row->chromaDcCbf[1] = intra;
	row->unavailable = 1;
	row->chromaAcCbf[1] = coded;
	row->chromaAcCbf[0] = coded;
	row->lumaCbf = coded;
	row->lumaDcCbf = 1;
	row->qpDeltaNonZero = 0;
	row->cbp = 0;
	row->refIdxNonZero = 0;
	row->chromaPredMode = 0;
	row->coded = 0;
	row->mbTypeBin = 0;
	for (i = 1; i <= nal->widthInMacroblocks + 1; i++)
		row[i] = row[0];
}

/* 9.3.1: the contexts and the engine, at the slice data's first byte */
static void sliceInit(VideoSliceJob *job)
{
	const VideoNal *nal = job->parser.nal;
	uint32_t cabacInitIdc = 3;

	if ((nal->sliceHeader & AVC_SLICE_TYPE) != AVC_SLICE_I)
		cabacInitIdc = nal->cabacInitIdc;
	contextsInit(job->cabac, cabacInitIdc, nal->sliceQp);
	engineInit(job->cabac);
}

/* 9.3.1.1: the context variables of cabac_init_idc (3: an I slice) at the QP */
static void contextsInit(VideoAvcCabac *cabac, uint32_t cabacInitIdc, uint32_t qp)
{
	const int8_t *m = cabacInitM[cabacInitIdc], *n = cabacInitN[cabacInitIdc];
	int32_t sliceQp = qp < 51 ? qp : 51, preCtxState, i;

	sliceQp = sliceQp > 0 ? sliceQp : 0;
	for (i = 0; i < AVC_CABAC_CONTEXTS; i++) {
		preCtxState = ((m[i] * sliceQp) >> 4) + n[i];
		if (preCtxState < 126) {
			if (preCtxState < 2)
				preCtxState = 1;
		} else {
			preCtxState = 126;
		}
		if (preCtxState < 64)
			cabac->contexts[i] = (63 - preCtxState) << 1;
		else
			cabac->contexts[i] = (preCtxState - 64) << 1 | 1;
	}
	cabac->contexts[AVC_CABAC_END_OF_SLICE] = 63 << 1;
}

/* 9.3.1.2 */
static void engineInit(VideoAvcCabac *cabac)
{
	cabac->range = 510;
	cabac->offset = videoBitsRead(cabac->bits, 9);
}

/* mb_skip_flag: the macroblock is skipped */
static uint32_t decodeSkip(VideoSliceJob *job, uint32_t ctxIdxOffset)
{
	VideoAvcCabacMb *mb = job->mb;
	uint32_t skipped;

	skipped = decodeDecision(job->cabac, ctxIdxOffset + (mb[-1].coded + mb->coded));
	mb->coded = !skipped;
	return skipped;
}

/* an I mb_type's suffix in a P or B slice: base and the I type */
static uint32_t decodeIntraMbType(VideoAvcCabac *cabac, uint32_t ctxIdx, uint32_t base)
{
	uint32_t bins, bin, type;

	if (!decodeDecision(cabac, ctxIdx)) {
		type = I_NXN;
	} else if (decodeTerminate(cabac)) {
		type = I_PCM;
	} else {
		bins = decodeDecision(cabac, ctxIdx + 1);
		bin = decodeDecision(cabac, ctxIdx + 2);
		bins = bins << 1 | bin;
		if (bin) {
			bins = bins << 1 | decodeDecision(cabac, ctxIdx + 2);
			bins = bins << 1 | decodeDecision(cabac, ctxIdx + 3);
			bins <<= 1;
			bins |= decodeDecision(cabac, ctxIdx + 3);
		} else {
			bins = bins << 1 | decodeDecision(cabac, ctxIdx + 3);
			bins = bins << 1 | decodeDecision(cabac, ctxIdx + 3);
			bins <<= 1;
		}
		type = intraMbTypes[bins];
	}
	return base + type;
}

/*
 * coded_block_pattern: the luma prefix by 8x8 block, then the chroma
 * suffix. The neighbours keep the luma bits inverted.
 */
static uint32_t decodeCbp(VideoSliceJob *job)
{
	VideoAvcCabac *cabac = job->cabac;
	uint32_t top = job->mb->cbp, left = job->mb[-1].cbp, cbp;

	cbp = decodeDecision(cabac, 73 + ((left & 2) >> 1) + ((top & 4) >> 1));
	cbp |= decodeDecision(cabac, 73 + (~cbp & 1) + ((top & 8) >> 2)) << 1;
	cbp |= decodeDecision(cabac, 73 + ((~cbp & 1) << 1) + ((left & 8) >> 3)) << 2;
	cbp |= decodeDecision(cabac, 73 + ((~cbp & 4) >> 2) + (~cbp & 2)) << 3;
	if (decodeDecision(cabac, 77 + ((top & 0x30) != 0) * 2 + ((left & 0x30) != 0)))
		cbp |= (decodeDecision(cabac, 81 + ((int32_t)top >= 32) * 2 +
			((int32_t)left >= 32)) + 1) << 4;
	return cbp;
}

/* mb_qp_delta: unary, mapped to a signed value */
static int32_t decodeQpDelta(VideoSliceJob *job)
{
	VideoAvcCabac *cabac = job->cabac;
	int32_t value = 0;

	if (decodeDecision(cabac, 60 + job->mb[-1].qpDeltaNonZero)) {
		value = 1;
		if (decodeDecision(cabac, 62)) {
			do
				value++;
			while (decodeDecision(cabac, 63));
		}
	}
	return (value & 1) ? (value + 1) >> 1 : -((value + 1) >> 1);
}

/* 9.3.3.2.1: a bin of the context */
static uint32_t decodeDecision(VideoAvcCabac *cabac, uint32_t ctxIdx)
{
	uint8_t *context = &cabac->contexts[ctxIdx];
	uint32_t state = *context, pStateIdx = state >> 1, valMps = state & 1;
	uint32_t range = cabac->range, offset = cabac->offset, rangeLps, bin;
	int32_t shift;

	rangeLps = rangeTabLps[pStateIdx][(range >> 6) & 3];
	range -= rangeLps;
	if ((int32_t)offset < (int32_t)range) {
		bin = valMps;
		*context = transIdxMps[pStateIdx] << 1 | valMps;
	} else {
		cabac->offset = offset - range;
		range = rangeLps;
		bin = 1 - valMps;
		if (pStateIdx == 0)
			valMps = 1 - valMps;
		*context = transIdxLps[pStateIdx] << 1 | valMps;
	}
	/* 9.3.3.2.2: renormalised to at least 256 */
	shift = __builtin_clz(range << 23);
	if (shift >= 1) {
		range <<= shift;
		cabac->range = range;
		cabac->offset = cabac->offset << shift | videoBitsRead(cabac->bits, shift);
	} else {
		cabac->range = range;
	}
	return bin;
}

/* 9.3.3.2.3 */
static uint32_t decodeBypass(VideoAvcCabac *cabac)
{
	uint32_t offset, bin = 0;

	cabac->offset <<= 1;
	offset = cabac->offset | videoBitsRead(cabac->bits, 1);
	if (offset >= cabac->range) {
		bin = 1;
		offset -= cabac->range;
	}
	cabac->offset = offset;
	return bin;
}

/* 9.3.3.2.2.3: end_of_slice_flag and the PCM escape, not renormalised when 1 */
static uint32_t decodeTerminate(VideoAvcCabac *cabac)
{
	uint32_t range = cabac->range - 2;
	int32_t shift;

	cabac->range = range;
	if (cabac->offset >= range)
		return 1;
	shift = __builtin_clz(range << 23);
	if (shift >= 1) {
		cabac->range = range << shift;
		cabac->offset = cabac->offset << shift | videoBitsRead(cabac->bits, shift);
	}
	return 0;
}

/* coeff_abs_level_minus1: a truncated unary prefix of 14, an Exp-Golomb suffix */
static int32_t decodeAbsLevel(VideoAvcCabac *cabac, int32_t numDecodAbsLevelGt1,
	int32_t numDecodAbsLevelEq1, uint32_t ctxBlockCatOffset)
{
	uint32_t ctxIdx, increment = 0, suffix;
	int32_t prefix, k, i;

	if (numDecodAbsLevelGt1 == 0) {
		increment = 4;
		if (numDecodAbsLevelEq1 + 1 < 4)
			increment = numDecodAbsLevelEq1 + 1;
	}
	if (!decodeDecision(cabac, 227 + ctxBlockCatOffset + increment))
		return 0;
	ctxIdx = 232 + ctxBlockCatOffset +
		(numDecodAbsLevelGt1 < 4 ? numDecodAbsLevelGt1 : 4);
	for (prefix = 1; prefix < 14; prefix++)
		if (!decodeDecision(cabac, ctxIdx))
			return prefix;
	for (k = 0; k < 14; k++)
		if (!decodeBypass(cabac))
			break;
	suffix = 1;
	for (i = 0; i < k; i++)
		suffix = suffix << 1 | decodeBypass(cabac);
	return suffix + 13;
}

/*
 * A block's significance map, then its levels from the last, into the
 * block cleared first: by the 4x4 zig-zag scan, from scan[0].
 */
static inline __attribute__((always_inline)) void residualBlock(VideoAvcCabac *cabac,
	int16_t *block, uint32_t sigCtx, uint32_t lastCtx, uint32_t ctxBlockCatOffset,
	int32_t maxNumCoeff, const uint8_t *scan, int32_t blockSize)
{
	uint32_t significant[16];
	int32_t numCoeff = maxNumCoeff, numGt1, numEq1, level, coeff, i;

	for (i = 0; i < numCoeff - 1; i++) {
		significant[i] = decodeDecision(cabac, sigCtx + i);
		if (significant[i] && decodeDecision(cabac, lastCtx + i))
			numCoeff = i + 1;
	}
	for (i = 0; i < blockSize; i++)
		block[i] = 0;
	level = decodeAbsLevel(cabac, 0, 0, ctxBlockCatOffset);
	coeff = level + 1;
	if (decodeBypass(cabac))
		coeff = -coeff;
	block[scan ? scan[numCoeff - 1] : numCoeff - 1] = coeff;
	numGt1 = 0;
	numEq1 = 0;
	if (level)
		numGt1 = 1;
	else
		numEq1 = 1;
	for (i = numCoeff - 2; i >= 0; i--) {
		if (!significant[i])
			continue;
		level = decodeAbsLevel(cabac, numGt1, numEq1, ctxBlockCatOffset);
		coeff = level + 1;
		if (decodeBypass(cabac))
			coeff = -coeff;
		block[scan ? scan[i] : i] = coeff;
		if (level)
			numGt1++;
		else
			numEq1++;
	}
}

/* ctxBlockCat 0: Intra16x16DCLevel */
static void residualLumaDc(VideoAvcCabac *cabac, int16_t *block)
{
	residualBlock(cabac, block, 105, 166, 0, 16, zigzag4x4, 16);
}

/* ctxBlockCat 1: Intra16x16ACLevel */
static void residualLumaAc(VideoAvcCabac *cabac, int16_t *block)
{
	residualBlock(cabac, block, 120, 181, 10, 15, zigzag4x4 + 1, 16);
}

/* ctxBlockCat 2: LumaLevel4x4 */
static void residualLuma(VideoAvcCabac *cabac, int16_t *block)
{
	residualBlock(cabac, block, 134, 195, 20, 16, zigzag4x4, 16);
}

/* ctxBlockCat 3: ChromaDCLevel */
static void residualChromaDc(VideoAvcCabac *cabac, int16_t *block)
{
	residualBlock(cabac, block, 149, 210, 30, 4, 0, 4);
}

/* ctxBlockCat 4: ChromaACLevel */
static void residualChromaAc(VideoAvcCabac *cabac, int16_t *block)
{
	residualBlock(cabac, block, 152, 213, 39, 15, zigzag4x4 + 1, 16);
}

/* ref_idx_lX: unary */
static int32_t decodeRefIdx(VideoAvcCabac *cabac, uint32_t increment)
{
	int32_t value = 0;

	if (decodeDecision(cabac, 54 + increment)) {
		value = 1;
		if (decodeDecision(cabac, 58)) {
			do
				value++;
			while (decodeDecision(cabac, 59));
		}
	}
	return value;
}

/* mvd_lX: a truncated unary prefix of 9, an Exp-Golomb suffix of order 3, a sign */
static int32_t decodeMvd(VideoAvcCabac *cabac, uint32_t ctxIdxOffset, uint32_t increment)
{
	int32_t value, k, i;
	uint32_t suffix;

	if (!decodeDecision(cabac, ctxIdxOffset + increment))
		return 0;
	if (!decodeDecision(cabac, ctxIdxOffset + 3)) {
		value = 1;
	} else if (!decodeDecision(cabac, ctxIdxOffset + 4)) {
		value = 2;
	} else if (!decodeDecision(cabac, ctxIdxOffset + 5)) {
		value = 3;
	} else {
		value = 4;
		while (decodeDecision(cabac, ctxIdxOffset + 6))
			if (++value >= 9)
				break;
		if (value >= 9) {
			for (k = 0; k < 16; k++)
				if (!decodeBypass(cabac))
					break;
			suffix = 1;
			for (i = 0; i < k + 3; i++)
				suffix = suffix << 1 | decodeBypass(cabac);
			value = suffix + 1;
		}
	}
	if (decodeBypass(cabac))
		value = -value;
	return value;
}

/* a motion vector difference by the absMvd of the left (a) and the top (b) */
static uint32_t decodeMvdPair(VideoAvcCabac *cabac, uint32_t a, uint32_t b)
{
	int8_t sumX = (int8_t)(a + b), sumY = (int8_t)((a >> 8) + (b >> 8));
	int32_t x, y;

	x = decodeMvd(cabac, 40, (sumX > 2) + (sumX > 32));
	y = decodeMvd(cabac, 47, (sumY > 2) + (sumY > 32));
	return ((uint32_t)x & 0xffff) | (uint32_t)y << 16;
}

/* an I_PCM macroblock: its samples, then the engine again */
static void decodePcm(VideoSliceJob *job)
{
	VideoBitstream *bits = job->parser.bits;
	VideoAvcCabacMb *mb;
	uint8_t *samples;
	int32_t i;

	while (!videoBitsAligned(bits))
		videoBitsRead(bits, 1);
	samples = job->pcmLuma;
	for (i = 0; i < 256; i++)
		*samples++ = videoBitsRead(bits, 8);
	samples = job->pcmCb;
	for (i = 0; i < 64; i++)
		*samples++ = videoBitsRead(bits, 8);
	samples = job->pcmCr;
	for (i = 0; i < 64; i++)
		*samples++ = videoBitsRead(bits, 8);
	job->pcmLuma += 256;
	job->pcmCb += 64;
	job->pcmCr += 64;
	engineInit(job->cabac);
	mb = job->mb;
	mb->unavailable = 0;
	mb->chromaAcCbf[1] = 0xff;
	mb->chromaDcCbf[1] = 1;
	mb->chromaAcCbf[0] = 0xff;
	mb->chromaDcCbf[0] = 1;
	mb->lumaCbf = 0xffff;
	mb->lumaDcCbf = 1;
	mb->qpDeltaNonZero = 0;
	mb->cbp = 0x30;
	mb->chromaPredMode = 0;
	mb->mbTypeBin = 1;
}

/*
 * An intra macroblock's prediction modes: an I_NxN's 16 luma modes (-1 the
 * predicted one), then intra_chroma_pred_mode.
 */
static void decodeIntraModes(VideoSliceJob *job, uint32_t intraType)
{
	VideoAvcCabacMb *mb;
	uint32_t mode;
	int32_t i;

	if (intraType == I_NXN) {
		for (i = 0; i < 16; i++) {
			if (decodeDecision(job->cabac, 68)) {
				job->intraModes[i] = -1;
			} else {
				mode = decodeDecision(job->cabac, 69);
				mode |= decodeDecision(job->cabac, 69) << 1;
				mode |= decodeDecision(job->cabac, 69) << 2;
				job->intraModes[i] = mode;
			}
		}
		job->intraModes += 16;
	}
	mb = job->mb;
	mode = decodeDecision(job->cabac, 64 + mb[-1].chromaPredMode + mb->chromaPredMode);
	if (mode && decodeDecision(job->cabac, 67))
		mode = decodeDecision(job->cabac, 67) + 2;
	mb->chromaPredMode = mode != 0;
	*job->chromaModes = mode;
	job->chromaModes++;
}

/* a P macroblock's partitions: 16x16, 16x8 or 8x16 */
static void partitionsP(VideoSliceJob *job, uint32_t type)
{
	VideoAvcCabac *cabac = job->cabac;
	VideoAvcCabacMb *mb = job->mb;
	uint16_t *left, *top;
	uint32_t refIdxBits = 0, leftBits, topBits, mvd, mvd0, mvd1;
	int8_t *refIdx;
	int32_t value;

	mb->chromaPredMode = 0;
	if (job->numRefIdxActive[0] >= 2) {
		refIdx = job->refIdx;
		leftBits = mb[-1].refIdxNonZero;
		topBits = mb->refIdxNonZero;
		value = decodeRefIdx(cabac, ((topBits >> 2) & 1) * 2 + ((leftBits >> 1) & 1));
		refIdx[0] = value;
		if (type == 0) {
			if (value)
				refIdxBits = 15;
		} else if (type == 1) {
			if (value)
				refIdxBits = 3;
			value = decodeRefIdx(cabac, (refIdxBits & 1) * 2 + ((leftBits >> 3) & 1));
			refIdx[1] = value;
			if (value)
				refIdxBits |= 0xc;
		} else {
			if (value)
				refIdxBits = 5;
			value = decodeRefIdx(cabac, ((topBits >> 3) & 1) * 2 + (refIdxBits & 1));
			refIdx[1] = value;
			if (value)
				refIdxBits |= 0xa;
		}
		job->refIdx = refIdx + 8;
	}
	mb->refIdxNonZero = refIdxBits;
	left = job->rowBuffer->absMvd[0];
	top = mb->absMvd[0];
	mvd = decodeMvdPair(cabac, left[0], top[0]);
	*job->mvd++ = mvd;
	mvd0 = absMvdOf(mvd);
	if (type == 0) {
		left[3] = left[2] = left[1] = left[0] = mvd0;
		top[3] = top[2] = top[1] = top[0] = mvd0;
	} else if (type == 1) {
		mvd = decodeMvdPair(cabac, left[2], mvd0);
		*job->mvd++ = mvd;
		mvd1 = absMvdOf(mvd);
		left[1] = left[0] = mvd0;
		left[3] = left[2] = mvd1;
		top[3] = top[2] = top[1] = top[0] = mvd1;
	} else {
		mvd = decodeMvdPair(cabac, mvd0, top[2]);
		*job->mvd++ = mvd;
		mvd1 = absMvdOf(mvd);
		left[3] = left[2] = left[1] = left[0] = mvd1;
		top[1] = top[0] = mvd0;
		top[3] = top[2] = mvd1;
	}
}

/*
 * A B macroblock's partitions, by list: the reference indices of the
 * partitions using it, then their motion vector differences.
 */
static void partitionsB(VideoSliceJob *job, uint32_t type)
{
	VideoAvcCabac *cabac = job->cabac;
	VideoAvcCabacMb *mb = job->mb;
	uint32_t refIdxBits = 0, leftBits, topBits, odd = type & 1, list, mvd, mvd0, mvd1;
	uint16_t *left, *top;
	int8_t *refIdx = job->refIdx;
	int32_t value, used = 0;

	mb->chromaPredMode = 0;
	leftBits = mb[-1].refIdxNonZero;
	topBits = mb->refIdxNonZero;
	if (job->numRefIdxActive[0] >= 2) {
		if (bPartitionLists[0][0][type]) {
			value = decodeRefIdx(cabac, ((topBits >> 2) & 1) * 2 + ((leftBits >> 1) & 1));
			used = 1;
			refIdx[0] = value;
			if (value)
				refIdxBits = type < 4 ? 15 : odd ? 5 : 3;
		}
		if (bPartitionLists[0][1][type]) {
			if (!odd) {
				value = decodeRefIdx(cabac, (refIdxBits & 1) * 2 + ((leftBits >> 3) & 1));
				if (value)
					refIdxBits |= 0xc;
			} else {
				value = decodeRefIdx(cabac, ((topBits >> 3) & 1) * 2 + (refIdxBits & 1));
				if (value)
					refIdxBits |= 0xa;
			}
			refIdx[1] = value;
			used++;
		}
	}
	if (job->numRefIdxActive[1] >= 2) {
		if (bPartitionLists[1][0][type]) {
			value = decodeRefIdx(cabac, ((topBits >> 6) & 1) * 2 + ((leftBits >> 5) & 1));
			used++;
			refIdx[4] = value;
			if (value)
				refIdxBits |= type < 4 ? 0xf0 : odd ? 0x50 : 0x30;
		}
		if (bPartitionLists[1][1][type]) {
			if (!odd) {
				value = decodeRefIdx(cabac, ((refIdxBits >> 4) & 1) * 2 +
					((leftBits >> 7) & 1));
				if (value)
					refIdxBits |= 0xc0;
			} else {
				value = decodeRefIdx(cabac, ((topBits >> 7) & 1) * 2 +
					((refIdxBits >> 4) & 1));
				if (value)
					refIdxBits |= 0xa0;
			}
			refIdx[5] = value;
			used++;
		}
	}
	if (used >= 1)
		job->refIdx = refIdx + 8;
	mb->refIdxNonZero = refIdxBits;
	for (list = 0; list < 2; list++) {
		left = job->rowBuffer->absMvd[list];
		top = mb->absMvd[list];
		mvd0 = 0;
		if (bPartitionLists[list][0][type]) {
			mvd = decodeMvdPair(cabac, left[0], top[0]);
			*job->mvd++ = mvd;
			mvd0 = absMvdOf(mvd);
		}
		if (type < 4) {
			left[3] = left[2] = left[1] = left[0] = mvd0;
			top[1] = top[0] = mvd0;
			top[2] = mvd0;
			top[3] = mvd0;
		} else if (!odd) {
			mvd1 = 0;
			if (bPartitionLists[list][1][type]) {
				mvd = decodeMvdPair(cabac, left[2], mvd0);
				*job->mvd++ = mvd;
				mvd1 = absMvdOf(mvd);
			}
			left[1] = left[0] = mvd0;
			left[3] = left[2] = mvd1;
			top[2] = top[1] = top[0] = mvd1;
			top[3] = mvd1;
		} else {
			mvd1 = 0;
			if (bPartitionLists[list][1][type]) {
				mvd = decodeMvdPair(cabac, mvd0, top[2]);
				*job->mvd++ = mvd;
				mvd1 = absMvdOf(mvd);
			}
			left[3] = left[2] = left[1] = left[0] = mvd1;
			top[1] = top[0] = mvd0;
			top[2] = mvd1;
			top[3] = mvd1;
		}
	}
}

/*
 * The motion vector differences of an 8x8 block of the shape, by the
 * absMvd of its left (row) and top (column) neighbours, which it replaces.
 */
static inline __attribute__((always_inline)) uint32_t *subMbMvds(VideoAvcCabac *cabac,
	uint16_t *left, uint16_t *top, uint32_t *out, uint32_t shape)
{
	uint32_t mvd, mvd0, mvd1, mvd2, mvd3;

	mvd = decodeMvdPair(cabac, left[0], top[0]);
	*out++ = mvd;
	mvd0 = absMvdOf(mvd);
	if (shape == SUB_8X8) {
		left[1] = mvd0;
		left[0] = mvd0;
		top[0] = mvd0;
		top[1] = mvd0;
	} else if (shape == SUB_8X4) {
		mvd = decodeMvdPair(cabac, left[1], mvd0);
		*out++ = mvd;
		mvd1 = absMvdOf(mvd);
		left[0] = mvd0;
		left[1] = mvd1;
		top[0] = mvd1;
		top[1] = mvd1;
	} else {
		mvd = decodeMvdPair(cabac, mvd0, top[1]);
		*out++ = mvd;
		mvd1 = absMvdOf(mvd);
		if (shape == SUB_4X8) {
			left[1] = mvd1;
			left[0] = mvd1;
			top[0] = mvd0;
			top[1] = mvd1;
		} else {
			mvd = decodeMvdPair(cabac, left[1], mvd0);
			*out++ = mvd;
			mvd2 = absMvdOf(mvd);
			mvd = decodeMvdPair(cabac, mvd2, mvd1);
			*out++ = mvd;
			mvd3 = absMvdOf(mvd);
			left[0] = mvd1;
			left[1] = mvd3;
			top[0] = mvd2;
			top[1] = mvd3;
		}
	}
	return out;
}

/* P_8x8: sub_mb_type, the 8x8 blocks' reference indices, then their mvd */
static void subMbP(VideoSliceJob *job, uint32_t type)
{
	VideoAvcCabac *cabac = job->cabac;
	VideoAvcCabacMb *mb = job->mb;
	VideoAvcMbInfo *info = job->mbInfo;
	uint32_t subMbType[4], refIdxBits = 0, leftBits, topBits, sub, i;
	int8_t *refIdx;
	int32_t value;

	mb->chromaPredMode = 0;
	for (i = 0; i < 4; i++) {
		if (decodeDecision(job->cabac, 21))
			sub = SUB_8X8;
		else if (!decodeDecision(job->cabac, 22))
			sub = SUB_8X4;
		else if (decodeDecision(job->cabac, 23))
			sub = SUB_4X8;
		else
			sub = SUB_4X4;
		subMbType[i] = sub;
		info->subMbType[i] = sub;
	}
	if (job->numRefIdxActive[0] >= 2 && type != P_8X8_REF0) {
		refIdx = job->refIdx;
		topBits = mb->refIdxNonZero;
		leftBits = mb[-1].refIdxNonZero;
		value = decodeRefIdx(cabac, ((topBits >> 2) & 1) * 2 + ((leftBits >> 1) & 1));
		refIdx[0] = value;
		if (value)
			refIdxBits = 1;
		value = decodeRefIdx(cabac, ((topBits >> 3) & 1) * 2 + (refIdxBits & 1));
		refIdx[1] = value;
		if (value)
			refIdxBits |= 2;
		value = decodeRefIdx(cabac, (refIdxBits & 1) * 2 + ((leftBits >> 3) & 1));
		refIdx[2] = value;
		if (value)
			refIdxBits |= 4;
		value = decodeRefIdx(cabac, ((refIdxBits >> 1) & 1) * 2 + ((refIdxBits >> 2) & 1));
		refIdx[3] = value;
		if (value)
			refIdxBits |= 8;
		job->refIdx = refIdx + 8;
	}
	mb->refIdxNonZero = refIdxBits;
	for (i = 0; i < 4; i++)
		job->mvd = subMbMvds(cabac, &job->rowBuffer->absMvd[0][i & 2],
			&mb->absMvd[0][(i & 1) * 2], job->mvd, subMbType[i]);
}

/* B_8x8: sub_mb_type, the reference indices by list, then the mvd by list */
static void subMbB(VideoSliceJob *job)
{
	VideoAvcCabac *cabac;
	VideoAvcCabacMb *mb = job->mb, *row;
	VideoAvcMbInfo *info = job->mbInfo;
	uint32_t subMbType[4], refIdxBits = 0, leftBits, topBits, bins, sub, list, i;
	uint16_t *left, *top;
	int8_t *refIdx;
	int32_t value, used = 0;

	cabac = job->cabac;
	for (i = 0; i < 4; i++) {
		if (!decodeDecision(job->cabac, 36)) {
			sub = 0;
		} else if (!(bins = decodeDecision(job->cabac, 37))) {
			sub = decodeDecision(job->cabac, 39) ? 2 : 1;
		} else {
			bins = decodeDecision(job->cabac, 38);
			bins = bins << 1 | decodeDecision(job->cabac, 39);
			bins = (bins << 1 | decodeDecision(job->cabac, 39)) << 1;
			if (bSubMbTypes[bins][1] == 5)
				bins |= decodeDecision(job->cabac, 39);
			sub = bSubMbTypes[bins][0];
		}
		subMbType[i] = sub;
		info->subMbType[i] = sub;
	}
	mb->chromaPredMode = 0;
	refIdx = job->refIdx;
	leftBits = mb[-1].refIdxNonZero;
	topBits = mb->refIdxNonZero;
	if (job->numRefIdxActive[0] >= 2) {
		if (bSubMbLists[0][subMbType[0]]) {
			value = decodeRefIdx(cabac, ((topBits >> 2) & 1) * 2 + ((leftBits >> 1) & 1));
			used = 1;
			refIdx[0] = value;
			if (value)
				refIdxBits = 1;
		}
		if (bSubMbLists[0][subMbType[1]]) {
			value = decodeRefIdx(cabac, ((topBits >> 3) & 1) * 2 + (refIdxBits & 1));
			used++;
			refIdx[1] = value;
			if (value)
				refIdxBits |= 2;
		}
		if (bSubMbLists[0][subMbType[2]]) {
			value = decodeRefIdx(cabac, (refIdxBits & 1) * 2 + ((leftBits >> 3) & 1));
			used++;
			refIdx[2] = value;
			if (value)
				refIdxBits |= 4;
		}
		if (bSubMbLists[0][subMbType[3]]) {
			value = decodeRefIdx(cabac, ((refIdxBits >> 1) & 1) * 2 +
				((refIdxBits >> 2) & 1));
			used++;
			refIdx[3] = value;
			if (value)
				refIdxBits |= 8;
		}
	}
	if (job->numRefIdxActive[1] >= 2) {
		if (bSubMbLists[1][subMbType[0]]) {
			value = decodeRefIdx(cabac, ((topBits >> 6) & 1) * 2 + ((leftBits >> 5) & 1));
			used++;
			refIdx[4] = value;
			if (value)
				refIdxBits |= 0x10;
		}
		if (bSubMbLists[1][subMbType[1]]) {
			value = decodeRefIdx(cabac, ((topBits >> 7) & 1) * 2 +
				((refIdxBits >> 4) & 1));
			used++;
			refIdx[5] = value;
			if (value)
				refIdxBits |= 0x20;
		}
		if (bSubMbLists[1][subMbType[2]]) {
			value = decodeRefIdx(cabac, ((refIdxBits >> 4) & 1) * 2 +
				((leftBits >> 7) & 1));
			used++;
			refIdx[6] = value;
			if (value)
				refIdxBits |= 0x40;
		}
		if (bSubMbLists[1][subMbType[3]]) {
			value = decodeRefIdx(cabac, ((refIdxBits >> 5) & 1) * 2 +
				((refIdxBits >> 6) & 1));
			used++;
			refIdx[7] = value;
			if (value)
				refIdxBits |= 0x80;
		}
	}
	if (used >= 1)
		job->refIdx = refIdx + 8;
	mb->refIdxNonZero = refIdxBits;
	row = job->rowBuffer;
	for (list = 0; list < 2; list++) {
		for (i = 0; i < 4; i++) {
			left = &row->absMvd[list][i & 2];
			top = &mb->absMvd[list][(i & 1) * 2];
			sub = subMbType[i];
			if (!bSubMbLists[list][sub]) {
				top[1] = 0;
				top[0] = 0;
				left[0] = 0;
				left[1] = 0;
			} else {
				job->mvd = subMbMvds(cabac, left, top, job->mvd, sub < 4 ? SUB_8X8 :
					sub < 10 && !(sub & 1) ? SUB_8X4 : sub < 10 ? SUB_4X8 : SUB_4X4);
			}
		}
	}
}

/* the run buffers at a row of the run */
static void rowPointers(VideoSliceJob *job, const VideoHelperJob *run, uint32_t row)
{
	const VideoNal *nal = job->parser.nal;

	job->mbInfo = (VideoAvcMbInfo *)(run->buffers[0] + row * nal->mbRowSize);
	job->intraModes = (int8_t *)(run->buffers[1] + row * nal->intraRowSize);
	job->chromaModes = (uint8_t *)(run->buffers[2] + row * nal->chromaModeRowSize);
	job->refIdx = (int8_t *)(run->buffers[3] + row * nal->refIdxRowSize);
	job->mvd = (uint32_t *)(run->buffers[4] + row * nal->mvdRowSize);
	job->lumaCoef = (int16_t *)(run->buffers[5] + row * nal->coefRowSize);
	job->cbCoef = (int16_t *)(run->buffers[6] + row * nal->chromaCoefRowSize);
	job->crCoef = (int16_t *)(run->buffers[7] + row * nal->chromaCoefRowSize);
	job->pcmLuma = (uint8_t *)(run->buffers[8] + row * nal->pcmRowSize);
	job->pcmCb = (uint8_t *)(run->buffers[9] + row * nal->pcmChromaRowSize);
	job->pcmCr = (uint8_t *)(run->buffers[10] + row * nal->pcmChromaRowSize);
}

/*
 * The run's macroblocks of a slice, of its type, up to its end: the
 * doneStatus 1 when the slice ended within the run, 3 at its last
 * macroblock, 0 the run is done and the slice goes on; 129 an error. The
 * macroblocks the run did not decode are returned.
 */
static inline __attribute__((always_inline)) int32_t sliceRun(VideoSliceJob *job,
	const VideoHelperJob *run, uint32_t sliceType)
{
	VideoNal *nal = job->parser.nal;
	int32_t width = nal->widthInMacroblocks, count = nal->runMacroblocks;
	int32_t x = nal->runColumn, done, row, status, error;

	error = videoSetJump(job->jump);
	done = 0;
	row = 0;
	if (error) {
		nal->error = error;
		status = 129;
		goto out;
	}
	for (;;) {
		job->mb = &job->rowBuffer[x + 2];
		rowPointers(job, run, row);
		for (; x < width; x++) {
			done++;
			if (sliceType == AVC_SLICE_I)
				macroblockI(job);
			else if (decodeSkip(job, sliceType == AVC_SLICE_P ? 11 : 24))
				macroblockSkip(job);
			else if (sliceType == AVC_SLICE_P)
				macroblockP(job);
			else
				macroblockB(job);
			if (decodeTerminate(job->cabac)) {
				status = done < count ? 1 : 3;
				goto out;
			}
			if (done >= count) {
				status = 0;
				goto out;
			}
			job->mbInfo++;
			job->mb++;
		}
		x = 0;
		leftAbsMvdClear(job->rowBuffer);
		row++;
		job->rowBuffer[1].qpDeltaNonZero = job->mb[-1].qpDeltaNonZero;
	}
out:
	nal->doneStatus = status;
	return count - done;
}

int32_t avcdecCabacSliceI(VideoSliceJob *job, VideoHelperJob *run)
{
	return sliceRun(job, run, AVC_SLICE_I);
}

int32_t avcdecCabacSliceP(VideoSliceJob *job, VideoHelperJob *run)
{
	return sliceRun(job, run, AVC_SLICE_P);
}

int32_t avcdecCabacSliceB(VideoSliceJob *job, VideoHelperJob *run)
{
	return sliceRun(job, run, AVC_SLICE_B);
}

/*
 * A run of a CABAC slice: the row buffer and the run's buffers opened (the
 * left entry of a run mid-row takes the last column's mb_qp_delta, the
 * macroblock before it), the bitstream unit where the slice left it, and
 * the engine at the slice data when the slice starts here.
 */
void avcdecCabacStart(const VideoHelperShared *shared, VideoSliceJob *job, VideoHelperJob *run)
{
	VideoNal *nal = job->parser.nal;
	uint32_t count = run->commandSize;
	VideoBitstream *bits;

	job->rowBuffer = (VideoAvcCabacMb *)vnzPublicOpenPrivate(shared->tables[2],
		ROW_BUFFER_SIZE);
	if (nal->load)
		neighboursInit(job);
	leftAbsMvdClear(job->rowBuffer);
	job->rowBuffer[1].qpDeltaNonZero =
		job->rowBuffer[nal->widthInMacroblocks + 1].qpDeltaNonZero;
	run->buffers[0] = vnzPublicOpenPrivate(run->buffers[0], count * 16);
	run->buffers[1] = vnzPublicOpenPrivate(run->buffers[1], count * 16);
	run->buffers[2] = vnzPublicOpenPrivate(run->buffers[2], count);
	run->buffers[3] = vnzPublicOpenPrivate(run->buffers[3], count * 8);
	run->buffers[4] = vnzPublicOpenPrivate(run->buffers[4], count * 128);
	run->buffers[5] = vnzPublicOpenPrivate(run->buffers[5], count * 544);
	run->buffers[6] = vnzPublicOpenPrivate(run->buffers[6], count * 136);
	run->buffers[7] = vnzPublicOpenPrivate(run->buffers[7], count * 136);
	run->buffers[8] = vnzPublicOpenPrivate(run->buffers[8], count * 256);
	run->buffers[9] = vnzPublicOpenPrivate(run->buffers[9], count * 64);
	run->buffers[10] = vnzPublicOpenPrivate(run->buffers[10], count * 64);
	videoBitsRestore(&nal->state);
	job->cabac = &nal->cabac;
	nal->cabac.bits = job->parser.bits;
	if (nal->load) {
		bits = job->parser.bits;
		while (!videoBitsAligned(bits))
			videoBitsRead(bits, 1);
		sliceInit(job);
	}
}

/* the run's buffers and the row buffer closed, the unit's position kept */
void avcdecCabacFinish(const VideoHelperShared *shared, VideoSliceJob *job, VideoHelperJob *run)
{
	VideoNal *nal = job->parser.nal;
	uint32_t count = run->commandSize;

	run->buffers[0] = vnzPublicClosePrivate(run->buffers[0], count * 16);
	run->buffers[1] = vnzPublicClosePrivate(run->buffers[1], count * 16);
	run->buffers[2] = vnzPublicClosePrivate(run->buffers[2], count);
	run->buffers[3] = vnzPublicClosePrivate(run->buffers[3], count * 8);
	run->buffers[4] = vnzPublicClosePrivate(run->buffers[4], count * 128);
	run->buffers[5] = vnzPublicClosePrivate(run->buffers[5], count * 544);
	run->buffers[6] = vnzPublicClosePrivate(run->buffers[6], count * 136);
	run->buffers[7] = vnzPublicClosePrivate(run->buffers[7], count * 136);
	run->buffers[8] = vnzPublicClosePrivate(run->buffers[8], count * 256);
	run->buffers[9] = vnzPublicClosePrivate(run->buffers[9], count * 64);
	run->buffers[10] = vnzPublicClosePrivate(run->buffers[10], count * 64);
	vnzPublicClosePrivate((uint32_t)job->rowBuffer, ROW_BUFFER_SIZE);
	videoBitsSave(&nal->state);
}


/* the macroblock's entry and its run buffer record, once its residual is read */
static inline __attribute__((always_inline)) void macroblockDone(VideoSliceJob *job, VideoAvcMbInfo *info, uint32_t cbp,
	int32_t qpDelta, uint32_t cbf)
{
	VideoAvcCabacMb *mb;

	info->cbp = cbp;
	info->cbf = cbf;
	info->qpDelta = qpDelta;
	mb = job->mb;
	mb->qpDeltaNonZero = qpDelta != 0;
	mb->lumaCbf = cbf;
	mb->cbp = (~cbp & 15) | (cbp & 0x30);
	mb->lumaDcCbf = (cbf >> 24) & 1;
	mb->chromaDcCbf[0] = (cbf >> 25) & 1;
	mb->chromaAcCbf[1] = (cbf >> 20) & 15;
	mb->chromaAcCbf[0] = (cbf >> 16) & 15;
	mb->chromaDcCbf[1] = (cbf >> 26) & 1;
}

/* a macroblock of an I slice */
static void macroblockI(VideoSliceJob *job)
{
	VideoAvcCabacMb *mb = job->mb;
	VideoAvcCabac *cabac = job->cabac;
	VideoAvcMbInfo *info = job->mbInfo;
	uint32_t first, bin, bins, type, cbp, luma, chroma, base, cbf;
	int32_t qpDelta;

	first = decodeDecision(cabac, 3 + mb[-1].mbTypeBin + mb->mbTypeBin);
	if (!first) {
		type = I_NXN;
	} else if (decodeTerminate(cabac)) {
		type = I_PCM;
	} else {
		bins = decodeDecision(cabac, 6);
		bin = decodeDecision(cabac, 7);
		bins = bins << 1 | bin;
		if (bin) {
			bins = bins << 1 | decodeDecision(cabac, 8);
			bins = bins << 1 | decodeDecision(cabac, 9);
			bins <<= 1;
			bins |= decodeDecision(cabac, 10);
		} else {
			bins = bins << 1 | decodeDecision(cabac, 9);
			bins = bins << 1 | decodeDecision(cabac, 10);
			bins <<= 1;
		}
		type = intraMbTypes[bins];
	}
	mb->mbTypeBin = first;
	info->skipped = 0;
	info->mbType = type;
	info->subMbType[0] = 0;
	info->subMbType[1] = 0;
	info->subMbType[2] = 0;
	info->subMbType[3] = 0;
	info->reserved = 0;
	info->reserved2 = 0;
	if (type == I_PCM) {
		decodePcm(job);
		info->cbp = 0;
		info->qpDelta = 0;
		info->cbf = 0;
		return;
	}
	decodeIntraModes(job, type);
	if (type >= 1) {
		/* I_16x16: the coded_block_pattern its type gives */
		qpDelta = decodeQpDelta(job);
		if (type >= I_16X16_CBP_LUMA) {
			luma = 15;
			base = I_16X16_CBP_LUMA;
		} else {
			luma = 0;
			base = 1;
		}
		chroma = (type - base) >> 2;
		cbp = chroma << 4 | luma;
	} else {
		cbp = decodeCbp(job);
		luma = cbp & 15;
		chroma = (cbp >> 4) & 3;
		qpDelta = 0;
		if (cbp)
			qpDelta = decodeQpDelta(job);
	}
	if (type >= 1)
		cbf = residual16x16(job, luma);
	else
		cbf = residual4x4(job, luma);
	cbf |= residualChroma(job, chroma);
	macroblockDone(job, info, cbp, qpDelta, cbf);
}

/* the residual of a P or B macroblock (intraType < 0 when inter), and its entry */
static inline __attribute__((always_inline)) void macroblockResidual(VideoSliceJob *job, VideoAvcMbInfo *info, int32_t intraType)
{
	uint32_t cbp, luma, chroma, cbf;
	int32_t qpDelta, base;

	if (intraType >= 1) {
		qpDelta = decodeQpDelta(job);
		if (intraType >= I_16X16_CBP_LUMA) {
			luma = 15;
			base = I_16X16_CBP_LUMA;
		} else {
			luma = 0;
			base = 1;
		}
		chroma = (intraType - base) >> 2;
		cbp = chroma << 4 | luma;
	} else {
		cbp = decodeCbp(job);
		luma = cbp & 15;
		chroma = (cbp >> 4) & 3;
		qpDelta = 0;
		if (cbp)
			qpDelta = decodeQpDelta(job);
	}
	if (intraType >= 1)
		cbf = residual16x16(job, luma);
	else
		cbf = residual4x4(job, luma);
	cbf |= residualChroma(job, chroma);
	macroblockDone(job, info, cbp, qpDelta, cbf);
	job->mb->unavailable = 0;
	if (job->mb[-1].unavailable)
		cbfNoneCoded(&job->mb[-1]);
}


/* the macroblock's record before its type's syntax */
static inline __attribute__((always_inline)) void infoInit(VideoAvcMbInfo *info, uint32_t type)
{
	info->skipped = 0;
	info->mbType = type;
	info->subMbType[0] = 0;
	info->subMbType[1] = 0;
	info->subMbType[2] = 0;
	info->subMbType[3] = 0;
	info->reserved = 0;
	info->reserved2 = 0;
}

/* no motion vector differences: of an intra, PCM or direct macroblock */
static inline __attribute__((always_inline)) void absMvdClear(VideoSliceJob *job)
{
	VideoAvcCabacMb *mb = job->mb;

	leftAbsMvdClear(job->rowBuffer);
	mb->refIdxNonZero = 0;
	leftAbsMvdClear(mb);
}

/*
 * An intra macroblock of a P or B slice: its modes, the neighbours outside
 * the slice taken as coded.
 */
static inline __attribute__((always_inline)) void macroblockIntra(VideoSliceJob *job, int32_t intraType)
{
	decodeIntraModes(job, intraType);
	absMvdClear(job);
	if (job->mb[-1].unavailable)
		cbfAllCoded(&job->mb[-1]);
	if (job->mb->unavailable)
		cbfAllCoded(job->mb);
}

/* a PCM macroblock of a P or B slice */
static inline __attribute__((always_inline)) void macroblockPcm(VideoSliceJob *job, VideoAvcMbInfo *info)
{
	decodePcm(job);
	absMvdClear(job);
	info->cbp = 0;
	info->qpDelta = 0;
	info->cbf = 0;
}

/* a macroblock of a P slice, not skipped */
static void macroblockP(VideoSliceJob *job)
{
	VideoAvcCabac *cabac = job->cabac;
	VideoAvcMbInfo *info = job->mbInfo;
	uint32_t type, bin;
	int32_t intraType;

	if (!decodeDecision(cabac, 14)) {
		bin = decodeDecision(cabac, 15);
		type = pMbTypes[bin << 1 | decodeDecision(cabac, bin == 1 ? 17 : 16)];
	} else {
		type = decodeIntraMbType(cabac, 17, P_TYPES);
	}
	infoInit(info, type);
	intraType = type - P_TYPES;
	if (intraType == I_PCM) {
		macroblockPcm(job, info);
		return;
	}
	if (intraType >= 0)
		macroblockIntra(job, intraType);
	else if (type < P_8X8)
		partitionsP(job, type);
	else
		subMbP(job, type);
	macroblockResidual(job, info, intraType);
}

/* a macroblock of a B slice, not skipped */
static void macroblockB(VideoSliceJob *job)
{
	VideoAvcCabac *cabac = job->cabac;
	VideoAvcCabacMb *mb = job->mb;
	VideoAvcMbInfo *info = job->mbInfo;
	uint32_t first, type, bin, bins;
	int32_t intraType;

	first = decodeDecision(cabac, 27 + mb[-1].mbTypeBin + mb->mbTypeBin);
	if (!first) {
		type = B_DIRECT_16X16;
	} else {
		bin = decodeDecision(cabac, 30);
		bins = decodeDecision(cabac, 27 + 4 + (bin == 0));
		if (!bin) {
			type = 1 + bins;
		} else {
			bins = bins << 1 | decodeDecision(cabac, 32);
			bins = bins << 1 | decodeDecision(cabac, 32);
			bins = (bins << 1 | decodeDecision(cabac, 32)) << 1;
			if (bMbTypes[bins][1] == 6)
				bins |= decodeDecision(cabac, 32);
			type = bMbTypes[bins][0];
			if (type == 0)
				type = decodeIntraMbType(cabac, 32, B_TYPES);
		}
	}
	intraType = type - B_TYPES;
	mb->mbTypeBin = first;
	infoInit(info, type);
	if (intraType == I_PCM) {
		macroblockPcm(job, info);
		return;
	}
	if (intraType >= 0) {
		macroblockIntra(job, intraType);
	} else if (type == B_8X8) {
		subMbB(job);
	} else if (type == B_DIRECT_16X16) {
		absMvdClear(job);
		job->mb->chromaPredMode = 0;
	} else {
		partitionsB(job, type);
	}
	macroblockResidual(job, info, intraType);
}

/* a skipped macroblock of a P or B slice */
static void macroblockSkip(VideoSliceJob *job)
{
	VideoAvcMbInfo *info = job->mbInfo;
	VideoAvcCabacMb *mb;

	info->cbf = 0;
	info->qpDelta = 0;
	info->cbp = 0;
	info->reserved2 = 0;
	info->reserved = 0;
	info->subMbType[3] = 0;
	info->subMbType[2] = 0;
	info->subMbType[1] = 0;
	info->subMbType[0] = 0;
	info->mbType = 0;
	info->skipped = 1;
	leftAbsMvdClear(job->rowBuffer);
	mb = job->mb;
	mb->unavailable = 0;
	mb->chromaAcCbf[1] = 0;
	mb->chromaDcCbf[1] = 0;
	mb->chromaAcCbf[0] = 0;
	mb->chromaDcCbf[0] = 0;
	mb->lumaCbf = 0;
	mb->lumaDcCbf = 0;
	mb->cbp = 15;
	mb->qpDeltaNonZero = 0;
	mb->refIdxNonZero = 0;
	mb->chromaPredMode = 0;
	mb->mbTypeBin = 0;
	leftAbsMvdClear(mb);
}

/*
 * ctxIdxInc of a 4x4 block's coded_block_flag: its left (A) and top (B)
 * blocks' flags, from the neighbours' or the macroblock's so far.
 */
static inline uint32_t lumaCbfIncrement(const VideoAvcCabacMb *mb, uint32_t cbf, uint32_t block)
{
	uint32_t a = (block & 5) ? cbf : mb[-1].lumaCbf;
	uint32_t b = (block & 10) ? cbf : mb->lumaCbf;

	a = (a >> (block ^ (((block & 1) << 2) ^ 5))) & 1;
	b = (b >> (block ^ (((block & 2) << 2) ^ 10))) & 1;
	return b * 2 + a;
}

/* an I_16x16 macroblock's luma: the DC block, then the AC blocks coded */
static uint32_t residual16x16(VideoSliceJob *job, uint32_t lumaCbp)
{
	VideoAvcCabacMb *mb = job->mb;
	VideoAvcCabac *cabac = job->cabac;
	int16_t *coef = job->lumaCoef;
	uint32_t dc, cbf, bits = 0, block;

	dc = decodeDecision(cabac, 85 + mb->lumaDcCbf * 2 + mb[-1].lumaDcCbf);
	if (dc) {
		residualLumaDc(cabac, coef);
		job->lumaCoef += 16;
	} else if (lumaCbp) {
		job->lumaCoef += 16;
	}
	cbf = dc << 24;
	if (lumaCbp) {
		coef = job->lumaCoef;
		for (block = 0; block < 16; block++) {
			if (decodeDecision(job->cabac, 89 + lumaCbfIncrement(job->mb, bits, block))) {
				bits += 1 << block;
				residualLumaAc(job->cabac, coef);
				coef += 16;
			}
		}
		job->lumaCoef = coef;
		cbf |= bits;
	}
	return cbf;
}

/* the luma 4x4 blocks of the 8x8 blocks coded */
static uint32_t residual4x4(VideoSliceJob *job, uint32_t lumaCbp)
{
	int16_t *coef;
	uint32_t bits = 0, block8, block;

	if (!lumaCbp)
		return 0;
	coef = job->lumaCoef;
	for (block8 = 0; block8 < 4; block8++) {
		if (!((lumaCbp >> block8) & 1))
			continue;
		for (block = block8 * 4; block < block8 * 4 + 4; block++) {
			if (decodeDecision(job->cabac, 93 + lumaCbfIncrement(job->mb, bits, block))) {
				bits += 1 << block;
				residualLuma(job->cabac, coef);
				coef += 16;
			}
		}
	}
	job->lumaCoef = coef;
	return bits;
}

/* ctxIdxInc of a chroma AC block's coded_block_flag, as lumaCbfIncrement's */
static inline uint32_t chromaCbfIncrement(const VideoAvcCabacMb *mb, uint32_t cbf,
	uint32_t component, uint32_t block)
{
	uint32_t a = (block & 5) ? cbf : mb[-1].chromaAcCbf[component];
	uint32_t b = (block & 10) ? cbf : mb->chromaAcCbf[component];

	a = (a >> (block ^ 1)) & 1;
	b = (b >> (block ^ 2)) & 1;
	return b * 2 + a;
}

/*
 * The chroma: the DC blocks of chroma 1 or 2 (a DC slot kept for chroma 2),
 * then the AC blocks of chroma 2 (the DC slot given back when nothing of
 * the component was coded).
 */
static uint32_t residualChroma(VideoSliceJob *job, uint32_t chromaCbp)
{
	VideoAvcCabacMb *mb;
	VideoAvcCabac *cabac;
	int16_t *cb, *cr;
	uint32_t cbf = 0, dcCb, dcCr, bitsCb, bitsCr, block;

	if (chromaCbp & 3) {
		cr = job->crCoef;
		cabac = job->cabac;
		mb = job->mb;
		cb = job->cbCoef;
		dcCb = decodeDecision(cabac, 97 + mb->chromaDcCbf[0] * 2 + mb[-1].chromaDcCbf[0]);
		if (dcCb) {
			residualChromaDc(cabac, cb);
			job->cbCoef += 4;
		} else if (chromaCbp == 2) {
			job->cbCoef += 4;
		}
		dcCr = decodeDecision(cabac, 97 + mb->chromaDcCbf[1] * 2 + mb[-1].chromaDcCbf[1]);
		if (dcCr) {
			residualChromaDc(cabac, cr);
			job->crCoef += 4;
		} else if (chromaCbp == 2) {
			job->crCoef += 4;
		}
		cbf = (dcCr << 26) + (dcCb << 25);
	}
	if (chromaCbp & 2) {
		cb = job->cbCoef;
		cr = job->crCoef;
		bitsCb = 0;
		for (block = 0; block < 4; block++) {
			if (decodeDecision(job->cabac, 101 +
				chromaCbfIncrement(job->mb, bitsCb, 0, block))) {
				bitsCb += 1 << block;
				residualChromaAc(job->cabac, cb);
				cb += 16;
			}
		}
		if (!bitsCb && !((cbf >> 25) & 1))
			cb -= 4;
		job->cbCoef = cb;
		bitsCr = 0;
		for (block = 0; block < 4; block++) {
			if (decodeDecision(job->cabac, 101 +
				chromaCbfIncrement(job->mb, bitsCr, 1, block))) {
				bitsCr += 1 << block;
				residualChromaAc(job->cabac, cr);
				cr += 16;
			}
		}
		if (!bitsCr && !((cbf >> 26) & 1))
			cr -= 4;
		job->crCoef = cr;
		cbf |= bitsCr << 20 | bitsCb << 16;
	}
	return cbf;
}
