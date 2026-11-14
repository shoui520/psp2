/* fputwc function */
#include <limits.h>
#include <string.h>
#include <wchar.h>
#include <xwchar.h>
#include "xstdio.h"

wint_t fputwc(wchar_t c, FILE *str)
{
	char buf[MB_LEN_MAX];
	char *s = buf;
	unsigned char *next;
	int n = -1;	/* bytes left to copy; -1 until converted */
	int m;

	_Lockfilelock(str);
	for (next = str->_Next;;) {
		unsigned char *wend = str->_WWend;

		if (wend <= next) {
			int ans = _WFwprep(str);

			wend = str->_WWend;
			next = str->_Next;
			if (ans < 0)
				goto error;
		}
		if ((m = wend - next) == 0)
			m = 1;
		if (n <= 0) {	/* convert */
			if (MB_LEN_MAX <= m) {	/* straight into the buffer */
				if ((n = _Wctomb((char *)next, c, &str->_Wstate)) < 0)
					goto error;
				str->_Next += n;
				break;
			}
			n = _Wctomb(s, c, &str->_Wstate);
			next = str->_Next;
			if (n < 0)
				goto error;
		}
		if (n < m)
			m = n;
		memcpy(next, s, m);
		n -= m;
		str->_Next = next = str->_Next + m;
		s += m;
		if (n == 0)
			break;
	}
	if (((str->_Mode & _MNBF) || ((str->_Mode & _MLBF) && c == L'\n')) && fflush(str) != 0) {
error:
		_Unlockfilelock(str);
		return WEOF;
	}
	_Unlockfilelock(str);
	return c;
}
