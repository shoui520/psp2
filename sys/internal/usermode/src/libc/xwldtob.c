/*
 * _WLdtob function: convert a long double (a double on psp2) for wprintf's
 * a, A, e, E, f, F, g and G conversions. The significant digits are
 * generated into a buffer, hexadecimal ones NXDIG at a time by scaling by
 * 2^28, decimal ones NDIG at a time after scaling by powers of ten in long
 * double (where _Ldtob computes in the extended format), then rounded and
 * handed to _WGenld with the exponent.
 */
#include <stdlib.h>
#include <wchar.h>

#include "xmath.h"
#include "xwstdio.h"

#define NDIG	8	/* decimal digits generated for each multiply */
#define NXDIG	7	/* hexadecimal digits generated for each multiply */
#define MAXSIG	40	/* the significant digits kept */

static const long double pows[] = {	/* 10^(2^n) */
	1e1L, 1e2L, 1e4L, 1e8L, 1e16L, 1e32L, 1e64L, 1e128L, 1e256L,
};

void _WLdtob(_WPft *px, wchar_t code)
{
	wchar_t ac[MAXSIG];
	wchar_t *p = ac;
	long double ldval = px->v.ld;
	short errx;
	short nsig;
	short xexp;

	if (code == L'a' || code == L'A')
		;
	else if (px->prec < 0)
		px->prec = 6;
	else if (px->prec == 0 && (code == L'g' || code == L'G'))
		px->prec = 1;
	if ((errx = _LDunscale(&xexp, &px->v.ld)) == _NANCODE) {	/* x == NaN */
		wmemcpy(px->s, code == L'a' || code == L'e' || code == L'f' || code == L'g' ? L"nan"
			: L"NAN", px->n1 = 3);
		return;
	} else if (0 < errx) {	/* x == INF */
		wmemcpy(px->s, code == L'a' || code == L'e' || code == L'f' || code == L'g' ? L"inf"
			: L"INF", px->n1 = 3);
		return;
	}

	if (code == L'a' || code == L'A') {	/* put "0x" */
		*px->s++ = L'0';
		*px->s++ = code == L'a' ? L'x' : L'X';
		px->n0 += 2;
	}
	if (errx == 0) {	/* x == 0 */
		nsig = 0;
		xexp = 0;
	} else if (code == L'a' || code == L'A') {	/* hexadecimal digits, binary exponent */
		static const wchar_t ldigs[16] = L"0123456789abcdef";
		static const wchar_t udigs[16] = L"0123456789ABCDEF";
		const wchar_t *digits = code == L'a' ? ldigs : udigs;
		int gen;

		nsig = px->prec < 0 ? MAXSIG - 7 : px->prec + 1;
		gen = nsig + 2;
		ldval = ldval < 0 ? -px->v.ld : px->v.ld;
		_LDscale(&ldval, -3);
		--xexp;
		ac[0] = 0;
		for (p = &ac[1]; 0 < gen && 0 < ldval; p += NXDIG) {	/* NXDIG at a time */
			int j;
			long lo;

			_LDscale(&ldval, 4 * NXDIG);
			lo = (long)ldval;
			if (0 < (gen -= NXDIG))
				ldval -= (long double)lo;
			for (p += NXDIG, j = NXDIG; 0 < lo && 0 <= --j;) {
				*--p = (wchar_t)(lo & 0xf);
				lo >>= 4;
			}
			while (0 <= --j)
				*--p = 0;
		}
		gen = p - &ac[1];
		p = &ac[1];
		if (gen < nsig)
			nsig = gen;
		if (0 <= nsig) {	/* round and strip trailing zeros */
			const char drop = nsig < gen && 0x8 <= p[nsig] ? 0xf : 0x0;
			int n;

			for (n = nsig; p[--n] == drop;)
				--nsig;
			if (drop == 0xf)
				++p[n];
			if (n < 0) {	/* the carry went into ac[0] */
				xexp += 4;
				--p;
				++nsig;
			}
			for (n = nsig; 0 <= --n;)
				p[n] = digits[p[n]];
		}
		if (px->prec < 0)
			px->prec = nsig - 1;
	} else {	/* decimal digits */
		int gen;
		int i;
		int n;

		if (ldval < 0)
			ldval = -ldval;
		xexp = xexp * 30103L / 100000L - NDIG / 2;	/* the power of ten, low */
		if (xexp < 0) {	/* scale up */
			n = (NDIG / 2 - 1 - xexp) & ~(NDIG / 2 - 1);
			xexp = -n;
			for (i = 0; 0 < n; n >>= 1, ++i) {
				if (n & 1)
					ldval *= pows[i];
			}
		} else if (0 < xexp) {	/* scale down */
			long double factor = 1;

			xexp &= ~(NDIG / 2 - 1);
			for (n = xexp, i = 0; 0 < n; n >>= 1, ++i) {
				if (n & 1)
					factor *= pows[i];
			}
			ldval /= factor;
		}
		gen = (code == L'f' || code == L'F' ? xexp + 11 : 7) + px->prec;
		if (19 < gen)
			gen = 19;
		ac[0] = L'0';
		for (p = &ac[1]; 0 < gen && 0 < ldval; p += NDIG) {	/* NDIG at a time */
			int j;
			long lo = (long)ldval;

			if (0 < (gen -= NDIG))
				ldval = (ldval - (long double)lo) * 1e8L;
			for (p += NDIG, j = NDIG; 0 < lo && 0 <= --j;) {
				ldiv_t qr = ldiv(lo, 10);

				*--p = (wchar_t)(qr.rem + L'0');
				lo = qr.quot;
			}
			while (0 <= --j)
				*--p = L'0';
		}
		gen = p - &ac[1];
		p = &ac[1];
		xexp += NDIG - 1;
		for (; *p == L'0'; ++p)	/* the leading zeros */
			--gen, --xexp;
		nsig = px->prec + (code == L'f' || code == L'F' ? xexp + 1
				   : code == L'e' || code == L'E' ? 1 : 0);
		if (gen < nsig)
			nsig = gen;
		if (0 <= nsig) {	/* round and strip trailing zeros */
			const char drop = nsig < gen && L'5' <= p[nsig] ? L'9' : L'0';
			int n;

			for (n = nsig; p[--n] == drop;)
				--nsig;
			if (drop == L'9')
				++p[n];
			if (n < 0) {	/* the carry went into ac[0] */
				++xexp;
				--p;
				++nsig;
			}
		}
	}
	_WGenld(px, code, p, nsig, xexp);
}
