/*
 * _WGetstr function: convert a wscanf string field, %c (sfl 0), %s (sfl 1) or
 * a scan set %[ (sfl -1), into a wchar_t array or, without the l qualifier,
 * through _Wctomb into a char array, ending with the shift back to the
 * initial state. A scan set with a '-' inside it is a set of ranges. Under
 * wscanf_s every array comes with its size.
 */
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include "xwstdio.h"
#include <xwchar.h>

#define BAD_ARGUMENT(mesg)	(EOF - _EXT1_FAIL(mesg))

/* test whether ch is in the scan set s[0, n), whose a-z are ranges */
static int inrange(wint_t ch, const wchar_t *s, size_t n)
{
	const wchar_t wch = (wchar_t)ch;

	for (; 3 <= n;) {
		if (s[1] == L'-') {
			if (s[0] <= wch && wch <= s[2])
				return 1;
			s += 3, n -= 3;
		} else if (s[0] == wch) {
			return 1;
		} else {
			++s, --n;
		}
	}
	for (; n != 0; ++s, --n) {
		if (*s == wch)
			return 1;
	}
	return 0;
}

int _WGetstr(_WSft *px, int sfl)
{
	char buf[4];
	char *s = NULL;
	char range = 0;
	wchar_t comp;
	char seen = 0;
	wint_t ch;
	int n;
	size_t nset;
	int wfl = px->qual == L'l';
	const wchar_t *pset = NULL;
	const wchar_t *t;
	wchar_t *ps = NULL;
	_Mbstinit(mbst);

	if (sfl < 0) {	/* parse the scan set in the format */
		comp = *++px->s == L'^' ? *px->s++ : L'\0';
		t = wcschr(*px->s == L']' ? px->s + 1 : px->s, L']');
		if (t == NULL)
			return 0;
		nset = t - px->s;
		pset = px->s;
		px->s = t;
		if (3 <= (int)nset && wmemchr(pset + 1, L'-', nset - 2) != NULL)
			range = 1;
	}
	px->nget = 0 < px->width ? px->width : sfl != 0 ? INT_MAX : 1;
	if (!px->noconv) {	/* the array */
		if (wfl) {
			ps = va_arg(px->ap, wchar_t *);
			if (ps == NULL)
				return BAD_ARGUMENT("wscanf_s: bad %lc, %ls, or %l[ argument");
		} else {
			s = va_arg(px->ap, char *);
			if (s == NULL)
				return BAD_ARGUMENT("wscanf_s: bad %c, %s, or %[ argument");
		}
	}
	if (!px->noconv && px->secure) {	/* the array's size */
		size_t size = va_arg(px->ap, size_t);

		if (RSIZE_MAX < size)
			return BAD_ARGUMENT("wscanf_s: bad %c, %s, or %[ size");
		if (size < px->prec)
			px->prec = size;
	}

	while ((ch = GETN(px)) != WEOF) {
		if (0 < sfl && iswspace(ch))
			break;
		if (sfl < 0) {
			if (!range && ((!comp && wmemchr(pset, ch, nset) == NULL)
				       || (comp && wmemchr(pset, ch, nset) != NULL)))
				break;
			if (range && ((!comp && !inrange(ch, pset, nset))
				      || (comp && inrange(ch, pset, nset))))
				break;
		}
		if (!wfl) {	/* a wide character to a multibyte one */
			if ((n = _Wctomb(buf, ch, &mbst)) < 0) {
				seen = 1;
				break;
			}
			if (!px->noconv) {
				if (px->prec < (size_t)n) {	/* the array is full */
					seen = 1;
					break;
				}
				memcpy(s, buf, n);
				s += n;
				px->stored = 1;
				px->prec -= n;
			}
			seen = 2;
		} else {
			if (!px->noconv) {
				if (px->prec == 0) {	/* the array is full */
					seen = 1;
					break;
				}
				*ps++ = ch;
				px->stored = 1;
				--px->prec;
			}
			seen = 2;
		}
	}
	if (!wfl && seen == 2) {	/* the shift back to the initial state */
		n = _Wctomb(buf, L'\0', &mbst) - 1;
		if (n < 0 || px->prec < (size_t)n)
			return 0;
		if (!px->noconv) {
			memcpy(s, buf, n);
			s += n;
			px->prec -= n;
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
	return seen == 2 ? 1 : (seen & 1) || ch == WEOF ? EOF : 0;
}
