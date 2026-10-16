/* Descriptive source name: the original library has no debug records. */
#include "buffer.h"

int MarlinSha1Digest(const void *data, unsigned int size, void *digest);

int MarlinGenerateSha1Mask(const void *seed, unsigned int seedSize,
			   int maskSize, unsigned char *mask)
{
	int result = 0;
	unsigned char *scratch = 0;
	unsigned int index = 0;
	unsigned char *counter;
	unsigned char digest[20];

	scratch = MarlinAllocateMemory(0, seedSize + 4);
	if (scratch == 0) {
		result = 2;
		goto out;
	}
	MarlinCopyMemory(scratch, seed, seedSize);
	for (index = 0; index < (maskSize + 19) / 20; index++) {
		counter = scratch + seedSize;
		counter[0] = index >> 24;
		counter[1] = index >> 16;
		counter[2] = index >> 8;
		counter[3] = index;
		result = MarlinSha1Digest(scratch, seedSize + 4, digest);
		if (result != 0) {
			result = 0x2206000c;
			goto out;
		}
		MarlinCopyMemory(mask + 20 * index, digest,
				 maskSize - 20 * index);
	}
out:
	MarlinFreeMemory(0, scratch);
	return result;
}

