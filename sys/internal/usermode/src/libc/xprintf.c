/*
 * _Printf function: Dinkumware's formatted output engine, with SCE's vector conversions
 * (the v and w qualifiers print the elements of a NEON vector, separated by ' ' or the
 * flag's character).
 */
#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "xstdio.h"
#include "xwchar.h"

#define MAX_PAD		(sizeof(spaces) - 1)	/* the longest run PAD writes at once */
#define TXT_BUF		64			/* _Putstr's output buffer */

/* write n bytes of s */
#define PUT(px, s, n)							\
	if (0 < (n)) {							\
		if (((px)->arg = (*(px)->pfn)((px)->arg, s, n)) == NULL)	\
			return EOF;					\
		(px)->nchar += (n);					\
	}

/* write n bytes from the run s, MAX_PAD at a time */
#define PAD(px, s, n)							\
	if (0 < (n)) {							\
		int i, j = (n);						\
									\
		for (; 0 < j; j -= i) {					\
			i = MAX_PAD < (size_t)j ? MAX_PAD : j;		\
			PUT(px, s, i);					\
		}							\
	}

/* take the argument numbered px->argno (or the next one) into dst */
#define GETVAL(px, dst, val)						\
	while (0 <= (px)->argno) {					\
		dst = val;						\
		--(px)->argno;						\
	}
#define GETARG(px, dst, type)	GETVAL(px, dst, va_arg(*pap, type))

static const char spaces[] = "                                ";
static const char zeroes[] = "00000000000000000000000000000000";
static const char fchar[] = " +-#0,;:_";
static const unsigned int fbit[] = {
	_FSP, _FPL, _FMI, _FNO, _FZE, _FVCOMMA, _FVSEMI, _FVCOLON, _FVUNDER, 0
};

/* write the field in ac and px->s with its padding */
static __attribute__((noinline)) int _Puttxt(_Pft *px, const char *ac)
{
	int width = px->width - px->n0 - px->nz0 - px->n1 - px->nz1 - px->n2 - px->nz2;

	if (px->flags & _FVSEP) {	/* SCE: separate a vector element */
		if (px->flags & _FVCOMMA) {
			PAD(px, ",", 1);
		} else if (px->flags & _FVSEMI) {
			PAD(px, ";", 1);
		} else if (px->flags & _FVCOLON) {
			PAD(px, ":", 1);
		} else if (px->flags & _FVUNDER) {
			PAD(px, "_", 1);
		} else {
			PAD(px, " ", 1);
		}
	}
	if (!(px->flags & _FMI))
		PAD(px, spaces, width);
	PUT(px, ac, px->n0);
	PAD(px, zeroes, px->nz0);
	PUT(px, px->s, px->n1);
	PAD(px, zeroes, px->nz1);
	PUT(px, px->s + px->n1, px->n2);
	PAD(px, zeroes, px->nz2);
	if (px->flags & _FMI)
		PAD(px, spaces, width);
	return 0;
}

/* the bytes of pwc as multibyte characters that fit prec and width, -1 if one is bad */
static int _Mbslen(const wchar_t *pwc, int prec, int width)
{
	char buf[_MBMAX];
	mbstate_t mbst = {0};
	size_t n;
	int m;

	for (n = 0;; ++pwc) {
		m = _Wctomb(buf, *pwc, &mbst);
		if (m < 0 || (*pwc == L'\0' && --m < 0))
			return -1;
		if (prec < n + m)
			break;
		n += m;
		if (width < n || *pwc == L'\0')
			break;
	}
	return n <= width ? n : 0;
}

