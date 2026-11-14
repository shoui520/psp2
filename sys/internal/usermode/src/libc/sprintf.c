/* sprintf function */
#include <string.h>

#include "xstdio.h"

static void *prout(void *s, const char *buf, size_t n)
{
	return (char *)memcpy(s, buf, n) + n;
}

int sprintf(char *__restrict s, const char *__restrict fmt, ...)
{
	int ans;
	va_list ap;

	va_start(ap, fmt);
	ans = _Printf(&prout, s, fmt, ap, 0);
	if (0 <= ans)
		s[ans] = '\0';
	va_end(ap);
	return ans;
}
