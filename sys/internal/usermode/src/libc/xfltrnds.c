/*
 * _Fltrounds function: FLT_ROUNDS, the current rounding mode as float.h
 * numbers it.
 */
#include <fenv.h>

int _Fltrounds(void)
{
	switch (fegetround()) {
	case FE_TOWARDZERO:
		return 0;
	case FE_TONEAREST:
		return 1;
	case FE_UPWARD:
		return 2;
	case FE_DOWNWARD:
		return 3;
	default:
		return -1;
	}
}
