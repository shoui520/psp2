/* Descriptive source name: the original library has no debug records. */
#include <gcauthmgr/rsa_signature.h>

int GcAuthRsaVerifyBlock(const MarlinRsaData *signature,
		       const MarlinRsaData *digest,
		       const MarlinRsaExponentKey *key, int count,
		       int size, int algorithm)
{
	unsigned int decoded[64];
	unsigned int encoded[64];
	unsigned char *front;
	unsigned char *back;
	unsigned int *first;
	unsigned int *last;
	int half;
	int lastByteOffset;
	int remaining;
	int index;
	int result;

	if (signature->size != size)
		return -1;
	front = signature->data;
	lastByteOffset = 4 * (count - 1);
	half = count / 2;
	result = 0;
	back = front + lastByteOffset;
	if (half > 0) {
		first = encoded;
		last = encoded + count;
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
	if (MarlinRsaExponentOperation(decoded, encoded, key, count) == -1)
		return -1;
	front = (unsigned char *)decoded;
	back = front + lastByteOffset;
	if (half > 0) {
		last = decoded + count;
		first = decoded;
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
	if (GcAuthRsaEncodeDigest((unsigned char *)encoded, size, digest,
				algorithm) == -1)
		return -1;
	first = decoded;
	index = result;
	if (count <= 0)
		return -1;
	do {
		if (*first != encoded[index])
			break;
		++index;
		++first;
	} while (index < count);
	if (count <= 0 || count != index)
		return -1;
	return result;
}

int GcAuthRsaVerify512(const MarlinRsaData *signature,
		     const MarlinRsaData *digest,
		     const MarlinRsaExponentKey *key, int algorithm)
{
	return GcAuthRsaVerifyBlock(signature, digest, key, 16, 64, algorithm);
}

int GcAuthRsaVerify1024(const MarlinRsaData *signature,
		      const MarlinRsaData *digest,
		      const MarlinRsaExponentKey *key, int algorithm)
{
	return GcAuthRsaVerifyBlock(signature, digest, key, 32, 128, algorithm);
}

int GcAuthRsaVerify2048(const MarlinRsaData *signature,
		      const MarlinRsaData *digest,
		      const MarlinRsaExponentKey *key, int algorithm)
{
	return GcAuthRsaVerifyBlock(signature, digest, key, 64, 256, algorithm);
}
