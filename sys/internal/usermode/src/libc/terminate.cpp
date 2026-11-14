// terminate.cpp: terminate and unexpected, and their handlers, per thread
#include <stdlib.h>
#include "cxxabi_internal.h"
_STD_BEGIN

terminate_handler set_terminate(terminate_handler pnew) _THROW0()
	{	// set the terminate handler
	__cxa_eh_globals *globals = __cxa_get_globals();
	terminate_handler pold = globals->terminateHandler;

	globals->terminateHandler = pnew;
	return (pold);
	}

void terminate() _THROW0()
	{	// call the terminate handler, then abort
	sceKernelCallAbortHandler(SCE_LIBC_TERMINATE_CODE, 0);
	__cxa_eh_globals *globals = __cxa_get_globals();
	void (*handler)() = globals->call_hook;

	if (handler != 0)
		{	// a one-time handler
		globals->call_hook = 0;
		(*handler)();
		}
	else
		(*globals->terminateHandler)();
	abort();
	}

unexpected_handler set_unexpected(unexpected_handler pnew) _THROW0()
	{	// set the unexpected handler
	__cxa_eh_globals *globals = __cxa_get_globals();
	unexpected_handler pold = globals->unexpectedHandler;

	globals->unexpectedHandler = pnew;
	return (pold);
	}

void unexpected()
	{	// call the unexpected handler, then terminate
	sceKernelCallAbortHandler(SCE_LIBC_UNEXPECTED_CODE, 0);
	__cxa_eh_globals *globals = __cxa_get_globals();
	void (*handler)() = globals->call_hook;

	if (handler != 0)
		{	// a one-time handler
		globals->call_hook = 0;
		(*handler)();
		}
	else
		(*globals->unexpectedHandler)();
	terminate();
	}
_STD_END
