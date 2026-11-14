/* _Fgpos function */
#include <errno.h>

#include "psp2io.h"
#include "xstdio.h"

/* the bytes from p1 up to p2, none if p2 is not past p1 */
long _Fbytes(FILE *str, const unsigned char *p1, const unsigned char *p2)
{
	return p1 < p2 ? p2 - p1 : 0;
}

long long _Fgpos(FILE *str, fpos_t *ptr)
{
	long long loff;

	if (str->_Bflag & 1) {	/* SCE: apply the pending seek */
		loff = _Lseek(str->_Handle, str->_offset, SEEK_CUR);
		str->_offset = 0;
	} else {
		loff = _Lseek(str->_Handle, 0, SEEK_CUR);
	}
	if (!(str->_Mode & (_MOPENR | _MOPENW)) || loff == -1) {	/* query failed */
		errno = EFPOS;
		return EOF;
	}
	if (str->_Mode & _MWRITE) {
		loff += _Fbytes(str, str->_Buf, str->_Next);
	} else if (str->_Mode & _MREAD) {	/* SCE: less what the buffers still hold */
		const unsigned char *rend = str->_Rsave != NULL ? str->_Rsave : str->_BRend;

		loff -= _Fbytes(str, str->_Rback, str->_Back + sizeof(str->_Back))
			+ _Fbytes(str, (const unsigned char *)(unsigned int)str->fpos, rend)
			+ _Fbytes(str, str->_Next, str->_WRend);
	}
	if (ptr == NULL)
		return loff;
	ptr->_Off = loff;
	ptr->_Wstate = str->_Wstate;
	return 0;
}
