#include "deflt.h"
#include "deflate_i.h"
#include <libdeflt.h>

/* SNC uses C99; keep the owning LibKernel header's size checks enabled. */
#if defined(__SNC__)
#define DEFLATE_ASSERT_NAME_(line) deflateHeaderSizeCheck##line
#define DEFLATE_ASSERT_NAME(line) DEFLATE_ASSERT_NAME_(line)
#define _Static_assert(condition, message) \
	typedef char DEFLATE_ASSERT_NAME(__LINE__)[(condition) ? 1 : -1]
#endif
#include <libkernel/libkernel.h>
#if defined(__SNC__)
#undef _Static_assert
#undef DEFLATE_ASSERT_NAME
#undef DEFLATE_ASSERT_NAME_
#endif

typedef struct {
	uint32_t next;
	uint32_t word;
	/* ROR position in the overlapping word; decoded bits start at bit 17. */
	int position;
} DeflateInput;

/* The native reader fetches a word even when only its low half is needed. */
static inline uint32_t DeflateLoadWord(uint32_t address)
{
	return *(const volatile uint32_t *)(uintptr_t)address;
}

static inline uint32_t DeflateWindow(const DeflateInput *input)
{
	unsigned int shift = (unsigned int)input->position & 31;

	return (input->word >> shift) | (input->word << ((32 - shift) & 31));
}

static inline void DeflateRefill(DeflateInput *input)
{
	if (input->position > 0) {
		input->word = DeflateLoadWord(input->next);
		input->next += 2;
		input->position -= 16;
	}
}

static inline void DeflateAdvance(DeflateInput *input, unsigned int count)
{
	input->position += count;
	DeflateRefill(input);
}

static inline uint32_t DeflateShiftRight(uint32_t value, unsigned int count)
{
	count &= 255;
	return count < 32 ? value >> count : 0;
}

/* A repeat crossing the alphabet boundary retains the count cursor index. */
static inline uint32_t *DeflateHistogram(deflateInternalData_t *pW,
					unsigned int index)
{
	if (index < 16)
		return &pW->u.s.flags[index];
	return (uint32_t *)&pW->u.s.count[index - 16].i;
}

static inline void DeflateStoreLength(deflateInternalData_t *pW,
				     unsigned int index, unsigned int length)
{
	if (index < sizeof(pW->u.s.len))
		pW->u.s.len[index] = length;
	else
		pW->u.s.pad[index - sizeof(pW->u.s.len)] = length;
}

