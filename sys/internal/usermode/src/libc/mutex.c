/*
 * mutex.c: the threads library's mutexes, lightweight mutexes that keep their owner and its
 * lock count
 */
#include <stdlib.h>

#include "xthrpsp2.h"

int _Mtx_init(_Mtx_t *mtx, int type)
{
	struct _Mtx_internal_imp_t *mutex;

	*mtx = NULL;
	if ((mutex = calloc(1, sizeof (*mutex))) == NULL)
		return _Thrd_nomem;
	if (sceKernelCreateLwMutex(&mutex->mtx, "SceLibc", SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO
				   | (type & _Mtx_recursive ? SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE : 0),
				   0, NULL) != SCE_OK) {
		free(mutex);
		return _Thrd_error;
	}
	mutex->type = type;
	*mtx = mutex;
	return _Thrd_success;
}

void _Mtx_destroy(_Mtx_t *mtx)
{
	if (mtx != NULL && *mtx != NULL) {
		sceKernelDeleteLwMutex(&(*mtx)->mtx);
		free(*mtx);
		*mtx = NULL;
	}
}

int _Mtx_unlock(_Mtx_t *mtx)
{
	--(*mtx)->count;
	if (sceKernelUnlockLwMutex(&(*mtx)->mtx, 1) != SCE_OK)
		return _Thrd_error;
	return _Thrd_success;
}

int _Mtx_lock(_Mtx_t *mtx)
{
	if (sceKernelLockLwMutex(&(*mtx)->mtx, 1, NULL) != SCE_OK)
		return _Thrd_error;
	++(*mtx)->count;
	(*mtx)->owner = sceKernelGetThreadId();
	return _Thrd_success;
}

int _Mtx_trylock(_Mtx_t *mtx)
{
	int res;

	if ((res = sceKernelTryLockLwMutex(&(*mtx)->mtx, 1)) == SCE_OK) {
		++(*mtx)->count;
		(*mtx)->owner = sceKernelGetThreadId();
		return _Thrd_success;
	}
	return res == SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN ? _Thrd_busy : _Thrd_error;
}

int _Mtx_timedlock(_Mtx_t *mtx, const xtime *xt)
{
	SceUInt32 timeout = _Xtime_diff_to_usec(xt);
	int res;

	if ((res = sceKernelLockLwMutex(&(*mtx)->mtx, 1, &timeout)) == SCE_OK) {
		++(*mtx)->count;
		(*mtx)->owner = sceKernelGetThreadId();
		return _Thrd_success;
	}
	return res == SCE_KERNEL_ERROR_WAIT_TIMEOUT ? _Thrd_timedout : _Thrd_error;
}

int _Save_state(_Mtx_t *mtx, _Mtx_state *state)
{
	int res = _Thrd_success;

	state->_Count = (*mtx)->count;
	state->_Owner = (*mtx)->owner;
	(*mtx)->count = 0;
	if (sceKernelUnlockLwMutex(&(*mtx)->mtx, 1) != SCE_OK)
		res = _Thrd_error;
	return res;
}

int _Restore_state(_Mtx_t *mtx, _Mtx_state *state)
{
	if (sceKernelLockLwMutex(&(*mtx)->mtx, 1, NULL) == SCE_OK) {
		++(*mtx)->count;
		(*mtx)->owner = sceKernelGetThreadId();
	}
	(*mtx)->count = state->_Count;
	(*mtx)->owner = state->_Owner;
	return _Thrd_success;
}

int _Mtx_current_owns(_Mtx_t *mtx)
{
	return (*mtx)->count != 0 && _Thrd_equal((*mtx)->owner, _Thrd_current());
}
