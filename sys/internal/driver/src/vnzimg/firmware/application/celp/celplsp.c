/*
 * The CELP decoder's frame header and line spectrum pairs (0x8c4678-0x8c5930):
 * the indices, the LSP vector dequantised from the two codebook pairs
 * (optionally predicted from the previous frame), its ordering and spacing,
 * the interpolation across the subframes and the conversion to LPC.
 */
#include "celp.h"

#define LSP_HALF			5
#define LSP_MINIMUM_GAP			0x20000
#define LSP_MAXIMUM			0xfe0000
#define PI_Q24				0x3243f6a

/* 0x8c4678: the header and the subframes' indices */
void celpDecodeHeader(CelpDecoderState *state)
{
	int32_t subframe;

	state->lspIndex[0] = celpBitsGet(state, 4);
	state->lspIndex[1] = celpBitsGet(state, 4);
	state->lspIndex[2] = celpBitsGet(state, 7);
	state->lspIndex[3] = celpBitsGet(state, 6);
	state->lspPredicted = celpBitsGet(state, 1);
	state->gainMode = celpBitsGet(state, 2);
	state->gainIndex = celpBitsGet(state, 6);
	for (subframe = 0; subframe < state->subframes; subframe++) {
		state->lagIndex[subframe] = celpBitsGet(state, 8);
		state->positionIndex[subframe] = celpBitsGetWide(state,
			celpPositionBits[state->bitRateIndex]);
		state->signIndex[subframe] = celpBitsGet(state, celpSignBits[state->bitRateIndex]);
		state->gainCode[subframe] = celpBitsGet(state, 6);
	}
	celpBitsAlign(state);
}

/* the first and second halves of the LSPs from the codebooks */
static void dequantize(CelpDecoderState *state)
{
	int32_t *lsp = state->lsp;
	int32_t index0 = state->lspIndex[0], index1 = state->lspIndex[1];
	int32_t index2 = state->lspIndex[2], index3 = state->lspIndex[3];
	int32_t k;

	if (!state->lspPredicted) {
		if (index2 < 64)
			for (k = 0; k < LSP_HALF; k++)
				lsp[k] = celpLspFirst[index0][k] + celpLspThird[index2][k];
		else
			for (k = 0; k < LSP_HALF; k++)
				lsp[k] = celpLspFirst[index0][k] - celpLspThird[index2 - 64][k];
		if (index3 < 32)
			for (k = 0; k < LSP_HALF; k++)
				lsp[LSP_HALF + k] = celpLspSecond[index1][k] + celpLspFourth[index3][k];
		else
			for (k = 0; k < LSP_HALF; k++)
				lsp[LSP_HALF + k] = celpLspSecond[index1][k] - celpLspFourth[index3 - 32][k];
		return;
	}
	if (index2 < 64)
		for (k = 0; k < LSP_HALF; k++)
			lsp[k] = celpLspThirdPredicted[index2][k]
				+ ((celpLspFirst[index0][k] + state->previousLsp[k]) >> 1);
	else
		for (k = 0; k < LSP_HALF; k++)
			lsp[k] = ((celpLspFirst[index0][k] + state->previousLsp[k]) >> 1)
				- celpLspThirdPredicted[index2 - 64][k];
	if (index3 < 32)
		for (k = 0; k < LSP_HALF; k++)
			lsp[LSP_HALF + k] = celpLspFourthPredicted[index3][k]
				+ ((celpLspSecond[index1][k] + state->previousLsp[LSP_HALF + k]) >> 1);
	else
		for (k = 0; k < LSP_HALF; k++)
			lsp[LSP_HALF + k] = ((celpLspSecond[index1][k] + state->previousLsp[LSP_HALF + k]) >> 1)
				- celpLspFourthPredicted[index3 - 32][k];
}

