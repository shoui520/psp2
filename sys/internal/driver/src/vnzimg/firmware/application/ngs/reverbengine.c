/*
 * The reverb engine (0x84be00-0x84d430) and its VLIW kernels (0x8ae418-0x8b25b0).
 *
 * The input of a block is run through a pre-delay ring, early reflection
 * taps and two allpasses, then through a feedback delay network of 16
 * lines (a Householder matrix, a chorus on the first three lines and a
 * one-pole absorption per line), and the wet outputs of the lines are summed
 * to each path with +-1 signs, filtered and added to the early reflections.
 * Run adds the wet output of the last block to the dry signal; the engine
 * (the RPC 327 entry) makes the wet output of the block Run stored.
 *
 * All products are 8.24: ngsMultiply(a, b) = (a b) >> 24 on 64 bits.
 */
#include "reverb.h"
#include "../ivc2.h"

#define PATH_GAIN_SCALE		0x1699c10	/* +3 dB */
#define SQRT_2			0x16a09e6
#define TWO_PI			0x6487ed8
#define DIFFUSION_SCALE		0x147ae		/* 0.005 */
#define SHORT_DECAY		0x2947ae0	/* decay times below this get the short decay tilt */
#define SHORT_DECAY_RANGE	0x27ae148
#define DECAY_FLOOR		0x19999a	/* 0.1 */
#define SHORT_DECAY_TILT	0x33b645
#define SURROUND_TILT		0xb33333	/* 0.7 */
#define MAX_DAMPING		0xfff194
#define FADE_HALF		(NGS_WINDOW_SIZE / 2)
#define REVERB_SAMPLE_RATE	48000
#define REVERB_MAX_PREDELAY	4800
#define HIGH_SHELF_FLOOR	167
#define DC_LEAK			(-1677)		/* y = x - x1 + (1 - 1677 / 2^24) y1 */
#define RING_BIAS		22048		/* ring read positions are (write + 7712 - delay + 7136) % ring */
#define LFO_DELAY_SCALE		0x5ccccd0
#define LFO_DELAY_BASE		0x7000000
#define CHORUS_LINES		3
#define RENORMALISE		0x3000000

/* the line lengths (samples); the second set is for a lower density */
static const int32_t lineLengths[NGS_REVERB_LINES] = {
	2617, 1999, 1801, 3407, 2179, 1597, 4063, 3019,
	4001, 3821, 2411, 3593, 1409, 2797, 1187, 3191,
};

static const int32_t lineLengthsSparse[NGS_REVERB_LINES] = {
	2617, 1999, 1801, 3229, 1910, 1361, 3821, 2347,
	3797, 3779, 2003, 3303, 1237, 2034, 1171, 2551,
};

/* the lengths of each slot's four allpasses */
static const int32_t allpassLengths[8][4] = {
	{ 605, 481, 371, 244 }, { 601, 485, 367, 248 }, { 599, 487, 373, 242 },
	{ 602, 479, 379, 241 }, { 600, 479, 383, 239 }, { 603, 483, 381, 234 },
	{ 602, 485, 377, 237 }, { 598, 481, 375, 247 },
};

/* early reflection taps by pattern: delays (samples) and gains (8.24) */
static const int32_t tapDelays[6][NGS_REVERB_TAPS] = {
	{ 3854, 3861, 3678, 2803, 1990, 1089, 485, 0 },
	{ 3842, 3859, 3688, 2801, 1982, 1093, 484, 2 },
	{ 1, 83, 232, 448, 1309, 2444, 2992, 3568 },
	{ 0, 81, 409, 812, 1356, 1652, 1696, 2401 },
	{ 0, 265, 625, 788, 1073, 1617, 3475, 3808 },
	{ 0, 268, 650, 1073, 1593, 1867, 3617, 3790 },
};

static const int32_t tapGains[6][NGS_REVERB_TAPS] = {
	{ (int32_t)0xffe24168, (int32_t)0xffafb229, (int32_t)0xffa1f089, (int32_t)0xff83e63f,
	  (int32_t)0xffec2b9b, (int32_t)0xffb7dd43, (int32_t)0xffdeeae2, (int32_t)0xff5ae28d },
	{ (int32_t)0xffe2ebc4, (int32_t)0xffbe46ae, (int32_t)0xffb1b7f6, (int32_t)0xff9d8645,
	  (int32_t)0xffecf8f9, (int32_t)0xffa13fd1, (int32_t)0xffa1f602, (int32_t)0xff5da2a5 },
	{ (int32_t)0xffa573eb, (int32_t)0xff3a3d71, (int32_t)0xffc47ae2, (int32_t)0xffcc28f6,
	  (int32_t)0xffbf5c29, (int32_t)0xffc9999a, (int32_t)0xffceb852, (int32_t)0xffceb852 },
	{ (int32_t)0xff3db22d, (int32_t)0xff9374bd, (int32_t)0xffd128e1, (int32_t)0xffcb6ae8,
	  (int32_t)0xffc9374c, (int32_t)0xffd16873, (int32_t)0xffcd4fe0, (int32_t)0xffc6a7f0 },
	{ (int32_t)0xffd388d0, (int32_t)0xff7c4007, (int32_t)0xff99e4d3, (int32_t)0xff8e5da0,
	  (int32_t)0xffada805, (int32_t)0xffb0f338, (int32_t)0xffbe2004, (int32_t)0xffb5e404 },
	{ (int32_t)0xffc60d1c, (int32_t)0xff551eb8, (int32_t)0xff9ff877, (int32_t)0xffbdc5d6,
	  (int32_t)0xffc1158c, (int32_t)0xffb08701, (int32_t)0xffab8f71, (int32_t)0xffc2bd67 },
};

/* the sign with which each line goes to each slot's output */
static const int8_t outputSigns[8][NGS_REVERB_LINES] = {
	{ 1, 1, 1, -1, 0, 0, 0, 0, 1, 1, 1, -1, 1, 1, 1, -1 },
	{ 1, 1, -1, 1, -1, 1, 1, 1, 1, 1, 0, 0, 0, 0, -1, 1 },
	{ 0, 0, 1, 1, 1, 0, 1, 1, 0, 0, 1, 1, -1, 1, 0, 1 },
	{ 1, 1, 1, 0, 1, -1, 1, 0, 1, -1, 1, 0, 1, -1, 1, 0 },
	{ -1, 1, 0, 0, 0, 0, -1, -1, 1, 1, 1, 1, 1, 1, 1, 1 },
	{ 1, -1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, -1, 1, 1, 1, -1, 1, 1, 1, 1, 1, -1, 1 },
	{ 0, 0, 1, 1, 1, 1, 1, -1, 1, -1, 1, 1, -1, 1, 0, 0 },
};

