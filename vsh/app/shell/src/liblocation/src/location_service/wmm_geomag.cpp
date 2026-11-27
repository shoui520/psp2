/*
 * wmm_geomag.cpp: IGRF-11 (2010.0 epoch, degree 13 main / degree 8 secular variation)
 * geomagnetic field & magnetic declination calculator (0x81198C94-0x81199430).
 * The file and function names are ours (PLACEHOLDER).
 */
#include "location_internal.h"
#include <math.h>
#include <std/string.h>

extern "C" {
float sce_paf_sinf(float x);
float sce_paf_cosf(float x);
float sce_paf_atan2f(float y, float x);
float sce_paf_powf(float x, float y);
}

static const float s_wmmMain[196] = {
	0.0f, -29496.5f, -1585.9f, 4945.1001f, -2396.6001f, 3026.0f, -2707.7f, 1668.6f,
	-575.40002f, 1339.7f, -2326.3f, -160.5f, 1231.7f, 251.7f, 634.20001f, -536.79999f,
	912.59998f, 809.0f, 286.39999f, 166.60001f, -211.2f, -357.10001f, 164.39999f, 89.699997f,
	-309.20001f, -231.10001f, 357.20001f, 44.700001f, 200.3f, 188.89999f, -141.2f, -118.1f,
	-163.10001f, 0.1f, -7.6999998f, 100.9f, 72.800003f, 68.599998f, -20.799999f, 76.0f,
	44.200001f, -141.39999f, 61.5f, -22.9f, -66.300003f, 13.1f, 3.0999999f, -77.900002f,
	54.900002f, 80.400002f, -75.0f, -57.799999f, -4.6999998f, -21.200001f, 45.299999f, 6.5999999f,
	14.0f, 24.9f, 10.4f, 7.0f, 1.6f, -27.700001f, 4.9000001f, -3.4000001f,
	24.299999f, 8.1999998f, 10.9f, -14.5f, -20.0f, -5.6999998f, 11.9f, -19.299999f,
	-17.4f, 11.6f, 16.700001f, 10.9f, 7.0999999f, -14.1f, -10.8f, -3.7f,
	1.7f, 5.4000001f, 9.3999996f, -20.5f, 3.4000001f, 11.6f, -5.3000002f, 12.8f,
	3.0999999f, -7.1999998f, -12.4f, -7.4000001f, -0.80000001f, 8.0f, 8.3999996f, 2.2f,
	-8.3999996f, -6.0999999f, -10.1f, 7.0f, -2.0f, -6.3000002f, 2.8f, 0.89999998f,
	-0.1f, -1.1f, 4.6999998f, -0.2f, 4.4000001f, 2.5f, -7.1999998f, -0.30000001f,
	-1.0f, 2.2f, -4.0f, 3.0999999f, -2.0f, -1.0f, -2.0f, -2.8f,
	-8.3000002f, 3.0f, -1.5f, 0.1f, -2.0999999f, 1.7f, 1.6f, -0.60000002f,
	-0.5f, -1.8f, 0.5f, 0.89999998f, -0.80000001f, -0.40000001f, 0.40000001f, -2.5f,
	1.8f, -1.3f, 0.2f, -2.0999999f, 0.80000001f, -1.9f, 3.8f, -1.8f,
	-2.0999999f, -0.2f, -0.80000001f, 0.30000001f, 0.30000001f, 1.0f, 2.2f, -0.69999999f,
	-2.5f, 0.89999998f, 0.5f, -0.1f, 0.60000002f, 0.5f, 0.0f, -0.40000001f,
	0.1f, -0.40000001f, 0.30000001f, 0.2f, -0.89999998f, -0.80000001f, -0.2f, 0.0f,
	0.80000001f, -0.2f, -0.89999998f, -0.80000001f, 0.30000001f, 0.30000001f, 0.40000001f, 1.7f,
	-0.40000001f, -0.60000002f, 1.1f, -1.2f, -0.30000001f, -0.1f, 0.80000001f, 0.5f,
	-0.2f, 0.1f, 0.40000001f, 0.5f, 0.0f, 0.40000001f, 0.40000001f, -0.2f,
	-0.30000001f, -0.5f, -0.30000001f, -0.80000001f,
};

