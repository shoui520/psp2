/*
 * Solaris reader-writer locks (Sony's dtrace/sce/solaris_rwlock.c): a kernel
 * mutex guarding the counts, and a semaphore each that readers and the
 * writer wait on.
 */
#pragma once

#include <dtrace/sce/solaris_types.h>

typedef enum {
	RW_DRIVER = 2,
	RW_DEFAULT = 4
} krw_type_t;

typedef enum {
	RW_WRITER = 0,
	RW_READER = 1
} krw_t;

typedef struct krwlock {
	SceUID mutex;			/* "SceDeci4pDtracepRw<name>": guards the counts */
	SceUID readSema;		/* "SceDeci4pDtracepRd<name>" */
	SceUID writeSema;		/* "SceDeci4pDtracepWr<name>" */
	int readers;			/* holding it */
	int readersWaiting;
	int writer;			/* 1 while the writer holds it */
	int writersWaiting;
} krwlock_t;

void rw_init(krwlock_t *rwlp, const char *name, krw_type_t type, void *arg);
void rw_destroy(krwlock_t *rwlp);
void rw_enter(krwlock_t *rwlp, krw_t enter_type);
void rw_exit(krwlock_t *rwlp);

#define RW_WRITE_HELD(rwlp)	((rwlp)->writer == 1)
#define RW_READ_HELD(rwlp)	((rwlp)->readers > 0)
#define RW_LOCK_HELD(rwlp)	(RW_READ_HELD(rwlp) || RW_WRITE_HELD(rwlp))
