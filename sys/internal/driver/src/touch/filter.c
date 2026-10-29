/* SceTouch: the force filter, which evens out the force the panels report.
 * Private names without a recovered SDK name remain unknown.
 */
#include "private.h"

/* force filter coefficients, per panel */
struct touchForceGain {
	float gain, gainAlt;
	unsigned maxForce, minForce;
};
struct touchForceSpread {
	float scaleNear, scaleNearAlt, slope, slopeAlt;
	unsigned short nearDistance, farDistance;
	unsigned short lowForce, highForce;
};
struct touchForceRange {
	unsigned short low, high;
	float scale;
};
struct touchForceCurve {
	float scale;
	unsigned short low, high;
};
struct touchFilter {
	unsigned enable;
	struct touchForceCurve curve[2];	/* set at init, never read */
	struct touchForceGain gain[2];
	struct touchForceSpread spread[2];
	struct touchForceRange range[2][3];
};

_Static_assert(sizeof(struct touchFilter) == 0x94, "filter layout");

static struct touchFilter s_filter;

static const float s_forceMax = 255.0f;

/* name unknown: the force filter's coefficients */
void initializeFilter(void)
{
	s_filter.enable = 1;
	s_filter.curve[0].scale = 0.08f;
	s_filter.curve[0].low = 0x3c;
	s_filter.curve[0].high = 0xa0;
	s_filter.curve[1].scale = 0.05f;
	s_filter.curve[1].low = 0x3c;
	s_filter.curve[1].high = 0xaa;
	s_filter.gain[0] = (struct touchForceGain){ 0.6f, 0.18f, 0xff, 0 };
	s_filter.gain[1] = (struct touchForceGain){ 0.6f, 0.25f, 0xff, 0 };
	s_filter.spread[0] = (struct touchForceSpread){ 0.45f, 0.1f, 0.45f / 280, 0.1f / 280,
							 70, 350, 52, 170 };
	s_filter.spread[1] = (struct touchForceSpread){ 0.45f, 0.15f, 0.45f / 190, 0.15f / 190,
							 40, 230, 40, 120 };
	s_filter.range[0][0] = (struct touchForceRange){ 0x40, 0xc0, 0.85f };
	s_filter.range[0][1] = (struct touchForceRange){ 0x40, 0xc0, 0.8f };
	s_filter.range[0][2] = (struct touchForceRange){ 0x40, 0xc0, 0.76f };
	s_filter.range[1][0] = (struct touchForceRange){ 0x38, 0xb8, 0.9f };
	s_filter.range[1][1] = (struct touchForceRange){ 0x38, 0xb8, 0.9f };
	s_filter.range[1][2] = (struct touchForceRange){ 0x40, 0xc0, 0.8f };
}

/*
 * name unknown: raise each touch's force by the pressure spread from the
 * other touches on the panel, then apply the panel's gain curve.
 */
void forceSpread(unsigned port, SceTouchData *data, unsigned flags)
{
	const struct touchForceSpread *spread = &s_filter.spread[port];
	const struct touchForceGain *gain = &s_filter.gain[port];
	float boost[SCE_TOUCH_MAX_REPORT], scaleNear, slope, g, range, w, out, fs, fu;
	unsigned n = data->reportNum, i, j, d, f, sum, high;
	SceTouchReport *report;
	int diff;

	if (!s_filter.enable)
		return;
	if (n < 2) {
		if (n == 1) {
			f = data->report[0].force;
			g = flags & 1 ? gain->gainAlt : gain->gain;
			range = (float)(gain->maxForce - gain->minForce);
			out = (float)f + (float)(f - gain->minForce) / range * g * (float)(int)f;
			data->report[0].force = out > s_forceMax ? 255 : (unsigned char)(unsigned)out;
		}
		return;
	}

	scaleNear = flags & 1 ? spread->scaleNearAlt : spread->scaleNear;
	slope = flags & 1 ? spread->slopeAlt : spread->slope;
	for (i = 0; i < n; i++) {
		boost[i] = 0.0f;
		for (j = 0; j < n; j++) {
			if (i == j)
				continue;
			diff = data->report[i].y - data->report[j].y;
			if (diff < 0)
				diff = -diff;
			d = (unsigned short)diff;
			if (d < spread->nearDistance)
				w = scaleNear;
			else if (d < spread->farDistance)
				w = (float)(int)(spread->farDistance - diff) * slope;
			else
				continue;
			if (!(w > 0.0f))
				continue;
			f = data->report[j].force;
			if (f <= spread->lowForce)
				continue;
			high = spread->highForce;
			w *= (float)(f <= high ? f - spread->lowForce : high - spread->lowForce) /
			     (float)(int)(high - spread->lowForce);
			if (w > boost[i])
				boost[i] = w;
		}
	}

	g = flags & 1 ? gain->gainAlt : gain->gain;
	range = (float)(gain->maxForce - gain->minForce);
	for (i = 0; i < n; i++) {
		report = &data->report[i];
		if (boost[i] > 0.0f) {
			sum = report->force + (unsigned)(boost[i] * (float)(int)report->force);
			if (sum <= 255) {
				report->force = sum;
				fs = (float)(int)sum;
				fu = (float)sum;
			} else {
				report->force = 255;
				sum = 255;
				fu = s_forceMax;
				fs = fu;
			}
		} else {
			sum = report->force;
			fs = (float)(int)sum;
			fu = (float)sum;
		}
		out = fu + (float)(sum - gain->minForce) / range * g * fs;
		report->force = out > s_forceMax ? 255 : (unsigned char)(unsigned)out;
	}
}

/* name unknown: scale each touch's force into the panel's reporting range */
void forceRange(unsigned port, SceTouchData *data, unsigned flags)
{
	const struct touchForceRange *range;
	unsigned n, i, f;
	unsigned char low, high;

	if (!s_filter.enable)
		return;
	if (flags & 2)
		range = &s_filter.range[port][2];
	else if (flags & 1)
		range = &s_filter.range[port][1];
	else
		range = &s_filter.range[port][0];
	n = data->reportNum;
	if (!n)
		return;
	low = range->low;
	high = range->high;
	for (i = 0; i < n; i++) {
		f = (unsigned char)(unsigned)((float)(int)data->report[i].force * range->scale);
		if (f >= high)
			f = high;
		data->report[i].force = low < f ? (unsigned char)(f - low) : 1;
	}
}

void setFilterEnable(unsigned enable)
{
	s_filter.enable = enable;
}
