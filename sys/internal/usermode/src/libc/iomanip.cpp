// iomanip.cpp: the manipulators resetiosflags, setiosflags, setbase, setprecision and setw
#include <iomanip>
_STD_BEGIN

static void rsfun(ios_base&, ios_base::fmtflags);
static void sifun(ios_base&, ios_base::fmtflags);
static void sbfun(ios_base&, int);
static void spfun(ios_base&, streamsize);
static void swfun(ios_base&, streamsize);

_Smanip<ios_base::fmtflags> resetiosflags(ios_base::fmtflags mask)
	{	// manipulator to reset format settings
	return (_Smanip<ios_base::fmtflags>(&rsfun, mask));
	}

_Smanip<ios_base::fmtflags> setiosflags(ios_base::fmtflags mask)
	{	// manipulator to set format settings
	return (_Smanip<ios_base::fmtflags>(&sifun, mask));
	}

_Smanip<int> setbase(int base)
	{	// manipulator to set base
	return (_Smanip<int>(&sbfun, base));
	}

_Smanip<streamsize> setprecision(streamsize prec)
	{	// manipulator to set precision
	return (_Smanip<streamsize>(&spfun, prec));
	}

_Smanip<streamsize> setw(streamsize wide)
	{	// manipulator to set width
	return (_Smanip<streamsize>(&swfun, wide));
	}

static void rsfun(ios_base& iostr, ios_base::fmtflags mask)
	{	// reset specified format flags
	iostr.setf(ios_base::_Fmtzero, mask);
	}

static void sifun(ios_base& iostr, ios_base::fmtflags mask)
	{	// set specified format flags
	iostr.setf(ios_base::_Fmtmask, mask);
	}

static void sbfun(ios_base& iostr, int base)
	{	// set base
	iostr.setf(base == 8 ? ios_base::oct
		: base == 10 ? ios_base::dec
		: base == 16 ? ios_base::hex
		: ios_base::_Fmtzero,
			ios_base::basefield);
	}

static void spfun(ios_base& iostr, streamsize prec)
	{	// set precision
	iostr.precision(prec);
	}

static void swfun(ios_base& iostr, streamsize wide)
	{	// set width
	iostr.width(wide);
	}
_STD_END
