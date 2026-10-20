#include <sysmem/sysclib.h>

char *__strcpy_chk(char *destination, const char *source, size_t capacity)
{
	if (__builtin_expect(capacity < strnlen(source, capacity), 0))
		__asm__ volatile("bkpt #0x81" ::: "memory");
	return strncpy(destination, source, capacity);
}
