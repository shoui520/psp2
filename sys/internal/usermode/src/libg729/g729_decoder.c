#include "g729_internal.h"

void g729PostProcessInit(G729HpfState *hpf)
{
	hpf->xHistPrev = 0.0f;
	hpf->xHistPrevPrev = 0.0f;
	hpf->yHistPrev = 0.0f;
	hpf->yHistPrevPrev = 0.0f;
}

void g729PostProcess(float *signal, int32_t length, G729HpfState *hpf)
{
	for (int32_t i = 0; i < length; i++) {
		float xPrevPrev = hpf->xHistPrev;
		float xCur = signal[i];
		float yOut = 0.93980581f * xCur
		           - 1.8795834f * hpf->xHistPrev
		           + 0.93980581f * hpf->xHistPrevPrev
		           + 1.9330735f * hpf->yHistPrev
		           - 0.93589199f * hpf->yHistPrevPrev;
		hpf->xHistPrevPrev = xPrevPrev;
		hpf->xHistPrev = xCur;
		hpf->yHistPrevPrev = hpf->yHistPrev;
		hpf->yHistPrev = yOut;
		signal[i] = yOut;
	}
}

void g729PostFilterInit(G729PostFilterState *pf)
{
	sceClibMemset(pf, 0, sizeof(G729PostFilterState));
	pf->pastGain = 1.0f;
	pf->gammaDen = 0.70f;
	pf->gammaNum = 0.65f;
	pf->gammaHarm = 0.25f;
	pf->lpcOrder = G729_LPC_ORDER;
	pf->shortTermOrder = 60;
}

void g729PostFilter(int32_t pitchDelay, const float *synthIn, const float *lpc, float *synthOut, int32_t *voicing, int32_t maxOrder, int32_t order, G729PostFilterState *pf)
{
	float aNum[G729_LPC_ORDER_BWD + 1];
	float aDen[G729_LPC_ORDER_BWD + 1];
	float residual[G729_SUBFRAME_LEN];
	float filtered[G729_SUBFRAME_LEN];

	(void)pitchDelay;
	(void)maxOrder;

	if (order > G729_LPC_ORDER_BWD)
		order = G729_LPC_ORDER_BWD;

	g729WeightAz(lpc, pf->gammaNum, order, aNum);
	g729WeightAz(lpc, pf->gammaDen, order, aDen);

	g729Residu(aNum, synthIn, residual, G729_SUBFRAME_LEN, order);
	g729SynFilt(aDen, residual, filtered, G729_SUBFRAME_LEN, pf->memSynPst, 1, order);

	float inEnergy = 0.01f;
	float outEnergy = 0.01f;
	for (int i = 0; i < G729_SUBFRAME_LEN; i++) {
		inEnergy += synthIn[i] * synthIn[i];
		outEnergy += filtered[i] * filtered[i];
	}
	float targetGain = g729Sqrtf(inEnergy / outEnergy);

	for (int i = 0; i < G729_SUBFRAME_LEN; i++) {
		pf->pastGain = 0.85f * pf->pastGain + 0.15f * targetGain;
		synthOut[i] = filtered[i] * pf->pastGain;
	}
	if (voicing != NULL)
		*voicing = 0;
}

void g729PostFilterShort(const float *synthIn, const float *lpc, const int32_t *pitchDelays, float *synthOut, G729PostFilterState *pf)
{
	int32_t voicing = 0;
	g729PostFilter(pitchDelays[0], &synthIn[0], &lpc[0], &synthOut[0], &voicing, 20, G729_LPC_ORDER, pf);
	g729PostFilter(pitchDelays[1], &synthIn[G729_SUBFRAME_LEN], &lpc[G729_LPC_ORDER + 1],
	               &synthOut[G729_SUBFRAME_LEN], &voicing, 20, G729_LPC_ORDER, pf);
}

static void g729DecodeFixedCodebook(int32_t cbIdx, int32_t signMask, float *codeVec)
{
	sceClibMemset(codeVec, 0, sizeof(float) * G729_SUBFRAME_LEN);
	int32_t pos0 = ((cbIdx >> 10) & 7) * 5 + 0;
	int32_t pos1 = ((cbIdx >> 7) & 7) * 5 + 1;
	int32_t pos2 = ((cbIdx >> 4) & 7) * 5 + 2;
	int32_t pos3 = (cbIdx & 7) * 5 + 3;

	if (pos0 < G729_SUBFRAME_LEN)
		codeVec[pos0] = ((signMask & 1) != 0) ? 1.0f : -1.0f;
	if (pos1 < G729_SUBFRAME_LEN)
		codeVec[pos1] = ((signMask & 2) != 0) ? 1.0f : -1.0f;
	if (pos2 < G729_SUBFRAME_LEN)
		codeVec[pos2] = ((signMask & 4) != 0) ? 1.0f : -1.0f;
	if (pos3 < G729_SUBFRAME_LEN)
		codeVec[pos3] = ((signMask & 8) != 0) ? 1.0f : -1.0f;
}

