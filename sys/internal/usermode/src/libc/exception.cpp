// exception.cpp: exception and bad_exception, whose members the psp2 headers declare out of
// line, and the handler exception::_Raise reports through
#include <exception>

_STD_BEGIN
_Prhand _Raise_handler = 0;	// the pointer to the raise handler

_Prhand exception::_Set_raise_handler(_Prhand _Pnew)
	{	// register a handler for _Raise calls
	const _Prhand _Pold = _Raise_handler;

	_Raise_handler = _Pnew;
	return (_Pold);
	}

exception::exception() _THROW0()
	{	// default construct
	}

exception::exception(const exception&) _THROW0()
	{	// copy construct
	}

exception& exception::operator=(const exception&) _THROW0()
	{	// assign
	return (*this);
	}

exception::~exception() _THROW0()
	{	// destroy the object
	}

const char *exception::what() const _THROW0()
	{	// return pointer to message string
	return ("");
	}

void exception::_Raise() const
	{	// raise the exception
	if (_Raise_handler != 0)
		(*_Raise_handler)(*this);	// call raise handler if present

	_Doraise();	// call the protected virtual
	_RAISE(*this);	// raise this exception
	}

void exception::_Doraise() const
	{	// perform class-specific exception handling
	}

bad_exception::bad_exception() _THROW0()
	{	// construct with no message string
	}

bad_exception::bad_exception(const bad_exception&) _THROW0()
	{	// copy construct
	}

bad_exception& bad_exception::operator=(const bad_exception&) _THROW0()
	{	// assign
	return (*this);
	}

const char *bad_exception::what() const _THROW0()
	{	// report a bad exception
	return ("bad exception");
	}

bad_exception::~bad_exception() _THROW0()
	{	// destroy the object
	}
_STD_END
