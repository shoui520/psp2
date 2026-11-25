#include "g729_internal.h"

void g729PreProcessInit(G729HpfState *hpf)
{
	hpf->xHistPrev = 0.0f;
	hpf->xHistPrevPrev = 0.0f;
	hpf->yHistPrev = 0.0f;
	hpf->yHistPrevPrev = 0.0f;
}

void g729PreProcess(float *signal, int32_t length, G729HpfState *hpf)
{
	for (int32_t i = 0; i < length; i++) {
		float xPrevPrev = hpf->xHistPrev;
		float xCur = signal[i];
		float yOut = 0.92727435f * xCur
		           - 1.8544941f * hpf->xHistPrev
		           + 0.92727435f * hpf->xHistPrevPrev
		           + 1.9059465f * hpf->yHistPrev
		           - 0.9114024f * hpf->yHistPrevPrev;
		hpf->xHistPrevPrev = xPrevPrev;
		hpf->xHistPrev = xCur;
		hpf->yHistPrevPrev = hpf->yHistPrev;
		hpf->yHistPrev = yOut;
		signal[i] = yOut;
	}
}

void g729VadInit(G729VadState *vad)
{
	sceClibMemset(vad, 0, sizeof(G729VadState));
	vad->minEnergy = 1e6f;
	vad->minEnergyPrev = 1e6f;
	vad->minEnergyNext = 1e6f;
	vad->energyFloor = 1e6f;
	vad->prevVadDecision = 1;
	vad->prngSeed = 11111U;
}

int32_t g729VadDecision(G729VadState *vad, const float *refl, const float *lsf, const float *corr)
{
	vad->frameCounter++;
	float fullEnergy = 10.0f * g729Log10f(corr[0] / 240.0f + 1e-6f);
	float spectralDist = 0.0f;
	for (int i = 0; i < G729_LPC_ORDER; i++) {
		float diff = lsf[i] - vad->meanLsf[i];
		spectralDist += diff * diff;
	}

	if (vad->frameCounter <= 32) {
		if (fullEnergy < 21.0f) {
			vad->speechFlag = 0;
		} else {
			vad->speechFlag = 1;
			vad->meanFullEnergy = 0.9f * vad->meanFullEnergy + 0.1f * fullEnergy;
			for (int i = 0; i < G729_LPC_ORDER; i++)
				vad->meanLsf[i] = 0.9f * vad->meanLsf[i] + 0.1f * lsf[i];
		}
		return vad->speechFlag;
	}

	float deltaEnergy = fullEnergy - vad->meanFullEnergy;
	int32_t marker = (deltaEnergy > 6.0f || spectralDist > 0.05f || g729Fabsf(refl[0]) > 0.85f) ? 1 : 0;

	if (marker != 0) {
		vad->burstCount++;
		if (vad->burstCount >= 3)
			vad->hangoverCount = 5;
	} else {
		vad->burstCount = 0;
		if (vad->hangoverCount > 0) {
			vad->hangoverCount--;
			marker = 1;
		} else {
			vad->meanFullEnergy = 0.95f * vad->meanFullEnergy + 0.05f * fullEnergy;
			for (int i = 0; i < G729_LPC_ORDER; i++)
				vad->meanLsf[i] = 0.95f * vad->meanLsf[i] + 0.05f * lsf[i];
		}
	}

	vad->speechFlag = marker;
	return marker;
}

static int32_t g729PitchOpenLoop(const float *wsp)
{
	int32_t bestLag = G729_PITCH_MIN;
	float bestCorr = -1e30f;

	for (int32_t lag = G729_PITCH_MIN; lag <= G729_PITCH_MAX; lag++) {
		float corr = 0.0f;
		float energy = 0.01f;
		for (int i = 0; i < G729_FRAME_LEN; i += 2) {
			corr += wsp[i] * wsp[i - lag];
			energy += wsp[i - lag] * wsp[i - lag];
		}
		float normCorr = corr / g729Sqrtf(energy);
		if (normCorr > bestCorr) {
			bestCorr = normCorr;
			bestLag = lag;
		}
	}
	return bestLag;
}

