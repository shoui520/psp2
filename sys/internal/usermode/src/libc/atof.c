/* atof function */
#include <stdlib.h>

double atof(const char *s)
{
	return (double)_Stod(s, 0, 0);
}
