/*
 * The system locks: their mutexes made on the first lock (and destroyed at exit). A
 * mutex is the application's, through the hooks; with none (here) the locks do nothing.
 */

#include "xmtx.h"

int __cxa_atexit(void (*func)(void *), void *arg, void *dso);

void (*_Mtxinit_hook)(_Rmtx *m);
void (*_Mtxdst_hook)(_Rmtx *m);
void (*_Mtxlock_hook)(_Rmtx *m);
void (*_Mtxunlock_hook)(_Rmtx *m);

static int initialized;
static _Rmtx mtx[_MAX_LOCK];

__attribute__((noinline)) void __iar_system_Mtxinit(_Rmtx *m)
{
	if (_Mtxinit_hook != 0)
		_Mtxinit_hook(m);
}

__attribute__((noinline)) void __iar_system_Mtxdst(_Rmtx *m)
{
	if (_Mtxdst_hook != 0)
		_Mtxdst_hook(m);
}

__attribute__((noinline)) void __iar_system_Mtxlock(_Rmtx *m)
{
	if (_Mtxlock_hook != 0)
		_Mtxlock_hook(m);
}

__attribute__((noinline)) void __iar_system_Mtxunlock(_Rmtx *m)
{
	if (_Mtxunlock_hook != 0)
		_Mtxunlock_hook(m);
}

static void clearlocks(void *arg)
{
	__iar_system_Mtxdst(&mtx[0]);
	__iar_system_Mtxdst(&mtx[1]);
	__iar_system_Mtxdst(&mtx[2]);
	__iar_system_Mtxdst(&mtx[3]);
}

void _Locksyslock(unsigned int lockno)
{
	if (!initialized) {
		__iar_system_Mtxinit(&mtx[0]);
		__iar_system_Mtxinit(&mtx[1]);
		__iar_system_Mtxinit(&mtx[2]);
		__iar_system_Mtxinit(&mtx[3]);
		__cxa_atexit(clearlocks, 0, 0);
	}
	initialized = 1;
	if (lockno < _MAX_LOCK)
		__iar_system_Mtxlock(&mtx[lockno]);
}

void _Unlocksyslock(unsigned int lockno)
{
	if (lockno < _MAX_LOCK)
		__iar_system_Mtxunlock(&mtx[lockno]);
}
