/* Native small structured operations. */
#include <stdint.h>

uint32_t ScePafMisc_C160806C(const uint32_t *object)
{
	const uint32_t *inner = (const uint32_t *)(uintptr_t)object[1];
	if (!inner)
		return 0;
	return (inner[1] << 1) + 2;
}

void ScePafMisc_8D99A9E5(uint8_t *object, uint32_t value)
{
	volatile uint32_t *words = (volatile uint32_t *)object;
	words[1] = value;
	object[16] = 0;
	words[0] = 0;
	words[2] = 0;
	words[3] = 0;
	object[17] = 1;
}

void ScePafMisc_FA68B0F5(uint8_t *object, uint32_t value)
{
	volatile uint32_t *words = (volatile uint32_t *)object;
	words[1] = value;
	object[16] = 0;
	words[0] = 0;
	words[2] = 0;
	words[3] = 0;
	object[17] = 1;
}
