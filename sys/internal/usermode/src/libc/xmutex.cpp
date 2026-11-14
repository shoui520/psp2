// xmutex.cpp: _Mutex, the lock each stream buffer holds, over a C library lightweight mutex
#include <yvals.h>
#include <xmtx.h>

_STD_BEGIN
_Mutex::_Mutex(_Uninitialized)
	{	// construct without a mutex
	_MtxInitialized = -1;
	}

_Mutex::_Mutex()
	{	// construct with a mutex
	_MtxInitialized = 1;
	_Mtxinit(&_Mtx, "SceLibcStreambuf");
	}

_Mutex::~_Mutex() _NOEXCEPT
	{	// destroy the mutex, if made
	if (0 < _MtxInitialized)
		_Mtxdst(&_Mtx);
	}

int _Mutex::_Lock()
	{	// lock the mutex
	return (_Mtxlock(&_Mtx));
	}

int _Mutex::_Unlock()
	{	// unlock the mutex
	return (_Mtxunlock(&_Mtx));
	}
_STD_END
