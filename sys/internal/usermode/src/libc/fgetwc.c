/* fgetwc function */
#include <limits.h>
#include <string.h>
#include <wchar.h>
#include <xwchar.h>
#include "xstdio.h"

#define MB_INCOMPLETE	(-2)	/* _Mbtowc: needs more bytes */
#define MB_NOINPUT	(-3)	/* _Mbtowc: a character produced without input */

wint_t fgetwc(FILE *str)
{
	unsigned char *const bufend = str->_Back + sizeof(str->_Back);
	const char *s;
	size_t nback, nused, n, m;
	int nconv;
	wchar_t wc;

	_Lockfilelock(str);
	if (str->_WRback < str->_WBack + sizeof(str->_WBack) / sizeof(wchar_t)) {
		wc = *str->_WRback++;	/* deliver a pushed-back character */
		_Unlockfilelock(str);
		return wc;
	}
	for (nback = bufend - str->_Rback;;) {	/* convert a multibyte character */
		nused = nback;
		wc = 0;
		if (nback != 0 && (str->_Mode & _MWIDE)) {	/* from the pushed-back bytes */
			s = (const char *)str->_Rback;
			n = nback;
		} else {	/* from the buffer */
			if (str->_WRend <= str->_Next && _WFrprep(str) <= 0) {
				_Unlockfilelock(str);
				return WEOF;
			}
			s = (const char *)str->_Next;
			n = str->_WRend - str->_Next;
		}
		nconv = _Mbtowc(&wc, s, n, &str->_Wstate);
		if (MB_INCOMPLETE < nconv)
			break;
		if (nconv <= MB_NOINPUT)
			goto done;
		/* incomplete: collect the bytes in the pushback buffer and read more */
		if (n < MB_LEN_MAX) {
			if (nused == 0) {
				str->_Rback = bufend - n;
				memcpy(bufend - n, str->_Next, n);
				str->_Next += n;
				nused = n;
			}
		} else {
			nused = 0;
		}
		if (nused == 0 || _WFrprep(str) <= 0)
			goto error;
		m = str->_WRend - str->_Next;
		if (MB_LEN_MAX - nused < m)
			m = MB_LEN_MAX - nused;
		memmove(bufend - m - nused, str->_Rback, nused);
		memcpy(bufend - m, str->_Next, m);
		str->_Rback = bufend - m - nused;
		nback = m + nused;
		str->_Next += m;
	}
	if (nconv < 0) {
error:
		str->_Mode |= _MERR;
		_Unlockfilelock(str);
		return WEOF;
	}
	if (nconv <= 0 && wc == 0)
		nconv = strlen(s) + 1;	/* the null character */
done:
	if (nconv == MB_NOINPUT)
		nconv = 0;
	if (nused != 0)
		str->_Rback += nconv;
	else
		str->_Next += nconv;
	_Unlockfilelock(str);
	return wc;
}
