/*
 * ROM-patch override slots. The firmware calls many of its functions through
 * a table of pointers first; every slot starts at robinNoOverride(), which
 * returns 0 ("not handled"), and a patch image may install its own handler.
 * A handler gets the arguments of the function it overrides, followed by a
 * pointer to the result when the function returns a value, and returns
 * nonzero to say it handled the call.
 */
#ifndef ROBIN_PATCH_H
#define ROBIN_PATCH_H

#include <stdint.h>

/* Unprototyped: each slot is called with its own function's arguments. */
typedef int (*RobinPatchHook)();

int robinNoOverride(void);

#define ROBIN_PATCH_SLOT(name) \
	RobinPatchHook name __attribute__((section(".itcm.rw"))) = \
		(RobinPatchHook)robinNoOverride

#endif /* ROBIN_PATCH_H */
