/* setbuf function */
#include "xstdio.h"

void setbuf(FILE *__restrict str, char *__restrict buf)
{
	setvbuf(str, buf, buf != NULL ? _IOFBF : _IONBF, BUFSIZ);
}
