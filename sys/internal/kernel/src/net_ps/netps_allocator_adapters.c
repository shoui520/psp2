/* Shared SceNetPs zero-filled allocation and release adapters. */

#include <net_ps/netps.h>

void *sceNetPsCalloc(SceSize size)
{
	return sceNetPsMalloc(size, 3, 8);
}

void sceNetPsRelease(void *ptr)
{
	sceNetPsFree(ptr);
}
