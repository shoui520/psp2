/*
 * Gyro bias tracking for the virtual controllers: while a controller lies
 * still, the average of a window of samples is its zero-rate output.
 */
#include <string.h>
#include <motion/motion_dev.h>

#define FILTER_STRICT	0x1	/* accept one quiet window */
#define FILTER_RELAXED	0x2	/* accept four of six fairly quiet windows */

#define FILTER_WINDOWS		6
#define FILTER_WINDOWS_NEEDED	4

static inline void zeroFill(void *buffer, SceSize size)
{
	uint8_t *p = buffer;
	SceSize i;

	for (i = 0; i < size; i++)
		p[i] = 0;
}

static inline float absolute(float value)
{
	return value <= 0.0f ? -value : value;
}

static inline float clamp(float value, float limit)
{
	if (value <= limit) {
		limit = -limit;
		if (value >= limit)
			limit = value;
	}
	return limit;
}

static inline void clearWindows(GyroFilter *filter)
{
	zeroFill(filter->accumulated, sizeof(filter->accumulated));
	filter->acceptedWindows = 0;
	filter->windows = 0;
}

static inline void clearSamples(GyroFilter *filter)
{
	filter->samples = 0;
	zeroFill(filter->maximum, sizeof(filter->maximum));
	zeroFill(filter->minimum, sizeof(filter->minimum));
	zeroFill(filter->sum, sizeof(filter->sum));
}

void clearGyroFilter(GyroFilter *filter)
{
	memset(filter, 0, sizeof(*filter));
}

void initGyroFilter(GyroFilter *filter, const SceMotionDevGyroCalibData *calibration,
		    int virtual)
{
	float gain, strictGain;
	int i;

	if (!(calibration->positive[0].reference > 0.0f &&
	      calibration->positive[1].reference > 0.0f &&
	      calibration->positive[2].reference > 0.0f &&
	      calibration->negative[0].reference < 0.0f &&
	      calibration->negative[1].reference < 0.0f &&
	      calibration->negative[2].reference < 0.0f))
		return;
	for (i = 0; i < 3; i++)
		filter->bias[i] = calibration->zeroRate[i];
	filter->flags = FILTER_STRICT | FILTER_RELAXED;
	for (i = 0; i < 3; i++)
		filter->scale[i] = absolute((calibration->positive[i].value[i] -
					     calibration->zeroRate[i]) /
					    calibration->positive[i].reference);
	for (i = 0; i < 3; i++) {
		filter->longClamp[i] = filter->scale[i] * 6.0f;
		filter->shortClamp[i] = filter->scale[i] * 0.5f;
	}
	filter->windowSize = virtual ? 45 : 250;
	if (SceMotionDevForDriver_77BA3A04() || motionUseWideGyroFilter()) {
		gain = 3.0f;
		strictGain = 1.0f;
	} else {
		gain = 3.2f;
		strictGain = 1.5f;
	}
	filter->gain = gain;
	filter->strictGain = strictGain;
	for (i = 0; i < 3; i++) {
		float strict = strictGain * filter->scale[i];
		float relaxed = gain * filter->scale[i];

		filter->strictLimit[i] = strict * strict + strict * strict;
		filter->relaxedLimit[i] = relaxed * relaxed + relaxed * relaxed;
	}
	filter->enabled = 1;
}

void resetGyroFilter(GyroFilter *filter)
{
	clearWindows(filter);
	clearSamples(filter);
}

/* the new bias, and how far it moved */
static inline void setBias(GyroFilter *filter, const float *bias, float *delta)
{
	int i;

	for (i = 0; i < 3; i++) {
		delta[i] = bias[i] >= filter->bias[i] ? bias[i] - filter->bias[i] :
							 filter->bias[i] - bias[i];
		if (bias[i] < filter->bias[i])
			delta[i] = -delta[i];
	}
	for (i = 0; i < 3; i++)
		filter->bias[i] = bias[i];
}

int updateGyroFilter(GyroFilter *filter, const uint16_t *gyro, float *bias, float *delta)
{
	float average[3], variation[3];
	unsigned int samples;
	int i, updated;

	if (!filter->enabled)
		return 0;
	samples = ++filter->samples;
	for (i = 0; i < 3; i++)
		filter->sum[i] += gyro[i];
	if (samples == 1) {
		for (i = 0; i < 3; i++) {
			filter->maximum[i] = gyro[i];
			filter->minimum[i] = gyro[i];
		}
	} else {
		for (i = 0; i < 3; i++) {
			if (gyro[i] > filter->maximum[i])
				filter->maximum[i] = gyro[i];
			if (gyro[i] < filter->minimum[i])
				filter->minimum[i] = gyro[i];
		}
	}
	if (filter->windowSize != samples)
		return 0;
	for (i = 0; i < 3; i++) {
		float low, high;

		average[i] = (float)filter->sum[i] / (float)samples;
		low = average[i] - (float)filter->minimum[i];
		high = (float)filter->maximum[i] - average[i];
		variation[i] = low * low + high * high;
	}
	filter->windows++;
	if (variation[0] < filter->strictLimit[0] && variation[1] < filter->strictLimit[1] &&
	    variation[2] < filter->strictLimit[2] && (filter->flags & FILTER_STRICT)) {
		for (i = 0; i < 3; i++)
			bias[i] = filter->bias[i] - clamp(filter->bias[i] - average[i],
							filter->longClamp[i]);
		setBias(filter, bias, delta);
		updated = 1;
	} else {
		if (variation[0] < filter->relaxedLimit[0] &&
		    variation[1] < filter->relaxedLimit[1] &&
		    variation[2] < filter->relaxedLimit[2] && (filter->flags & FILTER_RELAXED)) {
			for (i = 0; i < 3; i++)
				filter->accumulated[i] += filter->sum[i];
			filter->acceptedWindows++;
		}
		if (filter->windows != FILTER_WINDOWS) {
			clearSamples(filter);
			return 0;
		}
		if (filter->acceptedWindows >= FILTER_WINDOWS_NEEDED) {
			unsigned int count = filter->acceptedWindows * filter->windowSize;

			for (i = 0; i < 3; i++)
				bias[i] = filter->bias[i] -
					  clamp(filter->bias[i] -
						(float)filter->accumulated[i] / (float)count,
						filter->shortClamp[i]);
			setBias(filter, bias, delta);
			updated = 1;
		} else {
			updated = 0;
		}
	}
	clearWindows(filter);
	clearSamples(filter);
	return updated;
}
