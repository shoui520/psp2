// xthread.cpp: _Pad, the handshake that launches a thread (std::thread)
#include <exception>
#include <system_error>
#include <Dinkum/threads/xthread>
_STD_BEGIN

static SceInt32 _Call_func(SceSize, void *_Data)
	{	// entry point for new thread
	unsigned int _Res = 0;

	_TRY_BEGIN
		_Res = (*static_cast<_Pad **>(_Data))->_Go();
	_CATCH_ALL
		_XSTD terminate();
	_CATCH_END
	_Cnd_do_broadcast_at_thread_exit();
	return (_Res);
	}

_Pad::_Pad()
	{	// initialize handshake
	_Cnd_initX(&_Cond);
	_Auto_cnd _Cnd_cleaner(&_Cond);
	_Mtx_initX(&_Mtx, _Mtx_plain);
	_Auto_mtx _Mtx_cleaner(&_Mtx);
	_Started = false;
	_pStarted = &_Started;
	_Mtx_lockX(&_Mtx);
	_Mtx_cleaner._Release();
	_Cnd_cleaner._Release();
	}

_Pad::~_Pad() _NOEXCEPT
	{	// clean up handshake
	_Auto_cnd _Cnd_cleaner(&_Cond);
	_Auto_mtx _Mtx_cleaner(&_Mtx);
	_Mtx_unlockX(&_Mtx);
	}

void _Pad::_Launch(_Thrd_t *thr, size_t size)
	{	// launch a thread
	_pStarted = &_Started;
	_Thrd_startX(thr, _Call_func, size, this);
	while (!_Started)
		_Cnd_waitX(&_Cond, &_Mtx);
	}

void _Pad::_Release()
	{	// notify caller that it's safe to continue
	_Mtx_lockX(&_Mtx);
	*_pStarted = true;
	_Cnd_signalX(&_Cond);
	_Mtx_unlockX(&_Mtx);
	}
_STD_END
