/*
 * _Strxfrm function: transform the next part of a string ("C" locale: copy it).
 */
#include <stddef.h>

#include "xstrxfrm.h"

size_t _Strxfrm(char *buf, size_t n, _Xfrmctl *ps)
{
	size_t i;

	ps->s2 = ps->s1;
	for (i = 0; i < n; ++buf) {
		char c = *ps->s2;

		++i;
		*buf = c;
		if (c == '\0')
			break;
		++ps->s2;
	}
	ps->s1 = ps->s2;
	return i;
}
