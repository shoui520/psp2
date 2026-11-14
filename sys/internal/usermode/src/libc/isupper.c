/* isupper function */
#include <ctype.h>

int (isupper)(int c)
{
	return _Getchrtype(c) & _UP;
}
