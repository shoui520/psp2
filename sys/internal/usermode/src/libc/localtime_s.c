/* localtime_s function */
#include <stdlib.h>
#include <time.h>

#include "psp2io.h"

struct tm *localtime_s(const time_t *__restrict tod, struct tm *__restrict ts)
{
	if (tod == NULL || ts == NULL) {
		_EXT1_FAIL("localtime_s: bad argument");
		return NULL;
	}
	return _Localtime_r(tod, ts);
}
