/* asctime function */
#include <time.h>

#include "xtime.h"

char *asctime(const struct tm *t)
{
	char *buf = _Getasctimebuf();

	_Strftime(buf, sizeof("Sun Sep 16 01:03:52 1973\n"), "%c\n", t);
	return buf;
}
