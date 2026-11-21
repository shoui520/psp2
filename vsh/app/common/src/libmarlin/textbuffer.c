/*
 * A growing byte buffer, its capacity whole 4 KiB pages. The file's name is ours.
 */
#include "mln_internal.h"

#define MLN_TEXT_BUFFER_PAGE	4096

/* size rounded up to whole pages */
static inline __attribute__((always_inline)) unsigned int MlnTextBufferPages(unsigned int size)
{
	return (size + MLN_TEXT_BUFFER_PAGE - 1) >> 12 << 12;
}

/* append size bytes; 3 without a buffer */
int MlnTextBufferAppend(int heap, MlnTextBuffer *buffer, unsigned int size, const void *data)
{
	unsigned int used, total;
	unsigned char *grown;

	if (buffer == NULL)
		return 3;
	if (size == 0)
		return MLN_OK;
	used = buffer->size;
	total = size + buffer->size;
	if (total > buffer->capacity) {
		grown = MlnReallocate(heap, buffer->data, MlnTextBufferPages(total));
		if (grown == NULL)
			return MLN_ERROR_NO_MEMORY;
		buffer->data = grown;
		buffer->capacity = MlnTextBufferPages(total);
	}
	MlnMemcpy(buffer->data + used, data, size);
	buffer->size = total;
	return MLN_OK;
}

/* cut the buffer to size bytes; 3 when it holds fewer */
int MlnTextBufferSetSize(int heap, MlnTextBuffer *buffer, unsigned int size)
{
	if (buffer == NULL || buffer->size < size)
		return 3;
	buffer->size = size;
	return MLN_OK;
}

int MlnTextBufferCompare(int heap, const MlnTextBuffer *a, const MlnTextBuffer *b)
{
	int result;

	if (MlnCompareNull(a, b, &result) != 0)
		return result;
	return MlnBytesCompare(heap, a->size, a->data, b->size, b->data);
}

MlnTextBuffer *MlnTextBufferCreate(int heap, unsigned int size, const void *data)
{
	MlnTextBuffer *buffer = MlnAllocate(heap, sizeof(*buffer));
	unsigned int capacity;

	if (buffer == NULL)
		return NULL;
	buffer->size = size;
	capacity = size != 0 ? MlnTextBufferPages(size) : MLN_TEXT_BUFFER_PAGE;
	buffer->capacity = capacity;
	buffer->data = MlnAllocate(heap, capacity);
	if (buffer->data == NULL) {
		MlnFree(heap, buffer);
		return NULL;
	}
	if (data != NULL)
		MlnMemcpy(buffer->data, data, size);
	return buffer;
}

MlnTextBuffer *MlnTextBufferCreateFilled(int heap, unsigned int size, int value)
{
	MlnTextBuffer *buffer = MlnAllocate(heap, sizeof(*buffer));
	unsigned int capacity;

	if (buffer == NULL)
		return NULL;
	buffer->size = size;
	capacity = size != 0 ? MlnTextBufferPages(size) : MLN_TEXT_BUFFER_PAGE;
	buffer->capacity = capacity;
	buffer->data = MlnAllocate(heap, capacity);
	if (buffer->data == NULL) {
		MlnFree(heap, buffer);
		return NULL;
	}
	MlnMemset(buffer->data, value, size);
	return buffer;
}

void MlnTextBufferDestroy(int heap, MlnTextBuffer *buffer)
{
	if (buffer == NULL)
		return;
	if (buffer->data != NULL)
		MlnFree(heap, buffer->data);
	MlnFree(heap, buffer);
}
