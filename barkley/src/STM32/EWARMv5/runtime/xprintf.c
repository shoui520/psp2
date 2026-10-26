/*
 * xprintf.c: the C library's printf engine (IAR DLib, Dinkumware's design), the variant
 * without floating point the image links: the format's text a character at a time,
 * each conversion built in a _Pft and written with its padding.
 */

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "xstdio.h"

static int _Putwstr(_Pft *px, const _Wchart *ws);
static void _Litob(_Pft *px, char code);
static int _Puttxt(_Pft *px, const char *s, int n);

/* the n characters of s */
#define PUT(px, s, n)	_Puttxt(px, s, n)

/* n copies of c */
static inline __attribute__((always_inline)) int _Pad(_Pft *px, char *pc, char c, int n)
{
	int i;

	*pc = c;
	for (i = 0; i < n; i++)
		if (_Puttxt(px, pc, 1) != 0)
			return -1;
	return 0;
}

/* the field built in px (its first n0 characters in ac), padded to its width */
static inline __attribute__((always_inline)) int _Putfld(_Pft *px, const char *ac, char *pc, int pad)
{
	if (!(px->flags & _FMI) && _Pad(px, pc, ' ', pad) != 0)
		return -1;
	if (_Puttxt(px, ac, px->n0) != 0)
		return -1;
	if (_Pad(px, pc, '0', px->nz1) != 0)
		return -1;
	if (_Puttxt(px, px->s, px->n1) != 0)
		return -1;
	if ((px->flags & _FMI) && _Pad(px, pc, ' ', pad) != 0)
		return -1;
	return 0;
}

/* a decimal number of the format, saturating */
#define GETNUM(n, s)						\
	for ((n) = 0; '0' <= *(s) && *(s) <= '9'; ++(s))	\
		if ((n) != INT_MAX)				\
			(n) = (n) * 10 + *(s) - '0'

int _Printf(void *(*pfn)(void *arg, char c), void *arg, const char *fmt, va_list *pap)
{
	char pad;
	_Wchart wc;
	_Wchart wcs[2];
	_Pft x;
	char ac[24];
	_Mbstatet state = { 0 };
	int n;
	int m;
	int result;
	char code;
	unsigned short flags;
	unsigned long long v;
	const _Wchart *ws;

	x.pfn = pfn;
	x.arg = arg;
	x.nchar = 0;
	for (;;) {
		wc = 0;
		n = _Mbtowc(&wc, fmt, (size_t)-1, &state);
		if (n <= 0)
			n = *fmt != '\0';
		m = wc == '%' ? n - 1 : n;
		if (m > 0 && PUT(&x, fmt, m) < 0)
			return -1;
		if (wc != '%') {
			if (wc == 0)
				return x.nchar;
			fmt += n;
			continue;
		}

		/* the flags, the width, the precision, the qualifier */
		flags = 0;
		x.n0 = 0;
		x.n1 = 0;
		x.nz1 = 0;
		for (;;) {
			code = *++fmt;
			if (code == ' ')
				flags |= _FSP;
			else if (code == '#')
				flags |= _FNO;
			else if (code == '+')
				flags |= _FPL;
			else if (code == '-')
				flags |= _FMI;
			else if (code == '0')
				flags |= _FZE;
			else
				break;
		}
		if (code == '*') {
			x.width = va_arg(*pap, int);
			if (x.width < 0) {
				x.width = -x.width;
				flags |= _FMI;
			}
			++fmt;
		} else {
			GETNUM(x.width, fmt);
		}
		if (*fmt == '.') {
			if (*++fmt == '*') {
				x.prec = va_arg(*pap, int);
				++fmt;
			} else {
				GETNUM(x.prec, fmt);
			}
		} else {
			x.prec = -1;
		}
		x.flags = flags;
		x.qual = strchr("hltzL", *fmt) != 0 ? *fmt++ : '\0';
		if (x.qual == 'h' && *fmt == 'h') {
			x.qual = 'b';
			++fmt;
		}

		/* the conversion */
		x.ac = ac;
		code = *fmt;
		result = 0;
		switch (code) {
		case 'c':
			if (x.qual == 'l') {
				wcs[0] = va_arg(*pap, int);
				wcs[1] = 0;
				x.prec = -1;
				result = _Putwstr(&x, wcs) < 0 ? -1 : 0;
			} else {
				ac[x.n0++] = va_arg(*pap, int);
			}
			break;
		case 'd':
		case 'i':
			v = va_arg(*pap, long);
			if (x.qual == 'h')
				v = (short)v;
			else if (x.qual == 'b')
				v = (signed char)v;
			if ((long)v < 0)
				ac[x.n0++] = '-';
			else if (x.flags & _FPL)
				ac[x.n0++] = '+';
			else if (x.flags & _FSP)
				ac[x.n0++] = ' ';
			x.s = &ac[x.n0];
			x.v = v;
			_Litob(&x, code);
			break;
		case 'o':
		case 'u':
		case 'x':
		case 'X':
			if (x.qual != 'l' && (x.qual == 'q' || x.qual == 'j'))
				v = va_arg(*pap, unsigned long long);
			else
				v = va_arg(*pap, unsigned long);
			if (x.qual == 'h')
				v = (unsigned short)v;
			else if (x.qual == 'b')
				v = (unsigned char)v;
			if ((x.flags & _FNO) && (unsigned long)v != 0 && (code | 0x20) == 'x') {
				ac[x.n0] = '0';
				ac[++x.n0] = code;
				++x.n0;
			}
			x.s = &ac[x.n0];
			x.v = v;
			_Litob(&x, code);
			break;
		case 'p':
			x.v = (long)va_arg(*pap, void *);
			x.s = ac;
			_Litob(&x, 'x');
			break;
		case 's':
			if (x.qual == 'l') {
				ws = va_arg(*pap, const _Wchart *);
				result = _Putwstr(&x, ws) < 0 ? -1 : 0;
			} else {
				const char *p;

				x.s = va_arg(*pap, char *);
				if (x.prec < 0) {
					x.n1 = strlen(x.s);
				} else {
					p = (const char *)memchr(x.s, '\0', x.prec);
					x.n1 = p != 0 ? p - x.s : x.prec;
				}
			}
			break;
		default:
			ac[x.n0++] = code == '\0' ? '%' : code;
			break;
		}
		++fmt;
		if (result != 0)
			return -1;
		if (_Putfld(&x, ac, &pad, x.width - x.n0 - x.nz1 - x.n1) != 0)
			return -1;
	}
}

