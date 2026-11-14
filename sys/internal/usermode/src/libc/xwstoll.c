/*
 * _WStoll function: a wide string to long long, through _WStoull.
 */
#include <errno.h>
#include <limits.h>
#include <wchar.h>
#include <wctype.h>

#include "xwchar.h"

long long _WStoll(const wchar_t *s, wchar_t **endptr, int base)
{
	const wchar_t *sc;
	wchar_t *se, sign;
	unsigned long long x;

	if (endptr == NULL)
		endptr = &se;
	for (sc = s; iswspace(*sc); ++sc)
		;
	sign = *sc == '-' || *sc == '+' ? *sc++ : '+';
	x = _WStoull(sc, endptr, base);
	if (sc == *endptr) {		/* no digits */
		*endptr = (wchar_t *)s;
		return 0;
	}
	if ((sign == '+' && LLONG_MAX < x) || (sign == '-' && 0 - (unsigned long long)LLONG_MIN < x)) {
		errno = ERANGE;
		return sign == '-' ? LLONG_MIN : LLONG_MAX;
	}
	return (long long)(sign == '-' ? 0 - x : x);
}
