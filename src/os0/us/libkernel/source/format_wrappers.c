#include <stdarg.h>
#include <psp2/kernel/clib.h>
#include "format.h"

extern int sceKernelGetStdout(void);

int sceClibSnprintf(char *destination, SceSize size, const char *format, ...)
{
	struct FormatBuffer buffer = { size, destination };
	va_list arguments;
	int result;

	va_start(arguments, format);
	result = FormatEngine(&formatBufferOps, &buffer, size, format, arguments);
	va_end(arguments);
	return result;
}

int sceClibVsnprintf(char *destination, SceSize size, const char *format,
		va_list arguments)
{
	struct FormatBuffer buffer = { size, destination };

	return FormatEngine(&formatBufferOps, &buffer, size, format, arguments);
}

int sceClibPrintf(const char *format, ...)
{
	struct FormatFile file;
	va_list arguments;
	int result;

	va_start(arguments, format);
	file.descriptor = sceKernelGetStdout();
	file.options = 0;
	result = FormatEngine(&formatFileOps, &file, 0, format, arguments);
	va_end(arguments);
	return result;
}

int sceClibVprintf(const char *format, va_list arguments)
{
	struct FormatFile file;

	file.descriptor = sceKernelGetStdout();
	file.options = 0;
	return FormatEngine(&formatFileOps, &file, 0, format, arguments);
}

int sceClibDprintf(SceUID descriptor, const char *format, ...)
{
	struct FormatFile file;
	va_list arguments;
	int result;

	va_start(arguments, format);
	file.descriptor = descriptor;
	file.options = 0;
	result = FormatEngine(&formatFileOps, &file, 0, format, arguments);
	va_end(arguments);
	return result;
}

int sceClibVdprintf(SceUID descriptor, const char *format, va_list arguments)
{
	struct FormatFile file;

	file.descriptor = descriptor;
	file.options = 0;
	return FormatEngine(&formatFileOps, &file, 0, format, arguments);
}