static const float s_wmmSecular[81] = {
	0.0f, 11.4f, 16.700001f, -28.799999f, -11.3f, -3.9000001f, -23.0f, 2.7f,
	-12.9f, 1.3f, -3.9000001f, 8.6000004f, -2.9000001f, -2.9000001f, -8.1000004f, -2.0999999f,
	-1.4f, 2.0f, 0.40000001f, -8.8999996f, 3.2f, 4.4000001f, 3.5999999f, -2.3f,
	-0.80000001f, -0.5f, 0.5f, 0.5f, -1.5f, 1.5f, -0.69999999f, 0.89999998f,
	1.3f, 3.7f, 1.4f, -0.60000002f, -0.30000001f, -0.30000001f, -0.1f, -0.30000001f,
	-2.0999999f, 1.9f, -0.40000001f, -1.6f, -0.5f, -0.2f, 0.80000001f, 1.8f,
	0.5f, 0.2f, -0.1f, 0.60000002f, -0.60000002f, 0.30000001f, 1.4f, -0.2f,
	0.30000001f, -0.1f, 0.1f, -0.80000001f, -0.80000001f, -0.30000001f, 0.40000001f, 0.2f,
	-0.1f, 0.1f, 0.0f, -0.5f, 0.2f, 0.30000001f, 0.5f, -0.30000001f,
	0.40000001f, 0.30000001f, 0.1f, 0.2f, -0.1f, -0.5f, 0.40000001f, 0.2f,
	0.40000001f,
};

