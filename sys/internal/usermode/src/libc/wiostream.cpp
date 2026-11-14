// wiostream.cpp: the wide standard streams wcin, wcout, wcerr and wclog, and _Winit, which
// constructs them over the C streams on its first construction and flushes them on its last
// destruction.
#include <fstream>
#include <iostream>
#include <new>

_STD_BEGIN
int _Winit::_Init_cnt = -1;

static wfilebuf wfin(_Noinit);
static wfilebuf wfout(_Noinit);
static wfilebuf wferr(_Noinit);
wistream wcin(_Noinit);
wostream wcout(_Noinit);
wostream wcerr(_Noinit);
wostream wclog(_Noinit);

// the stream constructor _Winit calls, out of line (exported)
template basic_ostream<wchar_t, char_traits<wchar_t> >::basic_ostream(
	basic_streambuf<wchar_t, char_traits<wchar_t> > *, bool);

_Winit::_Winit()
	{	// initialize standard wide streams on first construction
	bool doinit;

	{	// count the construction
		_Lockit lock(_LOCK_STREAM);

		if (0 <= _Init_cnt) {
			++_Init_cnt;
			doinit = false;
		} else {
			_Init_cnt = 1;
			doinit = true;
		}
	}

	if (doinit) {	// initialize the standard wide streams
		new (&wfin) wfilebuf(stdin);
		new (&wfout) wfilebuf(stdout);
		new (&wferr) wfilebuf(stderr);
		new (&wcin) wistream(&wfin, true);
		new (&wcout) wostream(&wfout, true);
		wcin.tie(&wcout);
		new (&wcerr) wostream(&wferr, true);
		wcerr.tie(&wcout);
		wcerr.setf(ios_base::unitbuf);
		new (&wclog) wostream(&wferr, true);
		wclog.tie(&wcout);
	}
	}

_Winit::~_Winit() _NOEXCEPT
	{	// flush standard wide streams on last destruction
	bool doflush;

	{	// count the destruction
		_Lockit lock(_LOCK_STREAM);

		doflush = --_Init_cnt == 0;
	}

	if (doflush) {	// flush the standard wide streams
		wcout.flush();
		wcerr.flush();
		wclog.flush();
	}
	}
_STD_END
