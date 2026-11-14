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
long _Fgpos(FILE *str, fpos_t *ptr);
FILE *_Foprep(const char *filename, const char *mods, FILE *str, _FD_TYPE fd, int share,
	      int reopen);

#endif /* _XSTDIO */
