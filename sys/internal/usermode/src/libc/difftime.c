/* difftime function */
#include <time.h>
#include "xtime.h"

double difftime(time_t t1, time_t t0)
{
	t0 -= _TBIAS;	/* make them unsigned since 1900 */
	t1 -= _TBIAS;
	return t0 <= t1 ? (double)(t1 - t0) : -(double)(t0 - t1);
}
