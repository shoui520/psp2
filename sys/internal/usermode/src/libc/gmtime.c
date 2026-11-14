/* gmtime function */
#include <time.h>

#include "libc_internal.h"
#include "psp2io.h"

struct tm *gmtime(const time_t *tod)
{
	return _Gmtime_r(tod, _Gettmbuf());
}
