/* _WFwprep function */
#include <stdlib.h>

#include "xstdio.h"

#define WRITE_BUFSIZ	0x10000	/* SCE: the buffer the library allocates for writing */

/* prepare str for wide writing: 0 when there is room, -1 on error */
int _WFwprep(FILE *str)
{
	unsigned char *buf;

	if (str->_Next < str->_WWend)
		return 0;
	if ((str->_Mode & (_MOPENW | _MREAD | _MBYTE)) != _MOPENW) {
		str->_Mode |= str->_Mode & _MBYTE ? _MERR : _MERR | _MWIDE;
		return -1;
	}
	if ((str->_Mode & (_MWRITE | _MWIDE)) == (_MWRITE | _MWIDE) && str->_Next >= str->_Bend
	    && fflush(str) != 0)
		return -1;	/* the buffer is full and cannot be written */
	buf = str->_Buf;
	if (!(str->_Mode & (_MNBF | _MLBF)) && buf == &str->_Cbuf) {	/* allocate a buffer */
		buf = malloc(WRITE_BUFSIZ);
		str->_Buf = buf;
		if (buf == NULL) {	/* use the one-byte buffer */
			buf = &str->_Cbuf;
			str->_Next = buf;
			str->_Buf = buf;
			str->_Bend = buf + 1;
		} else {
			str->_Next = buf;
			str->_Bend = buf + WRITE_BUFSIZ;
			str->_Mode |= _MALBUF;
			str->_Rend = buf;
			str->_Wend = buf;
		}
	}
	str->_WRend = buf;
	str->_WWend = str->_Bend;
	str->_Mode |= _MWRITE | _MWIDE;
	return 0;
}
