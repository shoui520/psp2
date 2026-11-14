/* atoll function */
#include <stdlib.h>

long long atoll(const char *s)
{
	return (long long)_Stoull(s, 0, 10);
}
