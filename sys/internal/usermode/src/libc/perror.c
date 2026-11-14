/* perror function */
#include <errno.h>

#include "xstdio.h"

/*
 * SCE: perror and strerror call _Strerror with the error code alone (r1 is not set), where
 * strerror_s and strerrorlen_s pass a null buffer.
 */
char *_Strerror();

void perror(const char *s)
{
	if (s != NULL && *s != '\0') {
		fputs(s, stderr);
		fputs(": ", stderr);
	}
	fputs(_Strerror(errno), stderr);
	fputc('\n', stderr);
}
