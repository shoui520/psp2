/*
 * One channel of SBR: its reset for new bands (0x851e3e-0x8520b8) and its
 * frame (0x851924-0x851ce6).
 */
#include "aac.h"

/*
 * New frequency bands: the overlap the history keeps is cleared and
 * rescaled where the low band moved, and the patches and limiter bands
 * are rebuilt. Nonzero if they cannot be.
 */
int32_t aacSbrStateReset(AacSbrState *state, AacSbrHeader *header,
	AacSbrPrevious *previous, int32_t *work, uint8_t created)
{
	int32_t **imag = (int32_t **)work, **real = (int32_t **)(work + 36);
	int32_t *limiterWork = work + 72;
	AacSbrBands *bands = header->bands;
	int32_t oldKx = state->synthesis.kx, newKx = bands->kx;
	int32_t *history = state->history;
	int32_t overlap, low, high, from, to, shift, slot, i, result;
	int32_t lowReset, highReset, fromReset, toReset;

	for (i = 0; i < 6; i++) {
		real[i] = history;
		history += 64;
		imag[i] = history;
		history += 64;
	}

	aacSbrAdjustReset(&state->adjust);
	bands = header->bands;
	state->synthesis.kx = bands->kx;
	state->synthesis.stop = bands->stop;
	state->analysis.kx = state->synthesis.kx;
	state->analysis.stop = state->synthesis.stop;

	/* the slots that overlap into this frame lose the bands below the new kx */
	overlap = (previous->lastBorder - header->timeSlots) * header->timeStep;
	for (slot = overlap; slot < 6; slot++) {
		for (i = oldKx; i < newKx; i++) {
			real[slot][i] = 0;
			imag[slot][i] = 0;
		}
	}

	low = aacMin(oldKx, newKx);
	high = aacMax(oldKx, newKx);
	for (i = low; i < high; i++) {
		state->hf.lpcReal[0][i] = 0;
		state->hf.lpcReal[1][i] = 0;
		state->hf.lpcImag[0][i] = 0;
		state->hf.lpcImag[1][i] = 0;
	}

	if (oldKx < newKx) {
		lowReset = 0;
		highReset = oldKx;
		from = state->timing[4];
		to = state->timing[2];
	} else {
		lowReset = oldKx;
		highReset = state->synthesis.stop;
		from = state->timing[2];
		to = state->timing[4];
	}

	if (created) {
		shift = aacSbrHeadroom(real, imag, low, high, 0, overlap);
		aacSbrRescale(real, imag, low, high, 0, overlap, shift);
	} else {
		shift = aacSbrHeadroomReal(real, low, high, 0, overlap);
		aacSbrRescaleReal(real, low, high, 0, overlap, shift);
	}

	/* the moved bands follow the scale of the bands they joined */
	fromReset = from + shift;
	toReset = to - fromReset;
	if (toReset >= 1) {
		shift = -toReset;
		if (oldKx < newKx)
			state->timing[2] = fromReset;
		else
			state->timing[4] = fromReset;
		low = lowReset;
		high = highReset;
	} else {
		shift = toReset;
	}
	for (i = 0; i < overlap; i++) {
		aacSbrScale(real[i] + low, high - low, shift);
		aacSbrScale(imag[i] + low, high - low, shift);
	}

	result = aacSbrBuildPatches(&state->hf, bands->kx, bands->master, bands->numMaster,
		bands->noise, bands->numNoise, bands->stop, header->outputRate);
	result |= aacSbrLimiterBands(bands->limiter, &bands->numLimiter, bands->table[0],
		bands->numLow, state->hf.patches->patch, state->hf.patches->numPatches,
		header->limiterBands, limiterWork);
	return result;
}

/*
 * A channel's frame: the core's samples in `output` are analysed into
 * the QMF matrix after the slots the last frame left over, the high band
 * is generated and adjusted to the envelopes, and the synthesis writes
 * the output. The matrix's rows live in the history, then in `low`,
 * `buffer` and `high` (the output itself unless parametric stereo needs
 * it for the second channel).
 */
