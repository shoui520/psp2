/*
 * The SBR HF generator (0x8bf4b0-0x8c1660): chirp factors, the LPC
 * covariance and the patched high band. The original runs on the IVC2.
 */
#include "aac.h"
#include "aacsbr.h"

#define CHIRP_MIN	0x00200000	/* 1/64, Q27: smaller chirp factors are 0 */

/* 0x853aa0: the new chirp factor for a band's inverse filtering modes */
static int32_t newBandwidth(uint8_t mode, uint8_t previous, const int32_t *bandwidth)
{
	switch (mode) {
	case 3:
		return bandwidth[4];
	case 2:
		return bandwidth[3];
	case 1:
		return bandwidth[previous == 0 ? 1 : 2];
	default:
		return bandwidth[previous == 1 ? 1 : 0];
	}
}

/*
 * 0x8bf4b0: each noise band's chirp factor, smoothed against the last
 * frame's (3/4 + 1/4 rising, 29/32 + 3/32 falling)
 */
void aacSbrChirpFactors(AacSbrHf *hf, int32_t count, const uint8_t *invfMode,
			const uint8_t *previousInvfMode, int32_t *out)
{
	int32_t i;

	for (i = 0; i < count; i++) {
		int32_t table[5], bw, now, before, value, k;

		for (k = 0; k < 5; k++)
			table[k] = hf->patches->bandwidth[k];
		bw = newBandwidth(invfMode[i], previousInvfMode[i], table);
		out[i] = bw;

		if (bw < hf->previousBandwidth[i]) {
			now = 0x06000000;
			before = 0x02000000;
		} else {
			now = 0x07400000;
			before = 0x00c00000;
		}
		value = (int32_t)(((int64_t)bw * now + (int64_t)hf->previousBandwidth[i] * before) >> 27);
		if (value < CHIRP_MIN)
			value = 0;
		out[i] = value;
	}
}

#define ALPHA_MAX	0x07ffffff	/* |alpha|^2 limit, Q27 */
#define QUARTER		0x02000000	/* 1/4, Q27: the patched band's own gain */

static int64_t product(int32_t a, int32_t b)
{
	return (int64_t)a * b;
}

/* |value| as the kernel selects it */
static int32_t magnitude(int32_t value)
{
	return value > 0 ? value : (int32_t)(0u - (uint32_t)value);
}

/*
 * The LPC coefficients from the covariance (4.6.18.6.2), at a quarter of
 * their size; all zero when the predictor would be unstable
 */
static void lpcCoefficients(const AacSbrCovariance *phi, int32_t *alphaReal,
	int32_t *alphaImag)
{
	int32_t unstable = 0, numerator;

	alphaReal[1] = 0;
	alphaImag[1] = 0;
	if (phi->determinant != 0) {
		numerator = (int32_t)((product(phi->phi01Real, phi->phi12Real) -
			product(phi->phi01Imag, phi->phi12Imag) -
			product(phi->phi02Real, phi->phi11)) >> 29);
		if (magnitude(numerator) < magnitude(phi->determinant))
			alphaReal[1] = aacSbrQuotient(numerator, phi->determinant);
		else
			unstable = 1;
		numerator = (int32_t)((product(phi->phi01Imag, phi->phi12Real) +
			product(phi->phi01Real, phi->phi12Imag) -
			product(phi->phi02Imag, phi->phi11)) >> 29);
		if (magnitude(numerator) < magnitude(phi->determinant))
			alphaImag[1] = aacSbrQuotient(numerator, phi->determinant);
		else
			unstable = 1;
	}

	alphaReal[0] = 0;
	alphaImag[0] = 0;
	if (phi->phi11 != 0) {
		numerator = (int32_t)((product(phi->phi01Real, QUARTER) +
			product(alphaReal[1], phi->phi12Real) +
			product(alphaImag[1], phi->phi12Imag)) >> 27);
		if (magnitude(numerator) < phi->phi11)
			alphaReal[0] = aacSbrQuotient(-numerator, phi->phi11);
		else
			unstable = 1;
		numerator = (int32_t)((product(phi->phi01Imag, QUARTER) +
			product(alphaImag[1], phi->phi12Real) -
			product(alphaReal[1], phi->phi12Imag)) >> 27);
		if (magnitude(numerator) < phi->phi11)
			alphaImag[0] = aacSbrQuotient(-numerator, phi->phi11);
		else
			unstable = 1;
	}

	if ((int32_t)((product(alphaReal[0], alphaReal[0]) +
	    product(alphaImag[0], alphaImag[0])) >> 27) >= ALPHA_MAX)
		unstable = 1;
	if ((int32_t)((product(alphaReal[1], alphaReal[1]) +
	    product(alphaImag[1], alphaImag[1])) >> 27) >= ALPHA_MAX || unstable) {
		alphaReal[0] = alphaReal[1] = 0;
		alphaImag[0] = alphaImag[1] = 0;
	}
}

