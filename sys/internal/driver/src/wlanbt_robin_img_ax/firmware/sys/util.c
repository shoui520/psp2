/* Bit and byte helpers. */
#include "robin/util.h"

/* The number of bits set in value. */
unsigned int util_CountBitsSet(uint32_t value)
{
	unsigned int count = 0;

	while (value != 0) {
		value &= value - 1;
		++count;
	}
	return count;
}

uint32_t util_Swap32(uint32_t value)
{
	return (value >> 24) | (value << 24) | ((value & 0xff00) << 8) | ((value & 0xff0000) >> 8);
}

void util_SwapBuffer(uint8_t *data, unsigned int length)
{
	uint8_t byte;

	while (length != 0) {
		if (length < 4) {
			if (length == 3) {
				byte = data[2];
				data[2] = data[0];
			} else if (length == 2) {
				byte = data[1];
				data[1] = data[0];
			} else {
				return;
			}
			data[0] = byte;
			return;
		}
		byte = data[3];
		data[3] = data[0];
		data[0] = byte;
		byte = data[2];
		data[2] = data[1];
		data[1] = byte;
		length -= 4;
		data += 4;
	}
}
