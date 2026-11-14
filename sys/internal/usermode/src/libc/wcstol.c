/* wcstol function */
#include <errno.h>
#include <limits.h>
#include <wchar.h>
#include <wctype.h>
#include <xwchar.h>

long wcstol(const wchar_t *__restrict s, wchar_t **__restrict endptr, int base)
{
	const wchar_t *sc;
	wchar_t *se, sign;
	unsigned long x;

	if (endptr == NULL)
		endptr = &se;
	for (sc = s; iswspace(*sc); ++sc)
		;
	sign = *sc == L'-' || *sc == L'+' ? *sc++ : L'+';
	x = _WStoul(sc, endptr, base);
	if (sc == *endptr) {	/* no digits */
		*endptr = (wchar_t *)s;
		return 0;
	}
	if ((sign == L'+' && LONG_MAX < x) || (sign == L'-' && 0 - (unsigned long)LONG_MIN < x)) {
		errno = ERANGE;
		return sign == L'-' ? LONG_MIN : LONG_MAX;
	}
	return (long)(sign == L'-' ? 0 - x : x);
}
