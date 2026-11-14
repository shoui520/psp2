/*
 * _Wctombx and _Wctomb functions: a wide character to its UTF-8 sequence (SCE: the same
 * code twice).
 */
#include <stddef.h>
#include <wchar.h>
#include <xwchar.h>

static const mbstate_t initial = {0};

int _Wctombx(char *s, wchar_t wchar, mbstate_t *pst)
{
	if (s == NULL) {	/* reset the state */
		*pst = initial;
		return 0;
	} else {
		unsigned short wc = (unsigned short)wchar;
		char *t = s;
		int n;

		if ((wc & 0xff80) != 0) {
			if ((wc & 0xf800) != 0) {	/* three bytes */
				*t = (char)(wc >> 12 | 0xe0);
				n = 2;
			} else {			/* two bytes */
				*t = (char)(wc >> 6 | 0xc0);
				n = 1;
			}
		} else {				/* one byte */
			n = 0;
			*t = (char)wc;
		}
		for (++t; 0 < n; --n)	/* the continuation bytes */
			*t++ = (char)((wc >> 6 * (n - 1) & 0x3f) | 0x80);
		return t - s;
	}
}

int _Wctomb(char *s, wchar_t wchar, mbstate_t *pst)
{
	if (s == NULL) {	/* reset the state */
		*pst = initial;
		return 0;
	} else {
		unsigned short wc = (unsigned short)wchar;
		char *t = s;
		int n;

		if ((wc & 0xff80) != 0) {
			if ((wc & 0xf800) != 0) {	/* three bytes */
				*t = (char)(wc >> 12 | 0xe0);
				n = 2;
			} else {			/* two bytes */
				*t = (char)(wc >> 6 | 0xc0);
				n = 1;
			}
		} else {				/* one byte */
			n = 0;
			*t = (char)wc;
		}
		for (++t; 0 < n; --n)	/* the continuation bytes */
			*t++ = (char)((wc >> 6 * (n - 1) & 0x3f) | 0x80);
		return t - s;
	}
}
