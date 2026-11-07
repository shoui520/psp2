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

void *sslCopyMemory(void *destination, const void *source, SceUInt32 size)
{
	return sceClibMemcpy_safe(destination, source, size);
}

void *sslFillMemory(void *destination, int value, SceUInt32 size)
{
	return sceClibMemset(destination, value, size);
}

int sslCompareMemory(const void *left, const void *right, SceUInt32 size)
{
	return sceClibMemcmp(left, right, size);
}

void *sslMoveMemory(void *destination, const void *source, SceUInt32 size)
{
	return sceClibMemmove(destination, source, size);
}
