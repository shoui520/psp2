/*
 * SceDriverUser: SceMotion integer processing of DualShock 3 sensor samples.
 *
 * Each 10-bit axis goes through a linear calibration from the controller's
 * calibration record and, optionally, a second-order low-pass filter. The
 * gyro axis additionally tracks its rest offset: once the signal has been
 * quiet long enough, the offset is moved in whole steps and the caller is
 * told so. Names of the tracker's parts are descriptive; Sony's are unknown.
 */

#include "driver_user.h"
#include "motion.h"

#define DS3_AXIS_MAX	1023
#define DS3_CENTER	512

/* kept out of line, as in Sony's build */
#define DS3_FUNC	__attribute__((noinline, noclone))

int __aeabi_idiv(int n, int d);

static inline int Usat10(int value)
{
	if (value < 0)
		return 0;
	if (value > DS3_AXIS_MAX)
		return DS3_AXIS_MAX;
	return value;
}

/* low-pass coefficient sets {a1, a2, b0, b1, b2}, Q14 feedback / Q18 output */
static const Ds3LowPassCoefficients s_ds3LowPass1 = { 18727, -6763, 1105, 2615, 1105, 0 };
static const Ds3LowPassCoefficients s_ds3LowPass0 = { 0, -2811, 4799, 9598, 4799, 0 };
static const Ds3LowPassCoefficients s_ds3LowPass2 = { 25576, -10508, 329, 658, 329, 0 };

/* --- window statistics: mean, minimum and maximum of every n samples --- */

static DS3_FUNC void Ds3WindowInit(Ds3Window *window, int length)
{
	window->min = 0x7fffffff;
	window->max = (int)0x80000000;
	window->length = length;
	window->windowMean = 0;
	window->windowMin = 0;
	window->windowMax = 0;
	window->count = 0;
	window->sum = 0;
}

static DS3_FUNC void Ds3WindowReset(Ds3Window *window)
{
	window->count = 0;
	window->sum = 0;
	window->min = 0x7fffffff;
	window->max = (int)0x80000000;
}

static DS3_FUNC int Ds3WindowAdd(Ds3Window *window, int value)
{
	int sum = window->sum;
	int min = window->min;
	int count;

	if (value > window->max)
		window->max = value;
	window->sum = sum + value;
	if (value < min) {
		window->min = value;
		min = value;
	}
	count = window->count + 1;
	window->count = count;
	if (count != window->length)
		return 0;
	window->windowMean = __aeabi_idiv(sum + value + (count >> 1), count);
	window->count = 0;
	window->sum = 0;
	window->windowMin = min;
	window->windowMax = window->max;
	window->min = 0x7fffffff;
	window->max = (int)0x80000000;
	return 1;
}

/* squared distances of the window's extremes from its mean */
static DS3_FUNC int Ds3WindowSpread(const Ds3Window *window)
{
	int below = window->windowMean - window->windowMin;
	int above = window->windowMax - window->windowMean;

	return above * above + below * below;
}

/* --- ring of recent window means: running sum and range --- */

static DS3_FUNC void Ds3RingInit(Ds3Ring *ring, int *buffer, int size)
{
	ring->buffer = buffer;
	ring->size = size;
	ring->count = 0;
	ring->index = 0;
	ring->sum = 0;
}

static DS3_FUNC void Ds3RingReset(Ds3Ring *ring)
{
	ring->count = 0;
	ring->index = 0;
	ring->sum = 0;
}

static DS3_FUNC int Ds3RingAdd(Ds3Ring *ring, int value)
{
	int *buffer = ring->buffer;
	int index = ring->index;
	int oldest = buffer[index];
	int size, max, min, i;

	ring->index = index + 1;
	buffer[index] = value;
	size = ring->size;
	ring->sum += value;
	if (ring->count < size) {
		ring->count++;
		if (ring->count < size)
			return 0;
	} else {
		ring->sum -= oldest;
	}
	if (ring->index == size)
		ring->index = 0;
	max = (int)0x80000000;
	min = 0x7fffffff;
	for (i = 0; i < size; i++) {
		if (buffer[i] > max)
			max = buffer[i];
		if (buffer[i] < min)
			min = buffer[i];
	}
	ring->range = (unsigned int)max - (unsigned int)min;
	return 1;
}

