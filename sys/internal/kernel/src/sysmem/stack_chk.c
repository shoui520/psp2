#include <sysmem/compiler.h>

#if defined(SYSMEM_SKBL)
/* the boot loader's canary: nothing replaces it, so it is read-only data */
const unsigned int __stack_chk_guard = 0x0b8064e5;
#endif

/* a smashed stack canary: stop at a breakpoint */
void __attribute__((noreturn)) __stack_chk_fail(void)
{
	__breakpoint(BKPT_CHECK_FAILED);
	__builtin_unreachable();
}
