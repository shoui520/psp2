/* vsnprintf_s function */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "xstdio.h"

struct _Sbuf {
	char *s;	/* the next character to write */
	size_t n;	/* the room left */
};

static void *prout(void *arg, const char *buf, size_t n)
{
	struct _Sbuf *p = (struct _Sbuf *)arg;

	if (p->n < n)
		n = p->n;
	if (n != 0) {
		memcpy(p->s, buf, n);
		p->s += n;
		p->n -= n;
	}
	return arg;
}

int vsnprintf_s(char *__restrict s, rsize_t n, const char *__restrict fmt, va_list ap)
{
	struct _Sbuf x;
	int ans;

	if (s == NULL || n == 0 || RSIZE_MAX < n || fmt == NULL) {
		_EXT1_FAIL("vsnprintf_s: bad argument");
		ans = -1;
	} else {
		x.s = s;
		x.n = n - 1;
		ans = _Printf(&prout, &x, fmt, ap, 1);
		if (x.s != NULL)
			*x.s = '\0';
	}
	return ans;
}
