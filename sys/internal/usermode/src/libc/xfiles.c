/* _Files data object and the stream table's functions */
#include <stdlib.h>
#include <xmtx.h>

#include "libc_internal.h"
#include "xstdio.h"

#define STDBUF	256	/* SCE: the standard streams' buffers */

static unsigned char ibuf[STDBUF] = {0};
static unsigned char obuf[STDBUF] = {0};
static unsigned char ebuf[STDBUF] = {0};

FILE _Stdin = {
	_MOPENR | _MLBF, 0, 0,
	ibuf, ibuf + sizeof(ibuf), ibuf,
	ibuf, ibuf, _Stdin._Back + sizeof(_Stdin._Back),
	_Stdin._WBack + sizeof(_Stdin._WBack) / sizeof(_Stdin._WBack[0]),
};
FILE _Stdout = {
	_MOPENW | _MLBF, 1, 1,
	obuf, obuf + sizeof(obuf), obuf,
	obuf, obuf, _Stdout._Back + sizeof(_Stdout._Back),
	_Stdout._WBack + sizeof(_Stdout._WBack) / sizeof(_Stdout._WBack[0]),
};
FILE _Stderr = {
	_MOPENW | _MLBF, 2, 2,
	ebuf, ebuf + sizeof(ebuf), ebuf,
	ebuf, ebuf, _Stderr._Back + sizeof(_Stderr._Back),
	_Stderr._WBack + sizeof(_Stderr._WBack) / sizeof(_Stderr._WBack[0]),
};

FILE *_Files[FOPEN_MAX] = {&_Stdin, &_Stdout, &_Stderr};

/* SCE: the standard streams' read cache starts at their buffers */
void _InitStdFiles(void)
{
	_Stdin.fpos = (unsigned int)ibuf;
	_Stdin._WRend = ibuf;
	_Stdin._WWend = ibuf;
	_Stdout.fpos = (unsigned int)obuf;
	_Stdout._WRend = obuf;
	_Stdout._WWend = obuf;
	_Stderr.fpos = (unsigned int)ebuf;
	_Stderr._WRend = ebuf;
	_Stderr._WWend = ebuf;
}

FILE *_Fofind(void)
{
	FILE *str = NULL;
	size_t i;

	for (i = 3; i < FOPEN_MAX; ++i)	/* SCE: past the standard streams */
		if (_Files[i] == NULL) {	/* setup a new stream */
			str = (FILE *)malloc(sizeof(FILE));
			if (str != NULL) {
				_Files[i] = str;
				str->_Mode = _MALFIL;
				str->_Idx = (unsigned char)i;
			}
			break;
		}
	return str;
}

/* reset a closed stream: no handle, the one-character buffer */
static void clearfile(FILE *str)
{
	str->_Mode = 0;
	str->_Buf = &str->_Cbuf;
	str->_Handle = _FD_INVALID;
	str->_Next = &str->_Cbuf;
	str->_Rend = &str->_Cbuf;
	str->_WRend = &str->_Cbuf;
	str->_WWend = &str->_Cbuf;
	str->_Wend = &str->_Cbuf;
	str->_Rback = &str->_Cbuf;
	str->_WRback = str->_WBack + sizeof(str->_WBack) / sizeof(str->_WBack[0]);
	str->fpos = (unsigned int)&str->_Cbuf;
	str->_Rsize = 0;
	str->_offset = 0;
	str->_BRend = &str->_Cbuf;
	str->_Bflag = 0;
}

void _Fofree(FILE *str, int exiting)
{
	if (str->_Mode & _MALFIL) {	/* the library allocated it */
		size_t i;

		for (i = 0; i < FOPEN_MAX; ++i)
			if (_Files[i] == str) {	/* found the entry */
				_Files[i] = NULL;
				break;
			}
		clearfile(str);
		_Unlockfilelock(str);
		_Mtxdst(&str->_stream_mtx);
		if (!exiting)
			free(str);
	} else {
		clearfile(str);
		_Unlockfilelock(str);
	}
}
