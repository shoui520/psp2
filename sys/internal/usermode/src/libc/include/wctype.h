/*
 * wctype.h: the SDK's wide character classes, with Sony's psp2 definitions (SDK Release
 * 03.570.011, target/include/wctype.h and xwcc.h): a class or mapping is its index into the
 * C library's tables, which _Iswctype and _Towctrans look up.
 */
#ifndef _WCTYPE
#define _WCTYPE

#include <wchar.h>

typedef _Sizet wctrans_t;

int _Iswctype(wint_t c, wctype_t category);
wint_t _Towctrans(wint_t c, wctrans_t category);

int iswalnum(wint_t c);
int iswalpha(wint_t c);
int iswblank(wint_t c);
int iswcntrl(wint_t c);
int iswctype(wint_t c, wctype_t category);
int iswdigit(wint_t c);
int iswgraph(wint_t c);
int iswlower(wint_t c);
int iswprint(wint_t c);
int iswpunct(wint_t c);
int iswspace(wint_t c);
int iswupper(wint_t c);
int iswxdigit(wint_t c);
wint_t towctrans(wint_t c, wctrans_t category);
wint_t towlower(wint_t c);
wint_t towupper(wint_t c);
wctrans_t wctrans(const char *property);
wctype_t wctype(const char *property);

#define iswalnum(wc)		_Iswctype(wc, 1)
#define iswalpha(wc)		_Iswctype(wc, 2)
#define iswcntrl(wc)		_Iswctype(wc, 3)
#define iswctype(wc, off)	_Iswctype(wc, off)
#define iswdigit(wc)		_Iswctype(wc, 4)
#define iswgraph(wc)		_Iswctype(wc, 5)
#define iswlower(wc)		_Iswctype(wc, 6)
#define iswprint(wc)		_Iswctype(wc, 7)
#define iswpunct(wc)		_Iswctype(wc, 8)
#define iswspace(wc)		_Iswctype(wc, 9)
#define iswupper(wc)		_Iswctype(wc, 10)
#define iswxdigit(wc)		_Iswctype(wc, 11)
#define iswblank(wc)		_Iswctype(wc, 12)
#define towctrans(wc, off)	_Towctrans(wc, off)
#define towlower(wc)		_Towctrans(wc, 1)
#define towupper(wc)		_Towctrans(wc, 2)

#endif /* _WCTYPE */
