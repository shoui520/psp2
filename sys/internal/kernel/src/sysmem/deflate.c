/*
 * SceKernelUtils: the raw deflate decoder (ksceDeflateDecompress, and
 * ksceDeflateDecompressPartial, which asks a callback for more input).
 *
 * The original is hand-written assembly (0x81027cf0..0x81028564) with C
 * table builders (0x81029a18..0x81029d88) for its own two-level tables.
 * This is C for the same behaviour: the input is read through the same
 * 32-bit window refilled a halfword at a time, so the partial callback is
 * asked for input at the same points and with the same output size, and
 * the errors, their order and the end pointer are the original's. The
 * Huffman codes are decoded canonically instead of through Sony's tables.
 */

#include <psp2kern/types.h>
#include <psp2kern/kernel/utils.h>
#include <kernel/error.h>

void *memcpy(void *dst, const void *src, SceSize n);

#define DEFLATE_MAX_BITS		15
#define DEFLATE_LITLEN_CODES		288
#define DEFLATE_DIST_CODES		32
#define DEFLATE_PRECODE_CODES		19

/* the input: window holds the 4 bytes from in - 2, the next bit is its bit
   pos + 17 (mod 32); pos stays <= 0 between reads */
typedef struct DeflateStream {
	SceUInt32 window;
	int pos;
	const SceUInt8 *in;
	const SceUInt8 *refillAt;		/* partial: the halfword load that needs more input */
	SceDeflatePartialInputParam *param;
	SceUInt8 *dst;
	SceUInt8 *out;
	SceUInt8 *outEnd;
	int error;
} DeflateStream;

typedef struct DeflateHuffman {
	SceUInt16 count[DEFLATE_MAX_BITS + 1];	/* codes of each length */
	SceUInt16 symbol[DEFLATE_LITLEN_CODES];	/* by code */
} DeflateHuffman;

static const SceUInt16 s_lengthBase[29] = {
	3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
	35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258,
};

static const SceUInt8 s_lengthExtra[29] = {
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
	3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0,
};

static const SceUInt16 s_distBase[30] = {
	1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
	257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577,
};

static const SceUInt8 s_distExtra[30] = {
	0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
	7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13,
};

/* the order the precode lengths come in */
static const SceUInt8 s_precodeOrder[DEFLATE_PRECODE_CODES] = {
	16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15,
};

static inline SceUInt32 DeflateLoad32(const SceUInt8 *p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | ((SceUInt32)p[3] << 24);
}

static inline SceUInt16 DeflateLoad16(const SceUInt8 *p)
{
	return p[0] | (p[1] << 8);
}

/* the window rotated so that the next bit is bit 17 */
static inline SceUInt32 DeflateWindow(const DeflateStream *s)
{
	SceUInt32 n = s->pos & 31;

	return n == 0 ? s->window : (s->window >> n) | (s->window << (32 - n));
}

/* the next n (<= 15) bits */
static inline SceUInt32 DeflatePeek(const DeflateStream *s, int n)
{
	return (DeflateWindow(s) >> 17) & ((1u << n) - 1);
}

/* more input from the partial callback for the halfword at refillAt */
static void DeflateRefill(DeflateStream *s)
{
	SceDeflatePartialInputParam *param = s->param;
	const SceUInt8 *in;
	SceSize size;

	in = param->SceDeflateDecompressPartialInputCallback(param, s->out - s->dst);
	if (in == NULL) {
		s->error = SCE_DEFLATE_ERROR_INVALID_POINTER;
		return;
	}
	size = (const SceUInt8 *)param->pBufEnd - in;
	if (size == 0 || (size & 1)) {
		s->error = SCE_DEFLATE_ERROR_INVALID_POINTER;
		return;
	}
	s->refillAt = (const SceUInt8 *)param->pBufEnd - 2;
	s->in = in;
	s->window = (s->window >> 16) | ((SceUInt32)DeflateLoad16(in) << 16);
}

/* the window moves on by a halfword */
static inline void DeflateLoad(DeflateStream *s)
{
	if (s->in == s->refillAt) {
		DeflateRefill(s);
		return;
	}
	s->window = DeflateLoad32(s->in);
	s->in += 2;
}

static inline void DeflateAdvance(DeflateStream *s, int n)
{
	s->pos += n;
	if (s->pos > 0) {
		s->pos -= 16;
		DeflateLoad(s);
	}
}

