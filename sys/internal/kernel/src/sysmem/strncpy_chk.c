#include <sysmem/sysclib.h>

char *__strncpy_chk(char *destination, const char *source, size_t count,
		size_t capacity)
{
	if (__builtin_expect(capacity < count, 0))
		__asm__ volatile("bkpt #0x81" ::: "memory");
	return strncpy(destination, source, count);
}
