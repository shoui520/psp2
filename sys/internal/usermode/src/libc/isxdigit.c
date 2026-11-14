/* isxdigit function */
#include <ctype.h>

int (isxdigit)(int c)
{
	return _Getchrtype(c) & _XD;
}
