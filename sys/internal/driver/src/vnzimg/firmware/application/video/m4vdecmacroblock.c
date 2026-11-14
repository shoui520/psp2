/*
 * The MPEG-4 decoder's macroblock layer (0x84423c-0x847184): the VLCs of
 * the macroblock and block syntax, and the macroblocks of a run, as the
 * parser's jobs read them for the macroblock unit.
 */
#include "m4vdecparser.h"

/* an entry of a VLC table: what the code stands for and its length */
typedef struct M4vdecVlc {
	int32_t value;
	int32_t length;
} M4vdecVlc;

#define VLC_INVALID		(-1)

/* mcbpc: the macroblock's type and the chroma blocks coded */
#define MCBPC(type, cbpc)	((type) | (cbpc) << 4)
#define MCBPC_STUFFING		255
#define MCBPC_TYPE_STUFFING	7

/* a TCOEF code: last, the run of zeros before the coefficient and its level */
#define INTER(last, run, level)	((last) << 12 | (run) << 4 | (level))
#define INTRA(last, run, level)	((last) << 16 | (run) << 8 | (level))
#define TCOEF_ESCAPE		7167

/* the TCOEF codes of 12 bits, in three tables by their leading zeros */
typedef struct M4vdecTcoefVlc {
	M4vdecVlc from512[112];
	M4vdecVlc from128[96];
	M4vdecVlc from8[120];
} M4vdecTcoefVlc;

/*
 * The tables as Sony's image lays them out (0x8f82ac-0x8fa50c), one after
 * the other: an escape's second code can be the escape again, and its run
 * and level then index past LMAX's and RMAX's tables into the next.
 */
