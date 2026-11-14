// strstream.cpp: strstreambuf, istrstream, ostrstream and strstream, whose members the psp2
// headers declare out of line
#include <climits>
#include <cstring>
#include <strstream>

_STD_BEGIN
void strstreambuf::_Init(streamsize _Count,
	char *_Gp,
	char *_Pp,
	_Strstate _Mode)
	{	// initialize with possibly static buffer
	streambuf::_Init();
	_Minsize = _MINSIZE;
	_Pendsave = 0;
	_Seekhigh = 0;
	_Palloc = 0;
	_Pfree = 0;
	_Strmode = _Mode;

	if (_Gp == 0)
		{	// make dynamic
		_Strmode |= _Dynamic;
		if (_Minsize < _Count)
			_Minsize = (int)_Count;
		}
	else
		{	// make static
		size_t _Size = _Count < 0 ? INT_MAX
			: _Count == 0 ? _CSTD strlen(_Gp)
			: (size_t)_Count;
		_Seekhigh = _Gp + _Size;

		if (_Pp == 0)
			setg(_Gp, _Gp, _Gp + _Size);	// set read pointers only
		else
			{	// make writable too
			if (_Pp < _Gp)
				_Pp = _Gp;
			else if (_Gp + _Size < _Pp)
				_Pp = _Gp + _Size;
			setp(_Pp, _Gp + _Size);
			setg(_Gp, _Gp, _Pp);
			}
		}
	}

void strstreambuf::_Tidy()
	{	// free any allocated storage
	if ((_Strmode & (_Allocated | _Frozen)) != _Allocated)
		;	// no buffer to free
	else if (_Pfree != 0)
		(*_Pfree)(eback());	// free with function call
	else
		_DELETE_CRT_VEC(eback());	// free by deleting array

	_Seekhigh = 0;
	_Strmode &= ~(_Allocated | _Frozen);
	}

strstreambuf::~strstreambuf() _NOEXCEPT
	{	// destroy a strstreambuf
	_Tidy();
	}

void strstreambuf::freeze(bool _Freezeit)
	{	// freeze or unfreeze writing
	if (_Freezeit && !(_Strmode & _Frozen))
		{	// disable writing
		_Strmode |= _Frozen;
		_Pendsave = epptr();
		setp(pbase(), pptr(), eback());
		}
	else if (!_Freezeit && _Strmode & _Frozen)
		{	// re-enable writing
		_Strmode &= ~_Frozen;
		setp(pbase(), pptr(), _Pendsave);
		}
	}

int strstreambuf::overflow(int _Meta)
	{	// try to extend write area
	if (_Meta == EOF)
		return (0);	// nothing to write
	else if (pptr() != 0 && pptr() < epptr())
		return ((unsigned char)(*_Pninc() = (char)_Meta));
	else if (!(_Strmode & _Dynamic)
		|| _Strmode & (_Constant | _Frozen))
		return (EOF);	// can't extend
	else
		{	// okay to extend
		int _Oldsize = gptr() == 0 ? 0 : (int)(epptr() - eback());
		int _Newsize = _Oldsize;
		int _Inc = _Newsize / 2 < _Minsize
			? _Minsize : _Newsize / 2;	// grow by 50 per cent if possible
		_Minsize = _MINSIZE;	// back to default for future growth
		char *_Ptr = 0;

		while (0 < _Inc && INT_MAX - _Inc < _Newsize)
			_Inc /= 2;	// reduce growth increment if too big
		if (0 < _Inc)
			{	// room to grow, increase size
			_Newsize += _Inc;
			_Ptr = _Palloc != 0 ? (char *)(*_Palloc)(_Newsize)
				: _NEW_CRT char[_Newsize];
			}
		if (_Ptr == 0)
			return (EOF);	// couldn't grow, return failure

		if (0 < _Oldsize)
			_CSTD memcpy(_Ptr, eback(), _Oldsize);	// copy existing
		if (!(_Strmode & _Allocated))
			;	// no buffer to free
		else if (_Pfree != 0)
			(*_Pfree)(eback());	// free with function call
		else
			_DELETE_CRT_VEC(eback());	// free by deleting array

		_Strmode |= _Allocated;
		if (_Oldsize == 0)
			{	// set up new buffer
			_Seekhigh = _Ptr;
			setp(_Ptr, _Ptr + _Newsize);
			setg(_Ptr, _Ptr, _Ptr);
			}
		else
			{	// revise old pointers
			_Seekhigh = _Seekhigh - eback() + _Ptr;
			setp(pbase() - eback() + _Ptr, pptr() - eback() + _Ptr,
				_Ptr + _Newsize);
			setg(_Ptr, gptr() - eback() + _Ptr, pptr() + 1);
			}

		return ((unsigned char)(*_Pninc() = (char)_Meta));
		}
	}

int strstreambuf::pbackfail(int _Meta)
	{	// try to putback a character
	if (gptr() == 0 || gptr() <= eback() || (_Meta != EOF
			&& (unsigned char)_Meta != (unsigned char)gptr()[-1]
			&& _Strmode & _Constant))
		return (EOF);	// can't put it back
	else
		{	// safe to back up
		gbump(-1);
		if (_Meta != EOF)
			*gptr() = (char)_Meta;
		return (_Meta == EOF ? 0 : (unsigned char)_Meta);
		}
	}

