/* isalpha function */
#include <ctype.h>

int (isalpha)(int c)
{
	return _Getchrtype(c) & (_LO|_UP|_XA);
}
