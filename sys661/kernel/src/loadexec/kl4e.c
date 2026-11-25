/* Range-coded KL4E stream decoder used by sceLoadExec to decompress reboot.bin. */

#include <common_imp.h>

#include "loadexec_int.h"

enum {
	KL4E_MODEL_BYTES       = 2656,
	KL4E_LITERAL_TREE_SIZE = 255,
	KL4E_DISTANCE_TREE     = 2033,
	KL4E_DISTANCE_BITS     = 2216,
	KL4E_MATCH             = 2336,
	KL4E_LENGTH_LOW        = 2400,
	KL4E_LENGTH_HIGH       = 2424,
	KL4E_OUTPUT_FULL       = (s32)0x80000104,
	KL4E_BAD_DATA          = (s32)0x80000108,
};

typedef union Kl4eProbabilities {
	u8 byte[KL4E_MODEL_BYTES];
	u32 word[KL4E_MODEL_BYTES / sizeof(u32)];
} Kl4eProbabilities;

#define NORMALIZE() do { \
	if ((range >> 24) == 0) { \
		code = (code << 8) + *input++; \
		range <<= 8; \
	} \
} while (0)

#define DECODE_BIT(model, decay, increment, bit) do { \
	u32 probability = (model); \
	u32 bound; \
	NORMALIZE(); \
	bound = (range >> 8) * probability; \
	probability -= probability >> (decay); \
	if (code < bound) { \
		range = bound; \
		probability += (increment); \
		(bit) = 1; \
	} else { \
		code -= bound; \
		range -= bound; \
		(bit) = 0; \
	} \
	(model) = (u8)probability; \
} while (0)

