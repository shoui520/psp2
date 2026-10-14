/* Timer helpers; source-file name absent from debug records. */
#include "library.h"
#include <psp2kern/kernel/threadmgr.h>
#include <sce/threadmgr.h>

int64_t MsproalTimerStart(int milliseconds)
{
	int duration;

	gMsproalTimerStartTime = sceKernelGetSystemTimeWide();
	duration = milliseconds * 1000;
	gMsproalTimerDuration = (int64_t)duration;
	return duration;
}

int MsproalTimerCheck(void)
{
	uint64_t now = sceKernelGetSystemTimeWide();
	uint64_t elapsed;

	if (now >= gMsproalTimerStartTime)
		elapsed = now - gMsproalTimerStartTime;
	else
		elapsed = now - gMsproalTimerStartTime - 1;
	return gMsproalTimerDuration >= elapsed ? 0 : -104;
}

void MsproalTimerStop(void)
{
	gMsproalTimerStartTime = 0;
	gMsproalTimerDuration = 0;
}

uint64_t gMsproalTimerStartTime;
uint64_t gMsproalTimerDuration;
