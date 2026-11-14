/* rand function */
#include <stdlib.h>

#include "libc_internal.h"

unsigned long long _Randseed = 1;	/* the generator's state, set by srand */

int rand(void)
{
	_Randseed = _Randseed * 6364136223846793005ULL + 1;
	return (int)(_Randseed >> 32) & RAND_MAX;
}
