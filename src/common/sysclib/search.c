#include <stddef.h>
#include <string.h>

char *strstr(const char *string, const char *substring)
{
	size_t length = strlen(substring);
	unsigned char first;
	char *candidate;

	if (!length)
		return (char *)string;
	first = *substring++;
	length--;
	for (;;) {
		candidate = strchr(string, first);
		if (!candidate)
			return NULL;
		string = candidate + 1;
		if (!memcmp(string, substring, length))
			return candidate;
	}
}
