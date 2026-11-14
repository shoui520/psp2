/*
 * float.h: the C library's floating-point properties (Dinkumware's, as the SDK Release
 * 03.570.011 ships them in target/include/float.h, the __psp2__ branch). long double is
 * double on psp2.
 */
#ifndef _FLOAT
#define _FLOAT

int _Fltrounds(void);

		/* COMMON PROPERTIES */
#define FLT_RADIX		2
#define FLT_ROUNDS		(_Fltrounds())

#define FLT_EVAL_METHOD	-1	/* indeterminable */
#define DECIMAL_DIG	17	/* 64-bit long double */

		/* float PROPERTIES */
#define FLT_EPSILON		0x8p-26f
#define FLT_MAX			0xf.fffffp+124f
#define FLT_MIN			0x8p-129f

#define FLT_DIG			6
#define FLT_MANT_DIG	24
#define FLT_MAX_10_EXP	38
#define FLT_MAX_EXP		128
#define FLT_MIN_10_EXP	-37
#define FLT_MIN_EXP		-125

		/* double PROPERTIES */
#define DBL_EPSILON		0x8p-55
#define DBL_MAX			0xf.ffffffffffff8p+1020
#define DBL_MIN			0x8p-1025

#define DBL_DIG			15
#define DBL_MANT_DIG	53
#define DBL_MAX_10_EXP	308
#define DBL_MAX_EXP		1024
#define DBL_MIN_10_EXP	-307
#define DBL_MIN_EXP		-1021

		/* long double PROPERTIES */
#define LDBL_EPSILON	0x8p-55L
#define LDBL_MAX		0xf.ffffffffffff8p+1020L
#define LDBL_MIN		0x8p-1025L

#define LDBL_DIG		15
#define LDBL_MANT_DIG	53
#define LDBL_MAX_10_EXP	308
#define LDBL_MAX_EXP	1024
#define LDBL_MIN_10_EXP	-307
#define LDBL_MIN_EXP	-1021

#endif /* _FLOAT */