static const int s_monthDays[12] = {
	0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

/* 0x81198C94 */
int wmmExtrapolate(WmmContext *ctx, int mainDegree, int secularDegree, float date, float baseEpoch)
{
	float factor = date - baseEpoch;
	int mainCount = mainDegree * (mainDegree + 2);
	int secularCount = secularDegree * (secularDegree + 2);
	int sharedCount;
	int nmax;

	if (mainDegree == secularDegree) {
		sharedCount = mainCount;
		nmax = mainDegree;
	} else if (mainDegree < secularDegree) {
		sharedCount = mainCount;
		nmax = secularDegree;
		for (int i = mainCount + 1; i <= secularCount; i++)
			ctx->gh[i] = factor * s_wmmSecular[i];
	} else {
		sharedCount = secularCount;
		nmax = mainDegree;
		for (int i = secularCount + 1; i <= mainCount; i++)
			ctx->gh[i] = s_wmmMain[i];
	}

	for (int i = 1; i <= sharedCount; i++)
		ctx->gh[i] = s_wmmMain[i] + factor * s_wmmSecular[i];
	return nmax;
}

/* 0x81198D54 */
void wmmSynthField(const WmmContext *ctx, float *outX, float *outY, float *outZ,
		   float lat, float lon, float alt)
{
	float clatDeg = lat;
	if (90.0f - lat < 0.001f)
		clatDeg = 89.999f;
	else if (lat + 90.0f < 0.001f)
		clatDeg = -89.999f;

	float latRad = clatDeg * 0.01745329f;
	float slat = sce_paf_sinf(latRad);
	float clat = sce_paf_cosf(latRad);
	float lonRad = lon * 0.01745329f;
	float slon = sce_paf_sinf(lonRad);
	float clon = sce_paf_cosf(lonRad);

	float sl[14], cl[14];
	float p[120], q[120];
	sl[0] = slon;
	cl[0] = clon;

	float equatorialSq = 40680632.0f;
	float polarSq = 40408300.0f;
	float aa = equatorialSq * clat * clat;
	float bb = polarSq * slat * slat;
	float cc = aa + bb;
	float dd = __builtin_sqrtf(cc);
	float r = __builtin_sqrtf(alt * (alt + 2.0f * dd) + (equatorialSq * aa + polarSq * bb) / cc);
	float cd = (alt + dd) / r;
	float sd = (equatorialSq - polarSq) / dd * slat * clat / r;
	float ct = slat * cd - clat * sd;
	float st = clat * cd + slat * sd;

	p[0] = 2.0f * ct;
	p[1] = 2.0f * st;
	p[2] = 4.5f * ct * ct - 1.5f;
	p[3] = 3.0f * 1.7320508f * st * ct;
	q[0] = -st;
	q[1] = ct;
	q[2] = -3.0f * st * ct;
	q[3] = 1.7320508f * (ct * ct - st * st);

	int nmax = ctx->nmax;
	int npq = (nmax * (nmax + 3)) / 2;
	float x = 0.0f, y = 0.0f, z = 0.0f;
	int n = 0, m = 1, l = 1;
	float rr = 0.0f;
	float fn = 0.0f;
	float ratio = 6371.2f / r;

	for (int k = 1; k <= npq; k++) {
		if (n < m) {
			m = 0;
			n++;
			rr = sce_paf_powf(ratio, (float)(n + 2));
			fn = (float)n;
		}
		float fm = (float)m;
		if (k >= 5) {
			if (m == n) {
				float one = __builtin_sqrtf(1.0f - 0.5f / fm);
				int j = k - n - 1;
				p[k - 1] = (1.0f + 1.0f / fm) * one * st * p[j - 1];
				q[k - 1] = one * (st * q[j - 1] + ct / fm * p[j - 1]);
				cl[m - 1] = cl[m - 2] * cl[0] - sl[m - 2] * sl[0];
				sl[m - 1] = sl[m - 2] * cl[0] + cl[m - 2] * sl[0];
			} else {
				float gmm = fn * fn - fm * fm;
				float normDenom = __builtin_sqrtf(gmm);
				float prevCoeff =
					__builtin_sqrtf((fn - 1.0f) * (fn - 1.0f) - fm * fm) / normDenom;
				float curCoeff = (2.0f * fn - 1.0f) / normDenom;
				int i = k - n;
				int j = k - 2 * n + 1;
				p[k - 1] = (fn + 1.0f) * (curCoeff * ct / fn * p[i - 1] -
							  prevCoeff / (fn - 1.0f) * p[j - 1]);
				q[k - 1] = curCoeff * (ct * q[i - 1] - st / fn * p[i - 1]) -
					   prevCoeff * q[j - 1];
			}
		}

		float coeffTerm = rr * ctx->gh[l];
		if (m == 0) {
			l++;
		} else {
			float secTerm = rr * ctx->gh[l + 1];
			float azimuthTerm = coeffTerm * sl[m - 1] - secTerm * cl[m - 1];
			if (st > 0.0f)
				y += azimuthTerm * fm * p[k - 1] / ((fn + 1.0f) * st);
			else
				y += azimuthTerm * q[k - 1] * ct;
			coeffTerm = coeffTerm * cl[m - 1] + secTerm * sl[m - 1];
			l += 2;
		}
		z -= coeffTerm * p[k - 1];
		x += coeffTerm * q[k - 1];
		m++;
	}

	*outX = x * cd + z * sd;
	*outY = y;
	*outZ = z * cd - x * sd;
}

/* 0x8119924C */
void wmmSetDate(WmmContext *ctx, int month, int day, int year)
{
	int days[12];
	sce_paf_memcpy(days, s_monthDays, sizeof(days));
	int leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 1 : 0;
	int extra = (month > 2) ? leap : 0;
	int doy = day + days[month - 1] + extra;
	ctx->decYear = (float)((double)year + (double)doy / (365.0 + (double)leap));
	ctx->nmax = wmmExtrapolate(ctx, 13, 8, ctx->decYear, 2010.0f);
}

/* 0x8119934C */
void wmmCalcDeclination(WmmContext *ctx, float lat, float lon, float alt)
{
	float x, y, z;
	wmmSynthField(ctx, &x, &y, &z, lat, lon, alt);
	float horizSq = x * x + y * y;
	float f = __builtin_sqrtf(horizSq + z * z);
	float h = __builtin_sqrtf(horizSq);
	float decl;
	if (f < 0.0001f || h < 0.0001f) {
		decl = 0.0f;
	} else if (h + x < 0.0001f) {
		decl = 3.1415927f;
	} else {
		decl = 2.0f * sce_paf_atan2f(y, h + x);
	}
	ctx->declination = decl * 57.295776f;
}
