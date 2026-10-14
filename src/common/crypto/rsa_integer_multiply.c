/* Descriptive source name: the original library has no debug records. */
#include <sce/rsa_arithmetic.h>

void MarlinMultiplyIntegerWords(unsigned int *output,
	const unsigned int *first, int firstCount,
	const unsigned int *second, int secondCount)
{
	unsigned int product[65];
	unsigned int accumulator[128];
	unsigned int *cursor;
	unsigned int *copy;
	int index;
	int total;
	int productCount;
	int wordCount;
	unsigned int carry;

	if (firstCount > 0 && firstCount <= 64 &&
	    secondCount > 0 && secondCount <= 64) {
		productCount = firstCount + 1;
		wordCount = firstCount;
		total = firstCount + secondCount;
		carry = 0;
		cursor = accumulator;
		copy = accumulator;
		index = total;
		if (index > 0) {
			do {
				index--;
				*copy++ = carry;
			} while (index != 0);
		}
		index = secondCount;
		if (index > 0) {
			do {
				MarlinMultiplyIntegerByWord(product, first,
							    wordCount, *second++);
				MarlinAddIntegerWords(cursor, cursor, product,
						      productCount, carry);
				index--;
				cursor++;
			} while (index != 0);
		}
		copy = accumulator;
		index = total;
		if (index > 0) {
			do {
				index--;
				*output++ = *copy++;
			} while (index != 0);
		}
	}
}

void MarlinMultiplyIntegerByWord(unsigned int *output,
	const unsigned int *input, int count, unsigned int factor)
{
	unsigned int *cursor = output;
	unsigned int carry;
	unsigned int lowFactor;
	unsigned int highFactor;
	int index;

	if (count <= 0)
		return;
	carry = 0;
	index = 0;
	lowFactor = factor & 0xffff;
	highFactor = factor >> 16;
	for (index = 0; index < count; index++) {
		unsigned int word = *input++;
		unsigned short lowWord = word;
		unsigned int lowProduct = lowWord * lowFactor;
		unsigned int crossFirst = (word >> 16) * lowFactor;
		unsigned int crossSecond = lowWord * highFactor;
		unsigned int highProduct = (word >> 16) * highFactor;
		unsigned int middle = crossFirst + (lowProduct >> 16);
		unsigned int low = lowProduct + (crossFirst << 16)
			+ (crossSecond << 16);
		unsigned int sum = low + carry;

		*cursor++ = sum;
		carry = highProduct + (middle >> 16)
			+ ((crossSecond + (middle & 0xffff)) >> 16)
			+ (((low & carry) | ((low | carry) & ~sum)) >> 31);
	}
	output[index] = carry;
}
