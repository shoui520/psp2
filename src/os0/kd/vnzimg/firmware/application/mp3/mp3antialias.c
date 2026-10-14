/*
 * The alias reduction butterflies between the long subbands (ISO/IEC
 * 11172-3 2.4.3.4.10.1; VLIW, 0x8e8bf8): eight per boundary, with Q31
 * products summed in the IVC2's saturating accumulators.
 */
#include "mp3.h"

#define SUBBAND		18
#define BUTTERFLIES	8

/* cs[i] = 1 / sqrt(1 + c[i]^2), ca[i] = c[i] / sqrt(1 + c[i]^2), Q31 */
static const int32_t cs[BUTTERFLIES] = {
	1841452036, 1893526520, 2039311995, 2111652008, 2137858231, 2145680960, 2147267171, 2147468948,
};

static const int32_t ca[BUTTERFLIES] = {
	-1104871221, -1013036688, -672972958, -390655621, -203096531, -87972919, -30491193, -7945635,
};

void mp3Antialias(int32_t *work, uint16_t boundaries)
{
	uint32_t b, i;

	for (b = 1; b <= boundaries; b++) {
		int32_t *edge = work + b * SUBBAND;

		for (i = 0; i < BUTTERFLIES; i++) {
			int32_t lower = edge[-1 - (int32_t)i], upper = edge[i];

			edge[-1 - (int32_t)i] = ivc2High(ivc2SubtractWide(ivc2Product(cs[i], lower),
				ivc2Product(ca[i], upper)));
			edge[i] = ivc2High(ivc2AddWide(ivc2Product(ca[i], lower),
				ivc2Product(cs[i], upper)));
		}
	}
}
