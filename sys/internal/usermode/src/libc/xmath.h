/*
 * xmath.h: the library's private floating-point declarations.
 */
#ifndef _XMATH
#define _XMATH

/* what _Stopfx finds before a floating-point field */
#define FL_ERR		0	/* not a number */
#define FL_DEC		1	/* decimal digits */
#define FL_HEX		2	/* 0x and hexadecimal digits */
#define FL_INF		3	/* inf or infinity */
#define FL_NAN		4	/* nan, with an optional (n-char-sequence) */
#define FL_NEG		8	/* a minus sign */

/* the double layout, in 16-bit words, little-endian */
#define _D0		3	/* the sign, exponent and high fraction */
#define _D1		2
#define _D2		1
#define _D3		0	/* the low fraction */
#define _DOFF		4	/* the exponent's position in _D0 */
#define _DBIAS		0x3fe
#define _DFRAC		((unsigned short)((1 << _DOFF) - 1))
#define _DMASK		((unsigned short)(0x7fff & ~_DFRAC))
#define _DMAX		((unsigned short)((1 << (15 - _DOFF)) - 1))
#define _DSIGN		((unsigned short)0x8000)

/* the float layout */
#define _F0		1
#define _F1		0
#define _FOFF		7
#define _FBIAS		0x7e
#define _FFRAC		((unsigned short)((1 << _FOFF) - 1))
#define _FMASK		((unsigned short)(0x7fff & ~_FFRAC))
#define _FMAX		((unsigned short)((1 << (15 - _FOFF)) - 1))
#define _FSIGN		((unsigned short)0x8000)

/* what the classifiers return */
#define _FINITE		-1
#define _INFCODE	1
#define _NANCODE	2

typedef union {
	unsigned short _Sh[4];
	double _Val;
} _Dval;

typedef union {
	unsigned short _Sh[2];
	float _Val;
} _Fval;

/* the library's constants (exported variables) */
typedef union {
	unsigned short _Word[8];
	float _Float;
	double _Double;
	long double _Long_double;
} _Dconst;

extern const _Dconst _Denorm, _Hugeval, _Inf, _Nan, _Snan;
extern const _Dconst _FDenorm, _FInf, _FNan, _FSnan;
extern const _Dconst _LDenorm, _LInf, _LNan, _LSnan;

short _Dnorm(_Dval *ps);
short _Dscale(double *px, long lexp);
double _Dtento(double x, long n, int *perr);
short _FDnorm(_Fval *ps);
short _FDscale(float *px, long lexp);
float _FDtento(float x, long n, int *perr);

int _Stopfx(const char **ps, char **endptr);
int _Stoflt(const char *s0, const char *s, char **endptr, long lo[], int maxsig);
int _Stoxflt(const char *s0, const char *s, char **endptr, long lo[], int maxsig);

#endif /* _XMATH */
