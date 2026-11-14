/*
 * fegetenv function: store the FPSCR as the floating-point environment.
 */
#include <fenv.h>

int fegetenv(fenv_t *penv)
{
	*penv = __builtin_fpscr();
	return 0;
}
