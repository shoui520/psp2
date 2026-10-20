#include <sysmem/sysclib.h>

char *__strcat_chk(char *destination, const char *source, size_t capacity)
{
	size_t used = strnlen(destination, capacity);

	if (__builtin_expect(capacity <= used, 0))
		__asm__ volatile("bkpt #0x81" ::: "memory");
	if (__builtin_expect(strnlen(source, capacity) >= capacity - used, 0))
		__asm__ volatile("bkpt #0x81" ::: "memory");
	return strncat(destination, source, capacity);
}
