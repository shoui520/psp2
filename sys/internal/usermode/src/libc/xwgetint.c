/*
 * _WGetint function: convert a wscanf integer field. The conversion gives the
 * base (0 for %i, which takes it from the prefix), the digits, with the 0 or
 * 0x prefix, are gathered into a buffer, and the buffer is converted with
 * _WStoll or _WStoull and stored in the qualifier's type.
 */
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

#include <xwchar.h>

#include "xwstdio.h"

#define BAD_ARGUMENT()	(EOF - _EXT1_FAIL("wscanf_s: bad integer argument"))

/* store val in the integer type T, at pans or through the next argument */
#define STORE_INT(px, pans, T, val)					\
	do {								\
		if ((pans) != NULL) {					\
			*(T *)(pans) = (T)(val);			\
		} else {						\
			T *p = va_arg((px)->ap, T *);			\
									\
			if (p == NULL)					\
				return BAD_ARGUMENT();			\
			*p = (T)(val);					\
		}							\
	} while (0)

int _WGetint(_WSft *px, void *pans)
{
	wchar_t ac[32];
	wchar_t *p;
	char seendig = 0;
	wint_t ch;
	int base;
	size_t dlen;
	static const wchar_t digits[22] = L"0123456789abcdefABCDEF";
	static const wchar_t flit[] = L"diouxXp";
	static const char barr[] = {10, 0, 8, 10, 16, 16, 16};

	base = barr[(const wchar_t *)wcschr(flit, *px->s) - flit];
	px->nget = 0 < px->width ? px->width : INT_MAX;
	p = ac;
	ch = GETN(px);
	if (ch == L'+' || ch == L'-') {	/* the sign */
		*p++ = ch;
		ch = GETN(px);
	}
	if (ch == L'0') {	/* a possible prefix */
		*p++ = ch;
		ch = GETN(px);
		if ((ch == L'x' || ch == L'X') && (base == 0 || base == 16)) {
			*p++ = ch;
			base = 16;
			ch = GETN(px);
		} else {
			seendig = 1;
			if (base == 0)
				base = 8;
		}
	}
	dlen = base == 0 || base == 10 ? 10 : base == 8 ? 8 : 16 + 6;
	for (; ch == L'0'; seendig = 1)	/* leading zeros as one */
		ch = GETN(px);
	if (seendig)
		*p++ = L'0';
	for (; ch != WEOF && wmemchr(digits, ch, dlen) != NULL; seendig = 1) {
		if (p < &ac[sizeof(ac) / sizeof(ac[0]) - 1])
			*p++ = ch;
		ch = GETN(px);
	}
	UNGETN(px, ch);
	if (!seendig)
		return p == ac && ch == WEOF ? EOF : 0;
	*p = L'\0';
	if (px->noconv) {
		;
	} else if (*px->s == L'd' || *px->s == L'i') {	/* a signed integer */
		long long lval = _WStoll(ac, NULL, base);

		px->stored = 1;
		switch (px->qual) {
		case 'b':
			STORE_INT(px, pans, signed char, lval);
			break;
		case 'q':
			STORE_INT(px, pans, long long, lval);
			break;
		case 'j':
			STORE_INT(px, pans, intmax_t, lval);
			break;
		case 't':
			STORE_INT(px, pans, ptrdiff_t, lval);
			break;
		case 'z':
			STORE_INT(px, pans, size_t, lval);
			break;
		case 'h':
			STORE_INT(px, pans, short, lval);
			break;
		case 'l':
			STORE_INT(px, pans, long, lval);
			break;
		default:
			STORE_INT(px, pans, int, lval);
			break;
		}
	} else {	/* an unsigned integer or a pointer */
		unsigned long long ulval = _WStoull(ac, NULL, base);

		px->stored = 1;
		if (*px->s == L'p') {
			STORE_INT(px, pans, void *, (uintptr_t)ulval);
		} else {
			switch (px->qual) {
			case 'b':
				STORE_INT(px, pans, unsigned char, ulval);
				break;
			case 'q':
				STORE_INT(px, pans, unsigned long long, ulval);
				break;
			case 'j':
				STORE_INT(px, pans, uintmax_t, ulval);
				break;
			case 't':
				STORE_INT(px, pans, ptrdiff_t, ulval);
				break;
			case 'z':
				STORE_INT(px, pans, size_t, ulval);
				break;
			case 'h':
				STORE_INT(px, pans, unsigned short, ulval);
				break;
			case 'l':
				STORE_INT(px, pans, unsigned long, ulval);
				break;
			default:
				STORE_INT(px, pans, unsigned int, ulval);
				break;
			}
		}
	}
	return 1;
}
