/*
 * uchar.h: the SDK's Unicode character conversions (SDK Release 03.570.011,
 * target/include/uchar.h).
 */
#ifndef _UCHAR
#define _UCHAR

#include <yvals.h>

#ifndef NULL
#define NULL		0
#endif

#if !defined(__cplusplus) || __cplusplus < 201103L
typedef unsigned short char16_t;
typedef unsigned int char32_t;
#endif

typedef _Mbstatet mbstate_t;

#if !defined(_SIZE_T) && !defined(_SIZET) && !defined(_SIZE_T_DEFINED) && \
    !defined(_BSD_SIZE_T_DEFINED_)
#define _SIZE_T
#define _SIZET
#define _BSD_SIZE_T_DEFINED_
#define _SIZE_T_DEFINED
typedef unsigned int size_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

size_t mbrtoc16(char16_t *__restrict pc16, const char *__restrict s, size_t n,
		mbstate_t *__restrict ps);
size_t c16rtomb(char *__restrict s, char16_t c16, mbstate_t *__restrict ps);
size_t mbrtoc32(char32_t *__restrict pc32, const char *__restrict s, size_t n,
		mbstate_t *__restrict ps);
size_t c32rtomb(char *__restrict s, char32_t c32, mbstate_t *__restrict ps);

#ifdef __cplusplus
}
#endif

#endif /* _UCHAR */