/* the gain with which each slot feeds each line */
static const int32_t inputGains[8][NGS_REVERB_LINES] = {
	{ 0x24feb6, 0x6d2086, 0x60c4b6, 0x19cbae, 0x0, 0x0, 0x0, 0x0,
	  0x68db8, 0xadb6b, 0x51c811, 0x112470, 0x754999, 0x41df17, 0x78241d, 0x3290ee },
	{ 0x24feb6, 0x51c811, 0x6d2086, 0x19cbae, 0x60c4b6, 0x754999, 0xadb6b, 0x41df17,
	  0x68db8, 0x112470, 0x0, 0x0, 0x0, 0x0, 0x78241d, 0x3290ee },
	{ 0x0, 0x0, 0x78241d, 0x754999, 0x6d2086, 0x60c4b6, 0x51c811, 0x41df17,
	  0x0, 0x0, 0x3290ee, 0x24feb6, 0x112470, 0xadb6b, 0x68db8, 0x19cbae },
	{ 0x60c4b6, 0x51c811, 0x41df17, 0x0, 0x112470, 0xadb6b, 0x68db8, 0x0,
	  0x41df17, 0x3290ee, 0x24feb6, 0x0, 0x78241d, 0x754999, 0x6d2086, 0x0 },
	{ 0x60c4b6, 0x51c811, 0x0, 0x0, 0x0, 0x0, 0x24feb6, 0x41df17,
	  0x754999, 0x6d2086, 0x112470, 0x3290ee, 0xadb6b, 0x19cbae, 0x68db8, 0x78241d },
	{ 0xadb6b, 0x41df17, 0x754999, 0x3290ee, 0x60c4b6, 0x78241d, 0x19cbae, 0x112470,
	  0x51c811, 0x24feb6, 0x6d2086, 0x68db8, 0x0, 0x0, 0x0, 0x0 },
	{ 0x0, 0x0, 0x0, 0x0, 0x19cbae, 0x68db8, 0xadb6b, 0x112470,
	  0x3290ee, 0x24feb6, 0x41df17, 0x51c811, 0x60c4b6, 0x6d2086, 0x754999, 0x78241d },
	{ 0x0, 0x0, 0x41df17, 0x6d2086, 0xadb6b, 0x3290ee, 0x68db8, 0x19cbae,
	  0x24feb6, 0x51c811, 0x60c4b6, 0x112470, 0x754999, 0x78241d, 0x0, 0x0 },
};

/* wet output paths by channels in the layout (1 to 9) */
static const int8_t pathCounts[9] = { 1, 2, 3, 4, 5, 6, 6, 7, 8 };

/* how the layout places its paths among the slots of the tables above */
static const int8_t layouts[9] = { 0, 0, 1, 2, 3, 0, 2, 3, 0 };

/* the chorus's LFO: a phasor turned by sn, and its other two phases */
static const int32_t lfoRotation = 0x1000000;
static const int32_t lfoStep = 0x585;
static const int32_t lfoPhase1Cosine = (int32_t)0xff7ffb39;
static const int32_t lfoPhase1Sine = 0xddb115;
static const int32_t lfoPhase2Sine = (int32_t)0xff224967;
static const int32_t lfoPhase2Cosine = (int32_t)0xff8004c8;

/* the slot of a path in the tables for a layout of `channels` */
static int32_t pathSlot(int32_t path, int32_t channels)
{
	uint32_t index = channels - 1;

	if (index < 9) {
		switch (layouts[index]) {
		case 1:
			if (path == 2)
				path = 3;
			break;
		case 2:
			if (path >= 2)
				path += 2;
			break;
		case 3:
			if (path >= 3)
				path += 1;
			break;
		}
	}
	return path;
}

/* 0x8b22f8: a copy that drops the destination's whole 256-byte cache blocks first */
static void ringCopy(void *destination, const void *source, uint32_t size)
{
	uint32_t start = ((uint32_t)destination + 255) & ~255U;
	int32_t length = (int32_t)(size - (start - (uint32_t)destination)) & ~255;

	if (length > 0)
		cacheInvalidateFlush(start, length);
	memcpy(destination, source, size);
}

/* 0x8b1408: samples in 8.24 to 16 bits, in place */
static void packSamples(int32_t *samples, uint32_t count)
{
	int16_t *packed = (int16_t *)samples;
	uint32_t i;

	for (i = 0; i < count; i++)
		packed[i] = ivc2Clip(samples[i] >> 9, 16);
}

/* 0x8ae418 */
void ngsReverbRun(int32_t **inputs, int32_t **outputs, int32_t channels, uint32_t samples,
	NgsReverbEngine *engine)
{
	int32_t channel;
	uint32_t i;

	for (channel = 0; channel < channels; channel++) {
		int32_t dry = engine->path[channel].dry;
		int32_t *wet = engine->buffers[channel];
		int32_t *in = inputs[channel];
		int32_t *out = outputs[channel];

		for (i = 0; i < samples; i++) {
			int32_t sample = in[i];

			out[i] = ngsMultiply(sample, dry) + wet[i];
			wet[i] = sample;
		}
	}
}

/* 0x8ae5e8 */
void ngsReverbBypass(int32_t **inputs, int32_t **outputs, int32_t channels, uint32_t samples,
	NgsReverbEngine *engine, int32_t reset)
{
	int32_t channel, line, block, step, k;
	uint32_t i;

	for (channel = 0; channel < channels; channel++)
		for (i = 0; i < samples; i++)
			outputs[channel][i] = ngsMultiply(inputs[channel][i],
				engine->path[channel].dry);

	if (!reset)
		return;

	/* the wet output of the last block fades out into the dry */
	for (channel = 0; channel < channels; channel++) {
		step = ngsFadeStart / 64;
		for (k = 0; k < 64; k++)
			outputs[channel][k] += ngsMultiply(engine->buffers[channel][k],
				ngsWindow[ngsFadeStart + k * step]);
	}

	/* and everything is emptied */
	for (line = 0; line < NGS_REVERB_LINES; line++) {
		memset(engine->lines[line], 0, engine->lineLength[line] * 2);
		engine->lineOverrun[line] = 0;
		engine->lineLast[line] = 0;
	}
	for (channel = 0; channel < channels; channel++) {
		NgsReverbFilters *filters = engine->filters[channel];

		filters->previous = 0;
		filters->reserved2 = 0;
		filters->lowShelf.x1 = 0;
		filters->lowShelf.x2 = 0;
		filters->lowShelf.y1 = 0;
		filters->lowShelf.y2 = 0;
		for (block = 0; block < NGS_REVERB_ALLPASSES; block++)
			memset(filters->allpass[block].line, 0, 2420);
		memset(engine->buffers[channel], 0, 2048);
		memset(engine->path[channel].ring, 0, 29824);
	}
}

/*
 * 0x8b0fb8: the late reverb's input, delayed, and the early reflections,
 * from the last 7712 samples of the ring (history).
 */
static void delayTaps(const NgsReverbPath *path, const int16_t *history, int32_t *input,
	int32_t *early, uint32_t count)
{
	uint32_t i;
	int32_t tap;

	for (i = 0; i < count; i++)
		input[i] = history[NGS_REVERB_MAX_DELAY - path->reverbDelay + i] << 9;
	ngsClear(early, count * 4);
	for (tap = 0; tap < NGS_REVERB_TAPS; tap++) {
		const int16_t *source = history + NGS_REVERB_MAX_DELAY - path->tapDelay[tap];

		for (i = 0; i < count; i++)
			early[i] += ngsMultiply(source[i] << 9, path->tapGain[tap]);
	}
}

