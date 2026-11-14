/* ctime function */
#include <time.h>

char *ctime(const time_t *tod)
{
	return asctime(localtime(tod));
}
