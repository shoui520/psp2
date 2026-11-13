/*
 * The CELP encoder's excitation search (0x8ca638, with its helpers 0x8c9ad0,
 * 0x856d8a, 0x854dd8 and 0x854ec4): the frame's gain index from the speech's
 * level, then for each subframe the target of the weighted speech, the
 * adaptive codebook at the pitch lag, a beam search of the pulse positions
 * and signs against the weighted synthesis filter's impulse response, the
 * gain codebook index, and the filters' memories carried to the next
 * subframe.
 */
#include "celp.h"

#define HISTORY		150	/* celpEncLimits[2]: the past excitation the pitch reads */
#define MAX_LENGTH	80
#define MAX_PULSES	12
#define MAX_BEAM	32
#define USED		(-1)	/* a pulse's score once it is ordered */
#define TAKEN		((int32_t)0x80000001)	/* a cost once its candidate is taken */
#define TWO_POW_43	0x80000000000LL
#define LEAST_MAX	0x7fffffffffffffffLL	/* no error or cost found yet */

/*
 * 0x856d8a: the subframe's weighted synthesis filter run on in[] from rest:
 * the synthesis filter (lpc), then the weighting filter (numerator and
 * denominator) on its output.
 */
static void weightedSynthesis(CelpEncoderState *state, const int32_t *in, int32_t *out,
	const int32_t *lpc, const int32_t *numerator, const int32_t *denominator)
{
	int32_t synthesized[CELP_ORDER], weighted[CELP_ORDER];
	int32_t order = state->order, i, k;

	for (k = 0; k < order >> 1; k++) {
		synthesized[2 * k] = synthesized[2 * k + 1] = 0;
		weighted[2 * k] = weighted[2 * k + 1] = 0;
	}
	for (i = 0; i < state->subframeLength; i++) {
		int64_t input = (int64_t)in[i] << 16, past = 0, shaped = 0;

		for (k = 0; k < order; k++)
			past += (int64_t)lpc[k] * synthesized[k];
		for (k = 0; k < order; k++)
			shaped += (int64_t)numerator[k] * synthesized[k] -
				(int64_t)denominator[k] * weighted[k];
		for (k = order - 1; k >= 1; k--) {
			synthesized[k] = synthesized[k - 1];
			weighted[k] = weighted[k - 1];
		}
		synthesized[0] = (int32_t)((input + past) >> 16);
		weighted[0] = (int32_t)((input + past + shaped) >> 16);
		out[i] = weighted[0];
	}
}

/*
 * 0x854dd8: an energy's square root scaled by the target's: the root (of the
 * target energy, `exponent` its normalization) >> (exponent - shift), times the
 * reciprocal square root of `energy`, >> its exponent less half of `half`.
 */
static uint32_t scaleEnergy(int64_t *out, const int64_t *energy, const int64_t *root,
	int32_t exponent, int32_t shift, int32_t half)
{
	int64_t scaled = *root >> (exponent - shift), v = *energy;
	int32_t argument, e;
	uint32_t inverse;

	if (v <= 0x7fffffff) {
		argument = (int32_t)v;
		e = 31;
	} else if ((v >> 2) <= 0x7fffffff) {
		argument = (int32_t)(v >> 2);
		e = 32;
	} else {
		argument = (int32_t)(v >> 4);
		e = 33;
	}
	inverse = celpInvSqrt(argument);
	*out = (scaled * (int64_t)(int32_t)inverse) >> (e - (half >> 1));
	return inverse;
}

/*
 * 0x854ec4: the fixed codebook's excitation: each pulse (Q12) at its
 * position, sharpened at the pitch lag from gain mode 2 on.
 */
