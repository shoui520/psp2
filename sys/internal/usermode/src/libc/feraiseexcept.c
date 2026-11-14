/*
 * feraiseexcept function: set the exception flags in the FPSCR, and for each
 * exception whose trap is enabled perform an operation that raises it.
 */
#include <fenv.h>

static struct {	/* an operation that raises each exception */
	int except;
	double num;
	double den;
} raise_ops[] = {
	{FE_INVALID, 0.0, 0.0},
	{FE_DIVBYZERO, 1.0, 0.0},
	{FE_OVERFLOW, 1e300, 1e-300},
	{FE_UNDERFLOW, 1e-300, 1e300},
	{FE_INEXACT, 2.0, 3.0},
};

static __attribute__((noinline)) double raise_trapped(int except)
{
	double ans = 0.0;
	unsigned int i;

	for (i = 0; i < sizeof(raise_ops) / sizeof(raise_ops[0]); ++i) {
		if (raise_ops[i].except & except)
			ans = raise_ops[i].num / raise_ops[i].den;
	}
	return ans;
}

int feraiseexcept(int except)
{
	fenv_t env;

	if ((except &= FE_ALL_EXCEPT) != 0) {
		fegetenv(&env);
		env |= except;
		fesetenv(&env);
		if ((except &= env >> 8) != 0)	/* the enabled traps */
			raise_trapped(except);
	}
	return 0;
}
