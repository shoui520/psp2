/* isspace function */
#include <ctype.h>

int (isspace)(int c)
{
	return _Getchrtype(c) & (_CN|_SP|_XS);
}
