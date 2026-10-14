#include <stdint.h>

int BicUtilStringToInt(const int8_t *text, int base);

int UpdateUtilStringToInt(const int8_t *text, int unused, int base)
{
	(void)unused;
	if (!text)
		return 0;
	if (base != 16 && base != 10)
		return 0;
	return BicUtilStringToInt(text, base);
}
