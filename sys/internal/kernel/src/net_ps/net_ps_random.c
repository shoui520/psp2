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
	sceMt19937Init(&gNetPsRandom.emulation,
					 sceNetPsGetRandom());
	sceMt19937Init(&gNetPsRandom.stack,
					 sceNetPsGetRandom());
}

int sceNetPsSeedEmulationRandom(unsigned int seed)
{
	return sceMt19937Init(&gNetPsRandom.emulation, seed);
}

unsigned int sceNetPsEmulationRandom(void)
{
	return sceMt19937UInt(&gNetPsRandom.emulation);
}

/* NetBSD's arc4random (libkern.h) */
SceUInt32 arc4random(void)
{
	return sceMt19937UInt(&gNetPsRandom.stack);
}
