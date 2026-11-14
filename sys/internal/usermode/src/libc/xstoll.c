/*
 * _Stollx function: a string to long long, through _Stoullx.
 */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

long long _Stollx(const char *s, char **endptr, int base, int *perr)
{
	const char *sc;
	char *se, sign;
	unsigned long long x;

	if (endptr == NULL)
		endptr = &se;
	for (sc = s; isspace((unsigned char)*sc); ++sc)
		;
	sign = *sc == '-' || *sc == '+' ? *sc++ : '+';
	x = _Stoullx(sc, endptr, base, perr);
	if (sc == *endptr) {		/* no digits */
		*endptr = (char *)s;
		return 0;
	}
	if ((sign == '+' && LLONG_MAX < x) || (sign == '-' && 0 - (unsigned long long)LLONG_MIN < x)) {
		errno = ERANGE;
		if (perr != NULL)
			*perr = 1;
		return sign == '-' ? LLONG_MIN : LLONG_MAX;
	}
	return (long long)(sign == '-' ? 0 - x : x);
}

long long _Stoll(const char *s, char **endptr, int base)
{
	return _Stollx(s, endptr, base, NULL);
}