/* 0x8b0050: the diffusion allpasses 2 and 3 on the late input, in place */
static void allpassEarly(int32_t *samples, uint32_t count, NgsReverbFilters *filters)
{
	uint32_t block, i;

	for (block = 2; block < NGS_REVERB_ALLPASSES; block++) {
		NgsReverbAllpass *allpass = &filters->allpass[block];
		int32_t position = allpass->position;

		for (i = 0; i < count; i++) {
			int32_t delayed = allpass->line[position];
			int32_t state = samples[i] - ngsMultiply(delayed, allpass->gain);

			allpass->line[position] = state;
			samples[i] = delayed + ngsMultiply(state, allpass->gain);
			if (++position >= allpass->length)
				position -= allpass->length;
		}
		allpass->position = position;
	}
}

/*
 * 0x8aeb70: the pre-delay ring, the early reflections (into early) and the
 * delayed late input (in place of input) for one path.
 */
static int32_t reflections(int32_t *input, int32_t *early, uint32_t count, uint32_t index,
	NgsReverbEngine *engine, int16_t *history)
{
	NgsReverbPath *path = &engine->path[index];
	int16_t *packed = (int16_t *)input;
	uint32_t write = path->writePosition;
	uint32_t position = (path->reflectionsDelay + write) % NGS_REVERB_RING;
	uint32_t start, room;

	packSamples(input, count);
	room = NGS_REVERB_RING - position;
	if (room < count) {
		ringCopy(path->ring + position, packed, room * 2);
		ringCopy(path->ring, packed + room, (count - room) * 2);
	} else {
		ringCopy(path->ring + position, packed, count * 2);
	}

	start = (write + NGS_REVERB_HISTORY) % NGS_REVERB_RING;
	if (start + NGS_REVERB_HISTORY <= NGS_REVERB_RING) {
		ringCopy(history, path->ring + start, NGS_REVERB_HISTORY * 2);
	} else {
		room = NGS_REVERB_RING - start;
		ringCopy(history, path->ring + start, room * 2);
		ringCopy(history + room, path->ring, (NGS_REVERB_HISTORY - room) * 2);
	}
	write = (count + write) % NGS_REVERB_RING;

	delayTaps(path, history, input, early, count);
	allpassEarly(input, count, engine->filters[index]);
	path->writePosition = write;
	return 1;
}

/*
 * 0x8aef20: a path's late filter chain, in place: the first-order FIR
 * (b0, b1 scaled by the late gain, on a 64-bit sum), then allpass 0 and 1.
 * The allpasses add d g' instead of subtracting g d, so the products are of
 * the negated gain.
 */
static void lateFilter(int32_t *samples, uint32_t count, NgsReverbFilters *filters)
{
	NgsReverbAllpass *first = &filters->allpass[0];
	NgsReverbAllpass *second = &filters->allpass[1];
	int32_t b0 = ngsMultiply(filters->gain, filters->b0);
	int32_t b1 = ngsMultiply(filters->gain, filters->b1);
	int32_t previous = filters->previous;
	int32_t position0 = first->position, position1 = second->position;
	uint32_t i;

	for (i = 0; i < count; i++) {
		int32_t sample = samples[i];
		int32_t u = (int32_t)(((int64_t)sample * b0 + (int64_t)previous * b1) >> 24);
		int32_t delayed0 = first->line[position0], delayed1;
		int32_t state0 = u + ngsMultiply(delayed0, -first->gain);
		int32_t state1, out0;

		first->line[position0] = state0;
		out0 = ngsMultiply(state0, first->gain) + delayed0;

		delayed1 = second->line[position1];
		state1 = out0 + ngsMultiply(delayed1, -second->gain);
		second->line[position1] = state1;
		samples[i] = ngsMultiply(state1, second->gain) + delayed1;

		previous = sample;
		if (++position0 >= first->length)
			position0 -= first->length;
		if (++position1 >= second->length)
			position1 -= second->length;
	}
	filters->previous = previous;
	first->position = position0;
	second->position = position1;
}

/*
 * 0x8af748 and 0x8af890: the DC blocker on the wet output,
 * y = x - x1 + (1 - 1677 / 2^24) y1, with a 64-bit accumulator whose
 * fraction is kept between the blocks.
 */
static void dcBlock(NgsReverbEngine *engine, uint32_t channel, int32_t *samples, uint32_t count)
{
	uint64_t accumulator = ((uint64_t)engine->dcAccumulatorHigh[channel] << 32) |
		engine->dcAccumulatorLow[channel];
	int32_t previousInput = engine->dcInput[channel];
	int32_t previousOutput = engine->dcOutput[channel];
	uint32_t i;

	for (i = 0; i < count; i++) {
		int32_t input = samples[i];

		accumulator += (uint64_t)((int64_t)previousInput * -NGS_ONE);
		accumulator += (uint64_t)((int64_t)input * NGS_ONE);
		accumulator += (uint64_t)((int64_t)previousOutput * DC_LEAK);
		previousOutput = (int32_t)(accumulator >> 24);
		samples[i] = previousOutput;
		previousInput = input;
	}
	engine->dcAccumulatorLow[channel] = (uint32_t)accumulator;
	engine->dcAccumulatorHigh[channel] = (uint32_t)(accumulator >> 32);
	engine->dcInput[channel] = previousInput;
	engine->dcOutput[channel] = previousOutput;
}

/*
 * 0x8b2110 and 0x8b1e88: the input of the lines, row k of the 128-wide rows
 * (the rows are cleared from the caches first): the sum over the paths of the
 * path's samples times the line's gain for it. The products double the
 * operands' product on 64 bits and keep the high word, with the gain << 7.
 */
static void injectInput(int32_t *rows, int32_t **inputs, int32_t gains[][NGS_REVERB_PATHS],
	uint32_t count, int32_t paths)
{
	int32_t line, path;
	uint32_t i;

	for (line = 0; line < NGS_REVERB_LINES; line++)
		cacheInvalidateFlush((uint32_t)(rows + line * NGS_REVERB_CHUNK), count * 4);
	for (line = 0; line < NGS_REVERB_LINES; line++) {
		int32_t *row = rows + line * NGS_REVERB_CHUNK;

		for (i = 0; i < count; i++) {
			int32_t sum = 0;

			for (path = 0; path < paths; path++)
				sum += ivc2Multiply(inputs[path][i],
					(int32_t)((uint32_t)gains[line][path] << 7));
			row[i] = sum;
		}
	}
}

/*
 * 0x8b1b10: the lines' next samples, 192 per row: the delay lines at
 * their positions, in 8.24. The first 8 samples of a line are repeated
 * after its end, for the readers that run on.
 */
static void loadLines(int32_t *rows, NgsReverbEngine *engine, uint32_t count)
{
	int32_t k;
	uint32_t i, j;

	for (k = 0; k < NGS_REVERB_LINES; k++) {
		int32_t *row = rows + k * 192;
		int16_t *line = engine->lines[k];
		int32_t length = engine->lineLength[k];
		int32_t position = engine->linePosition[k];

		cacheInvalidateFlush((uint32_t)row, 768);
		for (j = 0; j < 8; j++)
			line[length + j] = line[j];
		for (i = 0; i < count; i++)
			row[i] = line[(position + i) % length] << 9;
	}
}

