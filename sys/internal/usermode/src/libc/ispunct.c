/* ispunct function */
#include <ctype.h>

int (ispunct)(int c)
{
	return _Getchrtype(c) & _PU;
}
