/* wcstombs function */
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <xwchar.h>

size_t wcstombs(char *__restrict s, const wchar_t *__restrict wcs, size_t n)
{
	char buf[_MBMAX];
	int i;
	size_t nc;
	_Mbstinit(mbst);

	for (nc = 0; nc < n || s == NULL; ++wcs)
		if (_MBMAX <= n - nc && s != NULL) {	/* copy in place */
			if ((i = _Wctomb(s + nc, *wcs, &mbst)) < 0)
				return (size_t)-1;
			nc += i;
			if (0 < i && s[nc - 1] == '\0')
				return nc - 1;
		} else {	/* copy from the local buffer */
			if ((i = _Wctomb(buf, *wcs, &mbst)) < 0)
				return (size_t)-1;
			if ((size_t)i <= n - nc) {	/* it all fits */
				if (s != NULL)
					memcpy(s + nc, buf, i);
				nc += i;
				if (0 < i && buf[i - 1] == '\0')
					return nc - 1;
			} else {	/* it won't all fit */
				if (s != NULL) {
					memcpy(s + nc, buf, n - nc);
					return n;
				}
				nc += i;
			}
		}
	return nc;
}
