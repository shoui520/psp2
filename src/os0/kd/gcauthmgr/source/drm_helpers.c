/* Descriptive source name: the original module has no debug records. */
#include <stddef.h>

int GcAuthAesCbc(unsigned char *data, size_t size, const void *key,
		const unsigned char *iv, int decrypt);
int GcAuthGenerateRandom20(unsigned char *data);

int GcAuthAesEncrypt(unsigned char *data, size_t size, const void *key,
		const unsigned char *iv)
{
	return GcAuthAesCbc(data, size, key, iv, 0);
}

int GcAuthAesDecrypt(unsigned char *data, size_t size, const void *key,
		const unsigned char *iv)
{
	return GcAuthAesCbc(data, size, key, iv, 1);
}

int GcAuthFillRandomBytes(unsigned char *data, int size)
{
	unsigned char random[20];
	unsigned char *end;

	if (size > 0) {
		end = data + size;
		do {
			if (GcAuthGenerateRandom20(random) != 0)
				return -1;
			*data++ = random[0];
		} while (data != end);
	}
	return 0;
}
