/* Descriptive source name: the original library has no debug records. */
int MarlinEncodeIntegerWords(unsigned char *data, int size,
			     const unsigned int *words, int count)
{
	unsigned char *end;
	int capacity = 4 * count;
	int shift;

	words += (unsigned int)capacity / 4;
	if (size > capacity)
		size = capacity;
	shift = 0;
	end = data + size;
	while (data < end) {
		if (shift == 0) {
			shift = 32;
			words--;
		}
		shift -= 8;
		*data++ = *words >> shift;
	}
	return 0;
}