/* write the wide string pwc as multibyte characters */
static __attribute__((noinline)) int _Putstr(_Pft *px, const wchar_t *pwc)
{
	char tmp[_MBMAX];
	char ac[TXT_BUF];
	mbstate_t mbst = {0};
	int prec = px->prec;
	int width = px->width;
	int left = 0 <= prec ? prec : INT_MAX;
	int len = _Mbslen(pwc, prec, width);
	int n, m, ans;

	if (len < 0)
		return -1;
	if (0 < len) {
		if (px->flags & _FMI) {
			len = px->width - len;
			px->width = 0;
		} else if (TXT_BUF < (unsigned int)px->width) {
			px->width = px->width - len + TXT_BUF;
		}
	} else if (len == 0) {
		px->width = 0;
	}
	if (left <= 0)
		goto flush;
	ans = 0;
	do {
		m = _Wctomb(tmp, *pwc, &mbst);
		if (m < 0 || (*pwc == L'\0' && --m < 0)) {
			ans = -1;
			goto done;
		}
		if (left < m)
			break;
		n = px->n0;
		if (TXT_BUF < (unsigned int)(n + m)) {	/* the buffer is full */
			ans = _Puttxt(px, ac);
			if (ans < 0)
				break;
			n = 0;
			px->width = 0;
			px->n0 = 0;
		}
		memcpy(&ac[n], tmp, m);
		px->n0 += m;
		if (*pwc == L'\0')
			break;
		left -= m;
		++pwc;
	} while (0 < left);
	if (ans == 0) {
flush:
		if ((px->flags & _FMI) && 0 < len)
			px->width = len + px->n0;
		ans = _Puttxt(px, ac);
		if (ans == 0) {
			px->width = 0;
			px->n0 = 0;
		}
	}
done:
	return ans;
}

