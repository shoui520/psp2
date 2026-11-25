#include "g729_internal.h"

void g729Autocorr(const float *samples, int32_t order, float *corr)
{
	float windowed[G729_LPC_WINDOW_LEN];
	for (int i = 0; i < G729_LPC_WINDOW_LEN; i++)
		windowed[i] = samples[i] * g_g729HamWindow[i];

	for (int lag = 0; lag <= order; lag++) {
		float sum = 0.0f;
		for (int i = 0; i < G729_LPC_WINDOW_LEN - lag; i++)
			sum += windowed[i] * windowed[i + lag];
		corr[lag] = sum;
	}
	if (corr[0] < 1.0f)
		corr[0] = 1.0f;
}

void g729LagWindow(int32_t order, float *corr)
{
	corr[0] *= 1.0001f;
	for (int i = 1; i <= order; i++)
		corr[i] *= g_g729LagWindow[i - 1];
}

float g729Levinson(const float *corr, float *lpc, float *refl, int32_t order)
{
	float prevLpc[G729_LPC_ORDER_BWD + 1];
	float err = corr[0];

	lpc[0] = 1.0f;
	for (int i = 1; i <= order; i++)
		lpc[i] = 0.0f;

	if (err <= 1e-6f) {
		for (int i = 0; i < order; i++)
			refl[i] = 0.0f;
		return 1.0f;
	}

	for (int step = 1; step <= order; step++) {
		float acc = corr[step];
		for (int j = 1; j < step; j++)
			acc += lpc[j] * corr[step - j];

		float rc = -acc / err;
		if (rc > 0.999f)
			rc = 0.999f;
		if (rc < -0.999f)
			rc = -0.999f;
		refl[step - 1] = rc;

		for (int j = 1; j < step; j++)
			prevLpc[j] = lpc[j] + rc * lpc[step - j];
		for (int j = 1; j < step; j++)
			lpc[j] = prevLpc[j];
		lpc[step] = rc;

		err *= (1.0f - rc * rc);
		if (err <= 1e-6f)
			err = 1e-6f;
	}
	return err;
}

static float g729ChebEval(float xVal, const float *poly, int32_t degree)
{
	float prevTwo = 0.0f;
	float prevOne = 1.0f;
	for (int i = 1; i < degree; i++) {
		float cur = 2.0f * xVal * prevOne - prevTwo + poly[i];
		prevTwo = prevOne;
		prevOne = cur;
	}
	return xVal * prevOne - prevTwo + 0.5f * poly[degree];
}

void g729AzLsp(const float *lpc, float *lsp, const float *oldLsp)
{
	float symPoly[G729_NC_ORDER + 1];
	float antiPoly[G729_NC_ORDER + 1];

	symPoly[0] = 1.0f;
	antiPoly[0] = 1.0f;
	for (int i = 1; i <= G729_NC_ORDER; i++) {
		symPoly[i] = lpc[i] + lpc[G729_LPC_ORDER + 1 - i] - symPoly[i - 1];
		antiPoly[i] = lpc[i] - lpc[G729_LPC_ORDER + 1 - i] + antiPoly[i - 1];
	}

	int32_t rootsFound = 0;
	const float *curPoly = symPoly;
	float xLow = g_g729ChebGrid[0];
	float yLow = g729ChebEval(xLow, curPoly, G729_NC_ORDER);

	for (int idx = 1; idx <= 60 && rootsFound < G729_LPC_ORDER; idx++) {
		float xHigh = xLow;
		float yHigh = yLow;
		xLow = g_g729ChebGrid[idx];
		yLow = g729ChebEval(xLow, curPoly, G729_NC_ORDER);

		if (yLow * yHigh <= 0.0f) {
			float lo = xLow;
			float hi = xHigh;
			float yLo = yLow;
			for (int iter = 0; iter < 4; iter++) {
				float mid = 0.5f * (lo + hi);
				float yMid = g729ChebEval(mid, curPoly, G729_NC_ORDER);
				if (yMid * yLo <= 0.0f) {
					hi = mid;
				} else {
					lo = mid;
					yLo = yMid;
				}
			}
			lsp[rootsFound++] = 0.5f * (lo + hi);
			curPoly = (rootsFound & 1) ? antiPoly : symPoly;
			yLow = g729ChebEval(xLow, curPoly, G729_NC_ORDER);
		}
	}

	if (rootsFound != G729_LPC_ORDER) {
		for (int i = 0; i < G729_LPC_ORDER; i++)
			lsp[i] = oldLsp[i];
	}
}

