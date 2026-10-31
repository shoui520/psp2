/* Native short nested-field accessors and state setters. */
#include <stdint.h>

void ScePafMisc_741DE852(uint32_t *object)
{
	*object = UINT32_MAX;
}

uint32_t ScePafMisc_D2E471DC(const void *object)
{
	const uint8_t *inner = *(const uint8_t *const *)object;
	return *(const uint32_t *)(inner + 36);
}
