/*
 * wchar.h: the SDK's wide character functions, with Sony's psp2 declarations (SDK Release
 * 03.570.011, target/include/wchar.h): wchar_t is 16 bits, wint_t 32 bits.
 */
#ifndef _WCHAR
#define _WCHAR

#include <stdarg.h>
#include <yvals.h>

#ifndef NULL
#define NULL		0
#endif

#define WCHAR_MIN	_WCMIN
#define WCHAR_MAX	_WCMAX
#define WEOF		((wint_t)(-1))

typedef _Mbstatet mbstate_t;

#if !defined(_SIZE_T) && !defined(_SIZET) && !defined(_SIZE_T_DEFINED) && \
    !defined(_BSD_SIZE_T_DEFINED_)
#define _SIZE_T
#define _SIZET
#define _BSD_SIZE_T_DEFINED_
#define _SIZE_T_DEFINED
typedef unsigned int size_t;
#endif

struct tm;
struct _Dnk_filet;
#ifndef _FILET
#define _FILET
typedef struct _Dnk_filet _Filet;
#endif

#if !defined(__cplusplus) && !defined(_WCHART) && !defined(_WCHAR_T_DEFINED)
#define _WCHART
#define _WCHAR_T_DEFINED
typedef unsigned short wchar_t;
#endif

#ifndef _WCTYPE_T
#define _WCTYPE_T
typedef _Sizet wctype_t;
#endif

#ifndef _WINT_T
#define _WINT_T
typedef _Wintt wint_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* stdio */
wint_t fgetwc(_Filet *stream);
wchar_t *fgetws(wchar_t *__restrict s, int n, _Filet *__restrict stream);
wint_t fputwc(wchar_t c, _Filet *stream);
int fputws(const wchar_t *__restrict s, _Filet *__restrict stream);
int fwide(_Filet *stream, int mode);
int fwprintf(_Filet *__restrict stream, const wchar_t *__restrict format, ...);
int fwscanf(_Filet *__restrict stream, const wchar_t *__restrict format, ...);
wint_t getwc(_Filet *stream);
wint_t getwchar(void);
wint_t putwc(wchar_t c, _Filet *stream);
wint_t putwchar(wchar_t c);
int swprintf(wchar_t *__restrict s, size_t n, const wchar_t *__restrict format, ...);
int swscanf(const wchar_t *__restrict s, const wchar_t *__restrict format, ...);
wint_t ungetwc(wint_t c, _Filet *stream);
int vfwprintf(_Filet *__restrict stream, const wchar_t *__restrict format, va_list arg);
int vswprintf(wchar_t *__restrict s, size_t n, const wchar_t *__restrict format, va_list arg);
int vwprintf(const wchar_t *__restrict format, va_list arg);
int wprintf(const wchar_t *__restrict format, ...);
int wscanf(const wchar_t *__restrict format, ...);
int vfwscanf(_Filet *__restrict stream, const wchar_t *__restrict format, va_list ap);
int vswscanf(const wchar_t *__restrict s, const wchar_t *__restrict format, va_list ap);
int vwscanf(const wchar_t *__restrict format, va_list ap);

/* stdlib */
wint_t btowc(int c);
size_t mbrlen(const char *__restrict s, size_t n, mbstate_t *__restrict ps);
size_t mbrtowc(wchar_t *__restrict pwc, const char *s, size_t n, mbstate_t *__restrict ps);
size_t mbsrtowcs(wchar_t *__restrict dst, const char **__restrict src, size_t len,
		 mbstate_t *__restrict ps);
int mbsinit(const mbstate_t *ps);
size_t wcrtomb(char *__restrict s, wchar_t wc, mbstate_t *__restrict ps);
size_t wcsrtombs(char *__restrict dst, const wchar_t **__restrict src, size_t len,
		 mbstate_t *__restrict ps);
long wcstol(const wchar_t *__restrict s, wchar_t **__restrict endptr, int base);
long long wcstoll(const wchar_t *__restrict s, wchar_t **__restrict endptr, int base);
unsigned long long wcstoull(const wchar_t *__restrict s, wchar_t **__restrict endptr,
			    int base);
double wcstod(const wchar_t *__restrict s, wchar_t **__restrict endptr);
float wcstof(const wchar_t *__restrict s, wchar_t **__restrict endptr);
long double wcstold(const wchar_t *__restrict s, wchar_t **__restrict endptr);
unsigned long wcstoul(const wchar_t *__restrict s, wchar_t **__restrict endptr, int base);
int wctob(wint_t c);

/* string */
wchar_t *wcscat(wchar_t *__restrict s1, const wchar_t *__restrict s2);
int wcscmp(const wchar_t *s1, const wchar_t *s2);
wchar_t *wcscpy(wchar_t *__restrict s1, const wchar_t *__restrict s2);
size_t wcslen(const wchar_t *s);
int wcsncmp(const wchar_t *s1, const wchar_t *s2, size_t n);
wchar_t *wcsncpy(wchar_t *__restrict s1, const wchar_t *__restrict s2, size_t n);
int wcscoll(const wchar_t *s1, const wchar_t *s2);
size_t wcscspn(const wchar_t *s1, const wchar_t *s2);
wchar_t *wcsncat(wchar_t *__restrict s1, const wchar_t *__restrict s2, size_t n);
size_t wcsspn(const wchar_t *s1, const wchar_t *s2);
wchar_t *wcstok(wchar_t *__restrict s1, const wchar_t *__restrict s2,
		wchar_t **__restrict ptr);
