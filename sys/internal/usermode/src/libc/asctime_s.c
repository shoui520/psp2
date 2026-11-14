/* asctime_s function */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "xtime.h"

errno_t asctime_s(char *s, rsize_t maxsize, const struct tm *t)
{
	errno_t ans = 0;

	if (s == NULL || maxsize == 0 || RSIZE_MAX < maxsize)
		return _EXT1_FAIL("asctime_s: bad destination argument");
	if (maxsize < 26 || t == NULL
	    || t->tm_wday < 0 || 6 < t->tm_wday
	    || t->tm_mon < 0 || 11 < t->tm_mon
	    || t->tm_mday < 1 || 31 < t->tm_mday
	    || t->tm_hour < 0 || 59 < t->tm_hour
	    || t->tm_min < 0 || 59 < t->tm_min
	    || t->tm_sec < 0 || 59 < t->tm_sec
	    || t->tm_year < -1900 || 8099 < t->tm_year) {
		*s = '\0';
		return _EXT1_FAIL("asctime_s: bad argument");
	}
	if (_Strftime(s, maxsize, "%c\n", t) == 0)
		ans = ERANGE;
	return ans;
}
