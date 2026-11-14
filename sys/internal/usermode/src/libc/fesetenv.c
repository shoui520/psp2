/*
 * fesetenv function: load the floating-point environment into the FPSCR.
 */
#include <fenv.h>

int fesetenv(const fenv_t *penv)
{
	__builtin_set_fpscr(*penv);
	return 0;
}
