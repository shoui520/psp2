/*
 * xmath.h: the library's private floating-point declarations.
 */
#ifndef _XMATH
#define _XMATH

#include <math.h>
#include <stddef.h>

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

/* the sign bits, tested in the value's whole word */
#define DSIGN(x)	(*(const unsigned long long *)&(x) & 0x8000000000000000ULL)
#define FSIGN(x)	(*(const unsigned int *)&(x) & 0x80000000U)

typedef union {
	unsigned short _Sh[4];
	double _Val;
} _Dval;

typedef union {
	unsigned short _Sh[2];
	float _Val;
} _Fval;

typedef union {	/* long double is double on psp2 */
	unsigned short _Sh[4];
	long double _Val;
} _Lval;

#define _L0		_D0
#define _LSIGN		_DSIGN
#define LSIGN(x)	(((_Lval){._Val = (x)})._Sh[_L0] & _LSIGN)


short _Dnorm(_Dval *ps);
short _Dscale(double *px, long lexp);
short _Dunscale(short *pex, double *px);
double _Dtento(double x, long n, int *perr);
short _FDnorm(_Fval *ps);
short _LDscale(long double *px, long lexp);
short _LDunscale(short *pex, long double *px);
long double _LDtento(long double x, long n, int *perr);
short _FDscale(float *px, long lexp);
short _FDunscale(short *pex, float *px);
float _FDtento(double x, long n, int *perr);

/*
 * The library's own extended format: a 64-bit fraction with its leading bit
 * at bit 63 and a 15-bit biased exponent, value 1.f * 2^(_Exp - 0x3fff).
 * strtod and _Dtento compute in it.
 */
typedef struct {
	int _Exp;
	unsigned long long _Frac;
} _Xfloat;

_Xfloat _Xfromf(float x);
float _Xtof(_Xfloat x, int *perr);
_Xfloat _Xfromld(long double x);
double _Xtod(_Xfloat x, int *perr);
_Xfloat _Xfromd(double x);
long double _Xtold(_Xfloat x, int *perr);
_Xfloat _Xmul(_Xfloat x, _Xfloat y);
_Xfloat _Xsub(_Xfloat x, _Xfloat y);
_Xfloat _Xadd(_Xfloat x, _Xfloat y);
_Xfloat _Xtento(_Xfloat x, int n);

int _Stopfx(const char **ps, char **endptr);
int _Stoflt(const char *s0, const char *s, char **endptr, long lo[], int maxsig);
int _Stoxflt(const char *s0, const char *s, char **endptr, long lo[], int maxsig);

/* the wide forms */
int _WStopfx(const wchar_t **ps, wchar_t **endptr);
int _WStoflt(const wchar_t *s0, const wchar_t *s, wchar_t **endptr, long lo[], int maxsig);
int _WStoxflt(const wchar_t *s0, const wchar_t *s, wchar_t **endptr, long lo[], int maxsig);
double _WStodx(const wchar_t *s, wchar_t **endptr, long pten, int *perr);
float _WStofx(const wchar_t *s, wchar_t **endptr, long pten, int *perr);
long double _WStoldx(const wchar_t *s, wchar_t **endptr, long pten, int *perr);

/* _Feraise's exceptions, as the ARM floating-point status bits */
#define _FE_DIVBYZERO	0x02
#define _FE_INEXACT	0x10
#define _FE_INVALID	0x01
#define _FE_OVERFLOW	0x04
#define _FE_UNDERFLOW	0x08

void _Feraise(int except);

/* the library's other constants */
extern const _Dconst _Eps, _Rteps;
extern const _Dconst _FEps, _FRteps;
extern const _Dconst _LEps, _LRteps;
extern const double _Xbig;	/* where e^-x stops mattering beside e^x */
extern const float _FXbig;
extern const long double _LXbig;
extern const double _Zero;	/* a zero the compiler cannot fold, for -0.0 */
extern const float _FZero;
extern const long double _LZero;

/* erf's ranges: its rational approximation below _Erf_small, 1 - erfc above */
extern double _Erf_big, _Erf_small;
extern float _FErf_big, _FErf_small;
extern long double _LErf_big, _LErf_small;

/* where lgamma and tgamma switch to Stirling's series */
extern double _Gamma_big;
extern float _FGamma_big;
extern long double _LGamma_big;

/* the classifiers */
short _Dtest(double *px);
short _FDtest(float *px);
short _LDtest(long double *px);

/* the integer part: clear the fraction bits below 2^xexp, return the class or the bits lost */
short _Dint(double *px, short xexp);
short _FDint(float *px, short xexp);
short _LDint(long double *px, short xexp);

