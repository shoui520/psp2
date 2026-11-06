/* Filename and private descriptive name are ours, not recovered Sony names. */
#include "memory.h"
#include "format.h"
#include "string_inline.h"
#include <libkernel/libkernel.h>

SceUInt32 sslStringLength(const char *string)
{
	return sslStringLengthInline(string);
}

int sslToUpper(int character)
{
	return sceClibToupper(character);
}

char *sslCopyString(char *destination, const char *source, SceUInt32 size)
{
	return sceClibStrncpy(destination, source, size);
}

char *sslAppendString(char *destination, const char *source, SceUInt32 size)
{
	return sceClibStrncat(destination, source, size);
}

int sslCompareStrings(const char *left, const char *right, SceUInt32 size)
{
	return sceClibStrncmp(left, right, size);
}

void _sceSslQuickSort(char *base, SceSize size,
		      int (*compare)(const void *, const void *),
		      int left, int right);

void sslSort(void *base, unsigned int count, unsigned int size,
	     int (*compare)(const void *, const void *))
{
	if (base != 0 && compare != 0 && count != 0 && size != 0)
		_sceSslQuickSort((char *)base, size, compare, 0, (int)count - 1);
}