/*
 * 0x8b0578: the chorus on lines 0 to 2. Each line is read through a
 * 4-point Catmull-Rom interpolator at 7 samples ahead, modulated by one of
 * three LFO phases 120 degrees apart; the LFO is the engine's phasor,
 * advanced a sample at a time and kept at unit length.
 */
static void chorus(int32_t *rows, NgsReverbEngine *engine, uint32_t count)
{
	int32_t cosine = engine->lfoCosine, sine = engine->lfoSine;
	int32_t cosine1, sine1, k, norm;
	int32_t lfo[CHORUS_LINES], delay, fraction, a, b, c;
	int32_t *row;
	uint32_t i;
	int32_t index, x0, x1, x2, xm;

	for (i = 0; i < count; i++) {
		cosine1 = ngsMultiply(cosine, lfoRotation) + ngsMultiply(sine, lfoStep);
		sine1 = ngsMultiply(sine, lfoRotation) - ngsMultiply(cosine, lfoStep);
		lfo[0] = cosine1;
		lfo[1] = ngsMultiply(cosine, lfoPhase1Cosine) + ngsMultiply(sine, lfoPhase1Sine);
		lfo[2] = ngsMultiply(sine, lfoPhase2Sine) + ngsMultiply(cosine, lfoPhase2Cosine);
		for (k = 0; k < CHORUS_LINES; k++) {
			row = rows + k * 192;
			delay = ngsMultiply(lfo[k], LFO_DELAY_SCALE) + LFO_DELAY_BASE;
			index = i + (delay >> 24);
			fraction = delay & 0xffffff;
			xm = row[index - 1];
			x0 = row[index];
			x1 = row[index + 1];
			x2 = row[index + 2];
			a = ngsMultiply(0x1800000, x0 - x1) + ((x2 - xm) >> 1);
			b = xm - ngsMultiply(0x2800000, x0) + (x1 << 1) - (x2 >> 1);
			c = (x1 - xm) >> 1;
			row[i] = ngsMultiply(ngsMultiply(ngsMultiply(a, fraction) + b, fraction) + c,
				fraction) + x0;
		}
		cosine = cosine1;
		sine = sine1;
	}
	norm = (RENORMALISE - (ngsMultiply(sine, sine) + ngsMultiply(cosine, cosine))) / 2;
	engine->lfoCosine = ngsMultiply(cosine, norm);
	engine->lfoSine = ngsMultiply(sine, norm);
}

/*
 * 0x8afa88: the absorption of each line (y = gain x + damping y1 on its
 * delayed samples, rows of s2), the Householder matrix (s2 += s1 - (sum of
 * the lines / 8)), s1 getting the lines' absorbed output.
 */
static void absorb(int32_t *rows, int32_t *wet, NgsReverbEngine *engine, uint32_t count,
	int32_t *sum)
{
	int32_t k, tail;
	uint32_t i;

	memset(sum, 0, (count + 15) / 16 * 64);
	for (k = 0; k < NGS_REVERB_LINES; k++) {
		int32_t *delayed = rows + k * 192;
		int32_t *inject = wet + k * NGS_REVERB_CHUNK;
		int32_t gain = engine->lineGain[k], damping = engine->lineDamping[k];
		int32_t last = engine->lineLast[k];

		tail = delayed[count + 3];
		for (i = 0; i < count; i++) {
			int32_t absorbed = ngsMultiply(delayed[i], gain) + ngsMultiply(last, damping);

			delayed[i] = inject[i];
			inject[i] = absorbed;
			sum[i] += absorbed;
			last = absorbed;
		}
		engine->lineOverrun[k] = tail;
		engine->lineLast[k] = last;
	}
	for (i = 0; i < count; i++)
		sum[i] = -(sum[i] >> 3);
	for (k = 0; k < NGS_REVERB_LINES; k++) {
		int32_t *delayed = rows + k * 192;
		int32_t *absorbed = wet + k * NGS_REVERB_CHUNK;

		for (i = 0; i < count; i++)
			delayed[i] += absorbed[i] + sum[i];
	}
}

/* 0x8b1488: the lines' new samples written back to their delay lines */
static void storeLines(int32_t *rows, NgsReverbEngine *engine, uint32_t count)
{
	int32_t k;
	uint32_t i;

	for (k = 0; k < NGS_REVERB_LINES; k++) {
		int32_t *row = rows + k * 192;
		int16_t *line = engine->lines[k];
		int32_t length = engine->lineLength[k];
		int32_t position = engine->linePosition[k];

		for (i = 0; i < count; i++)
			line[(position + i) % length] = ivc2Clip(row[i] >> 9, 16);
	}
}

/* 0x8b1828: each path gets the lines' absorbed outputs with their signs (+1, -1 or 0) */
static void mixOutput(int32_t *wet, int32_t **outputs, int32_t signs[][NGS_REVERB_PATHS],
	uint32_t count, int32_t paths)
{
	int32_t k, path;
	uint32_t i;

	for (path = 0; path < paths; path++)
		ngsClear(outputs[path], count * 4);
	for (k = 0; k < NGS_REVERB_LINES; k++) {
		int32_t *row = wet + k * NGS_REVERB_CHUNK;

		for (path = 0; path < paths; path++) {
			if (signs[k][path] == NGS_ONE)
				for (i = 0; i < count; i++)
					outputs[path][i] += row[i];
			else if (signs[k][path] == -NGS_ONE)
				for (i = 0; i < count; i++)
					outputs[path][i] -= row[i];
		}
	}
}

/*
 * 0x84bf7e: one path of a block. Path 0 first runs the feedback delay
 * network over the block, in chunks of 128 samples, for all paths; every
 * path then filters its wet output and adds the early reflections.
 */
static void reverbPath(const int32_t *early, int32_t *buffer, uint32_t count, uint32_t path,
	NgsReverbEngine *engine, int32_t *wet, int32_t *rows, int32_t *sum)
{
	int32_t *inputs[NGS_REVERB_PATHS];
	uint32_t chunk, blocks, block, i;
	int32_t k, other;

	chunk = count < NGS_REVERB_CHUNK ? count : NGS_REVERB_CHUNK;
	blocks = count < NGS_REVERB_CHUNK ? 1 : count / NGS_REVERB_CHUNK;
	memset(inputs, 0, sizeof(inputs));

	if (path == 0) {
		for (block = 0; block < blocks; block++) {
			for (other = 0; other < engine->paths; other++)
				inputs[other] = engine->buffers[other] + block * chunk;
			if (engine->paths == 1 || engine->paths == 2)
				injectInput(wet, inputs, engine->inputGain, chunk, engine->paths);
			loadLines(rows, engine, chunk + 16);
			chorus(rows, engine, chunk);
			absorb(rows, wet, engine, chunk, sum);
			storeLines(rows, engine, chunk);
			mixOutput(wet, inputs, engine->outputSign, chunk, engine->paths);
			for (k = 0; k < NGS_REVERB_LINES; k++) {
				engine->linePosition[k] += chunk;
				if (engine->lineLength[k] < engine->linePosition[k])
					engine->linePosition[k] -= engine->lineLength[k];
			}
		}
	}
	lateFilter(buffer, count, engine->filters[path]);
	for (i = 0; i < count; i++)
		buffer[i] += early[i];
}

