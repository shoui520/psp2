/*
 * SceTinyLibc's start and end: the lock its allocator and exit handlers share, __dso_handle,
 * and the module's constructors (SNC's _Zpreinit_array and _Zinit_array sections, which the
 * linker bounds). The exit handlers' pool is this object's, so every module that links the
 * runtime carries it.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include "tinylibc.h"
#include "tinylibc_internal.h"

typedef void (*TinyLibcInitFunc)(void);

/* the constructors, from the linker */
extern TinyLibcInitFunc __start__Zpreinit_array[] __attribute__((weak));
extern TinyLibcInitFunc __stop__Zpreinit_array[] __attribute__((weak));
extern TinyLibcInitFunc __start__Zinit_array[] __attribute__((weak));
extern TinyLibcInitFunc __stop__Zinit_array[] __attribute__((weak));

/* defined, not common: the linker keeps them in this order */
__attribute__((nocommon)) void *__dso_handle;

__attribute__((nocommon, aligned(8)))
TinyLibcAtexitEntry g_tinyLibcAtexitPool[TINYLIBC_ATEXIT_POOL_COUNT];
__attribute__((nocommon)) SceKernelLwMutexWork g_tinyLibcLock;

void tinyLibcInit(void)
{
	TinyLibcInitFunc *init;

	sceKernelCreateLwMutex(&g_tinyLibcLock, "SceTinyLibc",
			       SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO | SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE,
			       0, NULL);
	__dso_handle = &__dso_handle;

	for (init = __start__Zpreinit_array; init < __stop__Zpreinit_array; init++) {
		if (*init != NULL)
			(*init)();
	}
	for (init = __start__Zinit_array; init < __stop__Zinit_array; init++) {
		if (*init != NULL)
			(*init)();
	}
}
