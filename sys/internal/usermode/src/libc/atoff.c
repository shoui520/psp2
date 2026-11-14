/* atoff function */
#include <stdlib.h>

float atoff(const char *s)
{
	return (float)_Stof(s, 0, 0);
}
