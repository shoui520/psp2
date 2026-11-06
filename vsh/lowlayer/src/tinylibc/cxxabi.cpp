/*
 * SceTinyLibc's C++ runtime: operator delete frees into the runtime's mspace, and a pure virtual
 * call stops at a breakpoint. The file's name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include "tinylibc.h"

void operator delete(void *pointer)
{
	free(pointer);
}

extern "C" void __cxa_pure_virtual(void)
{
	__breakpoint(0);
}
