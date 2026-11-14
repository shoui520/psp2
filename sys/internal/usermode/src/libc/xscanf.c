/*
 * _Scanf function: Dinkumware's formatted input engine, with SCE's field precision (".n" or
 * ".*" after the width).
 */
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "xstdio.h"
#include "xwchar.h"

/* read a character, counting it */
#define GETN(px)	(++(px)->nchar, (*(px)->pfn)((px)->arg, 0, 1))

/* push back the character ch, uncounting it */
#define UNGETN(px, ch)							\
	do {								\
		--(px)->nchar;						\
		if ((ch) != EOF)					\
			(*(px)->pfn)((px)->arg, ch, 0);			\
	} while (0)

int _Scanf(_Scin *pfn, void *arg, const char *fmt, va_list ap, int secure)
{
	int nconv = EOF;
	_Sft x;
	mbstate_t mbst = {0};
	wchar_t wc;

	x.pfn = pfn;
	x.arg = arg;
	_Vacopy(&x.ap, ap);
	x.secure = secure;
	x.s = fmt;
	x.nchar = 0;
	for (;;) {
		int n, ch, code;

		wc = 0;
		n = _Mbtowc(&wc, x.s, INT_MAX, &mbst);
		if (n <= 0)
			n = *x.s != '\0';
		if (isspace(_Wctob(wc))) {	/* white space matches any amount */
			while (isspace(ch = GETN(&x)))
				;
			UNGETN(&x, ch);
		} else {			/* the text before the % must match */
			const char *s = x.s;
			int m = wc == L'%' ? n - 1 : n;

			for (; 0 <= --m; ++s) {
				if ((ch = GETN(&x)) != *s) {
					UNGETN(&x, ch);
					return nconv < 0 ? 0 : nconv;
				}
			}
		}
		x.s += n;
		if (wc != L'%') {
			if (wc == L'\0')
				return nconv < 0 ? 0 : nconv;
			continue;
		}

		x.noconv = *x.s == '*' ? *x.s++ : '\0';	/* the conversion */
		for (x.width = 0; isdigit((unsigned char)*x.s); ++x.s) {
			if (x.width < INT_MAX)
				x.width = x.width * 10 + (unsigned char)*x.s - '0';
		}
		if (*x.s != '.') {
			x.prec = SIZE_MAX;
		} else if (*++x.s == '*') {
			x.prec = va_arg(ap, int);
			++x.s;
		} else {
			for (x.prec = 0; isdigit((unsigned char)*x.s); ++x.s) {
				x.prec = x.prec <= SIZE_MAX / 10 - ((unsigned char)*x.s - '0')
					? x.prec * 10 + (unsigned char)*x.s - '0' : SIZE_MAX;
			}
		}
		x.qual = strchr("hjltzL", *x.s) != NULL ? *x.s++ : '\0';
		if (x.qual == 'h' && *x.s == 'h') {
			x.qual = 'b';
			++x.s;
		} else if (x.qual == 'l' && *x.s == 'l') {
			x.qual = 'q';
			++x.s;
		}
		if (strchr("cCn[", *x.s) == NULL) {	/* skip white space before the field */
			while (isspace(ch = GETN(&x)))
				;
			UNGETN(&x, ch);
		}
		if ((code = _Getfld(&x)) <= 0)
			return 0 <= nconv ? nconv : code;
		if (nconv < 0)
			nconv = 0;
		if (x.stored)
			++nconv;
		++x.s;
	}
}
