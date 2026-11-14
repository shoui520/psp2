// uncaught.cpp: uncaught_exception, which tells whether an exception is propagating
#include <exception>
#include "cxxabi_internal.h"
_STD_BEGIN

bool uncaught_exception() _THROW0()
	{	// test whether an exception is propagating, or terminate is running
	__cxa_eh_globals *globals = __cxa_get_globals();

	return (globals->implementation_ever_called_terminate
		|| globals->uncaughtExceptions != 0);
	}
_STD_END