static const struct {
	M4vdecVlc mcbpcIntra[32];
	M4vdecVlc mcbpcInter[256];
	M4vdecTcoefVlc tcoef[2];		/* inter, intra */
	int32_t lmaxIntra[2][64];
	int32_t lmaxInter[2][64];
	int32_t rmaxIntra[28];
	int32_t rmaxIntraLast[9];
	int32_t rmaxInter[13];
	int32_t rmaxInterLast[4];
	M4vdecCoefficient none;			/* 0x8fa504 */
	M4vdecVlc rvlcIntra[170];		/* the reversible VLC's, in INTRA()'s form */
	M4vdecVlc rvlcInter[170];
	M4vdecCoefficient rvlcNone;		/* 0x8fafac */
} g_m4vdecVlcTables = {
	/* 0x8f82ac: I-VOP mcbpc, by the top 6 of 9 bits */
	.mcbpcIntra = {
		{ VLC_INVALID, 0 }, { MCBPC(4, 1), 6 }, { MCBPC(4, 2), 6 }, { MCBPC(4, 3), 6 },
		{ MCBPC(4, 0), 4 }, { MCBPC(4, 0), 4 }, { MCBPC(4, 0), 4 }, { MCBPC(4, 0), 4 },
		{ MCBPC(3, 1), 3 }, { MCBPC(3, 1), 3 }, { MCBPC(3, 1), 3 }, { MCBPC(3, 1), 3 },
		{ MCBPC(3, 1), 3 }, { MCBPC(3, 1), 3 }, { MCBPC(3, 1), 3 }, { MCBPC(3, 1), 3 },
		{ MCBPC(3, 2), 3 }, { MCBPC(3, 2), 3 }, { MCBPC(3, 2), 3 }, { MCBPC(3, 2), 3 },
		{ MCBPC(3, 2), 3 }, { MCBPC(3, 2), 3 }, { MCBPC(3, 2), 3 }, { MCBPC(3, 2), 3 },
		{ MCBPC(3, 3), 3 }, { MCBPC(3, 3), 3 }, { MCBPC(3, 3), 3 }, { MCBPC(3, 3), 3 },
		{ MCBPC(3, 3), 3 }, { MCBPC(3, 3), 3 }, { MCBPC(3, 3), 3 }, { MCBPC(3, 3), 3 },
	},
	/* 0x8f83ac: P-VOP mcbpc, by 9 bits */
	.mcbpcInter = {
		{ VLC_INVALID, 0 }, { MCBPC_STUFFING, 9 }, { MCBPC(4, 3), 9 }, { MCBPC(4, 2), 9 },
		{ MCBPC(4, 1), 9 }, { MCBPC(1, 3), 9 }, { MCBPC(3, 2), 8 }, { MCBPC(3, 2), 8 },
		{ MCBPC(3, 1), 8 }, { MCBPC(3, 1), 8 }, { MCBPC(2, 3), 8 }, { MCBPC(2, 3), 8 },
		{ MCBPC(3, 3), 7 }, { MCBPC(3, 3), 7 }, { MCBPC(3, 3), 7 }, { MCBPC(3, 3), 7 },
		{ MCBPC(2, 2), 7 }, { MCBPC(2, 2), 7 }, { MCBPC(2, 2), 7 }, { MCBPC(2, 2), 7 },
		{ MCBPC(2, 1), 7 }, { MCBPC(2, 1), 7 }, { MCBPC(2, 1), 7 }, { MCBPC(2, 1), 7 },
		{ MCBPC(1, 2), 7 }, { MCBPC(1, 2), 7 }, { MCBPC(1, 2), 7 }, { MCBPC(1, 2), 7 },
		{ MCBPC(1, 1), 7 }, { MCBPC(1, 1), 7 }, { MCBPC(1, 1), 7 }, { MCBPC(1, 1), 7 },
		{ MCBPC(4, 0), 6 }, { MCBPC(4, 0), 6 }, { MCBPC(4, 0), 6 }, { MCBPC(4, 0), 6 },
		{ MCBPC(4, 0), 6 }, { MCBPC(4, 0), 6 }, { MCBPC(4, 0), 6 }, { MCBPC(4, 0), 6 },
		{ MCBPC(0, 3), 6 }, { MCBPC(0, 3), 6 }, { MCBPC(0, 3), 6 }, { MCBPC(0, 3), 6 },
		{ MCBPC(0, 3), 6 }, { MCBPC(0, 3), 6 }, { MCBPC(0, 3), 6 }, { MCBPC(0, 3), 6 },
		{ MCBPC(3, 0), 5 }, { MCBPC(3, 0), 5 }, { MCBPC(3, 0), 5 }, { MCBPC(3, 0), 5 },
		{ MCBPC(3, 0), 5 }, { MCBPC(3, 0), 5 }, { MCBPC(3, 0), 5 }, { MCBPC(3, 0), 5 },
		{ MCBPC(3, 0), 5 }, { MCBPC(3, 0), 5 }, { MCBPC(3, 0), 5 }, { MCBPC(3, 0), 5 },
		{ MCBPC(3, 0), 5 }, { MCBPC(3, 0), 5 }, { MCBPC(3, 0), 5 }, { MCBPC(3, 0), 5 },
		{ MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 },
		{ MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 },
		{ MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 },
		{ MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 },
		{ MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 },
		{ MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 },
		{ MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 },
		{ MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 }, { MCBPC(0, 2), 4 },
		{ MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 },
		{ MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 },
		{ MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 },
		{ MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 },
		{ MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 },
		{ MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 },
		{ MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 },
		{ MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 }, { MCBPC(0, 1), 4 },
		{ MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 },
		{ MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 },
		{ MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 },
		{ MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 },
		{ MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 },
		{ MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 },
		{ MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 },
		{ MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 },
		{ MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 },
		{ MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 },
		{ MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 },
		{ MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 },
		{ MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 },
		{ MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 },
		{ MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 },
		{ MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 }, { MCBPC(2, 0), 3 },
		{ MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 },
		{ MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 },
		{ MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 },
		{ MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 },
		{ MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 },
		{ MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 },
		{ MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 },
		{ MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 },
		{ MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 },
		{ MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 },
		{ MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 },
		{ MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 },
		{ MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 },
		{ MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 },
		{ MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 },
		{ MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 }, { MCBPC(1, 0), 3 },
	},
	.tcoef = {
		{	/* inter */
			/* 0x8f8bac: codes from 0x200, by their top 7 of 12 bits */
			.from512 = {
				{ INTER(1, 8, 1), 7 }, { INTER(1, 7, 1), 7 }, { INTER(1, 6, 1), 7 },
				{ INTER(1, 5, 1), 7 }, { INTER(0, 12, 1), 7 }, { INTER(0, 11, 1), 7 },
				{ INTER(0, 10, 1), 7 }, { INTER(0, 0, 4), 7 }, { INTER(1, 4, 1), 6 },
				{ INTER(1, 4, 1), 6 }, { INTER(1, 3, 1), 6 }, { INTER(1, 3, 1), 6 },
				{ INTER(1, 2, 1), 6 }, { INTER(1, 2, 1), 6 }, { INTER(1, 1, 1), 6 },
				{ INTER(1, 1, 1), 6 }, { INTER(0, 9, 1), 6 }, { INTER(0, 9, 1), 6 },
				{ INTER(0, 8, 1), 6 }, { INTER(0, 8, 1), 6 }, { INTER(0, 7, 1), 6 },
				{ INTER(0, 7, 1), 6 }, { INTER(0, 6, 1), 6 }, { INTER(0, 6, 1), 6 },
				{ INTER(0, 1, 2), 6 }, { INTER(0, 1, 2), 6 }, { INTER(0, 0, 3), 6 },
				{ INTER(0, 0, 3), 6 }, { INTER(0, 5, 1), 5 }, { INTER(0, 5, 1), 5 },
				{ INTER(0, 5, 1), 5 }, { INTER(0, 5, 1), 5 }, { INTER(0, 4, 1), 5 },
				{ INTER(0, 4, 1), 5 }, { INTER(0, 4, 1), 5 }, { INTER(0, 4, 1), 5 },
				{ INTER(0, 3, 1), 5 }, { INTER(0, 3, 1), 5 }, { INTER(0, 3, 1), 5 },
				{ INTER(0, 3, 1), 5 }, { INTER(1, 0, 1), 4 }, { INTER(1, 0, 1), 4 },
				{ INTER(1, 0, 1), 4 }, { INTER(1, 0, 1), 4 }, { INTER(1, 0, 1), 4 },
				{ INTER(1, 0, 1), 4 }, { INTER(1, 0, 1), 4 }, { INTER(1, 0, 1), 4 },
				{ INTER(0, 0, 1), 2 }, { INTER(0, 0, 1), 2 }, { INTER(0, 0, 1), 2 },
				{ INTER(0, 0, 1), 2 }, { INTER(0, 0, 1), 2 }, { INTER(0, 0, 1), 2 },
				{ INTER(0, 0, 1), 2 }, { INTER(0, 0, 1), 2 }, { INTER(0, 0, 1), 2 },
				{ INTER(0, 0, 1), 2 }, { INTER(0, 0, 1), 2 }, { INTER(0, 0, 1), 2 },
				{ INTER(0, 0, 1), 2 }, { INTER(0, 0, 1), 2 }, { INTER(0, 0, 1), 2 },
				{ INTER(0, 0, 1), 2 }, { INTER(0, 0, 1), 2 }, { INTER(0, 0, 1), 2 },
				{ INTER(0, 0, 1), 2 }, { INTER(0, 0, 1), 2 }, { INTER(0, 0, 1), 2 },
				{ INTER(0, 0, 1), 2 }, { INTER(0, 0, 1), 2 }, { INTER(0, 0, 1), 2 },
				{ INTER(0, 0, 1), 2 }, { INTER(0, 0, 1), 2 }, { INTER(0, 0, 1), 2 },
				{ INTER(0, 0, 1), 2 }, { INTER(0, 0, 1), 2 }, { INTER(0, 0, 1), 2 },
				{ INTER(0, 0, 1), 2 }, { INTER(0, 0, 1), 2 }, { INTER(0, 1, 1), 3 },
				{ INTER(0, 1, 1), 3 }, { INTER(0, 1, 1), 3 }, { INTER(0, 1, 1), 3 },
				{ INTER(0, 1, 1), 3 }, { INTER(0, 1, 1), 3 }, { INTER(0, 1, 1), 3 },
				{ INTER(0, 1, 1), 3 }, { INTER(0, 1, 1), 3 }, { INTER(0, 1, 1), 3 },
				{ INTER(0, 1, 1), 3 }, { INTER(0, 1, 1), 3 }, { INTER(0, 1, 1), 3 },
				{ INTER(0, 1, 1), 3 }, { INTER(0, 1, 1), 3 }, { INTER(0, 1, 1), 3 },
				{ INTER(0, 2, 1), 4 }, { INTER(0, 2, 1), 4 }, { INTER(0, 2, 1), 4 },
				{ INTER(0, 2, 1), 4 }, { INTER(0, 2, 1), 4 }, { INTER(0, 2, 1), 4 },
				{ INTER(0, 2, 1), 4 }, { INTER(0, 2, 1), 4 }, { INTER(0, 0, 2), 4 },
				{ INTER(0, 0, 2), 4 }, { INTER(0, 0, 2), 4 }, { INTER(0, 0, 2), 4 },
				{ INTER(0, 0, 2), 4 }, { INTER(0, 0, 2), 4 }, { INTER(0, 0, 2), 4 },
				{ INTER(0, 0, 2), 4 },
			},
			/* 0x8f8f2c: from 0x80, by the top 10 */
			.from128 = {
				{ INTER(0, 0, 9), 10 }, { INTER(0, 0, 8), 10 }, { INTER(1, 24, 1), 9 },
				{ INTER(1, 24, 1), 9 }, { INTER(1, 23, 1), 9 }, { INTER(1, 23, 1), 9 },
				{ INTER(1, 22, 1), 9 }, { INTER(1, 22, 1), 9 }, { INTER(1, 21, 1), 9 },
				{ INTER(1, 21, 1), 9 }, { INTER(1, 20, 1), 9 }, { INTER(1, 20, 1), 9 },
				{ INTER(1, 19, 1), 9 }, { INTER(1, 19, 1), 9 }, { INTER(1, 18, 1), 9 },
				{ INTER(1, 18, 1), 9 }, { INTER(1, 17, 1), 9 }, { INTER(1, 17, 1), 9 },
				{ INTER(1, 0, 2), 9 }, { INTER(1, 0, 2), 9 }, { INTER(0, 22, 1), 9 },
				{ INTER(0, 22, 1), 9 }, { INTER(0, 21, 1), 9 }, { INTER(0, 21, 1), 9 },
				{ INTER(0, 20, 1), 9 }, { INTER(0, 20, 1), 9 }, { INTER(0, 19, 1), 9 },
				{ INTER(0, 19, 1), 9 }, { INTER(0, 18, 1), 9 }, { INTER(0, 18, 1), 9 },
				{ INTER(0, 17, 1), 9 }, { INTER(0, 17, 1), 9 }, { INTER(0, 16, 1), 9 },
				{ INTER(0, 16, 1), 9 }, { INTER(0, 15, 1), 9 }, { INTER(0, 15, 1), 9 },
				{ INTER(0, 4, 2), 9 }, { INTER(0, 4, 2), 9 }, { INTER(0, 3, 2), 9 },
				{ INTER(0, 3, 2), 9 }, { INTER(0, 0, 7), 9 }, { INTER(0, 0, 7), 9 },
				{ INTER(0, 0, 6), 9 }, { INTER(0, 0, 6), 9 }, { INTER(1, 16, 1), 8 },
				{ INTER(1, 16, 1), 8 }, { INTER(1, 16, 1), 8 }, { INTER(1, 16, 1), 8 },
				{ INTER(1, 15, 1), 8 }, { INTER(1, 15, 1), 8 }, { INTER(1, 15, 1), 8 },
				{ INTER(1, 15, 1), 8 }, { INTER(1, 14, 1), 8 }, { INTER(1, 14, 1), 8 },
				{ INTER(1, 14, 1), 8 }, { INTER(1, 14, 1), 8 }, { INTER(1, 13, 1), 8 },
				{ INTER(1, 13, 1), 8 }, { INTER(1, 13, 1), 8 }, { INTER(1, 13, 1), 8 },
				{ INTER(1, 12, 1), 8 }, { INTER(1, 12, 1), 8 }, { INTER(1, 12, 1), 8 },
				{ INTER(1, 12, 1), 8 }, { INTER(1, 11, 1), 8 }, { INTER(1, 11, 1), 8 },
				{ INTER(1, 11, 1), 8 }, { INTER(1, 11, 1), 8 }, { INTER(1, 10, 1), 8 },
				{ INTER(1, 10, 1), 8 }, { INTER(1, 10, 1), 8 }, { INTER(1, 10, 1), 8 },
				{ INTER(1, 9, 1), 8 }, { INTER(1, 9, 1), 8 }, { INTER(1, 9, 1), 8 },
				{ INTER(1, 9, 1), 8 }, { INTER(0, 14, 1), 8 }, { INTER(0, 14, 1), 8 },
				{ INTER(0, 14, 1), 8 }, { INTER(0, 14, 1), 8 }, { INTER(0, 13, 1), 8 },
				{ INTER(0, 13, 1), 8 }, { INTER(0, 13, 1), 8 }, { INTER(0, 13, 1), 8 },
				{ INTER(0, 2, 2), 8 }, { INTER(0, 2, 2), 8 }, { INTER(0, 2, 2), 8 },
				{ INTER(0, 2, 2), 8 }, { INTER(0, 1, 3), 8 }, { INTER(0, 1, 3), 8 },
				{ INTER(0, 1, 3), 8 }, { INTER(0, 1, 3), 8 }, { INTER(0, 0, 5), 8 },
				{ INTER(0, 0, 5), 8 }, { INTER(0, 0, 5), 8 }, { INTER(0, 0, 5), 8 },
			},
			/* 0x8f922c: from 8, by all 12 */
			.from8 = {
				{ INTER(1, 1, 2), 11 }, { INTER(1, 1, 2), 11 }, { INTER(1, 0, 3), 11 },
				{ INTER(1, 0, 3), 11 }, { INTER(0, 0, 11), 11 }, { INTER(0, 0, 11), 11 },
				{ INTER(0, 0, 10), 11 }, { INTER(0, 0, 10), 11 }, { INTER(1, 28, 1), 10 },
				{ INTER(1, 28, 1), 10 }, { INTER(1, 28, 1), 10 }, { INTER(1, 28, 1), 10 },
				{ INTER(1, 27, 1), 10 }, { INTER(1, 27, 1), 10 }, { INTER(1, 27, 1), 10 },
				{ INTER(1, 27, 1), 10 }, { INTER(1, 26, 1), 10 }, { INTER(1, 26, 1), 10 },
				{ INTER(1, 26, 1), 10 }, { INTER(1, 26, 1), 10 }, { INTER(1, 25, 1), 10 },
				{ INTER(1, 25, 1), 10 }, { INTER(1, 25, 1), 10 }, { INTER(1, 25, 1), 10 },
				{ INTER(0, 9, 2), 10 }, { INTER(0, 9, 2), 10 }, { INTER(0, 9, 2), 10 },
				{ INTER(0, 9, 2), 10 }, { INTER(0, 8, 2), 10 }, { INTER(0, 8, 2), 10 },
				{ INTER(0, 8, 2), 10 }, { INTER(0, 8, 2), 10 }, { INTER(0, 7, 2), 10 },
				{ INTER(0, 7, 2), 10 }, { INTER(0, 7, 2), 10 }, { INTER(0, 7, 2), 10 },
				{ INTER(0, 6, 2), 10 }, { INTER(0, 6, 2), 10 }, { INTER(0, 6, 2), 10 },
				{ INTER(0, 6, 2), 10 }, { INTER(0, 5, 2), 10 }, { INTER(0, 5, 2), 10 },
				{ INTER(0, 5, 2), 10 }, { INTER(0, 5, 2), 10 }, { INTER(0, 3, 3), 10 },
				{ INTER(0, 3, 3), 10 }, { INTER(0, 3, 3), 10 }, { INTER(0, 3, 3), 10 },
				{ INTER(0, 2, 3), 10 }, { INTER(0, 2, 3), 10 }, { INTER(0, 2, 3), 10 },
				{ INTER(0, 2, 3), 10 }, { INTER(0, 1, 4), 10 }, { INTER(0, 1, 4), 10 },
				{ INTER(0, 1, 4), 10 }, { INTER(0, 1, 4), 10 }, { INTER(0, 0, 12), 11 },
				{ INTER(0, 0, 12), 11 }, { INTER(0, 1, 5), 11 }, { INTER(0, 1, 5), 11 },
				{ INTER(0, 23, 1), 11 }, { INTER(0, 23, 1), 11 }, { INTER(0, 24, 1), 11 },
				{ INTER(0, 24, 1), 11 }, { INTER(1, 29, 1), 11 }, { INTER(1, 29, 1), 11 },
				{ INTER(1, 30, 1), 11 }, { INTER(1, 30, 1), 11 }, { INTER(1, 31, 1), 11 },
				{ INTER(1, 31, 1), 11 }, { INTER(1, 32, 1), 11 }, { INTER(1, 32, 1), 11 },
				{ INTER(0, 1, 6), 12 }, { INTER(0, 2, 4), 12 }, { INTER(0, 4, 3), 12 },
				{ INTER(0, 5, 3), 12 }, { INTER(0, 6, 3), 12 }, { INTER(0, 10, 2), 12 },
				{ INTER(0, 25, 1), 12 }, { INTER(0, 26, 1), 12 }, { INTER(1, 33, 1), 12 },
				{ INTER(1, 34, 1), 12 }, { INTER(1, 35, 1), 12 }, { INTER(1, 36, 1), 12 },
				{ INTER(1, 37, 1), 12 }, { INTER(1, 38, 1), 12 }, { INTER(1, 39, 1), 12 },
				{ INTER(1, 40, 1), 12 }, { TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 },
				{ TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 },
				{ TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 },
				{ TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 },
				{ TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 },
				{ TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 },
				{ TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 },
				{ TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 },
				{ TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 },
				{ TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 },
				{ TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 },
			},
		},
		{	/* intra */
			/* 0x8f95ec: codes from 0x200, by their top 7 of 12 bits */
			.from512 = {
				{ INTRA(1, 4, 1), 7 }, { INTRA(1, 3, 1), 7 }, { INTRA(0, 6, 1), 7 },
				{ INTRA(1, 5, 1), 7 }, { INTRA(0, 7, 1), 7 }, { INTRA(0, 2, 2), 7 },
				{ INTRA(0, 1, 3), 7 }, { INTRA(0, 0, 9), 7 }, { INTRA(1, 0, 2), 6 },
				{ INTRA(1, 0, 2), 6 }, { INTRA(0, 5, 1), 6 }, { INTRA(0, 5, 1), 6 },
				{ INTRA(1, 2, 1), 6 }, { INTRA(1, 2, 1), 6 }, { INTRA(1, 1, 1), 6 },
				{ INTRA(1, 1, 1), 6 }, { INTRA(0, 4, 1), 6 }, { INTRA(0, 4, 1), 6 },
				{ INTRA(0, 3, 1), 6 }, { INTRA(0, 3, 1), 6 }, { INTRA(0, 0, 8), 6 },
				{ INTRA(0, 0, 8), 6 }, { INTRA(0, 0, 7), 6 }, { INTRA(0, 0, 7), 6 },
				{ INTRA(0, 1, 2), 6 }, { INTRA(0, 1, 2), 6 }, { INTRA(0, 0, 6), 6 },
				{ INTRA(0, 0, 6), 6 }, { INTRA(0, 2, 1), 5 }, { INTRA(0, 2, 1), 5 },
				{ INTRA(0, 2, 1), 5 }, { INTRA(0, 2, 1), 5 }, { INTRA(0, 0, 5), 5 },
				{ INTRA(0, 0, 5), 5 }, { INTRA(0, 0, 5), 5 }, { INTRA(0, 0, 5), 5 },
				{ INTRA(0, 0, 4), 5 }, { INTRA(0, 0, 4), 5 }, { INTRA(0, 0, 4), 5 },
				{ INTRA(0, 0, 4), 5 }, { INTRA(1, 0, 1), 4 }, { INTRA(1, 0, 1), 4 },
				{ INTRA(1, 0, 1), 4 }, { INTRA(1, 0, 1), 4 }, { INTRA(1, 0, 1), 4 },
				{ INTRA(1, 0, 1), 4 }, { INTRA(1, 0, 1), 4 }, { INTRA(1, 0, 1), 4 },
				{ INTRA(0, 0, 1), 2 }, { INTRA(0, 0, 1), 2 }, { INTRA(0, 0, 1), 2 },
				{ INTRA(0, 0, 1), 2 }, { INTRA(0, 0, 1), 2 }, { INTRA(0, 0, 1), 2 },
				{ INTRA(0, 0, 1), 2 }, { INTRA(0, 0, 1), 2 }, { INTRA(0, 0, 1), 2 },
				{ INTRA(0, 0, 1), 2 }, { INTRA(0, 0, 1), 2 }, { INTRA(0, 0, 1), 2 },
				{ INTRA(0, 0, 1), 2 }, { INTRA(0, 0, 1), 2 }, { INTRA(0, 0, 1), 2 },
				{ INTRA(0, 0, 1), 2 }, { INTRA(0, 0, 1), 2 }, { INTRA(0, 0, 1), 2 },
				{ INTRA(0, 0, 1), 2 }, { INTRA(0, 0, 1), 2 }, { INTRA(0, 0, 1), 2 },
				{ INTRA(0, 0, 1), 2 }, { INTRA(0, 0, 1), 2 }, { INTRA(0, 0, 1), 2 },
				{ INTRA(0, 0, 1), 2 }, { INTRA(0, 0, 1), 2 }, { INTRA(0, 0, 1), 2 },
				{ INTRA(0, 0, 1), 2 }, { INTRA(0, 0, 1), 2 }, { INTRA(0, 0, 1), 2 },
				{ INTRA(0, 0, 1), 2 }, { INTRA(0, 0, 1), 2 }, { INTRA(0, 0, 2), 3 },
				{ INTRA(0, 0, 2), 3 }, { INTRA(0, 0, 2), 3 }, { INTRA(0, 0, 2), 3 },
				{ INTRA(0, 0, 2), 3 }, { INTRA(0, 0, 2), 3 }, { INTRA(0, 0, 2), 3 },
				{ INTRA(0, 0, 2), 3 }, { INTRA(0, 0, 2), 3 }, { INTRA(0, 0, 2), 3 },
				{ INTRA(0, 0, 2), 3 }, { INTRA(0, 0, 2), 3 }, { INTRA(0, 0, 2), 3 },
				{ INTRA(0, 0, 2), 3 }, { INTRA(0, 0, 2), 3 }, { INTRA(0, 0, 2), 3 },
				{ INTRA(0, 1, 1), 4 }, { INTRA(0, 1, 1), 4 }, { INTRA(0, 1, 1), 4 },
				{ INTRA(0, 1, 1), 4 }, { INTRA(0, 1, 1), 4 }, { INTRA(0, 1, 1), 4 },
				{ INTRA(0, 1, 1), 4 }, { INTRA(0, 1, 1), 4 }, { INTRA(0, 0, 3), 4 },
				{ INTRA(0, 0, 3), 4 }, { INTRA(0, 0, 3), 4 }, { INTRA(0, 0, 3), 4 },
				{ INTRA(0, 0, 3), 4 }, { INTRA(0, 0, 3), 4 }, { INTRA(0, 0, 3), 4 },
				{ INTRA(0, 0, 3), 4 },
			},
			/* 0x8f996c: from 0x80, by the top 10 */
			.from128 = {
				{ INTRA(0, 0, 18), 10 }, { INTRA(0, 0, 17), 10 }, { INTRA(1, 14, 1), 9 },
				{ INTRA(1, 14, 1), 9 }, { INTRA(1, 13, 1), 9 }, { INTRA(1, 13, 1), 9 },
				{ INTRA(1, 12, 1), 9 }, { INTRA(1, 12, 1), 9 }, { INTRA(1, 11, 1), 9 },
				{ INTRA(1, 11, 1), 9 }, { INTRA(1, 10, 1), 9 }, { INTRA(1, 10, 1), 9 },
				{ INTRA(1, 1, 2), 9 }, { INTRA(1, 1, 2), 9 }, { INTRA(1, 0, 4), 9 },
				{ INTRA(1, 0, 4), 9 }, { INTRA(0, 12, 1), 9 }, { INTRA(0, 12, 1), 9 },
				{ INTRA(0, 11, 1), 9 }, { INTRA(0, 11, 1), 9 }, { INTRA(0, 7, 2), 9 },
				{ INTRA(0, 7, 2), 9 }, { INTRA(0, 6, 2), 9 }, { INTRA(0, 6, 2), 9 },
				{ INTRA(0, 5, 2), 9 }, { INTRA(0, 5, 2), 9 }, { INTRA(0, 3, 3), 9 },
				{ INTRA(0, 3, 3), 9 }, { INTRA(0, 2, 3), 9 }, { INTRA(0, 2, 3), 9 },
				{ INTRA(0, 1, 6), 9 }, { INTRA(0, 1, 6), 9 }, { INTRA(0, 1, 5), 9 },
				{ INTRA(0, 1, 5), 9 }, { INTRA(0, 0, 16), 9 }, { INTRA(0, 0, 16), 9 },
				{ INTRA(0, 4, 2), 9 }, { INTRA(0, 4, 2), 9 }, { INTRA(0, 0, 15), 9 },
				{ INTRA(0, 0, 15), 9 }, { INTRA(0, 0, 14), 9 }, { INTRA(0, 0, 14), 9 },
				{ INTRA(0, 0, 13), 9 }, { INTRA(0, 0, 13), 9 }, { INTRA(1, 8, 1), 8 },
				{ INTRA(1, 8, 1), 8 }, { INTRA(1, 8, 1), 8 }, { INTRA(1, 8, 1), 8 },
				{ INTRA(1, 7, 1), 8 }, { INTRA(1, 7, 1), 8 }, { INTRA(1, 7, 1), 8 },
				{ INTRA(1, 7, 1), 8 }, { INTRA(1, 6, 1), 8 }, { INTRA(1, 6, 1), 8 },
				{ INTRA(1, 6, 1), 8 }, { INTRA(1, 6, 1), 8 }, { INTRA(1, 0, 3), 8 },
				{ INTRA(1, 0, 3), 8 }, { INTRA(1, 0, 3), 8 }, { INTRA(1, 0, 3), 8 },
				{ INTRA(0, 10, 1), 8 }, { INTRA(0, 10, 1), 8 }, { INTRA(0, 10, 1), 8 },
				{ INTRA(0, 10, 1), 8 }, { INTRA(0, 9, 1), 8 }, { INTRA(0, 9, 1), 8 },
				{ INTRA(0, 9, 1), 8 }, { INTRA(0, 9, 1), 8 }, { INTRA(0, 8, 1), 8 },
				{ INTRA(0, 8, 1), 8 }, { INTRA(0, 8, 1), 8 }, { INTRA(0, 8, 1), 8 },
				{ INTRA(1, 9, 1), 8 }, { INTRA(1, 9, 1), 8 }, { INTRA(1, 9, 1), 8 },
				{ INTRA(1, 9, 1), 8 }, { INTRA(0, 3, 2), 8 }, { INTRA(0, 3, 2), 8 },
				{ INTRA(0, 3, 2), 8 }, { INTRA(0, 3, 2), 8 }, { INTRA(0, 1, 4), 8 },
				{ INTRA(0, 1, 4), 8 }, { INTRA(0, 1, 4), 8 }, { INTRA(0, 1, 4), 8 },
				{ INTRA(0, 0, 12), 8 }, { INTRA(0, 0, 12), 8 }, { INTRA(0, 0, 12), 8 },
				{ INTRA(0, 0, 12), 8 }, { INTRA(0, 0, 11), 8 }, { INTRA(0, 0, 11), 8 },
				{ INTRA(0, 0, 11), 8 }, { INTRA(0, 0, 11), 8 }, { INTRA(0, 0, 10), 8 },
				{ INTRA(0, 0, 10), 8 }, { INTRA(0, 0, 10), 8 }, { INTRA(0, 0, 10), 8 },
			},
			/* 0x8f9c6c: from 8, by all 12 */
			.from8 = {
				{ INTRA(1, 0, 7), 11 }, { INTRA(1, 0, 7), 11 }, { INTRA(1, 0, 6), 11 },
				{ INTRA(1, 0, 6), 11 }, { INTRA(0, 0, 22), 11 }, { INTRA(0, 0, 22), 11 },
				{ INTRA(0, 0, 21), 11 }, { INTRA(0, 0, 21), 11 }, { INTRA(1, 2, 2), 10 },
				{ INTRA(1, 2, 2), 10 }, { INTRA(1, 2, 2), 10 }, { INTRA(1, 2, 2), 10 },
				{ INTRA(1, 1, 3), 10 }, { INTRA(1, 1, 3), 10 }, { INTRA(1, 1, 3), 10 },
				{ INTRA(1, 1, 3), 10 }, { INTRA(1, 0, 5), 10 }, { INTRA(1, 0, 5), 10 },
				{ INTRA(1, 0, 5), 10 }, { INTRA(1, 0, 5), 10 }, { INTRA(0, 13, 1), 10 },
				{ INTRA(0, 13, 1), 10 }, { INTRA(0, 13, 1), 10 }, { INTRA(0, 13, 1), 10 },
				{ INTRA(0, 5, 3), 10 }, { INTRA(0, 5, 3), 10 }, { INTRA(0, 5, 3), 10 },
				{ INTRA(0, 5, 3), 10 }, { INTRA(0, 8, 2), 10 }, { INTRA(0, 8, 2), 10 },
				{ INTRA(0, 8, 2), 10 }, { INTRA(0, 8, 2), 10 }, { INTRA(0, 4, 3), 10 },
				{ INTRA(0, 4, 3), 10 }, { INTRA(0, 4, 3), 10 }, { INTRA(0, 4, 3), 10 },
				{ INTRA(0, 3, 4), 10 }, { INTRA(0, 3, 4), 10 }, { INTRA(0, 3, 4), 10 },
				{ INTRA(0, 3, 4), 10 }, { INTRA(0, 2, 4), 10 }, { INTRA(0, 2, 4), 10 },
				{ INTRA(0, 2, 4), 10 }, { INTRA(0, 2, 4), 10 }, { INTRA(0, 1, 7), 10 },
				{ INTRA(0, 1, 7), 10 }, { INTRA(0, 1, 7), 10 }, { INTRA(0, 1, 7), 10 },
				{ INTRA(0, 0, 20), 10 }, { INTRA(0, 0, 20), 10 }, { INTRA(0, 0, 20), 10 },
				{ INTRA(0, 0, 20), 10 }, { INTRA(0, 0, 19), 10 }, { INTRA(0, 0, 19), 10 },
				{ INTRA(0, 0, 19), 10 }, { INTRA(0, 0, 19), 10 }, { INTRA(0, 0, 23), 11 },
				{ INTRA(0, 0, 23), 11 }, { INTRA(0, 0, 24), 11 }, { INTRA(0, 0, 24), 11 },
				{ INTRA(0, 1, 8), 11 }, { INTRA(0, 1, 8), 11 }, { INTRA(0, 9, 2), 11 },
				{ INTRA(0, 9, 2), 11 }, { INTRA(1, 3, 2), 11 }, { INTRA(1, 3, 2), 11 },
				{ INTRA(1, 4, 2), 11 }, { INTRA(1, 4, 2), 11 }, { INTRA(1, 15, 1), 11 },
				{ INTRA(1, 15, 1), 11 }, { INTRA(1, 16, 1), 11 }, { INTRA(1, 16, 1), 11 },
				{ INTRA(0, 0, 25), 12 }, { INTRA(0, 0, 26), 12 }, { INTRA(0, 0, 27), 12 },
				{ INTRA(0, 1, 9), 12 }, { INTRA(0, 6, 3), 12 }, { INTRA(0, 1, 10), 12 },
				{ INTRA(0, 2, 5), 12 }, { INTRA(0, 7, 3), 12 }, { INTRA(0, 14, 1), 12 },
				{ INTRA(1, 0, 8), 12 }, { INTRA(1, 5, 2), 12 }, { INTRA(1, 6, 2), 12 },
				{ INTRA(1, 17, 1), 12 }, { INTRA(1, 18, 1), 12 }, { INTRA(1, 19, 1), 12 },
				{ INTRA(1, 20, 1), 12 }, { TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 },
				{ TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 },
				{ TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 },
				{ TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 },
				{ TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 },
				{ TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 },
				{ TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 },
				{ TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 },
				{ TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 },
				{ TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 },
				{ TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 }, { TCOEF_ESCAPE, 7 },
			},
		},
	},
	/* 0x8fa02c: the largest level of a run, by last and run */
	.lmaxIntra = {
		{
			27, 10, 5, 4, 3, 3, 3, 3, 2, 2, 1, 1, 1, 1, 1, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		}, {
			8, 3, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		},
	},
	/* 0x8fa22c: the largest level of a run, by last and run */
	.lmaxInter = {
		{
			12, 6, 4, 3, 3, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		}, {
			3, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		},
	},
	/* 0x8fa42c: the longest run of a level, not last */
	.rmaxIntra = {
		999, 14, 9, 7, 3, 2, 1, 1, 1, 1, 1, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	},
	/* 0x8fa49c: the longest run of a level, last */
	.rmaxIntraLast = {
		999, 20, 6, 1, 0, 0, 0, 0, 0,
	},
	/* 0x8fa4c0: the longest run of a level, not last */
	.rmaxInter = {
		999, 26, 10, 6, 2, 1, 1, 0, 0, 0, 0, 0, 0,
	},
	/* 0x8fa4f4: the longest run of a level, last */
	.rmaxInterLast = {
		999, 40, 1, 0,
	},
	.none = { 0, 0, 0, 0 },
	/* 0x8fa50c: by the index of the standard's table */
	.rvlcIntra = {
		{ INTRA(0, 0, 1), 3 }, { INTRA(0, 0, 2), 3 }, { INTRA(0, 0, 3), 4 },
		{ INTRA(0, 0, 4), 5 }, { INTRA(0, 0, 5), 6 }, { INTRA(0, 0, 6), 6 },
		{ INTRA(0, 0, 7), 7 }, { INTRA(0, 0, 8), 8 }, { INTRA(0, 0, 9), 8 },
		{ INTRA(0, 0, 10), 9 }, { INTRA(0, 0, 11), 9 }, { INTRA(0, 0, 12), 10 },
		{ INTRA(0, 0, 13), 10 }, { INTRA(0, 0, 14), 10 }, { INTRA(0, 0, 15), 11 },
		{ INTRA(0, 0, 16), 11 }, { INTRA(0, 0, 17), 11 }, { INTRA(0, 0, 18), 12 },
		{ INTRA(0, 0, 19), 12 }, { INTRA(0, 0, 20), 13 }, { INTRA(0, 0, 21), 13 },
		{ INTRA(0, 0, 22), 12 }, { INTRA(0, 0, 23), 13 }, { INTRA(0, 0, 24), 14 },
		{ INTRA(0, 0, 25), 14 }, { INTRA(0, 0, 26), 14 }, { INTRA(0, 0, 27), 15 },
		{ INTRA(0, 1, 1), 4 }, { INTRA(0, 1, 2), 5 }, { INTRA(0, 1, 3), 7 },
		{ INTRA(0, 1, 4), 8 }, { INTRA(0, 1, 5), 8 }, { INTRA(0, 1, 6), 9 },
		{ INTRA(0, 1, 7), 10 }, { INTRA(0, 1, 8), 11 }, { INTRA(0, 1, 9), 11 },
		{ INTRA(0, 1, 10), 12 }, { INTRA(0, 1, 11), 13 }, { INTRA(0, 1, 12), 14 },
		{ INTRA(0, 1, 13), 14 }, { INTRA(0, 2, 1), 5 }, { INTRA(0, 2, 2), 7 },
		{ INTRA(0, 2, 3), 9 }, { INTRA(0, 2, 4), 10 }, { INTRA(0, 2, 5), 11 },
		{ INTRA(0, 2, 6), 11 }, { INTRA(0, 2, 7), 13 }, { INTRA(0, 2, 8), 13 },
		{ INTRA(0, 2, 9), 13 }, { INTRA(0, 2, 10), 14 }, { INTRA(0, 2, 11), 14 },
		{ INTRA(0, 3, 1), 5 }, { INTRA(0, 3, 2), 8 }, { INTRA(0, 3, 3), 9 },
		{ INTRA(0, 3, 4), 11 }, { INTRA(0, 3, 5), 12 }, { INTRA(0, 3, 6), 13 },
		{ INTRA(0, 3, 7), 14 }, { INTRA(0, 3, 8), 14 }, { INTRA(0, 3, 9), 15 },
		{ INTRA(0, 4, 1), 6 }, { INTRA(0, 4, 2), 8 }, { INTRA(0, 4, 3), 10 },
		{ INTRA(0, 4, 4), 12 }, { INTRA(0, 4, 5), 12 }, { INTRA(0, 4, 6), 14 },
		{ INTRA(0, 5, 1), 6 }, { INTRA(0, 5, 2), 9 }, { INTRA(0, 5, 3), 11 },
		{ INTRA(0, 5, 4), 12 }, { INTRA(0, 5, 5), 14 }, { INTRA(0, 5, 6), 14 },
		{ INTRA(0, 6, 1), 7 }, { INTRA(0, 6, 2), 10 }, { INTRA(0, 6, 3), 11 },
		{ INTRA(0, 6, 4), 12 }, { INTRA(0, 6, 5), 15 }, { INTRA(0, 7, 1), 7 },
		{ INTRA(0, 7, 2), 10 }, { INTRA(0, 7, 3), 11 }, { INTRA(0, 7, 4), 13 },
		{ INTRA(0, 7, 5), 15 }, { INTRA(0, 8, 1), 8 }, { INTRA(0, 8, 2), 10 },
		{ INTRA(0, 8, 3), 13 }, { INTRA(0, 8, 4), 14 }, { INTRA(0, 9, 1), 8 },
		{ INTRA(0, 9, 2), 11 }, { INTRA(0, 9, 3), 13 }, { INTRA(0, 9, 4), 15 },
		{ INTRA(0, 10, 1), 9 }, { INTRA(0, 10, 2), 12 }, { INTRA(0, 11, 1), 10 },
		{ INTRA(0, 11, 2), 13 }, { INTRA(0, 12, 1), 10 }, { INTRA(0, 12, 2), 15 },
		{ INTRA(0, 13, 1), 11 }, { INTRA(0, 14, 1), 13 }, { INTRA(0, 15, 1), 13 },
		{ INTRA(0, 16, 1), 14 }, { INTRA(0, 17, 1), 14 }, { INTRA(0, 18, 1), 14 },
		{ INTRA(0, 19, 1), 15 }, { INTRA(1, 0, 1), 4 }, { INTRA(1, 0, 2), 8 },
		{ INTRA(1, 0, 3), 11 }, { INTRA(1, 0, 4), 13 }, { INTRA(1, 0, 5), 14 },
		{ INTRA(1, 1, 1), 5 }, { INTRA(1, 1, 2), 9 }, { INTRA(1, 1, 3), 12 },
		{ INTRA(1, 1, 4), 14 }, { INTRA(1, 1, 5), 15 }, { INTRA(1, 2, 1), 5 },
		{ INTRA(1, 2, 2), 11 }, { INTRA(1, 2, 3), 15 }, { INTRA(1, 3, 1), 6 },
		{ INTRA(1, 3, 2), 12 }, { INTRA(1, 4, 1), 6 }, { INTRA(1, 4, 2), 12 },
		{ INTRA(1, 5, 1), 6 }, { INTRA(1, 5, 2), 13 }, { INTRA(1, 6, 1), 6 },
		{ INTRA(1, 6, 2), 13 }, { INTRA(1, 7, 1), 7 }, { INTRA(1, 7, 2), 13 },
		{ INTRA(1, 8, 1), 7 }, { INTRA(1, 8, 2), 13 }, { INTRA(1, 9, 1), 7 },
		{ INTRA(1, 9, 2), 13 }, { INTRA(1, 10, 1), 7 }, { INTRA(1, 10, 2), 14 },
		{ INTRA(1, 11, 1), 7 }, { INTRA(1, 11, 2), 14 }, { INTRA(1, 12, 1), 8 },
		{ INTRA(1, 12, 2), 14 }, { INTRA(1, 13, 1), 8 }, { INTRA(1, 13, 2), 15 },
		{ INTRA(1, 14, 1), 8 }, { INTRA(1, 15, 1), 9 }, { INTRA(1, 16, 1), 9 },
		{ INTRA(1, 17, 1), 9 }, { INTRA(1, 18, 1), 9 }, { INTRA(1, 19, 1), 9 },
		{ INTRA(1, 20, 1), 9 }, { INTRA(1, 21, 1), 10 }, { INTRA(1, 22, 1), 10 },
		{ INTRA(1, 23, 1), 10 }, { INTRA(1, 24, 1), 10 }, { INTRA(1, 25, 1), 10 },
		{ INTRA(1, 26, 1), 11 }, { INTRA(1, 27, 1), 11 }, { INTRA(1, 28, 1), 11 },
		{ INTRA(1, 29, 1), 12 }, { INTRA(1, 30, 1), 12 }, { INTRA(1, 31, 1), 12 },
		{ INTRA(1, 32, 1), 12 }, { INTRA(1, 33, 1), 12 }, { INTRA(1, 34, 1), 12 },
		{ INTRA(1, 35, 1), 12 }, { INTRA(1, 36, 1), 13 }, { INTRA(1, 37, 1), 13 },
		{ INTRA(1, 38, 1), 14 }, { INTRA(1, 39, 1), 14 }, { INTRA(1, 40, 1), 14 },
		{ INTRA(1, 41, 1), 15 }, { INTRA(1, 42, 1), 15 }, { INTRA(1, 43, 1), 15 },
		{ INTRA(1, 44, 1), 15 }, { TCOEF_ESCAPE, 4 },
	},
	/* 0x8faa5c: by the index of the standard's table */
	.rvlcInter = {
		{ INTRA(0, 0, 1), 3 }, { INTRA(0, 0, 2), 4 }, { INTRA(0, 0, 3), 5 },
		{ INTRA(0, 0, 4), 7 }, { INTRA(0, 0, 5), 8 }, { INTRA(0, 0, 6), 8 },
		{ INTRA(0, 0, 7), 9 }, { INTRA(0, 0, 8), 10 }, { INTRA(0, 0, 9), 10 },
		{ INTRA(0, 0, 10), 11 }, { INTRA(0, 0, 11), 11 }, { INTRA(0, 0, 12), 12 },
		{ INTRA(0, 0, 13), 13 }, { INTRA(0, 0, 14), 13 }, { INTRA(0, 0, 15), 13 },
		{ INTRA(0, 0, 16), 13 }, { INTRA(0, 0, 17), 14 }, { INTRA(0, 0, 18), 14 },
		{ INTRA(0, 0, 19), 15 }, { INTRA(0, 1, 1), 3 }, { INTRA(0, 1, 2), 6 },
		{ INTRA(0, 1, 3), 8 }, { INTRA(0, 1, 4), 9 }, { INTRA(0, 1, 5), 10 },
		{ INTRA(0, 1, 6), 11 }, { INTRA(0, 1, 7), 12 }, { INTRA(0, 1, 8), 13 },
		{ INTRA(0, 1, 9), 14 }, { INTRA(0, 1, 10), 14 }, { INTRA(0, 2, 1), 4 },
		{ INTRA(0, 2, 2), 7 }, { INTRA(0, 2, 3), 9 }, { INTRA(0, 2, 4), 11 },
		{ INTRA(0, 2, 5), 12 }, { INTRA(0, 2, 6), 14 }, { INTRA(0, 2, 7), 14 },
		{ INTRA(0, 3, 1), 5 }, { INTRA(0, 3, 2), 8 }, { INTRA(0, 3, 3), 10 },
		{ INTRA(0, 3, 4), 12 }, { INTRA(0, 3, 5), 13 }, { INTRA(0, 3, 6), 14 },
		{ INTRA(0, 3, 7), 15 }, { INTRA(0, 4, 1), 5 }, { INTRA(0, 4, 2), 8 },
		{ INTRA(0, 4, 3), 11 }, { INTRA(0, 4, 4), 13 }, { INTRA(0, 4, 5), 15 },
		{ INTRA(0, 5, 1), 5 }, { INTRA(0, 5, 2), 9 }, { INTRA(0, 5, 3), 11 },
		{ INTRA(0, 5, 4), 13 }, { INTRA(0, 6, 1), 6 }, { INTRA(0, 6, 2), 10 },
		{ INTRA(0, 6, 3), 12 }, { INTRA(0, 6, 4), 14 }, { INTRA(0, 7, 1), 6 },
		{ INTRA(0, 7, 2), 10 }, { INTRA(0, 7, 3), 12 }, { INTRA(0, 7, 4), 15 },
		{ INTRA(0, 8, 1), 6 }, { INTRA(0, 8, 2), 10 }, { INTRA(0, 8, 3), 13 },
		{ INTRA(0, 9, 1), 7 }, { INTRA(0, 9, 2), 10 }, { INTRA(0, 9, 3), 14 },
		{ INTRA(0, 10, 1), 7 }, { INTRA(0, 10, 2), 11 }, { INTRA(0, 11, 1), 7 },
		{ INTRA(0, 11, 2), 12 }, { INTRA(0, 12, 1), 8 }, { INTRA(0, 12, 2), 13 },
		{ INTRA(0, 13, 1), 8 }, { INTRA(0, 13, 2), 14 }, { INTRA(0, 14, 1), 8 },
		{ INTRA(0, 14, 2), 14 }, { INTRA(0, 15, 1), 9 }, { INTRA(0, 15, 2), 14 },
		{ INTRA(0, 16, 1), 9 }, { INTRA(0, 16, 2), 14 }, { INTRA(0, 17, 1), 9 },
		{ INTRA(0, 17, 2), 15 }, { INTRA(0, 18, 1), 10 }, { INTRA(0, 19, 1), 10 },
		{ INTRA(0, 20, 1), 10 }, { INTRA(0, 21, 1), 11 }, { INTRA(0, 22, 1), 11 },
		{ INTRA(0, 23, 1), 11 }, { INTRA(0, 24, 1), 11 }, { INTRA(0, 25, 1), 11 },
		{ INTRA(0, 26, 1), 11 }, { INTRA(0, 27, 1), 12 }, { INTRA(0, 28, 1), 12 },
		{ INTRA(0, 29, 1), 12 }, { INTRA(0, 30, 1), 13 }, { INTRA(0, 31, 1), 13 },
		{ INTRA(0, 32, 1), 13 }, { INTRA(0, 33, 1), 13 }, { INTRA(0, 34, 1), 14 },
		{ INTRA(0, 35, 1), 14 }, { INTRA(0, 36, 1), 14 }, { INTRA(0, 37, 1), 15 },
		{ INTRA(0, 38, 1), 15 }, { INTRA(1, 0, 1), 4 }, { INTRA(1, 0, 2), 8 },
		{ INTRA(1, 0, 3), 11 }, { INTRA(1, 0, 4), 13 }, { INTRA(1, 0, 5), 14 },
		{ INTRA(1, 1, 1), 5 }, { INTRA(1, 1, 2), 9 }, { INTRA(1, 1, 3), 12 },
		{ INTRA(1, 1, 4), 14 }, { INTRA(1, 1, 5), 15 }, { INTRA(1, 2, 1), 5 },
		{ INTRA(1, 2, 2), 11 }, { INTRA(1, 2, 3), 15 }, { INTRA(1, 3, 1), 6 },
		{ INTRA(1, 3, 2), 12 }, { INTRA(1, 4, 1), 6 }, { INTRA(1, 4, 2), 12 },
		{ INTRA(1, 5, 1), 6 }, { INTRA(1, 5, 2), 13 }, { INTRA(1, 6, 1), 6 },
		{ INTRA(1, 6, 2), 13 }, { INTRA(1, 7, 1), 7 }, { INTRA(1, 7, 2), 13 },
		{ INTRA(1, 8, 1), 7 }, { INTRA(1, 8, 2), 13 }, { INTRA(1, 9, 1), 7 },
		{ INTRA(1, 9, 2), 13 }, { INTRA(1, 10, 1), 7 }, { INTRA(1, 10, 2), 14 },
		{ INTRA(1, 11, 1), 7 }, { INTRA(1, 11, 2), 14 }, { INTRA(1, 12, 1), 8 },
		{ INTRA(1, 12, 2), 14 }, { INTRA(1, 13, 1), 8 }, { INTRA(1, 13, 2), 15 },
		{ INTRA(1, 14, 1), 8 }, { INTRA(1, 15, 1), 9 }, { INTRA(1, 16, 1), 9 },
		{ INTRA(1, 17, 1), 9 }, { INTRA(1, 18, 1), 9 }, { INTRA(1, 19, 1), 9 },
		{ INTRA(1, 20, 1), 9 }, { INTRA(1, 21, 1), 10 }, { INTRA(1, 22, 1), 10 },
		{ INTRA(1, 23, 1), 10 }, { INTRA(1, 24, 1), 10 }, { INTRA(1, 25, 1), 10 },
		{ INTRA(1, 26, 1), 11 }, { INTRA(1, 27, 1), 11 }, { INTRA(1, 28, 1), 11 },
		{ INTRA(1, 29, 1), 12 }, { INTRA(1, 30, 1), 12 }, { INTRA(1, 31, 1), 12 },
		{ INTRA(1, 32, 1), 12 }, { INTRA(1, 33, 1), 12 }, { INTRA(1, 34, 1), 12 },
		{ INTRA(1, 35, 1), 12 }, { INTRA(1, 36, 1), 13 }, { INTRA(1, 37, 1), 13 },
		{ INTRA(1, 38, 1), 14 }, { INTRA(1, 39, 1), 14 }, { INTRA(1, 40, 1), 14 },
		{ INTRA(1, 41, 1), 15 }, { INTRA(1, 42, 1), 15 }, { INTRA(1, 43, 1), 15 },
		{ INTRA(1, 44, 1), 15 }, { TCOEF_ESCAPE, 4 },
	},
	.rvlcNone = { 0, 0, 0, 0 },
};