static inline int DeflateBits(DeflateStream *s, int n)
{
	int bits = DeflatePeek(s, n);

	DeflateAdvance(s, n);
	return bits;
}

/* the canonical code of lengths; 0 when complete, > 0 when incomplete,
   < 0 when over-subscribed */
static int DeflateBuild(DeflateHuffman *h, const SceUInt8 *length, int n)
{
	SceUInt16 offset[DEFLATE_MAX_BITS + 1];
	int symbol, len, left;

	for (len = 0; len <= DEFLATE_MAX_BITS; len++)
		h->count[len] = 0;
	for (symbol = 0; symbol < n; symbol++)
		h->count[length[symbol]]++;
	left = 1;
	for (len = 1; len <= DEFLATE_MAX_BITS; len++) {
		left <<= 1;
		left -= h->count[len];
		if (left < 0)
			return left;
	}
	offset[1] = 0;
	for (len = 1; len < DEFLATE_MAX_BITS; len++)
		offset[len + 1] = offset[len] + h->count[len];
	for (symbol = 0; symbol < n; symbol++) {
		if (length[symbol] != 0)
			h->symbol[offset[length[symbol]]++] = symbol;
	}
	return left;
}

/* the next symbol, its code consumed; -1 when no code matches */
static int DeflateDecode(DeflateStream *s, const DeflateHuffman *h)
{
	SceUInt32 bits = DeflatePeek(s, DEFLATE_MAX_BITS);
	int code = 0, first = 0, index = 0, count, len;

	for (len = 1; len <= DEFLATE_MAX_BITS; len++) {
		code |= bits & 1;
		bits >>= 1;
		count = h->count[len];
		if (code - count < first) {
			DeflateAdvance(s, len);
			return h->symbol[index + (code - first)];
		}
		index += count;
		first += count;
		first <<= 1;
		code <<= 1;
	}
	return -1;
}

/* a block's literals and matches up to its end */
static int DeflateCodes(DeflateStream *s, const DeflateHuffman *litlen, const DeflateHuffman *dist)
{
	SceUInt8 *from;
	SceSize len, distance;
	int symbol;

	for (;;) {
		symbol = DeflateDecode(s, litlen);
		if (s->error)
			return s->error;
		if (symbol < 0)
			return SCE_DEFLATE_ERROR_INVALID_FORMAT;
		if (symbol < 256) {
			if (s->out == s->outEnd)
				return SCE_DEFLATE_ERROR_INVALID_SIZE;
			*s->out++ = symbol;
			continue;
		}
		if (symbol == 256)
			return 0;
		symbol -= 257;
		if (symbol >= 29)
			return SCE_DEFLATE_ERROR_INVALID_FORMAT;
		len = s_lengthBase[symbol];
		if (s_lengthExtra[symbol] != 0)
			len += DeflateBits(s, s_lengthExtra[symbol]);
		if (s->error)
			return s->error;
		symbol = DeflateDecode(s, dist);
		if (s->error)
			return s->error;
		if (symbol < 0 || symbol >= 30)
			return SCE_DEFLATE_ERROR_INVALID_FORMAT;
		distance = s_distBase[symbol];
		if (s_distExtra[symbol] != 0)
			distance += DeflateBits(s, s_distExtra[symbol]);
		if (s->error)
			return s->error;
		if (len > (SceSize)(s->outEnd - s->out))
			return SCE_DEFLATE_ERROR_INVALID_SIZE;
		if (distance > (SceSize)(s->out - s->dst))
			return SCE_DEFLATE_ERROR_INVALID_FORMAT;
		from = s->out - distance;
		while (len-- != 0)
			*s->out++ = *from++;
	}
}

/* the fixed codes (block type 1) */
static void DeflateFixed(DeflateHuffman *litlen, DeflateHuffman *dist)
{
	SceUInt8 length[DEFLATE_LITLEN_CODES];
	int symbol;

	for (symbol = 0; symbol < 144; symbol++)
		length[symbol] = 8;
	for (; symbol < 256; symbol++)
		length[symbol] = 9;
	for (; symbol < 280; symbol++)
		length[symbol] = 7;
	for (; symbol < DEFLATE_LITLEN_CODES; symbol++)
		length[symbol] = 8;
	DeflateBuild(litlen, length, DEFLATE_LITLEN_CODES);
	for (symbol = 0; symbol < DEFLATE_DIST_CODES; symbol++)
		length[symbol] = 5;
	DeflateBuild(dist, length, DEFLATE_DIST_CODES);
}

