/*
 * fegetround function: the current rounding mode. Sony's build does not read
 * the FPSCR: it returns the entry of the mode table for the RMode field's
 * reset value, round to nearest.
 */
#include <fenv.h>

static const int rmodes[] = {	/* the FPSCR's RMode field as an FE_ value */
	FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO,
};

int fegetround(void)
{
	return rmodes[0];
}