/* 0x84423c: the run's buffers at a row of macroblocks */
void m4vdecRowBuffers(VideoM4vJob *job, const VideoHelperJob *run, uint32_t row)
{
	const VideoM4vCommand *command = (const VideoM4vCommand *)job->unit->command;

	job->macroblock = (VideoM4vMacroblock *)(run->buffers[0] + row * command->mbRowSize);
	job->vectors = (int16_t *)(run->buffers[4] + row * command->vectorRowSize);
	job->luma = (int16_t *)(run->buffers[5] + row * command->lumaRowSize);
	job->cb = (int16_t *)(run->buffers[6] + row * command->chromaRowSize);
	job->cr = (int16_t *)(run->buffers[7] + row * command->chromaRowSize);
}

/* 0x8442a8: an I-VOP's mcbpc */
uint32_t m4vdecMcbpcIntra(VideoBitstream *bits, VideoM4vJob *job)
{
	uint32_t code = m4vdecBitsPeek(bits, 9);
	const M4vdecVlc *vlc;

	if (code == 1) {
		m4vdecBitsRead(bits, 9);
		return MCBPC_TYPE_STUFFING;
	}
	if (code < 8)
		m4vdecParserError(NULL, job, 4, ERROR_VLC);
	code >>= 3;
	if (code >= 32) {
		m4vdecBitsRead(bits, 1);
		return MCBPC(3, 0);
	}
	vlc = &g_m4vdecVlcTables.mcbpcIntra[code];
	m4vdecBitsRead(bits, vlc->length);
	return vlc->value;
}