/* the codes of a dynamic block (block type 2) */
static int DeflateDynamic(DeflateStream *s, DeflateHuffman *litlen, DeflateHuffman *dist)
{
	SceUInt8 length[DEFLATE_LITLEN_CODES + DEFLATE_DIST_CODES];
	DeflateHuffman precode;
	int nlen, ndist, ncode, index, symbol, repeat, left, used;
	SceUInt32 header;

	header = DeflatePeek(s, 14);
	DeflateAdvance(s, 14);
	nlen = (header & 0x1f) + 257;
	ndist = ((header >> 5) & 0x1f) + 1;
	ncode = (header >> 10) + 4;
	for (index = 0; index < DEFLATE_PRECODE_CODES; index++)
		length[index] = 0;
	for (index = 0; index < ncode; index++)
		length[s_precodeOrder[index]] = DeflateBits(s, 3);
	if (s->error)
		return s->error;
	if (nlen > 286 || ndist > 30 || DeflateBuild(&precode, length, DEFLATE_PRECODE_CODES) != 0)
		return SCE_DEFLATE_ERROR_INVALID_FORMAT;
	index = 0;
	while (index < nlen + ndist) {
		symbol = DeflateDecode(s, &precode);
		if (s->error)
			return s->error;
		if (symbol < 16) {
			length[index++] = symbol;
			continue;
		}
		if (symbol == 16) {
			if (index == 0)
				return SCE_DEFLATE_ERROR_INVALID_FORMAT;
			left = length[index - 1];
			repeat = 3 + DeflateBits(s, 2);
		} else if (symbol == 17) {
			left = 0;
			repeat = 3 + DeflateBits(s, 3);
		} else {
			left = 0;
			repeat = 11 + DeflateBits(s, 7);
		}
		if (s->error)
			return s->error;
		if (index + repeat > nlen + ndist)
			return SCE_DEFLATE_ERROR_INVALID_FORMAT;
		while (repeat-- != 0)
			length[index++] = left;
	}
	if (DeflateBuild(litlen, length, nlen) != 0)
		return SCE_DEFLATE_ERROR_INVALID_FORMAT;
	left = DeflateBuild(dist, length + nlen, ndist);
	used = 0;
	for (index = 0; index < ndist; index++) {
		if (length[nlen + index] != 0)
			used++;
	}
	/* no distance code, or a single one of length 1, may be incomplete */
	if (left < 0 || (left > 0 && used > 1) || (used == 1 && dist->count[1] != 1 && left > 0))
		return SCE_DEFLATE_ERROR_INVALID_FORMAT;
	return 0;
}

/* the window starts again at a byte (partial input; the original's parity rule) */
static void DeflateRestart(DeflateStream *s, const SceUInt8 *in)
{
	if (((uintptr_t)s->refillAt ^ (uintptr_t)in) & 1) {
		s->window = (SceUInt32)*in << 8;
		s->pos = -9;
		in--;
		s->in = in;
		if (in == s->refillAt) {
			/* the original loses the byte here when it is the buffer's last */
			s->window &= 0xffff0000;
			DeflateRefill(s);
			return;
		}
		s->in = in + 2;
		s->window |= (SceUInt32)DeflateLoad16(in + 2) << 16;
		return;
	}
	s->pos = -1;
	s->window = (SceUInt32)DeflateLoad16(in) << 16;
	s->in = in;
}

