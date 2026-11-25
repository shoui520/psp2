#include "../../../../../naac_internal.h"

int32_t __f98CreateMSChannel(NaacMsChannel *ms)
{
	if (ms == NULL)
		return NAAC_ERR_INVALID_PARAM;
	sceClibMemset(ms, 0, sizeof(NaacMsChannel));
	return NAAC_OK;
}

void __f98AdvanceMSChannel(NaacMsChannel *ms, NaacCodChannel *left, NaacCodChannel *right, const NaacPsyParam *param)
{
	if (ms == NULL || left == NULL || right == NULL || param == NULL)
		return;

	if (left->blockType != right->blockType) {
		ms->msMaskPresent = 0;
		return;
	}

	int32_t usedCount = 0;
	int32_t maxSfb = left->maxSfb;
	if (maxSfb > param->numSfbLong)
		maxSfb = param->numSfbLong;

	for (int32_t sfb = 0; sfb < maxSfb; sfb++) {
		int32_t start = param->sfbOffsetLong[sfb];
		int32_t end = param->sfbOffsetLong[sfb + 1];
		float lrEnergy = 0.0f;
		float msEnergy = 0.0f;
		for (int32_t k = start; k < end; k++) {
			float lv = left->spectrum[k];
			float rv = right->spectrum[k];
			float mid = (lv + rv) * 0.5f;
			float side = (lv - rv) * 0.5f;
			lrEnergy += lv * lv + rv * rv;
			msEnergy += mid * mid + side * side;
		}
		if (msEnergy < lrEnergy * 0.9f) {
			ms->msUsed[sfb] = 1;
			usedCount++;
			for (int32_t k = start; k < end; k++) {
				float lv = left->spectrum[k];
				float rv = right->spectrum[k];
				left->spectrum[k] = (lv + rv) * 0.5f;
				right->spectrum[k] = (lv - rv) * 0.5f;
			}
		} else {
			ms->msUsed[sfb] = 0;
		}
	}

	if (usedCount == 0)
		ms->msMaskPresent = 0;
	else if (usedCount == maxSfb)
		ms->msMaskPresent = 2;
	else
		ms->msMaskPresent = 1;
}
