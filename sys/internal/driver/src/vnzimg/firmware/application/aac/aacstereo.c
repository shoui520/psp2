/*
 * M/S stereo (ISO/IEC 14496-3 4.6.8.1, 0x8b3d88): the bands ms_used marks,
 * or all with ms_mask_present 2, become left = mid + side and right =
 * mid - side. Bands whose right channel is intensity coded, or whose both
 * channels are noise, are left alone.
 */
#include "aac.h"

void aacStereo(AacBitstream *bits, AacChannelPairElement *cpe)
{
	const AacIcsInfo *info = &cpe->ics[0]->info;
	uint32_t isShort = info->windowSequence == AAC_EIGHT_SHORT_SEQUENCE;
	const uint32_t *swbOffset = isShort ? bits->tables->swbOffsetShort[bits->samplingIndex] :
		bits->tables->swbOffsetLong[bits->samplingIndex];
	int32_t maxSfb = info->maxSfb, window = 0, g, w, sfb, k;

	for (g = 0; g < info->numWindowGroups; g++) {
		for (w = 0; w < info->windowGroupLength[g]; w++, window += 128) {
			int32_t band = g * maxSfb;

			for (sfb = 0; sfb < maxSfb; sfb++, band++) {
				int32_t *left = cpe->ics[0]->spectrum + window;
				int32_t *right = cpe->ics[1]->spectrum + window;
				uint32_t leftCb = cpe->ics[0]->section.sfbCb[band];
				uint32_t rightCb = cpe->ics[1]->section.sfbCb[band];

				if (cpe->msMaskPresent != 2 &&
				    !(cpe->msMaskPresent == 1 && cpe->msUsed[band]))
					continue;
				if (rightCb >= AAC_INTENSITY_HCB)
					continue;
				if ((int8_t)leftCb == AAC_NOISE_HCB && rightCb == AAC_NOISE_HCB)
					continue;
				for (k = swbOffset[sfb]; k < (int32_t)swbOffset[sfb + 1]; k++) {
					int32_t mid = left[k], side = right[k];

					left[k] = mid + side;
					right[k] = mid - side;
				}
			}
		}
	}
}
