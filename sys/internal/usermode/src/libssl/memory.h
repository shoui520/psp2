/* Private descriptive names in this interface are ours. */
#ifndef LIBSSL_MEMORY_H
#define LIBSSL_MEMORY_H

#include <scetypes.h>

typedef void *(*SslAllocCallback)(SceUInt32 size);
typedef void *(*SslReallocCallback)(void *ptr, SceUInt32 size, SceUInt32 oldSize);
typedef void (*SslFreeCallback)(void *ptr);

void *sslAllocate(SceUInt32 size);
void *sslReallocate(void *ptr, SceUInt32 size, SceUInt32 oldSize);
void sslFree(void *ptr);
void *sslCopyMemory(void *destination, const void *source, SceUInt32 size);
void *sslFillMemory(void *destination, int value, SceUInt32 size);
void *sslMoveMemory(void *destination, const void *source, SceUInt32 size);
SceUInt32 sslStringLength(const char *string);
void *sslGetMemoryCallbacks(SslAllocCallback *alloc, SslReallocCallback *realloc,
			   SslFreeCallback *freeCallback);

#endif
