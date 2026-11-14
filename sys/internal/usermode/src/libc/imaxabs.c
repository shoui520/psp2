/* imaxabs function */
#include <inttypes.h>

intmax_t imaxabs(intmax_t i)
{
	return i < 0 ? -i : i;
}
