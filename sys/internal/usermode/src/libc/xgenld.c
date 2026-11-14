/*
 * _Genld function: lay out the digits _Ldtob generated, nsig of them at p
 * with the decimal exponent xexp, in the f or e style (g picks one), into
 * the print state's fields.
 */
#include <stdlib.h>
#include <string.h>

#include "xstdio.h"

void _Genld(_Pft *px, char code, char *p, short nsig, short xexp)
{
	const char point = '.';

	if (nsig <= 0) {
		nsig = 1;
		p = "0";
	}
	if (code == 'f' || code == 'F'
	    || ((code == 'g' || code == 'G') && -4 <= xexp && xexp < px->prec)) {	/* the f style */
		++xexp;	/* the digits before the point */
		if (code != 'f' && code != 'F') {	/* the fixup for g */
			if (!(px->flags & _FNO) && nsig < px->prec)
				px->prec = nsig;
			if ((px->prec -= xexp) < 0)
				px->prec = 0;
		}
		if (xexp <= 0) {	/* digits only after the point */
			px->s[px->n1++] = '0';
			if (0 < px->prec || (px->flags & _FNO))
				px->s[px->n1++] = point;
			if (px->prec < -xexp)
				xexp = -px->prec;
			px->nz1 = -xexp;
			px->prec += xexp;
			if (px->prec < nsig)
				nsig = px->prec;
			memcpy(&px->s[px->n1], p, px->n2 = nsig);
			px->nz2 = px->prec - nsig;
		} else if (nsig < xexp) {	/* zeros before the point */
			memcpy(&px->s[px->n1], p, nsig);
			px->n1 += nsig;
			px->nz1 = xexp - nsig;
			if (0 < px->prec || (px->flags & _FNO)) {
				px->s[px->n1] = point;
				++px->n2;
			}
			px->nz2 = px->prec;
		} else {	/* enough digits before the point */
			memcpy(&px->s[px->n1], p, xexp);
			px->n1 += xexp;
			nsig -= xexp;
			if (0 < px->prec || (px->flags & _FNO))
				px->s[px->n1++] = point;
			if (px->prec < nsig)
				nsig = px->prec;
			memcpy(&px->s[px->n1], p + xexp, nsig);
			px->n1 += nsig;
			px->nz1 = px->prec - nsig;
		}
	} else {	/* the e style */
		char buf[10];
		int n;

		if (code == 'g' || code == 'G') {	/* the fixup for g */
			if (nsig < px->prec && !(px->flags & _FNO))
				px->prec = nsig;
			if (--px->prec < 0)
				px->prec = 0;
			code = code == 'g' ? 'e' : 'E';
		} else if (code == 'a') {
			code = 'p';
		} else if (code == 'A') {
			code = 'P';
		}
		px->s[px->n1++] = *p++;
		if (0 < px->prec || (px->flags & _FNO))
			px->s[px->n1++] = point;
		if (0 < px->prec) {	/* the fraction digits */
			if (px->prec < --nsig)
				nsig = px->prec;
			memcpy(&px->s[px->n1], p, nsig);
			px->n1 += nsig;
			px->nz1 = px->prec - nsig;
		}
		p = &px->s[px->n1];	/* the exponent */
		*p++ = code;
		if (0 <= xexp) {
			*p++ = '+';
		} else {
			*p++ = '-';
			xexp = -xexp;
		}
		for (n = 0; 0 < xexp; ++n) {	/* its decimal digits, low first */
			div_t qr = div(xexp, 10);

			xexp = (short)qr.quot;
			buf[n] = (char)qr.rem;
		}
		if (n < 2 && (code == 'e' || code == 'E'))
			*p++ = '0';
		if (n == 0)
			*p++ = '0';
		for (; 0 < n;)
			*p++ = buf[--n] + '0';
		px->n2 = p - &px->s[px->n1];
	}
	if ((px->flags & (_FMI | _FZE)) == _FZE) {	/* pad with leading zeros */
		int n = px->n0 + px->n1 + px->nz1 + px->n2 + px->nz2;

		if (n < px->width)
			px->nz0 = px->width - n;
	}
}
