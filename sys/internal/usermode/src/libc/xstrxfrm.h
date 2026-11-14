/*
 * xstrxfrm.h: the C library's collation transform. psp2 collates in the "C" locale only, so
 * _Strxfrm copies the source unchanged, a buffer at a time.
 */
#ifndef _XSTRXFRM
#define _XSTRXFRM

#include <stddef.h>

typedef struct _Xfrmctl {
	const char *s1;		/* the next source character */
	const char *s2;		/* the scan position within one _Strxfrm call */
	char state[16];		/* the conversion state; the "C" locale keeps none */
} _Xfrmctl;

size_t _Strxfrm(char *buf, size_t n, _Xfrmctl *ps);

#endif /* _XSTRXFRM */