int strstreambuf::underflow()
	{	// read if read position available
	if (gptr() == 0)
		return (EOF);	// no read buffer
	else if (gptr() < egptr())
		return ((unsigned char)*gptr());	// char in buffer, read it
	else if (pptr() == 0 || (pptr() <= gptr() && _Seekhigh <= gptr()))
		return (EOF);	// no write buffer to read
	else
		{	// update _Seekhigh and expand read region
		if (_Seekhigh < pptr())
			_Seekhigh = pptr();
		setg(eback(), gptr(), _Seekhigh);
		return ((unsigned char)*gptr());
		}
	}

streampos strstreambuf::seekoff(streamoff _Off_arg,
	ios_base::seekdir _Way,
	ios_base::openmode _Which)
	{	// seek by specified offset
	int _Off = (int)_Off_arg;

	if (pptr() != 0 && _Seekhigh < pptr())
		_Seekhigh = pptr();	// update high water mark

	if (_Which & ios_base::in && gptr() != 0)
		{	// set input (and maybe output) pointer
		if (_Way == ios_base::end)
			_Off += (int)(_Seekhigh - eback());	// seek from end
		else if (_Way == ios_base::cur
			&& !(_Which & ios_base::out))
			_Off += (int)(gptr() - eback());	// seek from current
		else if (_Way != ios_base::beg || _Off == (int)_BADOFF)
			_Off = (int)_BADOFF;	// invalid seek
		if (0 <= _Off && _Off <= (int)(_Seekhigh - eback()))
			{	// seek from beginning, set one or two pointers
			gbump(_Off - (int)(gptr() - eback()));
			if (_Which & ios_base::out && pptr() != 0)
				setp(pbase(), gptr(), epptr());
			}
		else
			_Off = (int)_BADOFF;	// invalid seek from beginning
		}
	else if (_Which & ios_base::out && pptr() != 0)
		{	// set only output pointer
		if (_Way == ios_base::end)
			_Off += (int)(_Seekhigh - eback());	// seek from end
		else if (_Way == ios_base::cur)
			_Off += (int)(pptr() - eback());	// seek from current
		else if (_Way != ios_base::beg || _Off == (int)_BADOFF)
			_Off = (int)_BADOFF;	// invalid seek
		if (0 <= _Off && _Off <= (int)(_Seekhigh - eback()))
			pbump(_Off - (int)(pptr() - eback()));	// seek from beginning
		else
			_Off = (int)_BADOFF;	// invalid seek from beginning
		}
	else
		_Off = (int)_BADOFF;	// nothing to set
	return (streampos((streamoff)_Off));
	}

streampos strstreambuf::seekpos(streampos _Sp,
	ios_base::openmode _Which)
	{	// seek to memorized position
	int _Off = (int)(streamoff)_Sp;

	if (pptr() != 0 && _Seekhigh < pptr())
		_Seekhigh = pptr();	// update high water mark

	if (_Off == (int)_BADOFF)
		;	// invalid seek
	else if (_Which & ios_base::in && gptr() != 0)
		{	// set input (and maybe output) pointer
		if (0 <= _Off && _Off <= (int)(_Seekhigh - eback()))
			{	// set valid offset
			gbump(_Off - (int)(gptr() - eback()));
			if (_Which & ios_base::out && pptr() != 0)
				setp(pbase(), gptr(), epptr());
			}
		else
			_Off = (int)_BADOFF;	// offset invalid, don't seek
		}
	else if (_Which & ios_base::out && pptr() != 0)
		{	// set output pointer
		if (0 <= _Off && _Off <= (int)(_Seekhigh - eback()))
			pbump(_Off - (int)(pptr() - eback()));	// seek from beginning
		else
			_Off = (int)_BADOFF;	// offset invalid, don't seek
		}
	else
		_Off = (int)_BADOFF;	// nothing to set
	return (streampos((streamoff)_Off));
	}

istrstream::~istrstream() _NOEXCEPT
	{	// destroy an istrstream
	}

ostrstream::ostrstream(char *_Ptr, streamsize _Count, ios_base::openmode _Mode)
	: ostream(&_Strbuffer),
		_Strbuffer(_Ptr, _Count,
			_Ptr == 0 || (_Mode & ios_base::app) == 0
				? _Ptr : _Ptr + _CSTD strlen(_Ptr))
	{	// construct with [_Ptr, _Ptr + _Count)
	}

ostrstream::~ostrstream() _NOEXCEPT
	{	// destroy an ostrstream
	}

strstream::strstream(char *_Ptr, streamsize _Count, ios_base::openmode _Mode)
	: iostream(&_Strbuffer),
		_Strbuffer(_Ptr, _Count,
			_Ptr == 0 || (_Mode & ios_base::app) == 0
				? _Ptr : _Ptr + _CSTD strlen(_Ptr))
	{	// construct with [_Ptr, _Ptr + _Count)
	}

strstream::~strstream() _NOEXCEPT
	{	// destroy a strstream
	}
_STD_END
