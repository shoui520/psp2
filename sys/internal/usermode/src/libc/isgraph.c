/* isgraph function */
#include <ctype.h>

int (isgraph)(int c)
{
	return _Getchrtype(c) & (_DI|_LO|_PU|_UP|_XA);
}
