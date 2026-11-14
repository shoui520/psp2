/* isblank function */
#include <ctype.h>

int (isblank)(int c)
{
	return _Getchrtype(c) & (_SP|_XB);
}