static int32_t *align8(void *address)
{
	uintptr_t value = (uintptr_t)address;

	if (value & 7)
		value += 8 - (value & 7);
	return (int32_t *)value;
}

/*
 * One patch of one source band: the band predicted from its two previous
 * slots, its chirp factor setting the prediction's weight, into slots
 * first to last of the target band (in pairs); without a chirp factor
 * the band is copied at a quarter
 */
static void patchBand(int32_t **real, int32_t **imag, const int32_t *x, const int32_t *y,
	int32_t target, int32_t first, int32_t last, int32_t bandwidth,
	const int32_t *alphaReal, const int32_t *alphaImag)
{
	int32_t bandwidth2 = (int32_t)(product(bandwidth, bandwidth) >> 27);
	int32_t a0r = (int32_t)(product(bandwidth, alphaReal[0]) >> 27);
	int32_t a0i = (int32_t)(product(bandwidth, alphaImag[0]) >> 27);
	int32_t a1r = (int32_t)(product(bandwidth2, alphaReal[1]) >> 27);
	int32_t a1i = (int32_t)(product(bandwidth2, alphaImag[1]) >> 27);
	int32_t l, n;

	if (bandwidth < 1) {
		for (l = first; l < last; l++) {
			real[l][target] = x[l] >> 2;
			imag[l][target] = y[l] >> 2;
		}
		return;
	}
	for (l = first; l < last; l += 2) {
		for (n = l; n < l + 2; n++) {
			real[n][target] = (int32_t)((product(x[n], QUARTER) +
				product(x[n - 1], a0r) + product(x[n - 2], a1r) -
				product(y[n - 1], a0i) - product(y[n - 2], a1i)) >> 27);
			imag[n][target] = (int32_t)((product(y[n], QUARTER) +
				product(x[n - 1], a0i) + product(x[n - 2], a1i) +
				product(y[n - 1], a0r) + product(y[n - 2], a1r)) >> 27);
		}
	}
}

/*
 * 0x8bf720: the high band of a complex frame: slots first to last of
 * every patch's target bands from its source bands' LPC prediction
 * (4.6.18.6), above the patches cleared; the source bands are taken with
 * the last frame's six overlapping slots at the scale of the frame's
 * other slots
 */
