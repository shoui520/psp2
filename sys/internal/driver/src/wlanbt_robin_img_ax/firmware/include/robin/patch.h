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

typedef int (*RobinPatchHook)(uintptr_t a0, uintptr_t a1, uintptr_t a2,
			      uintptr_t a3);

int robinNoOverride(void);

#define ROBIN_PATCH_SLOT(name) \
	RobinPatchHook name __attribute__((section(".itcm.rw"))) = \
		(RobinPatchHook)robinNoOverride

#endif /* ROBIN_PATCH_H */
