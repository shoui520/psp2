/*
 * fenv.h: the floating-point environment (Dinkumware's), on the VFP's FPSCR. The SDK does
 * not ship this header; the library uses it internally.
 */
#ifndef _FENV
#define _FENV

typedef unsigned long fenv_t;
typedef unsigned long fexcept_t;

#define FE_INVALID	0x01
#define FE_DIVBYZERO	0x02
#define FE_OVERFLOW	0x04
#define FE_UNDERFLOW	0x08
#define FE_INEXACT	0x10
#define FE_ALL_EXCEPT	0x1f

/* the rounding modes, as the FPSCR's RMode field */
#define FE_TONEAREST	0x00
#define FE_UPWARD	0x01
#define FE_DOWNWARD	0x02
#define FE_TOWARDZERO	0x03

int fegetenv(fenv_t *penv);
int fesetenv(const fenv_t *penv);
int feraiseexcept(int except);
int fegetround(void);

#endif /* _FENV */