static void g729GetLspPoly(const float *lsp, float *poly)
{
	poly[0] = 1.0f;
	poly[1] = -2.0f * lsp[0];
	for (int i = 2; i <= G729_NC_ORDER; i++) {
		float factor = -2.0f * lsp[2 * i - 2];
		poly[i] = factor * poly[i - 1] + 2.0f * poly[i - 2];
		for (int j = i - 1; j >= 2; j--)
			poly[j] += factor * poly[j - 1] + poly[j - 2];
		poly[1] += factor;
	}
}

void g729LspAz(const float *lsp, float *lpc)
{
	float evenPoly[G729_NC_ORDER + 1];
	float oddPoly[G729_NC_ORDER + 1];

	g729GetLspPoly(&lsp[0], evenPoly);
	g729GetLspPoly(&lsp[1], oddPoly);

	for (int i = G729_NC_ORDER; i > 0; i--) {
		evenPoly[i] += evenPoly[i - 1];
		oddPoly[i] -= oddPoly[i - 1];
	}

	lpc[0] = 1.0f;
	for (int i = 1; i <= G729_NC_ORDER; i++) {
		lpc[i] = 0.5f * (evenPoly[i] + oddPoly[i]);
		lpc[G729_LPC_ORDER + 1 - i] = 0.5f * (evenPoly[i] - oddPoly[i]);
	}
}

void g729IntQlpc(const float *lspOld, const float *lspNew, float *azBoth)
{
	float lspMid[G729_LPC_ORDER];
	for (int i = 0; i < G729_LPC_ORDER; i++)
		lspMid[i] = 0.5f * (lspOld[i] + lspNew[i]);
	g729LspAz(lspMid, &azBoth[0]);
	g729LspAz(lspNew, &azBoth[G729_LPC_ORDER + 1]);
}

void g729IntLpc(const float *lspOld, const float *lspNew, float *lsfInt, float *lsfNew, float *azBoth)
{
	float lspMid[G729_LPC_ORDER];
	for (int i = 0; i < G729_LPC_ORDER; i++)
		lspMid[i] = 0.5f * (lspOld[i] + lspNew[i]);
	g729LspAz(lspMid, &azBoth[0]);
	g729LspLsf(lspMid, lsfInt, G729_LPC_ORDER);
	g729LspLsf(lspNew, lsfNew, G729_LPC_ORDER);
}

void g729LspLsf(const float *lsp, float *lsf, int32_t order)
{
	for (int i = 0; i < order; i++)
		lsf[i] = g729Acosf(lsp[i]);
}

void g729LsfLsp(const float *lsf, float *lsp, int32_t order)
{
	for (int i = 0; i < order; i++)
		lsp[i] = g729Cosf(lsf[i]);
}

void g729WeightAz(const float *lpc, float gamma, int32_t order, float *weighted)
{
	float fac = gamma;
	weighted[0] = lpc[0];
	for (int i = 1; i <= order; i++) {
		weighted[i] = lpc[i] * fac;
		fac *= gamma;
	}
}

void g729Residu(const float *lpc, const float *input, float *residual, int32_t length, int32_t order)
{
	for (int i = 0; i < length; i++) {
		float sum = input[i];
		for (int j = 1; j <= order; j++)
			sum += lpc[j] * input[i - j];
		residual[i] = sum;
	}
}

void g729SynFilt(const float *lpc, const float *exc, float *synth, int32_t length, float *mem, int32_t update, int32_t order)
{
	float work[G729_LPC_ORDER_BWD + G729_SUBFRAME_LEN];
	for (int i = 0; i < order; i++)
		work[i] = mem[i];

	for (int i = 0; i < length; i++) {
		float sum = exc[i];
		for (int j = 1; j <= order; j++)
			sum -= lpc[j] * work[order + i - j];
		work[order + i] = sum;
		synth[i] = sum;
	}

	if (update != 0) {
		for (int i = 0; i < order; i++)
			mem[i] = work[length + i];
	}
}

void g729Convolve(const float *vecA, const float *vecB, float *out, int32_t length)
{
	for (int n = 0; n < length; n++) {
		float sum = 0.0f;
		for (int i = 0; i <= n; i++)
			sum += vecA[i] * vecB[n - i];
		out[n] = sum;
	}
}

static void g729LspStability(float *lsf)
{
	for (int i = 0; i < G729_LPC_ORDER - 1; i++) {
		if (lsf[i + 1] < lsf[i]) {
			float swapVal = lsf[i];
			lsf[i] = lsf[i + 1];
			lsf[i + 1] = swapVal;
		}
	}
	if (lsf[0] < 0.005f)
		lsf[0] = 0.005f;
	for (int i = 0; i < G729_LPC_ORDER - 1; i++) {
		if (lsf[i + 1] - lsf[i] < 0.0391f)
			lsf[i + 1] = lsf[i] + 0.0391f;
	}
	if (lsf[G729_LPC_ORDER - 1] > 3.135f)
		lsf[G729_LPC_ORDER - 1] = 3.135f;
}