void aacSbrHfGenerate(AacSbrHf *hf, int32_t *timing, int32_t **real, int32_t **imag,
	int32_t timeStep, int32_t start, int32_t end, int32_t noiseBands, const uint8_t *invfMode,
	const uint8_t *previousInvfMode, int32_t *work)
{
	AacSbrPatches *patches = hf->patches;
	const AacSbrPatch *lastPatch = &patches->patch[patches->numPatches - 1];
	int32_t *noiseBand = work, *bandwidth = work + 6;
	int32_t *alphaReal = work + 12, *alphaImag = work + 14;
	AacSbrCovariance *covariance = (AacSbrCovariance *)(work + 16);
	int32_t *columnReal = align8((uint8_t *)work + 100);
	int32_t *columnImag = align8((uint8_t *)columnReal + 640);
	int32_t first = timeStep * start, last = patches->numSlots + timeStep * end;
	int32_t hfEnd = lastPatch->targetStart + lastPatch->numBands;
	int32_t scale, historyShift, shift, band, l, k, p, target;

	for (k = 12; k < 22; k++)
		work[k] = 0;
	aacSbrChirpFactors(hf, noiseBands, invfMode, previousInvfMode, bandwidth);

	for (l = first; l < last; l++) {
		for (k = hfEnd; k < 64; k++) {
			real[l][k] = 0;
			imag[l][k] = 0;
		}
	}
	for (p = 0; p < patches->numPatches; p++)
		noiseBand[p] = 0;

	scale = aacMin(timing[2], timing[0]);
	historyShift = (timing[2] - scale) & 31;
	shift = (timing[0] - scale) & 31;
	for (band = patches->sourceLow; band < patches->sourceHigh; band++) {
		columnReal[0] = hf->lpcReal[0][band] >> historyShift;
		columnImag[0] = hf->lpcImag[0][band] >> historyShift;
		columnReal[1] = hf->lpcReal[1][band] >> historyShift;
		columnImag[1] = hf->lpcImag[1][band] >> historyShift;
		for (l = 0; l < patches->numSlots + 6; l++) {
			columnReal[l + 2] = real[l][band] >> (l < 6 ? historyShift : shift);
			columnImag[l + 2] = imag[l][band] >> (l < 6 ? historyShift : shift);
		}
		aacSbrCovariance(covariance, columnReal + 2, columnImag + 2,
			patches->numSlots + 6);
		lpcCoefficients(covariance, alphaReal, alphaImag);

		for (p = 0; p < patches->numPatches; p++) {
			const AacSbrPatch *patch = &patches->patch[p];

			if (band < patch->source || band >= patch->sourceEnd)
				continue;
			target = band + patch->offset;
			while (target >= patches->noise[noiseBand[p]])
				noiseBand[p]++;
			patchBand(real, imag, columnReal + 2, columnImag + 2, target, first, last,
				bandwidth[noiseBand[p]], alphaReal, alphaImag);
		}
	}

	for (k = 0; k < noiseBands; k++)
		hf->previousBandwidth[k] = bandwidth[k];
	timing[3] = scale - 2;
}

/* the real LPC coefficients, at a quarter; zero when unstable */
static void lpcCoefficientsReal(const AacSbrCovariance *phi, int32_t *alpha)
{
	int32_t unstable = 0, numerator;

	alpha[1] = 0;
	if (phi->determinant != 0) {
		numerator = (int32_t)((product(phi->phi01Real, phi->phi12Real) -
			product(phi->phi02Real, phi->phi11)) >> 29);
		if (magnitude(numerator) < magnitude(phi->determinant))
			alpha[1] = aacSbrQuotient(numerator, phi->determinant);
		else
			unstable = 1;
	}
	alpha[0] = 0;
	if (phi->phi11 != 0) {
		numerator = (int32_t)((product(phi->phi01Real, QUARTER) +
			product(alpha[1], phi->phi12Real)) >> 27);
		if (magnitude(numerator) < phi->phi11)
			alpha[0] = aacSbrQuotient(-numerator, phi->phi11);
		else
			unstable = 1;
	}
	if (unstable) {
		alpha[1] = 0;
		alpha[0] = 0;
	}
}

/* the first reflection coefficient, -phi01 / phi11, clipped to +-1 */
static int32_t reflection(const AacSbrCovariance *phi)
{
	if (phi->phi11 == 0)
		return 0;
	if (aacAbs(phi->phi01Real) < aacAbs(phi->phi11))
		return -aacSbrQuotient(phi->phi01Real, phi->phi11);
	if (-(int32_t)(product(phi->phi01Real, phi->phi11) >> 27) >= 1)
		return ALPHA_MAX;
	return -0x08000000;
}

/*
 * The degree of aliasing of band k from the reflection coefficients of
 * it and the two below (the low-power decoder's, 4.6.18.7.6)
 */
static void aliasDegrees(int32_t *degree, int32_t band, int32_t r0, int32_t r1, int32_t r2)
{
	int32_t remainder = ALPHA_MAX - (aacShiftSaturate((int32_t)(product(r1, r1) >> 27), 4) >> 4);

	degree[band] = 0;
	if (!(band & 1)) {
		if (r0 >= 0)
			return;
		if (r1 < 0) {
			degree[band] = ALPHA_MAX;
			if (r2 >= 1)
				degree[band - 1] = remainder;
		} else if (r2 >= 1) {
			degree[band] = remainder;
		}
	} else {
		if (r0 < 1)
			return;
		if (r1 >= 1) {
			degree[band] = ALPHA_MAX;
			if (r2 < 0)
				degree[band - 1] = remainder;
		} else if (r2 < 0) {
			degree[band] = remainder;
		}
	}
}

