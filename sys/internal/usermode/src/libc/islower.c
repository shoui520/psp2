/* islower function */
#include <ctype.h>

int (islower)(int c)
{
	return _Getchrtype(c) & _LO;
}
