/* gets function */
#include <string.h>
#include "xstdio.h"

char *gets(char *buf)
{
	FILE *const str = stdin;
	unsigned char *s = (unsigned char *)buf;
	unsigned long long lo, brend;

	_Lockfilelock(str);
	if (str->_Mode & _MBYTE)	/* deliver the pushed-back characters */
		while (str->_Rback < str->_Back + sizeof(str->_Back)) {
			unsigned char *p = s;

			*s++ = *str->_Rback++;
			if (*p == '\n') {
				*p = '\0';
				_Unlockfilelock(str);
				return buf;
			}
		}
	for (brend = (unsigned int)str->_BRend;; brend = (unsigned int)str->_BRend) {
		unsigned char *pnl;
		size_t m;

		lo = brend - str->_Rsize;
		if (str->_Rsave != NULL) {	/* restore the read buffer */
			str->_Rend = str->_Rsave;
			str->_Rsave = NULL;
		}
		if (str->fpos < lo || brend <= str->fpos) {
			str->_BRend = str->_Buf;	/* SCE: forget the read cache */
			str->fpos = (unsigned int)str->_Buf;
			str->_Rsize = 0;
			if (_Frprep(str) < 0) {
				_Unlockfilelock(str);
				return NULL;
			}
			if (str->_Mode & _MEOF)
				break;
		}
		pnl = memchr(str->_Next, '\n', str->_Rend - str->_Next);
		m = pnl != NULL ? pnl - str->_Next + 1 : str->_Rend - str->_Next;
		memcpy(s, str->_Next, m);
		str->_Next += m;
		s += m;
		str->fpos += m;
		str->_offset += m;
		if (pnl != NULL) {	/* replace the newline */
			s[-1] = '\0';
			_Unlockfilelock(str);
			return buf;
		}
	}
	if (s == (unsigned char *)buf)
		buf = NULL;
	else
		*s = '\0';
	_Unlockfilelock(str);
	return buf;
}
