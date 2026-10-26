/*
 * printf: formatted output to stdout, a character at a time.
 */

#include "xstdio.h"

static void *prout(void *str, char c)
{
	if (putchar(c) != c)
		return 0;
	return str;
}

int printf(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	return _Printf(prout, (void *)1, format, &ap);
}
