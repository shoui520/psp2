/* Descriptive source name: the original library has no debug records. */
#include <marlin_hci/rsa_arithmetic.h>

void MarlinSquareIntegerWords(unsigned int *output,
	const unsigned int *input, int count)
{
	unsigned int product[64];
	unsigned int diagonal[129];
	unsigned int crossProducts[128];
	unsigned int *cursor;
	const unsigned int *first;
	const unsigned int *second;
	int index;
	int remaining;
	int total;

	if (count <= 0 || count > 64)
		return;
	remaining = count - 1;
	cursor = diagonal;
	first = input;
	for (index = count; index > 0; index--) {
		unsigned int word = *first++;
		unsigned short low = word;
		unsigned int high = word >> 16;
		unsigned int lowSquare = low * low;
		unsigned int cross = low * high;
		unsigned int middle = cross + (lowSquare >> 16);

		cursor[0] = lowSquare + (cross << 17);
		cursor[1] = high * high + (middle >> 16)
			+ (((unsigned short)middle + cross) >> 16);
		cursor += 2;
	}
	total = 2 * count;
	cursor = crossProducts;
	index = total;
	if (index > 0) {
		do {
			index--;
			*cursor++ = 0;
		} while (index != 0);
	}
	cursor = crossProducts + 1;
	first = input;
	second = input + 1;
	if (remaining > 0) {
		do {
			MarlinMultiplyIntegerByWord(product, second, remaining, *first++);
			MarlinAddIntegerWords(cursor, cursor, product, remaining + 1, 0);
			remaining--;
			cursor += 2;
			second++;
		} while (remaining != 0);
	}
	MarlinShiftLeftIntegerWords(crossProducts + 1, crossProducts + 1,
				   total - 1, 1);
	MarlinAddIntegerWords(output, diagonal, crossProducts, total, 0);
}
