/* snwprintf_s function */
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>
#include "xstdio.h"

struct _WSbuf {
	wchar_t *s;	/* the next character to write */
	size_t n;	/* the room left */
};

static void *prout(void *arg, const wchar_t *buf, size_t n)
{
	struct _WSbuf *p = (struct _WSbuf *)arg;

	if (p->n < n)
		n = p->n;
	if (n != 0) {
		wmemcpy(p->s, buf, n);
		p->s += n;
		p->n -= n;
	}
	return arg;
}

int snwprintf_s(wchar_t *__restrict s, rsize_t n, const wchar_t *__restrict fmt, ...)
{
	struct _WSbuf x;
	int ans;
	va_list ap;

	va_start(ap, fmt);
	if (s == NULL || n == 0 || RSIZE_MAX < n || fmt == NULL) {
		_EXT1_FAIL("snwprintf_s: bad argument");
		ans = -1;
	} else {
		x.s = s;
		x.n = n - 1;
		ans = _WPrintf(&prout, &x, fmt, ap, 1);
		if (x.s != NULL)
			*x.s = L'\0';
	}
	va_end(ap);
	return ans;
}
