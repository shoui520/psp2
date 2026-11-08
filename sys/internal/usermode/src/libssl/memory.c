/* Private descriptive names in this file are ours, not recovered Sony names. */
#include "memory.h"
#include <libkernel/libkernel.h>

static void *sslDefaultReallocate(void *ptr, SceUInt32 size, SceUInt32 oldSize);

SslAllocCallback sslAllocCallback;
SslReallocCallback sslReallocCallback = sslDefaultReallocate;
SslFreeCallback sslFreeCallback;

void sslFree(void *ptr)
{
	sslFreeCallback(ptr);
}

void *sslAllocate(SceUInt32 size)
{
	return sslAllocCallback(size);
}

static void *sslDefaultReallocate(void *ptr, SceUInt32 size, SceUInt32 oldSize)
{
	(void)ptr;
	(void)size;
	(void)oldSize;
	return 0;
}

void *SceSslInternal_C1299FB6(SslAllocCallback alloc, SslReallocCallback realloc,
			      SslFreeCallback freeCallback)
{
	if (alloc && realloc && freeCallback) {
		sslAllocCallback = alloc;
		sslReallocCallback = realloc;
		sslFreeCallback = freeCallback;
		return &sslFreeCallback;
	}
	return (void *)alloc;
}

void *sslGetMemoryCallbacks(SslAllocCallback *alloc, SslReallocCallback *realloc,
			   SslFreeCallback *freeCallback)
{
	void *result = alloc;

	if (alloc)
		*alloc = sslAllocCallback;
	if (realloc) {
		result = (void *)sslReallocCallback;
		*realloc = sslReallocCallback;
	}
	if (freeCallback) {
		result = (void *)sslFreeCallback;
		*freeCallback = sslFreeCallback;
	}
	return result;
}

void *sslReallocate(void *ptr, SceUInt32 size, SceUInt32 oldSize)
{
	return sslReallocCallback(ptr, size, oldSize);
}

void *sslReplaceAllocation(void *ptr, SceUInt32 size)
{
	if (ptr != 0)
		sslFreeCallback(ptr);
	return sslAllocCallback(size);
}

void *sslWipeAndFreePointer(void **ptr, SceUInt32 size)
{
	void *result = *ptr;

	if (result != 0) {
		sslFillMemory(result, 0, size);
		sslFreeCallback(*ptr);
		*ptr = 0;
		return 0;
	}
	return result;
}

void *sslDuplicateMemory(const void *source, SceUInt32 size)
{
	void *copy = 0;

	if (source != 0 && size != 0) {
		copy = sslAllocCallback(size);
		if (copy != 0)
			sslCopyMemory(copy, source, size);
	}
	return copy;
}

int sslCompareBuffers(const void *left, const void *right,
		      SceUInt32 leftLength, SceUInt32 rightLength)
{
	if (leftLength == rightLength) {
		if (left == right)
			return 0;
		return sslCompareMemory(left, right, leftLength);
	}
	if (leftLength < rightLength)
		return -1;
	return 1;
}
