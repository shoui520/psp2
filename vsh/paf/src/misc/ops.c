/* Small original ScePafMisc structure operations. */
#include <stdint.h>
#include <stddef.h>
#include "../std/string.h"

void ScePafMisc_FFE8F348(uint32_t *object)
{
	*object = UINT32_MAX;
}

float ScePafMisc_D7825ABE(const float *object)
{
	return object[0] + object[2] * 0.5f;
}

void ScePafMisc_1C571D8E(float *dst, const float *src)
{
	float a = src[0], b = src[1], c = src[2], d = src[3];

	dst[0] = a;
	dst[1] = b;
	dst[2] = c;
	dst[3] = d;
}

void ScePafMisc_425AD21A(uint32_t *object)
{
	object[0] = 0;
	object[1] = 0;
	object[2] = 0;
	object[3] = 0;
}

void ScePafMisc_6D6E44C1(uint32_t *object)
{
	object[0] = 0;
	object[1] = 0;
	object[3] = 0;
	object[2] = 0;
}

void ScePafMisc_BBC18829(uint32_t *object)
{
	object[0] = 0;
	object[1] = 0;
	object[3] = 0;
	object[2] = 0;
}

void ScePafMisc_CE8FA30A(uint32_t *object)
{
	object[0] = 0;
	object[1] = 0;
	object[2] = 0;
	object[3] = 0;
}

void ScePafMisc_73074712(float *object, float x, float y)
{
	object[0] += x;
	object[1] += y;
}

void ScePafMisc_BAC074DB(uint32_t *object)
{
	object[0] = 0;
	object[1] = 0;
	object[2] = 0x3F800000U;
	object[3] = 0x3F800000U;
}

void ScePafMisc_42A76852(float *object, const float *delta)
{
	float x = delta[0], y = delta[1];

	object[0] += x;
	object[1] += y;
}

void ScePafMisc_3C2E7C36(uint8_t *object, uint32_t first, uint32_t flag,
			uint32_t second, uint32_t third)
{
	int32_t old_flags = (int8_t)object[12];

	*(uint32_t *)(object + 0) = first;
	*(uint32_t *)(object + 4) = second;
	*(uint32_t *)(object + 8) = third;
	object[12] = (uint8_t)((old_flags & ~1) | (flag & 1));
}

int ScePafMisc_D88EEBD8(const uint8_t *object)
{
	const uint32_t *words = (const uint32_t *)object;

	if (!object[0x50])
		return 0;
	if (words[0xc] == 0 && words[0xd] == 0)
		return 0;
	return words[4] == 1;
}

/* Native ordering comparator on the strings held by two key records. */
int ScePafMisc_37897945(void *unused, const char *const *left,
			const char *const *right)
{
	return sce_paf_strcasecmp(*left, *right) > 0;
}

int ScePafMisc_78C1E8AD(const int32_t *object)
{
	return object[4] <= 1;
}

