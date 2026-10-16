/* Unaligned-access runtime family; the filename is inferred from the ABI. */
#include <stdint.h>
#include "robin/runtime.h"

int __aeabi_uwrite4(int value, void *address)
{
	uint8_t *bytes = address;
	uint32_t word = (uint32_t)value;

	bytes[0] = (uint8_t)word;
	bytes[1] = (uint8_t)(word >> 8);
	bytes[2] = (uint8_t)(word >> 16);
	bytes[3] = (uint8_t)(word >> 24);
	return value;
}
