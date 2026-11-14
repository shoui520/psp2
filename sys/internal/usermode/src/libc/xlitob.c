/*
 * _Litob function: convert an integer for printf's d, i, o, u, x and X
 * conversions, with the precision's leading zeros and '#''s octal zero.
 */
#include <string.h>

#include "xstdio.h"

#define BUFLEN	24	/* the digits of the longest integer, octal */

void _Litob(_Pft *px, char code)
{
	char ac[BUFLEN];
	const char *digs = code == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
	long long base = code == 'o' ? 8 : code != 'x' && code != 'X' ? 10 : 16;
	int i = sizeof(ac);
	unsigned long long ulval = px->v.li;

	if ((code == 'd' || code == 'i') && px->v.li < 0)
		ulval = -ulval;	/* safe against overflow */
	if (ulval != 0 || px->prec != 0)
		ac[--i] = digs[ulval % base];
	for (ulval /= base; 0 < ulval && 0 < i;) {	/* the other digits */
		unsigned long long quot = ulval / base;

		ac[--i] = digs[ulval - quot * base];
		ulval = quot;
	}
	if (base == 8 && (px->flags & _FNO) && (sizeof(ac) <= (unsigned int)i || ac[i] != '0'))
		ac[--i] = '0';	/* '#': a leading zero */
	px->n1 = sizeof(ac) - i;
	memcpy(px->s, &ac[i], px->n1);
	if (px->n1 < px->prec) {	/* the precision's leading zeros */
		px->nz0 = px->prec - px->n1;
		px->flags &= ~_FZE;
	} else if (px->prec < 0 && (px->flags & (_FMI | _FZE)) == _FZE
		   && 0 < (i = px->width - px->n0 - px->nz0 - px->n1)) {	/* '0': pad with zeros */
		px->nz0 = i;
	}
}
