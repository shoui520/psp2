/* strxfrm function */
#include <string.h>

#include "xstrxfrm.h"

size_t _Strxfrmx(char *__restrict s1, const char *__restrict s2, size_t n)
{
	static const _Xfrmctl initial;
	_Xfrmctl xfrm = initial;
	size_t nx = 0;

	xfrm.s1 = s2;
	while (nx < n) {	/* transform into s1 */
		size_t i = _Strxfrm(s1, n - nx, &xfrm);

		s1 += i;
		nx += i;
		if (0 < i && s1[-1] == '\0')
			return nx - 1;
	}
	for (;;) {	/* count the rest */
		char buf[32];
		size_t i = _Strxfrm(buf, sizeof(buf), &xfrm);

		nx += i;
		if (0 < i && buf[i - 1] == '\0')
			return nx - 1;
	}
}

size_t strxfrm(char *__restrict s1, const char *__restrict s2, size_t n)
{
	return _Strxfrmx(s1, s2, n);
}