/* convert one field, code, into ac and write it */
static __attribute__((noinline)) int _Putfld(_Pft *px, va_list *pap, char code, char *ac)
{
	unsigned short flags;
	int i, n = 1, prec;
	long double ld;

	switch (code) {
	case 'c':
		if (px->qual == 'v' || px->qual == 'w') {	/* SCE: the bytes of a vector */
			if (px->qual == 'v') {
				GETARG(px, px->vd.q, int8x8_t);
				flags = px->flags;
				n = 8;
			} else {
				GETARG(px, px->vq.q, int8x16_t);
				flags = px->flags;
				n = 16;
			}
			px->flags = flags & ~_FVSEP;
			i = 0;
			do {
				px->n2 = 0;
				px->nz2 = 0;
				px->n1 = 0;
				px->nz1 = 0;
				px->nz0 = 0;
				if (px->qual == 'v')
					ac[px->n0++] = px->vd.c[i];
				else
					ac[px->n0++] = px->vq.c[i];
				if (_Puttxt(px, ac) < 0)
					return EOF;
				++i;
				px->n0 = 0;
				px->flags = flags;
			} while (i < n);
		} else if (px->qual == 'l') {
			wchar_t wc = 0;
			wchar_t wcs[2];

			GETARG(px, wc, int);
			wcs[0] = wc;
			wcs[1] = L'\0';
			px->prec = -1;
			if (_Putstr(px, wcs) < 0)
				return EOF;
		} else {
			char ch = 0;

			GETARG(px, ch, int);
			ac[px->n0++] = ch;
			if (_Puttxt(px, ac) < 0)
				return EOF;
		}
		break;

	case 'd':
	case 'i':	/* signed decimal */
		if (px->qual == 'g') {
			GETARG(px, px->vd.q, int8x8_t);
			n = 8;
		} else if (px->qual == 'c') {
			GETARG(px, px->vd.q, int8x8_t);
			n = 4;
		} else if (px->qual == 'v') {
			GETARG(px, px->vd.q, int8x8_t);
			n = 2;
		} else if (px->qual == 'a') {
			GETARG(px, px->vd.q, int8x8_t);
		} else if (px->qual == 'e') {
			GETARG(px, px->vq.q, int8x16_t);
			n = 16;
		} else if (px->qual == 'd') {
			GETARG(px, px->vq.q, int8x16_t);
			n = 8;
		} else if (px->qual == 'w') {
			GETARG(px, px->vq.q, int8x16_t);
			n = 4;
		} else if (px->qual == 'i') {
			GETARG(px, px->vq.q, int8x16_t);
			n = 2;
		} else {
			GETVAL(px, px->v.li, px->qual == 'l' ? va_arg(*pap, long)
				: px->qual == 'q' ? va_arg(*pap, long long)
				: px->qual == 'j' ? va_arg(*pap, intmax_t)
				: px->qual == 't' ? va_arg(*pap, ptrdiff_t)
				: px->qual == 'z' ? (ptrdiff_t)va_arg(*pap, size_t)
				: va_arg(*pap, int));
		}
		if (px->qual == 'h')
			px->v.li = (short)px->v.li;
		else if (px->qual == 'b')
			px->v.li = (signed char)px->v.li;
		else if (px->qual == 't' || px->qual == 'z')
			px->v.li = (long)px->v.li;
		flags = px->flags;
		px->flags = flags & ~_FVSEP;
		for (i = 0; i < n; i++) {
			px->n2 = 0;
			px->nz2 = 0;
			px->n1 = 0;
			px->nz1 = 0;
			px->nz0 = 0;
			if (px->qual == 'g')
				px->v.li = px->vd.c[i];
			else if (px->qual == 'c')
				px->v.li = px->vd.h[i];
			else if (px->qual == 'v')
				px->v.li = px->vd.w[i];
			else if (px->qual == 'a')
				px->v.li = px->vd.d;
			else if (px->qual == 'e')
				px->v.li = px->vq.c[i];
			else if (px->qual == 'd')
				px->v.li = px->vq.h[i];
			else if (px->qual == 'w')
				px->v.li = px->vq.w[i];
			else if (px->qual == 'i')
				px->v.li = px->vq.d[i];
			if (px->v.li < 0)
				ac[px->n0++] = '-';
			else if (px->flags & _FPL)
				ac[px->n0++] = '+';
			else if (px->flags & _FSP)
				ac[px->n0++] = ' ';
			px->s = &ac[px->n0];
			_Litob(px, code);
			if (_Puttxt(px, ac) < 0)
				return EOF;
			px->n0 = 0;
			px->flags = flags;
		}
		break;

	case 'o':
	case 'u':
	case 'x':
	case 'X':	/* unsigned */
		if (px->qual == 'g') {
			GETARG(px, px->vd.q, int8x8_t);
			n = 8;
		} else if (px->qual == 'c') {
			GETARG(px, px->vd.q, int8x8_t);
			n = 4;
		} else if (px->qual == 'v') {
			GETARG(px, px->vd.q, int8x8_t);
			n = 2;
		} else if (px->qual == 'a') {
			GETARG(px, px->vd.q, int8x8_t);
		} else if (px->qual == 'e') {
			GETARG(px, px->vq.q, int8x16_t);
			n = 16;
		} else if (px->qual == 'd') {
			GETARG(px, px->vq.q, int8x16_t);
			n = 8;
		} else if (px->qual == 'w') {
			GETARG(px, px->vq.q, int8x16_t);
			n = 4;
		} else if (px->qual == 'i') {
			GETARG(px, px->vq.q, int8x16_t);
			n = 2;
		} else {
			GETVAL(px, px->v.li, px->qual == 'l' ? va_arg(*pap, unsigned long)
				: px->qual == 'q' ? va_arg(*pap, unsigned long long)
				: px->qual == 'j' ? va_arg(*pap, uintmax_t)
				: px->qual == 't' ? va_arg(*pap, ptrdiff_t)
				: va_arg(*pap, unsigned int));
		}
		if (px->qual == 'h')
			px->v.li = (unsigned short)px->v.li;
		else if (px->qual == 'b')
			px->v.li = (unsigned char)px->v.li;
		else if (px->qual == 't' || px->qual == 'z')
			px->v.li = (size_t)px->v.li;
		flags = px->flags;
		px->flags = flags & ~_FVSEP;
		for (i = 0; i < n; i++) {
			px->n2 = 0;
			px->nz2 = 0;
			px->n1 = 0;
			px->nz1 = 0;
			px->nz0 = 0;
			if (px->qual == 'g')
				px->v.li = (unsigned char)px->vd.c[i];
			else if (px->qual == 'c')
				px->v.li = (unsigned short)px->vd.h[i];
			else if (px->qual == 'v')
				px->v.li = (unsigned int)px->vd.w[i];
			else if (px->qual == 'a')
				px->v.li = px->vd.d;
			else if (px->qual == 'e')
				px->v.li = (unsigned char)px->vq.c[i];
			else if (px->qual == 'd')
				px->v.li = (unsigned short)px->vq.h[i];
			else if (px->qual == 'w')
				px->v.li = (unsigned int)px->vq.w[i];
			else if (px->qual == 'i')
				px->v.li = px->vq.d[i];
			if ((px->flags & _FNO) && px->v.li != 0 && (code == 'x' || code == 'X')) {
				ac[px->n0++] = '0';
				ac[px->n0++] = code;
			}
			px->s = &ac[px->n0];
			_Litob(px, code);
			if (_Puttxt(px, ac) < 0)
				return EOF;
			px->n0 = 0;
			px->flags = flags;
		}
		break;

	case 'e':
	case 'E':
	case 'f':
	case 'F':
	case 'g':
	case 'G':
	case 'a':
	case 'A':	/* floating point */
		if (px->qual == 'v') {
			GETARG(px, px->vd.q, int8x8_t);
			n = 2;
		} else if (px->qual == 'w') {
			GETARG(px, px->vq.q, int8x16_t);
			n = 4;
		} else if (px->qual == 'c') {
			GETARG(px, px->vd.q, int8x8_t);
			n = 4;
		} else if (px->qual == 'd') {
			GETARG(px, px->vq.q, int8x16_t);
			n = 8;
		} else {
			GETARG(px, px->v.ld, double);
		}
		flags = px->flags;
		prec = px->prec;
		px->flags = flags & ~_FVSEP;
		for (i = 0; i < n; i++) {
			px->n2 = 0;
			px->nz2 = 0;
			px->n1 = 0;
			px->nz1 = 0;
			px->nz0 = 0;
			if (px->qual == 'v')
				px->v.ld = px->vd.f[i];
			else if (px->qual == 'w')
				px->v.ld = px->vq.f[i];
			else if (px->qual == 'c')
				px->v.ld = px->vd.hf[i];
			else if (px->qual == 'd')
				px->v.ld = px->vq.hf[i];
			ld = px->v.ld;
			if (((unsigned short *)&ld)[3] & 0x8000)
				ac[px->n0++] = '-';
			else if (px->flags & _FPL)
				ac[px->n0++] = '+';
			else if (px->flags & _FSP)
				ac[px->n0++] = ' ';
			px->s = &ac[px->n0];
			_Ldtob(px, code);
			if (_Puttxt(px, ac) < 0)
				return EOF;
			px->n0 = 0;
			px->flags = flags;
			px->prec = prec;
		}
		break;

	case 'p':	/* a pointer, in hexadecimal */
		{
			void *p = NULL;

			GETARG(px, p, void *);
			px->v.li = (size_t)p;
		}
		px->s = &ac[px->n0];
		_Litob(px, 'x');
		if (_Puttxt(px, ac) < 0)
			return EOF;
		break;

	case 's':
		if (px->qual == 'l') {	/* a wide string */
			const wchar_t *ws = NULL;

			GETARG(px, ws, wchar_t *);
			if (ws == NULL) {
				if (px->secure)
					return -1 - _EXT1_FAIL("printf_s: bad %ls argument");
				ws = L"(null)";
			}
			if (_Putstr(px, ws) < 0)
				return EOF;
		} else {
			GETARG(px, px->s, char *);
			if (px->s == NULL) {
				if (px->secure)
					return -1 - _EXT1_FAIL("printf_s: bad %s argument");
				px->s = "(null)";
				n = strlen(px->s);
			} else if (0 <= px->prec) {
				const char *t = memchr(px->s, '\0', px->prec);

				n = t != NULL ? t - px->s : px->prec;
			} else {
				n = strlen(px->s);
			}
			px->n1 = n;
			if (_Puttxt(px, ac) < 0)
				return EOF;
		}
		break;

	case '%':	/* a literal % */
		ac[px->n0++] = '%';
		if (_Puttxt(px, ac) < 0)
			return EOF;
		break;

	default:	/* a bad conversion: write it as it is */
		ac[px->n0++] = '%';
		if (code != '\0')
			ac[px->n0++] = code;
		if (_Puttxt(px, ac) < 0)
			return EOF;
		break;
	}
	return 0;
}