static void g729DecodeGains(int32_t gainIdx, int32_t bfi, float *gainPitch, float *gainCode)
{
	if (bfi != 0) {
		*gainPitch *= 0.9f;
		if (*gainPitch > 0.9f)
			*gainPitch = 0.9f;
		*gainCode *= 0.98f;
		return;
	}
	int32_t idxA = (gainIdx >> 4) & 7;
	int32_t idxB = gainIdx & 15;
	*gainPitch = g_g729GainCodebookA[idxA][0] + g_g729GainCodebookB[idxB][0];
	*gainCode = g_g729GainCodebookA[idxA][1] + g_g729GainCodebookB[idxB][1];
}

void g729InitDecoder(G729DecoderCoreState *state)
{
	sceClibMemset(state, 0, sizeof(G729DecoderCoreState));
	state->excPtr = &state->oldExc[G729_PITCH_MAX + G729_INTERPOL_LEN];
	state->sharp = 0.2f;
	state->oldT0 = 60;
	state->prngSeed = 21845U;
	state->lastOrder = G729_LPC_ORDER;
	state->lastRate = (int32_t)G729_BITS_8000;

	for (int i = 0; i < G729_LPC_ORDER; i++) {
		state->lspOld[i] = g_g729LspInit[i];
		for (int k = 0; k < G729_MA_NP; k++)
			state->freqPrev[k][i] = (float)(i + 1) * 0.28559933f;
	}
	for (int i = 0; i < 4; i++)
		state->pastQuaEnergy[i] = -14.0f;
}

void g729DecodeFrame(const int32_t *prm, int32_t voicing, float *synthOut, float *azDec, int32_t *pitchDelays, int32_t *outOrder, G729DecoderCoreState *state)
{
	int32_t bfi = prm[0];
	int32_t frameType = prm[1];
	float lspNew[G729_LPC_ORDER];

	(void)voicing;
	*outOrder = G729_LPC_ORDER;

	if (frameType == 0 || frameType == 1) {
		if (frameType == 1)
			state->sidGain = g729Powf(10.0f, (float)prm[5] * 0.1f);
		state->curGain = 0.875f * state->curGain + 0.125f * state->sidGain;

		g729IntQlpc(state->lspOld, state->lspOld, azDec);
		for (int sub = 0; sub < 2; sub++) {
			float *subExc = &state->excPtr[sub * G729_SUBFRAME_LEN];
			for (int i = 0; i < G729_SUBFRAME_LEN; i++) {
				int16_t rnd = (int16_t)g729PrngNext(&state->prngSeed);
				subExc[i] = ((float)rnd / 32768.0f) * state->curGain;
			}
			g729SynFilt(&azDec[sub * (G729_LPC_ORDER + 1)], subExc,
			            &synthOut[sub * G729_SUBFRAME_LEN], G729_SUBFRAME_LEN, state->memSyn, 1, G729_LPC_ORDER);
			pitchDelays[sub] = state->oldT0;
		}
		for (int i = 0; i < G729_PITCH_MAX + G729_INTERPOL_LEN; i++)
			state->oldExc[i] = state->oldExc[i + G729_FRAME_LEN];
		return;
	}

	int32_t prmIdx = 2;
	if (frameType == 4) {
		int32_t isBwd = prm[prmIdx++];
		if (isBwd != 0)
			*outOrder = G729_LPC_ORDER_BWD;
	}

	g729DecLsp(&prm[prmIdx], lspNew, bfi, state->freqPrev);
	prmIdx += 4;
	g729IntQlpc(state->lspOld, lspNew, azDec);
	for (int i = 0; i < G729_LPC_ORDER; i++)
		state->lspOld[i] = lspNew[i];

	for (int sub = 0; sub < 2; sub++) {
		int32_t pitchCode = prm[prmIdx++];
		if (sub == 0 && frameType != 2)
			prmIdx++;
		int32_t cbIdx = prm[prmIdx++];
		int32_t signMask = prm[prmIdx++];
		int32_t gainIdx = prm[prmIdx++];

		int32_t lag = G729_PITCH_MIN + (pitchCode / 3);
		if (lag < G729_PITCH_MIN)
			lag = G729_PITCH_MIN;
		if (lag > G729_PITCH_MAX)
			lag = G729_PITCH_MAX;
		state->oldT0 = lag;
		pitchDelays[sub] = lag;

		float *subExc = &state->excPtr[sub * G729_SUBFRAME_LEN];
		for (int i = 0; i < G729_SUBFRAME_LEN; i++)
			subExc[i] = subExc[i - lag];

		float codeVec[G729_SUBFRAME_LEN];
		g729DecodeFixedCodebook(cbIdx, signMask, codeVec);
		g729DecodeGains(gainIdx, bfi, &state->gainPitch, &state->gainCode);

		for (int i = 0; i < G729_SUBFRAME_LEN; i++)
			subExc[i] = state->gainPitch * subExc[i] + state->gainCode * codeVec[i];

		g729SynFilt(&azDec[sub * (G729_LPC_ORDER + 1)], subExc,
		            &synthOut[sub * G729_SUBFRAME_LEN], G729_SUBFRAME_LEN, state->memSyn, 1, G729_LPC_ORDER);
	}

	if (*outOrder == G729_LPC_ORDER_BWD)
		g729BwdLpcAnalysis(&state->bwd, synthOut);

	for (int i = 0; i < G729_PITCH_MAX + G729_INTERPOL_LEN; i++)
		state->oldExc[i] = state->oldExc[i + G729_FRAME_LEN];
}
