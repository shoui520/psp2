/*
 * The CELP decoder's frame library (0x8c4000-0x8c4200, 0x8c5938-0x8c7ac8,
 * 0x8c7ac8-0x8c7d98): excitation of every subframe from the adaptive
 * (pitch) and fixed (pulse) codebooks, their gains, and the LPC synthesis
 * filter.
 */
#include "celp.h"

/* 0x8c4000: a wide value brought below 0x40004000 and its square root */
static int32_t normalizeSqrt(int64_t *value, int32_t shift, int32_t *exponent)
{
	int64_t v = *value;
	int32_t e;

	if (v > 0x20000000) {
		v >>= shift;
		e = 16;
	} else {
		v <<= 1;
		e = 24;
	}
	*value = v;
	if (!(v <= 0x40004000)) {
		do {
			v >>= 2;
			e--;
		} while (!(v <= 0x40004000));
		*value = v;
	}
	*exponent = e;
	return celpSqrt((int32_t)v);
}

/* 0x8c4200: the same, with the reciprocal square root */
static uint32_t normalizeInvSqrt(int64_t *value, int32_t shift, int32_t *exponent)
{
	int64_t v = *value;
	int32_t e;

	if (v > 0x20000000) {
		v >>= shift;
		e = 29;
	} else {
		v <<= 1;
		e = 23;
	}
	*value = v;
	if (!(v <= 0x7fffffff)) {
		do {
			v >>= 2;
			e++;
		} while (!(v <= 0x7fffffff));
		*value = v;
	}
	*exponent = e;
	return celpInvSqrt((int32_t)v);
}

/*
 * 0x8c74f0: how the pulses of a subframe share its positions. The
 * `subframeLength` / 2 position pairs start as groups of one; adjacent
 * groups of an equal size merge into one of the next until `pulses` are
 * left, and a pulse's group size gives the bits of its position
 * (`widths`). The groups are then dealt out over the positions, evenly
 * spaced, and each pulse's position table lists the positions it may take.
 */
void celpBuildPulses(CelpDecoderState *state, int32_t subframeLength, int32_t pulses,
	int32_t *widths, int32_t (*positions)[CELP_MAX_SUBFRAME])
{
	int32_t slots[CELP_MAX_SUBFRAME / 2 + 1] = { 0 };    /* the original clears 40 words */
	int32_t list[CELP_MAX_SUBFRAME / 2 + 2], next[CELP_MAX_SUBFRAME / 2 + 2];
	int32_t half = subframeLength / 2;
	int32_t count, passCount, i, j, minimum, size, slotCount, pulse, slot;

	for (i = 0; i < half; i++)
		list[i] = 1;
	list[half] = 0;
	count = half;
	if (pulses < half) {
		do {
			passCount = count;
			list[passCount] = 0;
			j = 0;
			i = 0;
			while (i < passCount) {
				int32_t a = list[i], b = list[i + 1];

				if (b != a) {
					next[j++] = a;
					i++;
					continue;
				}
				i += 2;
				count--;
				next[j++] = a + 1;
				if (!(pulses < count)) {
					while (j < count)
						next[j++] = list[i++];
					break;
				}
			}
			for (i = 0; i < count; i++)
				list[i] = next[i];
		} while (pulses < count);
	}
	for (i = 0; i < pulses; i++)
		widths[i] = list[i];

	minimum = 31;
	for (i = 0; i < pulses; i++)
		minimum = min(minimum, widths[i]);
	size = 1 << minimum;
	slotCount = subframeLength / size;
	for (i = 0; i < slotCount; i++)
		slots[i] = -1;
	for (pulse = 0; pulse < pulses; pulse++) {
		int32_t occupy = 1 << (widths[pulse] - minimum);
		int32_t taken = 0;

		slot = 0;
		while (taken < occupy) {
			if (slots[slot] == -1) {
				slots[slot] = pulse;
				slot += (occupy / 2 + slotCount) / occupy;
				slot %= slotCount;
				taken++;
			} else {
				slot++;
			}
		}
	}
	for (pulse = 0; pulse < pulses; pulse++) {
		int32_t filled = 0;

		for (slot = 0; slot < slotCount; slot++) {
			if (slots[slot] != pulse)
				continue;
			for (i = 0; i < size; i++)
				positions[pulse][filled + i] = slot + slotCount * i;
			filled += size;
		}
	}
}

/* 0x8c7ce8: the bits of a frame */
int32_t celpFrameBits(const CelpDecoderState *state)
{
	int32_t bits = 30, subframe;

	for (subframe = 0; subframe < state->subframes; subframe++)
		bits += 8 + celpPositionBits[state->bitRateIndex] + celpSignBits[state->bitRateIndex] + 6;
	if (state->align)
		bits = (bits + 7) & -8;
	return bits;
}

/* 0x8548e0: clears the largest sample (the first, among equals) */
static void removePeak(int32_t *samples, int32_t count)
{
	int32_t peak = 0, magnitude = 0, i;

	for (i = 0; i < count; i++) {
		if (magnitude < abs(samples[i])) {
			magnitude = abs(samples[i]);
			peak = i;
		}
	}
	samples[peak] = 0;
}

