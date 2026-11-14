/* wctrans function */
#include <string.h>
#include <wctype.h>

#include "xwctype.h"

wctrans_t wctrans(const char *name)
{
	const _Wctab *p = _Wctrans;
	int n;

	for (n = 1; p[n]._Name != NULL; ++n)
		if (strcmp(p[n]._Name, name) == 0)
			return (wctrans_t)n;
	return 0;
}
