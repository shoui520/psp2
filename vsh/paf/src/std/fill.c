/* Repeat a 32-bit fill pattern without imposing word alignment. */
#include <stddef.h>
#include <stdint.h>

void *ScePafStdc_8A58EE24(void *destination, uint32_t pattern, size_t words)
{
	uint8_t *out = (uint8_t *)destination;
	size_t i;

	for (i = 0; i < words; i++) {
		out[0] = (uint8_t)pattern;
		out[1] = (uint8_t)(pattern >> 8);
		out[2] = (uint8_t)(pattern >> 16);
		out[3] = (uint8_t)(pattern >> 24);
		out += 4;
	}
	return destination;
}
