// cxxabi_internal.h: the C++ runtime's exception objects and per-thread exception state, as
// the ARM exception handling ABI (EHABI) lays them out, and the system calls the C++ library
// makes. The names are the C++ ABI's and EHABI's where they have one, else ours.
#ifndef _CXXABI_INTERNAL_H
#define _CXXABI_INTERNAL_H
#include <exception>
#include <typeinfo>

#define SCE_LIBC_TERMINATE_CODE		0x108	/* sceKernelCallAbortHandler: terminate() */
#define SCE_LIBC_UNEXPECTED_CODE	0x109	/* sceKernelCallAbortHandler: unexpected() */

extern "C" {
		// THE EHABI UNWINDER INTERFACE
typedef enum
	{	// unwinder and personality routine results
	_URC_OK = 0,
	_URC_FOREIGN_EXCEPTION_CAUGHT = 1,
	_URC_HANDLER_FOUND = 6,
	_URC_INSTALL_CONTEXT = 7,
	_URC_CONTINUE_UNWIND = 8,
	_URC_FAILURE = 9
	} _Unwind_Reason_Code;

typedef struct _Unwind_Control_Block
	{	// the exception object the unwinder sees
	char exception_class[8];
	void (*exception_cleanup)(_Unwind_Reason_Code, struct _Unwind_Control_Block *);
	struct
		{	// the unwinder's own state
		unsigned int reserved1;	// nonzero while a copy of the state is saved
		unsigned int reserved2;
		unsigned int reserved3;
		unsigned int reserved4;
		unsigned int reserved5;
		} unwinder_cache;
	struct
		{	// what phase 1 found, for phase 2
		unsigned int sp;
		unsigned int bitpattern[5];
		} barrier_cache;
	struct
		{	// the personality routine's cleanup state
		unsigned int bitpattern[4];
		} cleanup_cache;
	struct
		{	// the current frame's exception table entry
		unsigned int fnstart;
		void *ehtp;
		unsigned int additional;
		unsigned int reserved1;
		} pr_cache;
	long long : 0;
	} _Unwind_Control_Block;

_Unwind_Reason_Code _Unwind_RaiseException(_Unwind_Control_Block *ucbp);
void _Unwind_Resume(_Unwind_Control_Block *ucbp);
void _Unwind_Complete(_Unwind_Control_Block *ucbp);
void _Unwind_DeleteException(_Unwind_Control_Block *ucbp);

		// THE DEBUGGER HOOK
#define _UASUBSYS_CPP		0x00	/* _Unwind_Activity: the C++ runtime */
#define _UAACT_STARTING		0x0	/* _Unwind_Activity: a throw starts */
void _Unwind_Activity(_Unwind_Control_Block *ucbp, unsigned int reason, unsigned int arg);
}

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

extern "C" {
__cxa_eh_globals *__cxa_get_globals();
int sceKernelCallAbortHandler(unsigned int code, const void *opt);
}
#endif /* _CXXABI_INTERNAL_H */
