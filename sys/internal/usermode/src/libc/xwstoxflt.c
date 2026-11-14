/*
 * _WStoxflt function: hexadecimal digits to an array of 7-digit words and a power of two.
 */
#include <wchar.h>
#include <wctype.h>
#include <stddef.h>
#include <string.h>

#include "xmath.h"

#define BASE	16		/* hexadecimal */
#define NDIG	7		/* hexadecimal digits per word */
#define MAXSIG	(5 * NDIG)	/* the most significant digits kept */

static const wchar_t digits[] = L"0123456789abcdefABCDEF";
static const wchar_t vals[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	10, 11, 12, 13, 14, 15,
};

int _WStoxflt(const wchar_t *s0, const wchar_t *s, wchar_t **endptr, long lo[], int maxsig)
{
	char buf[MAXSIG + 1];	/* the digits, with room for a rounding digit */
	int nsig = 0;		/* significant digits seen */
	int seen = 0;		/* any characters of the field seen */
	int word = 0;		/* the word being filled */
	const wchar_t *pd;

	maxsig *= NDIG;		/* words to digits */
	if (MAXSIG < maxsig)
		maxsig = MAXSIG;
	lo[0] = 0;		/* the power of two, in hexadecimal digits until the shift */
	lo[1] = 0;		/* the first word of the fraction */

	for (; *s == '0'; ++s)	/* strip leading zeros */
		seen = 1;
	for (; (pd = wmemchr(digits, *s, sizeof(digits) / sizeof(wchar_t) - 1)) != NULL; ++s, seen = 1) {
		if (nsig <= maxsig)
			buf[nsig++] = vals[pd - digits];
		else		/* too many digits: scale */
			++lo[0];
	}
	if (*s == '.')
		++s;
	if (nsig == 0) {	/* strip zeros after the point */
		for (; *s == '0'; ++s, seen = 1)
			--lo[0];
	}
	for (; (pd = wmemchr(digits, *s, sizeof(digits) / sizeof(wchar_t) - 1)) != NULL; ++s, seen = 1) {
		if (nsig <= maxsig) {	/* a fraction digit */
			buf[nsig++] = vals[pd - digits];
			--lo[0];
		}
	}

	if (maxsig < nsig) {	/* round up, then drop the extra digit */
		if (BASE / 2 <= buf[maxsig])
			++buf[maxsig - 1];
		nsig = maxsig;
		++lo[0];
	}
	for (; 0 < nsig && buf[nsig - 1] == '\0'; --nsig)	/* drop trailing zeros */
		++lo[0];
	if (nsig == 0)		/* at least one digit */
		buf[nsig++] = '\0';
	lo[0] <<= 2;		/* hexadecimal digits to bits */

	if (seen) {		/* the digits to words */
		int bufidx = 0;
		int wordidx = NDIG - nsig % NDIG;

		word = wordidx % NDIG == 0 ? 0 : 1;
		for (; bufidx < nsig; ++wordidx, ++bufidx) {
			if (wordidx % NDIG == 0)
				lo[++word] = buf[bufidx];
			else
				lo[word] = lo[word] * BASE + buf[bufidx];
		}

		if (*s == 'p' || *s == 'P') {	/* the exponent */
			const wchar_t *ssav = s;
			const wchar_t esign = *++s == '+' || *s == '-' ? *s++ : '+';
			int eseen = 0;
			long lexp = 0;

			for (; iswdigit(*s); ++s, eseen = 1) {
				if (lexp < 100000000)	/* else it overflows */
					lexp = lexp * 10 + *s - '0';
			}
			if (esign == '-')
				lexp = -lexp;
			lo[0] += lexp;
			if (!eseen)	/* roll back an incomplete exponent */
				s = ssav;
		}
	}

	if (!seen)		/* zero for a bad field */
		word = 0;
	if (endptr != NULL)
		*endptr = (wchar_t *)(seen ? s : s0);
	return word;
}
