/*
 * MGF1, the mask generation function OAEP uses (PKCS #1 v2).
 */
#include "rsa_oaep.h"

/* MGF1: the hashes of the seed and a big-endian counter, end to end */
void MarlinRsaMgf1(unsigned char *mask, int maskSize, const unsigned char *seed, int seedSize,
		   const MarlinRsaHash *hash)
{
	unsigned char digest[256];
	unsigned char input[260];
	unsigned char *digestEnd;
	unsigned char *cursor;
	unsigned char *end;
	unsigned int counter;
	int hashSize;
	int i;

	if ((maskSize | seedSize) < 0)
		return;
	hashSize = hash->size;
	if (hashSize < 0 || seedSize > 255 || hashSize > 254)
		return;
	cursor = digest + hashSize;
	digestEnd = cursor;
	end = mask + maskSize;
	for (i = 0; i < seedSize; i++)
		input[i] = seed[i];
	counter = 0;
	while (mask < end) {
		if (cursor == digestEnd) {
			input[seedSize + 3] = counter;
			input[seedSize] = counter >> 24;
			input[seedSize + 1] = counter >> 16;
			input[seedSize + 2] = counter >> 8;
			cursor = digest;
			hash->digest(digest, input, seedSize + 4);
			counter++;
		}
		*mask++ = *cursor++;
	}
}
