/*
 * xthreads.h: the threads library's C interface (Dinkumware's, as the SDK Release 03.570.011
 * ships it in target/include/Dinkum/threads: xthrcommon.h, xtimec.h and xthreads.h, the
 * __psp2__ branch), for the library's C sources.
 */
#ifndef _THREADS_XTHREADS_H
#define _THREADS_XTHREADS_H
#include <stddef.h>
#include <kernel/threadmgr.h>

enum {	/* return codes */
	_Thrd_success,
	_Thrd_nomem,
	_Thrd_timedout,
	_Thrd_busy,
	_Thrd_error
};

enum {	/* define TIME_UTC */
	TIME_UTC = 1
};

typedef struct xtime {	/* store time with nanosecond resolution */
	long sec;
	long nsec;
} xtime;

int xtime_get(xtime *, int);
long long _Xtime_get_ticks(void);
#define _XTIME_NSECS_PER_TICK	1000
#define _XTIME_TICKS_PER_TIME_T	(long long)1000000

typedef SceUID _Thrd_imp_t;
typedef SceKernelThreadEntry _Thrd_callback_t;
typedef struct _Mtx_internal_imp_t *_Mtx_imp_t;

typedef struct {	/* saved state for PS Vita mutex */
	int _Count;
	SceUID _Owner;
} _Mtx_state;

int _Thrd_start(_Thrd_imp_t *, _Thrd_callback_t, size_t, void *);
int _Save_state(struct _Mtx_internal_imp_t **, _Mtx_state *);
int _Restore_state(struct _Mtx_internal_imp_t **, _Mtx_state *);
typedef struct _Cnd_internal_imp_t *_Cnd_imp_t;
typedef int _Tss_imp_t;
typedef char _Once_flag_imp_t;
#define _ONCE_FLAG_INIT_IMP	0

	/* threads */
typedef _Thrd_imp_t _Thrd_t;
typedef int (*_Thrd_start_t)(void *);

int _Thrd_create(_Thrd_t *, _Thrd_start_t, void *);
int _Thrd_detach(_Thrd_t);
void _Thrd_exit(int);
int _Thrd_join(_Thrd_t, int *);
void _Thrd_sleep(const xtime *);
void _Thrd_yield(void);
int _Thrd_equal(_Thrd_t, _Thrd_t);
int _Thrd_lt(_Thrd_t, _Thrd_t);
_Thrd_t _Thrd_current(void);

	/* mutexes */
typedef _Mtx_imp_t _Mtx_t;
enum {	/* mutex types */
	_Mtx_plain = 0x01,
	_Mtx_try = 0x02,
	_Mtx_timed = 0x04,
	_Mtx_recursive = 0x100
};

int _Mtx_init(_Mtx_t *, int);
void _Mtx_destroy(_Mtx_t *);
int _Mtx_current_owns(_Mtx_t *);
int _Mtx_lock(_Mtx_t *);
int _Mtx_trylock(_Mtx_t *);
int _Mtx_timedlock(_Mtx_t *, const xtime *);
int _Mtx_unlock(_Mtx_t *);

	/* condition variables */
typedef _Cnd_imp_t _Cnd_t;

int _Cnd_init(_Cnd_t *);
void _Cnd_destroy(_Cnd_t *);
int _Cnd_wait(_Cnd_t *, _Mtx_t *);
int _Cnd_timedwait(_Cnd_t *, _Mtx_t *, const xtime *);
int _Cnd_broadcast(_Cnd_t *);
int _Cnd_signal(_Cnd_t *);
int _Cnd_register_at_thread_exit(_Cnd_t *, _Mtx_t *, int *);
int _Cnd_unregister_at_thread_exit(_Mtx_t *);
int _Cnd_do_broadcast_at_thread_exit(void);

	/* thread specific storage */
typedef _Tss_imp_t _Tss_t;
typedef void (*_Tss_dtor_t)(void *);
int _Tss_create(_Tss_t *, _Tss_dtor_t);
int _Tss_delete(_Tss_t);
int _Tss_set(_Tss_t, void *);
void *_Tss_get(_Tss_t);

	/* once functions */
#define _ONCE_FLAG_INIT		_ONCE_FLAG_INIT_IMP
#define _ONCE_FLAG_CPP_INIT	0

typedef _Once_flag_imp_t _Once_flag;
typedef int _Once_flag_cpp;
typedef void (*_Once_callback_t)(void);
typedef void (*_Once_callbackEx_t)(void *);

void _Call_once(_Once_flag *, _Once_callback_t);
void _Call_onceEx(_Once_flag_cpp *, _Once_callbackEx_t, void *);

	/* utility functions */
void _Thrd_abort(const char *);
#endif /* _THREADS_XTHREADS_H */
