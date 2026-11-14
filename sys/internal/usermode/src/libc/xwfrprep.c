/* _WFrprep function */
#include <stdlib.h>

#include "psp2io.h"
#include "xstdio.h"

#define READ_BUFSIZ	0x10000	/* SCE: the buffer the library allocates for reading */

/* prepare str for wide reading: 1 when bytes are buffered, 0 at end of file, -1 on error */
int _WFrprep(FILE *str)
{
	unsigned char *buf;
	int n;

	if (str->_Next < str->_WRend)
		return 1;
	if (str->_Mode & _MEOF)
		return 0;
	if ((str->_Mode & (_MOPENR | _MWRITE | _MBYTE)) != _MOPENR) {
		str->_Mode |= str->_Mode & _MBYTE ? _MERR : _MERR | _MWIDE;
		return -1;
	}
	buf = str->_Buf;
	if (!(str->_Mode & (_MNBF | _MLBF)) && buf == &str->_Cbuf) {	/* allocate a buffer */
		buf = malloc(READ_BUFSIZ);
		str->_Buf = buf;
		if (buf == NULL) {	/* use the one-byte buffer */
			buf = &str->_Cbuf;
			str->_Buf = buf;
			str->_Bend = &str->_Cbuf + 1;
		} else {
			str->_Rend = buf;
			str->_Mode |= _MALBUF;
			str->_Bend = buf + READ_BUFSIZ;
			str->_Wend = buf;
		}
	}
	str->_Next = buf;
	str->_WRend = buf;
	str->_WWend = buf;
	n = _Read(str->_Handle, buf, str->_Bend - buf);
	if (n < 0) {
		str->_Mode |= _MERR | _MWIDE;
		return -1;
	}
	if (n == 0) {
		str->_Mode = (str->_Mode & ~_MREAD) | _MEOF | _MWIDE;
		return 0;
	}
	str->_Mode |= _MREAD | _MWIDE;
	str->_WRend += n;
	return 1;
}
