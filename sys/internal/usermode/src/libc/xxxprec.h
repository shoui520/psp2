/*
 * The extended-precision helpers, written once for each floating type: a
 * value is held as the sum of n terms of decreasing magnitude, each term
 * carrying at most BITS_WORD significant bits, a zero term ending the
 * sum. The includer defines FTYPE (the type), FNAME(x) (the helper's name
 * for the type), FCONST(x) (the type's member of a _Dconst), FLIT(x) (a
 * literal of the type), FSIGNBIT(x) (the sign bit), FLDEXP, FSQRT,
 * BITS_WORD and BIG_EXP (an exponent above every term's).
 */
#include <string.h>

#include "xmath.h"

#define NBUF	4	/* the partial products _Xp_mulh keeps */

/* the sum of the terms */
FTYPE FNAME(Xp_getw)(const FTYPE *p, int n)
{
	if (n == 0)
		return FLIT(0.0);
	else if (n == 1 || p[0] == FLIT(0.0) || (1 < n && p[1] == FLIT(0.0)))
		return p[0];
	else if (n == 2)
		return p[0] + p[1];
	else
		return p[0] + (p[1] + p[2]);
}

/* set the terms to x, split in two where it has more than BITS_WORD bits */
FTYPE *FNAME(Xp_setw)(FTYPE *p, int n, FTYPE x)
{
	FTYPE x0 = x;
	short errx;
	short xexp;

	if (n <= 0) {
		;
	} else if (n == 1 || (errx = FNAME(Dunscale)(&xexp, &x0)) == 0) {
		p[0] = x0;	/* zero or no room: one term */
	} else if (0 < errx) {	/* INF or NaN */
		p[0] = x0;
		p[1] = FLIT(0.0);
	} else {	/* the high bits and the rest */
		FNAME(Dint)(&x0, BITS_WORD);
		FNAME(Dscale)(&x0, xexp);
		p[0] = x0;
		p[1] = x - x0;
		if (2 < n)
			p[2] = FLIT(0.0);
	}
	return p;
}

/* add a value of at most BITS_WORD bits, renormalizing the terms */
FTYPE *FNAME(Xp_addh)(FTYPE *p, int n, FTYPE x0)
{
	FTYPE xscaled = x0;
	short errx;
	short xexp;

	if (n == 0) {
		;
	} else if (0 < (errx = FNAME(Dunscale)(&xexp, &xscaled))) {	/* x0 INF or NaN */
		if (errx == _NANCODE || (errx = FNAME(Dtest)(p)) <= 0)
			p[0] = x0;
		else if (errx != _NANCODE && FSIGNBIT(x0) != FSIGNBIT(p[0])) {	/* INF - INF */
			_Feraise(_FE_INVALID);
			p[0] = FCONST(Nan);
			if (1 < n)
				p[1] = FLIT(0.0);
		}
	} else if (errx < 0) {	/* x0 finite and nonzero */
		long prevexp = BIG_EXP;
		int k;

		for (k = 0; k < n;) {	/* find the term x0 belongs in */
			FTYPE yscaled = p[k];
			int mybits = BITS_WORD;
			short yexp;
			long diff;

			if (0 < (errx = FNAME(Dunscale)(&yexp, &yscaled))) {
				break;	/* INF or NaN */
			} else if (errx == 0) {	/* the end of the terms: x0 goes here */
				p[k] = x0;
				if (k + 1 < n)
					p[k + 1] = FLIT(0.0);
				break;
			} else if ((diff = (long)yexp - xexp) <= -mybits && x0 != FLIT(0.0)) {
				int j;	/* x0 above p[k]: insert it */

				for (j = k; ++j < n && p[j] != FLIT(0.0);)
					;
				if (j < n - 1)
					++j;	/* room: move the zero down too */
				else if (j == n)
					--j;	/* no room: drop the last term */
				for (; k < j; --j)
					p[j] = p[j - 1];
				p[k] = x0;
				x0 = FLIT(0.0);
			} else if (mybits <= diff && x0 != FLIT(0.0)) {	/* x0 below p[k] */
				prevexp = yexp;
				++k;
			} else {	/* add x0 to p[k] */
				if ((p[k] += x0) == FLIT(0.0)) {	/* it vanished: close up */
					int m;

					for (m = k; ++m < n && (p[m - 1] = p[m]) != FLIT(0.0);)
						;
					p[n - 1] = FLIT(0.0);
					if (p[k] == FLIT(0.0))
						break;
				}
				x0 = p[k];
				FNAME(Dunscale)(&xexp, &x0);
				if (prevexp - mybits < xexp) {	/* carry the high bits up */
					x0 = p[k];
					FNAME(Dint)(&x0, (short)(mybits - (prevexp - xexp)));
					FNAME(Dscale)(&x0, xexp);
					if ((p[k] -= x0) == FLIT(0.0)) {	/* all of them: close up */
						int m;

						for (m = k; ++m < n && (p[m - 1] = p[m]) != FLIT(0.0);)
							;
						p[n - 1] = FLIT(0.0);
					}
					if (--k == 0) {
						prevexp = BIG_EXP;
					} else {
						xscaled = p[k - 1];
						FNAME(Dunscale)(&yexp, &xscaled);
						prevexp = yexp;
					}
				} else if (k + 1 == n) {
					break;	/* keep the bits of the last term */
				} else {	/* carry the low bits down */
					x0 = p[k];
					FNAME(Dunscale)(&yexp, &p[k]);
					FNAME(Dint)(&p[k], BITS_WORD);
					FNAME(Dscale)(&p[k], yexp);
					x0 -= p[k];
					prevexp = yexp;
					xscaled = x0 != FLIT(0.0) ? x0 : p[k];
					FNAME(Dunscale)(&xexp, &xscaled);
					++k;
				}
			}
		}
	}
	return p;
}