int _Printf(_Prout *pfn, void *arg, const char *fmt, va_list ap, int secure)
{
	_Pft x;
	va_list ap2, apn;
	mbstate_t mbst = {0};
	wchar_t wc;

	x.pfn = pfn;
	x.arg = arg;
	_Vacopy(&ap2, ap);
	x.secure = secure;
	x.nchar = 0;
	for (;;) {
		const char *s;
		int n, m, argno, ans;
		char next;

		wc = 0;
		n = _Mbtowc(&wc, fmt, INT_MAX, &mbst);
		if (n <= 0)
			n = *fmt != '\0';
		m = wc == L'%' ? n - 1 : n;
		if (0 < m) {	/* the text before the % */
			if ((x.arg = (*x.pfn)(x.arg, fmt, m)) == NULL)
				return EOF;
			x.nchar += m;
		}
		fmt += n;
		if (wc != L'%') {
			if (wc == L'\0')
				return x.nchar;
			continue;
		}

		argno = 0;	/* an argument number: n$ */
		x.argno = 0;
		s = fmt;
		do {
			if (!isdigit((unsigned char)*fmt))
				break;
			argno = argno * 10 + (unsigned char)*fmt - '0';
			x.argno = argno;
			++fmt;
		} while (argno < 1000);
		if (*fmt++ != '$' || argno <= 0 || 1000 <= argno) {
			x.argno = 0;
			fmt = s;
		}

		x.n2 = 0;	/* the flags */
		x.nz2 = 0;
		x.n1 = 0;
		x.nz1 = 0;
		x.n0 = 0;
		x.nz0 = 0;
		x.flags = 0;
		for (s = fmt;; ++s) {
			const char *t = memchr(fchar, *s, sizeof(fchar) - 1);

			if (t == NULL)
				break;
			x.flags |= fbit[t - fchar];
		}

		if (*s == '*') {	/* the width */
			x.width = va_arg(ap2, int);
			if (x.width < 0) {
				x.width = -x.width;
				x.flags |= _FMI;
			}
			++s;
		} else {
			for (x.width = 0; isdigit((unsigned char)*s); ++s) {
				if (x.width < INT_MAX)
					x.width = x.width * 10 + (unsigned char)*s - '0';
			}
		}

		if (*s != '.') {	/* the precision */
			x.prec = -1;
		} else if (*++s == '*') {
			x.prec = va_arg(ap2, int);
			++s;
		} else {
			char minus = 0;	/* SCE: a negative precision counts as none */

			if (*s == '-')
				minus = *s++;
			for (x.prec = 0; isdigit((unsigned char)*s); ++s) {
				if (minus == 0 && x.prec < INT_MAX)
					x.prec = x.prec * 10 + (unsigned char)*s - '0';
			}
		}

		x.qual = strchr("hjltzLvw", *s) != NULL ? *s++ : '\0';	/* the qualifier */
		next = s[1];
		if (x.qual == 'h' && *s == 'h') {
			x.qual = 'b';
			++s;
		} else if (x.qual == 'l' && *s == 'l') {
			x.qual = 'q';
			++s;
		} else {	/* SCE: a vector of hh, h, l or ll elements */
			if (x.qual == 'v') {
				if (*s == 'l') {
					if (next == 'l') {
						x.qual = 'a';
						s += 2;
						goto vector;
					}
					++s;
				} else if (*s == 'h') {
					if (next == 'h') {
						x.qual = 'g';
						s += 2;
					} else {
						x.qual = 'c';
						++s;
					}
					goto vector;
				}
			} else if (x.qual == 'w') {
				if (*s == 'l') {
					if (next == 'l') {
						x.qual = 'i';
						s += 2;
						goto vector;
					}
					++s;
				} else if (*s == 'h') {
					if (next == 'h') {
						x.qual = 'e';
						s += 2;
					} else {
						x.qual = 'd';
						++s;
					}
					goto vector;
				}
			}
			if (x.qual == 'v' || x.qual == 'a' || x.qual == 'g' || x.qual == 'c'
			    || x.qual == 'w' || x.qual == 'i' || x.qual == 'e' || x.qual == 'd') {
vector:
				if ((*s != 'c' || (*s == 'c' && (x.flags & _FSP)))
				    && !(x.flags & (_FVSEP & ~_FVSP)))
					x.flags |= _FVSP;
			}
		}

		{	/* the conversion */
			char ac[72] = {0};

			if (0 < x.argno) {
				--x.argno;
				_Vacopy(&apn, ap2);
				ans = _Putfld(&x, &apn, *s++, ac);
				fmt = s;
				if (ans != 0)
					return ans;
			} else {
				ans = _Putfld(&x, &ap2, *s++, ac);
				fmt = s;
				if (ans != 0)
					return ans;
			}
		}
	}
}
