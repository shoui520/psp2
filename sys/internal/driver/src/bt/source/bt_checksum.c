/* Bluetooth checksum adapters; the original source filename is unavailable. */
#include <stdint.h>

extern const uint32_t gBtCrc32Table[256];

uint32_t BtUpdateCrc32(uint32_t checksum, const uint8_t *source, int length)
{
	if (length > 0) {
		const uint8_t *end = source + length;

		do {
			checksum = gBtCrc32Table[(uint8_t)(*source++ ^ checksum)] ^
				   (checksum >> 8);
		} while (source != end);
	}
	return checksum;
}
