/*
 * SceTinyLibc's start: the lock its allocator and exit handlers share, __dso_handle, and the
 * module's constructors (SNC's _Zpreinit_array and _Zinit_array sections, which the linker
 * bounds). The original's bss also holds the 128 exit-handler entries (16 bytes each) that
 * trigger_util's __cxa_atexit fills; they come with that code.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include "tinylibc.h"

typedef void (*TinyLibcInitFunc)(void);

/* the constructors, from the linker */
extern TinyLibcInitFunc __start__Zpreinit_array[] __attribute__((weak));
extern TinyLibcInitFunc __stop__Zpreinit_array[] __attribute__((weak));
extern TinyLibcInitFunc __start__Zinit_array[] __attribute__((weak));
extern TinyLibcInitFunc __stop__Zinit_array[] __attribute__((weak));

void *__dso_handle;

static SceKernelLwMutexWork s_lock;

void tinyLibcInit(void)
{
	TinyLibcInitFunc *init;

	sceKernelCreateLwMutex(&s_lock, "SceTinyLibc",
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
