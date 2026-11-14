/* setvbuf function */
#include <limits.h>
#include <stdlib.h>

#include "xstdio.h"

int setvbuf(FILE *__restrict str, char *__restrict abuf, int smode, size_t size)
{
	int mode;
	unsigned char *buf = (unsigned char *)abuf;

	mode = smode == _IOFBF ? 0
		: smode == _IOLBF ? _MLBF
		: smode == _IONBF ? _MNBF
		: -1;
	if (mode == -1)
		return -1;
	_Lockfilelock(str);
	if ((str->_Mode & (_MREAD | _MWRITE)) != 0) {	/* too late */
		_Unlockfilelock(str);
		return -1;
	}
	if (INT_MAX < size) {
		size = INT_MAX;
	} else if (size == 0) {
		if (buf != NULL || mode == _MNBF) {	/* the one-character buffer */
			buf = &str->_Cbuf;
			size = 1;
			mode = _MNBF;
		} else {
			size = BUFSIZ;
		}
	}
	if (buf == NULL) {
		if ((buf = (unsigned char *)malloc(size)) == NULL) {
			_Unlockfilelock(str);
			return -1;
		}
		mode |= _MALBUF;
	}
	if (str->_Mode & _MALBUF) {
		free(str->_Buf);
		str->_Mode &= ~_MALBUF;
	}
	/* SCE: the file position moves with the buffer, and the read cache is dropped */
	str->fpos = str->fpos - (unsigned int)str->_Buf + (unsigned int)buf;
	str->_Buf = buf;
	str->_Next = buf;
	str->_Bend = buf + size;
	str->_Mode = str->_Mode & ~(_MLBF | _MNBF) | mode;
	str->_Rend = buf;
	str->_WRend = buf;
	str->_WWend = buf;
	str->_Wend = buf;
	str->_Rsize = 0;
	str->_offset = 0;
	str->_BRend = buf;
	str->_Bflag = 0;
	_Unlockfilelock(str);
	return 0;
}