static void pulseExcitation(CelpEncoderState *state, int32_t *out, const int32_t *positions,
	const int32_t *signs, int32_t lag)
{
	int32_t i;

	for (i = 0; i < state->subframeLength; i++)
		out[i] = 0;
	for (i = 0; i < state->pulseCount; i++)
		out[positions[i]] = signs[i] << 12;
	if (state->gainMode < 2 || lag < 1)
		return;
	for (i = lag; i < state->subframeLength; i++)
		out[i] = ((out[i] << 14) + out[i - lag] * celpEncSharpening[state->gainMode]) >> 14;
}

/* a square root's argument brought to 0x40004000 or below, and its shift */
static int32_t normalizeRoot(int64_t value, int32_t *shift)
{
	if (value <= 0x40004000) {
		*shift = 16;
		return (int32_t)value;
	}
	if ((value >> 2) <= 0x40004000) {
		*shift = 15;
		return (int32_t)(value >> 2);
	}
	*shift = 14;
	return (int32_t)(value >> 4);
}

/* the coprocessor's ratio correlation^2 / energy: 17 restoring steps against energy << 29 */
static int32_t ratio(int32_t correlation, int32_t energy)
{
	int64_t remainder = (int64_t)correlation * correlation;
	int64_t divisor = (int64_t)energy << 29;
	int32_t quotient = 0, step;

	for (step = 0; step < 17; step++) {
		int64_t difference;

		remainder <<= 1;
		difference = remainder - divisor;
		quotient <<= 1;
		if (difference >= 0) {
			remainder = difference;
			quotient++;
		}
	}
	return quotient;
}

/* a pair of positions' entry of the correlation matrix, which keeps the lower triangle */
static int32_t matrixEntry(const CelpEncoderState *state, int32_t a, int32_t b)
{
	return a > b ? state->matrix[a][b] : state->matrix[b][a];
}

/*
 * 0x8c9ad0: every candidate of the beam extended by each position of the
 * next pulse in order (order[placed]): the extended correlation, energy and
 * their ratio, by candidate * positions + position. A candidate the beam does
 * not have (-1) gives zeros.
 */
static void extendCandidates(CelpEncoderState *state, int32_t placed, const int32_t *order,
	const int32_t *positionCounts, int32_t beam, const int32_t *candidateCorrelation,
	const int32_t *candidateEnergy)
{
	int32_t pulse = order[placed], count = positionCounts[pulse];
	int32_t candidate, m, r;

	for (candidate = 0; candidate < beam; candidate++) {
		int32_t base = candidate * count;

		if (state->candidate[candidate][order[placed - 1]] == -1) {
			for (m = 0; m < count; m++) {
				state->sumCorrelation[base + m] = 0;
				state->sumEnergy[base + m] = 0;
				state->cost[base + m] = 0;
			}
			continue;
		}
		for (m = 0; m < count; m++) {
			int32_t position = state->pulsePosition[pulse][m];
			int32_t correlation = state->correlation[position] +
				candidateCorrelation[candidate];
			int32_t energy = state->matrix[position][position] + candidateEnergy[candidate];

			for (r = placed - 1; r >= 0; r--)
				energy += matrixEntry(state, position,
					state->candidate[candidate][order[r]]);
			state->sumCorrelation[base + m] = correlation;
			state->sumEnergy[base + m] = energy;
			state->cost[base + m] = ratio(correlation, energy);
		}
	}
}

/*
 * The gain index of the frame: its fixed codebook level spread over the
 * subframes from the last frame's, closest to each subframe's speech level
 * (gainIndex), and each subframe's gain level from it (gainLevel).
 */
