/*
 * xthrpsp2.h: the threads library's objects on psp2, a lightweight mutex (with its lock count
 * and owner) and a lightweight condition variable. The names are ours.
 */
#ifndef _XTHRPSP2_H
#define _XTHRPSP2_H
#include <sceerror.h>
#include <kernel/error.h>
#include <Dinkum/threads/xthreads.h>

struct _Mtx_internal_imp_t {	/* _Mtx_t */
	int type;			/* _Mtx_plain ... _Mtx_recursive */
	int reserved;
	SceKernelLwMutexWork mtx;
	int count;			/* the owner's locks */
	SceUID owner;
};

struct _Cnd_internal_imp_t {	/* _Cnd_t */
	SceKernelLwCondWork cond;
	SceKernelLwMutexWork mtx;	/* the condition variable's own */
};

/* xtime.c: a time as a timeout in microseconds */
SceUInt32 _Xtime_diff_to_usec(const xtime *xt);
#endif /* _XTHRPSP2_H */
