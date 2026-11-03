/* Filename and private descriptive name are ours, not recovered Sony names. */
#include "memory.h"
#include "format.h"
#include <libkernel/libkernel.h>

SceUInt32 sslStringLength(const char *string)
{
	SceUInt32 length = 0;

	while (*string++)
		length++;
	return length;
}

char *sslCopyString(char *destination, const char *source, SceUInt32 size)
{
	return sceClibStrncpy(destination, source, size);
}

char *sslAppendString(char *destination, const char *source, SceUInt32 size)
{
	return sceClibStrncat(destination, source, size);
}
