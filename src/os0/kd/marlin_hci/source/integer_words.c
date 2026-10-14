/* Descriptive source name: the original library has no debug records. */
int MarlinDecodeIntegerWords(unsigned int *words, int count,
			     const unsigned char *data, int size)
{
	int capacity = count * 4;
	unsigned int *word = words - 1;
	unsigned int *end = words + count;
	const unsigned char *cursor = data + size;
	int shift;

	if (size > capacity)
		data = cursor - capacity;
	for (shift = 32; cursor > data; shift += 8) {
		if (shift == 32) {
			word = words++;
			*word = 0;
			shift = 0;
		}
		*word |= *--cursor << shift;
	}
	for (; words < end; words++)
		*words = 0;
	return 0;
}
