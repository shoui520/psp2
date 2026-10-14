/*
 * The CELP encoder's line spectrum pairs (0x8556a6-0x856d76): the LPC
 * coefficients of the analysis to LSP angles (Chebyshev root search), the
 * quantiser, and the interpolation to every subframe's LPC.
 */
#include "celp.h"

#define CHEBYSHEV_ONE			0x800000
#define CHEBYSHEV_LIMIT			(-0x800000)

/* a polynomial in Chebyshev form at x (Q23), by the Clenshaw recursion */
static int32_t chebyshev(const int32_t *g, int32_t length, int32_t x)
{
	int32_t first = 0, second = 0, i;

	for (i = length - 1; i >= 0; i--) {
		int32_t next = (int32_t)((int64_t)first * x * 2 >> 23) - second + g[i];

		second = first;
		first = next;
	}
	return (g[0] + first - second) >> 1;
}

/*
 * 0x8556a6: the `n` LSP angles (Q16 radians, ascending in frequency) of the
 * LPC a[1..n] (a[0] is not read): the sum and difference polynomials are
 * searched from x = 1 down to -1 for sign changes, each root narrowed by
 * four bisections and a linear interpolation, alternating between the two.
 */
static void lpcToLsp(int32_t n, int32_t *angle, const int32_t *a)
{
	int32_t sum[CELP_MAX_ORDER / 2 + 1], difference[CELP_MAX_ORDER / 2 + 1];
	int32_t gSum[CELP_MAX_ORDER / 2 + 1], gDifference[CELP_MAX_ORDER / 2 + 1];
	int32_t *poly, *other;
	int32_t half = (n >> 1) + 1;
	int32_t count = 0, low, lowValue, last, i;

	sum[0] = CHEBYSHEV_ONE;
	difference[0] = CHEBYSHEV_ONE;
	for (i = 1; i < half; i++) {
		sum[i] = -(a[i] + a[n + 1 - i]);
		difference[i] = -(a[i] - a[n + 1 - i]);
	}
	for (i = 0; i < half - 1; i++) {
		sum[i + 1] -= sum[i];
		difference[i + 1] += difference[i];
	}
	gSum[0] = sum[half - 1];
	gDifference[0] = difference[half - 1];
	for (i = 1; i < half; i++) {
		gSum[i] = sum[half - 1 - i] << 1;
		gDifference[i] = difference[half - 1 - i] << 1;
	}
	poly = gSum;
	other = gDifference;
	low = CHEBYSHEV_ONE;
	lowValue = chebyshev(poly, half, low);
	last = 0x1000000;
	while (count < n) {
		int32_t square = (int32_t)((int64_t)low * low >> 23);
		int32_t step = (int32_t)(((int64_t)square * -0x3c497 >> 23) + 0x40545);
		int32_t high = max(low - step, CHEBYSHEV_LIMIT);
		int32_t highValue = chebyshev(poly, half, high);
		int64_t product = (int64_t)highValue * lowValue;

		if ((int32_t)(product >> 32) >= 0 && (uint32_t)product != 0) {
			/* no sign change: on towards -1 */
			low = high;
			lowValue = highValue;
		} else {
			int32_t width = low - high, from = high, fromValue = highValue, toValue = lowValue;
			int32_t root, scaled, tmp, *swap;

			for (i = 1; i < 5; i++) {
				int32_t middle, middleValue;

				width >>= 1;
				middle = from + width;
				middleValue = chebyshev(poly, half, middle);
				if ((int32_t)(middleValue * fromValue) >= 1) {
					from = middle;
					fromValue = middleValue;
				} else {
					toValue = middleValue;
				}
			}
			if (toValue != fromValue)
				root = (int32_t)((int64_t)width * fromValue / (int64_t)(fromValue - toValue)) + from;
			else
				root = from + width;
			scaled = (int16_t)max((int16_t)(root >> 8), -32767) << 1;
			if (scaled >= 0)
				tmp = celpAcos(scaled) + 1;
			else
				tmp = 0x3243f - celpAcos(-scaled) - 1;
			angle[count++] = tmp >> 1;
			if (!(root < last))
				root = from - width;
			last = root;
			swap = poly;
			poly = other;
			other = swap;
			low = root;
			lowValue = chebyshev(poly, half, low);
		}
		if (low < -0x7fffff)
			break;
	}
}

