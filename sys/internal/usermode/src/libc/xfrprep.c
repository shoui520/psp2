/* _Frprep function */
#include <stdlib.h>

#include "psp2io.h"
#include "xstdio.h"

#define READ_BUFSIZ	0x10000	/* SCE: the buffer the library allocates for reading */

/* prepare str for reading: 1 when bytes are buffered, 0 at end of file, -1 on error */
int _Frprep(FILE *str)
{
	unsigned long long pos = str->fpos;
	unsigned int brend = (unsigned int)str->_BRend;
	unsigned char *buf;
	int n;

	if (pos >= brend - str->_Rsize && pos < brend)
		return 1;	/* SCE: still in the read cache */
	if (str->_Mode & _MEOF)
		return 0;
	if ((str->_Mode & (_MOPENR | _MWRITE | _MWIDE)) != _MOPENR) {
		str->_Mode |= str->_Mode & _MWIDE ? _MERR : _MERR | _MBYTE;
		return -1;
	}
	buf = str->_Buf;
	if (!(str->_Mode & _MNBF) && buf == &str->_Cbuf) {	/* allocate a buffer */
		buf = malloc(READ_BUFSIZ);
		str->_Buf = buf;
		if (buf == NULL) {	/* use the one-byte buffer */
			buf = &str->_Cbuf;
			pos = str->fpos;
			str->_Buf = buf;
			str->_Bend = buf + 1;
		} else {
			pos = (unsigned int)buf + str->_offset;
			str->fpos = pos;
			str->_Mode |= _MALBUF;
			str->_Bend = buf + READ_BUFSIZ;
			str->_WRend = buf;
			str->_WWend = buf;
		}
	}
	str->_Next = buf;
	str->_Rend = buf;
	str->_Wend = buf;
	str->_BRend = buf;
	if (str->_Bflag & 1) {	/* SCE: apply the pending seek */
		_Lseek(str->_Handle, pos - (unsigned int)buf, SEEK_SET);
		str->_offset = 0;
		str->_Bflag &= ~1;
	}
	if (!(str->_Bflag & 2) && str->_offset < (long long)(str->_Bend - str->_Buf)
	    && !(str->_Bflag & 4)) {
		_Lseek(str->_Handle, 0, SEEK_SET);
	} else {
		str->_offset = 0;
		str->_Bflag |= 2;
	}
	n = _Read(str->_Handle, str->_Buf, str->_Bend - str->_Buf);
	if (n < 0) {
		str->_Mode |= _MERR | _MBYTE;
		return -1;
	}
	if (n == 0) {
		str->_Mode = (str->_Mode & ~_MREAD) | _MEOF | _MBYTE;
		return 0;
	}
	str->_Mode |= _MREAD | _MBYTE;
	pos = str->fpos;
	buf = str->_Buf + n;
	if (str->_Bflag & 2) {
		str->_Rsize = n;
		str->_BRend = (unsigned char *)(unsigned int)pos + n;
		str->_Rend = buf;
	} else {
		str->_BRend = buf;
		str->_Next = (unsigned char *)(unsigned int)pos;
		str->_Bflag |= 2;
		str->_Rend = buf;
		str->_Rsize = n;
		if ((unsigned int)buf < pos) {	/* SCE: past the end of the file */
			_Lseek(str->_Handle, str->_offset, SEEK_SET);
			str->_Mode = (str->_Mode & ~_MREAD) | _MEOF | _MBYTE;
			return 0;
		}
	}
	return 1;
}
