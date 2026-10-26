/*
 * xstdio.h: the C library's internal formatting interface (DLib, from Dinkumware's):
 * the state of one printf conversion, and the multibyte conversions it uses.
 */
#ifndef _XSTDIO_H
#define _XSTDIO_H

#include <stdarg.h>
#include <stddef.h>

typedef unsigned short _Wchart;		/* the library's wchar_t */

typedef struct {
	unsigned long wchar;
	unsigned long state;
} _Mbstatet;

/* _Pft's flags */
enum {
	_FSP	= 0x01,			/* ' ' */
	_FPL	= 0x02,			/* '+' */
	_FMI	= 0x04,			/* '-' */
	_FNO	= 0x08,			/* '#' */
	_FZE	= 0x10,			/* '0' */
};

/* one conversion: the value, the output function, the text built: n0 characters of
   ac (sign, prefix, a %c), nz1 zeros, n1 characters at s */
typedef struct {
	long v;
	void *(*pfn)(void *arg, char c);
	void *arg;
	char *s;
	char *ac;
	int n0;
	int n1;
	int nz1;
	int nchar;
	int prec;
	int width;
	unsigned short flags;
	char qual;
} _Pft;

int _Printf(void *(*pfn)(void *arg, char c), void *arg, const char *fmt, va_list *pap);
int _Mbtowc(_Wchart *pwc, const char *s, size_t n, _Mbstatet *state);
int _Wctomb(char *s, _Wchart wc, _Mbstatet *state);
int _Mbcurmax(void);

#endif
