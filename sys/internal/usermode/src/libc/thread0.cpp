// thread0.cpp: the system_error exceptions the thread library throws, and the trampoline
// call_once calls its target through.
#include <system_error>
#include <mutex>

_STD_BEGIN
static const char *const msgs[] =
	{	// error messages
	"device or resource busy",
	"invalid argument",
	"no such process",
	"not enough memory",
	"operation not permitted",
	"resource deadlock would occur",
	"resource unavailable try again"
	};

static const int codes[] =
	{	// error_code values
	(int)errc::device_or_resource_busy,
	(int)errc::invalid_argument,
	(int)errc::no_such_process,
	(int)errc::not_enough_memory,
	(int)errc::operation_not_permitted,
	(int)errc::resource_deadlock_would_occur,
	(int)errc::resource_unavailable_try_again
	};

void _Throw_Cpp_error(int code)
	{	// throw error object
	throw system_error(codes[code], generic_category(), msgs[code]);
	}

void _Throw_C_error(int code)
	{	// throw error object for C error
	switch (code)
		{	// select the exception
	case _Thrd_nomem:
	case _Thrd_timedout:
		_Throw_Cpp_error(_RESOURCE_UNAVAILABLE_TRY_AGAIN);

	case _Thrd_busy:
		_Throw_Cpp_error(_DEVICE_OR_RESOURCE_BUSY);

	case _Thrd_error:
		_Throw_Cpp_error(_INVALID_ARGUMENT);
		}
	}
_STD_END

_EXTERN_C
void _Do_call(void *_Tgt)
	{	// call target function
	((_STD _Once_pad *)_Tgt)->_Call();
	}
_END_EXTERN_C
