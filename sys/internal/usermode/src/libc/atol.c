/* atol function */
#include <stdlib.h>

long atol(const char *s)
{
	return (long)_Stoul(s, 0, 10);
}
