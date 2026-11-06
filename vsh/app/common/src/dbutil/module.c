/*
 * SceDbutil's start and stop: the constructors of its static objects, and their destructors
 * through paf's __cxa_finalize. The names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "../../../../paf/src/std/stdc++/atexit.h"

void *__dso_handle;

/* SNC's linker-provided constructor bounds */
extern void (*__start__Zpreinit_array[])(void);
extern void (*__stop__Zpreinit_array[])(void);
extern void (*__start__Zinit_array[])(void);
extern void (*__stop__Zinit_array[])(void);

static void dbutilInit(void);
static void dbutilTerm(void);

int module_start(SceSize argSize, const void *argp)
{
	dbutilInit();
	return 0;
}

int module_stop(SceSize argSize, const void *argp)
{
	dbutilTerm();
	return 0;
}

static __attribute__((noinline)) void dbutilInit(void)
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

static __attribute__((noinline)) void dbutilTerm(void)
{
	__cxa_finalize(&__dso_handle);
}
