#include <sysmem/sysmem.h>
#include "buffer.h"
#include "search.h"

SceUID marlinMemBlockId;
SceUID marlinHeapId;
int marlinMemoryReferences;

int MarlinMemoryInitialize(void)
{
	int result = 0;
	SceKernelHeapCreateOpt options;

	do {
		if (marlinMemoryReferences == 0) {
			if (marlinMemBlockId == 0) {
				marlinMemBlockId = sceKernelAllocMemBlockForKernel("SceMar", 0x1020d006,
									 0x10000, 0);
				if (marlinMemBlockId < 0) {
					result = marlinMemBlockId;
					if (result != 0)
						break;
				}
			}
			MarlinFillMemory(&options, 0, sizeof(options));
			options.size = sizeof(options);
			options.flags = HEAP_HAS_MEMBLOCK;
			options.memBlock = marlinMemBlockId;
			if (marlinHeapId == 0) {
				marlinHeapId = sceKernelCreateHeap("SceMar", 0x10000, &options);
				if (marlinHeapId < 0) {
					result = marlinHeapId;
					if (result != 0)
						break;
				}
			}
		}
		++marlinMemoryReferences;
	} while (0);
	return result;
}

void MarlinMemoryTerminate(void)
{
	if (marlinMemoryReferences == 1) {
		sceKernelDeleteHeap(marlinHeapId);
		marlinHeapId = 0;
		sceKernelFreeMemBlock(marlinMemBlockId);
		marlinMemBlockId = 0;
	}
	--marlinMemoryReferences;
	if (marlinMemoryReferences < 0)
		marlinMemoryReferences = 0;
	return;
}

int MarlinMemoryNoOp(void *allocator, void *memory, unsigned int size)
{
	int result = 0;

	if (allocator != 0)
		result = 0;
	if (memory != 0)
		result = 0;
	if (size != 0)
		result = 0;
	return result;
}

void *MarlinAllocateMemory(void *allocator, unsigned int size)
{
	int status = 0;
	void *memory = 0;

	if (allocator != 0)
		status = 0;
	if (size != 0)
		memory = sceKernelAllocHeapMemory(marlinHeapId, size);
	return memory;
}

void *MarlinReallocateMemory(void *allocator, void *memory, unsigned int size)
{
	void *replacement = 0;

	if (size != 0) {
		replacement = MarlinAllocateMemory(allocator, size);
		if (replacement != 0 && memory != 0)
			MarlinCopyMemory(replacement, memory, size);
	}
	if (memory != 0)
		MarlinFreeMemory(allocator, memory);
	return replacement;
}

void MarlinFreeMemory(void *allocator, void *memory)
{
	int status = 0;

	if (allocator != 0)
		status = 0;
	if (memory != 0)
		sceKernelFreeHeapMemory(marlinHeapId, memory);
	return;
}

void MarlinMemoryNoOpContext(void *allocator)
{
	int status = 0;

	if (allocator != 0)
		status = 0;
	return;
}

char *MarlinDuplicateString(void *allocator, const char *string)
{
	char *copy = 0;
	unsigned int size;

	if (string != 0) {
		size = MarlinStringLength(string) + 1;
		copy = MarlinAllocateMemory(allocator, size);
		if (copy != 0)
			MarlinCopyMemory(copy, string, size);
	}
	return copy;
}

void *MarlinMoveMemory(void *destination, const void *source, unsigned int size)
{
	unsigned char *write = destination;
	const unsigned char *read = source;

	if (write < read) {
		while (size-- != 0) {
			*write = *read;
			++write;
			++read;
		}
	} else {
		while (size-- != 0)
			write[size] = read[size];
	}
	return destination;
}

void *MarlinCopyMemory(void *destination, const void *source, unsigned int size)
{
	unsigned char *write = destination;
	const unsigned char *read = source;

	while (size-- != 0) {
		*write = *read;
		++write;
		++read;
	}
	return destination;
}

int MarlinCompareMemory(const void *first, const void *second, unsigned int size)
{
	const signed char *firstBytes = first;
	const signed char *secondBytes = second;
	unsigned int index;

	for (index = 0; index < size; ++index) {
		if (firstBytes[index] != secondBytes[index])
			return firstBytes[index] < secondBytes[index] ? -1 : 1;
	}
	return 0;
}

void *MarlinFillMemory(void *destination, int value, unsigned int size)
{
	unsigned char *write = destination;

	while (size-- != 0) {
		*write = (unsigned char)value;
		++write;
	}
	return destination;
}
