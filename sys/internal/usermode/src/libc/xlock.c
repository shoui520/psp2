/*
 * The library's locks: the standard streams' locks, the system locks (_Locksyslock) and the
 * condition the C++ static guards wait on.
 */
#include <stddef.h>
#include <stdio.h>
#include <sceerror.h>
#include <kernel/threadmgr.h>

#include <xmtx.h>

#include "libc_internal.h"

static unsigned int nsyslocks = 0;	/* the system locks made so far */

static const char *syslocknames[_MAX_LOCK] = {
	"SceLibcStream",
	"SceLibcDebug",
	"SceLibcMisc",
	"SceLibcAtexit",
	"SceLibcCxaguard",
	NULL,			/* _LOCK_ONCE: _Mtxinit's default name */
};

static SceKernelLwCondWork guardcnd;	/* waits on syslocks[_LOCK_GUARD] */
static _SceLibcMutexType syslocks[_MAX_LOCK];

int _TermLocks(void)
{
	unsigned int n = nsyslocks;
	unsigned int i;
	int res;

	_Cnddst(&guardcnd);
	nsyslocks = 0;
	for (i = 0; i < n; ++i)
		if (_Mtxdst(&syslocks[i]) < 0)
			break;
	if ((res = _Mtxdst(&_Stderr._stream_mtx)) < 0)
		return res;
	if ((res = _Mtxdst(&_Stdout._stream_mtx)) < 0)
		return res;
	return _Mtxdst(&_Stdin._stream_mtx);
}

int _InitLocks(void)
{
	unsigned int i;
	int res;

	if ((res = _Mtxinit(&_Stdin._stream_mtx, "SceLibcFileStdin")) < 0)
		return res;
	if ((res = _Mtxinit(&_Stdout._stream_mtx, "SceLibcFileStdout")) < 0)
		return res;
	if ((res = _Mtxinit(&_Stderr._stream_mtx, "SceLibcFileStderr")) < 0)
		return res;
	for (i = 0; i < _MAX_LOCK; ++i)
		if ((res = _Mtxinit(&syslocks[i], syslocknames[i])) < 0)
			break;
	nsyslocks = i;
	if (res < 0)
		return res;
	return _Cndinit(&guardcnd, "SceLibcCxaguard", &syslocks[_LOCK_GUARD]);
}

int _Lockfilelock(_Filet *str)
{
	if (str != NULL)
		return _Mtxlock(&str->_stream_mtx);
	return SCE_ERROR_ERRNO_EINVAL;
}

int _Unlockfilelock(_Filet *str)
{
	if (str != NULL)
		return _Mtxunlock(&str->_stream_mtx);
	return SCE_ERROR_ERRNO_EINVAL;
}

int _Locksyslock(unsigned int num)
{
	if (num < nsyslocks)
		return _Mtxlock(&syslocks[num]);
	return SCE_ERROR_ERRNO_EINVAL;
}

int _Unlocksyslock(unsigned int num)
{
	if (num < nsyslocks)
		return _Mtxunlock(&syslocks[num]);
	return SCE_ERROR_ERRNO_EINVAL;
}

/* wake the threads waiting for a C++ static guard (num 0: the only guard condition) */
int _Guardbroadcast(int num)
{
	if (num == 0)
		return _Cndbroadcast(&guardcnd);
	return SCE_ERROR_ERRNO_EINVAL;
}

/* wait for a C++ static guard; syslocks[_LOCK_GUARD] is held */
int _Guardwait(int num)
{
	if (num == 0)
		return _Cndwait(&guardcnd);
	return SCE_ERROR_ERRNO_EINVAL;
}