/* 0x85675a: the angles of the analysis' last block as fractions of pi (Q16) */
void celpEncLpcToLsp(CelpEncoderState *state, int32_t *angle)
{
	int32_t i;

	lpcToLsp(state->order, angle, &state->analysisLpc[(state->analysisBlocks - 1) * state->order - 1]);
	for (i = 0; i < state->order; i++)
		angle[i] = (int32_t)((int64_t)angle[i] * 20861 >> 16);
}

#define LSP_HALF			5
#define CELP_ENC_BIG			0x7fffffff

/* the weighted squared distance of a stage-two residual to a codebook row (plus and minus) */
typedef struct LspErrors {
	int32_t minus;
	int32_t plus;
} LspErrors;

/*
 * 0x855cf6: the LSP indices. The target `lsp` (Q15 fractions of pi) is
 * matched by two stages for each half: a first codebook of 16 rows (the two
 * best of each half are kept), then the residual against 64 (low half) or 32
 * (high half) rows of the second, added or subtracted; and the same with
 * the first stage averaged with the previous frame's LSPs and the predicted
 * second codebooks. The better of the plain and the predicted LSP gives
 * the indices: [0] and [1] the first stages, [2] and [3] the second (64 or
 * 32 added to a row that is subtracted), [4] 1 if predicted.
 */
