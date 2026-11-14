/* fgets function */
#include <string.h>
#include "xstdio.h"

char *fgets(char *__restrict buf, int n, FILE *__restrict str)
{
	unsigned char *s = (unsigned char *)buf;
	unsigned char *s1;

	if (n <= 1)
		return NULL;
	--n;
	_Lockfilelock(str);
	if (str->_Mode & _MBYTE) {	/* deliver the pushed-back characters */
		for (; 0 < n && str->_Rback < str->_Back + sizeof(str->_Back); --n) {
			*s = *str->_Rback++;
			if (*s++ == '\n')
				n = 1;	/* terminate a full line */
		}
	}
	if (0 < n) {
		do {
			unsigned long long pos, brend;
			size_t m;

			brend = (unsigned int)str->_BRend;
			if (str->_Rsave != NULL) {	/* restore the read buffer */
				str->_Rend = str->_Rsave;
				str->_Rsave = NULL;
			}
			pos = str->fpos;
			if (pos < brend - str->_Rsize || brend <= pos) {
				if (_Frprep(str) < 0) {
					_Unlockfilelock(str);
					return NULL;
				}
				if (str->_Mode & _MEOF)
					break;
			}
			s1 = memchr(str->_Next, '\n', str->_Rend - str->_Next);
			m = s1 != NULL ? s1 - str->_Next + 1 : str->_Rend - str->_Next;
			if (n < m) {
				s1 = NULL;
				m = n;
			}
			memcpy(s, str->_Next, m);
			s += m;
			str->fpos += m;
			n -= m;
			str->_Next += m;
			str->_offset += m;
		} while (s1 == NULL && 0 < n);
	}
	if (s == (unsigned char *)buf)
		buf = NULL;
	else
		*s = '\0';
	_Unlockfilelock(str);
	return buf;
}
