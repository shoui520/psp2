#include <sysmem/sysclib.h>

char *strncat(char *destination, const char *source, size_t count)
{
	size_t size = strnlen(source, count);

	if (size) {
		char *end = destination + strlen(destination);

		end[size] = 0;
		memcpy(end, source, size);
	}
	return destination;
}

char *__strncpy_chk2(char *destination, const char *source, size_t capacity)
{
	if (capacity < strnlen(source, capacity))
		__asm__ volatile("bkpt #0x81" ::: "memory");
	return strncpy(destination, source, capacity);
}

char *__strncpy_chk(char *destination, const char *source, size_t count,
		size_t capacity)
{
	if (capacity < count)
		__asm__ volatile("bkpt #0x81" ::: "memory");
	return strncpy(destination, source, count);
}

char *__strncat_chk2(char *destination, const char *source, size_t capacity)
{
	size_t used = strnlen(destination, capacity);

	if (capacity <= used)
		__asm__ volatile("bkpt #0x81" ::: "memory");
	if (strnlen(source, capacity) >= capacity - used)
		__asm__ volatile("bkpt #0x81" ::: "memory");
	return strncat(destination, source, capacity);
}

char *__strncat_chk(char *destination, const char *source, size_t count,
		size_t capacity)
{
	if (capacity < count)
		__asm__ volatile("bkpt #0x81" ::: "memory");
	return strncat(destination, source, count);
}
