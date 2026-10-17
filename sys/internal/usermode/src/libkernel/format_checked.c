#include <stdarg.h>
#include <psp2/kernel/clib.h>
#include <sysmem/compiler.h>

int sceClibSnprintfChk(char *destination, SceSize size, int flags,
		SceSize destinationSize, const char *format, ...)
{
	va_list arguments;
	int result;

	va_start(arguments, format);
	if (destinationSize < size)
		__breakpoint(BKPT_CHECK_FAILED);
	result = sceClibVsnprintf(destination, size, format, arguments);
	va_end(arguments);
	return result;
}

int sceClibVsnprintfChk(char *destination, SceSize size, int flags,
		SceSize destinationSize, const char *format, va_list arguments)
{
	if (destinationSize < size)
		__breakpoint(BKPT_CHECK_FAILED);
	return sceClibVsnprintf(destination, size, format, arguments);
}