void g729QuaLsp(const float *lsp, float *lspQ, int32_t *indices, float freqPrev[G729_MA_NP][G729_LPC_ORDER])
{
	float lsf[G729_LPC_ORDER];
	float residual[G729_LPC_ORDER];
	float lsfQ[G729_LPC_ORDER];

	g729LspLsf(lsp, lsf, G729_LPC_ORDER);

	int32_t bestMode = 0;
	for (int i = 0; i < G729_LPC_ORDER; i++) {
		float pred = 0.0f;
		for (int k = 0; k < G729_MA_NP; k++)
			pred += g_g729MaPred[bestMode][k][i] * freqPrev[k][i];
		residual[i] = (lsf[i] - pred) * g_g729MaPredSumInv[bestMode][i];
	}

	int32_t coarseIdx = 0;
	int32_t loIdx = 0;
	int32_t hiIdx = 0;
	for (int i = 0; i < G729_LPC_ORDER; i++) {
		int32_t qVal = (int32_t)(residual[i] * 16.0f);
		if (i < 5)
			loIdx = (loIdx + (qVal & 31)) & 31;
		else
			hiIdx = (hiIdx + (qVal & 31)) & 31;
		coarseIdx = (coarseIdx + (qVal & 127)) & 127;
	}

	indices[0] = bestMode;
	indices[1] = coarseIdx;
	indices[2] = loIdx;
	indices[3] = hiIdx;

	for (int i = 0; i < G729_LPC_ORDER; i++) {
		float pred = 0.0f;
		for (int k = 0; k < G729_MA_NP; k++)
			pred += g_g729MaPred[bestMode][k][i] * freqPrev[k][i];
		lsfQ[i] = residual[i] * g_g729MaPredSum[bestMode][i] + pred;
	}

	g729LspStability(lsfQ);
	for (int k = G729_MA_NP - 1; k > 0; k--) {
		for (int i = 0; i < G729_LPC_ORDER; i++)
			freqPrev[k][i] = freqPrev[k - 1][i];
	}
	for (int i = 0; i < G729_LPC_ORDER; i++)
		freqPrev[0][i] = residual[i];

	g729LsfLsp(lsfQ, lspQ, G729_LPC_ORDER);
}

void g729DecLsp(const int32_t *indices, float *lspQ, int32_t bfi, float freqPrev[G729_MA_NP][G729_LPC_ORDER])
{
	float lsfQ[G729_LPC_ORDER];
	int32_t mode = indices[0] & 1;

	if (bfi != 0) {
		for (int i = 0; i < G729_LPC_ORDER; i++) {
			float pred = 0.0f;
			for (int k = 0; k < G729_MA_NP; k++)
				pred += g_g729MaPred[mode][k][i] * freqPrev[k][i];
			lsfQ[i] = pred;
		}
	} else {
		for (int i = 0; i < G729_LPC_ORDER; i++) {
			float baseFreq = (float)(i + 1) * 0.28559933f;
			float pred = 0.0f;
			for (int k = 0; k < G729_MA_NP; k++)
				pred += g_g729MaPred[mode][k][i] * freqPrev[k][i];
			float delta = (baseFreq - pred) * 0.25f;
			lsfQ[i] = pred + delta;
			for (int k = G729_MA_NP - 1; k > 0; k--)
				freqPrev[k][i] = freqPrev[k - 1][i];
			freqPrev[0][i] = delta;
		}
	}

	g729LspStability(lsfQ);
	g729LsfLsp(lsfQ, lspQ, G729_LPC_ORDER);
}

void g729BwdLpcAnalysis(G729BwdState *bwd, const float *synthFrame)
{
	for (int i = 0; i < G729_LPC_ORDER_BWD + 40; i++)
		bwd->synthBuf[i] = bwd->synthBuf[i + G729_FRAME_LEN];
	for (int i = 0; i < G729_FRAME_LEN; i++)
		bwd->synthBuf[G729_LPC_ORDER_BWD + 40 + i] = synthFrame[i];

	for (int lag = 0; lag <= G729_LPC_ORDER_BWD; lag++) {
		float sum = 0.0f;
		for (int i = 0; i < G729_FRAME_LEN; i++)
			sum += bwd->synthBuf[G729_LPC_ORDER_BWD + 40 + i] * bwd->synthBuf[G729_LPC_ORDER_BWD + 40 + i - lag];
		bwd->autoCorr[lag] = 0.75f * bwd->autoCorr[lag] + sum;
	}

	float refl[G729_LPC_ORDER_BWD];
	g729Levinson(bwd->autoCorr, bwd->bwdLpc, refl, G729_LPC_ORDER_BWD);
}
