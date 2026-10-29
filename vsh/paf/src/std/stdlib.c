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

void *sce_paf_bsearch(const void *key, const void *base, size_t count,
		      size_t element_size,
		      int (*compare)(const void *element, const void *key))
{
	int32_t first = 0;
	int32_t last;

	if (!key || !compare || !count || !element_size)
		return NULL;
	last = (int32_t)((uint32_t)count - 1);
	while (first <= last) {
		/* The native midpoint uses wrapped signed addition, rounded to zero. */
		int32_t middle = (int32_t)((uint32_t)first + (uint32_t)last) / 2;
		const void *element = (const void *)((uintptr_t)base +
			(uint32_t)middle * (uint32_t)element_size);
		int comparison = compare(element, key);

		if (!comparison)
			return (void *)element;
		if (comparison > 0)
			last = (int32_t)((uint32_t)middle - 1);
		else
			first = (int32_t)((uint32_t)middle + 1);
	}
	return NULL;
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
