/* Private descriptive names in this interface are ours. */
#ifndef LIBSSL_MEMORY_H
#define LIBSSL_MEMORY_H

#include <scetypes.h>

typedef void *(*SslAllocCallback)(SceUInt32 size);
typedef void *(*SslReallocCallback)(void *ptr, SceUInt32 size, SceUInt32 oldSize);
typedef void (*SslFreeCallback)(void *ptr);

extern SslAllocCallback sslAllocCallback;
extern SslReallocCallback sslReallocCallback;
extern SslFreeCallback sslFreeCallback;

void *SceSslInternal_C1299FB6(SslAllocCallback alloc, SslReallocCallback realloc,
			      SslFreeCallback freeCallback);
void *sslAllocate(SceUInt32 size);
void *sslReallocate(void *ptr, SceUInt32 size, SceUInt32 oldSize);
void sslFree(void *ptr);
void *sslCopyMemory(void *destination, const void *source, SceUInt32 size);
void *sslFillMemory(void *destination, int value, SceUInt32 size);
int sslCompareMemory(const void *left, const void *right, SceUInt32 size);
void *sslMoveMemory(void *destination, const void *source, SceUInt32 size);
SceUInt32 sslStringLength(const char *string);
void *sslGetMemoryCallbacks(SslAllocCallback *alloc, SslReallocCallback *realloc,
			   SslFreeCallback *freeCallback);
void *sslReplaceAllocation(void *ptr, SceUInt32 size);
void *sslWipeAndFreePointer(void **ptr, SceUInt32 size);
void *sslDuplicateMemory(const void *source, SceUInt32 size);
int sslCompareBuffers(const void *left, const void *right,
		      SceUInt32 leftLength, SceUInt32 rightLength);

#endif
