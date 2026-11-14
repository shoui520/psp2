/* atoi function */
#include <stdlib.h>

int atoi(const char *s)
{
	return (int)_Stoul(s, 0, 10);
}
