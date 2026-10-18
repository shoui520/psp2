/* Timer APIs forward clock pointers and marshal the info size request. */
#include "libkernel_backends.h"
#include <scetypes.h>

struct TimerInfoRequest {
	SceSize size;
};


SceUID sceKernelCreateTimer(const char *name, SceUInt32 attributes,
			  const SceKernelTimerOptParam *option)
{
	return _sceKernelCreateTimer(name, attributes, option);
}

int sceKernelGetTimerBase(SceUID id, SceKernelSysClock *clock)
{
	return _sceKernelGetTimerBase(id, clock);
}

int sceKernelGetTimerTime(SceUID id, SceKernelSysClock *clock)
{
	return _sceKernelGetTimerTime(id, clock);
}

int sceKernelSetTimerTime(SceUID id, SceKernelSysClock *clock)
{
	return _sceKernelSetTimerTime(id, clock);
}

int sceKernelSetTimerEvent(SceUID id, SceInt32 type, SceKernelSysClock *interval,
			   SceInt32 repeat)
{
	return _sceKernelSetTimerEvent(id, type, interval, repeat);
}

int sceKernelCancelTimer(SceUID id, SceUInt32 *waiting)
{
	return _sceKernelCancelTimer(id, waiting);
}

int sceKernelGetTimerInfo(SceUID id, SceKernelTimerInfo *info)
{
	struct TimerInfoRequest request = { info ? info->size : 0 };

	return _sceKernelGetTimerInfo(id, info, &request);
}

int sceKernelGetTimerEventRemainingTime(SceUID id, SceKernelSysClock *clock)
{
	return _sceKernelGetTimerEventRemainingTime(id, clock);
}