/* 0x844318: a P-VOP's mcbpc */
uint32_t m4vdecMcbpcInter(VideoBitstream *bits, VideoM4vJob *job)
{
	uint32_t code = m4vdecBitsPeek(bits, 9);
	const M4vdecVlc *vlc;

	if (code == 1) {
		m4vdecBitsRead(bits, 9);
		return MCBPC_TYPE_STUFFING;
	}
	if (code == 0)
		m4vdecParserError(NULL, job, 4, ERROR_VLC);
	if (code >= 256) {
		m4vdecBitsRead(bits, 1);
		return MCBPC(0, 0);
	}
	vlc = &g_m4vdecVlcTables.mcbpcInter[code];
	m4vdecBitsRead(bits, vlc->length);
	return vlc->value;
}

/* a TCOEF code's entry, from the next 12 bits; NULL below 8 */
static inline const M4vdecVlc *tcoefEntry(VideoBitstream *bits, uint32_t intra)
{
	const M4vdecTcoefVlc *tables = &g_m4vdecVlcTables.tcoef[intra != 0];
	uint32_t code = m4vdecBitsPeek(bits, 12);

	if (code >= 0x200)
		return &tables->from512[(code >> 5) - 16];
	if (code >= 0x80)
		return &tables->from128[(code >> 2) - 32];
	if (code >= 8)
		return &tables->from8[code - 8];
	return NULL;
}

/* read the code of an entry, and its coefficient */
static inline void tcoefTake(VideoBitstream *bits, const M4vdecVlc *vlc, uint32_t intra,
	M4vdecCoefficient *c)
{
	m4vdecBitsRead(bits, vlc->length);
	if (intra) {
		c->run = (vlc->value >> 8) & 0xff;
		c->level = vlc->value & 0xff;
		c->last = (vlc->value >> 16) & 1;
	} else {
		c->run = (vlc->value >> 4) & 0xff;
		c->level = vlc->value & 0xf;
		c->last = (vlc->value >> 12) & 1;
	}
}

/*
 * 0x844384: a block's TCOEF, with its escapes: H.263's (last, run and an
 * 8-bit level), and MPEG-4's three (the level past LMAX, the run past RMAX,
 * or all of it fixed length)
 */
void m4vdecCoefficientRead(M4vdecCoefficient *out, VideoBitstream *bits, VideoM4vJob *job,
	uint32_t intra, uint32_t shortHeader)
{
	M4vdecCoefficient c = g_m4vdecVlcTables.none;
	const M4vdecVlc *vlc;

	if (shortHeader)
		intra = 0;
	vlc = tcoefEntry(bits, intra);
	if (vlc == NULL)
		goto done;
	tcoefTake(bits, vlc, intra, &c);
	if (vlc->value != TCOEF_ESCAPE) {
		c.sign = m4vdecBitsRead(bits, 1);
		goto done;
	}

	if (shortHeader) {
		c.last = m4vdecBitsRead(bits, 1);
		c.run = m4vdecBitsRead(bits, 6);
		c.level = m4vdecBitsRead(bits, 8);
		if (c.level == 0)
			m4vdecParserError(NULL, job, 4, ERROR_VLC);
		if (c.level == 128)
			m4vdecParserError(NULL, job, 4, ERROR_VLC);
		if (c.level < 128) {
			c.sign = 0;
		} else {
			c.sign = 1;
			c.level = 256 - c.level;
		}
	} else if (m4vdecBitsRead(bits, 1) == 0) {
		/* type 1: the level past the run's largest */
		vlc = tcoefEntry(bits, intra);
		if (vlc == NULL)
			goto done;
		tcoefTake(bits, vlc, intra, &c);
		if (intra)
			c.level += g_m4vdecVlcTables.lmaxIntra[c.last][c.run];
		else
			c.level += g_m4vdecVlcTables.lmaxInter[c.last][c.run];
		c.sign = m4vdecBitsRead(bits, 1);
	} else if (m4vdecBitsRead(bits, 1) == 0) {
		/* type 2: the run past the level's longest */
		const int32_t *rmax;

		vlc = tcoefEntry(bits, intra);
		if (vlc == NULL)
			goto done;
		tcoefTake(bits, vlc, intra, &c);
		if (intra)
			rmax = c.last ? g_m4vdecVlcTables.rmaxIntraLast : g_m4vdecVlcTables.rmaxIntra;
		else
			rmax = c.last ? g_m4vdecVlcTables.rmaxInterLast : g_m4vdecVlcTables.rmaxInter;
		c.run += rmax[c.level] + 1;
		c.sign = m4vdecBitsRead(bits, 1);
	} else {
		/* type 3: fixed length */
		c.last = m4vdecBitsRead(bits, 1);
		c.run = m4vdecBitsRead(bits, 6);
		m4vdecBitsRead(bits, 1);		/* marker_bit */
		c.level = m4vdecBitsRead(bits, 12);
		m4vdecBitsRead(bits, 1);		/* marker_bit */
		if (c.level < 2048) {
			c.sign = 0;
		} else {
			c.sign = 1;
			c.level = 4096 - c.level;
		}
	}
done:
	*out = c;
}

/* a reversible code's entry in the intra and the inter table */
#define RVLC(code, inter, intra) \
	case code: \
		vlc = intraBlock ? &g_m4vdecVlcTables.rvlcIntra[intra] : \
			&g_m4vdecVlcTables.rvlcInter[inter]; \
		break;

/*
 * 0x844706: a block's TCOEF in the reversible VLC (a data partitioned VOP
 * with reversible_vlc): the code's length from its bits (from a 1, up to
 * the next 1; from a 0, up to the second 0 after it, and one more), then
 * the code
 */
