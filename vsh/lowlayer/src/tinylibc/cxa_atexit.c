/*
 * SceTinyLibc's exit handlers: __cxa_finalize runs those registered for a module, newest first
 * (__aeabi_atexit registers them: aeabi_atexit.c).
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include "tinylibc.h"
#include "tinylibc_internal.h"

TinyLibcAtexitEntry *g_tinyLibcAtexitHead = NULL;

/* an entry from the pool goes back to it, any other to the mspace */
static inline void freeAtexitEntry(TinyLibcAtexitEntry *entry)
{
	int i;

	for (i = 0; i < TINYLIBC_ATEXIT_POOL_COUNT; i++) {
		if (&g_tinyLibcAtexitPool[i] == entry) {
			entry->func = NULL;
			return;
		}
	}
	free(entry);
}

/* the handlers registered for dso; the walk restarts when a handler changes the list */
void __cxa_finalize(void *dso)
{
	TinyLibcAtexitEntry *entry;
	TinyLibcAtexitEntry *next;
	TinyLibcAtexitEntry *prev;
	TinyLibcAtexitEntry *head;
	TinyLibcAtexitEntry *first;
	void (*func)(void *arg);
	void *arg;

	if (dso != __dso_handle)
		__breakpoint(1);

	sceKernelLockLwMutex(&g_tinyLibcLock, 1, NULL);
	if (dso != NULL) {
		head = g_tinyLibcAtexitHead;
		entry = head;
		first = entry;
		prev = NULL;
		while (entry != NULL) {
			next = entry->next;
			if (entry->dso == *(void **)dso) {
				if (entry == head) {
					g_tinyLibcAtexitHead = next;
					first = next;
				}
				if (prev != NULL)
					prev->next = next;
				sceKernelUnlockLwMutex(&g_tinyLibcLock, 1);

				func = entry->func;
				arg = entry->arg;
				freeAtexitEntry(entry);
				func(arg);

				sceKernelLockLwMutex(&g_tinyLibcLock, 1, NULL);
				head = g_tinyLibcAtexitHead;
			} else {
				prev = entry;
			}
			entry = next;
			if (first != head) {
				entry = head;
				first = entry;
			}
		}
	}
	sceKernelUnlockLwMutex(&g_tinyLibcLock, 1);
}
