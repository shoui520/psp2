/* The native abort shim cannot return. */
extern void abort(void) __attribute__((noreturn));

#include "../../../vendor/gcc-linaro-4.8-2014.04/libgcc/config/arm/pr-support.c"
