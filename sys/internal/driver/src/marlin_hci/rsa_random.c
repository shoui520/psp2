/* Descriptive source name: the original library has no debug records. */
#include <marlin_hci/rsa.h>

int MarlinRsaDefaultRandom(unsigned char *data, int size);

int (*randomGenerator)(unsigned char *, int) = MarlinRsaDefaultRandom;

int MarlinRsaHasRandomGenerator(void)
{
	if (randomGenerator == MarlinRsaDefaultRandom)
		return 0;
	return 1;
}

void MarlinRsaGenerateRandom(unsigned char *data, int size)
{
	if (randomGenerator != MarlinRsaDefaultRandom)
		randomGenerator(data, size);
}

void MarlinRsaFillNonzeroRandom(unsigned char *data, int size)
{
	unsigned char bytes[20];
	unsigned char *cursor;
	unsigned char *end;

	if (randomGenerator != MarlinRsaDefaultRandom) {
		cursor = bytes + sizeof(bytes);
		end = data + size;
		while (data < end) {
			if (cursor == bytes + sizeof(bytes)) {
				cursor = bytes;
				randomGenerator(bytes, sizeof(bytes));
			}
			if (*cursor != 0)
				*data++ = *cursor;
			cursor++;
		}
	}
}

int MarlinRsaSetRandomGenerator(int (*generator)(unsigned char *data, int size))
{
	randomGenerator = generator;
	return 0;
}

int MarlinRsaDefaultRandom(unsigned char *data, int size)
{
	return 0;
}