static DS3_FUNC int Ds3RingMean(const Ds3Ring *ring)
{
	return __aeabi_idiv(ring->sum + (ring->size >> 1), ring->size);
}

/* --- plain average of every n values --- */

static DS3_FUNC void Ds3AverageInit(Ds3Average *average, int length)
{
	average->length = length;
	average->mean = 0;
	average->count = 0;
	average->sum = 0;
}

static DS3_FUNC void Ds3AverageReset(Ds3Average *average)
{
	average->count = 0;
	average->sum = 0;
}

static DS3_FUNC int Ds3AverageAdd(Ds3Average *average, int value)
{
	int sum = average->sum;
	int count = average->count + 1;

	average->count = count;
	average->sum = sum + value;
	if (count != average->length)
		return 0;
	average->mean = __aeabi_idiv(sum + value + (count >> 1), count);
	average->count = 0;
	average->sum = 0;
	return 1;
}

/* --- gyro rest-offset tracker --- */

/* offset steps between the center and a new bias, scaled by 1024 */
static inline int Ds3StepsOff(const Ds3Tracker *tracker, int bias)
{
	return (tracker->center - bias) * 1024;
}

static DS3_FUNC void Ds3TrackerInit(Ds3Tracker *tracker, const Ds3TrackerConfig *config, int *offset)
{
	int bias = config->bias;
	int scaled, steps, adjust = 0;

	tracker->holdReload = config->holdReload;
	tracker->holdCount = config->holdCount;
	tracker->initialBias = config->bias;
	tracker->center = config->center;
	tracker->state.offset = config->offset;
	tracker->stepScale = config->stepScale;
	tracker->rangeMax = config->rangeMax;
	tracker->rangeMin = config->rangeMin;
	tracker->initialOffset = config->offset;
	tracker->enabled = 1;
	tracker->outOfRange = 0;
	Ds3WindowInit(&tracker->window, config->windowLength);
	tracker->spreadLimit = config->spreadLimit;
	Ds3RingInit(&tracker->ring, tracker->ringBuffer, config->ringSize);
	tracker->rangeLimit = config->rangeLimit;
	Ds3AverageInit(&tracker->average, config->averageLength);
	tracker->countdownStep = __aeabi_idiv(config->countdown, config->countdownSteps);
	tracker->countdown = config->countdown;
	tracker->lastInput = bias;
	tracker->isPending = 0;
	scaled = Ds3StepsOff(tracker, bias);
	if (scaled < -tracker->stepScale || scaled > tracker->stepScale) {
		steps = __aeabi_idiv(scaled, tracker->stepScale);
		tracker->state.offset += steps;
		adjust = steps * tracker->stepScale / 1024;
		bias += adjust;
	}
	tracker->state.bias = bias;
	tracker->output = Usat10(tracker->center + adjust);
	*offset = tracker->state.offset;
}

