#include "update_internal.h"
#include "motion_update.h"
#include <threadmgr/threadmgr.h>

int MotionUtilSendCommand(void)
{
	unsigned int resultPattern;
	int retries = 2;
	int result;

	for (;;) {
		resultPattern = 0;
		if (SceMotionDevForDriver_09918DAE(MotionUtilCommandComplete,
			    &updateMotionRequest, &updateMotionResponse,
			    &updateMotionEvent) == 0) {
			result = sceKernelWaitEventFlag(updateMotionEvent.eventFlagId,
				3, 5, &resultPattern, NULL);
			if (result == 0 && !(resultPattern & 2) &&
			    updateMotionResponse.payload.control == 6)
				return result;
		}
		if (--retries == 0)
			return (int)0x800f0205;
	}
}
