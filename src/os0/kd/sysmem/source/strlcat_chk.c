#include <stddef.h>
#include <sce/debug.h>

extern size_t strlcat(char *destination, const char *source, size_t size);

size_t __strlcat_chk(char *destination, const char *source, size_t size,
		    size_t capacity)
{
	if (capacity < size)
		SCE_PANIC(33);
	return strlcat(destination, source, size);
}
