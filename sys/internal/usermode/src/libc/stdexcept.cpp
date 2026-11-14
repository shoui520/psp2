// stdexcept.cpp: the destructors of the standard exception classes, which the psp2 headers
// declare out of line
#include <stdexcept>

_STD_BEGIN
logic_error::~logic_error() _THROW0()
	{	// destroy the object
	}

domain_error::~domain_error() _THROW0()
	{	// destroy the object
	}

invalid_argument::~invalid_argument() _THROW0()
	{	// destroy the object
	}

length_error::~length_error() _THROW0()
	{	// destroy the object
	}

out_of_range::~out_of_range() _THROW0()
	{	// destroy the object
	}

runtime_error::~runtime_error() _THROW0()
	{	// destroy the object
	}

overflow_error::~overflow_error() _THROW0()
	{	// destroy the object
	}

underflow_error::~underflow_error() _THROW0()
	{	// destroy the object
	}

range_error::~range_error() _THROW0()
	{	// destroy the object
	}
_STD_END
