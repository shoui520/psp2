/*
 * M521 GFSR pseudo-random number generator used by SceLibPspnetAdhoc and SceLibRudp.
 */

#include "pspnet_adhoc_internal.h"

static int g_r521Index;
static SceUInt32 g_r521State[521];

static inline void r521Step(void)
{
	int i;

	for (i = 0; i < 32; i++)
		g_r521State[i] ^= g_r521State[i + 489];
	for (i = 32; i < 521; i++)
		g_r521State[i] ^= g_r521State[i - 32];
}

__attribute__((noinline))
void adhocRandSeed(SceUInt32 seed)
{
	int i, j;
	SceUInt32 bits = 0;

	for (i = 0; i <= 16; i++) {
		for (j = 0; j < 32; j++) {
			seed = 1566083941U * seed + 1U;
			bits = (bits >> 1) | (seed & 0x80000000U);
		}
		g_r521State[i] = bits;
	}
	g_r521State[16] = (g_r521State[16] << 23) ^ (g_r521State[0] >> 9) ^ g_r521State[15];
	for (i = 17; i <= 520; i++) {
		g_r521State[i] = (g_r521State[i - 17] << 23) ^
				 (g_r521State[i - 16] >> 9) ^
				 g_r521State[i - 1];
	}
	r521Step();
	r521Step();
	r521Step();
	g_r521Index = 520;
}

__attribute__((noinline))
SceUInt32 adhocRandNext(void)
{
	if (++g_r521Index >= 521) {
		r521Step();
		g_r521Index = 0;
	}
	return g_r521State[g_r521Index];
}
