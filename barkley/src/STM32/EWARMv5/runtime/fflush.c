/*
 * fflush: a stream's written characters sent to its handle, or every open stream's
 * (stream 0).
 */

#include <yfuns.h>
#include "xmtx.h"
#include "xstdio.h"

static int ffl(FILE *str)
{
	unsigned char *s;
	size_t n;

	if ((str->_Mode & _MWRITE) == 0)
		return 0;
	for (s = str->_Buf; s < str->_Next; s += n) {
		n = __dwrite(str->_Handle, s, str->_Next - s);
		if (n == (size_t)-1 || n == 0)
			goto error;
	}
	if (__dwrite(str->_Handle, 0, 0) == (size_t)-1) {
error:
		str->_Next = str->_Buf;
		str->_Wend = str->_Buf;
		str->_WWend = str->_Buf;
		str->_Mode |= _MERR;
		return EOF;
	}
	str->_Next = str->_Buf;
	s = (str->_Mode & (_MLBF | _MNBF)) != 0 ? str->_Buf : str->_Bend;
	if (str->_Mode & _MBYTE)
		str->_Wend = s;
	else
		str->_WWend = s;
	return 0;
}

int fflush(FILE *str)
{
	int i, status;

	if (str != 0)
		return ffl(str);
	_Locksyslock(_LOCK_STREAM);
	status = 0;
	for (i = 0; i < FOPEN_MAX; i++)
		if (_Files[i] != 0 && ffl(_Files[i]) < 0)
			status = EOF;
	_Unlocksyslock(_LOCK_STREAM);
	return status;
}