static DS3_FUNC int Ds3TrackerUpdate(Ds3Tracker *tracker, int input, int *offset)
{
	int scaled, steps, mean, changed;

	if (input > tracker->rangeMax || input < tracker->rangeMin)
		tracker->outOfRange = 1;
	if (!Ds3WindowAdd(&tracker->window, input))
		return 0;
	tracker->windowMean = tracker->window.windowMean;
	tracker->spread = Ds3WindowSpread(&tracker->window);
	if (Ds3AverageAdd(&tracker->average, tracker->windowMean)) {
		mean = tracker->average.mean;
		tracker->averageMean = mean;
		scaled = Ds3StepsOff(tracker, mean);
		if (scaled < -tracker->stepScale || scaled > tracker->stepScale) {
			steps = __aeabi_idiv(scaled, tracker->stepScale);
			tracker->isPending = 1;
			tracker->pendingCountdown = tracker->countdown;
			tracker->pending.offset = steps + tracker->state.offset;
			tracker->pending.bias = mean + steps * tracker->stepScale / 1024;
		} else {
			tracker->pending.bias = mean;
			tracker->state.bias = mean;
		}
	}
	if (tracker->outOfRange) {
		tracker->outOfRange = 0;
		return 0;
	}
	if (tracker->spread >= tracker->spreadLimit)
		return 0;
	if (Ds3RingAdd(&tracker->ring, tracker->windowMean) &&
	    (tracker->ringRange = tracker->ring.range) < tracker->rangeLimit) {
		mean = Ds3RingMean(&tracker->ring);
		tracker->ringMean = mean;
		Ds3AverageReset(&tracker->average);
		scaled = Ds3StepsOff(tracker, tracker->ringMean);
		if (scaled < -tracker->stepScale || scaled > tracker->stepScale) {
			changed = 1;
			steps = __aeabi_idiv(scaled, tracker->stepScale);
			tracker->state.offset += steps;
			tracker->state.bias = mean + steps * tracker->stepScale / 1024;
			Ds3RingReset(&tracker->ring);
			tracker->holdCount = tracker->holdReload;
		} else {
			tracker->state.bias = mean;
			changed = 0;
		}
		tracker->isPending = 0;
		*offset = tracker->state.offset;
		return changed;
	}
	if (!tracker->isPending)
		return 0;
	tracker->state = tracker->pending;
	tracker->isPending = 0;
	Ds3RingReset(&tracker->ring);
	Ds3AverageReset(&tracker->average);
	*offset = tracker->state.offset;
	tracker->holdCount = tracker->holdReload;
	return 1;
}

static DS3_FUNC int Ds3TrackerProcess(Ds3Tracker *tracker, int input, int *offset, SceUInt8 *changed)
{
	int output, delta;

	if (tracker->holdCount != 0) {
		tracker->holdCount--;
		tracker->lastInput = input;
		*changed = 0;
		return tracker->output;
	}
	output = Usat10(tracker->center + (tracker->state.bias - input));
	tracker->output = output;
	if (tracker->enabled != 1) {
		tracker->lastInput = input;
		*changed = 0;
		return output;
	}
	*changed = Ds3TrackerUpdate(tracker, input, offset);
	if (tracker->isPending) {
		delta = input - tracker->lastInput;
		if (delta > tracker->pendingCountdown || delta < -tracker->pendingCountdown) {
			tracker->state = tracker->pending;
			tracker->isPending = 0;
			Ds3WindowReset(&tracker->window);
			Ds3RingReset(&tracker->ring);
			Ds3AverageReset(&tracker->average);
			*offset = tracker->state.offset;
			*changed = 1;
		} else {
			tracker->pendingCountdown -= tracker->countdownStep;
		}
	}
	tracker->lastInput = input;
	return tracker->output;
}

/* --- per-axis calibration and filter --- */

static DS3_FUNC int Ds3CalibrationInit(Ds3Calibration *calibration)
{
	calibration->rawSpan = 1;
	calibration->max = DS3_AXIS_MAX;
	calibration->outSpan = 0;
	calibration->outRef = 0;
	calibration->reserve0 = 0;
	calibration->reserve1 = 0;
	calibration->min = 0;
	return 0;
}

/* a three-point record spans points 0..2, a two-point one points 0..1; point 1 is the reference */
static DS3_FUNC void Ds3CalibrationFromPoints(Ds3Calibration *calibration, int type, const int *point)
{
	if (type == 2) {
		calibration->rawSpan = point[2] - point[0];
		calibration->outSpan = point[3] - point[1];
	} else {
		calibration->rawSpan = point[4] - point[0];
		calibration->outSpan = point[5] - point[1];
	}
	calibration->outRef = point[3];
	calibration->rawRef = point[2];
}

static DS3_FUNC void Ds3CalibrationFromSpan(Ds3Calibration *calibration, const int *reference, int rawSpan,
				   int outSpan)
{
	int rawRef = reference[0];

	calibration->rawSpan = rawSpan;
	calibration->outSpan = outSpan;
	calibration->outRef = reference[1];
	calibration->rawRef = rawRef;
}

