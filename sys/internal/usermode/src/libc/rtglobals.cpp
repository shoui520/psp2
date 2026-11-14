// rtglobals.cpp: where the C++ runtime keeps its global state: the new handler's cell, and
// each thread's exception state, a slot of its TLS area (the file name is ours).
#include <new>
#include <libkernel/libkernel.h>
#include "cxxabi_internal.h"
#include "newreplace.h"

#define TLS_EH_GLOBALS	212	/* the thread's __cxa_eh_globals */

static _STD new_handler new_handler_cell;	/* set_new_handler's */

_STD new_handler *_New_handler_address()
{	// the new handler's cell
	return (&new_handler_cell);
}

extern "C" __cxa_eh_globals *__rt_eh_globals_addr(void)
{	// the calling thread's exception state
	return ((__cxa_eh_globals *)sceKernelGetTLSAddr(TLS_EH_GLOBALS));
}
