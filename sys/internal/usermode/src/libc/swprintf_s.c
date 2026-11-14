/* swprintf_s function */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "xstdio.h"

struct _WSbuf {
	wchar_t *s;	/* the next character to write */
	size_t n;	/* the room left */
	int full;	/* the output did not fit */
};

static void *prout(void *arg, const wchar_t *buf, size_t n)
{
	struct _WSbuf *p = (struct _WSbuf *)arg;

	if (p->n < n) {
		p->full = 1;
		n = p->n;
	}
	if (n != 0) {
		wmemcpy(p->s, buf, n);
		p->s += n;
		p->n -= n;
	}
	return arg;
}

int swprintf_s(wchar_t *__restrict s, rsize_t n, const wchar_t *__restrict fmt, ...)
{
	struct _WSbuf x;
	int ans;
	va_list ap;

	va_start(ap, fmt);
	if (s == NULL || n == 0 || RSIZE_MAX < n) {
		_EXT1_FAIL("swprintf_s: bad argument");
		ans = 0;
	} else if (fmt == NULL) {
		*s = L'\0';
		_EXT1_FAIL("swprintf_s: bad format argument");
		ans = 0;
	} else {
		x.s = s;
		x.n = n - 1;
		x.full = 0;
		ans = _WPrintf(&prout, &x, fmt, ap, 1);
		if (x.full) {
			_EXT1_FAIL("swprintf_s: buffer overflow");
			ans = 0;
		} else if (-1 <= ans) {
			*x.s = L'\0';
		} else {
			*s = L'\0';
			ans = 0;
		}
	}
	va_end(ap);
	return ans;
}
