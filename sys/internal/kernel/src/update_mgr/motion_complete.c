#include "update_internal.h"
#include "motion_update.h"
#include <threadmgr/threadmgr.h>

void MotionUtilCommandComplete(int result,
	const struct MotionSensorResponse *request,
	struct MotionSensorResponse *response, void *arg)
{
	MotionUtilEventContext *context = arg;

	context->result = result;
	if (result)
		sceKernelSetEventFlag(context->eventFlagId, 2);
	else
		sceKernelSetEventFlag(context->eventFlagId, 1);
}
