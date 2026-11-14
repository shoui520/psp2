/*
 * math.h: the C library's mathematics declarations (Dinkumware's, as the SDK Release
 * 03.570.011 ships them in target/include/math.h and ymath.h): the functions SceLibm exports
 * and the values they return. long double is double on psp2.
 */
#ifndef _MATH
#define _MATH

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

#define HUGE_VAL	_Hugeval._Double
#define HUGE_VALF	_FInf._Float
#define HUGE_VALL	_LInf._Long_double
#define INFINITY	_FInf._Float
#define NAN		_FNan._Float

/* what _Dtest and fpclassify return */
#define _DENORM		(-2)
#define _FINITE		(-1)
#define _INFCODE	1
#define _NANCODE	2

#define FP_INFINITE	_INFCODE
#define FP_NAN		_NANCODE
#define FP_NORMAL	_FINITE
#define FP_SUBNORMAL	_DENORM
#define FP_ZERO		0

#define FP_ILOGB0	(-0x7fffffff - 1)
#define FP_ILOGBNAN	0x7fffffff

#define MATH_ERRNO		1
#define MATH_ERREXCEPT		2
#define math_errhandling	(MATH_ERRNO | MATH_ERREXCEPT)

/* what _Fpcomp returns */
#define _FP_LT	1
#define _FP_EQ	2
#define _FP_GT	4

/* the classification macros' functions */
int _Dclass(double x);
int _FDclass(float x);
int _LDclass(long double x);
int _Dsign(double x);
int _FDsign(float x);
int _LDsign(long double x);
int _Fpcomp(double x, double y);
int _FFpcomp(float x, float y);
int _LFpcomp(long double x, long double y);

double acos(double x);
double acosh(double x);
double asin(double x);
double asinh(double x);
double atan(double x);
double atan2(double y, double x);
double atanh(double x);
double cbrt(double x);
double ceil(double x);
double copysign(double x, double y);
double cos(double x);
double cosh(double x);
double erf(double x);
double erfc(double x);
double exp(double x);
double exp2(double x);
double expm1(double x);
double fabs(double x);
double fdim(double x, double y);
double floor(double x);
double fma(double x, double y, double z);
double fmax(double x, double y);
double fmin(double x, double y);
double fmod(double x, double y);
double frexp(double x, int *pexp);
double hypot(double x, double y);
int ilogb(double x);
double ldexp(double x, int n);
double lgamma(double x);
long long llrint(double x);
long long llround(double x);
double log(double x);
double log10(double x);
double log1p(double x);
double log2(double x);
double logb(double x);
long lrint(double x);
long lround(double x);
double modf(double x, double *pint);
double nan(const char *tag);
double nearbyint(double x);
double nextafter(double x, double y);
double nexttoward(double x, long double y);
double pow(double x, double y);
double remainder(double x, double y);
double remquo(double x, double y, int *pquo);
double rint(double x);
double round(double x);
double scalbln(double x, long n);
double scalbn(double x, int n);
double sin(double x);
double sinh(double x);
double sqrt(double x);
double tan(double x);
double tanh(double x);
double tgamma(double x);
double trunc(double x);

float acosf(float x);
float acoshf(float x);
float asinf(float x);
float asinhf(float x);
float atanf(float x);
float atan2f(float y, float x);
float atanhf(float x);
float cbrtf(float x);
float ceilf(float x);
float copysignf(float x, float y);
float cosf(float x);
float coshf(float x);
float erff(float x);
float erfcf(float x);
float expf(float x);
float exp2f(float x);
float expm1f(float x);
float fabsf(float x);
float fdimf(float x, float y);
float floorf(float x);
float fmaf(float x, float y, float z);
float fmaxf(float x, float y);
float fminf(float x, float y);
float fmodf(float x, float y);
float frexpf(float x, int *pexp);
float hypotf(float x, float y);
int ilogbf(float x);
float ldexpf(float x, int n);
float lgammaf(float x);
long long llrintf(float x);
long long llroundf(float x);
float logf(float x);
float log10f(float x);
float log1pf(float x);
float log2f(float x);
float logbf(float x);
long lrintf(float x);
long lroundf(float x);
float modff(float x, float *pint);
float nanf(const char *tag);
float nearbyintf(float x);
float nextafterf(float x, float y);
float nexttowardf(float x, long double y);
float powf(float x, float y);
float remainderf(float x, float y);
float remquof(float x, float y, int *pquo);
float rintf(float x);
float roundf(float x);
float scalblnf(float x, long n);
float scalbnf(float x, int n);
float sinf(float x);
float sinhf(float x);
float sqrtf(float x);
float tanf(float x);
float tanhf(float x);
float tgammaf(float x);
float truncf(float x);

long double acosl(long double x);
long double acoshl(long double x);
long double asinl(long double x);
long double asinhl(long double x);
long double atanl(long double x);
long double atan2l(long double y, long double x);
long double atanhl(long double x);
long double cbrtl(long double x);
long double ceill(long double x);
long double copysignl(long double x, long double y);
long double cosl(long double x);
long double coshl(long double x);
long double erfl(long double x);
long double erfcl(long double x);
long double expl(long double x);
long double exp2l(long double x);
long double expm1l(long double x);
long double fabsl(long double x);
long double fdiml(long double x, long double y);
long double floorl(long double x);
long double fmal(long double x, long double y, long double z);
long double fmaxl(long double x, long double y);
long double fminl(long double x, long double y);
long double fmodl(long double x, long double y);
long double frexpl(long double x, int *pexp);
long double hypotl(long double x, long double y);
int ilogbl(long double x);
long double ldexpl(long double x, int n);
long double lgammal(long double x);
long long llrintl(long double x);
long long llroundl(long double x);
long double logl(long double x);
long double log10l(long double x);
long double log1pl(long double x);
long double log2l(long double x);
long double logbl(long double x);
long lrintl(long double x);
long lroundl(long double x);
long double modfl(long double x, long double *pint);
long double nanl(const char *tag);
long double nearbyintl(long double x);
long double nextafterl(long double x, long double y);
long double nexttowardl(long double x, long double y);
long double powl(long double x, long double y);
long double remainderl(long double x, long double y);
long double remquol(long double x, long double y, int *pquo);
long double rintl(long double x);
long double roundl(long double x);
long double scalblnl(long double x, long n);
long double scalbnl(long double x, int n);
long double sinl(long double x);
long double sinhl(long double x);
long double sqrtl(long double x);
long double tanl(long double x);
long double tanhl(long double x);
long double tgammal(long double x);
long double truncl(long double x);

#endif /* _MATH */
