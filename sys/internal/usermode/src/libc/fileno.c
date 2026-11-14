/* sceLibcGetFD, sceLibcGetFH and fileno functions */
#include <fios2/fios2_api.h>
#include "xstdio.h"

/* the file descriptor of a stream: the standard streams keep theirs, the others a FIOS handle */
int sceLibcGetFD(FILE *str)
{
	if (str == &_Stdin || str == &_Stdout || str == &_Stderr)
		return str->_Handle;
	if (str != NULL && sceFiosIsValidHandle(str->_Handle))
		return sceFiosFHToFileno(str->_Handle);
	return -1;
}

/* the FIOS file handle of a stream */
SceFiosFH sceLibcGetFH(FILE *str)
{
	if (str != NULL && sceFiosIsValidHandle(str->_Handle))
		return str->_Handle;
	return -1;
}

int fileno(FILE *str)
{
	if (str == &_Stdin || str == &_Stdout || str == &_Stderr)
		return sceLibcGetFD(str);
	return sceLibcGetFH(str);
}