/* a stored block (block type 0) */
static int DeflateStored(DeflateStream *s)
{
	const SceUInt8 *in;
	SceUInt32 len, check, lenword;
	SceUInt8 *end;
	SceSize chunk;

	if (s->param == NULL) {
		in = s->in + ((s->pos + 8) >> 3);
		lenword = DeflateLoad32(in);
		in += 4;
		if ((lenword ^ ((lenword >> 16) | (lenword << 16))) != 0xffffffff)
			return SCE_DEFLATE_ERROR_INVALID_FORMAT;
		len = lenword & 0xffff;
		if (len > (SceSize)(s->outEnd - s->out))
			return SCE_DEFLATE_ERROR_INVALID_SIZE;
		memcpy(s->out, in, len);
		s->out += len;
		s->in = in + len;
		s->window = DeflateLoad32(s->in);
		s->in += 2;
		s->pos = -17;
		return 0;
	}
	s->pos = (s->pos + 8) & ~7;
	if (s->pos > 0) {
		s->pos -= 16;
		DeflateLoad(s);
	}
	len = DeflateWindow(s);
	DeflateLoad(s);
	check = DeflateWindow(s);
	DeflateLoad(s);
	if (s->error)
		return s->error;
	if ((check ^ len) >> 16 != 0xffff)
		return SCE_DEFLATE_ERROR_INVALID_FORMAT;
	len >>= 16;
	if (len == 0) {
		s->pos--;
		return 0;
	}
	if (len > (SceSize)(s->outEnd - s->out))
		return SCE_DEFLATE_ERROR_INVALID_SIZE;
	in = s->in + (s->pos >> 3);
	end = s->out + len;
	do {
		chunk = (const SceUInt8 *)s->param->pBufEnd - in;
		if (chunk > (SceSize)(end - s->out))
			chunk = end - s->out;
		memcpy(s->out, in, chunk);
		in += chunk;
		s->out += chunk;
		if (in == s->param->pBufEnd) {
			in = s->param->SceDeflateDecompressPartialInputCallback(s->param,
									s->out - s->dst);
			if (in == NULL || in == s->param->pBufEnd ||
			    (((const SceUInt8 *)s->param->pBufEnd - in) & 1))
				return SCE_DEFLATE_ERROR_INVALID_POINTER;
		}
	} while (s->out != end);
	s->refillAt = (const SceUInt8 *)s->param->pBufEnd - 2;
	DeflateRestart(s, in);
	return s->error;
}

/* the blocks up to the last one */
static int DeflateBlocks(DeflateStream *s, const void **next)
{
	DeflateHuffman litlen, dist;
	SceUInt32 header;
	int ret;

	do {
		header = DeflateBits(s, 3);
		if (s->error)
			return s->error;
		switch (header >> 1) {
		case 0:
			ret = DeflateStored(s);
			break;
		case 1:
			DeflateFixed(&litlen, &dist);
			ret = DeflateCodes(s, &litlen, &dist);
			break;
		case 2:
			ret = DeflateDynamic(s, &litlen, &dist);
			if (ret == 0)
				ret = DeflateCodes(s, &litlen, &dist);
			break;
		default:
			ret = SCE_DEFLATE_ERROR_INVALID_FORMAT;
			break;
		}
		if (ret < 0)
			return ret;
	} while (!(header & 1));
	if (next != NULL)
		*next = s->in + ((s->pos + 8) >> 3);
	return s->out - s->dst;
}

int ksceDeflateDecompress(void *dst, SceSize dst_size, const void *src, const void **next)
{
	DeflateStream s;
	const SceUInt8 *in = (const SceUInt8 *)((uintptr_t)src & ~1);

	s.dst = s.out = dst;
	s.outEnd = s.dst + dst_size;
	s.param = NULL;
	s.refillAt = NULL;
	s.error = 0;
	s.window = DeflateLoad32(in);
	s.in = in + 2;
	s.pos = (((uintptr_t)src & 1) << 3) - 17;
	return DeflateBlocks(&s, next);
}

int ksceDeflateDecompressPartial(void *dst, SceSize dst_size, const void *src, const void **next,
				 SceDeflatePartialInputParam *cbInfo)
{
	DeflateStream s;
	const SceUInt8 *in = src;

	if (cbInfo == NULL || cbInfo->size != sizeof(*cbInfo))
		return SCE_DEFLATE_ERROR_INVALID_CBINFO;
	s.dst = s.out = dst;
	s.outEnd = s.dst + dst_size;
	s.param = cbInfo;
	s.refillAt = (const SceUInt8 *)cbInfo->pBufEnd - 2;
	s.error = 0;
	s.window = 0;
	s.pos = -1;
	if (((uintptr_t)s.refillAt ^ (uintptr_t)in) & 1) {
		s.window = (SceUInt32)*in++ << 24;
		s.pos -= 8;
	}
	s.in = in - 2;
	DeflateLoad(&s);
	if (s.error)
		return s.error;
	return DeflateBlocks(&s, next);
}
