#include <stdarg.h>
#include <stddef.h>
#include <sysmem/debug.h>
#include <sysmem/compiler.h>
#include <sysmem/sysclib.h>

__attribute__((noinline))
int __vsnprintf_chk(char *destination, size_t size, int flags, size_t capacity,
		    const char *format, va_list arguments)
{
	(void)flags;
	if (capacity < size)
		SCE_PANIC(72);
	return vsnprintf(destination, size, format, arguments);
}

int __snprintf_chk(char *destination, size_t size, int flags, size_t capacity,
		   const char *format, ...)
{
	va_list arguments;
	int result;

	va_start(arguments, format);
	result = __vsnprintf_chk(destination, size, flags, capacity, format, arguments);
	va_end(arguments);
	return result;
}
