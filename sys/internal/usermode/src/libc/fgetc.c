/* fgetc function */
#include "xstdio.h"

int fgetc(FILE *str)
{
	unsigned long long pos, brend;
	unsigned char *next;
	int ch, ans;

	_Lockfilelock(str);
	if (str->_Rback < str->_Back + sizeof(str->_Back) && (str->_Mode & _MBYTE)) {
		ch = *str->_Rback++;	/* deliver a pushed-back character */
		_Unlockfilelock(str);
		return ch;
	}
	pos = str->fpos;
	brend = (unsigned int)str->_BRend;
	if (str->_Rsave != NULL) {	/* restore the read buffer */
		str->_Rend = str->_Rsave;
		str->_Rsave = NULL;
	}
	next = str->_Next;
	if (str->_Rend <= next && pos < brend) {	/* skip to the end of the read */
		str->_offset += brend - pos;
		str->fpos = brend;
		pos = brend;
	}
	if ((brend - str->_Rsize <= pos && pos < brend)
	    || (ans = _Frprep(str), next = str->_Next, pos = str->fpos, 0 < ans)) {
		str->_Next = next + 1;
		ch = *next;
		str->_offset += 1;
		str->fpos = pos + 1;
		_Unlockfilelock(str);
		return ch;
	}
	_Unlockfilelock(str);
	return EOF;
}
