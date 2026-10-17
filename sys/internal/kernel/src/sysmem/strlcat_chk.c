#include <stddef.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>

size_t __strlcat_chk(char *destination, const char *source, size_t size,
		    size_t capacity)
{
	if (capacity < size)
		SCE_PANIC(33);
	return strlcat(destination, source, size);
}
