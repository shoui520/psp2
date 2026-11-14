/* iscntrl function */
#include <ctype.h>

int (iscntrl)(int c)
{
	return _Getchrtype(c) & _BB;
}