static void searchGainIndex(CelpEncoderState *state)
{
	int32_t mode = state->gainMode, subframes = state->subframes, length = state->subframeLength;
	int32_t gainScale = celpEncGainScale[mode], levelFactor = celpEncLevelFactor[mode];
	int32_t levelScale = celpEncLevelScale[mode];
	int32_t level[4], subframe, index, base, i;
	int64_t best = LEAST_MAX;

	base = (int32_t)(((int64_t)celpLog10((int32_t)(((int64_t)levelFactor *
		state->previousGainLevel + 0x800000) >> 24) + 1) * levelScale + 0x80000000LL) >> 16);
	state->gainIndex = 0;
	for (subframe = 0; subframe < subframes; subframe++) {
		int64_t sum = 0;
		int32_t mean = 0, root, shift;

		for (i = 0; i < length; i++) {
			int32_t sample = state->speech[80 + subframe * length + i];

			sum += (int64_t)sample * sample;
		}
		if (length != 0 && sum != 0)
			mean = sum < 0 ? -celpEncDivide(-sum, length) : celpEncDivide(sum, length);
		root = celpSqrt(normalizeRoot(mean, &shift)) >> shift;
		level[subframe] = (int32_t)(((int64_t)levelScale * celpLog10((int32_t)(((int64_t)root *
			levelFactor + 0x800000) >> 24) + 1)) >> 16);
	}

	for (index = 0; index < 64; index++) {
		int32_t target = (index + 1) << 18;
		int64_t error = 0;

		for (subframe = 0; subframe < subframes; subframe++) {
			int64_t weighted = (int64_t)(subframe + 1) * (target - base);
			int32_t share, difference;

			if (subframes == 4)
				share = (int32_t)((weighted + 2) >> 2);
			else if (subframes == 2)
				share = (int32_t)((weighted + 1) >> 1);
			else
				share = (int32_t)((weighted + (subframes >> 1)) / subframes);
			difference = level[subframe] - (share + base);
			error += (int64_t)difference * difference;
		}
		if (error < best) {
			best = error;
			state->gainIndex = index;
		}
	}

	for (subframe = 0; subframe < subframes; subframe++) {
		int64_t weighted = (int64_t)(subframe + 1) * (((state->gainIndex + 1) << 18) - base);
		int32_t share = 0, power;

		if (subframes != 0 && weighted != 0)
			share = weighted < 0 ? -celpEncDivide(-weighted, subframes) :
				celpEncDivide(weighted, subframes);
		power = celpPow(0xa0000, (int32_t)(((int64_t)(share + base) << 16) / levelScale));
		state->gainLevel[subframe] = (int16_t)((((int64_t)((power >> 16) - 1)) * gainScale) >> 24);
	}
	state->previousGainLevel = state->gainLevel[subframes - 1];
}

/*
 * The adaptive codebook of a subframe: the past excitation (history, which the
 * subframe's samples are appended to) at the lag, whole or interpolated in
 * sixths of a sample; zeros without a lag.
 */
static void adaptiveCodebook(CelpEncoderState *state, int32_t subframe, int32_t *history,
	int32_t *acb)
{
	int32_t length = state->subframeLength, taps = celpEncLimits[0] - 1;
	int32_t lag = celpEncLagInteger[state->lagIndex[subframe]];
	int32_t fraction, phase = 0, i = 0, k;

	if (lag < 1) {
		for (i = 0; i < length; i++)
			acb[i] = 0;
		return;
	}
	fraction = celpEncLagFraction[state->lagIndex[subframe]];
	if (fraction == 0) {
		while (i < length) {
			for (k = 0; k < lag && i < length; k++, i++) {
				acb[i] = (int32_t)(((int64_t)celpEncInterpolation[0] *
					history[HISTORY - lag + k]) >> 15);
				history[HISTORY + i] = acb[i];
			}
		}
		return;
	}
	while (i < length) {
		int32_t whole, limit;

		phase += fraction;
		whole = (phase + 2) >> 3;
		limit = whole + lag;
		phase -= 6 * whole;
		for (k = 0; k < limit && i < length; k++, i++) {
			int32_t m;

			acb[i] = 0;
			for (m = -taps; m < taps; m++)
				acb[i] += (int32_t)(((int64_t)history[HISTORY + k - m - limit - 1] *
					celpEncInterpolation[abs(6 * m - phase + 6)]) >> 15);
			history[HISTORY + i] = acb[i];
		}
	}
}

