/* _Btowc function */
#include <stdio.h>
#include <wchar.h>
#include <xwchar.h>

wint_t _Btowc(int c)
{
	if (c == EOF)
		return WEOF;
	else {	/* convert a single byte */
		char ch = (char)c;
		mbstate_t mbst = {0};
		wchar_t wc;

		return _Mbtowc(&wc, &ch, 1, &mbst) < 0 ? WEOF : wc;
	}
}
