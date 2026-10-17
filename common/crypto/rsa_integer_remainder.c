/* Descriptive source name: the original library has no debug records. */
#include <marlin_hci/rsa_arithmetic.h>

void MarlinRemainderIntegerWords(unsigned int *output,
	const unsigned int *input, int inputCount,
	const unsigned int *modulus, int modulusCount)
{
	unsigned int normalizedModulus[65];
	unsigned int product[66];
	unsigned int remainder[129];
	unsigned int inputZeros;
	unsigned int modulusZeros;
	unsigned int shift;
	unsigned int top;
	int zeros;
	int count;
	int usedInput;
	int less;
	int index;

	if (inputCount <= 0 || inputCount > 128 ||
	    modulusCount <= 0 || modulusCount > 64)
		return;
	inputZeros = 0;
	zeros = 32;
	for (index = inputCount - 1; index >= 0 && zeros == 32; index--) {
		unsigned int word = input[index];
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
		zeros = 16 - upperHalf + upperByte + upperNibble + upperPair
			- ((word >> 14) & ~(word >> 15)) + 2;
		inputZeros += zeros;
	}
	modulusZeros = 0;
	zeros = 32;
	for (index = modulusCount - 1; index >= 0 && zeros == 32; index--) {
		unsigned int word = modulus[index];
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
		zeros = 16 - upperHalf + upperByte + upperNibble + upperPair
			- ((word >> 14) & ~(word >> 15)) + 2;
		modulusZeros += zeros;
	}
	count = modulusCount - (modulusZeros >> 5);
	shift = modulusZeros & 31;
	if (count == 0)
		return;
	usedInput = inputCount - (inputZeros >> 5);
	if (count == usedInput)
		less = MarlinIntegerWordsGreater(modulus, input, count);
	else
		less = count > usedInput;
	if (less == 1) {
		for (index = 0; index < usedInput; index++)
			output[index] = input[index];
		for (index = usedInput; index < modulusCount; index++)
			output[index] = 0;
		return;
	}
	normalizedModulus[count] = 0;
	MarlinShiftLeftIntegerWords(normalizedModulus, modulus, count, shift);
	remainder[usedInput] = 0;
	if ((inputZeros & 31) < shift)
		remainder[usedInput] = input[usedInput - 1] >> (32 - shift);
	MarlinShiftLeftIntegerWords(remainder, input, usedInput, shift);
	top = normalizedModulus[count - 1];
	for (index = usedInput; index >= count; index--) {
		unsigned int high = remainder[index];
		unsigned int quotient;
		unsigned int borrow;
		unsigned int *window = remainder + index - count;

		if (high < top) {
			unsigned int low = remainder[index - 1];
			int bit;

			for (bit = 0; bit < 32; bit++) {
				unsigned int previous = high;

				high = (low >> 31) | (high * 2);
				low *= 2;
				if ((high | (unsigned int)((int)previous >> 31)) >= top) {
					high -= top;
					low |= 1;
				}
			}
			quotient = low;
		} else {
			quotient = -1;
		}
		MarlinMultiplyIntegerByWord(product, normalizedModulus, count, quotient);
		borrow = MarlinSubtractIntegerWords(window, window, product, count + 1, 0);
		while (borrow != 0)
			borrow ^= MarlinAddIntegerWords(window, window,
						       normalizedModulus, count + 1, 0);
	}
	MarlinShiftRightIntegerWords(output, remainder, modulusCount, shift);
	for (index = count; index < modulusCount; index++)
		output[index] = 0;
}

void MarlinShiftRightIntegerWords(unsigned int *output,
	const unsigned int *input, int count, unsigned int shift)
{
	unsigned int carry;

	shift &= 63;
	if (shift == 0) {
		if (count > 0) {
			do {
				unsigned int word = *input++;

				count--;
				*output++ = word;
			} while (count != 0);
		}
	} else {
		count--;
		carry = 0;
		if (count >= 0) {
			input += count;
			output += count;
			do {
				unsigned int word = *input--;

				count--;
				*output-- = (word >> shift) | carry;
				carry = word << (32 - shift);
			} while (count >= 0);
		}
	}
}
