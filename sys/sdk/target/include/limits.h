/*
 * limits.h: the SDK's integer limits, with Sony's psp2 values (SDK Release 03.570.011,
 * target/include/limits.h and yvals.h): signed char, 32-bit int and long, 4-byte multibyte
 * characters (UTF-8).
 */
#ifndef _LIMITS
#define _LIMITS

#define CHAR_BIT	8
#define MB_LEN_MAX	4

#define SCHAR_MAX	0x7f
#define SCHAR_MIN	(-SCHAR_MAX - 1)
#define UCHAR_MAX	0xff
#ifdef __CHAR_UNSIGNED__
#define CHAR_MAX	UCHAR_MAX
#define CHAR_MIN	0
#else
#define CHAR_MAX	SCHAR_MAX
#define CHAR_MIN	SCHAR_MIN
#endif

#define SHRT_MAX	0x7fff
#define SHRT_MIN	(-SHRT_MAX - 1)
#define USHRT_MAX	0xffff

#define INT_MAX		0x7fffffff
#define INT_MIN		(-INT_MAX - 1)
#define UINT_MAX	0xffffffffU

#define LONG_MAX	0x7fffffffL
#define LONG_MIN	(-LONG_MAX - 1)
#define ULONG_MAX	0xffffffffUL

#define LLONG_MAX	0x7fffffffffffffffLL
#define LLONG_MIN	(-LLONG_MAX - 1)
#define ULLONG_MAX	0xffffffffffffffffULL

#endif /* _LIMITS */
