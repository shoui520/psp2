#include <sysmem/compiler.h>

/* a smashed stack canary: stop at a breakpoint */
void __attribute__((noreturn)) __stack_chk_fail(void)
{
	__breakpoint(BKPT_CHECK_FAILED);
	__builtin_unreachable();
}
