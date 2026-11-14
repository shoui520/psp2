/* _Fwprep function */
#include <stdlib.h>

#include "xstdio.h"

#define WRITE_BUFSIZ	0x10000	/* SCE: the buffer the library allocates for writing */

/* prepare str for writing: 0 when there is room, -1 on error */
int _Fwprep(FILE *str)
{
	unsigned char *buf;
	int ans;

	if (str->_Next < str->_Wend)
		return 0;
	if ((str->_Mode & (_MOPENW | _MREAD | _MWIDE)) != _MOPENW) {
		str->_Mode |= str->_Mode & _MWIDE ? _MERR : _MERR | _MBYTE;
		return -1;
	}
	if ((str->_Mode & (_MWRITE | _MBYTE)) == (_MWRITE | _MBYTE) && str->_Next >= str->_Bend
	    && (ans = fflush(str), ans != 0))
		return -1;	/* the buffer is full and cannot be written */
	buf = str->_Buf;
	if (!(str->_Mode & _MNBF) && buf == &str->_Cbuf) {	/* allocate a buffer */
		buf = malloc(WRITE_BUFSIZ);
		str->_Buf = buf;
		if (buf == NULL) {	/* use the one-byte buffer */
			buf = &str->_Cbuf;
			str->_Buf = buf;
			str->_Bend = buf + 1;
		} else {
			str->_WRend = buf;
			str->_Bend = buf + WRITE_BUFSIZ;
			str->_WWend = buf;
			str->fpos = (unsigned int)buf;
			str->_Mode |= _MALBUF;
		}
	}
	str->_Next = buf;
	str->_Rend = buf;
	str->_Wend = str->_Bend;
	str->_Mode |= _MWRITE | _MBYTE;
	str->_BRend = buf;
	str->_Bflag &= ~1;
	str->_Rsize = 0;
	return 0;
}
