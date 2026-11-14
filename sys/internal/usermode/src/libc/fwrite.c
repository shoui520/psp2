/* fwrite function */
#include <string.h>
#include "xstdio.h"

size_t fwrite(const void *__restrict ptr, size_t size, size_t nelem, FILE *__restrict str)
{
	const char *s = (const char *)ptr;
	size_t ns = size * nelem;
	size_t n = ns;
	size_t pending;	/* bytes buffered but not written when a flush fails */

	if (n == 0)
		return 0;
	if (size == 0)
		return nelem;
	_Lockfilelock(str);
	for (pending = 0; n != 0; pending = 0) {
		const char *pnl;
		size_t m;

		if (str->_Next >= str->_Wend && _Fwprep(str) < 0)
			break;
		pnl = (str->_Mode & _MLBF) ? memchr(s, '\n', n) : NULL;
		m = pnl != NULL ? pnl - s + 1 : n;
		if ((size_t)(str->_Wend - str->_Next) < m) {
			pnl = NULL;
			m = str->_Wend - str->_Next;
		}
		memcpy(str->_Next, s, m);
		s += m;
		n -= m;
		str->_Next += m;
		pending = str->_Next - str->_Buf;
		if (pnl != NULL && fflush(str) != 0)
			break;
	}
	if ((str->_Mode & _MNBF) && pending == 0) {
		pending = str->_Next - str->_Buf;
		if (fflush(str) == 0)
			pending = 0;
	}
	_Unlockfilelock(str);
	return (ns - (pending + n)) / size;
}
