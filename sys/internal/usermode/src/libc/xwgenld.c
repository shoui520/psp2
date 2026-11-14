/*
 * _WGenld function: lay out the digits _WLdtob generated, nsig of them at p
 * with the decimal exponent xexp, in the f or e style (g picks one), into
 * the wide print state's fields. The point is L'.' widened.
 */
#include <stdlib.h>
#include <wchar.h>

#include <xwchar.h>

#include "xwstdio.h"

void _WGenld(_WPft *px, wchar_t code, wchar_t *p, short nsig, short xexp)
{
	wint_t wpoint = _Btowc('.');
	const wchar_t point = wpoint == WEOF ? L'.' : (wchar_t)wpoint;

	if (nsig <= 0) {
		nsig = 1;
		p = L"0";
	}
	if (code == L'f' || code == L'F'
	    || ((code == L'g' || code == L'G') && -4 <= xexp && xexp < px->prec)) {	/* the f style */
		++xexp;	/* the digits before the point */
		if (code != L'f' && code != L'F') {	/* the fixup for g */
			if (!(px->flags & _FNO) && nsig < px->prec)
				px->prec = nsig;
			if ((px->prec -= xexp) < 0)
				px->prec = 0;
		}
		if (xexp <= 0) {	/* digits only after the point */
			px->s[px->n1++] = L'0';
			if (0 < px->prec || (px->flags & _FNO))
				px->s[px->n1++] = point;
			if (px->prec < -xexp)
				xexp = -px->prec;
			px->nz1 = -xexp;
			px->prec += xexp;
			if (px->prec < nsig)
				nsig = px->prec;
			wmemcpy(&px->s[px->n1], p, px->n2 = nsig);
			px->nz2 = px->prec - nsig;
		} else if (nsig < xexp) {	/* zeros before the point */
			wmemcpy(&px->s[px->n1], p, nsig);
			px->n1 += nsig;
			px->nz1 = xexp - nsig;
			if (0 < px->prec || (px->flags & _FNO)) {
				px->s[px->n1] = point;
				++px->n2;
			}
			px->nz2 = px->prec;
		} else {	/* enough digits before the point */
			wmemcpy(&px->s[px->n1], p, xexp);
			px->n1 += xexp;
			nsig -= xexp;
			if (0 < px->prec || (px->flags & _FNO))
				px->s[px->n1++] = point;
			if (px->prec < nsig)
				nsig = px->prec;
			wmemcpy(&px->s[px->n1], p + xexp, nsig);
			px->n1 += nsig;
			px->nz1 = px->prec - nsig;
		}
	} else {	/* the e style */
		char buf[10];
		int n;

		if (code == L'g' || code == L'G') {	/* the fixup for g */
			if (nsig < px->prec && !(px->flags & _FNO))
				px->prec = nsig;
			if (--px->prec < 0)
				px->prec = 0;
			code = code == L'g' ? L'e' : L'E';
		} else if (code == L'a') {
			code = L'p';
		} else if (code == L'A') {
			code = L'P';
		}
		px->s[px->n1++] = *p++;
		if (0 < px->prec || (px->flags & _FNO))
			px->s[px->n1++] = point;
		if (0 < px->prec) {	/* the fraction digits */
			if (px->prec < --nsig)
				nsig = px->prec;
			wmemcpy(&px->s[px->n1], p, nsig);
			px->n1 += nsig;
			px->nz1 = px->prec - nsig;
		}
		p = &px->s[px->n1];	/* the exponent */
		*p++ = code;
		if (0 <= xexp) {
			*p++ = L'+';
		} else {
			*p++ = L'-';
			xexp = -xexp;
		}
		for (n = 0; 0 < xexp; ++n) {	/* its decimal digits, low first */
			div_t qr = div(xexp, 10);

			xexp = (short)qr.quot;
			buf[n] = (char)qr.rem;
		}
		if (n < 2 && (code == L'e' || code == L'E'))
			*p++ = L'0';
		if (n == 0)
			*p++ = L'0';
		for (; 0 < n;)
			*p++ = buf[--n] + L'0';
		px->n2 = p - &px->s[px->n1];
	}
	if ((px->flags & (_FMI | _FZE)) == _FZE) {	/* pad with leading zeros */
		int n = px->n0 + px->n1 + px->nz1 + px->n2 + px->nz2;

		if (n < px->width)
			px->nz0 = px->width - n;
	}
}
