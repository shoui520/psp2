/*
 * xstdio.h: the C library's internal formatting interface (DLib, from Dinkumware's):
 * the state of one printf conversion, and the multibyte conversions it uses.
 */
#ifndef _XSTDIO_H
#define _XSTDIO_H

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

typedef unsigned short _Wchart;		/* the library's wchar_t */

/* a stream's mode */
enum {
	_MOPENR	= 0x0001,		/* opened for reading */
	_MOPENW	= 0x0002,		/* opened for writing */
	_MOPENA	= 0x0004,		/* appending */
	_MTRUNC	= 0x0008,
	_MCREAT	= 0x0010,
	_MBIN	= 0x0020,
	_MALBUF	= 0x0040,		/* the buffer is malloc's */
	_MALFIL	= 0x0080,		/* the FILE is malloc's */
	_MEOF	= 0x0100,
	_MERR	= 0x0200,
	_MLBF	= 0x0400,		/* line buffered */
	_MNBF	= 0x0800,		/* not buffered */
	_MREAD	= 0x1000,		/* reading */
	_MWRITE	= 0x2000,		/* writing */
	_MBYTE	= 0x4000,		/* byte oriented */
	_MWIDE	= 0x8000,		/* wide oriented */
};

/* the streams, open or not */
extern FILE *_Files[FOPEN_MAX];

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

int _Fwprep(FILE *str);
void _Close_all(void);
int _Printf(void *(*pfn)(void *arg, char c), void *arg, const char *fmt, va_list *pap);
int _Mbtowc(_Wchart *pwc, const char *s, size_t n, _Mbstatet *state);
int _Wctomb(char *s, _Wchart wc, _Mbstatet *state);
int _Mbcurmax(void);

#endif