/*
 * 0x84be2c, RPC 327's entry: the wet output of the block Run stored. The
 * scratch memory holds the early reflections (4 * paths * samples bytes
 * after 2048 + 8192 + 12288), the lines' rows and a block's sum.
 */
void ngsReverbEngine(NgsReverbSettings *settings, void *buffers, uint8_t *scratch)
{
	NgsReverbEngine *engine = &settings->engine;
	uint32_t count = settings->granularity;
	int32_t paths = engine->paths;
	int32_t *wet, *rows, *early;
	int32_t *inputs[2];
	int32_t path;

	if (paths < 1 || paths > 2 || settings->bypassed)
		return;
	(void)buffers;
	wet = (int32_t *)(scratch + 2048);
	rows = (int32_t *)((uint8_t *)wet + 8192);
	early = (int32_t *)((uint8_t *)rows + 12288);
	inputs[0] = engine->buffers[0];
	inputs[1] = engine->buffers[1];

	if (paths == 1) {
		ngsFilterStereo(inputs[0], inputs[0], inputs[0], inputs[0], count,
			&engine->filters[0]->highCut, &engine->filters[0]->highCut);
		ngsFilterStereo(inputs[0], inputs[0], inputs[0], inputs[0], count,
			&engine->filters[0]->lowShelf, &engine->filters[0]->lowShelf);
	} else {
		ngsFilterStereo(inputs[0], inputs[1], inputs[0], inputs[1], count,
			&engine->filters[0]->highCut, &engine->filters[1]->highCut);
		ngsFilterStereo(inputs[0], inputs[1], inputs[0], inputs[1], count,
			&engine->filters[0]->lowShelf, &engine->filters[1]->lowShelf);
	}
	for (path = 0; path < paths; path++)
		reflections(inputs[path], early + path * count, count, path, engine, (int16_t *)wet);
	for (path = 0; path < paths; path++)
		reverbPath(early + path * count, inputs[path], count, path, engine, wet, rows,
			(int32_t *)scratch);
	if (engine->parameterId != NGS_REVERB_PARAMS_ID_V1) {
		if (paths == 1)
			dcBlock(engine, 0, inputs[0], count);
		else
			for (path = 0; path < 2; path++)
				dcBlock(engine, path, inputs[path], count);
	}
}

/*
 * 0x84c14c: a delay line made longer or shorter, the old and the new line
 * cross-faded (the window's two halves) at the ends and at the position.
 * Returns the position in the new line. The write to line[p + 1] is the
 * original's; the line has room for it.
 */
static int32_t resizeLine(int32_t oldLength, int32_t newLength, int32_t position, int16_t *line)
{
	int16_t tail[NGS_REVERB_CHUNK], before[NGS_REVERB_CHUNK], after[NGS_REVERB_CHUNK];
	int32_t i, newPosition, atNew, atBefore, behind;

	for (i = 0; i < NGS_REVERB_CHUNK; i++) {
		tail[i] = line[oldLength - NGS_REVERB_CHUNK + i];
		before[i] = line[(oldLength + position - NGS_REVERB_CHUNK + i) % oldLength];
		after[i] = line[(position + i) % oldLength];
	}
	if (oldLength < newLength) {
		memset(line + oldLength, 0, (newLength - oldLength) * 2);
		for (i = 0; i < NGS_REVERB_CHUNK; i++) {
			line[i] = ngsMultiply(line[i], ngsWindow[i]);
			line[oldLength - NGS_REVERB_CHUNK + i] =
				ngsMultiply(line[oldLength - NGS_REVERB_CHUNK + i],
					ngsWindow[FADE_HALF + i]);
		}
	} else {
		for (i = 0; i < NGS_REVERB_CHUNK; i++) {
			int32_t kept = line[newLength - NGS_REVERB_CHUNK + i];

			line[newLength - NGS_REVERB_CHUNK + i] = ngsMultiply(ngsWindow[i], tail[i]) +
				ngsMultiply(kept, ngsWindow[FADE_HALF + i]);
		}
	}

	newPosition = position < newLength ? position : 0;
	behind = (newPosition + newLength - NGS_REVERB_CHUNK) % newLength;
	for (i = 0; i < NGS_REVERB_CHUNK; i++) {
		atNew = line[newPosition];
		atBefore = line[behind];
		line[newPosition + 1] = ngsMultiply(after[i], ngsWindow[FADE_HALF + i]) +
			ngsMultiply(atNew, ngsWindow[i]);
		line[behind + 1] = ngsMultiply(before[i], ngsWindow[i]) +
			ngsMultiply(atBefore, ngsWindow[FADE_HALF + i]);
		behind = (behind + 1) % newLength;
		newPosition = (newPosition + 1) % newLength;
	}
	return position < newLength ? position : 0;
}

/*
 * 0x84c31e: line k's absorption from the decay time: the gain that takes
 * the line's delay to -60 dB over the decay time, and a one-pole damping
 * from the high frequency ratio. The line's last output is scaled down by
 * how far the damping moved, to keep the filter from ringing.
 */
static void lineAbsorption(NgsReverbEngine *engine, int32_t k, int32_t decayTime,
	int32_t decayHFRatio)
{
	int32_t length = engine->lineLength[k];
	int32_t seconds, gain, power, damping, change;

	if (k < CHORUS_LINES)
		length += 7;
	seconds = (int32_t)((float)length / (float)REVERB_SAMPLE_RATE * 16777216.0f);
	gain = ngsPower(10 * NGS_ONE, -3 * ngsDivide(seconds, decayTime));
	power = ngsPower(gain, ngsDivide(NGS_ONE, decayHFRatio) + NGS_ONE);
	damping = ngsDivide(2 * NGS_ONE, power + NGS_ONE) - NGS_ONE;
	damping = min(damping, MAX_DAMPING);
	change = abs(damping - engine->lineDamping[k]);
	engine->lineDamping[k] = damping;
	engine->lineGain[k] = ngsMultiply(gain, NGS_ONE - damping);
	engine->lineLast[k] = ngsMultiply(engine->lineLast[k], NGS_ONE - change);
}

/*
 * 0x84c42a: a path's first-order late FIR (the high frequency ratio's
 * shelf, scaled by how long the lines are against the decay time).
 */
static void lateFir(NgsReverbEngine *engine, int32_t path, int32_t decayHFRatio,
	int32_t decayTime, int32_t sampleRate)
{
	NgsReverbFilters *filters;
	int32_t sum = 0, k, lengths, root, q, r;

	for (k = 0; k < NGS_REVERB_LINES; k++)
		sum += engine->lineLength[k];
	lengths = ngsSqrt(ngsDivide(((sum << 12) / sampleRate) << 12, decayTime));
	root = ngsSqrt(decayHFRatio);
	q = ngsDivide(NGS_ONE - root, root + NGS_ONE);
	r = ngsDivide(q, NGS_ONE - q);
	filters = engine->filters[path];
	filters->b0 = ngsMultiply(lengths, r + NGS_ONE);
	r = ngsDivide(q, NGS_ONE - q);
	filters->reserved = 0;
	filters->b1 = ngsMultiply(lengths, -r);
}

/*
 * 0x84c4d4: the gain a path's late reverb gets from the decay time: the
 * -60 dB decay over the mean delay of the lines it reads (12 of the 16),
 * the 1 / sqrt of it, and a lift for short decay times.
 */
