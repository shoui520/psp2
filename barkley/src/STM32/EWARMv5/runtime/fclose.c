/*
 * fclose: a stream flushed, its buffer and temporary file's name freed, its handle
 * closed (not the standard streams'); a stream malloc's is freed and taken from
 * _Files, a static one left closed.
 */

#include <stdlib.h>
#include <yfuns.h>
#include "xmtx.h"
#include "xstdio.h"

int fclose(FILE *str)
{
	int status = fflush(str);
	unsigned int i;

	_Locksyslock(_LOCK_STREAM);
	if (str->_Mode & _MALBUF)
		free(str->_Buf);
	str->_Buf = 0;
	if (str->_Handle > 2 && __close(str->_Handle) != 0)
		status = EOF;
	if (str->_Tmpnam != 0) {
		if (remove(str->_Tmpnam) != 0)
			status = EOF;
		free(str->_Tmpnam);
		str->_Tmpnam = 0;
	}
	if (str->_Mode & _MALFIL) {
		for (i = 0; i < FOPEN_MAX; i++)
			if (_Files[i] == str) {
				_Files[i] = 0;
				break;
			}
		free(str);
	} else {
		str->_Mode = 0;
		str->_Handle = -1;
		str->_Buf = &str->_Cbuf;
		str->_Next = &str->_Cbuf;
		str->_Rend = &str->_Cbuf;
		str->_WRend = &str->_Cbuf;
		str->_Wend = &str->_Cbuf;
		str->_WWend = &str->_Cbuf;
		str->_Rback = str->_Back + sizeof(str->_Back);
		str->_WRback = str->_WBack + sizeof(str->_WBack) / sizeof(str->_WBack[0]);
	}
	_Unlocksyslock(_LOCK_STREAM);
	return status;
}
