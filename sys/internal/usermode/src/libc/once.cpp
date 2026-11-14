// once.cpp: _Call_onceEx, the protected call behind std::call_once: one mutex, made on first
// use, serializes the calls, and an exception from the target leaves the flag clear.
#include <stdlib.h>
#include <yvals.h>
#include <Dinkum/threads/xthreads.h>

static _Once_flag mtx_once = _ONCE_FLAG_INIT;
static _Mtx_t mtx;

static void init_mtx(void);

class _Once_lock
	{	// holds the call-once mutex while it lives
public:
	_Once_lock()
		{	// lock the mutex
		_Mtx_lock(&mtx);
		}

	~_Once_lock() _NOEXCEPT
		{	// unlock the mutex
		_Mtx_unlock(&mtx);
		}
	};

void _Call_onceEx(_Once_flag_cpp *flag, _Once_callbackEx_t func, void *pv)
	{	// call func(pv) once, under the call-once mutex
	_Call_once(&mtx_once, init_mtx);
	_Once_lock lock;

	if (*flag == 0)
		{	// not called yet: call it
		func(pv);
		*flag = 1;
		}
	}

static void destroy_mtx(void)
	{	// destroy the call-once mutex at exit
	_Mtx_destroy(&mtx);
	}

static void init_mtx(void)
	{	// make the call-once mutex
	if (_Mtx_init(&mtx, _Mtx_plain) != _Thrd_success)
		_Thrd_abort("unable to initialize call-once mutex");
	atexit(destroy_mtx);
	}