static void g729PredLt3(float *exc, int32_t lag, int32_t frac)
{
	int32_t baseLag = lag;
	int32_t fNeg = -frac;
	if (fNeg < 0) {
		fNeg += 3;
		baseLag--;
	}

	for (int i = 0; i < G729_SUBFRAME_LEN; i++) {
		float sum = 0.0f;
		const float *xCur = &exc[i - baseLag];
		for (int k = 0; k < G729_INTERPOL_LEN; k++) {
			sum += xCur[-k] * g_g729Inter3L[3 * k + fNeg];
			sum += xCur[k + 1] * g_g729Inter3L[3 * k + 3 - fNeg];
		}
		exc[i] = sum;
	}
}

static int32_t g729PitchClosedLoop(const float *exc, const float *xn, const float *impulse, int32_t lagMin, int32_t lagMax, int32_t *outFrac)
{
	int32_t bestLag = lagMin;
	float bestScore = -1e30f;

	for (int32_t lag = lagMin; lag <= lagMax; lag++) {
		float filtered[G729_SUBFRAME_LEN];
		g729Convolve(&exc[-lag], impulse, filtered, G729_SUBFRAME_LEN);
		float num = 0.0f;
		float den = 0.01f;
		for (int i = 0; i < G729_SUBFRAME_LEN; i++) {
			num += xn[i] * filtered[i];
			den += filtered[i] * filtered[i];
		}
		float score = (num > 0.0f) ? ((num * num) / den) : 0.0f;
		if (score > bestScore) {
			bestScore = score;
			bestLag = lag;
		}
	}
	*outFrac = 0;
	return bestLag;
}

static int32_t g729SearchFixedCodebook(const float *target, const float *impulse, float *codeVec, float *filtCode, int32_t *outSign)
{
	float corrX[G729_SUBFRAME_LEN];
	for (int i = 0; i < G729_SUBFRAME_LEN; i++) {
		float sum = 0.0f;
		for (int j = i; j < G729_SUBFRAME_LEN; j++)
			sum += target[j] * impulse[j - i];
		corrX[i] = sum;
		codeVec[i] = 0.0f;
	}

	int32_t positions[4] = { 0, 1, 2, 3 };
	int32_t signMask = 0;
	for (int track = 0; track < 4; track++) {
		int32_t bestPos = track;
		float bestAbs = -1.0f;
		for (int pos = track; pos < G729_SUBFRAME_LEN; pos += 5) {
			float absVal = g729Fabsf(corrX[pos]);
			if (absVal > bestAbs) {
				bestAbs = absVal;
				bestPos = pos;
			}
		}
		positions[track] = bestPos;
		float sign = (corrX[bestPos] >= 0.0f) ? 1.0f : -1.0f;
		if (sign > 0.0f)
			signMask |= (1 << track);
		codeVec[bestPos] = sign;
	}

	g729Convolve(codeVec, impulse, filtCode, G729_SUBFRAME_LEN);
	*outSign = signMask;
	return ((positions[0] / 5) << 10) | ((positions[1] / 5) << 7) | ((positions[2] / 5) << 4) | (positions[3] / 5);
}

static int32_t g729QuantizeGains(const float *xn, const float *filteredPitch, const float *filteredCode, float *outGainPitch, float *outGainCode)
{
	float numPitch = 0.0f;
	float denPitch = 0.01f;
	for (int i = 0; i < G729_SUBFRAME_LEN; i++) {
		numPitch += xn[i] * filteredPitch[i];
		denPitch += filteredPitch[i] * filteredPitch[i];
	}
	float gp = numPitch / denPitch;
	if (gp < 0.0f)
		gp = 0.0f;
	if (gp > 1.2f)
		gp = 1.2f;

	float numCode = 0.0f;
	float denCode = 0.01f;
	for (int i = 0; i < G729_SUBFRAME_LEN; i++) {
		float rem = xn[i] - gp * filteredPitch[i];
		numCode += rem * filteredCode[i];
		denCode += filteredCode[i] * filteredCode[i];
	}
	float gc = numCode / denCode;
	if (gc < 0.0f)
		gc = 0.0f;

	*outGainPitch = gp;
	*outGainCode = gc;

	int32_t idxA = (int32_t)(gp * 6.0f);
	if (idxA > 7)
		idxA = 7;
	int32_t idxB = (int32_t)(g729Log10f(gc + 1.0f) * 4.0f);
	if (idxB < 0)
		idxB = 0;
	if (idxB > 15)
		idxB = 15;
	return (idxA << 4) | idxB;
}

