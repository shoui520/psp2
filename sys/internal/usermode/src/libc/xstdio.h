/*
 * xstdio.h: Dinkumware's internal stdio declarations, as SceLibc uses them: the stream mode
 * bits and the functions its stdio files share. Names are Dinkumware's.
 */
#ifndef _XSTDIO
#define _XSTDIO

#include <stdarg.h>
#include <stdio.h>
#include <wchar.h>

/* FILE._Mode bits */
#define _MOPENR		0x1	/* open for reading */
#define _MOPENW		0x2	/* open for writing */
#define _MOPENA		0x4	/* open for appending */
#define _MTRUNC		0x8	/* truncate on open */
#define _MCREAT		0x10	/* create on open */
#define _MBIN		0x20	/* binary stream */
#define _MALBUF		0x40	/* the library allocated the buffer */
#define _MALFIL		0x80	/* the library allocated the FILE */
#define _MEOF		0x100	/* end of file seen */
#define _MERR		0x200	/* error seen */
#define _MLBF		0x400	/* line buffered */
#define _MNBF		0x800	/* unbuffered */
#define _MREAD		0x1000	/* last operation was a read */
#define _MWRITE		0x2000	/* last operation was a write */
#define _MBYTE		0x4000	/* byte oriented */
#define _MWIDE		0x8000	/* wide oriented */

/* _Pft.flags: the print flags */
#define _FSP		0x01	/* ' ' */
#define _FPL		0x02	/* '+' */
#define _FMI		0x04	/* '-' */
#define _FNO		0x08	/* '#' */
#define _FZE		0x10	/* '0' */
#define _FVSP		0x20	/* SCE: separate vector elements with ' ' (the default) */
#define _FVCOMMA	0x40	/* SCE: ',' separates them */
#define _FVSEMI		0x80	/* SCE: ';' */
#define _FVCOLON	0x100	/* SCE: ':' */
#define _FVUNDER	0x200	/* SCE: '_' */
#define _FVSEP		(_FVSP | _FVCOMMA | _FVSEMI | _FVCOLON | _FVUNDER)

/* print formatting information */
typedef struct _Pft {
	union {	/* the value being converted */
		long long li;
		long double ld;
	} v;
	union {	/* SCE: a 128-bit vector argument (the w qualifiers) */
		int8x16_t q;
		signed char c[16];
		short h[8];
		int w[4];
		long long d[2];
		float f[4];
		__fp16 hf[8];
	} vq;
	union {	/* SCE: a 64-bit vector argument (the v qualifiers) */
		int8x8_t q;
		signed char c[8];
		short h[4];
		int w[2];
		long long d;
		float f[2];
		__fp16 hf[4];
	} vd;
	void *(*pfn)(void *, const char *, size_t);
	void *arg;
	char *s;
	int n0, nz0, n1, nz1, n2, nz2;
	int argno, prec, nchar, width;
	unsigned short flags;
	char qual;
	char secure;
} _Pft;

/* scanf formatting information */
typedef struct _Sft {
	int (*pfn)(void *, int, int);	/* reads (getfl 1) or pushes back (getfl 0) a character */
	void *arg;
	va_list ap;
	const char *s;			/* the format */
	int nchar;			/* characters read */
	int nget;			/* characters left in the field */
	int width;
	size_t prec;			/* SCE: the precision, ".n" or ".*" */
	char noconv;			/* '*': convert without storing */
	char qual;
	char stored;
	char secure;
} _Sft;

int _Getfld(_Sft *px);
int _Getfloat(_Sft *px, void *pans);
int _Getint(_Sft *px, void *pans);
int _Getstr(_Sft *px, int sfl);

/* read a character, counting it */
#define GET(px)		(++(px)->nchar, (*(px)->pfn)((px)->arg, 0, 1))

/* read a character of the field: EOF once the field's width is used up */
#define GETN(px)	(0 <= --(px)->nget ? GET(px) : (++(px)->nchar, EOF))

/* push back the character ch, uncounting it */
#define UNGET(px, ch)							\
	do {								\
		--(px)->nchar;						\
		(*(px)->pfn)((px)->arg, ch, 0);				\
	} while (0)

/* push back the character ch unless it is EOF, uncounting it */
#define UNGETN(px, ch)							\
	do {								\
		if ((ch) != EOF)					\
			UNGET(px, ch);					\
		else							\
			--(px)->nchar;					\
	} while (0)

void _Vacopy(va_list *pap, va_list ap);
void _Ldtob(_Pft *px, char code);
void _Litob(_Pft *px, char code);

/* the formatted I/O engines: pfn writes or reads through arg */
typedef void *_Prout(void *arg, const char *buf, size_t n);
typedef int _Scin(void *arg, int ch, int getfl);
typedef void *_WProut(void *arg, const wchar_t *buf, size_t n);
typedef unsigned int _WScin(void *arg, unsigned int ch, int getfl);

int _Printf(_Prout *pfn, void *arg, const char *fmt, va_list ap, int secure);
int _Scanf(_Scin *pfn, void *arg, const char *fmt, va_list ap, int secure);
int _WPrintf(_WProut *pfn, void *arg, const wchar_t *fmt, va_list ap, int secure);
int _WScanf(_WScin *pfn, void *arg, const wchar_t *fmt, va_list ap, int secure);

/* opening streams */
FILE *_Fofind(void);
void _Fofree(FILE *str, int exiting);

/* buffering and positioning */
int _Frprep(FILE *str);
int _WFrprep(FILE *str);
int _Fwprep(FILE *str);
int _WFwprep(FILE *str);
long _Fbytes(FILE *str, const unsigned char *p1, const unsigned char *p2);
long long _Fgpos(FILE *str, fpos_t *ptr);
int _Fspos(FILE *str, const fpos_t *ptr, long long off, int way);
FILE *_Foprep(const char *filename, const char *mods, FILE *str, _FD_TYPE fd, int share,
	      int reopen);

#endif /* _XSTDIO */
