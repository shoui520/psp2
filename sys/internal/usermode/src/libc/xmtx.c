/*
 * The C library's locks: lightweight mutexes, and the lightweight conditions the C++ static
 * guards wait on.
 */
#include <stddef.h>
#include <sceerror.h>
#include <kernel/threadmgr.h>

#include <xmtx.h>

#include "libc_internal.h"

int _Cndinit(SceKernelLwCondWork *cnd, const char *name, _SceLibcMutexType *mtx)
{
	SceInt32 res = SCE_ERROR_ERRNO_EINVAL;

	if (cnd != NULL && mtx != NULL)
		res = sceKernelCreateLwCond(cnd, name, SCE_KERNEL_LW_COND_ATTR_TH_PRIO,
					    (SceKernelLwMutexWork *)mtx, NULL);
	return res;
}

int _Cnddst(SceKernelLwCondWork *cnd)
{
	SceInt32 res = SCE_ERROR_ERRNO_EINVAL;

	if (cnd != NULL)
		res = sceKernelDeleteLwCond(cnd);
	return res;
}

int _Cndwait(SceKernelLwCondWork *cnd)
{
	SceInt32 res = SCE_ERROR_ERRNO_EINVAL;

	if (cnd != NULL)
		res = sceKernelWaitLwCond(cnd, NULL);
	return res;
}

int _Cndbroadcast(SceKernelLwCondWork *cnd)
{
	SceInt32 res = SCE_ERROR_ERRNO_EINVAL;

	if (cnd != NULL)
		res = sceKernelSignalLwCondAll(cnd);
	return res;
}

SceInt32 _Mtxinit(_SceLibcMutexType *mtx, const char *name)
{
	SceInt32 res;

	if (name == NULL)
		name = "SceLibc";
	if (mtx != NULL) {
		res = sceKernelCreateLwMutex((SceKernelLwMutexWork *)mtx, name,
					     SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO | SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE,
					     0, NULL);
		if (0 <= res)
			return 0;
		return res;
	}
	return SCE_ERROR_ERRNO_EINVAL;
}

SceInt32 _Mtxdst(_SceLibcMutexType *mtx)
{
	SceInt32 res;

	if (mtx != NULL) {
		res = sceKernelDeleteLwMutex((SceKernelLwMutexWork *)mtx);
		if (0 <= res)
			return 0;
		return res;
	}
	return SCE_ERROR_ERRNO_EINVAL;
}

SceInt32 _Mtxlock(_SceLibcMutexType *mtx)
{
	SceInt32 res;

	if (mtx != NULL) {
		res = sceKernelLockLwMutex((SceKernelLwMutexWork *)mtx, 1, NULL);
		if (0 <= res)
			return 0;
		return res;
	}
	return SCE_ERROR_ERRNO_EINVAL;
}

SceInt32 _Mtxunlock(_SceLibcMutexType *mtx)
{
	SceInt32 res;

	if (mtx != NULL) {
		res = sceKernelUnlockLwMutex((SceKernelLwMutexWork *)mtx, 1);
		if (0 <= res)
			return 0;
		return res;
	}
	return SCE_ERROR_ERRNO_EINVAL;
}
