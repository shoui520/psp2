/*
 * _FQuad function: reduce *px to [-pi/4, pi/4] by a multiple of pi/2 and
 * return the quadrant, modulo 4; _FQuadph does the same for *px + phase pi.
 * A large *px is first reduced modulo 2pi in the extended precision of the
 * _FXp_ helpers, from a table of 2^(8k) modulo 2pi.
 */
#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#include "xmath.h"

#define NTERMS	5U	/* the terms of a reduced value */
#define NUSED(e)	((e) / FLT_MANT_DIG * 2 + NTERMS - 1)	/* the terms worth keeping below 2^e */

/* 2pi 2^12, then 2^(8k) modulo 2pi for k = 1 to 15, each in terms of 12 bits */
static const float twopi_mod[][NTERMS] = {
	{25728.0F, 7.9257812F, 0.0012369156F, 4.1909516e-08F, 8.830625e-11F},
	{-1.6108398F, 0.00024223328F, 1.6094418e-08F, 1.6164847e-11F, 4.1685405e-15F},
	{2.3769531F, 0.00029277802F, 2.1385495e-07F, 4.2959414e-11F, 3.337608e-15F},
	{-0.89404297F, 7.402897e-05F, 7.307972e-08F, 2.127365e-11F, 1.0581813e-16F},
	{-2.6621094F, 0.0007505417F, 2.1595042e-08F, 5.2637006e-11F, 7.795847e-15F},
	{-2.7246094F, 0.00076675415F, 9.895302e-09F, 3.2514436e-11F, 4.4200754e-15F},
	{0.12939453F, 0.00046610832F, 6.137998e-08F, 4.206413e-12F, 9.731799e-16F},
	{1.828125F, 0.00028789043F, 1.8480932e-08F, 7.183587e-12F, 6.7654216e-15F},
	{3.1171875F, 0.0008044243F, 2.8521754e-08F, 3.423395e-11F, 8.926887e-15F},
	{0.24121094F, 0.00019490719F, 7.1013346e-08F, 1.3415047e-11F, 2.1423835e-15F},
	{-1.0322266F, 0.00028789043F, 2.3341272e-08F, 1.44240175e-11F, 6.0264294e-15F},
	{-0.28271484F, 0.00020360947F, 6.603659e-08F, 2.1088908e-11F, 4.3385434e-15F},
	{3.0751953F, 0.00016927719F, 3.061723e-08F, 5.138645e-11F, 5.200701e-15F},
	{1.8950195F, 0.00015985966F, 2.3748726e-08F, 2.2737368e-13F, 5.2909066e-16F},
	{1.3603516F, 0.00030982494F, 1.117005e-07F, 2.55298e-11F, 3.299444e-15F},
	{2.7539062F, 0.00024557114F, 7.136259e-08F, 3.5356607e-11F, 2.220446e-16F},
};
static const float huge_rad = 2607.5945F;	/* 2^12 2/pi: reduce modulo 2pi above it */
static const float invtwopi = 0.15915494F;
static const float twobypi = 0.63661975F;
static const float pi = 3.1415927F;
static const float piby4 = 0.7853982F;

unsigned int _FQuad(float *px)
{
	float x = *px;
	float g;

	if (-huge_rad < x && x < huge_rad) {	/* reduce by pi/2 directly */
		g = x * twobypi;
		g = 0.0F <= g ? g + 0.5F : g - 0.5F;
		_FDint(&g, 0);
		if (g != 0.0F) {	/* g pi/2 is gs times 2pi 2^12 */
			const float gs = (float)(g * 0.25F / 4096.0);

			x = x - gs * twopi_mod[0][0] - gs * twopi_mod[0][1] - gs * twopi_mod[0][2]
			    - gs * twopi_mod[0][3] - gs * twopi_mod[0][4];
		}
	} else {	/* reduce modulo 2pi first */
		float acc[NTERMS];
		float w[2];
		float t[NTERMS];
		short xexp;
		int n;

		g = x;
		_FDunscale(&xexp, &g);
		if (xexp < 33) {
			_FXp_setw(acc, NTERMS, x);
		} else {	/* x 2^(-8k) times 2^(8k) modulo 2pi */
			xexp = (xexp - 25) >> 3;
			_FDscale(&x, -8 * xexp);
			_FXp_setw(w, 2, x);
			memcpy(acc, twopi_mod[xexp], sizeof(acc));
			_FXp_mulh(acc, NTERMS, w[0]);
			if (w[1] != 0.0F) {
				memcpy(t, twopi_mod[xexp], sizeof(t));
				_FXp_mulh(t, NTERMS, w[1]);
				_FXp_addx(acc, NTERMS, t, NTERMS);
			}
		}
		for (n = NTERMS; acc[0] < -huge_rad || huge_rad < acc[0];) {	/* subtract multiples of 2pi */
			g = (acc[0] + acc[1]) * invtwopi;
			_FDint(&g, 0);
			_FXp_setw(w, 2, (float)(-g / 4096.0));
			memcpy(t, twopi_mod[0], n * sizeof(float));
			_FXp_mulh(t, n, w[0]);
			if (w[1] != 0.0F) {
				float u[NTERMS];
				float y = acc[0];
				short yexp;

				_FDunscale(&yexp, &y);
				n = NUSED(yexp) <= NTERMS ? NUSED(yexp) : NTERMS;
				memcpy(u, twopi_mod[0], n * sizeof(float));
				_FXp_mulh(u, n, w[1]);
				_FXp_addx(t, n, u, n);
			}
			_FXp_addx(acc, n, t, n);
		}
		g = (acc[0] + acc[1]) * twobypi;	/* then by pi/2 */
		g = 0.0F <= g ? g + 0.5F : g - 0.5F;
		_FDint(&g, 0);
		if (g != 0.0F) {
			float v[NTERMS];
			float y = acc[0];
			short yexp;

			_FDunscale(&yexp, &y);
			n = NUSED(yexp) <= NTERMS ? NUSED(yexp) : NTERMS;
			memcpy(v, twopi_mod[0], n * sizeof(float));
			_FXp_mulh(v, n, (float)(g * -0.25F / 4096.0));
			_FXp_addx(acc, n, v, n);
		}
		x = _FXp_getw(acc, n);
	}
	*px = x;
	if (g < -(float)INT_MAX || (float)INT_MAX < g)	/* keep the quadrant in an int */
		g = fmodf(g, (float)INT_MAX + 1);
	return (int)g & 3;
}

unsigned int _FQuadph(float *px, float phase)
{
	unsigned int qoff = _FQuad(px);
	float ph = phase;
	float x;

	_FDint(&ph, 1);	/* the multiple of 1/2 in phase */
	phase -= ph;
	if (ph < -(float)(INT_MAX / 2) || (float)(INT_MAX / 2) < ph)
		ph = fmodf(ph, (float)(INT_MAX / 2) + 1);
	qoff += (int)(ph * 2.0F) & 3;
	x = *px + phase * pi;
	if (piby4 <= x) {
		++qoff;
		x = *px + (phase - 0.5F) * pi;
	} else if (x <= -piby4) {
		--qoff;
		x = *px + (phase + 0.5F) * pi;
	}
	*px = x;
	return qoff;
}
