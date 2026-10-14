#include <stddef.h>
#include <sce/debug.h>

extern size_t strlcpy(char *destination, const char *source, size_t size);

size_t __strlcpy_chk(char *destination, const char *source, size_t size,
		    size_t capacity)
{
	if (capacity < size)
		SCE_PANIC(33);
	return strlcpy(destination, source, size);
}
