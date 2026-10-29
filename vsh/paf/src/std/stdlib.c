#include "stdlib.h"

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
