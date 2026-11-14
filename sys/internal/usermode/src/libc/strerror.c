/* strerror function */
#include <string.h>

/* SCE: called with the error code alone, as in perror.c */
char *_Strerror();

char *strerror(int errcode)
{
	return _Strerror(errcode);
}
