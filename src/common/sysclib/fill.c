/* The checked memset; the fill itself is SetMemoryCore (fill_core.S). */
#include <stddef.h>

extern void *SetMemoryCore(void *destination, int value, size_t size);

void *__memset_chk(void *destination, int value, size_t size, size_t capacity)
{
	if (capacity < size)
		__asm__ volatile("bkpt #0x81" ::: "memory");
	return SetMemoryCore(destination, value, size);
}
