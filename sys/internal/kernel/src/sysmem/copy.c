/* The checked memcpy; the copy itself is CopyMemoryForward (copy_forward.S). */
#include <stddef.h>
#include <sysmem/sysclib.h>

extern void *CopyMemoryForward(void *destination, const void *source, size_t size);

void *__memcpy_chk(void *destination, const void *source, size_t size,
		   size_t capacity)
{
	if (__builtin_expect(capacity < size, 0))
		__asm__ volatile("bkpt #0x81" ::: "memory");
	return CopyMemoryForward(destination, source, size);
}