static int32_t pathLevel(int32_t decayTime, NgsReverbEngine *engine, int32_t path,
	int32_t sampleRate)
{
	int32_t sum = 21, k, mean, level, tilt = 0, result;

	for (k = 0; k < NGS_REVERB_LINES; k++) {
		int32_t sign = outputSigns[path][k];

		sum += sign * (engine->lineLength[k] * sign);
	}
	mean = sum / 12;
	level = ngsPower(10 * NGS_ONE, -3 * ngsDivide(((mean << 12) / sampleRate) << 12, decayTime));
	if (decayTime < SHORT_DECAY)
		tilt = NGS_ONE - ngsDivide(decayTime - DECAY_FLOOR, SHORT_DECAY_RANGE);
	result = ngsSqrt(ngsDivide(NGS_ONE, level));
	result = ngsMultiply(result, ngsMultiply(SHORT_DECAY_TILT, tilt) + NGS_ONE / 2);
	if (path == 5 && tilt > 0) {
		int32_t cube = ngsMultiply(ngsMultiply(tilt, tilt), tilt);

		result = ngsMultiply(result, ngsMultiply(cube, SURROUND_TILT) + NGS_ONE);
	}
	return result;
}

/*
 * 0x84c5c2: the high frequency damping filter, two one-pole low-passes
 * with the pole q: y = (1 - q)^2 x + 2 q y1 - q^2 y2. A gain of 1 or more
 * leaves the signal alone; below 167 (-100 dB) it is raised, returning -1.
 */
static int32_t highShelf(int32_t frequency, int32_t gain, NgsBiquad *filter, int32_t sampleRate)
{
	int32_t result = 0, w, c, sine2, gain2, a, b, root, q;

	if (gain >= NGS_ONE) {
		filter->b0 = NGS_ONE;
		filter->b1 = 0;
		filter->b2 = 0;
		filter->a1 = 0;
		filter->a2 = 0;
		return 0;
	}
	if (gain < HIGH_SHELF_FLOOR) {
		gain = HIGH_SHELF_FLOOR;
		result = -1;
	}
	w = ngsMultiply((((frequency << 16) / sampleRate) << 8), TWO_PI);
	c = ngsCos(w);
	sine2 = NGS_ONE - ngsMultiply(c, c);
	gain2 = ngsMultiply(gain, gain);
	a = ngsMultiply(gain2, sine2);
	b = ngsMultiply(NGS_ONE - c, gain) * 2 - a;
	root = ngsSqrt(b);
	q = ngsDivide(NGS_ONE - ngsMultiply(gain, c) - root, NGS_ONE - gain);
	filter->b0 = ngsMultiply(NGS_ONE - q, NGS_ONE - q);
	filter->b1 = 0;
	filter->b2 = 0;
	filter->a1 = q * 2;
	filter->a2 = -ngsMultiply(q, q);
	return result;
}

/* the quotient of a numerator and the denominator, or its product with the inverse for a small one */
static int32_t shelfQuotient(int32_t numerator, int32_t denominator)
{
	if (denominator < 0x20001)
		return ngsMultiply(numerator, ngsDivide(NGS_ONE, denominator));
	return ngsDivide(numerator, denominator);
}

/*
 * 0x84c6b0: the low frequency shelf, a low-shelf biquad (the RBJ
 * cookbook's) of amplitude `gain` with Q = 1 / sqrt(2), for a frequency of
 * 100 Hz at least.
 */
static void lowShelf(int32_t frequency, int32_t gain, NgsBiquad *filter, int32_t sampleRate)
{
	int32_t w, c, s, alpha, amplitude, twoAlphaRoot, plus, minus;
	int32_t b0, b1, b2, a0, a1, a2;

	frequency = frequency < 100 ? 100 << 16 : frequency << 16;
	w = ngsMultiply((frequency / sampleRate) << 8, TWO_PI);
	c = ngsCos(w);
	s = ngsSin(w);
	alpha = ngsMultiply(s, SQRT_2);
	alpha = (alpha + (int32_t)((uint32_t)alpha >> 31)) >> 1;
	amplitude = gain > 0 ? ngsSqrt(gain) : 0;
	twoAlphaRoot = ngsMultiply(amplitude, alpha) << 1;
	plus = gain + NGS_ONE;
	minus = gain - NGS_ONE;

	b0 = ngsMultiply(gain, twoAlphaRoot + (plus - ngsMultiply(minus, c)));
	b1 = ngsMultiply(gain, minus - ngsMultiply(plus, c)) << 1;
	b2 = ngsMultiply(gain, plus - ngsMultiply(minus, c) - twoAlphaRoot);
	a0 = twoAlphaRoot + (ngsMultiply(minus, c) + plus);
	a2 = ngsMultiply(minus, c) + plus - twoAlphaRoot;
	a1 = -((ngsMultiply(plus, c) + minus) << 1);

	filter->b0 = shelfQuotient(b0, a0);
	filter->b1 = shelfQuotient(b1, a0);
	filter->b2 = shelfQuotient(b2, a0);
	filter->a1 = -shelfQuotient(a1, a0);
	filter->a2 = -shelfQuotient(a2, a0);
}

/*
 * 0x84c842: a path's early reflection taps from the pattern (or the ARM's
 * own taps): delays scaled by the pattern's scalar and the reverb delay,
 * taps of the same delay merged by power, the gains scaled by `level`.
 * When the taps' delays or the late delay move, the ring is cross-faded
 * from where the old taps read to where the new ones will. Returns the
 * warnings, negative: 2 for the scale clamped, 1 for a tap's delay.
 */