static void searchLsp(const int32_t *lsp, int32_t *indices, const int32_t *weight, const int32_t *previous)
{
	int32_t errorA[16] = { 0 }, errorB[16] = { 0 };
	int32_t bestA[2], bestB[2];
	int32_t bestMinusC[2], bestPlusC[2], bestMinusE[2], bestPlusE[2];
	int32_t bestMinusD[2], bestPlusD[2], bestMinusF[2], bestPlusF[2];
	int32_t indexC[2], indexD[2], indexE[2], indexF[2];
	int32_t valueC[2], valueD[2], valueE[2], valueF[2];
	int32_t low[LSP_HALF], high[LSP_HALF], residual[10], predicted[10];
	int32_t pass, row, k;

	for (row = 0; row < 16; row++) {
		for (k = 0; k < LSP_HALF; k++) {
			int32_t lowDifference = lsp[k] - celpEncLspFirst[row][k];
			int32_t highDifference = lsp[LSP_HALF + k] - celpEncLspSecond[row][k];

			errorA[row] += (int32_t)((int64_t)lowDifference * weight[k] * lowDifference >> 29);
			errorB[row] += (int32_t)((int64_t)highDifference * weight[LSP_HALF + k] * highDifference >> 29);
		}
	}
	for (pass = 0; pass < 2; pass++) {
		LspErrors c[64], e[64], d[32], f[32];

		bestA[pass] = 0;
		bestB[pass] = 0;
		for (row = 1; row < 16; row++) {
			if (errorA[row] < errorA[bestA[pass]])
				bestA[pass] = row;
			if (errorB[row] < errorB[bestB[pass]])
				bestB[pass] = row;
		}
		errorA[bestA[pass]] = CELP_ENC_BIG;
		errorB[bestB[pass]] = CELP_ENC_BIG;
		for (k = 0; k < LSP_HALF; k++) {
			low[k] = celpEncLspFirst[bestA[pass]][k];
			high[k] = celpEncLspSecond[bestB[pass]][k];
			residual[k] = lsp[k] - low[k];
			residual[LSP_HALF + k] = lsp[LSP_HALF + k] - high[k];
			predicted[k] = lsp[k] - ((previous[k] + low[k]) >> 1);
			predicted[LSP_HALF + k] = lsp[LSP_HALF + k] - ((previous[LSP_HALF + k] + high[k]) >> 1);
		}
		bestMinusC[pass] = 0;
		bestPlusC[pass] = 0;
		bestMinusE[pass] = 0;
		bestPlusE[pass] = 0;
		for (row = 0; row < 64; row++) {
			c[row].minus = c[row].plus = e[row].minus = e[row].plus = 0;
			for (k = 0; k < LSP_HALF; k++) {
				int32_t plusC = celpEncLspThird[row][k] + (residual[k] << 3);
				int32_t minusC = (residual[k] << 3) - celpEncLspThird[row][k];
				int32_t plusE = celpEncLspThirdPredicted[row][k] + (predicted[k] << 4);
				int32_t minusE = (predicted[k] << 4) - celpEncLspThirdPredicted[row][k];

				c[row].minus += (int32_t)((int64_t)minusC * minusC * weight[k] >> 34);
				c[row].plus += (int32_t)((int64_t)plusC * plusC * weight[k] >> 34);
				e[row].minus += (int32_t)((int64_t)minusE * minusE * weight[k] >> 36);
				e[row].plus += (int32_t)((int64_t)plusE * plusE * weight[k] >> 36);
			}
			if (c[row].minus < c[bestMinusC[pass]].minus)
				bestMinusC[pass] = row;
			if (c[row].plus < c[bestPlusC[pass]].plus)
				bestPlusC[pass] = row;
			if (e[row].minus < e[bestMinusE[pass]].minus)
				bestMinusE[pass] = row;
			if (e[row].plus < e[bestPlusE[pass]].plus)
				bestPlusE[pass] = row;
		}
		valueC[pass] = c[bestMinusC[pass]].minus;
		indexC[pass] = bestMinusC[pass];
		if (c[bestPlusC[pass]].plus < c[bestMinusC[pass]].minus) {
			valueC[pass] = c[bestPlusC[pass]].plus;
			indexC[pass] = bestPlusC[pass] + 64;
		}
		valueE[pass] = e[bestMinusE[pass]].minus;
		indexE[pass] = bestMinusE[pass];
		if (e[bestPlusE[pass]].plus < e[bestMinusE[pass]].minus) {
			valueE[pass] = e[bestPlusE[pass]].plus;
			indexE[pass] = bestPlusE[pass] + 64;
		}
		bestMinusD[pass] = 0;
		bestPlusD[pass] = 0;
		bestMinusF[pass] = 0;
		bestPlusF[pass] = 0;
		for (row = 0; row < 32; row++) {
			d[row].minus = d[row].plus = f[row].minus = f[row].plus = 0;
			for (k = 0; k < LSP_HALF; k++) {
				int32_t plusD = celpEncLspFourth[row][k] + (residual[LSP_HALF + k] << 3);
				int32_t minusD = (residual[LSP_HALF + k] << 3) - celpEncLspFourth[row][k];
				int32_t plusF = celpEncLspFourthPredicted[row][k] + (predicted[LSP_HALF + k] << 4);
				int32_t minusF = (predicted[LSP_HALF + k] << 4) - celpEncLspFourthPredicted[row][k];

				d[row].minus += (int32_t)((int64_t)minusD * minusD * weight[LSP_HALF + k] >> 34);
				d[row].plus += (int32_t)((int64_t)plusD * plusD * weight[LSP_HALF + k] >> 34);
				f[row].minus += (int32_t)((int64_t)minusF * minusF * weight[LSP_HALF + k] >> 36);
				f[row].plus += (int32_t)((int64_t)plusF * plusF * weight[LSP_HALF + k] >> 36);
			}
			if (d[row].minus < d[bestMinusD[pass]].minus)
				bestMinusD[pass] = row;
			if (d[row].plus < d[bestPlusD[pass]].plus)
				bestPlusD[pass] = row;
			if (f[row].minus < f[bestMinusF[pass]].minus)
				bestMinusF[pass] = row;
			if (f[row].plus < f[bestPlusF[pass]].plus)
				bestPlusF[pass] = row;
		}
		valueD[pass] = d[bestMinusD[pass]].minus;
		indexD[pass] = bestMinusD[pass];
		if (d[bestPlusD[pass]].plus < d[bestMinusD[pass]].minus) {
			valueD[pass] = d[bestPlusD[pass]].plus;
			indexD[pass] = bestPlusD[pass] + 32;
		}
		valueF[pass] = f[bestMinusF[pass]].minus;
		indexF[pass] = bestMinusF[pass];
		if (f[bestPlusF[pass]].plus < f[bestMinusF[pass]].minus) {
			valueF[pass] = f[bestPlusF[pass]].plus;
			indexF[pass] = bestPlusF[pass] + 32;
		}
	}
	{
		int32_t c0 = 0, d0 = 0, e0 = 0, f0 = 0;

		for (pass = 1; pass < 2; pass++) {
			if (valueC[pass] < valueC[c0])
				c0 = pass;
			if (valueD[pass] < valueD[d0])
				d0 = pass;
			if (valueE[pass] < valueE[e0])
				e0 = pass;
			if (valueF[pass] < valueF[f0])
				f0 = pass;
		}
		if (valueD[d0] + valueC[c0] < valueF[f0] + valueE[e0]) {
			indices[0] = bestA[c0];
			indices[1] = bestB[d0];
			indices[2] = indexC[c0];
			indices[3] = indexD[d0];
			indices[4] = 0;
		} else {
			indices[0] = bestA[e0];
			indices[1] = bestB[f0];
			indices[2] = indexE[e0];
			indices[3] = indexF[f0];
			indices[4] = 1;
		}
	}
}

