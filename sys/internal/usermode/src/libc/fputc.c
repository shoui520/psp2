/* fputc function */
#include "xstdio.h"

int fputc(int ci, FILE *str)
{
	unsigned char c = (unsigned char)ci;
	unsigned char *next;
	int ans;

	_Lockfilelock(str);
	next = str->_Next;
	if ((str->_Wend <= next && (ans = _Fwprep(str), next = str->_Next, ans < 0))
	    || ((str->_Next = next + 1, *next = c,
		 (str->_Mode & _MNBF) || ((str->_Mode & _MLBF) && c == '\n'))
		&& fflush(str) != 0)) {
		_Unlockfilelock(str);
		return EOF;
	}
	_Unlockfilelock(str);
	return c;
}
