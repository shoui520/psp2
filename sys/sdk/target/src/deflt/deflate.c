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

/* GCC 4.8 at -Os (Sony's flags for the library) keeps static inline helpers out of line */
#ifdef __SNC__
#define DEFLATE_INLINE static inline
#else
#define DEFLATE_INLINE static inline __attribute__((always_inline))
#endif

typedef struct {
	uint32_t next;
	uint32_t word;
	/* ROR position in the overlapping word; decoded bits start at bit 17. */
	int position;
} DeflateInput;

/* The native reader fetches a word even when only its low half is needed. */
DEFLATE_INLINE uint32_t DeflateLoadWord(uint32_t address)
{
	return *(const volatile uint32_t *)(uintptr_t)address;
}

DEFLATE_INLINE uint32_t DeflateWindow(const DeflateInput *input)
{
	unsigned int shift = (unsigned int)input->position & 31;

	return (input->word >> shift) | (input->word << ((32 - shift) & 31));
}

DEFLATE_INLINE void DeflateRefill(DeflateInput *input)
{
	if (input->position > 0) {
		input->word = DeflateLoadWord(input->next);
		input->next += 2;
		input->position -= 16;
	}
}

DEFLATE_INLINE void DeflateAdvance(DeflateInput *input, unsigned int count)
{
	input->position += count;
	DeflateRefill(input);
}

DEFLATE_INLINE uint32_t DeflateShiftRight(uint32_t value, unsigned int count)
{
	count &= 255;
	return count < 32 ? value >> count : 0;
}

/* A repeat crossing the alphabet boundary retains the count cursor index. */
DEFLATE_INLINE uint32_t *DeflateHistogram(deflateInternalData_t *pW,
					unsigned int index)
{
	if (index < 16)
		return &pW->u.s.flags[index];
	return (uint32_t *)&pW->u.s.count[index - 16].i;
}

