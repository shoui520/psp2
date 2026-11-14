/* vsnwprintf_s function */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
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

int vsnwprintf_s(wchar_t *__restrict s, rsize_t n, const wchar_t *__restrict fmt, va_list ap)
{
	struct _WSbuf x;
	int ans;

	if (s == NULL || n == 0 || RSIZE_MAX < n || fmt == NULL) {
		_EXT1_FAIL("vsnwprintf_s: bad argument");
		ans = -1;
	} else {
		x.s = s;
		x.n = n - 1;
		ans = _WPrintf(&prout, &x, fmt, ap, 1);
		if (x.s != NULL)
			*x.s = L'\0';
	}
	return ans;
}
