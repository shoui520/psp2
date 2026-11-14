/* fflush function */
#include "xstdio.h"
#include "psp2io.h"

int fflush(FILE *str)
{
	unsigned char *s;
	int n;

	if (str == NULL) {	/* flush every stream */
		int i, status;

		_Locksyslock(_LOCK_STREAM);
		for (status = 0, i = 0; i < FOPEN_MAX; ++i)
			if (_Files[i] != NULL && fflush(_Files[i]) < 0)
				status = EOF;
		_Unlocksyslock(_LOCK_STREAM);
		return status;
	}
	_Lockfilelock(str);
	if (!(str->_Mode & _MWRITE)) {
		_Unlockfilelock(str);
		return 0;
	}
	for (s = str->_Buf; s < str->_Next; s += n) {	/* write the buffer */
		n = _Write(str->_Handle, s, str->_Next - s);
		if (n <= 0) {
			str->_Next = str->_Buf;
			str->_Wend = str->_Buf;
			str->_WWend = str->_Buf;
			str->_Mode |= _MERR;
			_Unlockfilelock(str);
			return EOF;
		}
		str->fpos += n;
	}
	str->_Next = str->_Buf;
	str->_Wend = str->_Buf;
	str->_WWend = str->_Buf;
	str->_Mode &= ~_MWRITE;
	_Unlockfilelock(str);
	return 0;
}
