/*
 * The "C" locale, the only one: its conventions, and the library's state of it, which
 * an application may keep up to date through a hook (none here).
 */

#include <limits.h>
#include <locale.h>
#include "xstdio.h"

static const struct lconv clconv = {
	"", "", "", "", "", "", "",
	CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX,
	".", "", "", "", "", "false", "true",
	"", "",
};

/* what the image reads of the state: the hook, and the word _Getlocale returns */
struct {
	unsigned long reserved[6];
	unsigned long current;
	const char *name;
	unsigned long reserved2;
	const char *name2;
	unsigned long reserved3;
	void (*update)(void);
	const struct lconv *lconv;
	unsigned long reserved4[19];
} _Locale = {
	.name = "",
	.name2 = "",
	.lconv = &clconv,
};

__attribute__((noinline)) unsigned long _Getlocale(void)
{
	if (_Locale.update != 0)
		_Locale.update();
	return _Locale.current;
}

/* the "C" locale's conversions: a byte is its character */
static __attribute__((noinline)) int mbtowcC(_Wchart *pwc, const char *s, size_t n, _Mbstatet *pst)
{
	static const _Mbstatet initial = { 0 };

	if (s == 0) {
		*pst = initial;
		return 0;
	}
	if (n == 0)
		return -2;
	if (pwc != 0)
		*pwc = (unsigned char)*s;
	return *s != '\0';
}

static __attribute__((noinline)) int wctombC(char *s, _Wchart wc, _Mbstatet *pst)
{
	static const _Mbstatet initial = { 0 };

	if (s == 0) {
		*pst = initial;
		return 0;
	}
	*s = wc;
	return 1;
}

int _Mbtowc(_Wchart *pwc, const char *s, size_t n, _Mbstatet *pst)
{
	_Getlocale();
	return mbtowcC(pwc, s, n, pst);
}

int _Wctomb(char *s, _Wchart wc, _Mbstatet *pst)
{
	_Getlocale();
	return wctombC(s, wc, pst);
}

int _Mbcurmax(void)
{
	_Getlocale();
	return 1;
}
