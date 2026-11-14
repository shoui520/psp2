/*
 * xwchar.h: the SDK's internal wide character declarations (SDK Release 03.570.011,
 * target/include/xwchar.h): the C library's multibyte converters.
 */
#ifndef _XWCHAR
#define _XWCHAR

#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

int _Mbtowc(wchar_t *pwc, const char *s, size_t nin, mbstate_t *pst);
size_t _Wcsftime(wchar_t *buf, size_t bufsize, const char *fmt, size_t len,
		 const struct tm *t);
int _Wctomb(char *s, wchar_t wchar, mbstate_t *pst);
long double _WStold(const wchar_t *s, wchar_t **endptr, long base);
long long _WStoll(const wchar_t *s, wchar_t **endptr, int base);
unsigned long _WStoul(const wchar_t *s, wchar_t **endptr, int base);
unsigned long long _WStoull(const wchar_t *s, wchar_t **endptr, int base);

int _Mbtowcx(wchar_t *pwc, const char *s, size_t nin, mbstate_t *pst);
int _Wctombx(char *s, wchar_t wchar, mbstate_t *pst);

#ifdef __cplusplus
}
#endif

#endif /* _XWCHAR */