/* patchBand for a real band */
static void patchBandReal(int32_t **real, const int32_t *x, int32_t target, int32_t first,
	int32_t last, int32_t bandwidth, const int32_t *alpha)
{
	int32_t a0 = (int32_t)(product(bandwidth, alpha[0]) >> 27);
	int32_t a1 = (int32_t)(product((int32_t)(product(bandwidth, bandwidth) >> 27),
		alpha[1]) >> 27);
	int32_t l, n;

	if (bandwidth < 1) {
		for (l = first; l < last; l++)
			real[l][target] = x[l] >> 2;
		return;
	}
	for (l = first; l < last; l += 2)
		for (n = l; n < l + 2; n++)
			real[n][target] = (int32_t)((product(x[n], QUARTER) +
				product(x[n - 1], a0) + product(x[n - 2], a1)) >> 27);
}

/*
 * 0x8c0960: the high band of a real (low-power) frame, as
 * aacSbrHfGenerate; every band below kx also gives its reflection
 * coefficient for the degrees of aliasing, which the patches carry up
 */
void aacSbrHfGenerateReal(AacSbrHf *hf, int32_t *timing, int32_t **real, int32_t *aliasDegree,
	int32_t timeStep, int32_t start, int32_t end, int32_t noiseBands, const uint8_t *invfMode,
	const uint8_t *previousInvfMode, int32_t *work)
{
	AacSbrPatches *patches = hf->patches;
	const AacSbrPatch *lastPatch = &patches->patch[patches->numPatches - 1];
	int32_t *noiseBand = work, *bandwidth = work + 6, *alpha = work + 12;
	AacSbrCovariance *covariance = (AacSbrCovariance *)(work + 14);
	int32_t *column = align8((uint8_t *)(work + 14) + 36);
	int32_t first = timeStep * start, last = patches->numSlots + timeStep * end;
	int32_t hfEnd = lastPatch->targetStart + lastPatch->numBands;
	int32_t r0, r1 = 0, r2 = 0;
	int32_t scale, historyShift, shift, band, l, k, p, target;

	alpha[0] = 0;
	alpha[1] = 0;
	aacSbrChirpFactors(hf, noiseBands, invfMode, previousInvfMode, bandwidth);

	for (l = first; l < last; l++)
		for (k = hfEnd; k < 64; k++)
			real[l][k] = 0;
	for (p = 0; p < patches->numPatches; p++)
		noiseBand[p] = 0;

	scale = aacMin(timing[2], timing[0]);
	historyShift = (timing[2] - scale) & 31;
	shift = (timing[0] - scale) & 31;
	for (band = aacMax(1, patches->sourceLow - 2); band < patches->patch[0].targetStart;
	    band++) {
		column[0] = hf->lpcReal[0][band] >> historyShift;
		column[1] = hf->lpcReal[1][band] >> historyShift;
		for (l = 0; l < patches->numSlots + 6; l++)
			column[l + 2] = real[l][band] >> (l < 6 ? historyShift : shift);
		aacSbrCovarianceReal(covariance, column + 2, patches->numSlots + 6);
		lpcCoefficientsReal(covariance, alpha);

		r0 = reflection(covariance);
		if (band >= 2)
			aliasDegrees(aliasDegree, band, r0, r1, r2);
		r2 = r1;
		r1 = r0;

		for (p = 0; p < patches->numPatches; p++) {
			const AacSbrPatch *patch = &patches->patch[p];

			if (band < patch->source || band >= patch->sourceEnd)
				continue;
			target = band + patch->offset;
			while (target >= patches->noise[noiseBand[p]])
				noiseBand[p]++;
			patchBandReal(real, column + 2, target, first, last,
				bandwidth[noiseBand[p]], alpha);
		}
	}

	/* the high bands alias as their sources, but for each patch's first */
	for (band = patches->sourceLow; band < patches->sourceHigh; band++) {
		for (p = 0; p < patches->numPatches; p++) {
			const AacSbrPatch *patch = &patches->patch[p];

			target = band + patch->offset;
			if (band < patch->source || band >= patch->sourceEnd || target >= 64)
				continue;
			if (target != patch->targetStart)
				aliasDegree[target] = aliasDegree[band];
		}
	}

	for (k = 0; k < noiseBands; k++)
		hf->previousBandwidth[k] = bandwidth[k];
	timing[3] = scale - 2;
}
