// cxxabi_internal.h: the C++ runtime's exception objects and per-thread exception state, as
// the ARM exception handling ABI (EHABI) lays them out, and the system calls the C++ library
// makes. The names are the C++ ABI's and EHABI's where they have one, else ours.
#ifndef _CXXABI_INTERNAL_H
#define _CXXABI_INTERNAL_H
#include <exception>
#include <typeinfo>
#include "unwinder.h"

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

		// RUN-TIME TYPE INFORMATION
namespace __cxxabiv1 {
class __fundamental_type_info
	: public _STD type_info
	{	// a fundamental type
public:
	virtual ~__fundamental_type_info() _THROW0();
	};

class __array_type_info
	: public _STD type_info
	{	// an array type
public:
	virtual ~__array_type_info() _THROW0();
	};

class __function_type_info
	: public _STD type_info
	{	// a function type
public:
	virtual ~__function_type_info() _THROW0();
	};

class __enum_type_info
	: public _STD type_info
	{	// an enumeration type
public:
	virtual ~__enum_type_info() _THROW0();
	};

class __class_type_info
	: public _STD type_info
	{	// a class with no base
public:
	virtual ~__class_type_info() _THROW0();
	};

class __si_class_type_info
	: public __class_type_info
	{	// a class with one public, non-virtual base at offset zero
public:
	virtual ~__si_class_type_info() _THROW0();

	const __class_type_info *__base_type;
	};

struct __base_class_type_info
	{	// a base of a class with several bases
	const __class_type_info *__base_type;
	long __offset_flags;
	};

class __vmi_class_type_info
	: public __class_type_info
	{	// a class with several, or virtual, or non-public bases
public:
	virtual ~__vmi_class_type_info() _THROW0();

	unsigned int __flags;
	unsigned int __base_count;
	__base_class_type_info __base_info[1];
	};

class __pbase_type_info
	: public _STD type_info
	{	// a pointer or a pointer to member
public:
	virtual ~__pbase_type_info() _THROW0();

	unsigned int __flags;
	const _STD type_info *__pointee;

	enum __masks
		{	// the qualifiers of the pointee
		__const_mask = 0x1,
		__volatile_mask = 0x2,
		__restrict_mask = 0x4,
		__incomplete_mask = 0x8,
		__incomplete_class_mask = 0x10
		};
	};

class __pointer_type_info
	: public __pbase_type_info
	{	// a pointer
public:
	virtual ~__pointer_type_info() _THROW0();
	};

class __pointer_to_member_type_info
	: public __pbase_type_info
	{	// a pointer to member
public:
	virtual ~__pointer_to_member_type_info() _THROW0();

	const __class_type_info *__context;
	};
}	// namespace __cxxabiv1

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