void g729InitCoder(int32_t vadEnable, G729CoderState *state)
{
	sceClibMemset(state, 0, sizeof(G729CoderState));
	state->newSpeech = &state->oldSpeech[G729_PAST_SPEECH_LEN - G729_FRAME_LEN];
	state->speechPtr = state->newSpeech - 40;
	state->windowPtr = state->oldSpeech;
	state->wspPtr = &state->oldWsp[G729_PITCH_MAX];
	state->excPtr = &state->oldExc[G729_PITCH_MAX + G729_INTERPOL_LEN];
	state->sharp = 0.2f;

	for (int i = 0; i < G729_LPC_ORDER; i++) {
		state->lspOld[i] = g_g729LspInit[i];
		state->lspOldQ[i] = g_g729LspInit[i];
		for (int k = 0; k < G729_MA_NP; k++)
			state->freqPrev[k][i] = (float)(i + 1) * 0.28559933f;
	}
	for (int i = 0; i < 4; i++)
		state->pastQuaEnergy[i] = -14.0f;

	if (vadEnable != 0)
		g729VadInit(&state->vad);
}

int32_t g729CoderFrame(int32_t *prm, int32_t frameNum, int32_t vadEnable, uint32_t rate, G729CoderState *state)
{
	float corr[G729_LPC_ORDER + 1];
	float lpc[G729_LPC_ORDER + 1];
	float refl[G729_LPC_ORDER];
	float lspNew[G729_LPC_ORDER];
	float lspNewQ[G729_LPC_ORDER];
	float lsfInt[G729_LPC_ORDER];
	float lsfNew[G729_LPC_ORDER];
	float azBoth[(G729_LPC_ORDER + 1) * 2];
	float aWeight1[G729_LPC_ORDER + 1];
	float aWeight2[G729_LPC_ORDER + 1];

	(void)frameNum;

	g729Autocorr(state->windowPtr, G729_LPC_ORDER, corr);
	g729LagWindow(G729_LPC_ORDER, corr);
	g729Levinson(corr, lpc, refl, G729_LPC_ORDER);
	g729AzLsp(lpc, lspNew, state->lspOld);
	g729IntLpc(state->lspOld, lspNew, lsfInt, lsfNew, azBoth);

	if (vadEnable != 0) {
		int32_t vadFlag = g729VadDecision(&state->vad, refl, lsfNew, corr);
		if (vadFlag == 0) {
			if (state->vad.prevVadDecision != 0) {
				prm[0] = 1;
				prm[1] = 0;
				prm[2] = 0;
				prm[3] = 0;
				prm[4] = (int32_t)(g729Log10f(corr[0] / 240.0f + 1.0f) * 3.0f) & 31;
			} else {
				prm[0] = 0;
			}
			state->vad.prevVadDecision = 0;
			for (int i = 0; i < G729_PAST_SPEECH_LEN - G729_FRAME_LEN; i++)
				state->oldSpeech[i] = state->oldSpeech[i + G729_FRAME_LEN];
			return 0;
		}
		state->vad.prevVadDecision = 1;
	}

	int32_t lspIndices[4];
	g729QuaLsp(lspNew, lspNewQ, lspIndices, state->freqPrev);
	g729IntQlpc(state->lspOldQ, lspNewQ, azBoth);

	for (int i = 0; i < G729_LPC_ORDER; i++) {
		state->lspOld[i] = lspNew[i];
		state->lspOldQ[i] = lspNewQ[i];
	}

	for (int sub = 0; sub < 2; sub++) {
		const float *curAz = &azBoth[sub * (G729_LPC_ORDER + 1)];
		g729WeightAz(curAz, 0.94f, G729_LPC_ORDER, aWeight1);
		g729WeightAz(curAz, 0.60f, G729_LPC_ORDER, aWeight2);
		float res[G729_SUBFRAME_LEN];
		g729Residu(aWeight1, &state->speechPtr[sub * G729_SUBFRAME_LEN], res, G729_SUBFRAME_LEN, G729_LPC_ORDER);
		g729SynFilt(aWeight2, res, &state->wspPtr[sub * G729_SUBFRAME_LEN], G729_SUBFRAME_LEN, state->memW, 1, G729_LPC_ORDER);
	}

	int32_t openLag = g729PitchOpenLoop(state->wspPtr);
	int32_t lagMin = openLag - 3;
	if (lagMin < G729_PITCH_MIN)
		lagMin = G729_PITCH_MIN;
	int32_t lagMax = lagMin + 6;
	if (lagMax > G729_PITCH_MAX) {
		lagMax = G729_PITCH_MAX;
		lagMin = lagMax - 6;
	}

	prm[0] = (rate == G729_RATE_6400) ? 2 : ((rate == G729_RATE_11800) ? 4 : 3);
	int32_t prmIdx = 1;
	if (rate == G729_RATE_11800)
		prm[prmIdx++] = 0;
	prm[prmIdx++] = lspIndices[0];
	prm[prmIdx++] = lspIndices[1];
	prm[prmIdx++] = lspIndices[2];
	prm[prmIdx++] = lspIndices[3];

	for (int sub = 0; sub < 2; sub++) {
		const float *curAz = &azBoth[sub * (G729_LPC_ORDER + 1)];
		float impulse[G729_SUBFRAME_LEN];
		float xn[G729_SUBFRAME_LEN];
		float filteredPitch[G729_SUBFRAME_LEN];
		float codeVec[G729_SUBFRAME_LEN];
		float filteredCode[G729_SUBFRAME_LEN];
		float synthSub[G729_SUBFRAME_LEN];

		g729WeightAz(curAz, 0.94f, G729_LPC_ORDER, aWeight1);
		g729WeightAz(curAz, 0.60f, G729_LPC_ORDER, aWeight2);

		float unitImpulse[G729_SUBFRAME_LEN];
		sceClibMemset(unitImpulse, 0, sizeof(unitImpulse));
		unitImpulse[0] = 1.0f;
		float tmpMem[G729_LPC_ORDER];
		sceClibMemset(tmpMem, 0, sizeof(tmpMem));
		g729SynFilt(aWeight2, unitImpulse, impulse, G729_SUBFRAME_LEN, tmpMem, 0, G729_LPC_ORDER);

		float *subExc = &state->excPtr[sub * G729_SUBFRAME_LEN];
		g729Residu(curAz, &state->speechPtr[sub * G729_SUBFRAME_LEN], subExc, G729_SUBFRAME_LEN, G729_LPC_ORDER);
		g729SynFilt(aWeight2, subExc, xn, G729_SUBFRAME_LEN, state->memW0, 0, G729_LPC_ORDER);

		int32_t frac = 0;
		int32_t lag = g729PitchClosedLoop(subExc, xn, impulse, lagMin, lagMax, &frac);
		g729PredLt3(subExc, lag, frac);
		g729Convolve(subExc, impulse, filteredPitch, G729_SUBFRAME_LEN);

		int32_t signMask = 0;
		int32_t cbIdx = g729SearchFixedCodebook(xn, impulse, codeVec, filteredCode, &signMask);

		float gainPitch = 0.0f;
		float gainCode = 0.0f;
		int32_t gainIdx = g729QuantizeGains(xn, filteredPitch, filteredCode, &gainPitch, &gainCode);

		for (int i = 0; i < G729_SUBFRAME_LEN; i++)
			subExc[i] = gainPitch * subExc[i] + gainCode * codeVec[i];

		g729SynFilt(curAz, subExc, synthSub, G729_SUBFRAME_LEN, state->memSyn, 1, G729_LPC_ORDER);

		int32_t pitchCode = (lag - G729_PITCH_MIN) * 3 + frac;
		prm[prmIdx++] = pitchCode;
		if (sub == 0 && rate != G729_RATE_6400)
			prm[prmIdx++] = g729ParityPitch(pitchCode);
		prm[prmIdx++] = cbIdx;
		prm[prmIdx++] = signMask;
		prm[prmIdx++] = gainIdx;
	}

	for (int i = 0; i < G729_PAST_SPEECH_LEN - G729_FRAME_LEN; i++)
		state->oldSpeech[i] = state->oldSpeech[i + G729_FRAME_LEN];
	for (int i = 0; i < G729_PITCH_MAX; i++)
		state->oldWsp[i] = state->oldWsp[i + G729_FRAME_LEN];
	for (int i = 0; i < G729_PITCH_MAX + G729_INTERPOL_LEN; i++)
		state->oldExc[i] = state->oldExc[i + G729_FRAME_LEN];

	return 0;
}
