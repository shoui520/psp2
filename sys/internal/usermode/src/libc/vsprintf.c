/* vsprintf function */
#include <string.h>

#include "xstdio.h"

static void *prout(void *s, const char *buf, size_t n)
{
	return (char *)memcpy(s, buf, n) + n;
}

int vsprintf(char *__restrict s, const char *__restrict fmt, va_list ap)
{
	int ans = _Printf(&prout, s, fmt, ap, 0);

	if (0 <= ans)
		s[ans] = '\0';
	return ans;
}
