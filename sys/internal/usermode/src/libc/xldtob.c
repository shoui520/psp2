/*
 * _Ldtob function: convert a long double (a double on psp2) for printf's
 * a, A, e, E, f, F, g and G conversions. The significant digits are
 * generated into a buffer, hexadecimal ones NXDIG at a time by scaling by
 * 2^28, decimal ones NDIG at a time in the extended format, then rounded
 * and handed to _Genld with the exponent.
 */
#include <stdlib.h>
#include <string.h>

#include "xmath.h"
#include "xstdio.h"

#define NDIG	8	/* decimal digits generated for each multiply */
#define NXDIG	7	/* hexadecimal digits generated for each multiply */
#define MAXSIG	40	/* the significant digits kept */

void _Ldtob(_Pft *px, char code)
{
	char ac[MAXSIG];
	char *p = ac;
	long double ldval = px->v.ld;
	short errx;
	short nsig;
	short xexp;

	if (code == 'a' || code == 'A')
		;
	else if (px->prec < 0)
		px->prec = 6;
	else if (px->prec == 0 && (code == 'g' || code == 'G'))
		px->prec = 1;
	if ((errx = _LDunscale(&xexp, &px->v.ld)) == _NANCODE) {	/* x == NaN */
		memcpy(px->s, code == 'a' || code == 'e' || code == 'f' || code == 'g' ? "nan" : "NAN",
		       px->n1 = 3);
		return;
	} else if (0 < errx) {	/* x == INF */
		memcpy(px->s, code == 'a' || code == 'e' || code == 'f' || code == 'g' ? "inf" : "INF",
		       px->n1 = 3);
		return;
	}

	if (code == 'a' || code == 'A') {	/* put "0x" */
		*px->s++ = '0';
		*px->s++ = code == 'a' ? 'x' : 'X';
		px->n0 += 2;
	}
	if (errx == 0) {	/* x == 0 */
		nsig = 0;
		xexp = 0;
	} else if (code == 'a' || code == 'A') {	/* hexadecimal digits, binary exponent */
		const char *digits = code == 'a' ? "0123456789abcdef" : "0123456789ABCDEF";
		int gen;

		nsig = px->prec < 0 ? MAXSIG - 7 : px->prec + 1;
		gen = nsig + 1;
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
				*--p = (char)(lo & 0xf);
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
		_Xfloat xx;
		int err;
		int gen;

		if (ldval < 0)
			ldval = -ldval;
		xx = _Xfromd(ldval);
		xexp = xexp * 30103L / 100000L - NDIG / 2;	/* the power of ten, low */
		if (xexp < 0) {	/* scale up */
			xexp = -((NDIG / 2 - 1 - xexp) & ~(NDIG / 2 - 1));
			xx = _Xtento(xx, -xexp);
		} else if (0 < xexp) {	/* scale down */
			xexp &= ~(NDIG / 2 - 1);
			if (0 < xexp)
				xx = _Xtento(xx, -xexp);
		}
		gen = (code == 'f' || code == 'F' ? xexp + 10 : 6) + px->prec;
		if (19 < gen)
			gen = 19;
		ldval = _Xtold(xx, &err);
		ac[0] = '0';
		for (p = &ac[1]; 0 < gen && 0 < ldval; p += NDIG) {	/* NDIG at a time */
			int j;
			long lo = (long)ldval;

			if (0 < (gen -= NDIG)) {
				xx = _Xsub(xx, _Xfromd((double)lo));
				xx = _Xmul(xx, (_Xfloat){0x4019, 0xbebc200000000000ULL});	/* 1e8 */
				ldval = _Xtold(xx, &err);
			}
			for (p += NDIG, j = NDIG; 0 < lo && 0 <= --j;) {
				ldiv_t qr = ldiv(lo, 10);

				*--p = (char)(qr.rem + '0');
				lo = qr.quot;
			}
			while (0 <= --j)
				*--p = '0';
		}
		gen = p - &ac[1];
		p = &ac[1];
		xexp += NDIG - 1;
		for (; *p == '0'; ++p)	/* the leading zeros */
			--gen, --xexp;
		nsig = px->prec + (code == 'f' || code == 'F' ? xexp + 1
				   : code == 'e' || code == 'E' ? 1 : 0);
		if (17 < nsig)
			nsig = 17;
		if (0 <= nsig) {	/* round and strip trailing zeros */
			const char drop = nsig < gen && '5' <= p[nsig] ? '9' : '0';
			int n;

			for (n = nsig; p[--n] == drop;)
				--nsig;
			if (drop == '9')
				++p[n];
			if (n < 0) {	/* the carry went into ac[0] */
				++xexp;
				--p;
				++nsig;
			}
		}
	}
	_Genld(px, code, p, nsig, xexp);
}
