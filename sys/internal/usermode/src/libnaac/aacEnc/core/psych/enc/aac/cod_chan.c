#include "../../../../../naac_internal.h"

int32_t __f98CreateCodChannel(NaacCodChannel *cod, const NaacPsyParam *param)
{
	if (cod == NULL || param == NULL)
		return NAAC_ERR_INVALID_PARAM;

	int32_t sfbCount = 0;
	int32_t cbCount = 0;
	if (GetPsyBlockParam(param, NAAC_LONG_WINDOW, &sfbCount) != NAAC_OK)
		return NAAC_ERR_INTERNAL;
	if (GetAdaptBlockParam(param, NAAC_LONG_WINDOW, &cbCount) != NAAC_OK)
		return NAAC_ERR_INTERNAL;

	sceClibMemset(cod, 0, sizeof(NaacCodChannel));
	cod->blockType = NAAC_LONG_WINDOW;
	cod->windowShape = NAAC_SHAPE_SINE;
	cod->numWindowGroups = 1;
	cod->windowGroupLength[0] = 1;
	cod->maxSfb = sfbCount;
	cod->globalGain = 128;
	return NAAC_OK;
}

void naacFillCodChannel(NaacCodChannel *cod, const NaacPsyChannel *psy, const NaacPsyParam *param)
{
	if (cod == NULL || psy == NULL || param == NULL)
		return;

	cod->blockType = psy->switchCtrl.currentWindow;
	cod->windowShape = psy->switchCtrl.windowShape;
	cod->numWindowGroups = 1;
	cod->windowGroupLength[0] = 1;
	cod->maxSfb = (cod->blockType == NAAC_SHORT_WINDOW) ? param->numSfbShort : param->numSfbLong;

	sceClibMemcpy_safe(cod->spectrum, psy->mdctSpectrum, sizeof(cod->spectrum));
	sceClibMemcpy_safe(cod->threshold, psy->thrChan.thr.threshold, sizeof(cod->threshold));
}
