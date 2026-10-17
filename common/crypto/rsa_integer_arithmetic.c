/* Descriptive source name: the original library has no debug records. */
#include <marlin_hci/rsa_arithmetic.h>

unsigned int MarlinAddIntegerWords(unsigned int *output,
				 const unsigned int *first,
				 const unsigned int *second,
				 int count, unsigned int carry)
{
	carry &= 1;
	if (count > 0) {
		do {
			unsigned int left = *first++;
			unsigned int right = *second++;
			unsigned int sum = left + right + carry;

			carry = left | right;
			*output++ = sum;
			count--;
			carry = ((left & right) | (carry & ~sum)) >> 31;
		} while (count != 0);
	}
	return carry;
}

int MarlinIntegerWordsGreater(const unsigned int *first,
			     const unsigned int *second, int count)
{
	const unsigned int *end = first + count;
	int result = 0;

	second += count;
	while (end > first && *--end == *--second)
		;
	if (count > 0) {
		if (*end > *second)
			result = 1;
	}
	return result;
}