/* extended precision: a value as the sum of n terms of decreasing magnitude */
double _Xp_getw(const double *p, int n);
double *_Xp_setw(double *p, int n, double x);
double *_Xp_addh(double *p, int n, double x0);
double *_Xp_mulh(double *p, int n, double x0);
double *_Xp_setn(double *p, int n, long x);
double *_Xp_movx(double *p, int n, const double *q);
double *_Xp_addx(double *p, int n, const double *q, int m);
double *_Xp_subx(double *p, int n, const double *q, int m);
double *_Xp_ldexpx(double *p, int n, int m);
double *_Xp_mulx(double *p, int n, const double *q, int m, double *ptemp2);
double *_Xp_invx(double *p, int n, double *ptemp4);
double *_Xp_sqrtx(double *p, int n, double *ptemp4);
float _FXp_getw(const float *p, int n);
float *_FXp_setw(float *p, int n, float x);
float *_FXp_addh(float *p, int n, float x0);
float *_FXp_mulh(float *p, int n, float x0);
float *_FXp_setn(float *p, int n, long x);
float *_FXp_movx(float *p, int n, const float *q);
float *_FXp_addx(float *p, int n, const float *q, int m);
float *_FXp_subx(float *p, int n, const float *q, int m);
float *_FXp_ldexpx(float *p, int n, int m);
float *_FXp_mulx(float *p, int n, const float *q, int m, float *ptemp2);
float *_FXp_invx(float *p, int n, float *ptemp4);
float *_FXp_sqrtx(float *p, int n, float *ptemp4);
long double _LXp_getw(const long double *p, int n);
long double *_LXp_setw(long double *p, int n, long double x);
long double *_LXp_addh(long double *p, int n, long double x0);
long double *_LXp_mulh(long double *p, int n, long double x0);
long double *_LXp_setn(long double *p, int n, long x);
long double *_LXp_movx(long double *p, int n, const long double *q);
long double *_LXp_addx(long double *p, int n, const long double *q, int m);
long double *_LXp_subx(long double *p, int n, const long double *q, int m);
long double *_LXp_ldexpx(long double *p, int n, int m);
long double *_LXp_mulx(long double *p, int n, const long double *q, int m, long double *ptemp2);
long double *_LXp_invx(long double *p, int n, long double *ptemp4);
long double *_LXp_sqrtx(long double *p, int n, long double *ptemp4);

/* the shared kernels of the functions */
double _Poly(double x, const double *tab, int n);
float _FPoly(float x, const float *tab, int n);
long double _LPoly(long double x, const long double *tab, int n);
short _Exp(double *px, double y, long eoff);
short _FExp(float *px, float y, long eoff);
short _LExp(long double *px, long double y, long eoff);
double _Atan(double x, int idx);
float _FAtan(float x, int idx);
long double _LAtan(long double x, int idx);
double _Log(double x, int baseflag);
float _FLog(float x, int baseflag);
long double _LLog(long double x, int baseflag);
/* the logarithm series' polynomial in z^2, for pow */
double _Logpoly(double w);
float _FLogpoly(float w);
long double _LLogpoly(long double w);
/* round to an integer in the current rounding mode */
double _Rint(double x);
float _FRint(float x);
long double _LRint(long double x);
double _Sin(double x, unsigned int qoff);
float _FSin(float x, unsigned int qoff);
long double _LSin(long double x, unsigned int qoff);
/* the same with a flag for quadrants already in x */
double _Sinx(double x, unsigned int qoff, int quads);
float _FSinx(float x, unsigned int qoff, int quads);
long double _LSinx(long double x, unsigned int qoff, int quads);
/* x^y as a fraction times 2^*pex, for tgamma */
double _Pow(double x, double y, short *pex);
float _FPow(float x, float y, short *pex);
long double _LPow(long double x, long double y, short *pex);
double _Cosh(double x, double y);
double _Erfc(double x);
double _Hypot(double x, double y, int *pexp);
double _Tgamma(double *px, short *pexp);
float _FTgamma(float *px, short *pexp);
long double _LTgamma(long double *px, short *pexp);
float _FHypot(float x, float y, int *pexp);
long double _LHypot(long double x, long double y, int *pexp);
float _FErfc(float x);
long double _LErfc(long double x);
float _FCosh(float x, float y);
long double _LCosh(long double x, long double y);
double _Sinh(double x, double y);
float _FSinh(float x, float y);
long double _LSinh(long double x, long double y);
/* reduce x to [-pi/4, pi/4], return the quadrant */
unsigned int _Quad(double *px);
unsigned int _FQuad(float *px);
unsigned int _LQuad(long double *px);
/* the same for x + phase pi */
unsigned int _Quadph(double *px, double phase);
unsigned int _FQuadph(float *px, float phase);
unsigned int _LQuadph(long double *px, long double phase);

#endif /* _XMATH */
