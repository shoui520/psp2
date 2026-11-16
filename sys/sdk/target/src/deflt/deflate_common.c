#include "deflate_i.h"

/* Length descriptors followed by 32 packed distance descriptors. */
static const uint16_t s_fixhuffcodes[48] = {
	0x0c00, 0x0803, 0x0804, 0x0805, 0x0806, 0x0807, 0x0808, 0x0809,
	0x080a, 0x0c01, 0x0d01, 0x0e01, 0x0f01, 0x0c02, 0x0d02, 0x0e02,
	0x0f02, 0x0c03, 0x0d03, 0x0e03, 0x0f03, 0x0c04, 0x0d04, 0x0e04,
	0x0f04, 0x0c05, 0x0d05, 0x0e05, 0x0f05, 0x0902, 0x0d00, 0x0d00,
	0x7978, 0x7b7a, 0x7776, 0x7372, 0x6f6e, 0x6b6a, 0x6766, 0x6362,
	0x5f5e, 0x5b5a, 0x5756, 0x5352, 0x4f4e, 0x4b4a, 0x4746, 0x0000
};

/* Repeat symbols use the decoder's packed descriptors, 19 and 23. */
const uint8_t _sceDeflateLengthOrder[19] = {
	16, 19, 23, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

/*
 * The code-length code's fast table: flags[1..7] hold one bit per symbol of each length 7..1
 * (the top bit first); every symbol fills the entries of its bit-reversed code. A shift by 32
 * (the last bit of a word) is the ARM register shift's, which gives 0.
 */
unsigned int _sceDeflateMakeHuffmanTableLength(deflateInternalData_t *pW)
{
	signed char *tbl = pW->sym.len;
	unsigned int *p0 = &pW->u.s.flags[1];
	unsigned int code = 0;
	unsigned int k = 6;
	int ii;
	int j;

	do {
		unsigned int flags = *p0++;

		if (flags) {
			ii = 128 >> k;
			j = ~k;
			do {
				int jj;
				unsigned int count = __clz(flags) + 1;

				jj = __rbit(code) >> 25;
				flags <<= count;
				j += count * 8;
				do {
					tbl[jj] = j;
					jj += ii;
				} while ((unsigned int)jj <= 127);
				code += 1 << k;
			} while (flags);
			p0[-1] = flags;
		}
	} while (k--);
	return code;
}

/*
 * The distance code: flags[1..15] hold one bit per distance symbol of each length (the top bit
 * first). Sorts the symbols into distsym, records each length's limit, and returns the
 * code space left over (0 when the code is complete, or has at most one symbol).
 */
static inline int makeHuffmanTableDistance(deflateInternalData_t *pW)
{
	unsigned int *p0 = &pW->u.s.flags[0];
	unsigned int pos = 0;
	unsigned int code = 0;
	unsigned int i;

	for (i = 1; i < 16; i++) {
		/* the first symbol of this length less its first code: 0 for the shortest */
		unsigned int lendiff = pos - (code >> (15 - i));
		unsigned int flags = *++p0;

		while (flags) {
			int j = __clz(flags);

			flags -= 0x80000000u >> j;
			pW->distsym[pos++] = ((const unsigned char *)(s_fixhuffcodes + 32))[j];
			code += 1 << (15 - i);
		}
		if (i > 5) {
			pW->distmaxinfo[i - 6] = (code << 17) +
				((lendiff + DEFLATE_DISTANCE_SYMBOL_BASE) | 0xffff0000u);
		} else {
			if (!lendiff)
				pW->v.w.distminlen = i;
			pW->distmaxcode[i - 1] = code >> 10;
		}
	}
	if (pos <= 1) {
		pW->distmaxcode[0] = 32;
		pW->v.w.distminlen = pos;
		pW->distsym[pos] = 0;
		if (!pos)
			return 0;
		code <<= 1;
	}
	return code - 0x8000;
}

/* The literal/length fast table, from the sorted symbols, lengths hlen..8; then the longer codes */
static inline void makeFastDecodeTable(deflateInternalData_t *pW, unsigned int hlen)
{
	unsigned int code = 0;
	unsigned short *pSym = pW->sym.lit;

	for (;;) {
		unsigned int val = *pSym++;
		unsigned int end = code + (256u >> hlen);
		unsigned int exbits;

		if ((val & 0x400) && (exbits = val & 7) != 0 && exbits + hlen <= 8) {
			/* a length whose extra bits fit: one entry per value of them */
			val = (((val >> 8) & 7) << exbits) + ((exbits + hlen) << 12) + 0x803;
		} else {
			val += hlen << 12;
			exbits = 0;
		}
		do {
			unsigned int revcode = __rbit(code++ << 24);

			pW->u.ft[revcode] = ((revcode >> hlen) & ((1 << exbits) - 1)) + val;
		} while (code != end);
		while (end > pW->v.w.litmaxcode[hlen - 1]) {
			if (++hlen > 8) {
				code = end;
				goto longer8;
			}
		}
		code = end;
	}
longer8:
	code <<= 24;
	hlen = 24;
	while (code) {
		while (code >= pW->litmaxinfo[hlen - 18])
			hlen--;
		pW->u.ft[__rbit(code)] = hlen;
		code += 0x01000000;
	}
}

/*
 * The distance fast table, lengths distminlen..5; then the longer codes. With no distance codes
 * (distminlen 0) the limit read is the byte in front of distmaxcode, distmaxinfo[9]'s top byte.
 */
static inline void makeFastDecodeDistance(deflateInternalData_t *pW)
{
	unsigned int code = 0;
	unsigned char *pSym = pW->distsym;
	int hlen = pW->v.w.distminlen;

	for (;;) {
		int val = *pSym++ + (hlen << 12);
		unsigned int end = code + (32u >> hlen);

		do {
			unsigned int revcode = __rbit(code++) >> 27;

			pW->v.ftdist[revcode] = val;
		} while (code != end);
		while (end >= pW->distmaxcode[hlen - 1]) {
			if (++hlen > 5) {
				code = end;
				goto longer5;
			}
		}
		code = end;
	}
longer5:
	code <<= 27;
	while (code) {
		while (code >= pW->distmaxinfo[hlen - 6])
			hlen++;
		pW->v.ftdist[__rbit(code)] = (hlen << 12) - 0x800;
		code += 0x08000000;
	}
}

int _sceDeflateMakeHuffmanTable(deflateInternalData_t *pW)
{
	count_t *count = pW->u.s.count;
	unsigned char *pLen = pW->u.s.len;
	int pos = 0;
	int code;
	unsigned int i;
	unsigned int minlen;

	code = makeHuffmanTableDistance(pW);
	if (code)
		return code;

	code = 0;
	minlen = 0;
	for (i = 1; i < 16; i++) {
		int c = count[i - 1].i;

		if (c) {
			count[i - 1].p = &pW->sym.lit[pos];
			pos += c;
			if (!minlen)
				minlen = i;
			code += c << (15 - i);
		}
		if (i > 8) {
			pW->litmaxinfo[15 - i] = (code << 17) +
				((pos + DEFLATE_LITERAL_SYMBOL_BASE - (code >> (15 - i))) | 0xffff0000u);
		} else {
			pW->v.w.litmaxcode[i - 1] = (code >> 7) - 1;
		}
	}
	code -= 0x8000;
	if (code)
		return code;

	/* the literals in length order, then the length symbols' descriptors */
	for (i = 0; i < 256; i++) {
		int j = pLen[i];

		if (j)
			*count[j - 1].p++ = i;
	}
	for (i = 0; i < 30; i++) {
		int j = pLen[256 + i];

		if (j)
			*count[j - 1].p++ = s_fixhuffcodes[i];
	}
	makeFastDecodeTable(pW, minlen);
	makeFastDecodeDistance(pW);
	return 0;
}

void _sceDeflateMakeHuffmanFixcode(deflateInternalData_t *pW, int prevflag)
{
	const unsigned short *pSrc = s_fixhuffcodes;
	unsigned int i;

	if ((((unsigned int)prevflag >> 18) & 3) == 1)
		return;
	for (i = 0; i < 32; i++) {
		pW->u.ft[i * 8 + 7] = 24;
		pW->u.ft[i * 8 + 3] = 24;
		pW->v.ftdist[__rbit(i) >> 27] =
			((const unsigned char *)(pSrc + 32))[i] + (5 << 12);
	}
	for (i = 0; i < 24; i++) {
		unsigned int code = pSrc[i];
		unsigned int code2;
		unsigned int pos = __rbit(i) >> 25;

		if ((unsigned char)code == 1) {
			/* a length with one extra bit: both of its codes, the bit in place */
			code = (code >> 7) + 0x87f3;
			code2 = code + 1;
		} else {
			code += 7 << 12;
			code2 = code;
		}
		pW->u.ft[pos] = code;
		pW->u.ft[pos + 128] = code2;
	}
	for (i = 48; i < 192; i++)
		pW->u.ft[__rbit(i) >> 24] = (8 << 12) + i - 48;
	for (; i < 200; i++)
		pW->u.ft[__rbit(i) >> 24] = pSrc[i - 168] - (8 << 12);
	/* Fixed nine-bit literals occupy sorted-symbol slots 8 through 119. */
	for (i = 144; i < 256; i++)
		pW->sym.lit[8 + i - 144] = i;
	pW->litmaxinfo[6] = DEFLATE_LITERAL_SYMBOL_BASE + 8 - 0x190;
}