/*
 * 0x8567d2: the LSP angles `angle` (Q15 fractions of pi) quantised to the
 * LSPs `lsp` and the indices (indices[0..4], as searchLsp's); the search
 * is weighted by the reciprocals of the distances between neighbours.
 */
void celpEncQuantizeLsp(CelpEncoderState *state, int32_t *angle, int32_t *lsp, int32_t *indices)
{
	int32_t distance[CELP_ORDER + 1], weight[CELP_ORDER];
	int32_t i, k, n;

	distance[0] = angle[0];
	distance[CELP_ORDER] = 32768 - angle[CELP_ORDER - 1];
	for (i = 0; i < CELP_ORDER - 1; i++)
		distance[i + 1] = angle[i + 1] - angle[i];
	for (i = 0; i <= CELP_ORDER; i++) {
		distance[i] = max(distance[i], 256);
		distance[i] = (int32_t)(0x100000000LL / distance[i]);
	}
	for (i = 0; i < CELP_ORDER; i++)
		weight[i] = distance[i] + distance[i + 1];
	searchLsp(angle, indices, weight, state->previousLsp);

	if (indices[4] == 0) {
		if (indices[2] < 64)
			for (k = 0; k < LSP_HALF; k++)
				lsp[k] = ((celpEncLspFirst[indices[0]][k] << 3) + celpEncLspThird[indices[2]][k]) >> 3;
		else
			for (k = 0; k < LSP_HALF; k++)
				lsp[k] = ((celpEncLspFirst[indices[0]][k] << 3) - celpEncLspThird[indices[2] - 64][k]) >> 3;
		if (indices[3] < 32)
			for (k = 0; k < LSP_HALF; k++)
				lsp[LSP_HALF + k] = ((celpEncLspSecond[indices[1]][k] << 3) + celpEncLspFourth[indices[3]][k]) >> 3;
		else
			for (k = 0; k < LSP_HALF; k++)
				lsp[LSP_HALF + k] = ((celpEncLspSecond[indices[1]][k] << 3) - celpEncLspFourth[indices[3] - 32][k]) >> 3;
	} else {
		if (indices[2] < 64)
			for (k = 0; k < LSP_HALF; k++)
				lsp[k] = ((((celpEncLspFirst[indices[0]][k] + state->previousLsp[k]) >> 1) << 4)
					+ celpEncLspThirdPredicted[indices[2]][k]) >> 4;
		else
			for (k = 0; k < LSP_HALF; k++)
				lsp[k] = ((((celpEncLspFirst[indices[0]][k] + state->previousLsp[k]) >> 1) << 4)
					- celpEncLspThirdPredicted[indices[2] - 64][k]) >> 4;
		if (indices[3] < 32)
			for (k = 0; k < LSP_HALF; k++)
				lsp[LSP_HALF + k] = ((((celpEncLspSecond[indices[1]][k] + state->previousLsp[LSP_HALF + k]) >> 1) << 4)
					+ celpEncLspFourthPredicted[indices[3]][k]) >> 4;
		else
			for (k = 0; k < LSP_HALF; k++)
				lsp[LSP_HALF + k] = ((((celpEncLspSecond[indices[1]][k] + state->previousLsp[LSP_HALF + k]) >> 1) << 4)
					- celpEncLspFourthPredicted[indices[3] - 32][k]) >> 4;
	}
	n = state->order;
	for (i = n - 1; i >= 1; i--) {
		int32_t sorted = 1;

		for (k = 0; k < i; k++) {
			if (lsp[k + 1] < lsp[k]) {
				int32_t swap = lsp[k + 1];

				lsp[k + 1] = lsp[k];
				lsp[k] = swap;
				sorted = 0;
			}
		}
		if (sorted)
			break;
	}
	for (i = 0; i < n; i++)
		lsp[i] = max(lsp[i], 256);
	for (i = 0; i < n - 1; i++)
		if (lsp[i + 1] < lsp[i] + 256)
			lsp[i + 1] = lsp[i] + 256;
	for (i = 0; i < n; i++)
		lsp[i] = min(lsp[i], 32512);
	for (i = n - 2; i >= 0; i--)
		if (lsp[i + 1] - 256 < lsp[i])
			lsp[i] = lsp[i + 1] - 256;
}

