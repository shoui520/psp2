/*
 * xwstdio.h: the C library's wide formatted output: the print state _WPrintf keeps and the
 * conversions it calls (Dinkumware's names).
 */
#ifndef _XWSTDIO
#define _XWSTDIO

#include "xstdio.h"

/* wide print formatting information: _Pft with wide text */
typedef struct _WPft {
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
		long long d[1];
		float f[2];
		__fp16 hf[4];
	} vd;
	void *(*pfn)(void *, const wchar_t *, size_t);
	void *arg;
	wchar_t *s;
	int n0, nz0, n1, nz1, n2, nz2;
	int argno, prec, nchar, width;
	unsigned short flags;
	wchar_t qual;
	char secure;
} _WPft;

/* wide scanf formatting information: _Sft with a wide format */
typedef struct _WSft {
	_WScin *pfn;		/* reads (getfl 1) or pushes back (getfl 0) a character */
	void *arg;
	va_list ap;
	const wchar_t *s;	/* the format */
	int nchar;		/* characters read */
	int nget;		/* characters left in the field */
	int width;
	size_t prec;		/* SCE: the precision, ".n" or ".*" */
	wchar_t qual;
	char noconv;		/* '*': convert without storing */
	char stored;
	char secure;
} _WSft;

int _WGetfld(_WSft *px);
int _WGetfloat(_WSft *px, void *pans);
int _WGetint(_WSft *px, void *pans);
int _WGetstr(_WSft *px, int sfl);
void _WLdtob(_WPft *px, wchar_t code);
void _WLitob(_WPft *px, wchar_t code);
void _WGenld(_WPft *px, wchar_t code, wchar_t *p, short nsig, short xexp);

#endif /* _XWSTDIO */
