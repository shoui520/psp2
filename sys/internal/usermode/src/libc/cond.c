/*
 * cond.c: the threads library's condition variables, over a lightweight condition
 * variable and the lightweight mutex it waits with
 */
#include <stdlib.h>
#include <Dinkum/threads/xthreads.h>

#include "xthrpsp2.h"

int _Cnd_init(_Cnd_t *cond)
{
	struct _Cnd_internal_imp_t *cnd;

	if ((cnd = calloc(1, sizeof (*cnd))) == NULL)
		return _Thrd_nomem;
	if (sceKernelCreateLwMutex(&cnd->mtx, "SceLibc", SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO, 0,
				   NULL) != SCE_OK) {
		free(cnd);
		return _Thrd_error;
	}
	if (sceKernelCreateLwCond(&cnd->cond, "SceLibc", SCE_KERNEL_LW_COND_ATTR_TH_PRIO,
				  &cnd->mtx, NULL) != SCE_OK) {
		sceKernelDeleteLwMutex(&cnd->mtx);
		free(cnd);
		return _Thrd_error;
	}
	*cond = cnd;
	return _Thrd_success;
}

void _Cnd_destroy(_Cnd_t *cond)
{
	if (cond != NULL && *cond != NULL) {
		sceKernelDeleteLwCond(&(*cond)->cond);
		sceKernelDeleteLwMutex(&(*cond)->mtx);
		free(*cond);
		*cond = NULL;
	}
}

int _Cnd_wait(_Cnd_t *cond, _Mtx_t *mtx)
{
	_Mtx_state state;
	int res;

	if (_Save_state(mtx, &state) != _Thrd_success)
		return _Thrd_error;
	if (sceKernelLockLwMutex(&(*cond)->mtx, 1, NULL) != SCE_OK)
		return _Thrd_error;
	res = sceKernelWaitLwCond(&(*cond)->cond, NULL);
	if (sceKernelUnlockLwMutex(&(*cond)->mtx, 1) != SCE_OK)
		return _Thrd_error;
	if (_Restore_state(mtx, &state) != _Thrd_success)
		return _Thrd_error;
	return res != SCE_OK ? _Thrd_error : _Thrd_success;
}

int _Cnd_timedwait(_Cnd_t *cond, _Mtx_t *mtx, const xtime *xt)
{
	_Mtx_state state;
	SceUInt32 timeout;
	int res;

	if (_Save_state(mtx, &state) != _Thrd_success)
		return _Thrd_error;
	if (sceKernelLockLwMutex(&(*cond)->mtx, 1, NULL) != SCE_OK)
		return _Thrd_error;
	timeout = _Xtime_diff_to_usec(xt);
	res = sceKernelWaitLwCond(&(*cond)->cond, &timeout);
	if (sceKernelUnlockLwMutex(&(*cond)->mtx, 1) != SCE_OK)
		return _Thrd_error;
	if (_Restore_state(mtx, &state) != _Thrd_success)
		return _Thrd_error;
	if (res == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
		return _Thrd_timedout;
	return res != SCE_OK ? _Thrd_error : _Thrd_success;
}

int _Cnd_signal(_Cnd_t *cond)
{
	return sceKernelSignalLwCond(&(*cond)->cond) != SCE_OK ? _Thrd_error : _Thrd_success;
}

int _Cnd_broadcast(_Cnd_t *cond)
{
	return sceKernelSignalLwCondAll(&(*cond)->cond) != SCE_OK ? _Thrd_error : _Thrd_success;
}
