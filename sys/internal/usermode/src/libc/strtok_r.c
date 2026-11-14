/* strtok_r function */
#include <string.h>

char *strtok_r(char *s, const char *sep, char **last)
{
	char *sbegin, *send;

	sbegin = s != NULL ? s : *last;
	sbegin += strspn(sbegin, sep);
	if (*sbegin == '\0') {	/* end of scan */
		*last = "";
		return NULL;
	}
	send = sbegin + strcspn(sbegin, sep);
	if (*send != '\0')
		*send++ = '\0';
	*last = send;
	return sbegin;
}
