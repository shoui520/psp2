/*
 * spectral_data() (ISO/IEC 14496-3 4.6.3, 0x8b25b0): each band's codewords
 * split into their 4 or 2 values, with sign bits and escapes, into the
 * channel's 128-coefficient windows. Bands without a spectral codebook are
 * cleared, except the intensity bands the stereo kernel fills in. The
 * original divides by multiplying with reciprocals on the IVC2.
 */
#include "aac.h"

/* sign bits for the nonzero values of an unsigned codebook */
static int32_t readSign(AacBitstream *bits, int32_t value, uint8_t *sign)
{
	if (value == 0)
		return 0;
	return aacReadByte(bits, sign, 1);
}

/* escape_sequence(): 2^N plus N bits, N from 4 and counting the leading ones */
static int32_t readEscape(AacBitstream *bits, int32_t *value)
{
	uint16_t word;
	uint32_t count = 3;

	do {
		count++;
		if (count >= 13)
			return AAC_STATUS_ESCAPE;
		if (aacReadBits(bits, &word, 1))
			return AAC_STATUS_UNDERFLOW;
	} while (word == 1);
	if (aacReadBits(bits, &word, count))
		return AAC_STATUS_UNDERFLOW;
	*value = (1 << count) + word;
	return 0;
}

int32_t aacSpectralData(AacBitstream *bits, AacIcs *ics)
{
	const AacIcsInfo *info = &ics->info;
	const AacSectionData *section = &ics->section;
	const AacTables *tables = bits->tables;
	uint32_t isShort = info->windowSequence == AAC_EIGHT_SHORT_SEQUENCE;
	const uint32_t *swbOffset = isShort ? tables->swbOffsetShort[bits->samplingIndex] :
		tables->swbOffsetLong[bits->samplingIndex];
	int32_t numSwb = tables->numSwb[isShort][bits->samplingIndex];
	uint8_t sign[2] = { 0, 0 };
	int32_t g, sfb, w, k, band = 0, window = 0, result;

	ics->spectralStart = bits->position;
	for (g = 0; g < info->numWindowGroups; g++) {
		for (sfb = 0; sfb < numSwb; sfb++) {
			uint32_t cb = section->sfbCb[band];
			const AacCodebook *book;
			int32_t mod, offset;

			if (sfb < info->maxSfb)
				band++;
			if ((cb == AAC_INTENSITY_HCB || cb == AAC_INTENSITY_HCB2) && sfb < info->maxSfb)
				continue;
			if (cb == AAC_ZERO_HCB || cb >= AAC_RESERVED_HCB || sfb >= info->maxSfb) {
				for (w = 0; w < info->windowGroupLength[g]; w++)
					for (k = swbOffset[sfb]; k < (int32_t)swbOffset[sfb + 1]; k++)
						ics->spectrum[(window + w) * 128 + k] = 0;
				continue;
			}

			book = &tables->codebooks[cb];
			mod = 2 * book->lav + 1;
			offset = book->lav;
			if (book->isUnsigned) {
				mod = book->lav + 1;
				offset = 0;
			}
			for (w = 0; w < info->windowGroupLength[g]; w++) {
				int32_t *out = ics->spectrum + (window + w) * 128;

				for (k = swbOffset[sfb]; k < (int32_t)swbOffset[sfb + 1]; ) {
					uint16_t code;
					int32_t y, z;

					if (aacReadHuffman(bits, &code, cb))
						return AAC_STATUS_UNDERFLOW;
					if (book->dimension == 4) {
						y = code / (mod * mod * mod);
						code -= y * mod * mod * mod;
						z = code / (mod * mod);
						code -= z * mod * mod;
						y -= offset;
						z -= offset;
						if (book->isUnsigned) {
							if (readSign(bits, y, &sign[0]))
								return AAC_STATUS_UNDERFLOW;
							if (y && sign[0])
								y = -y;
							if (readSign(bits, z, &sign[0]))
								return AAC_STATUS_UNDERFLOW;
							if (z && sign[0])
								z = -z;
						}
						out[k++] = y;
						out[k++] = z;
					}

					y = code / mod;
					z = code - y * mod - offset;
					y -= offset;
					if (book->isUnsigned) {
						if (readSign(bits, y, &sign[0]) || readSign(bits, z, &sign[1]))
							return AAC_STATUS_UNDERFLOW;
					}
					if (cb == AAC_ESC_HCB) {
						if (y == 16 && (result = readEscape(bits, &y)))
							return result;
						if (z == 16 && (result = readEscape(bits, &z)))
							return result;
					}
					if (book->isUnsigned) {
						if (sign[0])
							y = -y;
						if (sign[1])
							z = -z;
					}
					out[k++] = y;
					out[k++] = z;
				}
			}
		}
		window += info->windowGroupLength[g];
	}
	return 0;
}
