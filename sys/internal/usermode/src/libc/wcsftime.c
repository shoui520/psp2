/* wcsftime function */
#include <time.h>
#include <wchar.h>
#include <xwchar.h>

size_t wcsftime(wchar_t *__restrict buf, size_t bufsize, const wchar_t *__restrict fmt,
		const struct tm *__restrict t)
{
	wchar_t *ibuf = buf;
	char fmtbuf[4];
	int ch;

	while (0 < bufsize && *fmt != L'\0')
		if (*fmt != L'%' || (ch = _Wctob(fmt[1])) <= 0) {	/* copy a character */
			*buf++ = *fmt++;
			--bufsize;
		} else {	/* convert a single format */
			const wchar_t *fmtsav = fmt;
			size_t n = 2;
			int nput;

			fmtbuf[0] = '%';
			fmtbuf[1] = (char)ch;
			fmt += 2;
			if ((ch == 'E' || ch == 'O') && fmtsav[2] != L'\0')
				fmtbuf[n++] = (char)_Wctob(*fmt++);
			if ((nput = (int)_Wcsftime(buf, bufsize, fmtbuf, n, t)) < 0)
				return 0;
			buf += nput;
			bufsize -= nput;
		}
	if (bufsize == 0)
		return 0;
	*buf = L'\0';
	return buf - ibuf;
}
