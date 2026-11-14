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

int _Stopfx(const char **ps, char **endptr);
int _Stoflt(const char *s0, const char *s, char **endptr, long lo[], int maxsig);
int _Stoxflt(const char *s0, const char *s, char **endptr, long lo[], int maxsig);

#endif /* _XMATH */
