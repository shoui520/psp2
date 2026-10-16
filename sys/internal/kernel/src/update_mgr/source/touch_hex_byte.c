#include "touch_update.h"

int TouchUtilReadHexByte(const int8_t *buffer, uint64_t *position)
{
	int8_t text[3];

	__builtin_memcpy(text, buffer + (uint32_t)*position, 2);
	*position += 2;
	text[2] = 0;
	return UpdateUtilStringToInt(text, 0, 16);
}
