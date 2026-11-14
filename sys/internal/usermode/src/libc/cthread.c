/*
 * cthread.c: the threads library's threads, kernel threads of the lowest user priority with
 * the smallest stack, on any user processor
 */
#include <stdio.h>
#include <stdlib.h>

#include "xthrpsp2.h"

typedef struct {	/* the thread function, and the handshake with its creator */
	_Thrd_start_t func;
	void *data;
	_Cnd_t *cond;
	_Mtx_t *mtx;
	int *started;
} _Thrd_binder;

static SceInt32 _Thrd_runner(SceSize argSize, void *pArgBlock);

int _Thrd_start(_Thrd_t *thr, _Thrd_callback_t func, size_t size, void *b)
{
	int res;

	if ((*thr = sceKernelCreateThread("SceLibc", func, SCE_KERNEL_LOWEST_PRIORITY_USER,
					  SCE_KERNEL_THREAD_STACK_SIZE_MIN, 0,
					  SCE_KERNEL_CPU_MASK_USER_ALL, NULL)) < 0)
		return _Thrd_error;
	res = _Thrd_error;
	if (sceKernelStartThread(*thr, sizeof (b), &b) == SCE_OK)
		res = _Thrd_success;
	return res;
}

int _Thrd_join(_Thrd_t thr, int *code)
{
	int res;
	SceInt32 stat;

	if (sceKernelWaitThreadEnd(thr, &stat, NULL) != SCE_OK)
		return _Thrd_error;
	if (code != NULL)
		*code = stat;
	res = _Thrd_error;
	if (sceKernelDeleteThread(thr) == SCE_OK)
		res = _Thrd_success;
	return res;
}

int _Thrd_detach(_Thrd_t thr)
{
	int res = _Thrd_error;

	if (sceKernelDeleteThread(thr) == SCE_OK)
		res = _Thrd_success;
	return res;
}

void _Thrd_exit(int code)
{
	sceKernelExitThread(code);
}

void _Thrd_sleep(const xtime *xt)
{
	sceKernelDelayThread(_Xtime_diff_to_usec(xt));
}

void _Thrd_yield(void)
{
	xtime xt;

	xtime_get(&xt, TIME_UTC);
	sceKernelDelayThread(_Xtime_diff_to_usec(&xt));
}

int _Thrd_equal(_Thrd_t thr0, _Thrd_t thr1)
{
	return thr0 == thr1;
}

int _Thrd_lt(_Thrd_t thr0, _Thrd_t thr1)
{
	return thr0 < thr1;
}

_Thrd_t _Thrd_current(void)
{
	return sceKernelGetThreadId();
}

void _Thrd_abort(const char *msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	abort();
}

int _Thrd_create(_Thrd_t *thr, _Thrd_start_t func, void *d)
{
	int res;
	int started = 0;
	_Cnd_t cond;
	_Mtx_t mtx;
	_Thrd_binder b;

	_Cnd_init(&cond);
	_Mtx_init(&mtx, _Mtx_plain);
	b.func = func;
	b.data = d;
	b.cond = &cond;
	b.mtx = &mtx;
	b.started = &started;
	_Mtx_lock(&mtx);
	if ((res = _Thrd_start(thr, _Thrd_runner, sizeof (b), &b)) == _Thrd_success) {
		while (!started)	/* wait for the handshake */
			_Cnd_wait(&cond, &mtx);
	}
	_Mtx_unlock(&mtx);
	_Cnd_destroy(&cond);
	_Mtx_destroy(&mtx);
	return res;
}

static SceInt32 _Thrd_runner(SceSize argSize, void *pArgBlock)
{
	return 0;
}
