/*
 * _Wcsftime function: format a time as wide characters, expanding the conversions through
 * _Gentime.
 */
#include <stddef.h>
#include <wchar.h>
#include <xwchar.h>

#include "xtime.h"

#define NSTACK	3	/* the deepest a conversion's expansion nests */

size_t _Wcsftime(wchar_t *buf, size_t bufsize, const char *fmt, size_t len,
		 const struct tm *t)
{
	size_t lensave[NSTACK];
	const char *fmtsave[NSTACK];
	int nstack = 0;
	wchar_t *ibuf = buf;
	_Mbstinit(mbst);

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
		if (wc == L'\0')
			break;
		if (bufsize == 0)
			return 0;
		if (wc == L'%' && len != 0) {	/* expand a conversion */
			char ac[20];
			char qual = '\0';
			int m;
			const char *p;

			if (*fmt == 'E' || *fmt == 'O')
				qual = *fmt++, --len;
			p = _Gentime(t, qual, *fmt, &m, ac);
			++fmt, --len;
			if (0 < m) {	/* convert the text of the conversion */
				_Mbstinit(mbst2);

				for (;;) {
					if ((n = _Mbtowc(&wc, p, m, &mbst2)) <= 0)
						break;
					if (bufsize == 0)
						return 0;
					--bufsize;
					*buf++ = wc;
					p += n;
					if ((m -= n) <= 0)
						break;
				}
			} else if (len != 0 && nstack < NSTACK) {	/* a format to expand first */
				fmtsave[nstack] = fmt, lensave[nstack++] = len;
				fmt = p, len = -m;
			} else {	/* replace the format */
				fmt = p, len = -m;
			}
		} else {	/* copy the character */
			*buf++ = wc;
			--bufsize;
		}
	}
	return buf - ibuf;
}