/* multiply by a value of at most BITS_WORD bits */
FTYPE *FNAME(Xp_mulh)(FTYPE *p, int n, FTYPE x0)
{
	short errx;
	int j;
	int k;
	FTYPE buf[NBUF];

	if (0 < n) {	/* zero, INF and NaN end early */
		buf[0] = p[0] * x0;
		if (0 <= (errx = FNAME(Dtest)(&buf[0]))) {
			if (errx == _NANCODE)
				_Feraise(_FE_INVALID);
			p[0] = buf[0];
			if (0 < errx && 1 < n)
				p[1] = FLIT(0.0);
			return p;
		}
		p[0] = FLIT(0.0);
	}
	for (j = 1, k = 0; k < n; ++k, --j) {	/* sum the partial products */
		for (; j < NBUF; ++j) {
			if (k + j < n && p[k + j] != FLIT(0.0)) {
				buf[j] = p[k + j] * x0;
				p[k + j] = FLIT(0.0);
			} else {	/* the end of the terms */
				buf[j] = FLIT(0.0);
				j = 2 * NBUF;
				break;
			}
		}
		if (buf[0] == FLIT(0.0)) {
			break;
		} else {	/* add the product in two halves */
			int i;
			FTYPE y0 = buf[0];
			short xexp;

			FNAME(Dunscale)(&xexp, &y0);
			FNAME(Dint)(&y0, BITS_WORD);
			FNAME(Dscale)(&y0, xexp);
			FNAME(Xp_addh)(p, n, y0);
			FNAME(Xp_addh)(p, n, buf[0] - y0);
			for (i = 1; i < j; ++i) {
				if ((buf[i - 1] = buf[i]) == FLIT(0.0))
					break;
			}
		}
	}
	return p;
}

/* set the terms to the integer x */
FTYPE *FNAME(Xp_setn)(FTYPE *p, int n, long x)
{
#if BITS_WORD < 16
	FNAME(Xp_setw)(p, n, (FTYPE)(x / 10000));	/* x has more bits than a float */
	FNAME(Xp_mulh)(p, n, (FTYPE)10000);
	FNAME(Xp_addh)(p, n, (FTYPE)(x % 10000));
#else
	FNAME(Xp_setw)(p, n, (FTYPE)x);
#endif
	return p;
}

/* copy n terms of q to p */
FTYPE *FNAME(Xp_movx)(FTYPE *p, int n, const FTYPE *q)
{
	memcpy(p, q, n * sizeof(FTYPE));
	return p;
}

/* add the terms of q */
FTYPE *FNAME(Xp_addx)(FTYPE *p, int n, const FTYPE *q, int m)
{
	int k;

	for (k = 0; k < m && q[k] != FLIT(0.0); ++k)
		FNAME(Xp_addh)(p, n, q[k]);
	return p;
}

/* subtract the terms of q */
FTYPE *FNAME(Xp_subx)(FTYPE *p, int n, const FTYPE *q, int m)
{
	int k;

	for (k = 0; k < m && q[k] != FLIT(0.0); ++k)
		FNAME(Xp_addh)(p, n, -q[k]);
	return p;
}

