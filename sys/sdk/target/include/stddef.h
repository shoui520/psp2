/*
 * stddef.h: the SDK's common definitions, with Sony's psp2 types (SDK Release 03.570.011,
 * target/include/stddef.h and yvals.h). Sony's GCC builds read the SDK's C headers, not the
 * compiler's (the SDK libraries' DWARF). GCC takes its built-in offsetof, which gives what the
 * SDK's (size_t)&((T *)0)->member gives and stays a constant expression in C++.
 */
#ifndef _STDDEF
#define _STDDEF

#ifndef NULL
#define NULL		0
#endif

#ifndef offsetof
#ifdef __SNC__
#define offsetof(T, member)	((size_t)__INTADDR__(&(((T *)0)->member)))
#else
#define offsetof(T, member)	__builtin_offsetof(T, member)
#endif
#endif

#if !defined(_PTRDIFF_T) && !defined(_PTRDIFFT) && !defined(_PTRDIFF_T_DEFINED)
#define _PTRDIFF_T
#define _PTRDIFFT
#define _PTRDIFF_T_DEFINED
typedef int ptrdiff_t;
#endif

#if !defined(_SIZE_T) && !defined(_SIZET) && !defined(_SIZE_T_DEFINED) && \
    !defined(_BSD_SIZE_T_DEFINED_)
#define _SIZE_T
#define _SIZET
#define _BSD_SIZE_T_DEFINED_
#define _SIZE_T_DEFINED
typedef unsigned int size_t;
#endif

/* wchar_t is 16 bits on psp2; C++ has it built in */
#if !defined(__cplusplus) && !defined(_WCHART) && !defined(_WCHAR_T_DEFINED)
#define _WCHART
#define _WCHAR_T_DEFINED
typedef unsigned short wchar_t;
#endif

#endif /* _STDDEF */

/* newlib's headers ask for wint_t (psp2's is unsigned int), as Cygwin's do of Sony's stddef.h */
#if defined(__need_wint_t) && !defined(_WINTT)
#define _WINTT
typedef unsigned int wint_t;
#endif
#undef __need_wint_t
