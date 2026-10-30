/*
 * Solaris condition variables (Sony's dtrace/sce/solaris_condvar.c): a kernel
 * condition variable on the adaptive mutex it is used with.
 */
#pragma once

#include <dtrace/sce/solaris_types.h>
#include <dtrace/sce/solaris_mutex.h>

typedef enum {
	CV_DEFAULT = 0,
	CV_DRIVER = 1
} kcv_type_t;

typedef struct kcondvar {
	kmutex_t *mutex;
	SceUID uid;
} kcondvar_t;

typedef long clock_t;

void cv_init(kcondvar_t *cvp, kmutex_t *mp, const char *name, kcv_type_t type, void *arg);
void cv_destroy(kcondvar_t *cvp);
void cv_wait(kcondvar_t *cvp, kmutex_t *mp);
void cv_signal(kcondvar_t *cvp);
void cv_broadcast(kcondvar_t *cvp);
clock_t cv_timedwait(kcondvar_t *cvp, kmutex_t *mp, clock_t tim);