void aacSbrChannelDecode(AacSbrState *state, int32_t *output, int32_t *low, int32_t *high,
	int32_t *buffer, AacSbrHeader *header, AacSbrFrame *frame, AacSbrPrevious *previous,
	int32_t synced, AacPs *ps, AacSbrQmf *psSynthesis, int32_t *psTiming,
	uint32_t downsampled, int32_t *work, uint8_t created)
{
	int32_t **real = (int32_t **)work, **imag = (int32_t **)(work + 38);
	int32_t *scratch = work + 76;
	int32_t numSlots = header->timeSlots * header->timeStep;
	int32_t half = numSlots >> 1, quarter = numSlots >> 2;
	int32_t *rows = state->history;
	int32_t analysed, shift, row = 0, end, i, j;
	AacSbrBands *bands;

	work += 140;
	for (i = 0; i < 6; i++, rows += 128) {
		real[row] = rows;
		imag[row++] = rows + 64;
	}
	for (rows = low + quarter * 128, i = 0; i < half - quarter; i++, rows += 128) {
		real[row] = rows;
		imag[row++] = rows + 64;
	}
	for (rows = buffer, i = 0; i < half; i++, rows += 128) {
		real[row] = rows;
		imag[row++] = rows + 64;
	}
	if (header->mode != 4)
		high = output;
	for (rows = high, i = 0; i < quarter; i++, rows += 128) {
		real[row] = rows;
		imag[row++] = rows + 64;
	}

	state->timing[0] = -1;
	aacSbrAnalysis(output, state->timing, real + 6, imag + 6, &state->analysis, work, created);

	/* the new slots take the history's scale while they fit */
	if (created) {
		shift = aacSbrHeadroom(real, imag, 0, state->analysis.kx, 6, numSlots + 6);
		if (state->timing[0] + shift < 56) {
			aacSbrRescale(real, imag, 0, state->analysis.kx, 6, numSlots + 6, shift);
			state->timing[0] += shift;
		}
	} else {
		shift = aacSbrHeadroomReal(real, 0, state->analysis.kx, 6, numSlots + 6);
		if (state->timing[0] + shift < 56) {
			aacSbrRescaleReal(real, 0, state->analysis.kx, 6, numSlots + 6, shift);
			state->timing[0] += shift;
		}
	}
	analysed = state->timing[0];

	if (synced) {
		const AacSbrGrid *grid = &frame->grid;

		end = (uint8_t)(grid->borders[grid->numEnvelopes] - header->timeSlots);
		bands = header->bands;
		for (i = bands->kx; i < header->bands->stop; i++)
			scratch[i] = 0;
		bands = header->bands;
		if (created)
			aacSbrHfGenerate(&state->hf, state->timing, real, imag, header->timeStep,
				grid->borders[0], end, bands->noiseBands, frame->invfMode,
				previous->invfMode, work);
		else
			aacSbrHfGenerateReal(&state->hf, state->timing, real, scratch, header->timeStep,
				grid->borders[0], end, bands->noiseBands, frame->invfMode,
				previous->invfMode, work);
		aacSbrAdjustEnvelopes(state->timing, &state->adjust, header, frame, real, imag,
			scratch, work, created);

		for (i = 0; i < header->bands->noiseBands; i++)
			previous->invfMode[i] = frame->invfMode[i];
		previous->coupling = frame->coupling;
		previous->lastBorder = grid->borders[grid->numEnvelopes];
		previous->ampResolution = frame->ampResolution;
	} else {
		state->timing[3] = analysed;
	}

	/* the LPC's history: the low band of the frame's last two slots */
	for (i = 0; i < 2; i++) {
		for (j = 0; j < state->analysis.kx; j++) {
			state->hf.lpcReal[i][j] = real[numSlots + i - 2][j];
			state->hf.lpcImag[i][j] = imag[numSlots + i - 2][j];
		}
	}

	if (header->mode == 4) {
		aacPsApply(ps, state->timing);
		aacSbrSynthesis(real, imag, 6, state->timing, low - numSlots * 64,
			&state->synthesis, ps, 1, downsampled, work);
		psTiming[2] = state->timing[6];
		psTiming[0] = state->timing[6];
		psTiming[3] = state->timing[6];
		aacSbrSynthesis(real, imag, 6, psTiming, low, psSynthesis, ps, 0, downsampled,
			work);
	} else {
		aacSbrSynthesis(real, imag, 6, state->timing, low, &state->synthesis, ps, 0,
			downsampled, work);
	}

	/* the last 6 slots overlap into the next frame */
	for (i = 0; i < 6; i++) {
		aacSbrCopy((int8_t *)real[i], (const int8_t *)real[numSlots + i], 256);
		aacSbrCopy((int8_t *)imag[i], (const int8_t *)imag[numSlots + i], 256);
	}
	state->timing[2] = analysed;
}

/*
 * 0x853dbc: The synthesis bank over a frame's QMF slots into `output`. The low band
 * from `first` on and the high band each carry their own scale (timing);
 * with parametric stereo the first channel's slots are mixed here, and
 * the second's left in the matrix for its own synthesis.
 */
void aacSbrSynthesis(int32_t **real, int32_t **imag, int32_t first, int32_t *timing,
	int32_t *output, AacSbrQmf *qmf, AacPs *ps, int32_t psChannel, uint32_t downsampled,
	int32_t *work)
{
	int32_t *left = work, *right = work + 64, *scratch = work + 128;
	uint8_t created = qmf->created;
	int32_t reference, high, lowShift, highShift, outputShift, shift;
	int32_t envelope = 0, slot;

	if (psChannel) {
		reference = timing[6];
		high = timing[6];
	} else {
		reference = aacMin(timing[0], timing[2]);
		high = timing[3];
	}
	lowShift = timing[5] - reference;
	highShift = timing[5] - high;
	outputShift = 1 - timing[5];
	if (downsampled)
		outputShift--;

	for (slot = 0; slot < qmf->numSlots; slot++) {
		if (psChannel) {
			if (ps->borders[envelope] == slot) {
				aacPsUpdate(ps, envelope, qmf->stop);
				envelope++;
			}
			aacPsMix(ps, &real[slot], &imag[slot], left, right, timing, slot, qmf->kx,
				scratch);
			shift = lowShift;
		} else {
			shift = reference - (slot < first ? timing[2] : timing[0]) + lowShift;
		}
		if (created)
			aacSbrSynthesisSlot(real[slot], imag[slot], shift, highShift, outputShift,
				qmf, scratch, downsampled);
		else
			aacSbrSynthesisSlotReal(real[slot], imag[slot], shift, highShift,
				outputShift, qmf, scratch, downsampled);
		aacSbrSynthesisOutput(output + slot * qmf->numBands, qmf, downsampled);
		if (psChannel) {
			aacSbrCopy((int8_t *)real[slot], (const int8_t *)left,
				(qmf->numBands >> 4) * 64);
			aacSbrCopy((int8_t *)imag[slot], (const int8_t *)right,
				(qmf->numBands >> 4) * 64);
		}
	}
}