/*
 * The log-domain gain of every subframe: a line from the predicted level
 * (from the previous frame's last) to the frame's gain index, raised to the
 * tenth power.
 */
static void decodeGainLevels(CelpDecoderState *state)
{
	int32_t mode = state->gainMode, subframes = state->subframes, subframe;
	int64_t scaled = (int64_t)state->previousGainLevel * celpLevelFactor[mode];
	int32_t level = celpLog10((int32_t)(scaled >> 24) + 0x8000);
	int32_t base = (int32_t)(((int64_t)celpLevelScale[mode] * (level - 0x483f4)) >> 16);
	int32_t target = (state->gainIndex + 1) << 18;

	for (subframe = 0; subframe < subframes; subframe++) {
		int64_t weighted = (int64_t)(subframe + 1) * (target - base);
		int64_t share;
		int32_t exponent, power;

		if (subframes == 4)
			share = weighted >> 2;
		else if (subframes == 2)
			share = weighted >> 1;
		else
			share = weighted / subframes;
		exponent = (int32_t)(((int64_t)((int32_t)share + base) * celpGainExponent[mode]) >> 32);
		power = celpPow(0xa0000, exponent);
		state->gainLevel[subframe] = (int32_t)(((int64_t)celpGainScale[mode] * (power - 0x10000)
			+ 0x1000000) >> 25);
	}
}

/* the pitch: the past excitation repeated at the lag, whole or interpolated */
static void adaptiveCodebook(CelpDecoderState *state, int32_t subframe, int32_t *acb)
{
	int32_t length = state->subframeLength;
	int32_t lag = celpLagInteger[state->lagIndex[subframe]];
	int32_t fraction = celpLagFraction[state->lagIndex[subframe]];
	int32_t historyLength = celpLimits[2];
	int32_t taps = celpLimits[0] - 1;
	int32_t i = 0, k;

	if (fraction == 0) {
		while (i < length) {
			for (k = 0; k < lag && i < length; k++, i++) {
				int32_t past = state->history[historyLength - lag + k];
				int32_t sample = (int32_t)(((int64_t)past * celpInterpolation[0]) >> 15);

				acb[i] = sample;
				state->history[historyLength + i] = sample;
			}
		}
		return;
	}
	{
		int32_t phase = 0;

		while (i < length) {
			int32_t whole, limit;

			phase += fraction;
			whole = (phase + 2) >> 3;
			limit = whole + lag;
			phase -= 6 * whole;
			for (k = 0; k < limit && i < length; k++, i++) {
				int32_t sum = 0, m;

				for (m = -taps; m < taps; m++) {
					int32_t coefficient = celpInterpolation[abs(6 * m - phase + 6)];
					int32_t past = state->history[historyLength + k - m - limit - 1];

					sum += (int32_t)(((int64_t)coefficient * past) >> 15);
				}
				acb[i] = sum;
				state->history[historyLength + i] = sum;
			}
		}
	}
}

/* the pulses: +-1 (Q15) at the positions their indices choose */
static void fixedCodebook(CelpDecoderState *state, int32_t subframe, int32_t *fix)
{
	int32_t positionIndex = state->positionIndex[subframe];
	int32_t signIndex = state->signIndex[subframe];
	int32_t pulse;

	for (pulse = state->pulseCount - 1; pulse >= 0; pulse--) {
		int32_t width = state->pulseWidth[pulse];
		int32_t mask = ~(-1 << width);

		fix[state->pulsePosition[pulse][positionIndex & mask]] = (signIndex & 1) ? -32768 : 0x8000;
		signIndex >>= 1;
		positionIndex >>= width;
	}
}

