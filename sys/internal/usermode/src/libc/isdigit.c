/* isdigit function */
#include <ctype.h>

int (isdigit)(int c)
{
	return _Getchrtype(c) & _DI;
}
