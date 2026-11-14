/*
 * _Stoullx function: a string to unsigned long long, in base 2 to 36 (0: the C prefix decides).
 */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define BASE_MAX	36	/* the largest base */

static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";

/* the most digits a base can have without overflow checking */
static const char ndigs[BASE_MAX + 1] = {
	0, 0, 65, 41, 33, 28, 25, 23, 22, 21,
	20, 19, 18, 18, 17, 17, 17, 16, 16, 16,
	15, 15, 15, 15, 14, 14, 14, 14, 14, 14,
	14, 13, 13, 13, 13, 13, 13,
};

unsigned long long _Stoullx(const char *s, char **endptr, int base, int *perr)
{
	const char *sc, *sd;
	const char *s1, *s2;
	char dig, sign;
	ptrdiff_t n;
	unsigned long long x, y;

	if (perr != NULL)
		*perr = 0;
	for (sc = s; isspace((unsigned char)*sc); ++sc)
		;
	sign = *sc == '-' || *sc == '+' ? *sc++ : '+';
	if (base < 0 || base == 1 || BASE_MAX < base) {	/* a silly base */
		if (endptr != NULL)
			*endptr = (char *)s;
		return 0;
	} else if (0 < base) {		/* strip 0x or 0X */
		if (base == 16 && *sc == '0' && (sc[1] == 'x' || sc[1] == 'X'))
			sc += 2;
	} else if (*sc != '0') {
		base = 10;
	} else if (sc[1] == 'x' || sc[1] == 'X') {
		base = 16;
		sc += 2;
	} else {
		base = 8;
	}
	for (s1 = sc; *sc == '0'; ++sc)		/* skip leading zeros */
		;
	x = 0;
	y = 0;
	dig = 0;
	for (s2 = sc; (sd = memchr(digits, tolower((unsigned char)*sc), base)) != NULL; ++sc) {
		y = x;				/* for overflow checking */
		dig = (char)(sd - digits);
		x = x * base + dig;
	}
	if (s1 == sc) {			/* no digits */
		if (endptr != NULL)
			*endptr = (char *)s;
		return 0;
	}
	n = sc - s2 - ndigs[base];
	if (n < 0 || (n <= 0 && x - dig <= x && (x - dig) / base == y)) {
		if (sign == '-')
			x = 0 - x;
	} else {			/* overflow */
		errno = ERANGE;
		if (perr != NULL)
			*perr = 1;
		x = ULLONG_MAX;
	}
	if (endptr != NULL)
		*endptr = (char *)sc;
	return x;
}

unsigned long long _Stoull(const char *s, char **endptr, int base)
{
	return _Stoullx(s, endptr, base, NULL);
}
