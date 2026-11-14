/* swprintf function */
#include <wchar.h>

#include "xstdio.h"

struct _WSbuf {
	wchar_t *s;	/* the next character to write */
	size_t n;	/* the room left */
};

static void *prout(void *arg, const wchar_t *buf, size_t n)
{
	struct _WSbuf *p = (struct _WSbuf *)arg;
	void *ans = arg;

	if (p->n < n) {	/* the output does not fit */
		ans = NULL;
		n = p->n;
	}
	if (n != 0) {
		wmemcpy(p->s, buf, n);
		p->s += n;
		p->n -= n;
	}
	return ans;
}

int swprintf(wchar_t *__restrict s, size_t n, const wchar_t *__restrict fmt, ...)
{
	struct _WSbuf x;
	int ans;
	va_list ap;

	va_start(ap, fmt);
	x.s = n == 0 ? NULL : s;
	x.n = n == 0 ? 0 : n - 1;
	ans = _WPrintf(&prout, &x, fmt, ap, 0);
	if (x.s != NULL)
		*x.s = L'\0';
	va_end(ap);
	return ans;
}
