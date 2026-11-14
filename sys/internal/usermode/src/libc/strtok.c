/* strtok function */
#include <string.h>

#include "libc_internal.h"

char *strtok(char *__restrict s1, const char *__restrict s2)
{
	char **ssave = _Getstrtokptr();
	char *sbegin, *send;

	sbegin = s1 != NULL ? s1 : *ssave;
	sbegin += strspn(sbegin, s2);
	if (*sbegin == '\0') {	/* end of scan */
		*ssave = "";
		return NULL;
	}
	send = sbegin + strcspn(sbegin, s2);
	if (*send != '\0')
		*send++ = '\0';
	*ssave = send;
	return sbegin;
}