s32 decodeKL4E(void *dest, s32 size, const void *src, const void **srcEnd)
{
	Kl4eProbabilities probabilities;
	u8 *output = (u8 *)dest;
	u8 *outputEnd = output + size;
	u32 cacheLimit = ((u32)outputEnd - 64) & ~63u;
	const u8 *stream = (const u8 *)src;
	const u8 *input = stream + 5;
	u32 range = ~0u;
	u32 code, initialProbability, initialWord;
	u32 literalShift = stream[0] & 7;
	u32 symbol = 0, state = 0;
	u32 bit, length, distance;
	s32 lengthClass;
	u32 i;

	__builtin_memcpy(&code, stream + 1, sizeof(code));
	code = __builtin_bswap32(code);
	if ((s8)stream[0] < 0) {
		if (srcEnd != NULL)
			*srcEnd = input;
		return KL4E_BAD_DATA;
	}

	initialProbability = 128 - 16 * ((stream[0] >> 3) & 3);
	initialWord = initialProbability * 0x01010101u;
	for (i = 0; i < KL4E_MODEL_BYTES / sizeof(u32); i++)
		probabilities.word[i] = initialWord;

literal:
	{
		u32 context = (((symbol & 0xFF) | (((u32)output & 7) << 8)) >> literalShift) & 7;
		u8 *tree = &probabilities.byte[context * KL4E_LITERAL_TREE_SIZE];

		symbol = 1;
		do {
			DECODE_BIT(tree[symbol - 1], 3, 31, bit);
			symbol = symbol * 2 + bit;
		} while (symbol < 256);
	}

match:
	*output = (u8)symbol;
	DECODE_BIT(probabilities.byte[KL4E_MATCH + state], 4, 15, bit);
	output++;
	if (bit == 0) {
		if (state != 0)
			state--;
		if (output == outputEnd) {
			if (srcEnd != NULL)
				*srcEnd = input;
			return KL4E_OUTPUT_FULL;
		}
		goto literal;
	}

	lengthClass = -1;
	do {
		DECODE_BIT(probabilities.byte[KL4E_MATCH + state + 8], 4, 15, bit);
		state += 8;
		if (bit == 0)
			break;
		lengthClass++;
	} while (lengthClass < 6);

	length = 1;
	if (lengthClass >= 0) {
		u32 row = ((u32)lengthClass << 5) | ((((u32)output << lengthClass) & 3) << 3) | (state & 7);
		u32 value = 1;

		if (lengthClass >= 3) {
			u8 highProbability = probabilities.byte[KL4E_LENGTH_HIGH + row];

			DECODE_BIT(highProbability, 3, 31, bit);
			value = 2 + bit;
			if (lengthClass > 3) {
				DECODE_BIT(highProbability, 3, 31, bit);
				value = value * 2 + bit;
				if (lengthClass > 4) {
					NORMALIZE();
					for (i = 0; i < (u32)(lengthClass - 4); i++) {
						range >>= 1;
						bit = code < range;
						if (bit == 0)
							code -= range;
						value = value * 2 + bit;
					}
				}
			}
			probabilities.byte[KL4E_LENGTH_HIGH + row] = highProbability;
		}

		DECODE_BIT(probabilities.byte[KL4E_LENGTH_LOW + row], 3, 31, bit);
		length = value * 2 + bit;
		if (lengthClass > 0) {
			DECODE_BIT(probabilities.byte[KL4E_LENGTH_LOW + row + 8], 3, 31, bit);
			length = length * 2 + bit;
			if (lengthClass > 1) {
				DECODE_BIT(probabilities.byte[KL4E_LENGTH_LOW + row + 16], 3, 31, bit);
				length = length * 2 + bit;
				if (bit != 0 && length == 255) {
					if (srcEnd != NULL)
						*srcEnd = input;
					return (s32)(output - (u8 *)dest);
				}
			}
		}
	}

	{
		s32 treeBase = length <= 2 ? lengthClass : lengthClass + 56;
		u32 treeLimit = length <= 2 ? 64 : 128;
		u32 node = 8;
		s32 selector;

		do {
			DECODE_BIT(probabilities.byte[KL4E_DISTANCE_TREE + treeBase + node], 3, 31, bit);
			node *= 2;
			selector = (s32)(node - treeLimit);
			node += bit * 8;
		} while (selector < 0);

		if (selector == 0 && bit == 0) {
			if (output == (u8 *)dest) {
				if (srcEnd != NULL)
					*srcEnd = input;
				return KL4E_BAD_DATA;
			}
			distance = 0;
		} else {
			u32 packed, value = 1, modelIndex;
			s32 distanceClass, lastBit;

			if (bit == 0)
				selector -= 8;
			distanceClass = selector >> 3;
			modelIndex = (u32)(KL4E_DISTANCE_BITS + selector) / sizeof(u32);
			packed = probabilities.word[modelIndex];
			if (distanceClass >= 3) {
				u8 highProbability = (u8)(packed >> 24);

				DECODE_BIT(highProbability, 3, 31, bit);
				value = 2 + bit;
				if (distanceClass > 3) {
					DECODE_BIT(highProbability, 3, 31, bit);
					value = value * 2 + bit;
					if (distanceClass > 4) {
						NORMALIZE();
						for (i = 0; i < (u32)(distanceClass - 4); i++) {
							range >>= 1;
							bit = code < range;
							if (bit == 0)
								code -= range;
							value = value * 2 + bit;
						}
					}
				}
				packed = (packed & 0x00FFFFFFu) | ((u32)highProbability << 24);
			}

			lastBit = distanceClass < 2 ? distanceClass : 2;
			for (i = 0; i <= (u32)lastBit; i++) {
				u8 lowProbability = (u8)(packed >> (i * 8));

				DECODE_BIT(lowProbability, 3, 31, bit);
				packed = (packed & ~(0xFFu << (i * 8))) | ((u32)lowProbability << (i * 8));
				value *= 2;
				if (i == (u32)lastBit) {
					if (bit == 0)
						value--;
				} else {
					value += bit;
				}
			}
			distance = value;
			probabilities.word[modelIndex] = packed;
			if (distance >= (u32)(output - (u8 *)dest)) {
				if (srcEnd != NULL)
					*srcEnd = input;
				return KL4E_BAD_DATA;
			}
		}
	}

	{
		u8 *copyEnd = output + length;
		const u8 *copySource = output - distance - 1;

		if ((u32)copyEnd >= (u32)outputEnd) {
			if (srcEnd != NULL)
				*srcEnd = input;
			return KL4E_OUTPUT_FULL;
		}
		state = 6 | ((u32)copyEnd & 1);
		if ((u32)copyEnd <= cacheLimit) {
			__asm__ __volatile__("cache 0x18, 64(%0)" :: "r"(output) : "memory");
			if (length >= 4 && distance >= 3) {
				u8 *wordEnd = output + (length & ~3u);

				for (;;) {
					u32 value;

					__builtin_memcpy(&value, copySource, sizeof(value));
					__builtin_memcpy(output, &value, sizeof(value));
					copySource += sizeof(value);
					if (output == wordEnd)
						break;
					output += sizeof(value);
				}
				output = copyEnd;
				symbol = *output;
				goto match;
			}
		}

		for (;;) {
			symbol = *copySource;
			if (output == copyEnd)
				break;
			*output++ = (u8)symbol;
			copySource++;
		}
		goto match;
	}
}

#undef DECODE_BIT
#undef NORMALIZE
