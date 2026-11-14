// ios.cpp: ios_base's members the headers declare out of line (_HAS_BACKWARD_COMPATIBILITY),
// the standard-stream table that keeps cin, cout and the rest alive, and the file-position
// constants.
#include <xiosbase>

_STD_BEGIN
#define NSTDSTR	8	/* cin, wcin, cout, wcout, cerr, wcerr, clog, wclog */

fpos_t _Fpz = {0};
extern const streamoff _BADOFF = -1;

static ios_base *stdstr[NSTDSTR + 2] = {0};	/* the standard streams */
static signed char stdopens[NSTDSTR + 2] = {0};	/* their open counts */

_STD_END

char _PJP_CPP_Copyright[] =	/* global: the stub library names it unmangled */
	"Copyright (c) 1992-2012 by P.J. Plauger,"
	" licensed by Dinkumware, Ltd."
	" ALL RIGHTS RESERVED.";

_STD_BEGIN

template const _Iosb<int>::_Iostate _Iosb<int>::goodbit;	/* exported */
template const _Iosb<int>::_Seekdir _Iosb<int>::beg;

int ios_base::_Index = 0;
bool ios_base::_Sync = true;

/*
 * Sony's build calls the string constructor out of line in clear (the exported copy);
 * SNC's explicit-inline budget for clear reproduces it.
 */
#pragma control %push inlinesize=128
void ios_base::clear(iostate _State, bool _Reraise)
{	// set state, possibly reraise exception
	_Mystate = (iostate)(_State & _Statmask);
	if ((_Mystate & _Except) == 0)
		;
	else if (_Reraise)
		_RERAISE;
	else if (_Mystate & _Except & badbit)
		_THROW_N(failure, string("ios_base::badbit set"));
	else if (_Mystate & _Except & failbit)
		_THROW_N(failure, string("ios_base::failbit set"));
	else
		_THROW_N(failure, string("ios_base::eofbit set"));
}
#pragma control %pop inlinesize

ios_base::_Iosarray& ios_base::_Findarr(int _Idx)
{	// locate or make a variable array element
	_Iosarray *_Ptr1, *_Ptr2;

	for (_Ptr1 = _Arr, _Ptr2 = 0; _Ptr1 != 0; _Ptr1 = _Ptr1->_Next)
		if (_Ptr1->_Index == _Idx)
			return (*_Ptr1);	// found element, return it
		else if (_Ptr2 == 0 && _Ptr1->_Lo == 0 && _Ptr1->_Vp == 0)
			_Ptr2 = _Ptr1;	// found recycling candidate

	if (_Ptr2 != 0)
	{	// recycle existing element
		_Ptr2->_Index = _Idx;
		return (*_Ptr2);
	}

	_Arr = _NEW_CRT _Iosarray(_Idx, _Arr);	// make a new element
	return (*_Arr);
}

void ios_base::register_callback(event_callback _Pfn, int _Idx)
{	// register event handler
	_Calls = _NEW_CRT _Fnarray(_Idx, _Pfn, _Calls);
}

void ios_base::_Callfns(event _Ev)
{	// call all event handlers, reporting event
	for (_Fnarray *_Pfa = _Calls; _Pfa != 0; _Pfa = _Pfa->_Next)
		(*_Pfa->_Pfn)(_Ev, *this, _Pfa->_Index);
}

void ios_base::_Tidy()
{	// discard storage for an ios_base
	_Callfns(erase_event);
	_Iosarray *_Ptr1, *_Ptr2;

	for (_Ptr1 = _Arr; _Ptr1 != 0; _Ptr1 = _Ptr2)
	{	// delete array element
		_Ptr2 = _Ptr1->_Next;
		_DELETE_CRT(_Ptr1);
	}
	_Arr = 0;

	_Fnarray *_Pfa1, *_Pfa2;
	for (_Pfa1 = _Calls; _Pfa1 != 0; _Pfa1 = _Pfa2)
	{	// delete callback element
		_Pfa2 = _Pfa1->_Next;
		_DELETE_CRT(_Pfa1);
	}
	_Calls = 0;
}

ios_base& ios_base::copyfmt(const ios_base& _Other)
{	// copy format stuff
	if (this != &_Other)
	{	// copy all but _Mystate
		_Tidy();
		_Fmtfl = _Other._Fmtfl;
		_Prec = _Other._Prec;
		_Wide = _Other._Wide;
		_Iosarray *_Ptr = _Other._Arr;

		for (_Arr = 0; _Ptr != 0; _Ptr = _Ptr->_Next)
			if (_Ptr->_Lo != 0 || _Ptr->_Vp != 0)
			{	// copy over nonzero array values
				iword(_Ptr->_Index) = _Ptr->_Lo;
				pword(_Ptr->_Index) = _Ptr->_Vp;
			}

		for (_Fnarray *_Pfa = _Other._Calls; _Pfa != 0; _Pfa = _Pfa->_Next)
			register_callback(_Pfa->_Pfn, _Pfa->_Index);

		_Callfns(copyfmt_event);	// call callbacks
		exceptions(_Other._Except);	// cause any throw at end
	}
	return (*this);
}

ios_base::~ios_base() _NOEXCEPT
{	// destroy the object
	if (0 < _Stdstr && 0 < --stdopens[_Stdstr])
		return;
	_Tidy();
}

void ios_base::_Addstd(ios_base *_This)
{	// add standard stream to destructor list
	_Lockit _Lock(_LOCK_STREAM);

	for (_This->_Stdstr = 0; ++_This->_Stdstr < NSTDSTR;)
		if (stdstr[_This->_Stdstr] == 0 || stdstr[_This->_Stdstr] == _This)
			break;	// found a free slot or this stream's own
	stdstr[_This->_Stdstr] = _This;
	++stdopens[_This->_Stdstr];
}

void ios_base::_Addstd()
{	// add this as standard stream
	_Addstd(this);
}

void ios_base::_Init()
{	// initialize a new ios_base
	_Stdstr = 0;
	_Except = goodbit;
	_Fmtfl = (fmtflags)(skipws | dec);
	_Prec = 6;
	_Wide = 0;
	_Arr = 0;
	_Calls = 0;
	clear(goodbit);
}

ios_base::failure::~failure() _THROW0()
{	// destroy the object
}
_STD_END
