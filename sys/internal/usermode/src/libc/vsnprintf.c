/* vsnprintf function */
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

int vsnprintf(char *__restrict s, size_t n, const char *__restrict fmt, va_list ap)
{
	struct _Sbuf x;
	int ans;

	x.s = n == 0 ? NULL : s;
	x.n = n == 0 ? 0 : n - 1;
	ans = _Printf(&prout, &x, fmt, ap, 0);
	if (x.s != NULL)
		*x.s = '\0';
	return ans;
}
