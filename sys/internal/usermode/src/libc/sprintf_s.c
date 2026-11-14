/* sprintf_s function */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "xstdio.h"

struct _Sbuf {
	char *s;	/* the next character to write */
	size_t n;	/* the room left */
	int full;	/* the output did not fit */
};

static void *prout(void *arg, const char *buf, size_t n)
{
	struct _Sbuf *p = (struct _Sbuf *)arg;

	if (p->n < n) {
		p->full = 1;
		n = p->n;
	}
	if (n != 0) {
		memcpy(p->s, buf, n);
		p->s += n;
		p->n -= n;
	}
	return arg;
}

int sprintf_s(char *__restrict s, rsize_t n, const char *__restrict fmt, ...)
{
	struct _Sbuf x;
	int ans;
	va_list ap;

	va_start(ap, fmt);
	if (s == NULL || n == 0 || RSIZE_MAX < n) {
		_EXT1_FAIL("sprintf_s: bad argument");
		ans = 0;
	} else if (fmt == NULL) {
		*s = '\0';
		_EXT1_FAIL("sprintf_s: bad format argument");
		ans = 0;
	} else {
		x.s = s;
		x.n = n - 1;
		x.full = 0;
		ans = _Printf(&prout, &x, fmt, ap, 1);
		if (x.full) {
			_EXT1_FAIL("sprintf_s: buffer overflow");
			ans = 0;
		} else if (-1 <= ans) {
			*x.s = '\0';
		} else {
			*s = '\0';
			ans = 0;
		}
	}
	va_end(ap);
	return ans;
}
