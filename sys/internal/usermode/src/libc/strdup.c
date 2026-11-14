/* strdup function */
#include <stdlib.h>
#include <string.h>

char *strdup(const char *s)
{
	size_t n = strlen(s) + 1;
	char *p = (char *)malloc(n);

	if (p != NULL)
		strncpy(p, s, n);
	return p;
}