/*
 * 0x855ada: the LPC coefficients (Q12) of `n` LSPs (Q16 fractions of pi) in
 * `lpc`: the polynomials of the even and odd LSPs, two at a time, then
 * half their sum and difference.
 */
static void lspToLpc(int32_t n, const int32_t *lsp, int32_t *lpc)
{
	int32_t c[CELP_MAX_ORDER], a[CELP_MAX_ORDER + 1], b[CELP_MAX_ORDER + 1];
	int32_t i, j, k, u, v;

	for (i = 0; i < n; i++)
		c[i] = -(celpCos(lsp[i] << 1) << 1);
	for (i = 0; i < n; i++) {
		a[i] = 0;
		b[i] = 0;
	}
	a[n] = 0;
	b[n] = 0;
	for (k = 0; k <= n; k++) {
		int32_t sum;

		u = (k == 0) << 16;
		v = u;
		for (j = 0; j < n; j += 2) {
			int32_t a0 = a[j], a1 = a[j + 1], b0 = b[j], b1 = b[j + 1];
			int64_t product0 = (int64_t)c[j] * a0, product1 = (int64_t)c[j + 1] * a1;

			b[j] = a0;
			b[j + 1] = a1;
			a[j] = u;
			a[j + 1] = v;
			u += (int32_t)(product0 >> 16) + b0;
			v += (int32_t)(product1 >> 16) + b1;
		}
		sum = a[j] - b[j] + u + v;
		a[j] = u;
		b[j] = v;
		if (k >= 1)
			lpc[k - 1] = -(sum >> 1);
	}
}

/* 0x856c24: the quantised LSPs interpolated from the previous frame's and turned into every subframe's LPC */
void celpEncInterpolateLsp(CelpEncoderState *state)
{
	const int16_t *weights;
	int32_t subframe, k, index;

	index = (int16_t)(state->subframes - 2);
	if (state->subframes == 4)
		index = (int16_t)(index + 1);
	weights = &celpEncLspWeights[index];
	for (subframe = 0; subframe < state->subframes; subframe++) {
		int32_t interpolated[CELP_MAX_ORDER];

		if (subframe < state->subframes - 1) {
			int32_t weight = *weights++;

			for (k = 0; k < state->order; k++) {
				int32_t difference = state->lsp[k] - state->previousLsp[k];

				interpolated[k] = state->previousLsp[k] + (int32_t)((int64_t)difference * weight >> 15);
			}
		} else {
			for (k = 0; k < state->order; k++) {
				interpolated[k] = state->lsp[k];
				state->previousLsp[k] = state->lsp[k];
			}
		}
		for (k = 0; k < state->order; k++)
			interpolated[k] = (int32_t)((int64_t)celpEncPi[0] * interpolated[k] >> 15);
		lspToLpc(state->order, interpolated, &state->lpc[subframe * state->order]);
	}
}
