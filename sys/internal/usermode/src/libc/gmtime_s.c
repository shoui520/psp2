/* gmtime_s function */
#include <stdlib.h>
#include <time.h>

#include "psp2io.h"

struct tm *gmtime_s(const time_t *__restrict tod, struct tm *__restrict ts)
{
	if (tod == NULL || ts == NULL) {
		_EXT1_FAIL("gmtime_s: bad argument");
		return NULL;
	}
	return _Gmtime_r(tod, ts);
}