/* the step-down of the subframe's LPC: the reflection coefficients (Q24) */
static void reflectionCoefficients(const CelpEncoderState *state, const int32_t *lpc,
	int32_t *reflection)
{
	int32_t a[CELP_ORDER], copy[CELP_ORDER];
	int32_t order = state->order, m, i;

	for (i = 0; i < order; i++)
		a[i] = -(lpc[i] << 2);
	for (m = order - 1; m >= 0; m--) {
		int32_t k = a[m], denominator;
		int64_t inverse;

		denominator = (int32_t)((0x1000000000LL - (int64_t)k * k) >> 18);
		inverse = denominator ? TWO_POW_43 / denominator : TWO_POW_43;
		reflection[m] = k;
		for (i = 0; i <= m; i++)
			copy[i] = a[i];
		for (i = 0; i < m; i++)
			a[i] = (int32_t)(((int64_t)(copy[i] -
				(int32_t)(((int64_t)copy[m - i - 1] * k) >> 18)) * inverse) >> 25);
	}
}

/*
 * The positions of the pulses of a subframe: the pulses ordered by their best
 * single position, then a beam search that places them one at a time, keeping
 * the best candidates. Only the best candidate is kept at the end.
 */
static void searchPulses(CelpEncoderState *state, const int32_t *sign, int32_t *positions,
	int32_t *signs)
{
	int32_t positionCounts[MAX_PULSES], score[MAX_PULSES], order[MAX_PULSES];
	int32_t candidateCorrelation[MAX_BEAM], candidateEnergy[MAX_BEAM];
	int32_t pulses = state->pulseCount, pulse, beam, placed, m, i;

	for (pulse = 0; pulse < pulses; pulse++)
		positionCounts[pulse] = 1 << state->pulseWidth[pulse];

	/* each pulse's best position alone */
	for (pulse = 0; pulse < pulses; pulse++) {
		int32_t best = 0;

		for (m = 0; m < positionCounts[pulse]; m++) {
			int32_t position = state->pulsePosition[pulse][m];

			best = max(best, ratio(state->correlation[position],
				state->matrix[position][position]));
		}
		score[pulse] = best;
	}
	for (i = 0; i < pulses; i++) {
		int32_t best = -1, chosen = -1;

		for (pulse = 0; pulse < pulses; pulse++) {
			if (best < score[pulse]) {
				best = score[pulse];
				chosen = pulse;
			}
		}
		order[i] = chosen;
		score[chosen] = USED;
	}

	/* the first pulse: every position a candidate */
	pulse = order[0];
	for (m = 0; m < positionCounts[pulse]; m++) {
		int32_t position = state->pulsePosition[pulse][m];

		state->candidate[m][pulse] = position;
		candidateCorrelation[m] = state->correlation[position];
		candidateEnergy[m] = state->matrix[position][position];
	}
	for (m = positionCounts[pulse]; m < MAX_BEAM; m++)
		state->candidate[m][pulse] = -1;
	beam = positionCounts[pulse];

	for (placed = 1; placed < pulses; placed++) {
		int32_t count, total, next, width;

		extendCandidates(state, placed, order, positionCounts, beam, candidateCorrelation,
			candidateEnergy);
		pulse = order[placed];
		count = positionCounts[pulse];
		width = state->pulseWidth[pulse];
		total = beam * count;
		next = min(total, count);
		for (m = 0; m < next; m++) {
			int32_t best = 0, j, from;

			for (j = 1; j < total; j++)
				if (state->cost[j] > state->cost[best])
					best = j;
			state->cost[best] = TAKEN;
			candidateCorrelation[m] = state->sumCorrelation[best];
			candidateEnergy[m] = state->sumEnergy[best];
			from = best >> width;
			for (i = 0; i < placed; i++)
				state->nextCandidate[m][order[i]] = state->candidate[from][order[i]];
			state->nextCandidate[m][pulse] = state->pulsePosition[pulse][best & (count - 1)];
		}
		for (m = 0; m < next; m++)
			for (i = 0; i <= placed; i++)
				state->candidate[m][order[i]] = state->nextCandidate[m][order[i]];
		for (m = next; m < MAX_BEAM; m++)
			for (i = 0; i <= placed; i++)
				state->candidate[m][order[i]] = -1;
		beam = next;
	}

	for (pulse = 0; pulse < pulses; pulse++) {
		positions[pulse] = state->candidate[0][pulse];
		signs[pulse] = sign[positions[pulse]];
	}
}

