/* These entry points are fixed in the shipped library. */
#include "halt_assert.h"

int printf_enabled(void)
{
	return 1;
}

void on_error(void)
{
	__breakpoint(0);
}

void CommonDebugBreak(void)
{
	__breakpoint(0);
}
