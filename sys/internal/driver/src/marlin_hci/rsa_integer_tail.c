/* Descriptive source name: the original library has no debug records. */
#include <marlin_hci/rsa_arithmetic.h>

unsigned int MarlinSubtractIntegerWords(unsigned int *output,
	const unsigned int *first, const unsigned int *second, int count,
	unsigned int borrow)
{
	borrow &= 1;
	if (count > 0) {
		do {
			unsigned int left = *first++;
			unsigned int right = *second++;
			unsigned int difference = left - right - borrow;

			*output++ = difference;
			count--;
			borrow = ((right & ~left) |
				 ((right | ~left) & difference)) >> 31;
		} while (count != 0);
	}
	return borrow;
}

void MarlinShiftLeftIntegerWords(unsigned int *output,
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
		carry = 0;
		if (count > 0) {
			do {
				unsigned int word = *input++;

				count--;
				*output++ = (word << shift) | carry;
				carry = word >> (32 - shift);
			} while (count != 0);
		}
	}
}