/* 0x8ca638: the gain index, then every subframe's pulses and gains */
void celpEncSearchExcitation(CelpEncoderState *state)
{
	CelpEncFilterMemory *memory = &state->memory;
	const int16_t (*book)[4][2];
	int32_t history[HISTORY + MAX_LENGTH];
	int32_t target[MAX_LENGTH], acb[MAX_LENGTH], filteredAcb[MAX_LENGTH];
	int32_t fixed[MAX_LENGTH], fixedFiltered[MAX_LENGTH];
	int32_t positions[MAX_PULSES], signs[MAX_PULSES];
	int32_t sign[MAX_LENGTH];
	int32_t sum[MAX_LENGTH], reflection[CELP_ORDER];
	int32_t order = state->order, mode, subframe, i, j, k, m;

	searchGainIndex(state);
	mode = state->gainMode;
	book = state->subframeLength == 40 ? celpEncGainBookShort : celpEncGainBookLong;

	for (subframe = 0; subframe < state->subframes; subframe++) {
		int32_t length = state->subframeLength;
		const int32_t *lpc = &state->lpc[subframe * order];
		const int32_t *numerator = &state->perceptualNumerator[subframe * order];
		const int32_t *denominator = &state->perceptualDenominator[subframe * order];
		int32_t lag = celpEncLagInteger[state->lagIndex[subframe]];
		int32_t gain, scaled, p, bestIndex, gain1, gain2;
		int64_t correlation, energy, quotient, targetEnergy, targetAcb, acbEnergy;
		int64_t fixedTarget, acbEnergyScaled, fixedEnergyScaled, bestCost, level;
		int32_t fixedAcb, fixedEnergy, shift = 0;

		/* the target: the weighted speech less the filters' response without input */
		for (i = 0; i < order; i++) {
			state->zeroInput[80 + i] = memory->synthesis[80 + i];
			state->weightedZeroInput[80 + i] = memory->weightedSynthesis[80 + i];
		}
		for (i = 0, p = 80; i < length; i++, p--) {
			int32_t speech = state->speech[80 + subframe * length + i];
			int64_t weighted = (int64_t)speech << 16, zero = 0, shaped = 0;

			for (k = 0; k < order; k++) {
				weighted += (int64_t)numerator[k] * memory->weightInput[p + k] -
					(int64_t)denominator[k] * memory->weightOutput[p + k];
				shaped += (int64_t)numerator[k] * state->zeroInput[p + k] -
					(int64_t)denominator[k] * state->weightedZeroInput[p + k];
				zero += (int64_t)lpc[k] * state->zeroInput[p + k];
			}
			memory->weightOutput[p - 1] = (int32_t)(weighted >> 16);
			memory->weightInput[p - 1] = speech;
			state->zeroInput[p - 1] = (int32_t)(zero >> 16);
			state->weightedZeroInput[p - 1] = (int32_t)((zero + shaped) >> 16);
			target[i] = (int32_t)((weighted - (zero + shaped)) >> 16);
		}

		/* the adaptive codebook, its filtered response and gain */
		for (i = 0; i < HISTORY; i++)
			history[i] = state->history[i];
		adaptiveCodebook(state, subframe, history, acb);
		weightedSynthesis(state, acb, filteredAcb, lpc, numerator, denominator);
		correlation = 0;
		energy = 0;
		for (i = 0; i < length; i++) {
			correlation += (int64_t)target[i] * filteredAcb[i];
			energy += ((int64_t)filteredAcb[i] * filteredAcb[i]) >> 11;
		}
		quotient = energy ? (correlation << 11) / energy : 0;
		gain = min(max((int32_t)quotient, 0), 2458) << 3;
		for (i = 0; i < length; i++)
			state->reducedTarget[i] = (int16_t)((int32_t)(((uint32_t)target[i] << 14) -
				(uint32_t)(gain * (filteredAcb[i] >> 11))) >> 14);

		/* the impulse response, sharpened at the lag */
		weightedSynthesis(state, celpEncImpulse, state->impulse, lpc, numerator, denominator);
		for (i = 0; i < length; i++)
			state->impulse16[i] = (int16_t)state->impulse[i];
		if (mode >= 2 && lag >= 1)
			for (i = lag; i < length; i++)
				state->impulse16[i] = (int16_t)((state->impulse16[i] +
					celpEncSharpening[mode] * state->impulse16[i - lag]) >> 14);

		/* the target's correlation with the response, made positive, and the signs */
		for (i = 0; i < length; i++) {
			int32_t c = 0;

			for (j = 0; j < length - i; j++)
				c += state->reducedTarget[i + j] * state->impulse16[j];
			state->correlation[i] = c;
			sign[i] = 1;
			if (c < 0) {
				sign[i] = -1;
				state->correlation[i] = -c;
			}
		}
		/* the response's correlations: the diagonal, then the lower triangle signed */
		{
			int32_t diagonal = 0;

			for (i = length - 1; i >= 0; i--) {
				diagonal += state->impulse16[length - 1 - i] * state->impulse16[length - 1 - i];
				state->matrix[i][i] = diagonal;
			}
		}
		for (i = length - 1; i >= 0; i--)
			sum[i] = 0;
		for (i = length - 1; i >= 1; i--) {
			for (k = i; k >= 1; k--) {
				sum[k] += state->impulse16[length - 1 - i] * state->impulse16[length - 1 - i + k];
				state->matrix[i][i - k] = (sum[k] * sign[i] * sign[i - k]) << 1;
			}
		}

		searchPulses(state, sign, positions, signs);

		/* the target's energy at the subframe's gain level, through the filter */
		for (i = 0; i < length; i++)
			filteredAcb[i] >>= 3;
		targetEnergy = 0;
		targetAcb = 0;
		acbEnergy = 0;
		for (i = 0; i < length; i++) {
			targetEnergy += (int64_t)target[i] * target[i];
			acbEnergy += (int64_t)filteredAcb[i] * filteredAcb[i];
			targetAcb += (int64_t)target[i] * filteredAcb[i];
		}
		targetAcb >>= 8;
		acbEnergy >>= 16;
		reflectionCoefficients(state, lpc, reflection);
		scaled = (int32_t)state->gainLevel[subframe] << 10;
		level = (((int64_t)scaled * scaled) >> 10) * length;
		for (i = 0; i < order; i++)
			level = ((int64_t)(int32_t)((0x1000000000LL - (int64_t)reflection[i] *
				reflection[i]) >> 18) * level) >> 18;
		level >>= 10;

		/* the fixed codebook's excitation and its filtered response */
		pulseExcitation(state, fixed, positions, signs, lag);
		weightedSynthesis(state, fixed, fixedFiltered, lpc, numerator, denominator);
		fixedTarget = 0;
		{
			int64_t cross = 0, power = 0;

			for (i = 0; i < length; i++) {
				fixedTarget += (int64_t)fixedFiltered[i] * target[i];
				cross += (int64_t)fixedFiltered[i] * filteredAcb[i];
				power += (int64_t)fixedFiltered[i] * fixedFiltered[i];
			}
			fixedAcb = (int32_t)(cross >> 8);
			fixedEnergy = (int32_t)(power >> 12);
		}
		acbEnergyScaled = 0;
		fixedEnergyScaled = 0;
		for (i = 0; i < length; i++) {
			acb[i] >>= 9;
			acbEnergyScaled += (int64_t)acb[i] * acb[i];
			fixedEnergyScaled += (int64_t)fixed[i] * fixed[i];
		}
		acbEnergyScaled >>= 2;
		fixedEnergyScaled >>= 12;
		if (acbEnergyScaled != 0 || fixedEnergyScaled != 0) {
			int64_t root, copy;

			root = celpSqrt(normalizeRoot(level, &shift));
			if (acbEnergyScaled != 0) {
				copy = acbEnergyScaled;
				scaleEnergy(&acbEnergyScaled, &copy, &root, shift, 10, 2);
			}
			if (fixedEnergyScaled != 0) {
				copy = fixedEnergyScaled;
				scaleEnergy(&fixedEnergyScaled, &copy, &root, shift, 5, 12);
			}
		}

		/* the gain codebook: the pair that leaves the least error */
		bestCost = LEAST_MAX;
		bestIndex = -1;
		gain1 = 0;
		gain2 = 0;
		for (i = 0; i < 64; i++) {
			int32_t x = (int32_t)((acbEnergyScaled * book[i][mode][0]) >> 10);
			int32_t y = (int32_t)((fixedEnergyScaled * book[i][mode][1]) >> 5);
			int64_t xy = ((int64_t)x * y) >> 14, cost;

			cost = ((xy * fixedAcb) >> 12) - (((int64_t)y * fixedTarget) >> 12) -
				(int64_t)x * targetAcb;
			cost = targetEnergy + ((cost << 1) >> 14);
			cost += ((((int64_t)x * x) >> 14) * acbEnergy) >> 14;
			cost += (((((int64_t)y * y) >> 14) * fixedEnergy) >> 12) >> 14;
			if (cost < bestCost && cost >= 0) {
				bestCost = cost;
				bestIndex = i;
				gain1 = x;
				gain2 = y;
			}
		}
		state->gainCode[subframe] = bestIndex == -1 ? 0 : bestIndex;

		/* the indices of the positions and the signs */
		pulseExcitation(state, fixed, positions, signs, lag);
		state->positionIndex[subframe] = 0;
		state->signIndex[subframe] = 0;
		for (i = 0; i < state->pulseCount; i++) {
			int32_t count = 1 << state->pulseWidth[i];

			state->positionIndex[subframe] <<= state->pulseWidth[i];
			for (m = 0; m < count; m++)
				if (positions[i] == state->pulsePosition[i][m])
					break;
			state->positionIndex[subframe] += m;
			state->signIndex[subframe] = (state->signIndex[subframe] << 1) | (signs[i] < 0);
		}

		/* the excitation into the history, and through the filters */
		for (i = 0; i < HISTORY - length; i++)
			state->history[i] = state->history[length + i];
		for (i = 0, p = 80; i < length; i++, p--) {
			int64_t excitation = (((int64_t)gain1 * acb[i]) >> 2) +
				(((int64_t)gain2 * fixed[i]) >> 12);
			int64_t synthesized = excitation << 2, shaped = 0;

			state->history[HISTORY - length + i] = (int32_t)(excitation >> 3);
			for (k = 0; k < order; k++) {
				shaped += (int64_t)numerator[k] * memory->synthesis[p + k] -
					(int64_t)denominator[k] * memory->weightedSynthesis[p + k];
				synthesized += (int64_t)lpc[k] * memory->synthesis[p + k];
			}
			memory->synthesis[p - 1] = (int32_t)(synthesized >> 16);
			memory->weightedSynthesis[p - 1] = (int32_t)((synthesized + shaped) >> 16);
		}
		for (i = 0; i < order; i++) {
			memory->synthesis[80 + i] = memory->synthesis[p + i];
			memory->weightedSynthesis[80 + i] = memory->weightedSynthesis[p + i];
			memory->weightInput[80 + i] = memory->weightInput[p + i];
			memory->weightOutput[80 + i] = memory->weightOutput[p + i];
		}
	}
}
