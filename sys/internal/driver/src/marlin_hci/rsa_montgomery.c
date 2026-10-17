/* Descriptive source name: the original library has no debug records. */
#include <marlin_hci/rsa_arithmetic.h>

void MarlinMontgomeryMultiply(unsigned int *output, const unsigned int *first,
	const unsigned int *second, const unsigned int *modulus, int count,
	unsigned int inverse)
{
	unsigned int accumulator[65];
	unsigned int product[65];
	unsigned int correction[65];
	unsigned int carry;
	unsigned int highCarry;
	const unsigned int *firstCursor;
	int index;

	if (count <= 0 || count > 64)
		return;
	for (index = 0; index < count + 1; index++)
		accumulator[index] = 0;
	firstCursor = first;
	for (index = 0; index < count; index++) {
		MarlinMultiplyIntegerByWord(correction, modulus, count,
			(accumulator[0] + *firstCursor * *second) * inverse);
		MarlinMultiplyIntegerByWord(product, second, count, *firstCursor);
		carry = MarlinAddIntegerWords(correction, correction, product,
					    count + 1, 0);
		highCarry = MarlinAddIntegerWords(accumulator, accumulator + 1,
			correction + 1, count,
			(correction[0] | accumulator[0]) != 0);
		accumulator[count] = carry + highCarry;
		firstCursor++;
	}
	if (carry + highCarry != 0 ||
	    !MarlinIntegerWordsGreater(modulus, accumulator, count))
		MarlinSubtractIntegerWords(accumulator, accumulator, modulus,
					 count, 0);
	{
		unsigned int *write = output;
		const unsigned int *read = accumulator;
		int words = count;

		do {
			unsigned int word = *read++;

			--words;
			*write++ = word;
		} while (words != 0);
	}
}

void MarlinMontgomeryPower(unsigned int *output, const unsigned int *input,
	const unsigned int *exponent, const unsigned int *modulus, int count,
	unsigned int inverse)
{
	unsigned int shiftedExponent[64];
	unsigned int accumulator[65];
	unsigned int square[128];
	unsigned int top;
	unsigned int carry;
	unsigned int leadingZeros;
	unsigned int shift;
	unsigned int *cursor;
	int usedWords;
	int squareCount;
	int zeros;
	int index;

	if (count <= 0 || count > 64)
		return;
	squareCount = 2 * count;
	top = modulus[count - 1];
	carry = 0;
	for (index = 0; index < count; index++) {
		unsigned int word = modulus[index];
		unsigned int negative = -word - carry;

		accumulator[index] = negative;
		carry = (word | negative) >> 31;
	}
	if ((top & 0x80000000) == 0)
		MarlinRemainderIntegerWords(accumulator, accumulator, count,
					  modulus, count);
	leadingZeros = 0;
	zeros = 32;
	for (index = count - 1; index >= 0 && zeros == 32; index--) {
		unsigned int word = exponent[index];
		int upperHalf = ((int)-(word >> 16) >> 16) & 16;
		int upperByte;
		int upperNibble;
		int upperPair;

		word >>= upperHalf;
		upperByte = ((int)(word - 256) >> 16) & 8;
		word <<= upperByte;
		upperNibble = ((int)(word - 4096) >> 16) & 4;
		word <<= upperNibble;
		upperPair = ((int)(word - 0x4000) >> 16) & 2;
		word <<= upperPair;
		zeros = 16 - upperHalf;
		zeros += upperByte;
		zeros += upperNibble;
		zeros += upperPair;
		zeros -= (word >> 14) & ~(word >> 15);
		zeros += 2;
		leadingZeros += zeros;
	}
	usedWords = count - (leadingZeros >> 5);
	shift = leadingZeros & 31;
	MarlinShiftLeftIntegerWords(shiftedExponent, exponent, usedWords, shift);
	cursor = shiftedExponent;
	while (usedWords > 0) {
		unsigned int bit = 0;

		for (index = 0; index < usedWords; index++) {
			unsigned int word = cursor[index];

			cursor[index] = (bit & 1) | (word * 2);
			bit = word >> 31;
		}
		shift++;
		if (shift >= 32) {
			usedWords--;
			cursor++;
			shift &= 31;
		}
		MarlinSquareIntegerWords(square, accumulator, count);
		MarlinMontgomeryReduce(accumulator, square, squareCount,
				       modulus, count, inverse);
		if (bit != 0)
			MarlinMontgomeryMultiply(accumulator, accumulator, input,
					 modulus, count, inverse);
	}
	for (index = 0; index < count; index++)
		*output++ = accumulator[index];
}

void MarlinMontgomeryReduce(unsigned int *output, const unsigned int *input,
	int inputCount, const unsigned int *modulus, int count,
	unsigned int inverse)
{
	unsigned int carries[64];
	unsigned int product[65];
	unsigned int accumulator[128];
	unsigned int *high;
	unsigned int carry;
	int index;

	if (count <= 0 || count > 64 ||
	    inputCount < count || inputCount > 2 * count)
		return;
	high = accumulator + count;
	for (index = 0; index < count; index++) {
		accumulator[index] = input[index];
		high[index] = 0;
	}
	for (index = count; index < inputCount; index++)
		accumulator[index] = input[index];
	for (index = 0; index < count; index++) {
		MarlinMultiplyIntegerByWord(product, modulus, count,
					    accumulator[index] * inverse);
		carry = MarlinAddIntegerWords(accumulator + index,
			accumulator + index, product, count, 0);
		carries[index] = product[count] + carry;
	}
	carry = MarlinAddIntegerWords(high, high, carries, count, 0);
	if (carry != 0 || !MarlinIntegerWordsGreater(modulus, high, count))
		MarlinSubtractIntegerWords(high, high, modulus, count, 0);
	for (index = 0; index < count; index++)
		*output++ = high[index];
}
