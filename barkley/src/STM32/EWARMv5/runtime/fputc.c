/*
 * fputc: a character into the stream's buffer, flushed at once if the stream is not
 * buffered, at a newline if it is line buffered.
 */

#include "xstdio.h"

int fputc(int ci, FILE *str)
{
	unsigned char c = ci;

	if (str->_Wend <= str->_Next && _Fwprep(str) < 0)
		return EOF;
	*str->_Next++ = c;
	if (((str->_Mode & _MNBF) != 0 || ((str->_Mode & _MLBF) != 0 && c == '\n')) &&
	    fflush(str) != 0)
		return EOF;
	if (str->_Mode & (_MLBF | _MNBF))
		str->_Wend = str->_Next;
	return c;
}