void m4vdecCoefficientReadReversible(M4vdecCoefficient *out, VideoBitstream *bits,
	uint32_t intraBlock)
{
	M4vdecCoefficient c = g_m4vdecVlcTables.rvlcNone;
	const M4vdecVlc *vlc;
	uint32_t code = m4vdecBitsPeek(bits, 15);
	uint32_t bit = 0x4000;
	uint32_t length = 1;
	int32_t left = 2;

	if (code & 0x4000) {
		left = 1;
		while (left > 0 && bit != 0) {
			bit >>= 1;
			if (code & bit)
				left--;
			length++;
		}
	} else {
		while (left > 0 && bit != 0) {
			bit >>= 1;
			if ((code & bit) == 0)
				left--;
			length++;
		}
	}
	length++;
	code = (code & 0x7fff) >> (15 - length);

	switch (code) {
	RVLC(0x3f7d, 168, 168)	/* 011111101111101 */
	RVLC(0x3f7c, 167, 167)	/* 011111101111100 */
	RVLC(0x3efd, 166, 166)	/* 011111011111101 */
	RVLC(0x3efc, 165, 165)	/* 011111011111100 */
	RVLC(0x3dfd, 137, 137)	/* 011110111111101 */
	RVLC(0x3dfc, 115, 115)	/* 011110111111100 */
	RVLC(0x3bfd, 112, 112)	/* 011101111111101 */
	RVLC(0x3bfc, 102, 102)	/* 011101111111100 */
	RVLC(0x37fd, 101,  95)	/* 011011111111101 */
	RVLC(0x37fc,  81,  89)	/* 011011111111100 */
	RVLC(0x2ffd,  59,  81)	/* 010111111111101 */
	RVLC(0x2ffc,  47,  76)	/* 010111111111100 */
	RVLC(0x2003, 164, 164)	/* 10000000000011 */
	RVLC(0x2002, 163, 163)	/* 10000000000010 */
	RVLC(0x1ffd,  42,  59)	/* 001111111111101 */
	RVLC(0x1ffc,  18,  26)	/* 001111111111100 */
	RVLC(0x1ff9, 162, 162)	/* 01111111111001 */
	RVLC(0x1ff8, 135, 135)	/* 01111111111000 */
	RVLC(0x1ff5, 133, 133)	/* 01111111110101 */
	RVLC(0x1ff4, 131, 131)	/* 01111111110100 */
	RVLC(0x1fed, 111, 111)	/* 01111111101101 */
	RVLC(0x1fec, 107, 107)	/* 01111111101100 */
	RVLC(0x1fdd, 100,  25)	/* 01111111011101 */
	RVLC(0x1fdc,  99,  24)	/* 01111111011100 */
	RVLC(0x1fbd,  98,  23)	/* 01111110111101 */
	RVLC(0x1fbc,  79,  39)	/* 01111110111100 */
	RVLC(0x1f7d,  77,  38)	/* 01111101111101 */
	RVLC(0x1f7c,  75,  50)	/* 01111101111100 */
	RVLC(0x1efd,  73,  49)	/* 01111011111101 */
	RVLC(0x1efc,  65,  58)	/* 01111011111100 */
	RVLC(0x1dfd,  55,  57)	/* 01110111111101 */
	RVLC(0x1dfc,  41,  71)	/* 01110111111100 */
	RVLC(0x1bfd,  35,  65)	/* 01101111111101 */
	RVLC(0x1bfc,  34,  70)	/* 01101111111100 */
	RVLC(0x17fd,  28,  85)	/* 01011111111101 */
	RVLC(0x17fc,  27, 101)	/* 01011111111100 */
	RVLC(0x1003, 161, 161)	/* 1000000000011 */
	RVLC(0x1002, 160, 160)	/* 1000000000010 */
	RVLC(0x0ffd,  17, 100)	/* 00111111111101 */
	RVLC(0x0ffc,  16,  99)	/* 00111111111100 */
	RVLC(0x0ff9, 129, 129)	/* 0111111111001 */
	RVLC(0x0ff8, 127, 127)	/* 0111111111000 */
	RVLC(0x0ff5, 125, 125)	/* 0111111110101 */
	RVLC(0x0ff4, 123, 123)	/* 0111111110100 */
	RVLC(0x0fed, 121, 121)	/* 0111111101101 */
	RVLC(0x0fec, 106, 106)	/* 0111111101100 */
	RVLC(0x0fdd,  97,  22)	/* 0111111011101 */
	RVLC(0x0fdc,  96,  20)	/* 0111111011100 */
	RVLC(0x0fbd,  95,  19)	/* 0111110111101 */
	RVLC(0x0fbc,  94,  37)	/* 0111110111100 */
	RVLC(0x0f7d,  71,  48)	/* 0111101111101 */
	RVLC(0x0f7c,  62,  47)	/* 0111101111100 */
	RVLC(0x0efd,  51,  46)	/* 0111011111101 */
	RVLC(0x0efc,  46,  56)	/* 0111011111100 */
	RVLC(0x0dfd,  40,  80)	/* 0110111111101 */
	RVLC(0x0dfc,  26,  88)	/* 0110111111100 */
	RVLC(0x0bfd,  15,  84)	/* 0101111111101 */
	RVLC(0x0bfc,  14,  93)	/* 0101111111100 */
	RVLC(0x0803, 159, 159)	/* 100000000011 */
	RVLC(0x0802, 158, 158)	/* 100000000010 */
	RVLC(0x07fd,  13,  98)	/* 0011111111101 */
	RVLC(0x07fc,  12,  97)	/* 0011111111100 */
	RVLC(0x07f9, 157, 157)	/* 011111111001 */
	RVLC(0x07f8, 156, 156)	/* 011111111000 */
	RVLC(0x07f5, 155, 155)	/* 011111110101 */
	RVLC(0x07f4, 154, 154)	/* 011111110100 */
	RVLC(0x07ed, 153, 153)	/* 011111101101 */
	RVLC(0x07ec, 119, 119)	/* 011111101100 */
	RVLC(0x07dd, 117, 117)	/* 011111011101 */
	RVLC(0x07dc, 110, 110)	/* 011111011100 */
	RVLC(0x07bd,  93,  21)	/* 011110111101 */
	RVLC(0x07bc,  92,  18)	/* 011110111100 */
	RVLC(0x077d,  91,  17)	/* 011101111101 */
	RVLC(0x077c,  69,  36)	/* 011101111100 */
	RVLC(0x06fd,  58,  64)	/* 011011111101 */
	RVLC(0x06fc,  54,  55)	/* 011011111100 */
	RVLC(0x05fd,  39,  75)	/* 010111111101 */
	RVLC(0x05fc,  33,  69)	/* 010111111100 */
	RVLC(0x0403, 152, 152)	/* 10000000011 */
	RVLC(0x0402, 151, 151)	/* 10000000010 */
	RVLC(0x03fd,  25,  63)	/* 001111111101 */
	RVLC(0x03fc,  11,  91)	/* 001111111100 */
	RVLC(0x03f9, 150, 150)	/* 01111111001 */
	RVLC(0x03f8, 114, 114)	/* 01111111000 */
	RVLC(0x03f5, 105, 105)	/* 01111110101 */
	RVLC(0x03f4,  90,  16)	/* 01111110100 */
	RVLC(0x03ed,  89,  15)	/* 01111101101 */
	RVLC(0x03ec,  88,  14)	/* 01111101100 */
	RVLC(0x03dd,  87,  35)	/* 01111011101 */
	RVLC(0x03dc,  86,  34)	/* 01111011100 */
	RVLC(0x03bd,  85,  45)	/* 01110111101 */
	RVLC(0x03bc,  67,  44)	/* 01110111100 */
	RVLC(0x037d,  50,  54)	/* 01101111101 */
	RVLC(0x037c,  45,  79)	/* 01101111100 */
	RVLC(0x02fd,  32,  74)	/* 01011111101 */
	RVLC(0x02fc,  24,  68)	/* 01011111100 */
	RVLC(0x0203, 149, 149)	/* 1000000011 */
	RVLC(0x0202, 148, 148)	/* 1000000010 */
	RVLC(0x01fd,  10,  87)	/* 00111111101 */
	RVLC(0x01fc,   9,  96)	/* 00111111100 */
	RVLC(0x01f9, 147, 147)	/* 0111111001 */
	RVLC(0x01f8, 146, 146)	/* 0111111000 */
	RVLC(0x01f5, 145, 145)	/* 0111110101 */
	RVLC(0x01f4,  84,  13)	/* 0111110100 */
	RVLC(0x01ed,  83,  12)	/* 0111101101 */
	RVLC(0x01ec,  82,  11)	/* 0111101100 */
	RVLC(0x01dd,  64,  33)	/* 0111011101 */
	RVLC(0x01dc,  61,  43)	/* 0111011100 */
	RVLC(0x01bd,  57,  62)	/* 0110111101 */
	RVLC(0x01bc,  53,  83)	/* 0110111100 */
	RVLC(0x017d,  38,  78)	/* 0101111101 */
	RVLC(0x017c,  23,  73)	/* 0101111100 */
	RVLC(0x0103, 144, 144)	/* 100000011 */
	RVLC(0x0102, 143, 143)	/* 100000010 */
	RVLC(0x00fd,   8,  94)	/* 0011111101 */
	RVLC(0x00fc,   7,  92)	/* 0011111100 */
	RVLC(0x00f9, 142, 142)	/* 011111001 */
	RVLC(0x00f8, 141, 141)	/* 011111000 */
	RVLC(0x00f5, 140, 140)	/* 011110101 */
	RVLC(0x00f4, 139, 139)	/* 011110100 */
	RVLC(0x00ed, 109, 109)	/* 011101101 */
	RVLC(0x00ec,  80,  10)	/* 011101100 */
	RVLC(0x00dd,  78,   9)	/* 011011101 */
	RVLC(0x00dc,  76,  32)	/* 011011100 */
	RVLC(0x00bd,  49,  53)	/* 010111101 */
	RVLC(0x00bc,  31,  42)	/* 010111100 */
	RVLC(0x0083, 138, 138)	/* 10000011 */
	RVLC(0x0082, 136, 136)	/* 10000010 */
	RVLC(0x007d,  22,  67)	/* 001111101 */
	RVLC(0x007c,   6,  90)	/* 001111100 */
	RVLC(0x0079, 134, 134)	/* 01111001 */
	RVLC(0x0078, 104, 104)	/* 01111000 */
	RVLC(0x0075,  74,   8)	/* 01110101 */
	RVLC(0x0074,  72,   7)	/* 01110100 */
	RVLC(0x006d,  70,  31)	/* 01101101 */
	RVLC(0x006c,  44,  30)	/* 01101100 */
	RVLC(0x005d,  37,  61)	/* 01011101 */
	RVLC(0x005c,  21,  52)	/* 01011100 */
	RVLC(0x0043, 132, 132)	/* 1000011 */
	RVLC(0x0042, 130, 130)	/* 1000010 */
	RVLC(0x003d,   5,  86)	/* 00111101 */
	RVLC(0x003c,   4,  82)	/* 00111100 */
	RVLC(0x0039, 128, 128)	/* 0111001 */
	RVLC(0x0038, 126, 126)	/* 0111000 */
	RVLC(0x0035, 124, 124)	/* 0110101 */
	RVLC(0x0034,  68,   6)	/* 0110100 */
	RVLC(0x002d,  66,  29)	/* 0101101 */
	RVLC(0x002c,  63,  41)	/* 0101100 */
	RVLC(0x0023, 122, 122)	/* 100011 */
	RVLC(0x0022, 120, 120)	/* 100010 */
	RVLC(0x001d,  30,  77)	/* 0011101 */
	RVLC(0x001c,   3,  72)	/* 0011100 */
	RVLC(0x0019, 118, 118)	/* 011001 */
	RVLC(0x0018, 116, 116)	/* 011000 */
	RVLC(0x0015,  60,   5)	/* 010101 */
	RVLC(0x0014,  56,   4)	/* 010100 */
	RVLC(0x0013, 113, 113)	/* 10011 */
	RVLC(0x0012, 108, 108)	/* 10010 */
	RVLC(0x000d,  52,  66)	/* 001101 */
	RVLC(0x000c,  20,  60)	/* 001100 */
	RVLC(0x000b, 103, 103)	/* 1011 */
	RVLC(0x000a,  29,   2)	/* 1010 */
	RVLC(0x0009,  48,   3)	/* 01001 */
	RVLC(0x0008,  43,  28)	/* 01000 */
	RVLC(0x0007,  19,   1)	/* 111 */
	RVLC(0x0006,   0,   0)	/* 110 */
	RVLC(0x0005,  36,  51)	/* 00101 */
	RVLC(0x0004,   2,  40)	/* 00100 */
	RVLC(0x0001,   1,  27)	/* 0001 */
	RVLC(0x0000, 169, 169)	/* 0000 */
	default:
		*out = c;
		return;
	}
	m4vdecBitsRead(bits, vlc->length);
	c.run = (vlc->value >> 8) & 0xff;
	c.level = vlc->value & 0xff;
	c.last = (vlc->value >> 16) & 1;
	if (vlc->value != TCOEF_ESCAPE) {
		c.sign = m4vdecBitsRead(bits, 1);
	} else {
		m4vdecBitsRead(bits, 1);		/* the bit after the escape */
		c.last = m4vdecBitsRead(bits, 1);
		c.run = m4vdecBitsRead(bits, 6);
		m4vdecBitsRead(bits, 1);		/* marker_bit */
		c.level = m4vdecBitsRead(bits, 11);
		m4vdecBitsRead(bits, 1);		/* marker_bit */
		c.sign = m4vdecBitsRead(bits, 5);	/* the closing escape and the sign */
	}
	*out = c;
}

#undef RVLC

/*
 * The macroblock layer's other VLC tables (0x8fb1b4-0x8fbd04), as the image
 * lays them out: a motion code below 4 reads the entries before its table's
 */
static const struct {
	M4vdecVlc cbpy[48];
	M4vdecVlc unused1[16];		/* no code reads these two */
	M4vdecVlc unused2[64];
	M4vdecVlc motionFrom512[14];
	M4vdecVlc motionFrom128[96];
	M4vdecVlc motionFrom4[124];
} g_m4vdecVlcTables2 __attribute__((used)) = {
	/* 0x8fb1b4: cbpy (intra), by 6 bits */
	.cbpy = {
		{ VLC_INVALID, 0 }, { VLC_INVALID, 0 }, { 6, 6 }, { 9, 6 },
		{ 8, 5 }, { 8, 5 }, { 4, 5 }, { 4, 5 },
		{ 2, 5 }, { 2, 5 }, { 1, 5 }, { 1, 5 },
		{ 0, 4 }, { 0, 4 }, { 0, 4 }, { 0, 4 },
		{ 12, 4 }, { 12, 4 }, { 12, 4 }, { 12, 4 },
		{ 10, 4 }, { 10, 4 }, { 10, 4 }, { 10, 4 },
		{ 14, 4 }, { 14, 4 }, { 14, 4 }, { 14, 4 },
		{ 5, 4 }, { 5, 4 }, { 5, 4 }, { 5, 4 },
		{ 13, 4 }, { 13, 4 }, { 13, 4 }, { 13, 4 },
		{ 3, 4 }, { 3, 4 }, { 3, 4 }, { 3, 4 },
		{ 11, 4 }, { 11, 4 }, { 11, 4 }, { 11, 4 },
		{ 7, 4 }, { 7, 4 }, { 7, 4 }, { 7, 4 },
	},
	/* 0x8fb334: a 4-bit VLC no code reads */
	.unused1 = {
		{ VLC_INVALID, 0 }, { 0, 4 }, { 1, 3 }, { 1, 3 },
		{ 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 },
		{ 3, 1 }, { 3, 1 }, { 3, 1 }, { 3, 1 },
		{ 3, 1 }, { 3, 1 }, { 3, 1 }, { 3, 1 },
	},
	/* 0x8fb3b4: a 6-bit VLC no code reads */
	.unused2 = {
		{ VLC_INVALID, 0 }, { 1, 6 }, { 2, 5 }, { 2, 5 },
		{ 4, 5 }, { 4, 5 }, { 5, 5 }, { 5, 5 },
		{ 6, 3 }, { 6, 3 }, { 6, 3 }, { 6, 3 },
		{ 6, 3 }, { 6, 3 }, { 6, 3 }, { 6, 3 },
		{ 3, 3 }, { 3, 3 }, { 3, 3 }, { 3, 3 },
		{ 3, 3 }, { 3, 3 }, { 3, 3 }, { 3, 3 },
		{ 0, 3 }, { 0, 3 }, { 0, 3 }, { 0, 3 },
		{ 0, 3 }, { 0, 3 }, { 0, 3 }, { 0, 3 },
		{ 7, 1 }, { 7, 1 }, { 7, 1 }, { 7, 1 },
		{ 7, 1 }, { 7, 1 }, { 7, 1 }, { 7, 1 },
		{ 7, 1 }, { 7, 1 }, { 7, 1 }, { 7, 1 },
		{ 7, 1 }, { 7, 1 }, { 7, 1 }, { 7, 1 },
		{ 7, 1 }, { 7, 1 }, { 7, 1 }, { 7, 1 },
		{ 7, 1 }, { 7, 1 }, { 7, 1 }, { 7, 1 },
		{ 7, 1 }, { 7, 1 }, { 7, 1 }, { 7, 1 },
		{ 7, 1 }, { 7, 1 }, { 7, 1 }, { 7, 1 },
	},
	/* 0x8fb5b4: motion codes from 0x200, by the top 4 of 12 bits */
	.motionFrom512 = {
		{ 3, 4 }, { -3, 4 }, { 2, 3 }, { 2, 3 },
		{ -2, 3 }, { -2, 3 }, { 1, 2 }, { 1, 2 },
		{ 1, 2 }, { 1, 2 }, { VLC_INVALID, 2 }, { VLC_INVALID, 2 },
		{ VLC_INVALID, 2 }, { VLC_INVALID, 2 },
	},
	/* 0x8fb624: from 0x80, by the top 10 */
	.motionFrom128 = {
		{ 12, 10 }, { -12, 10 }, { 11, 10 }, { -11, 10 },
		{ 10, 9 }, { 10, 9 }, { -10, 9 }, { -10, 9 },
		{ 9, 9 }, { 9, 9 }, { -9, 9 }, { -9, 9 },
		{ 8, 9 }, { 8, 9 }, { -8, 9 }, { -8, 9 },
		{ 7, 7 }, { 7, 7 }, { 7, 7 }, { 7, 7 },
		{ 7, 7 }, { 7, 7 }, { 7, 7 }, { 7, 7 },
		{ -7, 7 }, { -7, 7 }, { -7, 7 }, { -7, 7 },
		{ -7, 7 }, { -7, 7 }, { -7, 7 }, { -7, 7 },
		{ 6, 7 }, { 6, 7 }, { 6, 7 }, { 6, 7 },
		{ 6, 7 }, { 6, 7 }, { 6, 7 }, { 6, 7 },
		{ -6, 7 }, { -6, 7 }, { -6, 7 }, { -6, 7 },
		{ -6, 7 }, { -6, 7 }, { -6, 7 }, { -6, 7 },
		{ 5, 7 }, { 5, 7 }, { 5, 7 }, { 5, 7 },
		{ 5, 7 }, { 5, 7 }, { 5, 7 }, { 5, 7 },
		{ -5, 7 }, { -5, 7 }, { -5, 7 }, { -5, 7 },
		{ -5, 7 }, { -5, 7 }, { -5, 7 }, { -5, 7 },
		{ 4, 6 }, { 4, 6 }, { 4, 6 }, { 4, 6 },
		{ 4, 6 }, { 4, 6 }, { 4, 6 }, { 4, 6 },
		{ 4, 6 }, { 4, 6 }, { 4, 6 }, { 4, 6 },
		{ 4, 6 }, { 4, 6 }, { 4, 6 }, { 4, 6 },
		{ -4, 6 }, { -4, 6 }, { -4, 6 }, { -4, 6 },
		{ -4, 6 }, { -4, 6 }, { -4, 6 }, { -4, 6 },
		{ -4, 6 }, { -4, 6 }, { -4, 6 }, { -4, 6 },
		{ -4, 6 }, { -4, 6 }, { -4, 6 }, { -4, 6 },
	},
	/* 0x8fb924: from 4, by all 12 */
	.motionFrom4 = {
		{ 32, 12 }, { -32, 12 }, { 31, 12 }, { -31, 12 },
		{ 30, 11 }, { 30, 11 }, { -30, 11 }, { -30, 11 },
		{ 29, 11 }, { 29, 11 }, { -29, 11 }, { -29, 11 },
		{ 28, 11 }, { 28, 11 }, { -28, 11 }, { -28, 11 },
		{ 27, 11 }, { 27, 11 }, { -27, 11 }, { -27, 11 },
		{ 26, 11 }, { 26, 11 }, { -26, 11 }, { -26, 11 },
		{ 25, 11 }, { 25, 11 }, { -25, 11 }, { -25, 11 },
		{ 24, 10 }, { 24, 10 }, { 24, 10 }, { 24, 10 },
		{ -24, 10 }, { -24, 10 }, { -24, 10 }, { -24, 10 },
		{ 23, 10 }, { 23, 10 }, { 23, 10 }, { 23, 10 },
		{ -23, 10 }, { -23, 10 }, { -23, 10 }, { -23, 10 },
		{ 22, 10 }, { 22, 10 }, { 22, 10 }, { 22, 10 },
		{ -22, 10 }, { -22, 10 }, { -22, 10 }, { -22, 10 },
		{ 21, 10 }, { 21, 10 }, { 21, 10 }, { 21, 10 },
		{ -21, 10 }, { -21, 10 }, { -21, 10 }, { -21, 10 },
		{ 20, 10 }, { 20, 10 }, { 20, 10 }, { 20, 10 },
		{ -20, 10 }, { -20, 10 }, { -20, 10 }, { -20, 10 },
		{ 19, 10 }, { 19, 10 }, { 19, 10 }, { 19, 10 },
		{ -19, 10 }, { -19, 10 }, { -19, 10 }, { -19, 10 },
		{ 18, 10 }, { 18, 10 }, { 18, 10 }, { 18, 10 },
		{ -18, 10 }, { -18, 10 }, { -18, 10 }, { -18, 10 },
		{ 17, 10 }, { 17, 10 }, { 17, 10 }, { 17, 10 },
		{ -17, 10 }, { -17, 10 }, { -17, 10 }, { -17, 10 },
		{ 16, 10 }, { 16, 10 }, { 16, 10 }, { 16, 10 },
		{ -16, 10 }, { -16, 10 }, { -16, 10 }, { -16, 10 },
		{ 15, 10 }, { 15, 10 }, { 15, 10 }, { 15, 10 },
		{ -15, 10 }, { -15, 10 }, { -15, 10 }, { -15, 10 },
		{ 14, 10 }, { 14, 10 }, { 14, 10 }, { 14, 10 },
		{ -14, 10 }, { -14, 10 }, { -14, 10 }, { -14, 10 },
		{ 13, 10 }, { 13, 10 }, { 13, 10 }, { 13, 10 },
		{ -13, 10 }, { -13, 10 }, { -13, 10 }, { -13, 10 },
	},
};

