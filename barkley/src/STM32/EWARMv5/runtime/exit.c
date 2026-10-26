/*
 * exit: the functions registered with __cxa_atexit (the static destructors, the locks'
 * clean-up) called, last first, the streams closed, and the debugger told (__exit).
 */

#include "xstdio.h"

#define DYNEXIT_MAX	32

struct dynexit {
	void *arg;
	void (*func)(void *arg);
	void *dso;
};

void __exit(int status);

static struct dynexit dynexit[DYNEXIT_MAX];
static struct dynexit *dynexitNext = dynexit;

/* 0, or 1 when full */
int __cxa_atexit(void (*func)(void *), void *arg, void *dso)
{
	struct dynexit *e = dynexitNext;

	if (e >= dynexit + DYNEXIT_MAX)
		return 1;
	dynexitNext = e + 1;
	if (e == 0)
		return 1;
	e->arg = arg;
	e->func = func;
	e->dso = dso;
	return 0;
}

/* every function registered, whatever its dso; one registered meanwhile is called too */
__attribute__((noinline)) void __cxa_finalize(void *dso)
{
	struct dynexit *last = dynexitNext, *e = last;
	void (*func)(void *);

	if (e == 0)
		return;
	while (e != dynexit) {
		if (last != dynexitNext) {
			e = dynexitNext;
			last = e;
		}
		e--;
		func = e->func;
		if (func != 0) {
			e->func = 0;
			if (e->arg != 0)
				func(e->arg);
			else
				((void (*)(void))func)();
		}
	}
}

__attribute__((noinline)) void __call_dtors(void)
{
	__cxa_finalize(0);
}

void exit(int status)
{
	__call_dtors();
	_Close_all();
	for (;;)
		__exit(status);
}
