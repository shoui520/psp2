#include "g729_internal.h"

int32_t g729ParityPitch(int32_t pitchIndex)
{
	int32_t shifted = pitchIndex >> 1;
	int32_t sum = 1;
	for (int i = 0; i < 6; i++) {
		shifted >>= 1;
		sum += (shifted & 1);
	}
	return sum & 1;
}

int32_t g729CheckParityPitch(int32_t pitchIndex, int32_t parityBit)
{
	int32_t shifted = pitchIndex >> 1;
	int32_t sum = 1;
	for (int i = 0; i < 6; i++) {
		shifted >>= 1;
		sum += (shifted & 1);
	}
	sum += parityBit;
	return sum & 1;
}

static void g729WriteBits(uint8_t *outBytes, int32_t *bitPos, int32_t value, int32_t numBits)
{
	for (int32_t bit = numBits - 1; bit >= 0; bit--) {
		int32_t pos = *bitPos;
		if (((value >> bit) & 1) != 0)
			outBytes[pos >> 3] |= (uint8_t)(0x80U >> (pos & 7));
		(*bitPos)++;
	}
}

static int32_t g729ReadBits(const uint8_t *inBytes, int32_t *bitPos, int32_t numBits)
{
	int32_t accum = 0;
	for (int32_t bit = 0; bit < numBits; bit++) {
		int32_t pos = *bitPos;
		int32_t curBit = (inBytes[pos >> 3] >> (7 - (pos & 7))) & 1;
		accum = (accum << 1) | curBit;
		(*bitPos)++;
	}
	return accum;
}

int32_t g729Prm2Bits(const int32_t *prm, uint8_t *outBytes, int32_t *outBitCount, uint32_t rate)
{
	int32_t frameType = prm[0];
	int32_t bitPos = 0;

	if (frameType == 0) {
		*outBitCount = 0;
		return 0;
	}

	if (frameType == 1) {
		sceClibMemset(outBytes, 0, 2);
		for (int i = 0; i < 4; i++)
			g729WriteBits(outBytes, &bitPos, prm[i + 1], g_g729BitsNoSid[i]);
		*outBitCount = 15;
		return 0;
	}

	if (rate == G729_RATE_6400) {
		sceClibMemset(outBytes, 0, 8);
		for (int i = 0; i < 11; i++) {
			if (g_g729BitsNo64k[i] > 0)
				g729WriteBits(outBytes, &bitPos, prm[i + 1], g_g729BitsNo64k[i]);
		}
		*outBitCount = (int32_t)G729_BITS_6400;
		return 0;
	}

	if (rate == G729_RATE_11800) {
		sceClibMemset(outBytes, 0, 15);
		int32_t isBwd = prm[1];
		if (isBwd != 0) {
			for (int i = 0; i < 12; i++)
				g729WriteBits(outBytes, &bitPos, prm[i + 1], g_g729BitsNo118kBwd[i]);
		} else {
			for (int i = 0; i < 14; i++)
				g729WriteBits(outBytes, &bitPos, prm[i + 1], g_g729BitsNo118kFwd[i]);
		}
		*outBitCount = (int32_t)G729_BITS_11800;
		return 0;
	}

	sceClibMemset(outBytes, 0, 10);
	for (int i = 0; i < 11; i++)
		g729WriteBits(outBytes, &bitPos, prm[i + 1], g_g729BitsNo8k[i]);
	*outBitCount = (int32_t)G729_BITS_8000;
	return 0;
}

int32_t g729Bits2Prm(const uint8_t *inBytes, int32_t *prm, uint32_t bitLength, int32_t *bfi)
{
	int32_t bitPos = 0;

	if (bitLength == G729_BITS_DTX) {
		prm[0] = *bfi;
		prm[1] = 0;
		return 0;
	}

	if (bitLength == G729_BITS_SID || bitLength == G729_BITS_SID_ALT) {
		prm[0] = *bfi;
		prm[1] = 1;
		for (int i = 0; i < 4; i++)
			prm[i + 2] = g729ReadBits(inBytes, &bitPos, g_g729BitsNoSid[i]);
		return 0;
	}

	if (bitLength == G729_BITS_6400) {
		prm[0] = *bfi;
		prm[1] = 2;
		for (int i = 0; i < 11; i++) {
			if (g_g729BitsNo64k[i] > 0)
				prm[i + 2] = g729ReadBits(inBytes, &bitPos, g_g729BitsNo64k[i]);
			else
				prm[i + 2] = 0;
		}
		return 0;
	}

	if (bitLength == G729_BITS_11800) {
		prm[0] = *bfi;
		prm[1] = 4;
		int32_t modeBits = g729ReadBits(inBytes, &bitPos, 2);
		int32_t isBwd = (modeBits == 3) ? 1 : 0;
		prm[2] = isBwd;
		if (isBwd != 0) {
			for (int i = 1; i < 12; i++)
				prm[i + 2] = g729ReadBits(inBytes, &bitPos, g_g729BitsNo118kBwd[i]);
		} else {
			for (int i = 1; i < 14; i++)
				prm[i + 2] = g729ReadBits(inBytes, &bitPos, g_g729BitsNo118kFwd[i]);
		}
		return 0;
	}

	if (bitLength == G729_BITS_8000) {
		prm[0] = *bfi;
		prm[1] = 3;
		for (int i = 0; i < 11; i++)
			prm[i + 2] = g729ReadBits(inBytes, &bitPos, g_g729BitsNo8k[i]);
		return 0;
	}

	return -1;
}
