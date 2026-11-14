// cxxabi_internal.h: the C++ runtime's per-thread exception state and the system calls the
// C++ library makes. The names are the Itanium C++ ABI's where it has one, else ours.
#ifndef _CXXABI_INTERNAL_H
#define _CXXABI_INTERNAL_H
#include <exception>

#define SCE_LIBC_TERMINATE_CODE		0x108	/* sceKernelCallAbortHandler: terminate() */
#define SCE_LIBC_UNEXPECTED_CODE	0x109	/* sceKernelCallAbortHandler: unexpected() */

struct __cxa_eh_globals
	{	// a thread's exception state
	void *caughtExceptions;
	_STD unexpected_handler unexpectedHandler;
	_STD terminate_handler terminateHandler;
	unsigned char flag;
	void (*pendingHandler)();	// called once, in place of the next handler
	void *reserved[3];
	int initialized;
	};

extern "C" {
__cxa_eh_globals *__cxa_get_globals();
int sceKernelCallAbortHandler(unsigned int code, const void *opt);
}
#endif /* _CXXABI_INTERNAL_H */
