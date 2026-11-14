/*
 * pure_virt.c: __cxa_pure_virtual (libsnc pure_virt.o), the vtable entry of a pure virtual
 * function: calling one is a run-time error.
 */
#include "error.h"

void __cxa_pure_virtual(void)
{
	__abort_execution(ec_pure_virtual_called);
}
