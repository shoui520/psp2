/* Descriptive source name: the original library has no debug records. */
#include "buffer.h"

int MarlinDecodeIntegerWords(unsigned int *words, int count,
			     const unsigned char *data, int size);
int MarlinEncodeIntegerWords(unsigned char *data, int size,
			     const unsigned int *words, int count);

int MarlinCreateIntegerWords(const MarlinBuffer *buffer, int count,
			    unsigned int **words)
{
	int result = 0;
	unsigned char *data;
	int size;
	unsigned int *memory = 0;

	if (buffer != 0) {
		data = buffer->data;
		size = buffer->size;
		while (size > 0 && *data == 0) {
			size--;
			data++;
		}
		memory = MarlinAllocateMemory(0, 4 * count);
		if (memory == 0) {
			result = 2;
			goto out;
		}
		MarlinDecodeIntegerWords(memory, count, data, size);
		*words = memory;
		memory = 0;
	} else {
		*words = 0;
	}
out:
	if (memory != 0)
		MarlinFreeMemory(0, memory);
	return result;
}

int MarlinCreateIntegerBuffer(int count, const unsigned int *words,
			     int size, MarlinBuffer **output)
{
	int result = 0;
	MarlinBuffer *buffer = 0;
	unsigned char *data;
	int remaining;
	unsigned char bytes[256];

	if (words != 0) {
		MarlinEncodeIntegerWords(bytes, 256, words, count);
		data = bytes;
		for (remaining = 4 * count;
		     remaining > 0 && *data == 0; remaining--)
			data++;
		if (remaining > size) {
			result = 0x22060008;
			goto out;
		}
		buffer = MarlinBufferCreateFilled(0, size, 0);
		if (buffer == 0) {
			result = 2;
			goto out;
		}
		MarlinCopyMemory(buffer->data + (size - remaining), data,
				 remaining);
		*output = buffer;
		buffer = 0;
	} else {
		*output = 0;
	}
out:
	if (buffer != 0)
		MarlinBufferDestroy(0, buffer);
	return result;
}
