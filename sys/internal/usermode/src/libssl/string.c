/* Filename and private descriptive name are ours, not recovered Sony names. */
#include "memory.h"

SceUInt32 sslStringLength(const char *string)
{
	SceUInt32 length = 0;

	while (*string++)
		length++;
	return length;
}
