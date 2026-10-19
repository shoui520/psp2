#include <sysmem/sysclib.h>

char *__strncpy_chk2(char *destination, const char *source, size_t capacity)
{
	if (capacity < strnlen(source, capacity))
		__asm__ volatile("bkpt #0x81" ::: "memory");
	return strncpy(destination, source, capacity);
}