/* the macroblock types as the decoder keeps them (0x845968) */
#define MB_INTRA		0
#define MB_INTER		1
#define MB_INTRA_Q		2
#define MB_INTER_Q		3
#define MB_INTER4V		4
#define MB_STUFFING		7

/* 0x8454de: cbpy, the luma blocks coded: inverted for an inter macroblock */
uint32_t m4vdecCbpy(VideoBitstream *bits, VideoM4vJob *job, uint32_t intra)
{
	uint32_t code = m4vdecBitsPeek(bits, 6);
	uint32_t cbpy;

	if (code < 2)
		m4vdecParserError(NULL, job, 4, ERROR_VLC);
	if (code >= 48) {
		m4vdecBitsRead(bits, 2);
		cbpy = 15;
	} else {
		m4vdecBitsRead(bits, g_m4vdecVlcTables2.cbpy[code].length);
		cbpy = g_m4vdecVlcTables2.cbpy[code].value;
	}
	if (!intra)
		cbpy = 15 - cbpy;
	return cbpy;
}

/*
 * 0x84554a: a block's coefficients, at their zigzag positions; an intra
 * block's from 1, after its DC, unless the DC is a TCOEF too
 */
void m4vdecBlockCoefficients(VideoBitstream *bits, VideoM4vJob *job, int32_t *block,
	uint32_t inter, uint32_t reversible, uint32_t dcAsTcoef, uint32_t shortHeader)
{
	M4vdecCoefficient c;
	int16_t i = 0;

	if (!dcAsTcoef)
		i = inter == 0;
	do {
		if (reversible)
			m4vdecCoefficientReadReversible(&c, bits, inter == 0);
		else
			m4vdecCoefficientRead(&c, bits, job, inter == 0, shortHeader);
		i += c.run;
		if (i >= 64)
			m4vdecParserError(NULL, job, 4, ERROR_VLC);
		block[i] = c.sign == 1 ? -c.level : c.level;
		i++;
	} while (c.last == 0);
}

/* 0x8455e2: a motion vector's code: -32..32 */
int32_t m4vdecMotionCode(VideoBitstream *bits, VideoM4vJob *job)
{
	uint32_t code;
	const M4vdecVlc *vlc;

	if (m4vdecBitsRead(bits, 1))
		return 0;
	code = m4vdecBitsPeek(bits, 12);
	if (code >= 0x200) {
		vlc = &g_m4vdecVlcTables2.motionFrom512[(code >> 8) - 2];
	} else if (code >= 0x80) {
		vlc = &g_m4vdecVlcTables2.motionFrom128[(code >> 2) - 32];
	} else {
		if ((int32_t)(code - 4) < 0)
			m4vdecParserError(NULL, job, 4, ERROR_VLC);
		vlc = &g_m4vdecVlcTables2.motionFrom4[code - 4];
	}
	m4vdecBitsRead(bits, vlc->length);
	return vlc->value;
}

/* a dct_dc_size code of 1 and the zeros before it, size bits in all */
static inline uint32_t dcSizeIs(VideoBitstream *bits, uint32_t length)
{
	return m4vdecBitsPeek(bits, length) == 1;
}

/* 0x845688: dct_dc_size_luminance (blocks 0-3) or dct_dc_size_chrominance */
uint32_t m4vdecDcSize(VideoBitstream *bits, int32_t block)
{
	uint32_t size = 0;
	uint32_t length;
	uint32_t code;

	if (block >= 0 && block < 4) {
		for (length = 11; length >= 4; length--)
			if (dcSizeIs(bits, length)) {
				size = length + 1;
				goto take;
			}
		code = m4vdecBitsPeek(bits, 3);
		if (code == 1) {
			size = 4;
		} else if (code == 2) {
			size = 3;
		} else if (code == 3) {
			size = 0;
		} else {
			code = m4vdecBitsPeek(bits, 2);
			if (code == 2)
				size = 2;
			else if (code == 3)
				size = 1;
			else
				return 0;
			length = 2;
			goto take;
		}
		length = 3;
	} else {
		for (length = 12; length >= 4; length--)
			if (dcSizeIs(bits, length)) {
				size = length;
				goto take;
			}
		length = 3;
		if (dcSizeIs(bits, 3)) {
			size = 3;
		} else {
			size = 3 - m4vdecBitsPeek(bits, 2);
			length = 2;
		}
	}
take:
	m4vdecBitsRead(bits, length);
	return size;
}

/*
 * 0x845836: whether the packet ends at the next byte: stuffing, then a
 * resync marker (resyncBits) or a VOP, GOV or sequence end start code
 */
uint32_t m4vdecPacketEnd(VideoBitstream *bits, uint32_t resyncBits)
{
	if (m4vdecBitsPeekAligned(bits, resyncBits) == 1 && m4vdecBitsValidStuffing(bits))
		return 1;
	if (m4vdecBitsPeekAligned(bits, 32) == 0x1b6 && m4vdecBitsValidStuffing(bits))
		return 1;
	if (m4vdecBitsPeekAligned(bits, 32) == 0x1b3 && m4vdecBitsValidStuffing(bits))
		return 1;
	if (m4vdecBitsPeekAligned(bits, 32) == 1 && m4vdecBitsValidStuffing(bits))
		return 1;
	return 0;
}

/* 0x8458a2: RUN_ENDED at a start code prefix or a resync marker after stuffing */
uint32_t m4vdecMarkerNext(VideoBitstream *bits, uint32_t resyncBits)
{
	if (m4vdecBitsPeekAligned(bits, 23) == 0 && m4vdecBitsValidStuffing(bits))
		return RUN_ENDED;
	if (m4vdecBitsPeekAligned(bits, resyncBits) == 1 && m4vdecBitsValidStuffing(bits))
		return RUN_ENDED;
	return 0;
}

/*
 * 0x8458e6: a short header run's status after its macroblocks: ended at a
 * short video start marker, a start code prefix or the picture's last
 * macroblock; 1 at a GOB's start
 */
uint32_t m4vdecRunStatus(VideoM4vUnit *unit, VideoBitstream *bits, uint32_t gobStart,
	int32_t count)
{
	if (m4vdecBitsPeekAligned(bits, SHORT_VIDEO_START_BITS) == SHORT_VIDEO_START)
		return RUN_ENDED;
	if (gobStart)
		return 1;
	if (m4vdecBitsPeekAligned(bits, 23) == 0)
		return RUN_ENDED;
	if (unit->firstMacroblock + count < unit->macroblocks)
		return 0;
	return RUN_ENDED;
}

/*
 * 0x84593a: whether an intra macroblock's DC is a TCOEF, by
 * intra_dc_vlc_thr: never (0), always (7), else from a quantiser of 13 by 2
 */
uint32_t m4vdecDcAsTcoef(VideoM4vJob *job, uint32_t type, int32_t quant)
{
	uint32_t threshold;

	if (type != MB_INTRA && type != MB_INTRA_Q)
		return 0;
	threshold = job->unit->vop->intraDcVlcThr;
	if (threshold == 0)
		return 0;
	if (threshold == 7)
		return 1;
	return quant >= (int32_t)(threshold * 2 + 11);
}

/* 0x845968: a mcbpc's macroblock type as the decoder keeps it */
uint32_t m4vdecMacroblockType(uint32_t type)
{
	switch (type) {
	case 0:
		return MB_INTER;
	case 1:
		return MB_INTER_Q;
	case 2:
		return MB_INTER4V;
	case 3:
		return MB_INTRA;
	case 4:
		return MB_INTRA_Q;
	case 7:
		return MB_STUFFING;
	default:
		return 0;
	}
}

/* one component of a motion vector: its code and residual by vop_fcode */
static inline int32_t motionComponent(int32_t code, uint32_t residual, uint32_t shift)
{
	int32_t value;

	if ((1 << shift) == 1 || code == 0)
		return code;
	value = (((code < 0 ? -code : code) - 1) << shift) + residual + 1;
	if (code < 0)
		value = -value;
	return value;
}

/* 0x845992: a motion vector's difference, into the row's vectors */
void m4vdecMotionVector(VideoM4vJob *job, VideoBitstream *bits, uint32_t index)
{
	int32_t fcode = job->unit->vop->fcode;
	int32_t horizontal, vertical;
	uint32_t horizontalResidual = 0, verticalResidual = 0;

	horizontal = m4vdecMotionCode(bits, job);
	if (fcode >= 2 && horizontal != 0)
		horizontalResidual = m4vdecBitsRead(bits, fcode - 1);
	vertical = m4vdecMotionCode(bits, job);
	if (fcode >= 2 && vertical != 0)
		verticalResidual = m4vdecBitsRead(bits, fcode - 1);
	horizontal = motionComponent(horizontal, horizontalResidual, fcode - 1);
	vertical = motionComponent(vertical, verticalResidual, fcode - 1);
	job->vectors[index * 2] = horizontal;
	job->vectors[index * 2 + 1] = vertical;
}

/* 0x845a62: an intra DC's differential, after its size */
int32_t m4vdecDcDifferential(VideoBitstream *bits, int32_t block)
{
	uint32_t size = m4vdecDcSize(bits, block);
	uint32_t code;
	int32_t differential;

	if (size == 0)
		return 0;
	code = m4vdecBitsRead(bits, size);
	if ((code >> (size - 1)) == 0)
		differential = -(int32_t)(code ^ ((1u << size) - 1));
	else
		differential = code;
	if (size > 8)
		m4vdecBitsRead(bits, 1);	/* marker_bit */
	return differential;
}

/* 0x8fbd20: the zigzag scan: a raster position's coefficient */
static const int32_t g_m4vdecZigzag[64] = {
	 0,  1,  5,  6, 14, 15, 27, 28,
	 2,  4,  7, 13, 16, 26, 29, 42,
	 3,  8, 12, 17, 25, 30, 41, 43,
	 9, 11, 18, 24, 31, 40, 44, 53,
	10, 19, 23, 32, 39, 45, 52, 54,
	20, 22, 33, 38, 46, 51, 55, 60,
	21, 34, 37, 47, 50, 56, 59, 61,
	35, 36, 48, 49, 57, 58, 62, 63,
};

/* 0x8fbe20: the alternate horizontal scan (AC predicted from above) */
static const int32_t g_m4vdecAlternateHorizontal[64] = {
	 0,  1,  2,  3, 10, 11, 12, 13,
	 4,  5,  8,  9, 17, 16, 15, 14,
	 6,  7, 19, 18, 26, 27, 28, 29,
	20, 21, 24, 25, 30, 31, 32, 33,
	22, 23, 34, 35, 42, 43, 44, 45,
	36, 37, 40, 41, 46, 47, 48, 49,
	38, 39, 50, 51, 56, 57, 58, 59,
	52, 53, 54, 55, 60, 61, 62, 63,
};

/* 0x8fbf20: the alternate vertical scan (from the left) */
static const int32_t g_m4vdecAlternateVertical[64] = {
	 0,  4,  6, 20, 22, 36, 38, 52,
	 1,  5,  7, 21, 23, 37, 39, 53,
	 2,  8, 19, 24, 34, 40, 50, 54,
	 3,  9, 18, 25, 35, 41, 51, 55,
	10, 17, 26, 30, 42, 46, 56, 60,
	11, 16, 27, 31, 43, 47, 57, 61,
	12, 15, 28, 32, 44, 48, 58, 62,
	13, 14, 29, 33, 45, 49, 59, 63,
};


/* the vop_quant changes of dquant */
#define DQUANT_TABLE		{ -1, -2, 1, 2 }

/* a quantiser in 1..31 */
static inline int32_t quantClamp(int32_t quant)
{
	if (quant >= 32)
		return 31;
	if (quant < 1)
		return 1;
	return quant;
}

/* the blocks of a macroblock coded, AC and DC, from its coefficients */
static inline void blockMark(VideoM4vMacroblock *mb, const int32_t *block, uint32_t bit)
{
	int32_t i;

	if (block[0] != 0) {
		mb->blocks |= bit;
		mb->dcCoded |= bit;
	}
	for (i = 1; i < 64; i++)
		if (block[i] != 0) {
			mb->blocks |= bit;
			mb->acCoded |= bit;
			break;
		}
}

/*
 * 0x845ab8: a macroblock's six blocks: each one's DC (read, given, or a
 * TCOEF) and coefficients, its DC predicted, in raster order into the row's
 * luma, Cb or Cr coefficients; then the macroblock's DC predictors kept
 */
void m4vdecBlocks(VideoM4vJob *job, VideoBitstream *bits, uint32_t type, uint32_t intraH263,
	uint8_t coded, int32_t quantDc, uint32_t dcGiven, uint32_t predict, const int16_t *dcs)
{
	VideoM4vMacroblock *mb = job->macroblock;
	VideoM4vUnit *unit = job->unit;
	uint32_t shortHeader = unit->vol->shortHeader;
	int32_t *block = job->block;
	uint32_t intra = type == MB_INTRA || type == MB_INTRA_Q;
	int16_t dc[6];
	const int32_t *scan;
	uint32_t dcAsTcoef;
	uint32_t direction;
	int32_t quant;
	int32_t n, i;

	quant = job->quant + mb->dquant;
	if (quant > 31)
		quant = 31;
	if (quant < 2)
		quant = 1;
	job->quant = quant;
	dcAsTcoef = m4vdecDcAsTcoef(job, type, quantDc);
	for (n = 0; n < 6; n++) {
		uint32_t bit = 1 << (5 - n);
		int16_t **out;
		int16_t *to;

		for (i = 0; i < 64; i++)
			block[i] = 0;
		if (intra) {
			if (!dcAsTcoef || dcGiven) {
				int32_t value;

				if (dcGiven) {
					value = dcs[n];
				} else if (shortHeader) {
					value = m4vdecBitsRead(bits, 8);	/* intra_dc_coefficient */
					if (value == 255)
						value = 128;
				} else {
					value = m4vdecDcDifferential(bits, n);
				}
				block[0] = value;
			}
			if (coded & bit)
				m4vdecBlockCoefficients(bits, job, block, 0, unit->vop->reversibleVlc,
					dcAsTcoef, shortHeader);
			blockMark(mb, block, bit);
			scan = g_m4vdecZigzag;
			if (!intraH263) {
				direction = m4vdecDcPredict(job, block[0], dc, job->quant, n);
				if (mb->acPrediction == 1)
					scan = direction ? g_m4vdecAlternateHorizontal :
						g_m4vdecAlternateVertical;
			}
		} else {
			if (coded & bit)
				m4vdecBlockCoefficients(bits, job, block, 1, unit->vop->reversibleVlc,
					dcAsTcoef, shortHeader);
			blockMark(mb, block, bit);
			scan = g_m4vdecZigzag;
		}
		for (i = 0; i < 64; i++)
			job->scanned[i] = block[scan[i]];

		out = n < 4 ? &job->luma : n == 4 ? &job->cb : &job->cr;
		to = *out;
		if ((mb->acCoded >> (5 - n)) & 1) {
			for (i = 0; i < 64; i++)
				to[i] = job->scanned[i];
			to += 64;
		} else if ((mb->dcCoded >> (5 - n)) & 1) {
			to[0] = job->scanned[0];
			to += 4;
		} else if (intra) {
			to[0] = 0;
			to += 4;
		}
		*out = to;
	}
	if (predict) {
		if (intra)
			m4vdecDcStore(job, dc);
		else
			m4vdecDcSkip(job);
	}
}

