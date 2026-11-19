/*
 * float.h: the SDK's float properties, with Sony's psp2 values (SDK Release 03.570.011,
 * target/include/float.h, the __psp2__ branch).
 */
#ifndef _FLOAT
#define _FLOAT

#define FLT_RADIX		2

/* float */
#define FLT_EPSILON		0x8p-26f
#define FLT_MAX			0xf.fffffp+124f
#define FLT_MIN			0x8p-129f
#define FLT_DIG			6
#define FLT_MANT_DIG		24
#define FLT_MAX_10_EXP		38
#define FLT_MAX_EXP		128
#define FLT_MIN_10_EXP		-37
#define FLT_MIN_EXP		-125

/* double */
#define DBL_EPSILON		0x8p-55
#define DBL_MAX			0xf.ffffffffffff8p+1020
#define DBL_MIN			0x8p-1025
#define DBL_DIG			15
#define DBL_MANT_DIG		53
#define DBL_MAX_10_EXP		308
#define DBL_MAX_EXP		1024
#define DBL_MIN_10_EXP		-307
#define DBL_MIN_EXP		-1021

/* long double (64-bit) */
#define LDBL_EPSILON		0x8p-55L
#define LDBL_MAX		0xf.ffffffffffff8p+1020L
#define LDBL_MIN		0x8p-1025L
#define LDBL_DIG		15
#define LDBL_MANT_DIG		53
#define LDBL_MAX_10_EXP		308
#define LDBL_MAX_EXP		1024
#define LDBL_MIN_10_EXP		-307
#define LDBL_MIN_EXP		-1021

#endif
