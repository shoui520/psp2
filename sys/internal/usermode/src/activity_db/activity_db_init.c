/*
 * SceActivityDb's static objects: their constructors, run at module start, and their
 * destructors through paf's __cxa_finalize at module stop. The names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "../../../../../vsh/paf/src/std/stdc++/atexit.h"
#include "activity_db_init.h"

void *__dso_handle;

/* SNC's linker-provided constructor bounds */
extern void (*__start__Zpreinit_array[])(void);
extern void (*__stop__Zpreinit_array[])(void);
extern void (*__start__Zinit_array[])(void);
extern void (*__stop__Zinit_array[])(void);

__attribute__((noinline)) void activityDbInit(void)
{
	void (**init)(void);

	__dso_handle = &__dso_handle;
	for (init = __start__Zpreinit_array; init < __stop__Zpreinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	for (init = __start__Zinit_array; init < __stop__Zinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
}

__attribute__((noinline)) void activityDbTerm(void)
{
	__cxa_finalize(&__dso_handle);
}
