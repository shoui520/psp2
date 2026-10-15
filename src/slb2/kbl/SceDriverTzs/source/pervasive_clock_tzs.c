/*
 * pervasive_clock_tzs.c: the device clock selects of ScePervasiveBaseClk
 * and the DSI pixel clocks' state, as SceLowio keeps them.
 */
#include "pervasive_tzs.h"
#include <sce/pervasive_pixel.h>


static PixelClockContext s_pixelClock[2];

int GpuClockSet(unsigned int core, unsigned int mp)
{
	if (core > 7 || mp > 7)
		return PERVASIVE_ERROR;
	s_baseClock->gpuClock = core << 16 | mp;
	PervasiveDmb();
	return 0;
}

int VeneziaClockSet(unsigned int clock)
{
	if (clock - 1 > 6)
		return PERVASIVE_ERROR;
	s_baseClock->veneziaClock = clock;
	PervasiveDmb();
	return 0;
}

int VipClockSet(unsigned int clock)
{
	if (clock - 1 > 5)
		return PERVASIVE_ERROR;
	s_baseClock->vipClock = clock;
	PervasiveDmb();
	return 0;
}

int PixelClockInit(void)
{
	s_pixelClock[0].setting = NULL;
	s_pixelClock[1].setting = NULL;
	s_pixelClock[0].registers = &s_baseClock->dsi0PixelClock;
	s_pixelClock[0].pllConfig = s_pixelClock[0].registers->pllConfig;
	s_pixelClock[1].registers = &s_baseClock->dsi1PixelClock;
	s_pixelClock[1].pllConfig = s_pixelClock[1].registers->pllConfig;
	return 0;
}

int PervasiveMiscInit(void)
{
	return 0;
}
