/* fclose function */
#include <errno.h>
#include <stdlib.h>
#include "xstdio.h"
#include "psp2io.h"
#include "libc_internal.h"

/* close a stream; exiting: the process ends, so nothing is freed */
static int fcloseStream(FILE *str, int exiting)
{
	int status;

	if (str == NULL)
		return EOF;
	if ((str->_Mode & (_MOPENR | _MOPENW)) == 0 || str->_Handle < 0) {
		_Locksyslock(_LOCK_STREAM);
		_Lockfilelock(str);
		_Fofree(str, exiting);
		_Unlocksyslock(_LOCK_STREAM);
		errno = EBADF;
		return EOF;
	}
	_Lockfilelock(str);
	status = fflush(str);
	_Locksyslock(_LOCK_STREAM);
	if ((str->_Mode & _MALBUF) && !exiting)
		free(str->_Buf);
	str->_Buf = NULL;
	if (_Close(str->_Handle) != 0)
		status = EOF;
	if (str->_Tmpnam != NULL) {	/* remove a temporary file */
		if (remove(str->_Tmpnam) != 0)
			status = EOF;
		if (!exiting) {
			free(str->_Tmpnam);
			str->_Tmpnam = NULL;
		}
	}
	_Fofree(str, exiting);
	_Unlocksyslock(_LOCK_STREAM);
	return status;
}

/* close every open stream (module_exit) */
void _Fcloseall(int exiting)
{
	size_t i;

	for (i = 0; i < FOPEN_MAX; ++i) {
		FILE *str = _Files[i];

		if (str != NULL && (str->_Mode & (_MOPENR | _MOPENW)) != 0 && 0 <= str->_Handle)
			fcloseStream(str, exiting);
	}
}

int fclose(FILE *str)
{
	return fcloseStream(str, 0);
}
