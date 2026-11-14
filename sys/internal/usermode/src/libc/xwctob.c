/*
 * _Wctob function: the single byte a wide character converts to, else EOF.
 */
#include <stdio.h>
#include <wchar.h>
#include <xwchar.h>

int _Wctob(wint_t wc)
{
	if (wc == WEOF) {
		return EOF;
	} else {	/* convert it and count the bytes */
		char buf[4];
		_Mbstinit(mbst);

		return _Wctomb(buf, (wchar_t)wc, &mbst) == 1 ? (unsigned char)buf[0] : EOF;
	}
}
