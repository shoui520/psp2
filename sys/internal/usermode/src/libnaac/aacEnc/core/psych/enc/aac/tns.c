#include "../../../../../naac_internal.h"

int32_t __f98CreateTnsData(NaacTnsData *tns)
{
	if (tns == NULL)
		return NAAC_ERR_INVALID_PARAM;
	sceClibMemset(tns, 0, sizeof(NaacTnsData));
	return NAAC_OK;
}

void naacApplyTns(NaacTnsData *tns, float *spectrum, int32_t length)
{
	if (tns == NULL || spectrum == NULL || length <= 4)
		return;

	float rZero = 0.0f;
	float rOne = 0.0f;
	for (int32_t i = 0; i < length - 1; i++) {
		rZero += spectrum[i] * spectrum[i];
		rOne += spectrum[i] * spectrum[i + 1];
	}

	if (rZero <= 1e-6f) {
		tns->tnsActive = 0;
		return;
	}

	float reflection = rOne / rZero;
	if (naacFabsf(reflection) < 0.4f) {
		tns->tnsActive = 0;
		return;
	}

	tns->tnsActive = 1;
	tns->numFilters = 1;
	tns->filterOrder = 1;
	tns->direction = 0;
	tns->coeffs[0] = reflection;
	tns->quantCoeffs[0] = (int32_t)(reflection * 7.0f);

	float prev = 0.0f;
	for (int32_t i = 0; i < length; i++) {
		float cur = spectrum[i];
		spectrum[i] = cur - reflection * prev;
		prev = cur;
	}
}
