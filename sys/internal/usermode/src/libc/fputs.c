/* fputs function */
#include <string.h>
#include "xstdio.h"

int fputs(const char *__restrict s, FILE *__restrict str)
{
	_Lockfilelock(str);
	while (*s != '\0') {
		const char *pnl;
		size_t m;

		if (str->_Next >= str->_Wend && _Fwprep(str) < 0)
			goto error;
		pnl = (str->_Mode & _MLBF) ? strrchr(s, '\n') : NULL;
		m = pnl != NULL ? pnl - s + 1 : strlen(s);
		if ((size_t)(str->_Wend - str->_Next) < m) {
			pnl = NULL;
			m = str->_Wend - str->_Next;
		}
		memcpy(str->_Next, s, m);
		str->_Next += m;
		s += m;
		if (pnl != NULL && fflush(str) != 0)
			goto error;
	}
	if ((str->_Mode & _MNBF) && fflush(str) != 0) {
error:
		_Unlockfilelock(str);
		return EOF;
	}
	_Unlockfilelock(str);
	return 0;
}
