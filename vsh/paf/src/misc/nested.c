/* Native short nested-field accessors and state setters. */
#include <stdint.h>

uint32_t ScePafMisc_147115CE(const void *object)
{
	const uint8_t *inner = *(const uint8_t *const *)((const uint8_t *)object + 44);
	return *(const uint32_t *)(inner + 4);
}

uint32_t ScePafMisc_2795D5F1(const void *object)
{
	const uint8_t *inner = *(const uint8_t *const *)((const uint8_t *)object + 44);
	return *(const uint32_t *)(inner + 8);
}

uint32_t ScePafMisc_3E579D64(const void *object)
{
	const uint8_t *inner = *(const uint8_t *const *)((const uint8_t *)object + 44);
	return *(const uint32_t *)(inner + 72);
}

uint32_t ScePafMisc_73032689(const void *object)
{
	const uint8_t *inner = *(const uint8_t *const *)((const uint8_t *)object + 44);
	return *(const uint16_t *)(inner + 80);
}

uint32_t ScePafMisc_7B15112F(const void *object)
{
	const uint8_t *inner = *(const uint8_t *const *)((const uint8_t *)object + 44);
	return *(const uint32_t *)(inner + 12);
}

uint32_t ScePafMisc_9ADD8A26(const void *object)
{
	const uint8_t *inner = *(const uint8_t *const *)((const uint8_t *)object + 44);
	return *(const uint32_t *)(inner + 16);
}

uint32_t ScePafMisc_A664B8C0(const void *object)
{
	const uint8_t *inner = *(const uint8_t *const *)((const uint8_t *)object + 44);
	return *(const uint16_t *)(inner + 82);
}

uint32_t ScePafMisc_BF87C099(const void *object)
{
	const uint8_t *inner = *(const uint8_t *const *)((const uint8_t *)object + 44);
	return *(const uint32_t *)(inner + 68);
}

uint32_t ScePafMisc_D8F98A86(const void *object)
{
	const uint8_t *inner = *(const uint8_t *const *)((const uint8_t *)object + 44);
	return *(const uint32_t *)(inner + 76);
}

uint32_t ScePafMisc_97DBBDA7(const uint32_t *object)
{
	return object[2] * object[3];
}

void ScePafMisc_741DE852(uint32_t *object)
{
	*object = UINT32_MAX;
}

uint32_t ScePafMisc_D2E471DC(const void *object)
{
	const uint8_t *inner = *(const uint8_t *const *)object;
	return *(const uint32_t *)(inner + 36);
}
