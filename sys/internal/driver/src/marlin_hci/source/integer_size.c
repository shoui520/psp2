/* Descriptive source name: the original library has no debug records. */
int MarlinIntegerByteLength(int count, const unsigned int *words)
{
	int wordIndex;
	int byteIndex;
	unsigned int mask;

	for (wordIndex = count - 1; wordIndex >= 0; wordIndex--) {
		mask = 0xff000000;
		for (byteIndex = 0; byteIndex <= 3;) {
			if ((words[wordIndex] & mask) != 0)
				return 4 * (wordIndex + 1) - byteIndex;
			byteIndex++;
			mask >>= 8;
		}
	}
	return 0;
}

