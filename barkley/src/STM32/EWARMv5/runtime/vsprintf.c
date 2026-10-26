/*
 * vsprintf: formatted output to a string, terminated; its length, or a negative value.
 */

#include "xstdio.h"

static void *sprout(void *s, char c)
{
	char **ps = s;

	*(*ps)++ = c;
	return s;
}

int vsprintf(char *s, const char *format, va_list ap)
{
	char *p = s;
	int ans = _Printf(sprout, &p, format, &ap);

	*p = '\0';
	if (ans < 0)
		return ans;
	return p - s;
}
