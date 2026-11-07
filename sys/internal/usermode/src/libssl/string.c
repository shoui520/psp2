/* Filename and private descriptive name are ours, not recovered Sony names. */
#include "memory.h"
#include "format.h"
#include "string_inline.h"
#include <libkernel/libkernel.h>

SceUInt32 sslStringLength(const char *string)
{
	return sslStringLengthInline(string);
}

int sslCompareCString(const char *left, const char *right)
{
	while (*left == *right && *left != '\0') {
		left++;
		right++;
	}
	if (*left == *right && *left == '\0')
		return 0;
	return (unsigned char)*left - (unsigned char)*right;
}

char *sslCopyCString(char *destination, const char *source)
{
	char *result = destination;

	while ((*destination++ = *source++) != '\0')
		;
	return result;
}

void *sslBinarySearch(const void *key, const void *base, int count,
		      int size, int (*compare)(const void *, const void *))
{
	int low = 0;
	int high = count - 1;
	int middle;
	int result;
	const char *element;

	while (high > low) {
		middle = (high + low) / 2;
		element = (const char *)base + middle * size;
		result = compare(key, element);
		if (result == 0)
			return (void *)element;
		if (result > 0)
			low = middle + 1;
		else
			high = middle - 1;
	}
	if (high != low)
		return 0;
	element = (const char *)base + low * size;
	return compare(key, element) == 0 ? (void *)element : 0;
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
