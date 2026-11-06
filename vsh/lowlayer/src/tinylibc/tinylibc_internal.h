/*
 * What SceTinyLibc's objects share: __dso_handle, the lock and the exit handlers' pool
 * (tinylibc.c), and the runtime's mspace (malloc.c). The names are ours.
 */
#ifndef TINYLIBC_TINYLIBC_INTERNAL_H
#define TINYLIBC_TINYLIBC_INTERNAL_H

#include <scetypes.h>
#include <kernel/threadmgr.h>

/* the exit handlers __cxa_atexit keeps before it allocates */
#define TINYLIBC_ATEXIT_POOL_COUNT	128

typedef struct TinyLibcAtexitEntry {
	struct TinyLibcAtexitEntry *next;
	void (*func)(void *arg);
	void *arg;
	void *dso;
} TinyLibcAtexitEntry;

extern void *__dso_handle;
extern TinyLibcAtexitEntry g_tinyLibcAtexitPool[TINYLIBC_ATEXIT_POOL_COUNT];
extern SceKernelLwMutexWork g_tinyLibcLock;
extern void *g_tinyLibcMspace;

#endif
