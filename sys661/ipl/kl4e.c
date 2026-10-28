/* Range-coded KL4E stream decoder used by loadelf_boot.c. */

#include "ipl.h"

enum {
	KL4E_MODEL_BYTES = 2808,
	KL4E_LITERAL_TREE_SIZE = 255,
	KL4E_DISTANCE_TREE = 2033,
	KL4E_DISTANCE_BITS = 2344,
	KL4E_MATCH = 2488,
	KL4E_LENGTH_LOW = 2552,
	KL4E_LENGTH_HIGH = 2576,
	KL4E_OUTPUT_FULL = 0x80000104,
	KL4E_BAD_DISTANCE = 0x80000108,
};

/* Packed distance models share storage with the byte probability trees. */
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

int decompressKl4e(void *dest, SceSize size, const void *src, const void **srcEnd)
{
	Kl4eProbabilities probabilities;
	u8 *output = dest;
	u8 *outputEnd = output + size;
	u32 cacheLimit = ((u32)outputEnd - 64) & ~63u;
	const u8 *stream = src;
	const u8 *input = stream + 5;
	u32 range = ~0u;
	u32 code, initialProbability, initialWord;
	u32 literalShift = stream[0] & 7;
	u32 symbol = 0, state = 0;
	u32 bit, length, distance;
	int lengthClass;
	unsigned int i;

	__builtin_memcpy(&code, stream + 1, sizeof(code));
	code = __builtin_bswap32(code);
	if (stream[0] & 0x80) {
		u8 *copyEnd = output + code;

		if ((u32)copyEnd >= (u32)outputEnd)
			return KL4E_OUTPUT_FULL;
		/* The original preincrements the destination, including the last byte. */
		for (;;) {
			u8 value = *input;

			*++output = value;
			if (output == copyEnd)
				break;
			input++;
		}
		if (srcEnd != NULL)
			*srcEnd = input;
		return output - (u8 *)dest;
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
	*output = symbol;
	DECODE_BIT(probabilities.byte[KL4E_MATCH + state], 4, 15, bit);
	output++;
	if (bit == 0) {
		if (state != 0)
			state--;
		if (output == outputEnd)
			return KL4E_OUTPUT_FULL;
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
		u32 row = (lengthClass << 5) | ((((u32)output << lengthClass) & 3) << 3) | (state & 7);
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
					for (i = 0; i < (unsigned int)(lengthClass - 4); i++) {
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
					return output - (u8 *)dest;
				}
			}
		}
	}

	{
		int treeBase = length <= 2 ? lengthClass : lengthClass + 56;
		u32 treeLimit = length <= 2 ? 64 : 256;
		u32 node = 8;
		int selector;

		do {
			DECODE_BIT(probabilities.byte[KL4E_DISTANCE_TREE + treeBase + node], 3, 31, bit);
			node *= 2;
			selector = node - treeLimit;
			node += bit * 8;
		} while (selector < 0);

		if (selector == 0 && bit == 0) {
			if ((u32)probabilities.byte + treeBase == (u32)output)
				return KL4E_BAD_DISTANCE;
			distance = 0;
		} else {
			u32 packed, value = 1, modelIndex;
			int distanceClass, lastBit;

			if (bit == 0)
				selector -= 8;
			distanceClass = selector >> 3;
			modelIndex = (KL4E_DISTANCE_BITS + selector) / sizeof(u32);
			packed = probabilities.word[modelIndex];
			if (distanceClass >= 3) {
				u8 highProbability = packed >> 24;

				DECODE_BIT(highProbability, 3, 31, bit);
				value = 2 + bit;
				if (distanceClass > 3) {
					DECODE_BIT(highProbability, 3, 31, bit);
					value = value * 2 + bit;
					if (distanceClass > 4) {
						NORMALIZE();
						for (i = 0; i < (unsigned int)(distanceClass - 4); i++) {
							range >>= 1;
							bit = code < range;
							if (bit == 0)
								code -= range;
							value = value * 2 + bit;
						}
					}
				}
				packed = (packed & 0x00FFFFFF) | ((u32)highProbability << 24);
			}

			lastBit = distanceClass < 2 ? distanceClass : 2;
			for (i = 0; i <= (unsigned int)lastBit; i++) {
				u8 lowProbability = packed >> (i * 8);

				DECODE_BIT(lowProbability, 3, 31, bit);
				packed = (packed & ~(0xFFu << (i * 8))) | ((u32)lowProbability << (i * 8));
				value *= 2;
				if (i == (unsigned int)lastBit) {
					if (bit == 0)
						value--;
				} else {
					value += bit;
				}
			}
			distance = value;
			probabilities.word[modelIndex] = packed;
			if (distance >= (u32)(output - (u8 *)dest))
				return KL4E_BAD_DISTANCE;
		}
	}

	{
		u8 *copyEnd = output + length;
		const u8 *copySource = output - distance - 1;

		state = 6 | ((u32)copyEnd & 1);
		if ((u32)copyEnd > cacheLimit) {
			if ((u32)copyEnd >= (u32)outputEnd)
				return KL4E_OUTPUT_FULL;
		} else {
			__asm__ __volatile__("cache 0x18, 63(%0)" :: "r"(output) : "memory");
			if (distance >= 3) {
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
			*output++ = symbol;
			copySource++;
		}
		goto match;
	}
}

#undef DECODE_BIT
#undef NORMALIZE
