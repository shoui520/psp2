// thread0.cpp: the system_error exceptions the thread library throws, and the trampoline
// call_once calls its target through, the lock tags and bind's placeholders.
#include <functional>
#include <system_error>
#include <mutex>

_STD_BEGIN
namespace placeholders {	// placeholders
_Ph<1> _1;
_Ph<2> _2;
_Ph<3> _3;
_Ph<4> _4;
_Ph<5> _5;
_Ph<6> _6;
_Ph<7> _7;
_Ph<8> _8;
_Ph<9> _9;
_Ph<10> _10;
_Ph<11> _11;
_Ph<12> _12;
_Ph<13> _13;
_Ph<14> _14;
_Ph<15> _15;
_Ph<16> _16;
_Ph<17> _17;
_Ph<18> _18;
_Ph<19> _19;
_Ph<20> _20;
}	// namespace placeholders

const adopt_lock_t adopt_lock = adopt_lock_t();
const try_to_lock_t try_to_lock = try_to_lock_t();
const defer_lock_t defer_lock = defer_lock_t();

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
