/* strerrorlen_s function */
#include <stdlib.h>
#include <string.h>
#include "libc_internal.h"

size_t strerrorlen_s(errno_t errnum)
{
	return strlen(_Strerror(errnum, NULL));
}
