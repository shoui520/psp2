/* _Mbtowc function: SCE's UTF-8 decoder, for characters of up to three bytes */
#include <errno.h>

#include "xwchar.h"

/* decode a character from s into *pwc: its length, 0 for L'\0', -2 incomplete, -1 invalid */
static __attribute__((noinline)) int _Mbtowcx(wchar_t *pwc, const char *s, size_t nin, mbstate_t *pst)
{
	static const mbstate_t initial = {0};
	const char *su = s;
	signed char left = pst->_State;
	wchar_t wc = (wchar_t)pst->_Wchar;

	if (s == NULL) {	/* reset the shift state */
		*pst = initial;
		return 0;
	}
	for (; nin != 0; nin--, su++) {
		unsigned char c = *su;

		if (left > 0) {	/* a continuation byte */
			if ((c & 0xc0) != 0x80)
				goto eilseq;
			wc = (*su & 0x3f) | (wchar_t)(wc << 6);
			left--;
		} else if (!(c & 0x80)) {	/* one byte */
			wc = c;
		} else if ((c & 0xe0) == 0xc0) {	/* the first of two */
			left = 1;
			wc = c & 0x1f;
			continue;
		} else if ((c & 0xf0) == 0xe0) {	/* the first of three */
			left = 2;
			wc = c & 0x0f;
			continue;
		} else {
			goto eilseq;
		}
		if (left == 0) {	/* a whole character */
			if (pwc != NULL)
				*pwc = wc;
			pst->_State = 0;
			if (wc == 0)
				return 0;
			return su - s + 1;
		}
	}
	pst->_Wchar = wc;	/* incomplete: keep what was read */
	pst->_State = left;
	return -2;
eilseq:
	errno = EILSEQ;
	return -1;
}

int _Mbtowc(wchar_t *pwc, const char *s, size_t nin, mbstate_t *pst)
{
	return _Mbtowcx(pwc, s, nin, pst);
}
