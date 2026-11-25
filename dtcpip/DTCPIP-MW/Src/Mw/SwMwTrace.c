/*
 * The middleware's trace: a printf that flushes after every message, so a device's log
 * shows a failure's last line (the [SWMW] errors print with it). Sony's source for it is
 * not named by any record or string; the name and the file's place are ours.
 */

#include <stdarg.h>
#include <stdio.h>

/* print a format's arguments and flush: every trace lands before the next step runs */
__attribute__((noinline)) static int swMwVTrace(const char *format, va_list arguments)
{
	vfprintf(stdout, format, arguments);
	fflush(stdout);
	return 0;
}

int swMwTrace(const char *format, ...)
{
	va_list arguments;

	va_start(arguments, format);
	return swMwVTrace(format, arguments);
}
