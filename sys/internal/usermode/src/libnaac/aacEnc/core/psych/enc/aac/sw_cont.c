#include "../../../../../naac_internal.h"

int32_t __f98CreateSwitchControlChannel(NaacSwitchControl *ctrl)
{
	if (ctrl == NULL)
		return NAAC_ERR_INVALID_PARAM;
	sceClibMemset(ctrl, 0, sizeof(NaacSwitchControl));
	ctrl->currentWindow = NAAC_LONG_WINDOW;
	ctrl->nextWindow = NAAC_LONG_WINDOW;
	ctrl->windowShape = NAAC_SHAPE_SINE;
	ctrl->prevEnergy = 1.0f;
	ctrl->attackRatio = 10.0f;
	return NAAC_OK;
}

int32_t naacAdvanceSwitchControl(NaacSwitchControl *ctrl, const float *pcm, int32_t length)
{
	if (ctrl == NULL || pcm == NULL || length <= 0)
		return NAAC_LONG_WINDOW;

	float curEnergy = 1.0f;
	for (int32_t i = 0; i < length; i++)
		curEnergy += pcm[i] * pcm[i];

	int32_t attack = (curEnergy > ctrl->prevEnergy * ctrl->attackRatio) ? 1 : 0;
	ctrl->prevEnergy = curEnergy;

	if (ctrl->currentWindow == NAAC_LONG_WINDOW) {
		ctrl->nextWindow = (attack != 0) ? NAAC_START_WINDOW : NAAC_LONG_WINDOW;
	} else if (ctrl->currentWindow == NAAC_START_WINDOW) {
		ctrl->nextWindow = NAAC_SHORT_WINDOW;
	} else if (ctrl->currentWindow == NAAC_SHORT_WINDOW) {
		ctrl->nextWindow = (attack != 0) ? NAAC_SHORT_WINDOW : NAAC_STOP_WINDOW;
	} else {
		ctrl->nextWindow = NAAC_LONG_WINDOW;
	}

	ctrl->currentWindow = ctrl->nextWindow;
	return ctrl->currentWindow;
}
