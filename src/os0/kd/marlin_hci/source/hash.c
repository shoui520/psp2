/* Descriptive source name: the original library has no debug records. */
#include "buffer.h"
#include <psp2kern/kernel/utils.h>

int MarlinHmacDigest(unsigned int algorithm, const MarlinBuffer *key,
		     const MarlinBuffer *data, MarlinBuffer **digest)
{
	int result;
	MarlinBuffer *buffer = 0;
	unsigned int size;

	switch (algorithm) {
	case 1:
		size = 20;
		buffer = MarlinBufferCreate(0, size, 0);
		if (buffer == 0) {
			result = 2;
			goto out;
		}
		ksceHmacSha1Digest(key->data, key->size, data->data,
				  data->size, buffer->data);
		break;
	case 2:
		size = 32;
		buffer = MarlinBufferCreate(0, size, 0);
		if (buffer == 0) {
			result = 2;
			goto out;
		}
		ksceHmacSha256Digest(key->data, key->size, data->data,
				    data->size, buffer->data);
		break;
	default:
		result = 3;
		goto out;
	}
	*digest = buffer;
	buffer = 0;
	result = 0;
out:
	if (buffer != 0)
		MarlinBufferDestroy(0, buffer);
	return result;
}
