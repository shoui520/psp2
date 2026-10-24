/*
 * SCE: the stack's random numbers, from two MT19937 states seeded by the
 * kernel's random number generator: one for the network emulation, which a
 * test can reseed to repeat a run, and one behind NetBSD's arc4random.
 * Sony's file name is unknown.
 */

#include <libmt19937.h>
#include <sysmem/utils.h>

#include "net_ps_internal.h"

static struct {
	SceMt19937Context emulation;
	SceMt19937Context stack;
} gNetPsRandom;

void sceNetPsSeedRandom(void)
{
	SceKernelUtilsForDriver_4C9A5730(&gNetPsRandom.emulation,
					 sceNetPsGetRandom());
	SceKernelUtilsForDriver_4C9A5730(&gNetPsRandom.stack,
					 sceNetPsGetRandom());
}

int sceNetPsSeedEmulationRandom(unsigned int seed)
{
	return SceKernelUtilsForDriver_4C9A5730(&gNetPsRandom.emulation, seed);
}

unsigned int sceNetPsEmulationRandom(void)
{
	return SceKernelUtilsForDriver_92AEDFBC(&gNetPsRandom.emulation);
}

/* NetBSD's arc4random (libkern.h) */
SceUInt32 arc4random(void)
{
	return SceKernelUtilsForDriver_92AEDFBC(&gNetPsRandom.stack);
}
