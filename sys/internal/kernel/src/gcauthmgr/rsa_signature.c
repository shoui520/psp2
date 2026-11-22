/* Descriptive source name: the original library has no debug records. */
#include <gcauthmgr/rsa_signature.h>

unsigned int GcAuthRsaSignBlock(MarlinRsaData *output, const MarlinRsaData *digest,
		     const struct MarlinRsaPrivateKey *key, int count,
		     int size, int algorithm)
{
	unsigned int block[64];
	unsigned char *front;
	unsigned char *back;
	unsigned int *first;
	unsigned int *last;
	int half;
	int lastByteOffset;
	int remaining;
	int result;

	if (GcAuthRsaEncodeDigest((unsigned char *)block, size, digest,
				algorithm) == -1)
		return -1;
	lastByteOffset = 4 * (count - 1);
	front = (unsigned char *)block;
	half = count / 2;
	result = 0;
	back = front + lastByteOffset;
	if (half > 0) {
		last = block + count;
		first = block;
		remaining = half;
		do {
			unsigned int high = back[3] | (back[2] << 8) |
				(back[1] << 16) | ((unsigned int)back[0] << 24);
			unsigned int low = front[3] | (front[2] << 8) |
				(front[1] << 16) | ((unsigned int)front[0] << 24);

			back -= 4;
			front += 4;
			*first++ = high;
			--remaining;
			*--last = low;
		} while (remaining != 0);
	}
	if (MarlinRsaPrivateOperation(block, block, key, count) == -1)
		return -1;
	front = output->data;
	back = front + lastByteOffset;
	if (half > 0) {
		last = block + count;
		first = block;
		remaining = half;
		do {
			unsigned int high = *--last;
			unsigned int low = *first++;

			front[3] = high;
			front[0] = high >> 24;
			front[1] = high >> 16;
			front[2] = high >> 8;
			back[0] = low >> 24;
			--remaining;
			back[1] = low >> 16;
			front += 4;
			back[2] = low >> 8;
			back[3] = low;
			back -= 4;
		} while (remaining != 0);
	}
	output->size = size;
	return result;
}

unsigned int GcAuthRsaSign512(MarlinRsaData *output,
			    const MarlinRsaData *digest,
			    const struct MarlinRsaPrivateKey *key, int algorithm)
{
	return GcAuthRsaSignBlock(output, digest, key, 16, 64, algorithm);
}

unsigned int GcAuthRsaSign1024(MarlinRsaData *output,
			     const MarlinRsaData *digest,
			     const struct MarlinRsaPrivateKey *key, int algorithm)
{
	return GcAuthRsaSignBlock(output, digest, key, 32, 128, algorithm);
}

unsigned int GcAuthRsaSign2048(MarlinRsaData *output,
			     const MarlinRsaData *digest,
			     const struct MarlinRsaPrivateKey *key, int algorithm)
{
	return GcAuthRsaSignBlock(output, digest, key, 64, 256, algorithm);
}
