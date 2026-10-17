/* The native abort shim cannot return. */
extern void abort(void) __attribute__((noreturn));

#include "../../contrib/gcc-4.8-2014.05/libgcc/config/arm/pr-support.c"