DEFLATE_INLINE void DeflateStoreLength(deflateInternalData_t *pW,
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
#ifdef SCE_DEFLT_2014
	deflateInternalData_t work;
	DeflateInput input;
	uint32_t start = (uintptr_t)pDst, dst = start, end = start + uiBufSize;
	uint32_t source = (uintptr_t)pSrcDeflate;
	uint32_t header, type;

	input.next = (source & ~1u) + 2;
	input.word = DeflateLoadWord(source & ~1u);
	input.position = ((source & 1) << 3) - 17;
	do {
		header = DeflateWindow(&input);
		DeflateAdvance(&input, 3);
		type = (header >> 18) & 3;
		int minlen;

		if (type != 2) {
			if (type >= 2)
				return SCE_DEFLATE_ERROR_INVALID_FORMAT;
			if (type == 0) {
				uint32_t lenWord, nlenWord, storedLen;
				uint32_t nextWord;

				input.position = (input.position + 8) & ~7;
				DeflateRefill(&input);
				lenWord = DeflateWindow(&input);
				nextWord = DeflateLoadWord(input.next);
				input.next += 2;
				input.word = nextWord;
				nlenWord = DeflateWindow(&input);
				input.word = DeflateLoadWord(input.next);
				input.next += 2;
				if (((int32_t)(nlenWord ^ lenWord) >> 16) != -1)
					return SCE_DEFLATE_ERROR_INVALID_FORMAT;
				storedLen = lenWord >> 16;
				if (storedLen != 0) {
					uint32_t nextDst = dst + storedLen;

					if (nextDst > end)
						return SCE_DEFLATE_ERROR_INVALID_SIZE;
					do {
						uint32_t byteWord = DeflateWindow(&input);

						DeflateAdvance(&input, 8);
						*(uint8_t *)(uintptr_t)dst++ =
							(uint8_t)(byteWord >> 16);
					} while (dst != nextDst);
				}
				input.position--;
				continue;
			}
			minlen = _sceDeflateMakeHuffmanFixcode(&work);
		} else {
			uint32_t *clearWords = (uint32_t *)&work;
			uint32_t counts, literalCount, previousLength, mask, idx;
			int distanceCount, position, excess;

			for (idx = 0; idx < sizeof(work.u.s) / sizeof(uint32_t); idx++)
				clearWords[idx] = 0;
			counts = DeflateWindow(&input);
			DeflateAdvance(&input, 14);
			for (idx = 0; idx < ((counts >> 27) & 15) + 4; idx++) {
				uint32_t codeLen = (DeflateWindow(&input) >> 17) & 7;

				DeflateAdvance(&input, 3);
				work.u.s.flags[codeLen] |=
					0x80000000u >> _sceDeflateLengthOrder[idx];
			}
			int tableErr = _sceDeflateMakeHuffmanTableLength(&work);

			literalCount = (counts >> 17) & 31;
			distanceCount = (counts >> 22) & 31;
			if (tableErr != 0 || literalCount > 29 || distanceCount > 29)
				return SCE_DEFLATE_ERROR_INVALID_FORMAT;
			literalCount += 257;
			position = 0;
			previousLength = 0x80000000u;
			do {
				int descriptor = work.tail.s.sym.len[
					(DeflateWindow(&input) >> 17) & 127];
				int codeLen;
				uint32_t run;

				DeflateAdvance(&input, (unsigned int)(descriptor & 7));
				codeLen = descriptor >> 3;
				if (codeLen >= 0) {
					DeflateStoreLength(&work, (unsigned int)position++,
							   (unsigned int)codeLen);
					previousLength = (unsigned int)codeLen;
					if (codeLen != 0)
						work.u.s.count[codeLen - 1].i++;
				} else if (codeLen + 16) {
					unsigned int extra = (unsigned int)(codeLen + 16);

					input.position += (int)extra;
					run = (DeflateWindow(&input) << 15) >> (32 - extra);
					position += (int)(2 * extra - 3 + run);
					previousLength = 0;
				} else {
					run = ((DeflateWindow(&input) >> 17) & 3) + 3;
					input.position += 2;
					if (previousLength == 0) {
						position += (int)run;
					} else {
						if ((int32_t)(previousLength + 15) < 0)
							return SCE_DEFLATE_ERROR_INVALID_FORMAT;
						work.u.s.count[previousLength - 1].i += (int)run;
						do {
							DeflateStoreLength(&work,
									   (unsigned int)position++,
									   previousLength);
						} while (--run);
					}
				}
			} while (position < (int)literalCount);
			excess = position - (int)literalCount;
			mask = DeflateShiftRight(0x80000000u, (unsigned int)excess);
			if (excess != 0) {
				distanceCount -= excess;
				if (previousLength != 0) {
					*DeflateHistogram(&work, previousLength) = -(mask << 1);
					previousLength += 15;
					*DeflateHistogram(&work, previousLength) -=
						(uint32_t)excess;
				}
			}
			distanceCount++;
			while (distanceCount > 0) {
				int descriptor = work.tail.s.sym.len[
					(DeflateWindow(&input) >> 17) & 127];
				int codeLen;
				uint32_t run;

				DeflateAdvance(&input, (unsigned int)(descriptor & 7));
				codeLen = descriptor >> 3;
				if (codeLen >= 0) {
					previousLength = (unsigned int)codeLen;
					*DeflateHistogram(&work, previousLength) |= mask;
					run = 1;
				} else if (codeLen + 16) {
					unsigned int extra = (unsigned int)(codeLen + 16);

					input.position += (int)extra;
					run = ((DeflateWindow(&input) << 15) >> (32 - extra)) +
					      2 * extra - 3;
					previousLength = 0;
				} else {
					run = ((DeflateWindow(&input) >> 17) & 3) + 3;
					input.position += 2;
					*DeflateHistogram(&work, previousLength) |=
						2 * (mask - DeflateShiftRight(mask, run));
				}
				distanceCount -= (int)run;
				if (distanceCount > 0)
					mask = DeflateShiftRight(mask, run);
			}
			if (distanceCount != 0)
				return SCE_DEFLATE_ERROR_INVALID_FORMAT;
			DeflateRefill(&input);
			minlen = _sceDeflateMakeHuffmanTable(&work);
			if (minlen < 0)
				return SCE_DEFLATE_ERROR_INVALID_FORMAT;
		}

		/* Build the 8-bit fast literal/length lookup table. */
		{
			uint32_t code = 0;
			const uint16_t *symPtr = work.tail.s.sym.lit;
			int hlen = minlen;
			int longCodes = 0;

			for (;;) {
				uint16_t entry = *symPtr++ | (uint16_t)(hlen << 12);
				unsigned int fillCount = 256u >> hlen;

				do {
					work.u.ft[__rbit(code)] = entry;
					code += 0x01000000u;
				} while (--fillCount);
				if ((code >> 17) == 0)
					break;
				while ((code >> 17) >= work.tail.s.litmaxcode[hlen - 1]) {
					if (++hlen > 8) {
						longCodes = 1;
						break;
					}
				}
				if (longCodes)
					break;
			}
			if (longCodes) {
				const uint32_t *limitPtr = work.litmaxinfo;
				uint16_t shift = (uint16_t)(34 - hlen);
				int finished = 0;

				while (!finished) {
					uint32_t limit = *limitPtr++;

					shift--;
					while (code < limit) {
						uint32_t nextCode = code + 0x01000000u;

						work.u.ft[__rbit(code)] = shift;
						if (nextCode < code) {
							finished = 1;
							break;
						}
						code = nextCode;
					}
				}
			}
		}

		for (;;) {
			uint32_t window = DeflateWindow(&input);
			uint32_t symbol = work.u.ft[(window >> 17) & 255];
			uint32_t bits = symbol >> 12;
			int literal;

			if (bits == 0) {
				uint32_t canonical = __rbit(window) << 17;
				const uint32_t *limitPtr = &work.litmaxinfo[24 - symbol];
				uint32_t record;
				unsigned int shift = symbol;

				do {
					record = *limitPtr++;
					shift--;
				} while (canonical >= record);
				symbol = work.tail.lit_fix[
					(int)(canonical >> shift) - (int16_t)record -
					DEFLATE_LITERAL_SYMBOL_BASE];
				bits = 32 - shift;
				literal = symbol < 0x800;
			} else {
				literal = !(symbol & 0x800);
			}
			if (literal) {
				if (dst == end)
					return SCE_DEFLATE_ERROR_INVALID_SIZE;
				DeflateAdvance(&input, bits);
				*(uint8_t *)(uintptr_t)dst++ = (uint8_t)symbol;
				continue;
			}

			DeflateAdvance(&input, bits);

			uint32_t baseLen = (symbol >> 3) & 0xff;
			uint32_t extra = symbol & 7;

			if (extra != 0) {
				if (baseLen > extra) {
					input.position += (int)extra;
					window = DeflateWindow(&input);
					DeflateRefill(&input);
					baseLen += (window << 15) >> (32 - extra);
				} else if (baseLen == extra) {
					break;
				} else {
					return SCE_DEFLATE_ERROR_INVALID_FORMAT;
				}
			}

			uint32_t canonical = __rbit(DeflateWindow(&input)) << 17;
			uint32_t distBits = work.dist.d.distmaxinfo[14];

			if (canonical >= work.dist.d.distmaxinfo[14]) {
				int step = 12;
				uint32_t hiLimit = work.dist.d.distmaxinfo[12];
				uint32_t loLimit = work.dist.d.distmaxinfo[13];

				while (step > 0 && canonical >= hiLimit) {
					step -= 2;
					hiLimit = work.dist.d.distmaxinfo[step];
					loLimit = work.dist.d.distmaxinfo[step + 1];
					distBits += 2;
				}
				if (canonical >= loLimit) {
					distBits += 2;
					loLimit = hiLimit;
				} else {
					distBits += 1;
				}
				canonical -= loLimit << 17;
			}
			distBits &= 0xf;

			int distVal = work.dist.d.distsym[canonical >> (32 - distBits)];

			DeflateAdvance(&input, distBits);
			if (distVal <= 3) {
				if ((unsigned int)distVal > 3)
					return SCE_DEFLATE_ERROR_INVALID_FORMAT;
			} else {
				unsigned int lz = __clz((unsigned int)distVal);

				input.position += (int)(30 - lz);
				window = DeflateWindow(&input);
				DeflateRefill(&input);
				distVal += (int)(((window << 15) >> lz) >> 2);
			}

			uint32_t copyLen = baseLen + 3;
			uint32_t nextDst = dst + copyLen;

			if (end < nextDst)
				return SCE_DEFLATE_ERROR_INVALID_SIZE;
			if ((uint32_t)distVal >= dst - start)
				return SCE_DEFLATE_ERROR_INVALID_FORMAT;

			uint32_t backref = dst - (uint32_t)distVal - 1;
			uint32_t available = end - nextDst;

			if (distVal <= 2 || available <= 2) {
				do {
					*(uint8_t *)(uintptr_t)dst++ =
						*(const uint8_t *)(uintptr_t)backref++;
				} while (--copyLen);
			} else {
				int remaining = (int)copyLen;

				do {
					*(uint32_t *)(uintptr_t)dst = DeflateLoadWord(backref);
					backref += 4;
					dst += 4;
					remaining -= 4;
				} while (remaining > 0);
				dst = nextDst;
			}
		}
	} while (!(header & 0x20000));
	if (ppNext)
		*ppNext = (const void *)(uintptr_t)
			(input.next + ((input.position + 8) >> 3));
	return (int)(dst - start);
#else
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
#ifndef DEFLATE_MEMCPY
#define DEFLATE_MEMCPY sceClibMemcpy_safe
#endif
			DEFLATE_MEMCPY((void *)(uintptr_t)dst,
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
#endif
}
