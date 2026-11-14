/*
 * _Getstr function: convert a scanf string field, %c (sfl 0), %s (sfl 1) or
 * a scan set %[ (sfl -1), into a char array or, with the l qualifier,
 * through _Mbtowc into a wchar_t array. A scan set with a '-' inside it is a
 * set of ranges. Under scanf_s every array comes with its size.
 */
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "xstdio.h"
#include <xwchar.h>

#define BAD_ARGUMENT(mesg)	(EOF - _EXT1_FAIL(mesg))

/* test whether ch is in the scan set s[0, n), whose a-z are ranges */
static int inrange(int ch, const unsigned char *s, size_t n)
{
	const unsigned char uch = (unsigned char)ch;

	for (; 3 <= n;) {
		if (s[1] == '-') {
			if (s[0] <= uch && uch <= s[2])
				return 1;
			s += 3, n -= 3;
		} else if (s[0] == uch) {
			return 1;
		} else {
			++s, --n;
		}
	}
	for (; n != 0; ++s, --n) {
		if (*s == uch)
			return 1;
	}
	return 0;
}

int _Getstr(_Sft *px, int sfl)
{
	char buf[4];
	char *s = NULL;
	char range = 0;
	char comp;
	char seen = 0;
	int ch;
	int nbuf = 0;
	size_t nset;
	int wfl = px->qual == 'l';
	const char *pset = NULL;
	const char *t;
	wchar_t wc;
	wchar_t *ps = NULL;
	_Mbstinit(mbst);

	if (sfl < 0) {	/* parse the scan set in the format */
		comp = *++px->s == '^' ? *px->s++ : '\0';
		t = strchr(*px->s == ']' ? px->s + 1 : px->s, ']');
		if (t == NULL)
			return 0;
		nset = t - px->s;
		pset = px->s;
		px->s = t;
		if (3 <= (int)nset && memchr(pset + 1, '-', nset - 2) != NULL)
			range = 1;
	}
	px->nget = 0 < px->width ? px->width : sfl != 0 ? INT_MAX : 1;
	if (!px->noconv) {	/* the array */
		if (wfl) {
			ps = va_arg(px->ap, wchar_t *);
			if (ps == NULL)
				return BAD_ARGUMENT("scanf_s: bad %lc, %ls, or %l[ argument");
		} else {
			s = va_arg(px->ap, char *);
			if (s == NULL)
				return BAD_ARGUMENT("scanf_s: bad %c, %s, or %[ argument");
		}
	}
	if (!px->noconv && px->secure) {	/* the array's size */
		size_t size = va_arg(px->ap, size_t);

		if (RSIZE_MAX < size)
			return BAD_ARGUMENT("scanf_s: bad %c, %s, or %[ size");
		if (size < px->prec)
			px->prec = size;
	}

	while ((ch = GETN(px)) != EOF) {
		if (0 < sfl && isspace(ch))
			break;
		if (sfl < 0) {
			if (!range && ((!comp && memchr(pset, ch, nset) == NULL)
				       || (comp && memchr(pset, ch, nset) != NULL)))
				break;
			if (range && ((!comp && !inrange(ch, (const unsigned char *)pset, nset))
				      || (comp && inrange(ch, (const unsigned char *)pset, nset))))
				break;
		}
		if (!wfl) {
			if (!px->noconv) {
				if (px->prec == 0) {	/* the array is full */
					seen = 1;
					break;
				}
				*s++ = ch;
				px->stored = 1;
				--px->prec;
			}
			seen = 2;
		} else {	/* a multibyte character to a wide one */
			int n;

			buf[nbuf++] = ch;
			switch (n = _Mbtowc(&wc, buf, nbuf, &mbst)) {
			case -2:	/* incomplete */
				seen |= 1;
				if (nbuf < sizeof(buf))
					break;
				/* fall through */
			case -1:	/* invalid: put the bytes back */
				while (0 < nbuf) {
					int c = buf[--nbuf];

					UNGETN(px, c);
				}
				return seen ? 0 : EOF;
			case 0:
				if (wc == L'\0')
					n = strlen(buf) + 1;
				/* fall through */
			default:
				if (n == -3)	/* a wide character from no bytes */
					n = 0;
				while (n < nbuf) {
					int c = buf[--nbuf];

					UNGETN(px, c);
				}
				if (!px->noconv) {
					if (px->prec == 0) {	/* the array is full */
						seen = 1;
						break;
					}
					*ps++ = wc;
					px->stored = 1;
					--px->prec;
				}
				seen = 2;
				nbuf = 0;
				--px->width;
			}
		}
	}
	UNGETN(px, ch);
	if (sfl != 0 && seen == 2) {	/* terminate a string */
		if (!px->noconv) {
			if (px->prec == 0)
				return 0;
			if (wfl)
				*ps = L'\0';
			else
				*s = '\0';
		}
		return 1;
	}
	return seen == 2 ? 1 : (seen & 1) || ch == EOF ? EOF : 0;
}
