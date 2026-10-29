#include "stdlib.h"

/* Sony's PAF random state starts at one in all three firmware variants. */
static uint32_t paf_random_seed = 1;

void sce_paf_srand(unsigned int seed)
{
	paf_random_seed = seed;
}

int sce_paf_rand(void)
{
	paf_random_seed = paf_random_seed * 0x41C64E6DU + 0x3039U;
	return (paf_random_seed >> 16) & 0x7fff;
}

int sce_paf_abs(int value)
{
	if (value < 0)
		return (int)(0U - (uint32_t)value);
	return value;
}

int ScePafStdc_D41FD265(void)
{
	return 0;
}
