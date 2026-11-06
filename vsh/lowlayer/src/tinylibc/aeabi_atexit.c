/*
 * SceTinyLibc's __aeabi_atexit: the destroyer of a static object, kept for __cxa_finalize in the
 * exit handlers' pool, else in an entry from the mspace. The file's name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include "tinylibc.h"
#include "tinylibc_internal.h"

int __aeabi_atexit(void *object, void (*destroyer)(void *), void *dso)
{
	TinyLibcAtexitEntry *entry;
	void *handle;
	int i;

	sceKernelLockLwMutex(&g_tinyLibcLock, 1, NULL);
	entry = NULL;
	for (i = 0; i < TINYLIBC_ATEXIT_POOL_COUNT; i++) {
		if (g_tinyLibcAtexitPool[i].func == NULL) {
			entry = &g_tinyLibcAtexitPool[i];
			break;
		}
	}
	if (entry == NULL) {
		entry = (TinyLibcAtexitEntry *)malloc(sizeof(*entry));
		if (entry == NULL) {
			/* Sony's: locks again where it should unlock */
			sceKernelLockLwMutex(&g_tinyLibcLock, 1, NULL);
			return 1;
		}
	}
	handle = *(void **)dso;
	entry->func = destroyer;
	entry->next = g_tinyLibcAtexitHead;
	entry->arg = object;
	entry->dso = handle;
	g_tinyLibcAtexitHead = entry;
	sceKernelUnlockLwMutex(&g_tinyLibcLock, 1);
	return 0;
}
