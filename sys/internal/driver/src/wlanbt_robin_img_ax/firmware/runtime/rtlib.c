/*
 * The ARM C library's small Thumb start-up and exit functions (exit 0x525e4, __rt_lib_shutdown
 * 0x52768, __rt_lib_init_noop 0x52820); the rest of the library start-up is boot/reset.S. The
 * filename is inferred.
 */
#include "robin/runtime.h"

void exit(int status)
{
	__rt_exit(status);
}

int __rt_lib_shutdown(void)
{
	return 0;
}

/* The library initialisers __rt_lib_init runs through __rt_lib_init_thunk: there are none. */
void __rt_lib_init_noop(void)
{
}