/* 0x8c5938: the excitation of every subframe of the frame */
void celpDecodeExcitation(CelpDecoderState *state)
{
	int32_t length = state->subframeLength, order = state->order;
	int32_t mode = state->gainMode;
	int32_t historyLength = celpLimits[2];
	int32_t acb[CELP_MAX_SUBFRAME + 4], fix[CELP_MAX_SUBFRAME + 4];
	int32_t negated[CELP_MAX_ORDER], reflection[CELP_MAX_ORDER], saved[CELP_MAX_ORDER];
	int32_t removals = 0;
	int32_t subframe, i, j;

	decodeGainLevels(state);
	state->previousGainLevel = state->gainLevel[state->subframes - 1];
	if (state->lostCount != 0) {
		int32_t lost;

		state->lostFrames++;
		for (lost = 0; lost < state->lostFrames; lost++) {
			removals += length >> 3;
			if ((length * 3) >> 2 < removals) {
				removals = (length * 3) >> 2;
				break;
			}
		}
	} else {
		state->lostFrames = state->lostCount;
	}

	for (subframe = 0; subframe < state->subframes; subframe++) {
		int32_t lag = celpLagInteger[state->lagIndex[subframe]];
		const int32_t (*book)[4][2] = length == 40 ? celpGainBookShort : celpGainBookLong;
		int64_t energy, acbEnergy = 0, fixEnergy = 0, level;
		int32_t gainAcoustic = 0, gainFixed = 0;
		int32_t offset = length * subframe;

		for (i = 0; i < length + 4; i++) {
			acb[i] = 0;
			fix[i] = 0;
		}
		if (lag >= 1)
			adaptiveCodebook(state, subframe, acb);
		fixedCodebook(state, subframe, fix);
		if (mode >= 2 && lag >= 1)
			for (i = lag; i < length; i++)
				fix[i] += (int32_t)(((int64_t)celpSharpening[mode] * fix[i - lag]) >> 15);

		/* the energy the filter's gain leaves: the target less the reflections' */
		for (i = 0; i < order; i++)
			negated[i] = -state->lpc[order * subframe + i];
		for (i = order - 1; i >= 0; i--) {
			int64_t x = negated[i];
			int32_t denominator = 0x8000 - (int32_t)((x * x + 16384) >> 15);
			int32_t inverse = 0x40000000;

			if (denominator != 0)
				inverse = 0x40000000 / denominator;
			reflection[i] = negated[i];
			for (j = 0; j <= i; j++)
				saved[j] = negated[j];
			for (j = 0; j < i; j++) {
				int32_t scaled = (int32_t)(((int64_t)saved[i] * saved[i - j - 1]) >> 15);

				negated[j] = (int32_t)(((int64_t)(saved[j] - scaled) * inverse) >> 15);
			}
		}
		level = state->gainLevel[subframe];
		energy = (int64_t)length * ((level * level) >> 15);
		for (i = 0; i < order; i++) {
			int64_t r = reflection[i];
			int32_t factor = 0x8000 - (int32_t)((r * r) >> 15);

			energy = ((energy >> 6) * factor) >> 9;
		}
		for (i = 0; i < (length >> 2) * 4; i++) {
			acbEnergy += (int64_t)acb[i] * acb[i];
			fixEnergy += (int64_t)fix[i] * fix[i];
		}
		acbEnergy >>= 15;
		fixEnergy >>= 15;

		/* the gains, scaled to the target energy */
		if (acbEnergy != 0) {
			int64_t target = energy, measured = acbEnergy;
			int32_t targetExponent, measuredExponent;
			int32_t root = normalizeSqrt(&target, 15, &targetExponent);
			int32_t inverseRoot = (int32_t)normalizeInvSqrt(&measured, 11, &measuredExponent);
			int64_t product = (int64_t)root * inverseRoot;

			gainAcoustic = (int32_t)(((int64_t)book[state->gainCode[subframe]][mode][0]
				* (int32_t)(product >> ((targetExponent + measuredExponent - 15) & 63))) >> 14);
		}
		if (fixEnergy != 0) {
			int64_t target = energy, measured = fixEnergy;
			int32_t targetExponent, measuredExponent;
			int32_t root = normalizeSqrt(&target, 15, &targetExponent);
			int32_t inverseRoot = (int32_t)normalizeInvSqrt(&measured, 11, &measuredExponent);
			int64_t product = (int64_t)root * inverseRoot;

			gainFixed = (int32_t)(((int64_t)book[state->gainCode[subframe]][mode][1]
				* (int32_t)(product >> ((targetExponent + measuredExponent - 15) & 63))) >> 14);
		}
		for (i = 0; i < ((length + 3) & ~3); i++)
			state->excitation[offset + i] = (int32_t)(((int64_t)gainAcoustic * acb[i]
				+ (int64_t)gainFixed * fix[i]) >> 15);

		if (state->lostCount != 0)
			for (i = 0; i < removals; i++)
				removePeak(&state->excitation[offset], length);

		/* the past excitation slides down by a subframe and takes this one */
		for (i = 0; i < historyLength - length; i++)
			state->history[i] = state->history[length + i];
		for (i = 0; i < length; i += 8)
			for (j = 0; j < 8; j++)
				state->history[historyLength - length + i + j] = state->excitation[offset + i + j];
	}
}

/* 0x8c7ac8: the LPC synthesis filter over the frame's excitation */
void celpSynthesize(CelpDecoderState *state)
{
	int32_t length = state->subframeLength;
	int32_t subframe, i, k;

	for (subframe = 0; subframe < state->subframes; subframe++) {
		const int32_t *lpc = &state->lpc[state->order * subframe];
		int32_t *out = &state->output[length * subframe];
		const int32_t *excitation = &state->excitation[length * subframe];

		for (i = 0; i < length; i++) {
			int64_t sum = (int64_t)excitation[i] << 15;

			for (k = 0; k < CELP_ORDER; k++)
				sum += (int64_t)lpc[k] * state->filterState[k];
			out[i] = (int32_t)(sum >> 15);
			for (k = CELP_ORDER - 1; k >= 1; k--)
				state->filterState[k] = state->filterState[k - 1];
			state->filterState[0] = out[i];
		}
	}
}
