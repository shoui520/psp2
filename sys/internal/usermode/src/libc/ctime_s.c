/* ctime_s function */
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

errno_t ctime_s(char *s, rsize_t maxsize, const time_t *tod)
{
	struct tm ts;

	if (s == NULL || maxsize == 0 || RSIZE_MAX < maxsize)
		return _EXT1_FAIL("ctime_s: bad destination argument");
	if (maxsize < 26 || tod == NULL || localtime_s(tod, &ts) == NULL
	    || ts.tm_year < -1900 || 8099 < ts.tm_year) {
		*s = '\0';
		return _EXT1_FAIL("ctime_s: bad argument");
	}
	return asctime_s(s, maxsize, &ts);
}