static DS3_FUNC int Ds3CalibrationApply(const Ds3Calibration *calibration, int raw)
{
	int value = __aeabi_idiv(calibration->outSpan * (raw - calibration->rawRef) << 10,
				 calibration->rawSpan);

	value = calibration->outRef + (value >> 10);
	if (value < calibration->min)
		return calibration->min;
	if (value >= calibration->max)
		value = calibration->max;
	return value;
}

static DS3_FUNC void Ds3LowPassReset(Ds3LowPass *filter, int center)
{
	filter->center = center;
	filter->reserve = 0;
	filter->w1 = 0;
	filter->w2 = 0;
}

static DS3_FUNC int Ds3LowPassApply(Ds3LowPass *filter, int input)
{
	int w2 = filter->w2;
	int w1 = filter->w1;
	int w0;

	filter->w2 = w1;
	w0 = (filter->a1 * w1 + filter->a2 * w2 + ((input - filter->center) << 18) + 0x2000) >> 14;
	filter->w1 = w0;
	return Usat10(filter->center +
		      ((filter->b2 * w2 + filter->b0 * w0 + filter->b1 * w1 + 0x2000) >> 18));
}

static DS3_FUNC int Ds3LowPassSelect(Ds3LowPassHeader *filter, int rate, int index)
{
	const Ds3LowPassCoefficients *coefficients;

	g_motion.ds3LowPass[0] = &s_ds3LowPass0;
	g_motion.ds3LowPass[1] = &s_ds3LowPass1;
	g_motion.ds3LowPass[2] = &s_ds3LowPass2;
	if (rate > 0 || index > 2)
		return -1;
	coefficients = g_motion.ds3LowPass[index];
	filter->filter.a1 = coefficients->a1;
	filter->filter.a2 = coefficients->a2;
	filter->filter.b0 = coefficients->b0;
	filter->filter.b1 = coefficients->b1;
	filter->filter.b2 = coefficients->b2;
	return 0;
}

static DS3_FUNC void Ds3AxisInit(Ds3Axis *axis)
{
	axis->pOutput = &axis->output;
	axis->pLowPass = &axis->lowPass;
	axis->filterMode = 0;
	Ds3CalibrationInit(&axis->calibration);
}

static DS3_FUNC void Ds3AxisSetCenter(Ds3Axis *axis, int center)
{
	axis->center = center;
	Ds3LowPassReset(&axis->lowPass.filter, center);
}

static DS3_FUNC void Ds3AxisCalibrate(Ds3Axis *axis, const Ds3AxisRecord *record)
{
	Ds3CalibrationFromPoints(&axis->calibration, record->type, record->point);
	Ds3AxisSetCenter(axis, record->point[2]);
}

static DS3_FUNC void Ds3AxisCalibrateSpan(Ds3Axis *axis, const Ds3SpanRecord *record)
{
	Ds3CalibrationFromSpan(&axis->calibration, &record->rawRef, record->rawSpan, record->outSpan);
	Ds3AxisSetCenter(axis, record->rawRef);
}

static DS3_FUNC int Ds3AxisApply(Ds3Axis *axis, int raw)
{
	int value = Ds3CalibrationApply(&axis->calibration, raw);

	if (axis->filterMode == 1)
		value = Ds3LowPassApply(&axis->lowPass.filter, value);
	return value;
}

static DS3_FUNC int Ds3AxisSetFilter(Ds3Axis *axis, int mode, int rate, int index)
{
	if (mode > 1)
		return -1;
	axis->filterMode = mode;
	if (mode != 1)
		return 0;
	return Ds3LowPassSelect(&axis->lowPass, rate, index);
}

/* --- the four axes: three accelerometer axes and the gyro --- */

static DS3_FUNC int Ds3AxesInit(Ds3Axes *axes)
{
	axes->axis[0] = &axes->storage[0];
	axes->axis[1] = &axes->storage[1];
	axes->axis[2] = &axes->storage[2];
	axes->axis[3] = &axes->storage[3];
	Ds3AxisInit(&axes->storage[0]);
	Ds3AxisInit(&axes->storage[1]);
	Ds3AxisInit(&axes->storage[2]);
	Ds3AxisInit(&axes->storage[3]);
	return 0;
}

