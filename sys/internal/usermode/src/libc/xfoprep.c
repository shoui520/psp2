/* _Foprep function */
#include <errno.h>
#include <libkernel/libkernel.h>
#include <xmtx.h>

#include "psp2io.h"
#include "xstdio.h"

FILE *_Foprep(const char *filename, const char *mods, FILE *str, _FD_TYPE fd, int share,
	      int reopen)
{
	static const FILE initial = {0, 0, _FD_INVALID};
	unsigned char idx;
	unsigned short malfil;
	_SceLibcMutexType mtx;

	if (str == NULL)
		return NULL;
	idx = str->_Idx;
	mtx = str->_stream_mtx;
	malfil = str->_Mode & _MALFIL;
	*str = initial;
	str->_Idx = idx;
	str->_Buf = &str->_Cbuf;
	str->_Next = &str->_Cbuf;
	str->_Rend = &str->_Cbuf;
	str->_WRend = &str->_Cbuf;
	str->_WWend = &str->_Cbuf;
	str->_Wend = &str->_Cbuf;
	str->_Rback = str->_Back + sizeof(str->_Back);
	str->_WRback = str->_WBack + sizeof(str->_WBack) / sizeof(str->_WBack[0]);
	str->fpos = (unsigned int)&str->_Cbuf;
	str->_Rsize = 0;
	str->_offset = 0;
	str->_BRend = &str->_Cbuf;
	str->_Bflag = 0;
	str->_Mode = malfil | (*mods == 'r' ? _MOPENR
			       : *mods == 'w' ? _MCREAT | _MOPENW | _MTRUNC
			       : *mods == 'a' ? _MCREAT | _MOPENW | _MOPENA
			       : 0);
	for (; *++mods == 'b' || *mods == '+';)
		if (*mods == 'b') {
			if (str->_Mode & _MBIN)
				break;
			str->_Mode |= _MBIN;
		} else if (str->_Mode & (_MOPENR | _MOPENW)) {
			if ((str->_Mode & (_MOPENR | _MOPENW)) == (_MOPENR | _MOPENW))
				break;
			str->_Mode |= _MOPENR | _MOPENW;
		}
	if (filename != NULL) {	/* open the file */
		if (str->_Mode & (_MOPENR | _MOPENW))
			str->_Handle = _Fopen(filename, share ? str->_Mode | _MALBUF : str->_Mode,
					      share == 'U');
	} else if (str->_Mode & (_MOPENR | _MOPENW)) {
		str->_Handle = fd;
	}
	if (reopen) {	/* keep the stream's lock */
		str->_stream_mtx = mtx;
	} else {
		char name[35];

		sceClibSnprintf(name, sizeof(name), "SceLibcFileFD:0x%08X", str->_Handle);
		_Mtxinit(&str->_stream_mtx, name);
	}
	if (str->_Handle < 0) {	/* the open failed */
		int err = errno;

		fclose(str);
		errno = err;
		return NULL;
	}
	return str;
}