/* a wide string (%lc, %ls) as multibyte characters, up to the precision's bytes, in a
   buffer as wide as the field */
static int _Putwstr(_Pft *px, const _Wchart *ws)
{
	static const _Mbstatet initial = { 0 };
	char pad;
	char mb[3];
	unsigned int size;
	int result;
	_Mbstatet state;
	char local[64];
	char *buf;
	int prec;
	int n;

	prec = px->prec;
	if (prec < 0)
		prec = INT_MAX;
	size = _Mbcurmax() + px->width;
	state = initial;
	if ((px->flags & _FMI) || size <= sizeof(local)) {
		buf = local;
		size = sizeof(local);
	} else {
		buf = (char *)malloc(size);
		if (buf == 0)
			return -1;
	}
	result = 0;
	for (; prec > 0; prec -= n) {
		n = _Wctomb(mb, *ws, &state);
		if (n < 0 || (*ws == 0 && --n < 0)) {
			result = -1;
			break;
		}
		if (prec < n)
			break;
		if (size < (unsigned int)(px->n0 + n)) {
			/* the buffer is full: out with it, unpadded */
			px->width = 0;
			_Putfld(px, buf, &pad, px->width - px->n0 - px->nz1 - px->n1);
			px->n0 = 0;
		}
		memcpy(buf + px->n0, mb, n);
		px->n0 += n;
		if (*ws++ == 0)
			break;
	}
	_Putfld(px, buf, &pad, px->width - px->n0 - px->nz1 - px->n1);
	px->n0 = 0;
	px->width = 0;
	if (buf != local)
		free(buf);
	return result;
}

/* an integer's digits, from the end of ac back to px->s, in the code's base */
static void _Litob(_Pft *px, char code)
{
	char *ac = px->ac;
	unsigned long base;
	unsigned long v;
	unsigned long q;
	int i = 24;
	char c;

	base = code == 'o' ? 8 : (code | 0x20) == 'x' ? 16 : 10;
	v = px->v;
	if ((code == 'd' || code == 'i') && px->v < 0)
		v = -v;
	if (v != 0 || px->prec != 0) {
		do {
			q = v / base;
			c = v - q * base + '0';
			if (c >= '9' + 1)
				c += code - 'Q';
			ac[--i] = c;
			v = q;
		} while (v != 0 && px->s < ac + i);
	}
	if (base == 8 && (px->flags & _FNO) && ac[i] != '0')
		ac[--i] = '0';
	px->n1 = 24 - i;
	px->s = ac + i;
	if (px->n1 < px->prec) {
		px->nz1 = px->prec - px->n1;
		px->flags &= ~_FZE;
	} else if (px->prec < 0 && (px->flags & (_FMI | _FZE)) == _FZE) {
		int n = px->width - px->n0 - px->nz1 - px->n1;

		if (n > 0)
			px->nz1 = n;
	}
}

static int _Puttxt(_Pft *px, const char *s, int n)
{
	for (; n != 0; n--) {
		px->arg = px->pfn(px->arg, *s++);
		if (px->arg == 0)
			return -1;
		px->nchar++;
	}
	return 0;
}
