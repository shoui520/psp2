/*
 * _LQuad function: reduce *px to [-pi/4, pi/4] by a multiple of pi/2 and
 * return the quadrant, modulo 4; _LQuadph does the same for *px + phase pi.
 * A large *px is first reduced modulo 2pi in the extended precision of the
 * _LXp_ helpers, from a table of 2^(32k) modulo 2pi.
 */
#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#include "xmath.h"

#define NTERMS	6U	/* the terms of a reduced value */
#define NUSED(e)	((e) / LDBL_MANT_DIG * 2 + NTERMS - 1)	/* the terms worth keeping below 2^e */

/* 2pi 2^26, then 2^(32k) modulo 2pi for k = 1 to 31, each in terms of 26 bits */
static const long double twopi_mod[][NTERMS] = {
	{421657424.0L, 4.266313076019287L, 1.6436930039276376e-08L,
	 1.0591266091149881e-15L, 2.2759105699003892e-23L, 2.5273749203696416e-31L},
	{-2.6613588333129883L, 2.1647686310188874e-08L, 6.910990653391202e-16L,
	 6.408695738823732e-24L, 1.5408024069663833e-31L, 4.40091445381236e-40L},
	{3.1179919242858887L, 2.8555996678392148e-08L, 4.6610476081950436e-17L,
	 3.482582857772617e-24L, 3.7185088708813144e-32L, 1.2500102095926479e-39L},
	{3.075364589691162L, 3.0668620532026125e-08L, 7.600455090725909e-16L,
	 9.337292545650162e-24L, 5.614598009552163e-32L, 1.4591619314875657e-39L},
	{1.3461300730705261L, 1.1628260576657112e-08L, 2.29676874705605e-16L,
	 7.976429978559979e-25L, 8.404072846549933e-32L, 5.734912494047587e-41L},
	{1.9405710995197296L, 8.893802139198215e-09L, 3.142594142964921e-16L,
	 1.4016839495678813e-24L, 8.703118021651947e-32L, 6.818110514195659e-40L},
	{3.131271541118622L, 4.250965535135265e-08L, 7.621561298886914e-16L,
	 6.824431746762447e-25L, 1.0824217056347734e-31L, 1.8252993687177123e-39L},
	{-0.20412081480026245L, 7.97096788573981e-09L, 3.042639093581891e-16L,
	 9.541507334767441e-25L, 5.853046597806083e-32L, 7.580843617961323e-40L},
	{2.694543421268463L, 2.6702222655217156e-08L, 1.690235717400235e-17L,
	 6.208597817555575e-24L, 9.495289542142841e-32L, 5.628648090345905e-41L},
	{-2.7347564697265625L, 5.832538096939288e-08L, 4.698341410071466e-16L,
	 1.3057742998756786e-23L, 1.7189640923589318e-31L, 2.8894335990701856e-39L},
	{0.03856959939002991L, 2.878096738712088e-08L, 3.61145983414578e-16L,
	 5.4993931283153866e-24L, 1.469506940734844e-32L, 1.0079393155455425e-40L},
	{-3.081346273422241L, 1.4316768215394404e-08L, 3.3599287682471854e-16L,
	 2.2059087097789776e-24L, 1.6678276489473844e-31L, 1.2815707790930955e-39L},
	{1.5592415630817413L, 1.523601955710774e-08L, 1.3921582719909954e-16L,
	 4.861832589272144e-24L, 1.255779292552305e-32L, 2.3941819226356147e-40L},
	{1.0870567560195923L, 6.921511364055277e-09L, 4.660133739054295e-17L,
	 6.242700570311119e-24L, 5.104306948715783e-32L, 9.464677894759081e-40L},
	{-1.714937537908554L, 5.840647521182518e-09L, 4.037573919594143e-16L,
	 5.045967712907927e-24L, 5.379451972676879e-32L, 2.4748542373210243e-40L},
	{1.8575372993946075L, 7.715371896921397e-09L, 1.3414763864642794e-16L,
	 2.6942528559408186e-25L, 6.489690517854794e-32L, 2.984065736638353e-40L},
	{1.433419644832611L, 2.7999477847373555e-08L, 3.6215878997403284e-16L,
	 3.014372414064279e-24L, 6.750495980103796e-32L, 7.360892964613394e-40L},
	{-2.0348376631736755L, 4.1961722985206507e-10L, 3.201688190623505e-16L,
	 1.3171617042577702e-23L, 1.2617618828887388e-31L, 1.918370547377776e-39L},
	{1.9240394234657288L, 5.349858778203043e-09L, 6.516331004090229e-17L,
	 4.819961134321984e-24L, 8.981267756110539e-32L, 7.636349269086114e-40L},
	{2.376597046852112L, 2.03159107314832e-08L, 4.1536571380373845e-16L,
	 3.1188690710896065e-24L, 1.3929088488741044e-31L, 1.8386948186439633e-39L},
	{0.9603767693042755L, 2.3361105494501544e-08L, 1.449118127494744e-16L,
	 5.138146405505842e-24L, 6.669710865527503e-33L, 2.8866555686552387e-40L},
	{-0.4051349461078644L, 5.651379364479681e-09L, 1.9059153858873526e-16L,
	 1.7875986487065264e-24L, 6.579034553736217e-32L, 1.316575367998949e-39L},
	{-1.3746243119239807L, 9.59849311144012e-09L, 2.3132800000015473e-16L,
	 5.259079176706676e-24L, 5.376671340790009e-32L, 1.3654485202250712e-39L},
	{-0.20848733186721802L, 2.4058547598571067e-08L, 1.4972245034662149e-16L,
	 2.285694890593936e-24L, 7.717946638740152e-32L, 3.797926090686449e-40L},
	{-0.10318255424499512L, 2.0429783198494533e-08L, 2.9904678197314365e-16L,
	 5.123690332202441e-24L, 3.5993916202086764e-33L, 1.8323575777107854e-40L},
	{-1.0183133482933044L, 6.44550635087171e-09L, 4.177074952051006e-16L,
	 3.5533903223719626e-24L, 1.5878724627701312e-33L, 1.0422459483397197e-39L},
	{-0.9006745517253876L, 1.7472660207573654e-08L, 3.228577714755236e-16L,
	 3.477433962644239e-24L, 3.337594334601168e-33L, 9.50117164792203e-40L},
	{-1.3266288340091705L, 1.511857794511684e-08L, 4.197107148904672e-16L,
	 1.897088006400998e-24L, 7.760487191549235e-32L, 5.059900017290001e-40L},
	{-1.0843746960163116L, 2.5696393013419083e-08L, 1.3694588477957642e-16L,
	 5.1916620390627434e-24L, 7.139286916281627e-32L, 1.7122413386892528e-40L},
	{-2.1698179244995117L, 5.687181570124267e-08L, 8.159349002194672e-16L,
	 6.5079255660153516e-24L, 1.0916679972615476e-31L, 4.570172916260453e-40L},
	{2.289651572704315L, 2.0057680849561166e-08L, 2.1323776514441193e-17L,
	 4.162958230801051e-24L, 8.117559910595622e-32L, 1.662302652248031e-39L},
	{-0.12334302067756653L, 1.8622198005857626e-08L, 3.382305128782378e-16L,
	 5.055551781260684e-24L, 8.751708696947919e-32L, 1.3288887746625677e-40L},
};
static const long double huge_rad = 85445659.44705395L;	/* 2^27 2/pi: reduce modulo 2pi above it */
static const long double invtwopi = 0.15915494309189535L;
static const long double twobypi = 0.6366197723675814L;
static const long double pi = 3.141592653589793L;
static const long double piby4 = 0.7853981633974483L;

