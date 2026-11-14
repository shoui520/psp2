/*
 * _WLitob function: convert an integer for wprintf's d, i, o, u, x and X
 * conversions, with the precision's leading zeros and '#''s octal zero.
 */
#include <wchar.h>

#include "xwstdio.h"

#define BUFLEN	24	/* the digits of the longest integer, octal */

void _WLitob(_WPft *px, wchar_t code)
{
	static const wchar_t ldigs[16] = L"0123456789abcdef";
	static const wchar_t udigs[16] = L"0123456789ABCDEF";
	wchar_t ac[BUFLEN];
	const wchar_t *digs = code == L'X' ? udigs : ldigs;
	long long base = code == L'o' ? 8 : code != L'x' && code != L'X' ? 10 : 16;
	int i = BUFLEN;
	unsigned long long ulval = px->v.li;

	if ((code == L'd' || code == L'i') && px->v.li < 0)
		ulval = -ulval;	/* safe against overflow */
	if (ulval != 0 || px->prec != 0)
		ac[--i] = digs[ulval % base];
	for (ulval /= base; 0 < ulval && 0 < i;) {	/* the other digits */
		unsigned long long quot = ulval / base;

		ac[--i] = digs[ulval - quot * base];
		ulval = quot;
	}
	if (base == 8 && (px->flags & _FNO) && (BUFLEN <= (unsigned int)i || ac[i] != L'0'))
		ac[--i] = L'0';	/* '#': a leading zero */
	px->n1 = BUFLEN - i;
	wmemcpy(px->s, &ac[i], px->n1);
	if (px->n1 < px->prec) {	/* the precision's leading zeros */
		px->nz0 = px->prec - px->n1;
		px->flags &= ~_FZE;
	} else if (px->prec < 0 && (px->flags & (_FMI | _FZE)) == _FZE
		   && 0 < (i = px->width - px->n0 - px->nz0 - px->n1)) {	/* '0': pad with zeros */
		px->nz0 = i;
	}
}
