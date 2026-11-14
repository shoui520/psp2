/* rand_r function */
#include <stdlib.h>

int rand_r(unsigned int *seed)
{
	*seed = *seed * 1103515245 + 12345;
	return (int)(*seed >> 16) & RAND_MAX;
}