unsigned int _LQuad(long double *px)
{
	long double x = *px;
	long double g;

	if (-huge_rad < x && x < huge_rad) {	/* reduce by pi/2 directly */
		g = x * twobypi;
		g = 0.0L <= g ? g + 0.5L : g - 0.5L;
		_LDint(&g, 0);
		if (g != 0.0L) {	/* g pi/2 is gs times 2pi 2^26 */
			const long double gs = g * 0.25L / 67108864.0L;

			x = x - gs * twopi_mod[0][0] - gs * twopi_mod[0][1] - gs * twopi_mod[0][2]
			    - gs * twopi_mod[0][3] - gs * twopi_mod[0][4];
		}
	} else {	/* reduce modulo 2pi first */
		long double acc[NTERMS];
		long double w[2];
		long double t[NTERMS];
		short xexp;
		int n;

		g = x;
		_LDunscale(&xexp, &g);
		if (xexp < 86) {
			_LXp_setw(acc, NTERMS, x);
		} else {	/* x 2^(-32k) times 2^(32k) modulo 2pi */
			xexp = (xexp - 54) >> 5;
			_LDscale(&x, -32 * xexp);
			_LXp_setw(w, 2, x);
			memcpy(acc, twopi_mod[xexp], sizeof(acc));
			_LXp_mulh(acc, NTERMS, w[0]);
			if (w[1] != 0.0L) {
				memcpy(t, twopi_mod[xexp], sizeof(t));
				_LXp_mulh(t, NTERMS, w[1]);
				_LXp_addx(acc, NTERMS, t, NTERMS);
			}
		}
		for (n = NTERMS; acc[0] < -huge_rad || huge_rad < acc[0];) {	/* subtract multiples of 2pi */
			g = (acc[0] + acc[1]) * invtwopi;
			_LDint(&g, 0);
			_LXp_setw(w, 2, -g / 67108864.0L);
			memcpy(t, twopi_mod[0], n * sizeof(long double));
			_LXp_mulh(t, n, w[0]);
			if (w[1] != 0.0L) {
				long double u[NTERMS];
				long double y = acc[0];
				short yexp;

				_LDunscale(&yexp, &y);
				n = NUSED(yexp) <= NTERMS ? NUSED(yexp) : NTERMS;
				memcpy(u, twopi_mod[0], n * sizeof(long double));
				_LXp_mulh(u, n, w[1]);
				_LXp_addx(t, n, u, n);
			}
			_LXp_addx(acc, n, t, n);
		}
		g = (acc[0] + acc[1]) * twobypi;	/* then by pi/2 */
		g = 0.0L <= g ? g + 0.5L : g - 0.5L;
		_LDint(&g, 0);
		if (g != 0.0L) {
			long double v[NTERMS];
			long double y = acc[0];
			short yexp;

			_LDunscale(&yexp, &y);
			n = NUSED(yexp) <= NTERMS ? NUSED(yexp) : NTERMS;
			memcpy(v, twopi_mod[0], n * sizeof(long double));
			_LXp_mulh(v, n, g * -0.25L / 67108864.0L);
			_LXp_addx(acc, n, v, n);
		}
		x = _LXp_getw(acc, n);
	}
	*px = x;
	if (g < -(long double)INT_MAX || (long double)INT_MAX < g)	/* keep the quadrant in an int */
		g = fmodl(g, (long double)INT_MAX + 1);
	return (int)g & 3;
}

unsigned int _LQuadph(long double *px, long double phase)
{
	unsigned int qoff = _LQuad(px);
	long double ph = phase;
	long double x;

	_LDint(&ph, 1);	/* the multiple of 1/2 in phase */
	phase -= ph;
	if (ph < -(long double)(INT_MAX / 2) || (long double)(INT_MAX / 2) < ph)
		ph = fmodl(ph, (long double)(INT_MAX / 2) + 1);
	qoff += (int)(ph * 2.0L) & 3;
	x = *px + phase * pi;
	if (piby4 <= x) {
		++qoff;
		x = *px + (phase - 0.5L) * pi;
	} else if (x <= -piby4) {
		--qoff;
		x = *px + (phase + 0.5L) * pi;
	}
	*px = x;
	return qoff;
}
