// badalloc.cpp: bad_alloc and bad_array_new_length, whose members the psp2 headers declare
// out of line
#include <new>

_STD_BEGIN
bad_alloc::bad_alloc() _THROW0()
	{	// construct with no message string
	}

bad_alloc::bad_alloc(const bad_alloc&) _THROW0()
	{	// construct a copy (the message is fixed)
	}

bad_alloc::bad_alloc(const char *) _THROW0()
	{	// construct, ignoring the message string
	}

bad_alloc& bad_alloc::operator=(const bad_alloc& _Right) _THROW0()
	{	// assign
	exception::operator=(_Right);
	return (*this);
	}

bad_alloc::~bad_alloc() _THROW0()
	{	// destroy the object
	}

const char *bad_alloc::what() const _THROW0()
	{	// report a bad allocation
	return ("bad allocation");
	}

bad_array_new_length::~bad_array_new_length() _NOEXCEPT
	{	// destroy the object
	}
_STD_END