/*
 * 0x845d48: an I-VOP's macroblock: mcbpc (past stuffing, up to the
 * packet's end), ac_pred_flag, cbpy and dquant, then its blocks; the DC's
 * VLC by the quantiser before dquant, or after it with currentQuant
 */
void m4vdecMacroblockIntra(VideoM4vJob *job, VideoBitstream *bits, int32_t *quant,
	uint32_t currentQuant)
{
	int32_t dquant[4] = DQUANT_TABLE;
	VideoM4vUnit *unit = job->unit;
	VideoM4vMacroblock *mb = job->macroblock;
	uint32_t shortHeader = unit->vol->shortHeader;
	uint32_t intraH263 = unit->vop->intraH263;
	int32_t quantDc = *quant;
	uint32_t mcbpc, type;
	uint8_t coded = 0;

	mb->acCoded = 0;
	mb->dcCoded = 0;
	mb->blocks = 63;
	mb->acPrediction = 0;
	mb->dquant = 0;
	mb->type = 0;
	do {
		mb->blocks = 63;
		mcbpc = (uint16_t)m4vdecMcbpcIntra(bits, job);
		mb->type = mcbpc & 7;
		type = m4vdecMacroblockType(mcbpc & 7);
		if (type == MB_STUFFING)
			continue;
		if (!shortHeader && (type == MB_INTRA || type == MB_INTRA_Q) && !intraH263)
			mb->acPrediction = m4vdecBitsRead(bits, 1);
		coded = m4vdecCbpy(bits, job, type == MB_INTRA || type == MB_INTRA_Q) << 2 |
			((mcbpc >> 4) & 3);
		mb->blocks = coded;
		if (type == MB_INTER_Q || type == MB_INTRA_Q) {
			int32_t change = dquant[m4vdecBitsRead(bits, 2)];

			mb->dquant = change;
			*quant += change;
			*quant = quantClamp(*quant);
		}
	} while (type == MB_STUFFING && m4vdecPacketEnd(bits, 17) == 0);
	if (currentQuant)
		quantDc = *quant;
	m4vdecBlocks(job, bits, type, intraH263, coded, quantDc, 0, 1, NULL);
}

/*
 * 0x845e88: a P-VOP's macroblock: not_coded, mcbpc, ac_pred_flag, cbpy,
 * dquant and its motion vectors, then its blocks, or the DC predictors
 * reset for one not coded
 */
void m4vdecMacroblockInter(VideoM4vJob *job, VideoBitstream *bits, int32_t *quant,
	uint32_t currentQuant)
{
	int32_t dquant[4] = DQUANT_TABLE;
	VideoM4vUnit *unit = job->unit;
	VideoM4vMacroblock *mb = job->macroblock;
	uint32_t shortHeader = unit->vol->shortHeader;
	uint32_t intraH263 = unit->vop->intraH263;
	int32_t quantDc = *quant;
	uint32_t resyncBits;
	uint32_t notCoded;
	uint32_t mcbpc, type;
	uint8_t coded = 0;
	int32_t i;

	mb->acCoded = 0;
	mb->dcCoded = 0;
	mb->blocks = 0;
	mb->acPrediction = 0;
	mb->dquant = 0;
	mb->type = 0;
	resyncBits = unit->vop->fcode + 16;
	do {
		notCoded = unit->vop->vopCoded ? m4vdecBitsRead(bits, 1) : 1;
		if (notCoded) {
			type = MB_NOT_CODED;
			mb->type = MB_NOT_CODED;
			break;
		}
		mcbpc = (uint16_t)m4vdecMcbpcInter(bits, job);
		mb->type = mcbpc & 7;
		type = m4vdecMacroblockType(mcbpc & 7);
		if (type == MB_STUFFING)
			continue;
		if (!shortHeader && (type == MB_INTRA || type == MB_INTRA_Q) && !intraH263)
			mb->acPrediction = m4vdecBitsRead(bits, 1);
		coded = m4vdecCbpy(bits, job, type == MB_INTRA || type == MB_INTRA_Q) << 2 |
			((mcbpc >> 4) & 3);
		mb->blocks = coded;
		if (type == MB_INTER_Q || type == MB_INTRA_Q || type == 6) {
			int32_t change = dquant[m4vdecBitsRead(bits, 2)];

			mb->dquant = change;
			*quant += change;
			*quant = quantClamp(*quant);
		}
	} while (type == MB_STUFFING && m4vdecPacketEnd(bits, resyncBits) == 0);

	if (type == MB_INTER || type == MB_INTER_Q) {
		m4vdecMotionVector(job, bits, 0);
		job->vectors += 2;
	} else if (type == MB_INTER4V) {
		for (i = 0; i < 4; i++)
			m4vdecMotionVector(job, bits, i);
		job->vectors += 8;
	}
	if (notCoded) {
		m4vdecDcSkip(job);
		return;
	}
	if (currentQuant)
		quantDc = *quant;
	m4vdecBlocks(job, bits, type, intraH263, coded, quantDc, 0, 1, NULL);
}

/* 0x846044: whether a GOB's start code is next */
uint32_t m4vdecGobStartNext(VideoM4vJob *job)
{
	return m4vdecBitsPeek(job->bits, GOB_START_BITS) == 1;
}

#define DC_MARKER		0x6b001	/* dc_marker */
#define DC_MARKER_BITS		19

/* a data partitioned run's partition from where the run before it left it */
static inline VideoBitstream *partitionResume(VideoM4vUnit *unit, int32_t partition,
	VideoM4vBits *local)
{
	local->bits = unit->vop->partitionBits[partition].bits;
	local->after = unit->vop->partitionBits[partition].after;
	m4vdecBitsRestore(&unit->vop->partitionRegisters[partition]);
	return &local->bits;
}

/* and where this run leaves it (the reader is the unit's, with the word after it) */
static inline void partitionSave(VideoM4vUnit *unit, int32_t partition, VideoBitstream *bits)
{
	const VideoM4vBits *from = (const VideoM4vBits *)bits;

	unit->vop->partitionBits[partition].bits = from->bits;
	unit->vop->partitionBits[partition].after = from->after;
	m4vdecBitsSave(&unit->vop->partitionRegisters[partition]);
}

/*
 * 0x84605e: a data partitioned I-VOP packet's run, in its three passes:
 * mcbpc, dquant and the DCs up to the DC marker; ac_pred_flag and cbpy;
 * the blocks. The packet's macroblocks past the run are read into the
 * job's own records, and each partition's reader saved where the run
 * ends. Returns the run's macroblocks.
 */
int32_t m4vdecPacketIntra(VideoM4vJob *job, const VideoHelperJob *run)
{
	int32_t dquant[4] = DQUANT_TABLE;
	VideoBitstream *bits = job->bits;
	VideoM4vUnit *unit = job->unit;
	int32_t stride = unit->macroblocks;
	uint8_t *types = (uint8_t *)unit->partitionData;
	uint32_t intraH263 = unit->vop->intraH263;
	uint8_t *cbps = types + stride;
	int16_t *quants = (int16_t *)(cbps + stride);
	int16_t *dcs = quants + stride;
	int32_t column = unit->runOffset;
	int32_t width = unit->widthInMacroblocks;
	int32_t runMacroblocks = unit->runMacroblocks;
	VideoM4vMacroblock *first;
	VideoM4vBits local;
	int32_t pastRun;
	int32_t quant, quantDc;
	int32_t count = 0, row = 0, i, n;

	pastRun = unit->vop->pastRun;
	unit->vop->pastRun = 0;
	if (pastRun)
		bits = partitionResume(unit, 0, &local);
	quant = unit->load ? unit->vop->quant : unit->vop->runQuant;
	first = job->macroblock;

	/* the first partition */
	do {
		uint32_t mcbpc = (uint16_t)m4vdecMcbpcIntra(bits, job);
		uint32_t mcbpcType = mcbpc & 7;
		uint32_t type = m4vdecMacroblockType(mcbpcType);
		VideoM4vMacroblock *mb;
		uint32_t dcAsTcoef;

		if (type == MB_STUFFING)
			continue;
		mb = job->macroblock;
		quantDc = quant;
		mb->acCoded = 0;
		mb->dcCoded = 0;
		mb->acPrediction = 0;
		mb->type = mcbpcType;
		mb->dquant = 0;
		types[count] = type;
		cbps[count] = (mcbpc >> 4) & 3;
		if (type == MB_INTRA_Q) {
			int32_t change = dquant[m4vdecBitsRead(bits, 2)];

			mb->dquant = change;
			quant = max32(min32(quant + change, 31), 1);
		}
		if (column == 0 && row == 0)
			quantDc = quant;
		quants[count] = quantDc;
		dcAsTcoef = m4vdecDcAsTcoef(job, type, quantDc);
		if (type == MB_INTRA || type == MB_INTRA_Q) {
			for (n = 0; n < 6; n++) {
				int32_t value;

				if (dcAsTcoef) {
					dcs[count * 6 + n] = 0;
					continue;
				}
				if (intraH263) {
					value = m4vdecBitsRead(bits, 8);
					if (value == 128)
						m4vdecParserError(NULL, job, 4, ERROR_VLC);
					if (value == 255)
						value = 128;
				} else {
					value = m4vdecDcDifferential(bits, n);
				}
				if (count < runMacroblocks)
					dcs[count * 6 + n] = value;
			}
		}
		if (count == runMacroblocks - 1)
			partitionSave(unit, 0, bits);
		if (count < runMacroblocks) {
			if (column == width - 1) {
				m4vdecRowBuffers(job, run, ++row);
				column = 0;
			} else {
				job->macroblock++;
				column++;
			}
		} else {
			if (column == width - 1) {
				column = 0;
				row++;
			} else {
				column++;
			}
			unit->vop->pastRun++;
			job->macroblock = &job->pastRunMacroblock;
		}
		count++;
	} while (m4vdecBitsPeek(bits, DC_MARKER_BITS) != DC_MARKER);
	m4vdecBitsRead(bits, DC_MARKER_BITS);

	/* the second */
	if (pastRun)
		bits = partitionResume(unit, 1, &local);
	job->macroblock = first;
	column = unit->runOffset;
	m4vdecRowBuffers(job, run, 0);
	row = 0;
	for (i = 0; i < count; i++) {
		uint32_t type = types[i];
		uint32_t intra = type == MB_INTRA || type == MB_INTRA_Q;
		VideoM4vMacroblock *mb = job->macroblock;
		uint8_t coded;

		if (intra && !intraH263)
			mb->acPrediction = m4vdecBitsRead(bits, 1);
		coded = m4vdecCbpy(bits, job, intra) << 2 | cbps[i];
		mb->blocks = coded;
		cbps[i] = coded;
		if (i == runMacroblocks - 1)
			partitionSave(unit, 1, bits);
		if (i < runMacroblocks) {
			if (width - 1 == column) {
				m4vdecRowBuffers(job, run, ++row);
				column = 0;
			} else {
				job->macroblock++;
				column++;
			}
		} else {
			job->macroblock = &job->pastRunMacroblock;
		}
	}

	/* the third */
	if (pastRun)
		bits = partitionResume(unit, 2, &local);
	job->macroblock = first;
	column = unit->runOffset;
	m4vdecRowBuffers(job, run, 0);
	row = 0;
	for (i = 0; i < count; i++) {
		m4vdecBlocks(job, bits, types[i], intraH263, cbps[i], (uint16_t)quants[i], 1,
			i < runMacroblocks, dcs + min32(runMacroblocks, i) * 6);
		if (i == runMacroblocks - 1)
			partitionSave(unit, 2, bits);
		if (i < runMacroblocks) {
			if (column == width - 1) {
				m4vdecRowBuffers(job, run, ++row);
				m4vdecDcRowStart(job, 0);
				column = 0;
			} else {
				job->macroblock++;
				column++;
			}
		} else {
			job->macroblock = &job->pastRunMacroblock;
			job->luma = job->pastRunLuma;
			job->cb = job->pastRunCb;
			job->cr = job->pastRunCr;
		}
	}
	return count - unit->vop->pastRun;
}

/* whether a resync marker or a start code prefix follows the stuffing */
static inline uint32_t markerAhead(VideoBitstream *bits, uint32_t resyncBits)
{
	if (m4vdecBitsPeekAligned(bits, 23) == 0 && m4vdecBitsValidStuffing(bits))
		return 1;
	if (m4vdecBitsPeekAligned(bits, resyncBits) == 1 && m4vdecBitsValidStuffing(bits))
		return 1;
	return 0;
}

/*
 * 0x8464ee: an I-VOP run decoded here: data partitioned, a short header's
 * GOBs or packets of macroblocks; the unit's status where it stops.
 * Returns the run's macroblocks not decoded.
 */
int32_t m4vdecRunIntra(VideoM4vJob *job, const VideoHelperJob *run)
{
	VideoM4vUnit *unit = job->unit;
	VideoBitstream *bits = job->bits;
	int32_t column = unit->runOffset;
	int32_t width = unit->widthInMacroblocks;
	int32_t runMacroblocks = unit->runMacroblocks;
	int32_t quant = unit->load ? unit->vop->quant : unit->vop->runQuant;
	int32_t count, row, n;
	int32_t error;
	uint32_t status;

	error = videoSetJump(job->jump);
	count = 0;
	if (error) {
		unit->error = error;
		unit->doneMacroblocks = unit->runMacroblocks;
		status = VIDEO_UNIT_ENDED;
	} else if (unit->vol->dataPartitioned) {
		m4vdecRowBuffers(job, run, 0);
		m4vdecDcRowStart(job, column);
		count = m4vdecPacketIntra(job, run);
		if (unit->firstMacroblock + count >= unit->macroblocks) {
			status = RUN_ENDED;
			count = runMacroblocks;
		} else if (unit->vop->pastRun == 0 && m4vdecPacketEnd(bits, 17)) {
			status = m4vdecMarkerNext(bits, 17);
		} else if (unit->vop->pastRun != 0) {
			status = 0;
		} else {
			status = RUN_ENDED;
		}
	} else if (unit->vol->shortHeader) {
		n = min32(unit->vop->runEnd, runMacroblocks);
		row = 0;
		while (count < n) {
			m4vdecRowBuffers(job, run, row);
			m4vdecDcRowStart(job, column);
			while (column < width) {
				count++;
				m4vdecMacroblockIntra(job, bits, &quant,
					column == 0 && unit->runRow + row == 0);
				column++;
				job->macroblock++;
			}
			row++;
			column = 0;
		}
		if (m4vdecBitsPeek(bits, GOB_START_BITS) != 1 &&
			m4vdecBitsPeekAligned(bits, GOB_START_BITS) == 1)
			m4vdecBitsByteAlign(bits);
		n = m4vdecGobStartNext(job);
		unit->vop->runQuant = quant;
		status = m4vdecRunStatus(unit, bits, n, count);
	} else {
		row = 0;
		for (;;) {
			m4vdecRowBuffers(job, run, row);
			m4vdecDcRowStart(job, column);
			for (; column < width; column++, job->macroblock++) {
				count++;
				m4vdecMacroblockIntra(job, bits, &quant,
					column == 0 && unit->runRow + row == 0);
				unit->vop->runQuant = quant;
				if (count >= runMacroblocks) {
					if (m4vdecPacketEnd(bits, 17))
						status = m4vdecMarkerNext(bits, 17);
					else if (unit->firstMacroblock + count < unit->macroblocks)
						status = 0;
					else
						status = RUN_ENDED;
					goto done;
				}
				if (markerAhead(bits, 17)) {
					status = 1;
					goto done;
				}
			}
			column = 0;
			row++;
		}
	}
done:
	unit->doneStatus = status;
	return runMacroblocks - count;
}

#define MOTION_MARKER		0x1f001	/* motion_marker */
#define MOTION_MARKER_BITS	17

/*
 * 0x8466fe: a data partitioned P-VOP packet's run: not_coded, mcbpc and
 * the motion vectors up to the motion marker; ac_pred_flag, cbpy, dquant
 * and the DCs; the blocks. As 0x84605e for the macroblocks past the run.
 */