static int32_t earlyReflections(NgsReverbEngine *engine, int32_t index, int32_t reverbDelay,
	int32_t pattern, int32_t scale, int32_t level, const NgsReverbTap *taps, int32_t sampleRate)
{
	NgsReverbPath *path = &engine->path[index];
	int16_t *ring = path->ring;
	int32_t delays[NGS_REVERB_TAPS], gains[NGS_REVERB_TAPS];
	int32_t positions[NGS_REVERB_TAPS];
	int16_t saved[NGS_REVERB_TAPS][NGS_REVERB_CHUNK], savedLate[NGS_REVERB_CHUNK];
	int32_t ratio, ratioWarning = 0, tapWarning = 0, changed = 0, j, i, n;
	int32_t lateSamples, lateDelay, base, late, delay, gain, weight, keep;

	ratio = ngsDivide(reverbDelay + ((0x4d50000 / sampleRate) << 8), 0x199999);
	if (ratio > NGS_ONE) {
		ratio = NGS_ONE;
		ratioWarning = 2;
	} else if (ratio < 0) {
		ratio = 0;
		ratioWarning = 2;
	}

	for (j = 0; j < NGS_REVERB_TAPS; j++) {
		delay = tapDelays[pattern][j];
		gain = tapGains[pattern][j];
		if (taps) {
			gain = (int32_t)(taps[j].gain * 16777216.0f);
			delay = taps[j].delay;
			if (delay < 0) {
				delay = 0;
				tapWarning = 1;
			} else if (delay > NGS_REVERB_MAX_DELAY) {
				delay = NGS_REVERB_MAX_DELAY;
				tapWarning = 1;
			}
		}
		delays[j] = ngsMultiply(ngsMultiply(delay, scale), ratio);
		if (path->tapDelay[j] != delays[j])
			changed = 1;
		gains[j] = gain;
	}
	/* taps of the same delay are one, of the power of both and the sign of the first */
	for (i = 1; i < NGS_REVERB_TAPS; i++) {
		for (j = 0; j < i; j++) {
			if (delays[j] != delays[i] || gains[j] == 0)
				continue;
			gain = ngsSqrt(ngsMultiply(gains[j], gains[j]) + ngsMultiply(gains[i], gains[i]));
			gains[j] = gains[j] < 0 ? -gain : gain;
			gains[i] = 0;
			break;
		}
	}
	for (j = 0; j < NGS_REVERB_TAPS; j++)
		path->tapGain[j] = ngsMultiply(gains[j], level);

	lateSamples = (int32_t)((float)reverbDelay / 16777216.0f * 48000.0f);
	lateDelay = lateSamples < 0 ? 0 : min(lateSamples, REVERB_MAX_PREDELAY);
	if (!changed && lateDelay == path->reverbDelay)
		return -(ratioWarning + tapWarning);

	/* the 128 samples the old taps are about to read */
	base = (path->writePosition + NGS_REVERB_HISTORY) % NGS_REVERB_RING;
	for (j = 0; j < NGS_REVERB_TAPS; j++)
		positions[j] = (base - path->tapDelay[j] + RING_BIAS) % NGS_REVERB_RING;
	late = (base - path->reverbDelay + RING_BIAS) % NGS_REVERB_RING;
	for (n = 0; n < NGS_REVERB_CHUNK; n++) {
		for (j = 0; j < NGS_REVERB_TAPS; j++) {
			saved[j][n] = ring[positions[j]];
			positions[j] = (positions[j] + 1) % NGS_REVERB_RING;
		}
		savedLate[n] = ring[late];
		late = (late + 1) % NGS_REVERB_RING;
	}

	for (j = 0; j < NGS_REVERB_TAPS; j++)
		path->tapDelay[j] = delays[j];
	path->reverbDelay = lateDelay;
	for (j = 0; j < NGS_REVERB_TAPS; j++)
		positions[j] = (base - path->tapDelay[j] + RING_BIAS) % NGS_REVERB_RING;
	late = (base - path->reverbDelay + RING_BIAS) % NGS_REVERB_RING;

	/* and where the new ones will read, blended with them */
	for (n = 0; n < NGS_REVERB_CHUNK; n++) {
		weight = ngsWindow[n];
		keep = NGS_ONE - weight;
		for (j = 0; j < NGS_REVERB_TAPS; j++) {
			ring[positions[j]] = ngsMultiply(ring[positions[j]], keep) +
				ngsMultiply(weight, saved[j][n]);
			positions[j] = (positions[j] + 1) % NGS_REVERB_RING;
		}
		ring[late] = ngsMultiply(ring[late], keep) + ngsMultiply(weight, savedLate[n]);
		late = (late + 1) % NGS_REVERB_RING;
	}
	return -(ratioWarning + tapWarning);
}

/*
 * 0x84cbf0: the ring's pre-delay changed. Growing it clears the samples
 * between the old and new read positions and fades the last 256 samples
 * before the old one out and, from the copy, in before the new one; shrinking
 * it cross-fades the ring before the new position with the old samples.
 * The 128-entry half windows are stretched to 256 samples by their means.
 */
static uint32_t resizePredelay(int32_t oldDelay, int32_t newDelay, uint32_t write, int16_t *ring)
{
	int32_t copy[256];
	uint32_t readOld = (oldDelay + write) % NGS_REVERB_RING;
	uint32_t readNew = (newDelay + write) % NGS_REVERB_RING;
	uint32_t position;
	int32_t n, previous, current, weight, rise, fall, previousRise, previousFall, value;

	if (oldDelay < newDelay) {
		if (readOld < readNew) {
			memset(ring + readOld, 0, (readNew - readOld) * 2);
		} else {
			memset(ring + readOld, 0, (NGS_REVERB_RING - readOld) * 2);
			memset(ring, 0, readNew * 2);
		}
		position = (NGS_REVERB_RING + readOld - 256) % NGS_REVERB_RING;
		previous = NGS_ONE;
		for (n = 0; n < 256; n++) {
			copy[n] = ring[position] << 9;
			current = ngsWindow[FADE_HALF + (n >> 1)];
			ring[position] = ngsMultiply(ring[position], (current + previous) >> 1);
			previous = current;
			position = (position + 1) % NGS_REVERB_RING;
		}
		position = (NGS_REVERB_RING + readNew - 256) % NGS_REVERB_RING;
		previous = 0;
		for (n = 0; n < 256; n++) {
			current = ngsWindow[n >> 1];
			weight = (current + previous) >> 1;
			value = ring[position] + (ngsMultiply(weight, copy[n]) >> 9);
			previous = current;
			if (value >= 0x8000)
				value = 32767;
			else
				value = max(value, -32767);
			ring[position] = value;
			position = (position + 1) % NGS_REVERB_RING;
		}
	} else if (newDelay < oldDelay) {
		position = (NGS_REVERB_RING + readOld - 256) % NGS_REVERB_RING;
		for (n = 0; n < 256; n++) {
			copy[n] = ring[position] << 9;
			position = (position + 1) % NGS_REVERB_RING;
		}
		position = (NGS_REVERB_RING + readNew - 256) % NGS_REVERB_RING;
		previousRise = 0;
		previousFall = NGS_ONE;
		for (n = 0; n < 256; n++) {
			rise = ngsWindow[n >> 1];
			fall = ngsWindow[FADE_HALF + (n >> 1)];
			value = (ngsMultiply(ring[position], (fall + previousFall) >> 1) << 9) +
				ngsMultiply(copy[n], (rise + previousRise) >> 1);
			previousRise = rise;
			previousFall = fall;
			ring[position] = value >> 9;
			position = (position + 1) % NGS_REVERB_RING;
		}
	}
	return write;
}

/* the samples of line k in the layout: 8-aligned, 8 spare, and 8 more for the chorus's lines */
static int32_t lineSamples(int32_t k)
{
	int32_t length = lineLengths[k];

	if (length % 8)
		length += 8 - length % 8;
	length += 8;
	if (k < CHORUS_LINES)
		length += 8;
	return length;
}

/* 0x84cdd8: the bytes Reset lays out for `paths` paths, or -1 */
static int32_t layoutSize(int32_t paths, int32_t full)
{
	int32_t size = 2384, k;

	if (paths < 1 || paths >= 9)
		return -1;
	if (full)
		for (k = 0; k < NGS_REVERB_LINES; k++)
			size += lineSamples(k) * 2;
	return size + paths * 0xa2e0 + 32;
}

/*
 * 0x84ce30: the engine made over memory that follows it: per path the
 * input buffer (2048 bytes), the ring (29824) and the filters (9824), each
 * block 16-aligned (a block already aligned is moved on by 16), and the
 * lines' memory if full. The tables are set for the slots of the layout.
 */
