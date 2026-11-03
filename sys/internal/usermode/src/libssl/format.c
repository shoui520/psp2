/* Private descriptive identifiers and filename are ours, not Sony names. */
#include "format.h"
#include <libkernel/libkernel.h>

int sslVFormatString(char *destination, SceUInt32 size, const char *format,
		     va_list arguments)
{
	return sceClibVsnprintf(destination, size, format, arguments);
}

int sslFormatString(char *destination, SceUInt32 size, const char *format, ...)
{
	va_list arguments;
	int result;

	va_start(arguments, format);
	result = sceClibVsnprintf(destination, size, format, arguments);
	va_end(arguments);
	return result;
}
