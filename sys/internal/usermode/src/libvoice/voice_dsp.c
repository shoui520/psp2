/*
 * Real radix-2 FFT and elementary math routines for SceVoice.
 */

#include "voice_internal.h"

/* sin(pi / 2^k) for k = 1..24 */
static const float gFftSinTable[24] = {
	1.0f,
	0.7071067690849304f,
	0.3826834559440613f,
	0.19509032368659973f,
	0.0980171412229538f,
	0.049067676067352295f,
	0.024541229009628296f,
	0.012271538376808167f,
	0.006135884672403336f,
	0.0030679567717015743f,
	0.0015339802484959364f,
	0.0007669903570786119f,
	0.0003834952076431364f,
	0.0001917476038215682f,
	9.58738019107841e-5f,
	4.793690095539205e-5f,
	2.3968450477696024e-5f,
	1.1984225238848012e-5f,
	5.992112619424006e-6f,
	2.996056309712003e-6f,
	1.4980281548560015e-6f,
	7.490140774280007e-7f,
	3.7450703871400037e-7f,
	1.8725351935700019e-7f,
};

int voiceRealFft(float *data, int length)
{
	uint32_t shifted = (uint32_t)length >> 1;
	int log2Len = 0;
	while (shifted != 0) {
		shifted >>= 1;
		log2Len++;
	}
	if (length != (1 << log2Len)) {
		return -1;
	}

	int nComplex = 1 << log2Len;
	int halfComplex = nComplex >> 1;
	if (halfComplex > 0) {
		/* Bit-reversal permutation on interleaved complex pairs */
		int revIdx = 1;
		for (int idx = 1; idx <= halfComplex; idx++) {
			int cur = 2 * idx - 1;
			if (cur < revIdx) {
				float tmpReal = data[revIdx - 1];
				float tmpImag = data[revIdx];
				data[revIdx - 1] = data[cur - 1];
				data[revIdx] = data[cur];
				data[cur - 1] = tmpReal;
				data[cur] = tmpImag;
			}
			int bit = halfComplex;
			while (bit > 1 && revIdx > bit) {
				revIdx -= bit;
				bit >>= 1;
			}
			revIdx += bit;
		}

		/* Danielson-Lanczos butterflies */
		int mmax = 2;
		int stageShift = 1;
		float prevSin = 0.0f;
		const float *sinPtr = gFftSinTable;
		while (nComplex > mmax) {
			float sinHalf = *sinPtr++;
			stageShift++;
			float wpi = -prevSin;
			float wpr = (-2.0f * sinHalf) * sinHalf;
			int halfMmax = mmax >> 1;
			float wr = 1.0f;
			float wi = 0.0f;

			for (int m = 1; m <= halfMmax; m++) {
				int start = 2 * m - 1;
				int step = 1 << stageShift;
				for (int i = start; i <= nComplex; i += step) {
					int j = i + mmax;
					float reJ = data[j - 1];
					float imJ = data[j];
					float tempRe = reJ * wr - imJ * wi;
					float tempIm = reJ * wi + imJ * wr;
					data[j - 1] = data[i - 1] - tempRe;
					data[j] = data[i] - tempIm;
					data[i - 1] = data[i - 1] + tempRe;
					data[i] = data[i] + tempIm;
				}
				float nextWi = wpi * wr + wpr * wi;
				wr = (wpr * wr - wpi * wi) + wr;
				wi = wi + nextWi;
			}
			mmax <<= 1;
			prevSin = sinHalf;
		}
	}

	int halfLen = length >> 1;
	int quarterLen = length >> 2;
	if (quarterLen <= 0) {
		float dc = data[0] + data[1];
		float nyq = data[0] - data[1];
		data[0] = dc;
		data[1] = nyq;
		return 0;
	}

	/* Unpack real-valued FFT from half-size complex FFT */
	float *tail = &data[2 * halfLen];
	float *head = data + 3;
	for (int k = 1; k <= quarterLen; k++) {
		tail -= 2;
		float angle = ((float)k * 6.2831855f) / (float)length;
		float cosVal = voiceCosf(angle);
		float sinVal = voiceSinf(angle);

		float h1Re = 0.5f * (*(head - 1) + tail[0]);
		float h1Im = 0.5f * (head[0] - tail[1]);
		float h2Re = 0.5f * (head[0] + tail[1]);
		float h2Im = -0.5f * (*(head - 1) - tail[0]);

		float rotRe = cosVal * h2Re + sinVal * h2Im;
		float rotIm = cosVal * h2Im - sinVal * h2Re;

		*(head - 1) = h1Re + rotRe;
		head[0] = h1Im + rotIm;
		tail[0] = h1Re - rotRe;
		tail[1] = rotIm - h1Im;
		head += 2;
	}

	float dc = data[0] + data[1];
	float nyq = data[0] - data[1];
	data[0] = dc;
	data[1] = nyq;

	/* De-interleave real and imaginary halves */
	for (int span = 1; span < halfLen; span <<= 1) {
		for (int base = 0; base < length; base += span * 4) {
			float *left = &data[base + span];
			float *right = left + span;
			for (int i = 0; i < span; i++) {
				float tmp = left[i];
				left[i] = right[i];
				right[i] = tmp;
			}
		}
	}

	return 0;
}

float voiceExpf(float x)
{
	float scale = 1.0f;
	while (x > 1.0f) {
		x -= 1.0f;
		scale = (float)((double)scale * 2.71828183);
	}
	while (x < 0.0f) {
		x += 1.0f;
		scale = (float)((double)scale * 0.367879441);
	}

	double x2 = (double)(x * x);
	double xd = (double)x;
	double inner = 1.0 + x2 / (60.0 * (1.0 + x2 / 140.0));
	double frac = (2.0 * xd) / ((2.0 - xd) + (x2 / 6.0) / inner);
	return (float)((1.0 + frac) * (double)scale);
}

float voiceLogf(float x)
{
	double acc = 0.0;
	if (x < 1.0e-7f) {
		x = 1.0e-7f;
	}
	while ((double)x > 2.71828183) {
		x = (float)((double)x / 2.71828183);
		acc += 1.0;
	}
	while ((double)x * 2.71828183 < 1.0) {
		x = (float)((double)x * 2.71828183);
		acc -= 1.0;
	}

	double u = (double)(x - 1.0f);
	double uPlus2 = u + 2.0;
	double cf = (2.0 * u) / (uPlus2 + (-u * u) / (3.0 * uPlus2 + (-4.0 * u * u) / (5.0 * uPlus2)));
	return (float)(cf + acc);
}

float voiceLog10f(float x)
{
	return voiceLogf(x) * 0.43429f;
}

float voiceSinf(float x)
{
	float sign = 1.0f;
	double xd = (double)x;
	while (xd > 1.57079633) {
		xd -= 3.14159265;
		sign = -sign;
		x = (float)xd;
		xd = (double)x;
	}
	while (xd < -1.57079633) {
		xd += 3.14159265;
		sign = -sign;
		x = (float)xd;
		xd = (double)x;
	}

	float x2 = x * x;
	double c3 = (double)(42.0f - x2);
	double c2 = (double)(20.0f - x2) + (double)(20.0f * x * x) / c3;
	double c1 = (double)(6.0f - x2) + (double)(6.0f * x * x) / c2;
	double denom = 1.0 + (double)x2 / c1;
	return (float)((double)sign * (xd / denom));
}

float voiceCosf(float x)
{
	return voiceSinf((float)((double)x + 1.57079633));
}

float voiceSqrtf(float x)
{
	return voiceExpf(voiceLogf(x) * 0.5f);
}
