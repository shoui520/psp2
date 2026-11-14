// iostream.cpp: the byte standard streams cin, cout, cerr and clog, and ios_base::Init, which
// constructs them over the C streams on its first construction and flushes them on its last
// destruction.
#include <fstream>
#include <istream>
#include <new>

_STD_BEGIN
int ios_base::Init::_Init_cnt = -1;

static filebuf fin(_Noinit);
static filebuf fout(_Noinit);
istream cin(_Noinit);
ostream cout(_Noinit);
static filebuf ferr(_Noinit);
ostream cerr(_Noinit);
ostream clog(_Noinit);

// the stream constructor Init calls, out of line (exported)
template basic_ostream<char, char_traits<char> >::basic_ostream(
	basic_streambuf<char, char_traits<char> > *, bool);

ios_base::Init::Init()
{	// initialize standard streams on first construction
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

	if (doinit) {	// initialize the standard streams
		new (&fin) filebuf(stdin);
		new (&fout) filebuf(stdout);
		new (&cin) istream(&fin, true);
		new (&cout) ostream(&fout, true);
		cin.tie(&cout);

		new (&ferr) filebuf(stderr);
		new (&cerr) ostream(&ferr, true);
		cerr.tie(&cout);
		cerr.setf(ios_base::unitbuf);
		new (&clog) ostream(&ferr, true);
		clog.tie(&cout);
	}
}

ios_base::Init::~Init() _NOEXCEPT
{	// flush standard streams on last destruction
	bool doflush;

	{	// count the destruction
		_Lockit lock(_LOCK_STREAM);

		doflush = --_Init_cnt == 0;
	}

	if (doflush) {	// flush the standard streams
		_TRY_BEGIN
		cout.flush();
		cerr.flush();
		clog.flush();
		_CATCH_ALL
		_CATCH_END
	}
}
_STD_END
