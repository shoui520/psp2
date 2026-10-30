/*
 * Solaris kernel mutexes (Sony's dtrace/sce/solaris_mutex.c): an adaptive
 * mutex is a kernel mutex, a spin mutex a spinlock with its owner's thread
 * ID and a name for the messages.
 */
#pragma once

#include <dtrace/sce/solaris_types.h>
#include <sysmem/spinlock.h>

typedef enum {
	MUTEX_ADAPTIVE = 0,	/* a kernel mutex */
	MUTEX_SPIN = 1,		/* a spinlock */
	MUTEX_DRIVER = 4,	/* a spinlock */
	MUTEX_DEFAULT = 6	/* a kernel mutex */
} kmutex_type_t;

typedef enum {
	DTRACE_MUTEX_KERNEL = 0,
	DTRACE_MUTEX_SPIN = 1
} dtrace_mutex_kind_t;

typedef struct kmutex {
	dtrace_mutex_kind_t kind;
	union {
		SceUID uid;			/* DTRACE_MUTEX_KERNEL */
		SceKernelSpinlock lock;		/* DTRACE_MUTEX_SPIN */
	};
	SceUID owner;				/* the spinlock holder's thread, or -1 */
	char name[32];
} kmutex_t;

void mutex_name(kmutex_t *mp, char *name);
void mutex_init(kmutex_t *mp, const char *name, kmutex_type_t type, void *arg);
void mutex_destroy(kmutex_t *mp);
void mutex_enter(kmutex_t *mp);
void mutex_exit(kmutex_t *mp);
int mutex_owned(kmutex_t *mp);

#define MUTEX_HELD(mp)		mutex_owned(mp)
#define MUTEX_NOT_HELD(mp)	(!mutex_owned(mp))

extern kmutex_t mod_lock;
