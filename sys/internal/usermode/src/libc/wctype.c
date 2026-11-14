/* wctype function */
#include <string.h>
#include <wctype.h>

#include "xwctype.h"

wctype_t wctype(const char *name)
{
	const _Wctab *p = _Wctype;
	int n;

	for (n = 1; p[n]._Name != NULL; ++n)
		if (strcmp(p[n]._Name, name) == 0)
			break;
	return p[n]._Name == NULL ? (wctype_t)0 : (wctype_t)n;
}