/* multiply by 2^m */
FTYPE *FNAME(Xp_ldexpx)(FTYPE *p, int n, int m)
{
	int k;

	for (k = 0; k < n; ++k) {
		p[k] = FLDEXP(p[k], m);
		if (p[k] == FLIT(0.0))
			break;
	}
	return p;
}

/* multiply by the terms of q, with 2 * n terms of scratch at ptemp2 */
FTYPE *FNAME(Xp_mulx)(FTYPE *p, int n, const FTYPE *q, int m, FTYPE *ptemp2)
{
	if (n == 0 || m == 0) {
		;
	} else if (q[0] == FLIT(0.0) || q[1] == FLIT(0.0)) {
		FNAME(Xp_mulh)(p, n, q[0]);
	} else {	/* sum the products by each term of q */
		FTYPE *px = ptemp2;
		FTYPE *pac = ptemp2 + n;
		int j;

		FNAME(Xp_movx)(px, n, p);
		FNAME(Xp_mulh)(p, n, q[0]);
		for (j = 1; j < m && q[j] != FLIT(0.0); ++j) {
			FNAME(Xp_movx)(pac, n, px);
			FNAME(Xp_mulh)(pac, n, q[j]);
			FNAME(Xp_addx)(p, n, pac, n);
		}
	}
	return p;
}

/* 1 / p, by Newton's iteration, with 4 * n terms of scratch at ptemp4 */
FTYPE *FNAME(Xp_invx)(FTYPE *p, int n, FTYPE *ptemp4)
{
	short errx;

	if (n == 0) {
		;
	} else if (0 <= (errx = FNAME(Dtest)(p))) {	/* zero, INF or NaN */
		if (errx == _INFCODE)
			p[0] = FLIT(0.0);
		else if (errx == 0)
			p[0] = FCONST(Inf);
	} else {
		FTYPE *pac = ptemp4;
		FTYPE *py = ptemp4 + n;
		FTYPE *ptemp2 = py + n;
		FTYPE x0 = p[0];
		int k;

		FNAME(Xp_movx)(py, n, p);
		FNAME(Xp_mulh)(py, n, FLIT(-1.0));	/* py = -x */
		if (1 < n)
			x0 += p[1];
		FNAME(Xp_setw)(p, n, FLIT(1.0) / x0);	/* p = y0 */
		for (k = 2; k < n; k *= 2) {	/* each step doubles the precision */
			FNAME(Xp_movx)(pac, n, p);
			FNAME(Xp_mulx)(pac, n, py, n, ptemp2);
			FNAME(Xp_addh)(pac, n, FLIT(1.0));	/* 1 - x * y */
			FNAME(Xp_mulx)(pac, n, p, n, ptemp2);	/* y * (1 - x * y) */
			FNAME(Xp_addx)(p, n, pac, n);	/* y += y * (1 - x * y) */
		}
	}
	return p;
}

/* sqrt(p), as p / sqrt(p) by Newton's iteration, with 4 * n terms of scratch at ptemp4 */
FTYPE *FNAME(Xp_sqrtx)(FTYPE *p, int n, FTYPE *ptemp4)
{
	if (n == 0) {
		;
	} else if (0 <= FNAME(Dtest)(p) || p[0] < 0) {	/* zero, INF, NaN or negative */
		if (p[0] < 0) {
			_Feraise(_FE_INVALID);
			p[0] = FCONST(Nan);
		}
	} else {
		FTYPE *pac = ptemp4;
		FTYPE *py = ptemp4 + n;
		FTYPE *ptemp2 = py + n;
		FTYPE x0 = p[0];
		int k;

		if (1 < n)
			x0 += p[1];
		FNAME(Xp_setw)(py, n, FLIT(1.0) / FSQRT(x0));	/* py = y0 */
		for (k = 2; k < n; k *= 2) {	/* each step doubles the precision */
			FNAME(Xp_movx)(pac, n, py);
			FNAME(Xp_mulh)(pac, n, FLIT(-0.5));
			FNAME(Xp_mulx)(pac, n, p, n, ptemp2);
			FNAME(Xp_mulx)(pac, n, py, n, ptemp2);
			FNAME(Xp_addh)(pac, n, FLIT(1.5));	/* 3/2 - x * y * y / 2 */
			FNAME(Xp_mulx)(py, n, pac, n, ptemp2);	/* y *= 3/2 - x * y * y / 2 */
		}
		FNAME(Xp_mulx)(p, n, py, n, ptemp2);	/* x * (1 / sqrt(x)) */
	}
	return p;
}
