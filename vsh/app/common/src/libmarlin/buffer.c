/*
 * Byte buffers: a size and the bytes, which follow the header when the buffer is
 * allocated whole, and counted arrays of words or pointers.
 */
#include "mln_internal.h"

#define MLN_BUFFER_SIZE_MAX	(0xffffffffu - sizeof(MlnBuffer))

MlnBuffer *MlnBufferInitInline(int heap, MlnBuffer *buffer, unsigned int size)
{
	if (buffer != NULL) {
		MlnMemset(buffer, 0, sizeof(*buffer));
		buffer->size = size;
		buffer->data = (unsigned char *)(buffer + 1);
	}
	return buffer;
}

MlnBuffer *MlnBufferCreate(int heap, unsigned int size, const void *data)
{
	MlnBuffer *buffer;

	if (size > MLN_BUFFER_SIZE_MAX)
		return NULL;
	buffer = MlnAllocate(heap, size + sizeof(*buffer));
	if (buffer == NULL)
		return NULL;
	MlnMemset(buffer, 0, sizeof(*buffer));
	buffer->size = size;
	buffer->data = (unsigned char *)(buffer + 1);
	if (data != NULL)
		MlnMemcpy(buffer->data, data, size);
	return buffer;
}

MlnBuffer *MlnBufferCreateFilled(int heap, unsigned int size, int value)
{
	MlnBuffer *buffer;

	if (size > MLN_BUFFER_SIZE_MAX)
		return NULL;
	buffer = MlnAllocate(heap, size + sizeof(*buffer));
	if (buffer == NULL)
		return NULL;
	MlnMemset(buffer, 0, sizeof(*buffer));
	buffer->size = size;
	buffer->data = (unsigned char *)(buffer + 1);
	MlnMemset(buffer->data, value, size);
	return buffer;
}

void MlnBufferDestroy(int heap, MlnBuffer *buffer)
{
	MlnFree(heap, buffer);
}

MlnBuffer *MlnBufferConcat(int heap, const MlnBuffer *a, const MlnBuffer *b)
{
	MlnBuffer *buffer;
	unsigned int size;

	if (a == NULL || b == NULL)
		return NULL;
	size = a->size + b->size;
	if (size > MLN_BUFFER_SIZE_MAX)
		return NULL;
	buffer = MlnAllocate(heap, size + sizeof(*buffer));
	if (buffer == NULL)
		return NULL;
	MlnMemset(buffer, 0, sizeof(*buffer));
	buffer->size = size;
	buffer->data = (unsigned char *)(buffer + 1);
	MlnMemcpy(buffer->data, a->data, a->size);
	MlnMemcpy(buffer->data + a->size, b->data, b->size);
	return buffer;
}

/* -2 or 2 when only one side is missing, else the order of the bytes, then of the sizes */
int MlnBytesCompare(int heap, unsigned int sizeA, const void *a, unsigned int sizeB,
		    const void *b)
{
	int aMissing = a == NULL;
	int bMissing = b == NULL;
	unsigned int size;
	int ret;

	if (bMissing | aMissing) {
		if (bMissing & aMissing)
			return 0;
		return aMissing ? -2 : 2;
	}
	size = sizeB < sizeA ? sizeB : sizeA;
	ret = MlnMemcmp(a, b, size);
	if (ret != 0)
		return ret;
	if (sizeA > size)
		return 1;
	if (sizeB > size)
		return -1;
	return 0;
}

/* 3 with the result when either pointer is missing, else 0 */
int MlnCompareNull(const void *a, const void *b, int *result)
{
	int aMissing = a == NULL;
	int bMissing = b == NULL;

	if (!(aMissing | bMissing))
		return 0;
	if (aMissing & bMissing) {
		*result = 0;
		return 3;
	}
	*result = aMissing ? -2 : 2;
	return 3;
}

int MlnBufferCompare(int heap, const MlnBuffer *a, const MlnBuffer *b)
{
	int aMissing, bMissing;

	aMissing = a == NULL;
	bMissing = b == NULL;
	if (bMissing | aMissing) {
		if (bMissing & aMissing)
			return 0;
		return aMissing ? -2 : 2;
	}
	return MlnBytesCompare(heap, a->size, a->data, b->size, b->data);
}

int MlnBufferCompareKey(int heap, const MlnBuffer *a, const MlnBuffer *b)
{
	int aMissing, bMissing;

	aMissing = a == NULL;
	bMissing = b == NULL;
	if (bMissing | aMissing) {
		if (bMissing & aMissing)
			return 0;
		return aMissing ? -2 : 2;
	}
	return MlnBytesCompare(heap, a->size, a->data, b->size, b->data);
}

MlnBuffer *MlnBufferClear(int heap, MlnBuffer *buffer)
{
	if (buffer != NULL)
		MlnMemset(buffer, 0, sizeof(*buffer));
	return buffer;
}

MlnBuffer *MlnBufferSet(int heap, MlnBuffer *buffer, unsigned int size, void *data)
{
	if (buffer != NULL) {
		buffer->size = size;
		buffer->data = data;
	}
	return buffer;
}

MlnBuffer *MlnBufferInitCopy(int heap, MlnBuffer *buffer, unsigned int size, const void *data)
{
	unsigned char *copy;

	if (buffer == NULL)
		return NULL;
	copy = MlnAllocate(heap, size);
	if (copy == NULL)
		return NULL;
	MlnMemcpy(copy, data, size);
	buffer->size = size;
	buffer->data = copy;
	return buffer;
}

void MlnBufferFreeData(int heap, MlnBuffer *buffer)
{
	if (buffer != NULL) {
		MlnFree(heap, buffer->data);
		buffer->data = NULL;
	}
}

int MlnBufferToString(int heap, const MlnBuffer *buffer, char **string)
{
	char *copy;

	if (buffer == NULL || buffer->size == 0 || buffer->data == NULL || string == NULL)
		return MLN_ERROR_INVALID;
	copy = MlnAllocate(heap, buffer->size + 1);
	if (copy == NULL)
		return MLN_ERROR_NO_MEMORY;
	MlnMemcpy(copy, buffer->data, buffer->size);
	copy[buffer->size] = '\0';
	*string = copy;
	return MLN_OK;
}

MlnArray *MlnWordArrayCreate(int heap, int count, const unsigned int *words)
{
	unsigned int *array;
	unsigned int size;
	int i;

	if (count < 1 || count > 0x3ffffffd)
		return NULL;
	size = (count + 1) * sizeof(unsigned int);
	array = MlnAllocate(heap, size);
	if (array == NULL)
		return NULL;
	MlnMemset(array, 0, size);
	array[0] = count;
	if (words != NULL) {
		for (i = 0; i < count; i++)
			array[1 + i] = words[i];
	}
	return (MlnArray *)array;
}

void MlnArrayDestroy(int heap, MlnArray *array)
{
	unsigned int i;

	if (array == NULL)
		return;
	for (i = 0; i < array->count; i++) {
		if (array->items[i] != NULL)
			MlnFree(heap, array->items[i]);
	}
	MlnFree(heap, array);
}
