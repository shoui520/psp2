// future.cpp: future_error's messages and destructor, and the helpers <future> calls to throw
// a future_error or rethrow a stored exception.
#include <future>

_STD_BEGIN
const char *_Future_error_map(int _Errcode)
	{	// convert to name of future error
	switch (_Errcode)
		{	// switch on error code value
	case (int)future_errc::broken_promise:
		return ("broken promise");

	case (int)future_errc::future_already_retrieved:
		return ("future already retrieved");

	case (int)future_errc::promise_already_satisfied:
		return ("promise already satisfied");

	case (int)future_errc::no_state:
		return ("no state");

	default:
		return (0);
		}
	}

__attribute__((noinline))	/* SCE: future_error::what calls it */
const char *_Get_future_error_what(int _Errcode) _THROW0()
	{	// get what string for future error
	switch (_Errcode)
		{	// switch on error code value
	case (int)future_errc::broken_promise:
		return ("broken promise");

	case (int)future_errc::future_already_retrieved:
		return ("future already retrieved");

	case (int)future_errc::promise_already_satisfied:
		return ("promise already satisfied");

	case (int)future_errc::no_state:
		return ("no state");

	default:
		return (0);
		}
	}

future_error::~future_error() _NOEXCEPT
	{	// destroy the object
	}

void _Throw_future_error(const error_code& _Code)
	{	// throw an exception object
	_THROW_NCEE(future_error, _Code);
	}

void _Rethrow_future_exception(_XSTD exception_ptr _Ptr)
	{	// rethrow an exception object
	_XSTD rethrow_exception(_Ptr);
	}
_STD_END

/* Exported by NID only: throws runtime_error("unknown exception"). */
extern "C" void SceLibstdcxx_A27A741F(void)
	{	// throw an unknown exception
	throw _STD runtime_error("unknown exception");
	}
