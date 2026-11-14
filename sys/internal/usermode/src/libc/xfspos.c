/* _Fspos function */
#include <errno.h>

#include "psp2io.h"
#include "xstdio.h"

/* is pos inside the read cache of str? */
#define INCACHE(str, pos, brend)	((brend) - (str)->_Rsize <= (pos) && (pos) < (brend))

int _Fspos(FILE *str, const fpos_t *ptr, long long loff, int way)
{
	unsigned long long pos, old;
	unsigned int brend;
	unsigned short mode, bflag;
	int ans;

	if (!(str->_Mode & (_MOPENR | _MOPENW)) || (ans = fflush(str), mode = str->_Mode, ans != 0)) {
		errno = EFPOS;
		return EOF;
	}
	if (way != SEEK_CUR && way != SEEK_END && way != SEEK_SET) {
		errno = EINVAL;
		return EOF;
	}
	bflag = str->_Bflag & ~5;
	if (ptr != NULL)
		loff += (long)ptr->_Off;	/* SCE: the offset as a long */
	str->_Bflag = bflag;
	if (way == SEEK_CUR) {	/* SCE: move inside the read cache */
		brend = (unsigned int)str->_BRend;
		pos = str->fpos + loff;
		str->fpos = pos;
		if (INCACHE(str, pos, brend)) {
			bflag |= 1;
			str->_Next += loff;
			str->_Bflag = bflag;
		}
		if (bflag & 2) {
			loff += str->_offset;
			str->_offset = 0;
		}
	}
	if (way == SEEK_SET) {
		if (loff >= 0) {
			brend = (unsigned int)str->_BRend;
			pos = (unsigned int)str->_Buf + loff;
			old = str->fpos;
			str->fpos = pos;
			if (INCACHE(str, pos, brend)) {
				str->_Next += pos - old;
				bflag |= 1;
				str->_Bflag = bflag;
			}
			str->_offset = 0;
		} else {
			brend = (unsigned int)str->_BRend;
			pos = str->fpos;
			if (INCACHE(str, pos, brend)) {
				bflag |= 1;
				str->_Bflag = bflag;
			}
		}
	}
	if (way == SEEK_CUR && (mode & _MREAD)) {	/* less what the buffers still hold */
		long n = _Fbytes(str, str->_Next, str->_WRend);

		n = _Fbytes(str, str->_Rback, str->_Back + sizeof(str->_Back))
			+ _Fbytes(str, (unsigned char *)((unsigned int)str->fpos - (unsigned int)loff),
				  str->_Rsave != NULL ? str->_Rsave : str->_BRend)
			+ n;
		bflag = str->_Bflag;
		loff -= n;
	}
	if (way == SEEK_END && loff >= 0)
		str->_Bflag = bflag | 4;
	if (way == SEEK_CUR || way == SEEK_END || way == SEEK_SET)	/* SCE: checked again */
		loff = _Lseek(str->_Handle, loff, way);
	bflag = str->_Bflag;
	if (way == SEEK_END) {
		brend = (unsigned int)str->_BRend;
		pos = (unsigned int)str->_Buf + loff;
		old = str->fpos;
		str->fpos = pos;
		if (INCACHE(str, pos, brend)) {
			str->_Next += pos - old;
			bflag |= 1;
			str->_Bflag = bflag;
		}
		str->_offset = 0;
	}
	if (!(bflag & 2))
		str->_offset = loff;
	if (loff == -1) {
		errno = EFPOS;
		return EOF;
	}
	brend = (unsigned int)str->_BRend;
	pos = str->fpos;
	if (pos < brend - str->_Rsize || brend <= pos || (str->_Mode & _MWRITE)) {	/* empty the buffers */
		str->_Next = str->_Buf;
		str->_Rend = str->_Buf;
		str->_Rsave = NULL;
		str->_WRend = str->_Buf;
		str->_WWend = str->_Buf;
		str->_Wend = str->_Buf;
		str->_Rback = str->_Back + sizeof(str->_Back);
		str->_WRback = str->_WBack + sizeof(str->_WBack) / sizeof(str->_WBack[0]);
		str->_BRend = str->_Buf;
		str->_Rsize = 0;
	} else if (str->_Mode & (_MREAD | _MWRITE)) {	/* SCE: keep the read cache */
		str->_Rback = str->_Back + sizeof(str->_Back);
		str->_WRback = str->_WBack + sizeof(str->_WBack) / sizeof(str->_WBack[0]);
		if (str->_Rsave != NULL) {
			str->_Rend = str->_Rsave;
			str->_Rsave = NULL;
		}
	}
	if (ptr != NULL)
		str->_Wstate = ptr->_Wstate;
	str->_Mode &= ~(_MEOF | _MREAD | _MWRITE);
	return 0;
}
