/*
 * _Fwprep: a stream made ready to write: byte oriented, its buffer flushed if full, a
 * buffer of its own (malloc's, else its one character) if it has none.
 */

#include <stdlib.h>
#include "xstdio.h"

int _Fwprep(FILE *str)
{
	if (str->_Next < str->_Wend)
		return 0;
	if ((str->_Mode & (_MWIDE | _MREAD | _MOPENW)) != _MOPENW) {
		str->_Mode |= (str->_Mode & _MWIDE) != 0 ? _MERR : _MERR | _MBYTE;
		return EOF;
	}
	if ((str->_Mode & (_MWRITE | _MBYTE)) == (_MWRITE | _MBYTE) && str->_Bend <= str->_Next &&
	    fflush(str) != 0)
		return EOF;
	if ((str->_Mode & (_MLBF | _MNBF)) == 0 && str->_Buf == &str->_Cbuf) {
		if ((str->_Buf = malloc(BUFSIZ)) == 0) {
			str->_Buf = &str->_Cbuf;
			str->_Next = &str->_Cbuf;
			str->_Bend = &str->_Cbuf + 1;
		} else {
			str->_Mode |= _MALBUF;
			str->_Next = str->_Buf;
			str->_Bend = str->_Buf + BUFSIZ;
			str->_WRend = str->_Buf;
			str->_WWend = str->_Buf;
		}
	}
	str->_Rend = str->_Buf;
	str->_Wend = str->_Bend;
	str->_Mode |= _MWRITE | _MBYTE;
	return 0;
}
