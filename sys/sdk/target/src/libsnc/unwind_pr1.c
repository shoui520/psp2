/*
 * __aeabi_unwind_cpp_pr1, one of the ARM EHABI personality routines that SNC's
 * runtime library (libsnc, simple_unwind_pr.o) links into SNC-built modules:
 * it forwards to the common routine when a C++ unwinder is linked in and
 * otherwise reports _URC_FAILURE.  Nothing in a C module calls it; the
 * exception index entries refer to it.  The SNC linker keeps only the
 * routines a module refers to, hence one file per routine.
 */

#include <stddef.h>

#include "unwind_pr.h"

#define URC_FAILURE	9

extern int __ARM_unwind_cpp_prcommon(int state, void *ucbp, void *context,
				     int index) __attribute__((weak));

__attribute__((noinline))
int __aeabi_unwind_cpp_pr1(int state, void *ucbp, void *context)
{
	if (__ARM_unwind_cpp_prcommon != NULL)
		return __ARM_unwind_cpp_prcommon(state, ucbp, context, 1);
	return URC_FAILURE;
}
