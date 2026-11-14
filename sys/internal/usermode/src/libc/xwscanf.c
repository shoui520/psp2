/*
 * _WScanf function: Dinkumware's wide formatted input engine, with SCE's field precision
 * (".n" or ".*" after the width).
 */
#include <stdint.h>
#include <limits.h>
#include <wchar.h>
#include <wctype.h>

#include "xwstdio.h"

/* read a wide character, counting it */
#define GETN(px)	(++(px)->nchar, (*(px)->pfn)((px)->arg, 0, 1))

int _WScanf(_WScin *pfn, void *arg, const wchar_t *fmt, va_list ap, int secure)
{
	int nconv = EOF;
	_WSft x;

	x.pfn = pfn;
	x.arg = arg;
	_Vacopy(&x.ap, ap);
	x.secure = secure;
	x.s = fmt;
	x.nchar = 0;
	for (;;) {
		wint_t ch;
		int code;

		for (; *x.s != L'%'; ++x.s) {	/* the text before the % */
			if (*x.s == L'\0')
				return nconv < 0 ? 0 : nconv;
			if (iswspace(*x.s)) {	/* white space matches any amount */
				while (iswspace(ch = GETN(&x)))
					;
				--x.nchar;
				if (ch != WEOF)
					(*x.pfn)(x.arg, ch, 0);
			} else if (*x.s != (ch = GETN(&x))) {	/* the text must match */
				if (ch != WEOF) {
					--x.nchar;
					(*x.pfn)(x.arg, ch, 0);
				}
				return nconv < 0 ? 0 : nconv;
			}
		}

		if (*++x.s == L'*') {	/* the conversion */
			x.noconv = '*';
			++x.s;
		} else {
			x.noconv = '\0';
		}
		for (x.width = 0; iswdigit(*x.s); ++x.s) {
			if (x.width < INT_MAX)
				x.width = x.width * 10 + *x.s - L'0';
		}
		if (*x.s != L'.') {
			x.prec = SIZE_MAX;
		} else if (*++x.s == L'*') {
			x.prec = va_arg(ap, int);	/* SCE: from the caller's list, not x.ap */
			++x.s;
		} else {
			for (x.prec = 0; iswdigit(*x.s); ++x.s) {
				x.prec = x.prec <= SIZE_MAX / 10 - (*x.s - L'0')
					? x.prec * 10 + *x.s - L'0' : SIZE_MAX;
			}
		}
		x.qual = wcschr(L"hjltzL", *x.s) != NULL ? *x.s++ : L'\0';
		if (x.qual == L'h' && *x.s == L'h') {
			x.qual = L'b';
			++x.s;
		} else if (x.qual == L'l' && *x.s == L'l') {
			x.qual = L'q';
			++x.s;
		}
		if (wcschr(L"cCn[", *x.s) == NULL) {	/* skip white space before the field */
			while (iswspace(ch = GETN(&x)))
				;
			--x.nchar;
			if (ch != WEOF)
				(*x.pfn)(x.arg, ch, 0);
		}
		if ((code = _WGetfld(&x)) <= 0)
			return 0 <= nconv ? nconv : code;
		if (nconv < 0)
			nconv = 0;
		if (x.stored)
			++nconv;
		++x.s;
	}
}
