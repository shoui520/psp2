#include <stdint.h>

int BicUtilStringToInt(const int8_t *text, int base)
{
	const uint8_t *end = (const uint8_t *)text;
	int result = *text;
	int index = 0;
	int multiplier;

	if (!result)
		return result;
	while ((result = *++end) != 0)
		index++;
	multiplier = 1;
	for (;;) {
		int8_t character = text[index];
		unsigned int byte = (uint8_t)character;

		if (byte - '0' <= 9) {
			result += multiplier * (character - '0');
		} else if (byte - 'A' <= 5) {
			if (base != 16)
				return result;
			result += multiplier * (character - 'A' + 10);
		} else if (byte - 'a' <= 5 && base == 16) {
			result += multiplier * (character - 'a' + 10);
		} else {
			return result;
		}
		index--;
		multiplier *= base;
		if (index == -1)
			return result;
	}
}