int32_t m4vdecPacketInter(VideoM4vJob *job, const VideoHelperJob *run)
{
	int32_t dquant[4] = DQUANT_TABLE;
	VideoBitstream *bits = job->bits;
	VideoM4vUnit *unit = job->unit;
	uint8_t *types = (uint8_t *)unit->partitionData;
	int32_t stride = unit->macroblocks;
	uint8_t *cbps = types + stride;
	int16_t *quants = (int16_t *)(cbps + stride);
	int16_t *dcs = quants + stride;
	int32_t column = unit->runOffset;
	int32_t width = unit->widthInMacroblocks;
	int32_t runMacroblocks = unit->runMacroblocks;
	VideoM4vMacroblock *first;
	uint32_t intraH263;
	VideoM4vBits local;
	int32_t pastRun;
	int32_t quant, quantDc;
	int32_t count = 0, row = 0, i, n;

	pastRun = unit->vop->pastRun;
	unit->vop->pastRun = 0;
	if (pastRun)
		bits = partitionResume(unit, 0, &local);
	first = job->macroblock;
	intraH263 = unit->vop->intraH263;
	quant = unit->load ? unit->vop->quant : unit->vop->runQuant;

	/* the first partition */
	do {
		uint32_t mcbpcType, type, cbpc;
		VideoM4vMacroblock *mb;

		if (m4vdecBitsRead(bits, 1)) {
			cbpc = 0;
			mcbpcType = type = MB_NOT_CODED;
		} else {
			uint32_t mcbpc = (uint16_t)m4vdecMcbpcInter(bits, job);

			mcbpcType = mcbpc & 7;
			type = m4vdecMacroblockType(mcbpcType);
			if (type == MB_STUFFING)
				continue;
			cbpc = (mcbpc >> 4) & 3;
		}
		mb = job->macroblock;
		mb->dquant = 0;
		mb->acPrediction = 0;
		mb->dcCoded = 0;
		mb->acCoded = 0;
		mb->type = mcbpcType;
		mb->blocks = cbpc;
		types[count] = type;
		cbps[count] = cbpc;
		if (type == MB_INTER4V) {
			for (n = 0; n < 4; n++)
				m4vdecMotionVector(job, bits, n);
			job->vectors += 8;
		} else if (type == MB_INTER || type == MB_INTER_Q) {
			m4vdecMotionVector(job, bits, 0);
			job->vectors += 2;
		}
		if (runMacroblocks - 1 == count)
			partitionSave(unit, 0, bits);
		if (count < runMacroblocks) {
			if (width - 1 == column) {
				m4vdecRowBuffers(job, run, ++row);
				column = 0;
			} else {
				job->macroblock++;
				column++;
			}
		} else {
			unit->vop->pastRun++;
			job->macroblock = &job->pastRunMacroblock;
		}
		count++;
	} while (m4vdecBitsPeek(bits, MOTION_MARKER_BITS) != MOTION_MARKER);
	m4vdecBitsRead(bits, MOTION_MARKER_BITS);

	/* the second */
	if (pastRun)
		bits = partitionResume(unit, 1, &local);
	job->macroblock = first;
	column = unit->runOffset;
	m4vdecRowBuffers(job, run, 0);
	row = 0;
	for (i = 0; i < count; i++) {
		VideoM4vMacroblock *mb = job->macroblock;
		uint32_t type = types[i];

		quantDc = quant;
		if (type != MB_NOT_CODED) {
			uint32_t intra = type == MB_INTRA || type == MB_INTRA_Q;
			uint32_t dcAsTcoef;
			uint8_t coded;

			if (intra && !intraH263)
				mb->acPrediction = m4vdecBitsRead(bits, 1);
			coded = m4vdecCbpy(bits, job, intra) << 2 | cbps[i];
			mb->blocks = coded;
			cbps[i] = coded;
			if (type == MB_INTER_Q || type == MB_INTRA_Q || type == 6) {
				int32_t change = dquant[m4vdecBitsRead(bits, 2)];

				mb->dquant = change;
				quant = max32(min32(quant + change, 31), 1);
			}
			if (column == 0 && row == 0)
				quantDc = quant;
			quants[i] = quantDc;
			dcAsTcoef = m4vdecDcAsTcoef(job, type, quantDc);
			if (intra) {
				for (n = 0; n < 6; n++) {
					int32_t value;

					if (dcAsTcoef) {
						dcs[i * 6 + n] = 0;
						continue;
					}
					if (intraH263) {
						value = m4vdecBitsRead(bits, 8);
						if (value == 128)
							m4vdecParserError(NULL, job, 4, ERROR_VLC);
						if (value == 255)
							value = 128;
					} else {
						value = m4vdecDcDifferential(bits, n);
					}
					if (i < runMacroblocks)
						dcs[i * 6 + n] = value;
				}
			}
		}
		if (runMacroblocks - 1 == i)
			partitionSave(unit, 1, bits);
		if (i < runMacroblocks) {
			if (width - 1 == column) {
				m4vdecRowBuffers(job, run, ++row);
				column = 0;
			} else {
				job->macroblock++;
				column++;
			}
		} else {
			if (width - 1 == column) {
				column = 0;
				row++;
			} else {
				column++;
			}
			job->macroblock = &job->pastRunMacroblock;
			job->vectors = job->pastRunVectors;
		}
	}

	/* the third */
	if (pastRun)
		bits = partitionResume(unit, 2, &local);
	job->macroblock = first;
	column = unit->runOffset;
	m4vdecRowBuffers(job, run, 0);
	row = 0;
	for (i = 0; i < count; i++) {
		uint32_t type = types[i];

		if (type != MB_NOT_CODED)
			m4vdecBlocks(job, bits, type, intraH263, cbps[i], (uint16_t)quants[i], 1,
				i < runMacroblocks, dcs + min32(runMacroblocks, i) * 6);
		else if (i < runMacroblocks)
			m4vdecDcSkip(job);
		if (runMacroblocks - 1 == i)
			partitionSave(unit, 2, bits);
		if (i < runMacroblocks) {
			if (width - 1 == column) {
				m4vdecRowBuffers(job, run, ++row);
				m4vdecDcRowStart(job, 0);
				column = 0;
			} else {
				job->macroblock++;
				column++;
			}
		} else {
			job->macroblock = &job->pastRunMacroblock;
			job->luma = job->pastRunLuma;
			job->cb = job->pastRunCb;
			job->cr = job->pastRunCr;
		}
	}
	return count - unit->vop->pastRun;
}

/*
 * 0x846c1c: a P-VOP run decoded here, as 0x8464ee; a VOP not coded stops
 * at the picture's last row
 */
int32_t m4vdecRunInter(VideoM4vJob *job, const VideoHelperJob *run)
{
	VideoM4vUnit *unit = job->unit;
	int32_t column = unit->runOffset;
	int32_t width = unit->widthInMacroblocks;
	uint32_t resyncBits = unit->vop->fcode + 16;
	VideoBitstream *bits = job->bits;
	int32_t runMacroblocks = unit->runMacroblocks;
	int32_t quant = unit->load ? unit->vop->quant : unit->vop->runQuant;
	int32_t count, row, n;
	int32_t error;
	uint32_t status;

	error = videoSetJump(job->jump);
	count = 0;
	if (error) {
		unit->error = error;
		unit->doneMacroblocks = unit->runMacroblocks;
		status = VIDEO_UNIT_ENDED;
	} else if (unit->vop->vopCoded && unit->vol->dataPartitioned) {
		m4vdecRowBuffers(job, run, 0);
		m4vdecDcRowStart(job, column);
		count = m4vdecPacketInter(job, run);
		if (unit->firstMacroblock + count >= unit->macroblocks) {
			status = RUN_ENDED;
			count = runMacroblocks;
		} else if (unit->vop->pastRun == 0 && m4vdecPacketEnd(bits, resyncBits)) {
			status = m4vdecMarkerNext(bits, resyncBits);
		} else if (unit->vop->pastRun != 0) {
			status = 0;
		} else {
			status = RUN_ENDED;
		}
	} else if (unit->vol->shortHeader) {
		n = min32(unit->vop->runEnd, runMacroblocks);
		row = 0;
		while (count < n) {
			m4vdecRowBuffers(job, run, row);
			m4vdecDcRowStart(job, column);
			while (column < width) {
				count++;
				m4vdecMacroblockInter(job, bits, &quant,
					column == 0 && unit->runRow + row == 0);
				column++;
				job->macroblock++;
			}
			row++;
			column = 0;
		}
		if (m4vdecBitsPeek(bits, GOB_START_BITS) != 1 &&
			m4vdecBitsPeekAligned(bits, GOB_START_BITS) == 1)
			m4vdecBitsByteAlign(bits);
		n = m4vdecGobStartNext(job);
		unit->vop->runQuant = quant;
		status = m4vdecRunStatus(unit, bits, n, count);
	} else {
		row = 0;
		for (;;) {
			m4vdecRowBuffers(job, run, row);
			m4vdecDcRowStart(job, column);
			for (; column < width; column++, job->macroblock++) {
				count++;
				m4vdecMacroblockInter(job, bits, &quant,
					column == 0 && unit->runRow + row == 0);
				unit->vop->runQuant = quant;
				if (count >= runMacroblocks) {
					if (unit->vop->vopCoded == 0)
						status = unit->runRow + row == unit->heightInMacroblocks - 1 ?
							RUN_ENDED : 0;
					else if (m4vdecPacketEnd(bits, resyncBits))
						status = m4vdecMarkerNext(bits, resyncBits);
					else if (unit->firstMacroblock + count < unit->macroblocks)
						status = 0;
					else
						status = RUN_ENDED;
					goto done;
				}
				if (m4vdecBitsPeekAligned(bits, resyncBits) == 1 &&
					m4vdecBitsValidStuffing(bits)) {
					status = 1;
					goto done;
				}
			}
			column = 0;
			row++;
		}
	}
done:
	unit->doneStatus = status;
	return runMacroblocks - count;
}

/*
 * 0x846e4c: a VOP not coded (vop_coded 0): the run's macroblocks marked
 * not coded. Returns the run's macroblocks not marked (none).
 */
int32_t m4vdecVopNotCoded(VideoM4vJob *job, const VideoHelperJob *run)
{
	VideoM4vUnit *unit = job->unit;
	int32_t runMacroblocks = unit->runMacroblocks;
	int32_t column = unit->runOffset;
	int32_t width = unit->widthInMacroblocks;
	int32_t done = 0;
	int32_t row = 0;

	for (;;) {
		m4vdecRowBuffers(job, run, row);
		for (; column < width; column++, job->macroblock++) {
			job->macroblock->blocks = 0;
			job->macroblock->dcCoded = 0;
			job->macroblock->acCoded = 0;
			job->macroblock->type = MB_NOT_CODED;
			if (++done >= runMacroblocks) {
				unit->doneStatus = unit->runRow + row == unit->heightInMacroblocks - 1 ?
					RUN_ENDED : 0;
				return runMacroblocks - done;
			}
		}
		column = 0;
		row++;
	}
}

/* 0x846ef4: a VOP's or packet's first row: every DC predictor reset */
void m4vdecDcReset(VideoM4vJob *job)
{
	int32_t count = job->unit->widthInMacroblocks * 4;
	int32_t i;

	for (i = 0; i < count; i++)
		job->dcRow[i] = DC_RESET;
	for (i = 6; i >= 0; i--)
		job->dc[i] = DC_RESET;
}

/* 0x846f38: a row from a column on: the predictors above, and the left's reset */
void m4vdecDcRowStart(VideoM4vJob *job, int32_t column)
{
	int32_t i;

	for (i = 0; i < 7; i++)
		job->dc[i] = DC_RESET;
	job->dcRowAt = job->dcRow + column * 4;
}

/* 0x84711a: a predictor by the DC scaler, rounded to the nearest */
int32_t m4vdecDcDivide(int32_t value, int32_t scaler)
{
	if (scaler == 0)
		return value;
	if (value < 0)
		return -((-value * 2 + scaler) / (scaler * 2));
	return (value * 2 + scaler) / (scaler * 2);
}

/* 0x84713c: the luma's dc_scaler of a quantiser */
uint8_t m4vdecDcScalerLuma(int32_t quant)
{
	if (quant >= 25)
		return quant * 2 - 16;
	if (quant >= 9)
		return quant + 8;
	if (quant >= 5)
		return quant * 2;
	return 8;
}

/* 0x847162: the chroma's */
uint8_t m4vdecDcScalerChroma(int32_t quant)
{
	if (quant >= 25)
		return quant - 6;
	if (quant >= 5)
		return (quant + 13) >> 1;
	return 8;
}

/* a DC in -2048..2047 */
static inline int32_t dcClamp(int32_t value)
{
	if (value >= 2047)
		return 2047;
	if (value < -2047)
		return -2048;
	return value;
}

/*
 * 0x846f66: an intra block's DC from its differential and the prediction
 * of its left (A), above left (B) and above (C) blocks: from C when
 * |A - B| < |B - C| (returns 1), else from A; dcs holds the macroblock's
 * blocks' DCs, scaled
 */
uint32_t m4vdecDcPredict(VideoM4vJob *job, int16_t differential, int16_t *dcs, int32_t quant,
	int32_t block)
{
	const int16_t *above = job->dcRowAt;
	uint32_t scaler = block < 4 ? m4vdecDcScalerLuma(quant) : m4vdecDcScalerChroma(quant);
	int32_t a, b, c;
	int32_t prediction;
	uint32_t fromAbove;
	int32_t value;

	c = dcs[0];
	switch (block) {
	case 0:
		c = above[0];
		b = job->dc[0];
		a = job->dc[1];
		break;
	case 1:
		a = c;
		b = above[0];
		c = above[1];
		break;
	case 2:
		a = job->dc[2];
		b = job->dc[1];
		break;
	case 3:
		a = dcs[2];
		b = c;
		c = dcs[1];
		break;
	case 4:
		a = job->dc[4];
		b = job->dc[3];
		c = above[2];
		break;
	default:
		a = job->dc[6];
		b = job->dc[5];
		c = above[3];
		break;
	}
	if (abs32(a - b) < abs32(b - c)) {
		fromAbove = 1;
		prediction = c;
	} else {
		fromAbove = 0;
		prediction = a;
	}
	value = dcClamp(differential + (int16_t)m4vdecDcDivide(prediction, scaler));
	dcs[block] = dcClamp((int16_t)value * (int32_t)scaler);
	return fromAbove;
}

/*
 * 0x84704c: an intra macroblock's DCs kept: on the left for the next
 * macroblock (with the ones above it), and above for the next row
 */
void m4vdecDcStore(VideoM4vJob *job, const int16_t *dcs)
{
	int16_t *above = job->dcRowAt;

	job->dc[0] = above[1];
	job->dc[1] = dcs[1];
	job->dc[2] = dcs[3];
	job->dc[3] = above[2];
	job->dc[4] = dcs[4];
	job->dc[5] = above[3];
	job->dc[6] = dcs[5];
	job->dcRowAt[0] = dcs[2];
	job->dcRowAt[1] = dcs[3];
	job->dcRowAt[2] = dcs[4];
	job->dcRowAt[3] = dcs[5];
	job->dcRowAt += 4;
}

/* 0x8470be: the same for a macroblock that is not intra: its DCs reset */
void m4vdecDcSkip(VideoM4vJob *job)
{
	int16_t *above = job->dcRowAt;

	job->dc[0] = above[1];
	job->dc[2] = DC_RESET;
	job->dc[1] = DC_RESET;
	job->dc[4] = DC_RESET;
	job->dc[3] = above[2];
	job->dc[6] = DC_RESET;
	job->dc[5] = above[3];
	job->dcRowAt[0] = DC_RESET;
	job->dcRowAt[1] = DC_RESET;
	job->dcRowAt[2] = DC_RESET;
	job->dcRowAt[3] = DC_RESET;
	job->dcRowAt += 4;
}

/*
 * 0x847184: an error at a stage of the parse (1 the sequence, 2 the picture, 3 a packet, 4 a
 * macroblock). The unit keeps the code; a macroblock's marks the unit failed. A code from
 * ERROR_FATAL on ends the job: back to where the job set its jump.
 */
void m4vdecParserError(const VideoM4vStream *stream, VideoM4vJob *job, uint32_t stage,
	int32_t code)
{
	VideoM4vUnit *unit = job->unit;

	(void)stream;
	if (stage == 4) {
		unit->failed = 1;
		unit->error = code;
	}
	if (code < ERROR_FATAL) {
		unit->error = code;
		return;
	}
	videoLongJump(job->jump, code);
}
