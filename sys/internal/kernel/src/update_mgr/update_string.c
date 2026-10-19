#include <stdint.h>
#include "bic_update.h"

int UpdateUtilStringToInt(const int8_t *text, int unused, int base)
{
	(void)unused;
	if (!text)
		return 0;
	if (base != 16 && base != 10)
		return 0;
	return BicUtilStringToInt(text, base);
}
