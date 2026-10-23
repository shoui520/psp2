/* The native abort shim cannot return. */
extern void abort(void) __attribute__((noreturn));

/* SCE: the runtime has no __gnu_unwind_frame; nothing calls it, so it stays file-local and
 * the compiler drops it. */
#define __gnu_unwind_frame static __attribute__((unused)) __gnu_unwind_frame

#include "../../contrib/gcc-4.8-2014.05/libgcc/config/arm/pr-support.c"
