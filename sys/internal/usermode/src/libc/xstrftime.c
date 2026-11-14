/*
 * _Strftime function: format a time, expanding the conversions through _Gentime.
 */
#include <stddef.h>
#include <string.h>
#include <wchar.h>
#include <xwchar.h>

#include "xtime.h"

#define NSTACK	3	/* the deepest a conversion's expansion nests */

size_t _Strftime(char *buf, size_t bufsize, const char *fmt, const struct tm *t)
{
	size_t len, lensave[NSTACK];
	const char *fmtsave[NSTACK];
	int nstack = 0;
	char *ibuf = buf;
	_Mbstinit(mbst);

	len = strlen(fmt);
	for (;;) {	/* parse the format */
		wchar_t wc = 0;
		int n;

		if (len == 0) {	/* resume a saved format, or finish */
			if (nstack == 0)
				break;
			--nstack;
			fmt = fmtsave[nstack], len = lensave[nstack];
		}
		if ((n = _Mbtowc(&wc, fmt, len, &mbst)) <= 0)
			n = *fmt == '\0' ? 0 : 1;
		fmt += n, len -= n;
		if (wc == '%' && len != 0) {	/* expand a conversion */
			char ac[20];
			char qual = '\0';
			int m;
			const char *p;

			if (*fmt == 'E' || *fmt == 'O')
				qual = *fmt++, --len;
			p = _Gentime(t, qual, *fmt, &m, ac);
			++fmt, --len;
			if (0 < m) {	/* the text of the conversion */
				if (bufsize < m)
					return 0;
				memcpy(buf, p, m);
				buf += m, bufsize -= m;
			} else if (len != 0 && nstack < NSTACK) {	/* a format to expand first */
				fmtsave[nstack] = fmt, lensave[nstack++] = len;
				fmt = p, len = -m;
			} else {	/* replace the format */
				fmt = p, len = -m;
			}
		} else {	/* copy the text */
			if (bufsize < n)
				return 0;
			memcpy(buf, fmt - n, n);
			buf += n, bufsize -= n;
		}
	}
	if (bufsize == 0)
		return 0;
	*buf = '\0';
	return buf - ibuf;
}
