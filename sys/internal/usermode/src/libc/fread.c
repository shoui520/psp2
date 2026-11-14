/* fread function */
#include <string.h>
#include "xstdio.h"

size_t fread(void *__restrict ptr, size_t size, size_t nelem, FILE *__restrict str)
{
	unsigned char *s = (unsigned char *)ptr;
	size_t ns = size * nelem;
	size_t n = ns;

	if (n == 0)
		return 0;
	_Lockfilelock(str);
	if (str->_Mode & _MBYTE)	/* deliver the pushed-back characters */
		for (; n != 0 && str->_Rback < str->_Back + sizeof(str->_Back); --n)
			*s++ = *str->_Rback++;
	while (n != 0) {
		unsigned long long pos, brend;
		size_t m;

		brend = (unsigned int)str->_BRend;
		if (str->_Rsave != NULL) {	/* restore the read buffer */
			str->_Rend = str->_Rsave;
			str->_Rsave = NULL;
		}
		pos = str->fpos;
		if (pos < brend - str->_Rsize || brend <= pos) {
			if (_Frprep(str) <= 0)
				break;
			brend = (unsigned int)str->_BRend;
			pos = str->fpos;
		}
		m = brend - pos;
		if (n < m)
			m = n;
		memcpy(s, str->_Next, m);
		s += m;
		n -= m;
		str->_Next += m;
		str->_offset += m;
		str->fpos += m;
	}
	_Unlockfilelock(str);
	return (ns - n) / size;
}
