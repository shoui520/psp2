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

uint32_t _sceDeflateMakeHuffmanTableLength(deflateInternalData_t *pW)
{
	uint32_t code = 0;
	int order;

	for (order = 6; order >= 0; order--) {
		uint32_t bits = pW->u.s.flags[7 - order];
		uint8_t descriptor = (uint8_t)~order;

		if (!bits)
			continue;
		while (bits) {
			uint32_t skip = __clz(bits) + 1;
			uint32_t index = __rbit(code) >> 25;

			bits = skip < 32 ? bits << skip : 0;
			descriptor += 8 * skip;
			do {
				pW->sym.len[index] = descriptor;
				index += 128u >> order;
			} while (index <= 127);
			code += 1u << order;
		}
		pW->u.s.flags[7 - order] = 0;
	}
	return code;
}

int32_t _sceDeflateMakeHuffmanTable(deflateInternalData_t *pW)
{
	uint32_t count = 0, space = 0, bits, first;
	uint32_t minLiteralBits = 0, total = 0;
	uint32_t code, end, canonical, index, symbol;
	uint16_t *symbols;

	for (bits = 1; bits <= 15; bits++) {
		uint32_t bitmap = pW->u.s.flags[bits];

		first = count - (space >> (15 - bits));
		while (bitmap) {
			uint32_t distance = __clz(bitmap);

			count++;
			bitmap -= 0x80000000u >> distance;
			space += 1u << (15 - bits);
			pW->distsym[count - 1] =
				((const uint8_t *)(s_fixhuffcodes + 32))[distance];
		}
		if (bits <= 5) {
			if (!first)
				pW->v.w.distminlen = bits;
			pW->distmaxcode[bits - 1] = space >> 10;
		} else {
			pW->distmaxinfo[bits - 6] = (space << 17) +
				(0xffff0000u | (uint16_t)(first + DEFLATE_DISTANCE_SYMBOL_BASE));
		}
	}
	if (count <= 1) {
		pW->distmaxcode[0] = 32;
		pW->v.w.distminlen = count;
		pW->distsym[count] = 0;
		if (count)
			space *= 2;
	}
	if (count && space != 0x8000)
		return (int32_t)(space - 0x8000);

	space = 0;
	for (bits = 1; bits <= 15; bits++) {
		count = pW->u.s.count[bits - 1].i;
		if (count) {
			uint16_t *next = &pW->sym.lit[total];

			total += count;
			if (!minLiteralBits)
				minLiteralBits = bits;
			pW->u.s.count[bits - 1].p = next;
			space += count << (15 - bits);
		}
		if (bits <= 8) {
			pW->v.w.litmaxcode[bits - 1] =
				((int32_t)space >> 7) - 1;
		} else {
			first = total + DEFLATE_LITERAL_SYMBOL_BASE -
				(uint32_t)((int32_t)space >> (15 - bits));
			pW->litmaxinfo[15 - bits] = (space << 17) +
				(0xffff0000u | (uint16_t)first);
		}
	}
	if (space != 0x8000)
		return (int32_t)(space - 0x8000);
	for (symbol = 0; symbol < 286; symbol++) {
		bits = pW->u.s.len[symbol];
		if (bits) {
			uint16_t *next = pW->u.s.count[bits - 1].p;

			pW->u.s.count[bits - 1].p = next + 1;
			*next = symbol < 256 ? symbol : s_fixhuffcodes[symbol - 256];
		}
	}

	symbols = pW->sym.lit;
	bits = minLiteralBits;
	code = 0;
	for (;;) {
		uint32_t descriptor = *symbols++;
		uint32_t extra = descriptor & 7;
		uint32_t mask;

		end = code + (256u >> bits);
		if ((descriptor & 0x400) && extra && extra + bits <= 8) {
			descriptor = (((descriptor >> 8) & 7) << extra) +
				((extra + bits) << 12) + 0x803;
			mask = (1u << extra) - 1;
		} else {
			descriptor += bits << 12;
			mask = 0;
		}
		do {
			index = __rbit(code++ << 24);
			pW->u.ft[index] =
				((index >> bits) & mask) + descriptor;
		} while (code != end);
		while (end > pW->v.w.litmaxcode[bits - 1]) {
			if (++bits > 8)
				goto long_literals;
		}
		code = end;
	}
long_literals:
	canonical = end << 24;
	index = 24;
	while (canonical) {
		while (canonical >= pW->litmaxinfo[index - 18])
			index--;
		pW->u.ft[__rbit(canonical)] = index;
		canonical += 0x01000000;
	}

	bits = pW->v.w.distminlen;
	code = 0;
	index = 0;
	for (;;) {
		uint16_t descriptor = pW->distsym[index++] + (bits << 12);
		uint32_t limit;

		end = code + (32u >> bits);
		do {
			pW->v.ftdist[__rbit(code++) >> 27] = descriptor;
		} while (code != end);
		for (;;) {
			/* With no distance codes, the preceding packed record supplies the limit. */
			limit = bits ? pW->distmaxcode[bits - 1] :
				pW->distmaxinfo[9] >> 24;
			if (end < limit)
				break;
			if (++bits > 5)
				goto long_distances;
		}
		code = end;
	}
long_distances:
	canonical = end << 27;
	while (canonical) {
		while (canonical >= pW->distmaxinfo[bits - 6])
			bits++;
		pW->v.ftdist[__rbit(canonical)] =
			(bits << 12) - 0x800;
		canonical += 0x08000000;
	}
	return 0;
}

void _sceDeflateMakeHuffmanFixcode(deflateInternalData_t *pW, int prevflag)
{
	uint32_t code, index;

	if ((((uint32_t)prevflag >> 18) & 3) == 1)
		return;
	for (code = 0; code < 32; code++) {
		pW->u.ft[code * 8 + 7] = 24;
		pW->u.ft[code * 8 + 3] = 24;
		pW->v.ftdist[__rbit(code) >> 27] =
			((const uint8_t *)(s_fixhuffcodes + 32))[code] + (5u << 12);
	}
	for (code = 0; code < 24; code++) {
		uint32_t descriptor = s_fixhuffcodes[code];
		uint16_t lower, upper;

		index = __rbit(code) >> 25;
		if ((uint8_t)descriptor == 1) {
			lower = (8u << 12) + 0x803 + (((descriptor >> 8) & 7) << 1);
			upper = lower + 1;
		} else {
			lower = descriptor + (7u << 12);
			upper = lower;
		}
		pW->u.ft[index] = lower;
		pW->u.ft[index + 128] = upper;
	}
	for (code = 48; code < 192; code++)
		pW->u.ft[__rbit(code) >> 24] = (8u << 12) + code - 48;
	for (; code < 200; code++)
		pW->u.ft[__rbit(code) >> 24] =
			s_fixhuffcodes[code - 168] + (8u << 12);
	/* Fixed nine-bit literals occupy sorted-symbol slots 8 through 119. */
	for (code = 144; code < 256; code++)
		pW->sym.lit[8 + code - 144] = code;
	pW->litmaxinfo[6] = DEFLATE_LITERAL_SYMBOL_BASE + 8 - 0x190;
}
