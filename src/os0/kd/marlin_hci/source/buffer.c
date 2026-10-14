#include "buffer.h"

MarlinBuffer *MarlinBufferInitialize(void *allocator, MarlinBuffer *buffer,
				   unsigned int size)
{
	if (buffer != 0) {
		MarlinFillMemory(buffer, 0, sizeof(*buffer));
		buffer->size = size;
		buffer->data = buffer->payload;
	}
	return buffer;
}

MarlinBuffer *MarlinBufferCreate(void *allocator, unsigned int size,
				const void *data)
{
	MarlinBuffer *buffer;

	if (size > 0xfffffff7U)
		return 0;
	buffer = MarlinAllocateMemory(allocator, size + sizeof(*buffer));
	if (buffer == 0)
		return 0;
	MarlinBufferInitialize(allocator, buffer, size);
	if (data != 0)
		MarlinCopyMemory(buffer->data, data, size);
	return buffer;
}

MarlinBuffer *MarlinBufferCreateFilled(void *allocator, unsigned int size,
				      unsigned char value)
{
	MarlinBuffer *buffer;

	buffer = MarlinBufferCreate(allocator, size, 0);
	if (buffer != 0)
		MarlinFillMemory(buffer->data, value, size);
	return buffer;
}

void MarlinBufferDestroy(void *allocator, MarlinBuffer *buffer)
{
	MarlinFreeMemory(allocator, buffer);
}

MarlinBuffer *MarlinBufferConcatenate(void *allocator, const MarlinBuffer *first,
				     const MarlinBuffer *second)
{
	MarlinBuffer *buffer;

	if (first == 0 || second == 0)
		return 0;
	buffer = MarlinBufferCreate(allocator, first->size + second->size, 0);
	if (buffer == 0)
		return 0;
	MarlinCopyMemory(buffer->data, first->data, first->size);
	MarlinCopyMemory(buffer->data + first->size, second->data, second->size);
	return buffer;
}

int MarlinCompareDataRanges(void *allocator, unsigned int firstSize,
			    const void *first, unsigned int secondSize,
			    const void *second)
{
	unsigned int commonSize;
	int comparison;
	int nullComparison;

	if (MarlinCompareNullPointers(first, second, &nullComparison) == 3)
		return nullComparison;
	commonSize = firstSize < secondSize ? firstSize : secondSize;
	comparison = MarlinCompareMemory(first, second, commonSize);
	if (comparison != 0)
		return comparison;
	if (firstSize > commonSize)
		return 1;
	if (secondSize > commonSize)
		return -1;
	return 0;
}

int MarlinCompareNullPointers(const void *first, const void *second, int *result)
{
	int status = 0;

	if (first == 0 || second == 0) {
		status = 3;
		if (first == 0 && second == 0)
			*result = 0;
		else if (first == 0)
			*result = -2;
		else
			*result = 2;
	}
	return status;
}

int MarlinBufferCompare(void *allocator, const MarlinBuffer *first,
			const MarlinBuffer *second)
{
	int result;

	if (MarlinCompareNullPointers(first, second, &result) == 3)
		return result;
	return MarlinCompareDataRanges(allocator, first->size, first->data,
				       second->size, second->data);
}

MarlinBuffer *MarlinBufferInitializeExternal(void *allocator, MarlinBuffer *buffer,
					   unsigned int size, void *data)
{
	if (buffer == 0)
		return 0;
	buffer->size = size;
	buffer->data = data;
	return buffer;
}

MarlinBuffer *MarlinBufferCopyInto(void *allocator, MarlinBuffer *buffer,
				 unsigned int size, const void *source)
{
	void *data;

	if (buffer == 0)
		return 0;
	data = MarlinAllocateMemory(allocator, size);
	if (data == 0)
		return 0;
	MarlinCopyMemory(data, source, size);
	buffer->size = size;
	buffer->data = data;
	return buffer;
}

void MarlinBufferReleaseData(void *allocator, MarlinBuffer *buffer)
{
	if (buffer != 0) {
		MarlinFreeMemory(allocator, buffer->data);
		buffer->data = 0;
	}
}
