/* localtime function */
#include <time.h>

#include "libc_internal.h"
#include "psp2io.h"

struct tm *localtime(const time_t *tod)
{
	return _Localtime_r(tod, _Gettmbuf());
}