int32_t ngsReverbReset(NgsReverbEngine *engine, const NgsReverbConfig *config, int32_t channels,
	int32_t unused, int32_t full)
{
	uint8_t *cursor;
	int32_t paths, path, line, block, slot, k, j, padding = 0, pad, invPaths;

	if (channels < 1 || channels >= 10 || engine == NULL || config == NULL)
		return -1;
	paths = pathCounts[channels - 1];
	memset(engine, 0, sizeof(*engine));
	cursor = (uint8_t *)engine + sizeof(*engine);
	pad = 16 - ((uint32_t)cursor & 15);
	cursor += pad;
	padding += pad;
	for (path = 0; path < paths; path++) {
		engine->buffers[path] = (int32_t *)cursor;
		engine->path[path].ring = (int16_t *)(cursor + 2048);
		engine->filters[path] = (NgsReverbFilters *)(cursor + 2048 + 29824);
		cursor += 2048 + 29824 + 9824;
	}
	pad = 16 - ((uint32_t)cursor & 15);
	cursor += pad;
	padding += pad;
	if (full) {
		for (line = 0; line < NGS_REVERB_LINES; line++) {
			engine->lines[line] = (int16_t *)cursor;
			memset(cursor, 0, lineSamples(line) * 2);
			cursor += lineSamples(line) * 2;
		}
	} else {
		for (line = 0; line < NGS_REVERB_LINES; line++)
			engine->lines[line] = NULL;
	}
	cursor += 32 - padding;
	if (cursor - (uint8_t *)engine != layoutSize(paths, full))
		return -2;

	for (path = 0; path < paths; path++) {
		memset(engine->filters[path], 0, 9824);
		memset(engine->buffers[path], 0, 2048);
		memset(engine->path[path].ring, 0, 29824);
	}
	if (full)
		for (line = 0; line < NGS_REVERB_LINES; line++)
			memset(engine->lines[line], 0, lineSamples(line) * 2);
	engine->lfoSine = NGS_ONE;
	engine->paths = paths;
	engine->channels = channels;

	for (path = 0; path < paths; path++) {
		NgsReverbPath *p = &engine->path[path];

		slot = pathSlot(path, channels);
		for (block = 0; block < NGS_REVERB_ALLPASSES; block++) {
			NgsReverbAllpass *allpass = &engine->filters[path]->allpass[block];

			allpass->position = 0;
			allpass->length = allpassLengths[slot][block];
			allpass->gain = 0x800000;
			memset(allpass->line, 0, 2420);
		}
		p->dry = 0;
		p->reflectionsDelay = 0;
		p->writePosition = 0;
		p->reverbDelay = 0;
		for (j = 0; j < NGS_REVERB_TAPS; j++) {
			p->tapDelay[j] = tapDelays[path % 6][j];
			p->tapGain[j] = tapGains[path % 6][j];
		}
	}

	invPaths = NGS_ONE / paths;
	for (k = 0; k < NGS_REVERB_LINES; k++) {
		engine->lineGain[k] = 0;
		engine->lineDamping[k] = NGS_ONE;
		engine->linePosition[k] = 0;
		engine->lineLength[k] = lineLengths[k];
		for (path = 0; path < paths; path++) {
			slot = pathSlot(path, channels);
			engine->inputGain[k][path] = ngsMultiply(inputGains[slot][k], invPaths);
			engine->outputSign[k][path] = outputSigns[slot][k] << 24;
		}
	}
	return 0;
}

/*
 * 0x84d150: the engine configured from the parameters: the lines' lengths
 * (the denser, the more of the first set), their absorption, and for each
 * channel the dry and late gains, the damping and shelf filters, the
 * allpasses' gain, the early reflections and the pre-delay. Returns -3 for
 * a pattern of 6 or more, -1 for no pre-delay change room, -22 for one
 * clamped to 0.15 s.
 */
int32_t ngsReverbConfigure(NgsReverbEngine *engine, const NgsReverbConfig *config,
	int32_t channels, const NgsReverbTap *taps)
{
	int32_t lengths[NGS_REVERB_LINES];
	int32_t count, k, path, result = 0, delay, block;
	int32_t dry, room, reflections, reverb, early, late, roomHF, roomLF;
	NgsReverbFilters *filters;

	if (engine == NULL || config == NULL)
		return -1;
	engine->parameterId = config->desc.id;
	count = (int32_t)((uint32_t)config->density * 13) >> 24;
	count += 3;
	for (k = 0; k < NGS_REVERB_LINES; k++)
		lengths[k] = k < count ? lineLengths[k] : lineLengthsSparse[k];

	for (k = 0; k < NGS_REVERB_LINES; k++) {
		if (engine->lines[k] == NULL)
			continue;
		if (engine->lineLength[k] != lengths[k]) {
			engine->linePosition[k] = resizeLine(engine->lineLength[k], lengths[k],
				engine->linePosition[k], engine->lines[k]);
			engine->lineLength[k] = lengths[k];
		}
		lineAbsorption(engine, k, config->decayTime, config->decayHFRatio);
	}
	for (path = 0; path < engine->paths; path++)
		lateFir(engine, path, config->decayHFRatio, config->decayTime, REVERB_SAMPLE_RATE);

	for (path = 0; path < channels; path++) {
		dry = ngsPower(10 * NGS_ONE, config->dry);
		room = ngsPower(10 * NGS_ONE, config->room);
		reflections = ngsPower(10 * NGS_ONE, config->reflections);
		reverb = ngsPower(10 * NGS_ONE, config->reverb);
		early = ngsMultiply(reflections, room);
		late = ngsMultiply(ngsMultiply(reverb, PATH_GAIN_SCALE), room);
		engine->path[path].dry = dry;
		if (engine->parameterId == NGS_REVERB_PARAMS_ID_V1)
			late = ngsMultiply(late, room);
		filters = engine->filters[path];
		filters->gain = ngsMultiply(late,
			pathLevel(config->decayTime, engine, path, REVERB_SAMPLE_RATE));

		roomHF = ngsPower(10 * NGS_ONE, config->roomHF);
		roomLF = ngsPower(10 * NGS_ONE, config->roomLF);
		highShelf(config->hfReference, roomHF, &filters->highCut, REVERB_SAMPLE_RATE);
		lowShelf(config->lfReference, roomLF, &filters->lowShelf, REVERB_SAMPLE_RATE);
		for (block = 0; block < NGS_REVERB_ALLPASSES; block++)
			filters->allpass[block].gain = ngsMultiply(DIFFUSION_SCALE, config->diffusion);
		if (config->earlyReflectionPattern[path] >= 6)
			return -3;
		earlyReflections(engine, path, config->reverbDelay, config->earlyReflectionPattern[path],
			config->earlyReflectionScalar, early, taps, REVERB_SAMPLE_RATE);

		delay = (int32_t)((float)config->reflectionsDelay / 16777216.0f * 48000.0f);
		if (delay < 0) {
			delay = 0;
			result = -1;
		} else if (delay > NGS_REVERB_MAX_DELAY) {
			delay = NGS_REVERB_MAX_DELAY;
			result = -22;
		}
		delay &= ~7;
		if (delay != engine->path[path].reflectionsDelay) {
			resizePredelay(engine->path[path].reflectionsDelay, delay,
				engine->path[path].writePosition, engine->path[path].ring);
			engine->path[path].reflectionsDelay = delay;
		}
	}
	return result;
}
