// cxxabi_internal.h: the C++ runtime's exception objects and per-thread exception state, as
// the ARM exception handling ABI (EHABI) lays them out, and the system calls the C++ library
// makes. The names are the C++ ABI's and EHABI's where they have one, else ours.
#ifndef _CXXABI_INTERNAL_H
#define _CXXABI_INTERNAL_H
#include <exception>
#include <typeinfo>
#include "unwinder.h"
#include "../../../../sdk/target/src/libsnc/rtti.h"

#define SCE_LIBC_TERMINATE_CODE		0x108	/* sceKernelCallAbortHandler: terminate() */
#define SCE_LIBC_UNEXPECTED_CODE	0x109	/* sceKernelCallAbortHandler: unexpected() */

		// THE C++ EXCEPTION OBJECT
struct __cxa_exception
	{	// the header in front of a thrown object
	union
		{	// the first word
		unsigned int referenceCount;	// a primary exception: its references
		void *primaryException;	// a dependent exception: the object it rethrows
		};
	unsigned int reserved;
	const _STD type_info *exceptionType;
	void (*exceptionDestructor)(void *);
	_STD unexpected_handler unexpectedHandler;
	_STD terminate_handler terminateHandler;
	__cxa_exception *nextCaughtException;
	int handlerCount;
	__cxa_exception *nextPropagatingException;
	int propagationCount;
	_Unwind_Control_Block ucb;
	};

		// PER-THREAD STATE
struct __cxa_eh_globals
	{	// a thread's exception state
	unsigned int uncaughtExceptions;
	_STD unexpected_handler unexpectedHandler;
	_STD terminate_handler terminateHandler;
	bool implementation_ever_called_terminate;
	void (*call_hook)();	// called once, in place of the next handler
	__cxa_exception *caughtExceptions;
	__cxa_exception *propagatingExceptions;
	void *emergency_buffer;
	int initialized;
	};

typedef enum
	{	// how a thrown object matched a handler
	ctm_failed = 0,
	ctm_succeeded = 1,
	ctm_succeeded_with_ptr_to_base = 2
	} __cxa_type_match_result;

extern "C" {
__cxa_eh_globals *__cxa_get_globals();
__cxa_type_match_result __cxa_type_match(_Unwind_Control_Block *ucbp,
	const _STD type_info *match_type, bool is_reference_type, void **matched_object);
bool __cxa_begin_cleanup(_Unwind_Control_Block *ucbp);
void __cxa_call_terminate(_Unwind_Control_Block *ucbp);
void __cxa_call_unexpected(_Unwind_Control_Block *ucbp);
int sceKernelCallAbortHandler(unsigned int code, const void *opt);
}
#endif /* _CXXABI_INTERNAL_H */
