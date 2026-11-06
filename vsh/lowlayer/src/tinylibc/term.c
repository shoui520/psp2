/* SceTinyLibc's end, for a module that stops: its exit handlers, then the lock. */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include "tinylibc.h"
#include "tinylibc_internal.h"

void tinyLibcTerm(void)
{
	__cxa_finalize(&__dso_handle);
	sceKernelDeleteLwMutex(&g_tinyLibcLock);
}