int sceDeflateDecompress(void *pDst, unsigned int uiBufSize,
			const void *pSrcDeflate, const void **ppNext)
{
	deflateInternalData_t work;
	DeflateInput input;
	uint32_t start = (uintptr_t)pDst, dst = start, end = start + uiBufSize;
	uint32_t source = (uintptr_t)pSrcDeflate;
	uint32_t header, previous, type;

	work.endflag = source << 31;
	input.next = (source & ~1u) + 2;
	input.word = DeflateLoadWord(source & ~1u);
	input.position = ((source & 1) << 3) - 17;
	do {
		header = DeflateWindow(&input);
		DeflateAdvance(&input, 3);
		type = (header >> 18) & 3;
		if (type == 3)
			return SCE_DEFLATE_ERROR_INVALID_FORMAT;
		previous = work.endflag;
		work.endflag = header;
		if (!type) {
			uint32_t length, nextDst;

			source = input.next + ((input.position + 8) >> 3);
			length = DeflateLoadWord(source);
			source += 4;
			if ((length ^ ((length >> 16) | (length << 16))) != 0xffffffffu)
				return SCE_DEFLATE_ERROR_INVALID_FORMAT;
			length = (uint16_t)length;
			nextDst = dst + length;
			if (nextDst > end)
				return SCE_DEFLATE_ERROR_INVALID_SIZE;
			input.next = source + length;
			sceClibMemcpy_safe((void *)(uintptr_t)dst,
					  (const void *)(uintptr_t)source, length);
			dst = nextDst;
			input.word = DeflateLoadWord(input.next);
			input.next += 2;
			input.position = -17;
		} else {
			if (type == 1) {
				_sceDeflateMakeHuffmanFixcode(&work, previous);
			} else {
				volatile uint8_t *clear = (volatile uint8_t *)&work;
				uint32_t counts, literalCount, previousLength, mask, i;
				int distanceCount, position, excess;

				/* Include the six bytes used by a straddling repeat. */
				for (i = offsetof(deflateInternalData_t, u.s.pad) + 6; i; i--)
					clear[i - 1] = 0;
				counts = DeflateWindow(&input);
				DeflateAdvance(&input, 14);
				for (i = 0; i < ((counts >> 27) & 15) + 4; i++) {
					uint32_t length = (DeflateWindow(&input) >> 17) & 7;

					DeflateAdvance(&input, 3);
					work.u.s.flags[length] +=
						0x80000000u >> _sceDeflateLengthOrder[i];
				}
				i = _sceDeflateMakeHuffmanTableLength(&work);
				literalCount = (counts >> 17) & 31;
				distanceCount = (counts >> 22) & 31;
				if (literalCount > 29 || distanceCount > 29 || i != 128)
					return SCE_DEFLATE_ERROR_INVALID_FORMAT;
				literalCount += 257;
				position = 0;
				previousLength = 0x80000000u;
				do {
					int descriptor = work.sym.len[
						(DeflateWindow(&input) >> 17) & 127];
					int length;
					uint32_t run;

					DeflateAdvance(&input, descriptor & 7);
					length = descriptor >> 3;
					if (length >= 0) {
						DeflateStoreLength(&work, position++, length);
						previousLength = length;
						if (length)
							work.u.s.count[length - 1].i++;
					} else if (length + 16) {
						unsigned int extra = length + 16;

						input.position += extra;
						run = (DeflateWindow(&input) << 15) >> (32 - extra);
						position += 2 * extra - 3 + run;
						previousLength = 0;
					} else {
						run = ((DeflateWindow(&input) >> 17) & 3) + 3;
						input.position += 2;
						if (!previousLength) {
							position += run;
						} else {
							if ((int32_t)(previousLength + 15) < 0)
								return SCE_DEFLATE_ERROR_INVALID_FORMAT;
							work.u.s.count[previousLength - 1].i += run;
							do {
								DeflateStoreLength(&work, position++,
										   previousLength);
							} while (--run);
						}
					}
				} while (position < literalCount);
				excess = position - literalCount;
				mask = DeflateShiftRight(0x80000000u, excess);
				if (excess) {
					distanceCount -= excess;
					if (previousLength) {
						*DeflateHistogram(&work, previousLength) = -(mask << 1);
						previousLength += 15;
						*DeflateHistogram(&work, previousLength) -= excess;
					}
				}
				distanceCount++;
				while (distanceCount > 0) {
					int descriptor = work.sym.len[
						(DeflateWindow(&input) >> 17) & 127];
					int length;
					uint32_t run;

					DeflateAdvance(&input, descriptor & 7);
					length = descriptor >> 3;
					if (length >= 0) {
						previousLength = length;
						*DeflateHistogram(&work, previousLength) |= mask;
						run = 1;
					} else if (length + 16) {
						unsigned int extra = length + 16;

						input.position += extra;
						run = ((DeflateWindow(&input) << 15) >>
						       (32 - extra)) + 2 * extra - 3;
						previousLength = 0;
					} else {
						run = ((DeflateWindow(&input) >> 17) & 3) + 3;
						input.position += 2;
						*DeflateHistogram(&work, previousLength) |=
							2 * (mask - DeflateShiftRight(mask, run));
					}
					distanceCount -= run;
					if (distanceCount > 0)
						mask = DeflateShiftRight(mask, run);
				}
				if (distanceCount)
					return SCE_DEFLATE_ERROR_INVALID_FORMAT;
				DeflateRefill(&input);
				if (_sceDeflateMakeHuffmanTable(&work))
					return SCE_DEFLATE_ERROR_INVALID_FORMAT;
			}
			for (;;) {
				uint32_t window = DeflateWindow(&input);
				uint32_t symbol = work.u.ft[(window >> 17) & 255];
				uint32_t bits = symbol >> 12;
				uint32_t length, encoding, distanceMinus1, nextDst, backref, available;
				int literal = !(symbol & 0x800);

				if (!bits) {
					uint32_t canonical = __rbit(window) << 17, record;
					unsigned int shift = symbol;

					do {
						record = work.litmaxinfo[shift - 18];
						shift--;
					} while (canonical >= record);
					symbol = work.sym.lit[
						(canonical >> shift) + (int16_t)record -
						DEFLATE_LITERAL_SYMBOL_BASE];
					bits = 32 - shift;
					literal = symbol < 0x800;
				}
				if (literal) {
					if (dst == end)
						return SCE_DEFLATE_ERROR_INVALID_SIZE;
					DeflateAdvance(&input, bits);
					*(uint8_t *)(uintptr_t)dst++ = symbol;
					continue;
				}
				DeflateAdvance(&input, bits);
				if (!(symbol & 0x400)) {
					length = symbol & 0x7ff;
				} else {
					uint32_t extra = (uint8_t)symbol;

					if (!extra) {
						if (symbol & 0x100)
							return SCE_DEFLATE_ERROR_INVALID_FORMAT;
						break;
					}
					input.position += extra;
					window = DeflateWindow(&input);
					DeflateRefill(&input);
					length = (((symbol << 21) & 0xe0000fffu) |
						  ((window & 0x1ffff) << 12)) >> (29 - extra);
					length += 3;
				}
				window = DeflateWindow(&input);
				encoding = work.v.ftdist[(window >> 17) & 31];
				bits = encoding >> 12;
				if (encoding & 0x800) {
					uint32_t canonical = __rbit(window) << 17, record;

					do {
						record = work.distmaxinfo[bits - 5];
						bits++;
					} while (canonical >= record);
					encoding = work.distsym[
						(canonical >> (32 - bits)) + (int16_t)record -
						DEFLATE_DISTANCE_SYMBOL_BASE];
				} else {
					encoding = (uint8_t)encoding;
				}
				DeflateAdvance(&input, bits);
				bits = encoding >> 2;
				if (!bits)
					return SCE_DEFLATE_ERROR_INVALID_FORMAT;
				input.position += 30 - bits;
				window = (DeflateWindow(&input) & 0xfff9ffffu) |
					 ((encoding & 3) << 17);
				DeflateRefill(&input);
				distanceMinus1 = (window << 13) >> bits;
				nextDst = dst + length;
				/* The native ADD carry feeds the backward-address subtraction. */
				backref = dst - distanceMinus1 - (nextDst >= dst);
				if (end < nextDst)
					return SCE_DEFLATE_ERROR_INVALID_SIZE;
				if (backref < start)
					return SCE_DEFLATE_ERROR_INVALID_FORMAT;
				available = end - nextDst;
				if (available <= 2 || distanceMinus1 == 2) {
					do {
						uint8_t value = *(const uint8_t *)(uintptr_t)backref++;

						*(uint8_t *)(uintptr_t)dst++ = value;
					} while (--length);
				} else if (distanceMinus1 <= 1) {
					uint32_t value = DeflateLoadWord(backref);
					int remaining = length;

					if (!distanceMinus1)
						value = (value & 0xffff00ffu) | ((value & 255) << 8);
					value = (uint16_t)value | (value << 16);
					do {
						*(uint32_t *)(uintptr_t)dst = value;
						dst += 4;
						remaining -= 4;
					} while (remaining > 0);
					dst = nextDst;
				} else {
					int remaining = length;

					do {
						uint32_t value = DeflateLoadWord(backref);

						backref += 4;
						*(uint32_t *)(uintptr_t)dst = value;
						dst += 4;
						remaining -= 4;
					} while (remaining > 0);
					dst = nextDst;
				}
			}
		}
	} while (!(work.endflag & 0x20000));
	if (ppNext)
		*ppNext = (const void *)(uintptr_t)
			(input.next + ((input.position + 8) >> 3));
	return (int)(dst - start);
}
