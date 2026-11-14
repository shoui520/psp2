/* srand function */
#include <stdlib.h>

#include "libc_internal.h"

void srand(unsigned int seed)
{
	_Randseed = seed;
}
