#include <psp2kern/kernel/threadmgr/event_flags.h>
#include "bbmc_update.h"

int BbmcUtilCallback(int notifyId, int notifyCount, int notifyArg, void *userData)
{
	unsigned int events = notifyArg;
	unsigned int pattern = events & 1 ? 0x10 : 0;

	if (events & 2)
		pattern |= 0x20;
	if (events & 4)
		pattern |= 0x40;
	if (events & 8)
		pattern |= 0x80;
	if (events & 0x10)
		pattern |= 0x100;
	if (events & 0x20)
		pattern |= 0x200;
	if (events & 0x40)
		pattern |= 0x400;
	if (events & 0x80)
		pattern |= 0x800;
	if (events & 0x100)
		pattern |= (events & 0x7f000000) | 0x1000;
	if (events & 0x200)
		pattern |= 0x2000;
	if (events & 0x400)
		pattern |= 0x4000;
	if (events & 0x800)
		pattern |= 0x8000;
	if (events & 0x1000)
		pattern |= 0x10000;
	if (events & 0x2000)
		pattern |= 0x20000;
	if (events & 0x4000)
		pattern |= 0x40000;
	if (events & 0x8000)
		pattern |= 0x80000;
	if (events & 0x400000)
		pattern |= 0x400000;
	if (events & 0x800000)
		pattern |= 0x800000;
	if (events)
		SceBbmcForDriver_E83B73CA(updateBbmcContext.subscriber.subscriberId, events);
	if (pattern)
		sceKernelSetEventFlag(updateBbmcContext.eventFlagId, pattern);
	return SceBbmcForDriver_435858B1(updateBbmcContext.subscriber.subscriberId);
}