/* the LSPs ascending, at least a gap apart and below the maximum */
static void sortLsp(CelpDecoderState *state)
{
	int32_t *lsp = state->lsp;
	int32_t n = state->order, pass, i, sorted, swap;

	for (pass = n - 1; pass >= 1; pass--) {
		sorted = 1;
		for (i = 0; i < pass; i++) {
			if (lsp[i + 1] < lsp[i]) {
				sorted = 0;
				swap = lsp[i + 1];
				lsp[i + 1] = lsp[i];
				lsp[i] = swap;
			}
		}
		if (sorted)
			break;
	}
	for (i = 0; i < n; i++)
		lsp[i] = max(lsp[i], LSP_MINIMUM_GAP);
	for (i = 0; i < n - 1; i++)
		if (lsp[i + 1] < lsp[i] + LSP_MINIMUM_GAP)
			lsp[i + 1] = lsp[i] + LSP_MINIMUM_GAP;
	for (i = 0; i < n; i++)
		lsp[i] = min(lsp[i], LSP_MAXIMUM);
	for (i = n - 1; i >= 1; i--)
		if (lsp[i] - LSP_MINIMUM_GAP < lsp[i - 1])
			lsp[i - 1] = lsp[i] - LSP_MINIMUM_GAP;
}

/*
 * 0x8c48b8: the LPC coefficients (Q15) of `n` angles (Q24 radians) into
 * state->lpc, from `offset` words on: two polynomials built a pair of angles
 * at a time, then their mean.
 */
static void lspToLpc(CelpDecoderState *state, int32_t n, const int32_t *angle, int32_t offset)
{
	int32_t c[CELP_MAX_ORDER], a[CELP_MAX_ORDER + 1], b[CELP_MAX_ORDER + 1], q[CELP_MAX_ORDER + 1];
	int32_t i, j, k, u, v;

	for (i = 0; i < n; i++)
		c[i] = -(celpCos(angle[i] >> 8) << 9);
	for (i = 0; i < n; i++) {
		a[i] = 0;
		b[i] = 0;
	}
	a[n] = 0;
	b[n] = 0;
	u = 0x1000000;
	v = 0x1000000;
	for (k = 0; k <= n; k++) {
		for (j = 0; j < n; j += 2) {
			int32_t a0 = a[j], a1 = a[j + 1], b0 = b[j], b1 = b[j + 1];
			int32_t nextU = u + b0 + (int32_t)(((int64_t)c[j] * a0) >> 24);
			int32_t nextV = v + b1 + (int32_t)(((int64_t)c[j + 1] * a1) >> 24);

			b[j] = a0;
			b[j + 1] = a1;
			a[j] = u;
			a[j + 1] = v;
			u = nextU;
			v = nextV;
		}
		q[k] = (a[j] + u + (v - b[j])) >> 1;
		a[j] = u;
		b[j] = v;
		u = 0;
		v = 0;
	}
	for (i = 0; i < n; i++)
		state->lpc[offset + i] = -(q[1 + i] >> 9);
}

/* 0x8c4ce0: the LSPs of the frame and the LPC of every subframe */
void celpDecodeLsp(CelpDecoderState *state)
{
	int32_t current[CELP_MAX_ORDER], interpolated[CELP_MAX_ORDER];
	int32_t subframe, k;

	dequantize(state);
	sortLsp(state);
	for (k = 0; k < state->order; k++)
		current[k] = state->lsp[k];
	for (subframe = 0; subframe < state->subframes; subframe++) {
		if (subframe < state->subframes - 1) {
			int32_t weight = (subframe + 1) * celpSubframeWeights[state->subframes];

			for (k = 0; k < state->order; k++) {
				int32_t difference = current[k] - state->previousLsp[k];

				interpolated[k] = state->previousLsp[k] + (int32_t)(((int64_t)difference * weight) >> 24);
			}
		} else {
			for (k = 0; k < state->order; k++) {
				interpolated[k] = current[k];
				state->previousLsp[k] = current[k];
			}
		}
		for (k = 0; k < state->order; k++)
			interpolated[k] = (int32_t)(((int64_t)interpolated[k] * PI_Q24) >> 24);
		lspToLpc(state, state->order, interpolated, state->order * subframe);
	}
}
