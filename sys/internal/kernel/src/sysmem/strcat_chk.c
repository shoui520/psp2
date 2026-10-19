#include <sysmem/sysclib.h>

char *__strncat_chk2(char *destination, const char *source, size_t capacity)
{
	size_t used = strnlen(destination, capacity);

	if (capacity <= used)
		__asm__ volatile("bkpt #0x81" ::: "memory");
	if (strnlen(source, capacity) >= capacity - used)
		__asm__ volatile("bkpt #0x81" ::: "memory");
	return strncat(destination, source, capacity);
}