static DS3_FUNC int Ds3AxesCalibrate(Ds3Axes *axes, const Ds3CalibrationRecords *records)
{
	Ds3AxisCalibrate(&axes->storage[0], &records->accel[0]);
	Ds3AxisCalibrate(&axes->storage[1], &records->accel[1]);
	Ds3AxisCalibrate(&axes->storage[2], &records->accel[2]);
	Ds3AxisCalibrateSpan(&axes->storage[3], &records->gyro);
	return 0;
}

static DS3_FUNC int Ds3AxesApply(Ds3Axes *axes, const int *raw, int *out)
{
	out[0] = Ds3AxisApply(&axes->storage[0], raw[0]);
	out[1] = Ds3AxisApply(&axes->storage[1], raw[1]);
	out[2] = Ds3AxisApply(&axes->storage[2], raw[2]);
	out[3] = Ds3AxisApply(&axes->storage[3], raw[3]);
	return 0;
}

static DS3_FUNC void Ds3AxesSetFilter(Ds3Axes *axes, int index, int mode, int rate, int coefficients)
{
	Ds3AxisSetFilter(axes->axis[index], mode, rate, coefficients);
}

/* --- per-controller processing --- */

int Ds3SensorInit(Ds3Sensor *sensor, const SceUInt16 *calibration, int *offset)
{
	Ds3TrackerConfig config;
	Ds3CalibrationRecords records;

	if (offset == NULL || calibration == NULL)
		return -1;
	config.center = DS3_CENTER;
	config.offset = calibration[9];
	config.bias = calibration[8];
	config.stepScale = 0x6999;
	config.holdCount = 32;
	config.holdReload = 2;
	config.rangeMax = 819;
	config.rangeMin = 204;
	config.reserve0 = 4;
	config.windowLength = 16;
	config.spreadLimit = 10;
	config.ringSize = 4;
	config.rangeLimit = 4;
	config.averageLength = 236;
	config.reserve1 = 0;
	config.countdownSteps = 100;
	config.countdown = 100;
	sensor->trackerEnabled = 1;
	Ds3TrackerInit(&sensor->tracker, &config, offset);

	records.accel[0] = (Ds3AxisRecord){ 3, { calibration[1], 625, calibration[0], DS3_CENTER,
						 calibration[2], 399 } };
	records.accel[1] = (Ds3AxisRecord){ 3, { calibration[4], 399, calibration[3], DS3_CENTER,
						 calibration[5], 625 } };
	records.accel[2].type = 2;
	records.accel[2].point[0] = calibration[7];
	records.accel[2].point[1] = 399;
	records.accel[2].point[2] = calibration[6];
	records.accel[2].point[3] = DS3_CENTER;
	records.gyro = (Ds3SpanRecord){ DS3_CENTER, DS3_CENTER, 1, 1 };
	Ds3AxesInit(&sensor->axes);
	Ds3AxesCalibrate(&sensor->axes, &records);
	Ds3AxesSetFilter(&sensor->axes, 0, 0, 0, 0);
	Ds3AxesSetFilter(&sensor->axes, 1, 0, 0, 0);
	Ds3AxesSetFilter(&sensor->axes, 2, 0, 0, 0);
	Ds3AxesSetFilter(&sensor->axes, 3, 0, 0, 0);
	sensor->initialized = 1;
	return 0;
}

int Ds3SensorProcess(Ds3Sensor *sensor, SceUInt16 *sample, int *offset)
{
	int raw[4], out[4];
	SceUInt8 changed = 0;

	if (offset == NULL || sample == NULL)
		return -1;
	if (!sensor->initialized)
		return -2;
	if (!sensor->trackerEnabled)
		sample[3] = Usat10(sensor->gyroCenter - sample[3] + DS3_CENTER);
	else
		sample[3] = Ds3TrackerProcess(&sensor->tracker, sample[3], offset, &changed);
	raw[0] = sample[0];
	raw[1] = sample[1];
	raw[2] = sample[2];
	raw[3] = sample[3];
	Ds3AxesApply(&sensor->axes, raw, out);
	sample[0] = out[0];
	sample[1] = out[1];
	sample[2] = out[2];
	sample[3] = out[3];
	return changed;
}
