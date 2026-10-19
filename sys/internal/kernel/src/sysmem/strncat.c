/* strncat (the file name is ours; the checked copies follow it in their own files) */

#include <sysmem/sysclib.h>

char *strncat(char *destination, const char *source, size_t count)
{
	size_t size = strnlen(source, count);

	if (size) {
		char *end = destination + strlen(destination);

		end[size] = 0;
		memcpy(end, source, size);
	}
	return destination;
}