size_t wcsxfrm(wchar_t *__restrict s1, const wchar_t *__restrict s2, size_t n);
int wmemcmp(const wchar_t *s1, const wchar_t *s2, size_t n);
wchar_t *wmemcpy(wchar_t *__restrict s1, const wchar_t *__restrict s2, size_t n);
wchar_t *wmemmove(wchar_t *s1, const wchar_t *s2, size_t n);
wchar_t *wmemset(wchar_t *s, wchar_t c, size_t n);
wchar_t *wcschr(const wchar_t *s, wchar_t c);
wchar_t *wcspbrk(const wchar_t *s1, const wchar_t *s2);
wchar_t *wcsrchr(const wchar_t *s, wchar_t c);
wchar_t *wcsstr(const wchar_t *s1, const wchar_t *s2);
wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n);

/* time */
size_t wcsftime(wchar_t *__restrict s, size_t n, const wchar_t *__restrict format,
		const struct tm *__restrict tptr);

/* the library's converters */
wint_t _Btowc(int c);
int _Wctob(wint_t c);
double _WStod(const wchar_t *s, wchar_t **endptr, long base);
float _WStof(const wchar_t *s, wchar_t **endptr, long base);
long double _WStold(const wchar_t *s, wchar_t **endptr, long base);
unsigned long _WStoul(const wchar_t *s, wchar_t **endptr, int base);

/* the bounds-checking interfaces (C11 Annex K) */
#if __STDC_WANT_LIB_EXT1__
#if !defined(_ERRNO_T_DEFINED)
#define _ERRNO_T_DEFINED
typedef int errno_t;
#endif
#if !defined(_RSIZE_T_DEFINED)
#define _RSIZE_T_DEFINED
typedef size_t rsize_t;
#endif

int fwprintf_s(_Filet *__restrict stream, const wchar_t *__restrict format, ...);
int fwscanf_s(_Filet *__restrict stream, const wchar_t *__restrict format, ...);
int snwprintf_s(wchar_t *__restrict s, rsize_t n, const wchar_t *__restrict format, ...);
int swprintf_s(wchar_t *__restrict s, rsize_t n, const wchar_t *__restrict format, ...);
int swscanf_s(const wchar_t *__restrict s, const wchar_t *__restrict format, ...);
int vfwprintf_s(_Filet *__restrict stream, const wchar_t *__restrict format, va_list arg);
int vfwscanf_s(_Filet *__restrict stream, const wchar_t *__restrict format, va_list arg);
int vsnwprintf_s(wchar_t *__restrict s, rsize_t n, const wchar_t *__restrict format,
		 va_list arg);
int vswprintf_s(wchar_t *__restrict s, rsize_t n, const wchar_t *__restrict format,
		va_list arg);
int vswscanf_s(const wchar_t *__restrict s, const wchar_t *__restrict format, va_list arg);
int vwprintf_s(const wchar_t *__restrict format, va_list arg);
int vwscanf_s(const wchar_t *__restrict format, va_list arg);
int wprintf_s(const wchar_t *__restrict format, ...);
int wscanf_s(const wchar_t *__restrict format, ...);
errno_t wcscpy_s(wchar_t *__restrict s1, rsize_t s1max, const wchar_t *__restrict s2);
errno_t wcsncpy_s(wchar_t *__restrict s1, rsize_t s1max, const wchar_t *__restrict s2,
		  rsize_t n);
errno_t wmemcpy_s(wchar_t *__restrict s1, rsize_t s1max, const wchar_t *__restrict s2,
		  rsize_t n);
errno_t wmemmove_s(wchar_t *s1, rsize_t s1max, const wchar_t *s2, rsize_t n);
errno_t wcscat_s(wchar_t *__restrict s1, rsize_t s1max, const wchar_t *__restrict s2);
errno_t wcsncat_s(wchar_t *__restrict s1, rsize_t s1max, const wchar_t *__restrict s2,
		  rsize_t n);
wchar_t *wcstok_s(wchar_t *__restrict s1, rsize_t *__restrict s1max,
		  const wchar_t *__restrict s2, wchar_t **__restrict ptr);
size_t wcsnlen_s(const wchar_t *s, size_t maxsize);
errno_t wcrtomb_s(size_t *__restrict retval, char *__restrict s, rsize_t smax, wchar_t wc,
		  mbstate_t *__restrict ps);
errno_t mbsrtowcs_s(size_t *__restrict retval, wchar_t *__restrict dst, rsize_t dstmax,
		    const char **__restrict src, rsize_t len, mbstate_t *__restrict ps);
errno_t wcsrtombs_s(size_t *__restrict retval, char *__restrict dst, rsize_t dstmax,
		    const wchar_t **__restrict src, rsize_t len, mbstate_t *__restrict ps);
#endif /* __STDC_WANT_LIB_EXT1__ */

#ifdef __cplusplus
}
#endif

#endif /* _WCHAR */
