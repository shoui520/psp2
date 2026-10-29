/* Small original ScePafMisc structure operations. */
#include <stdint.h>

void ScePafMisc_FFE8F348(uint32_t *object)
{
	*object = UINT32_MAX;
}

int ScePafMisc_0EB94344(uint32_t *object, uint32_t first, uint32_t second)
{
	object[17] = second;
	object[16] = first;
	return 1;
}

void ScePafMisc_447B094F(uint32_t *dst, const uint32_t *src)
{
	dst[0] = src[2];
	dst[1] = src[3];
}

void ScePafMisc_A857C48F(uint32_t *object)
{
	object[0] = 0;
	object[2] = 0;
	object[1] = 0;
}

void ScePafMisc_B38C90B6(uint32_t *dst, const uint32_t *src)
{
	dst[0] = src[14];
	dst[1] = src[15];
}

void ScePafMisc_D1E648EF(uint32_t *object)
{
	object[0] = 0;
	object[2] = 0;
	object[1] = 0;
}

float ScePafMisc_D7825ABE(const float *object)
{
	return object[0] + object[2] * 0.5f;
}

void ScePafMisc_E7FC564D(uint32_t *dst, const uint32_t *src)
{
	dst[0] = src[12];
	dst[1] = src[13];
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

void ScePafMisc_EBEBC3F0(uint32_t *dst, const void *object)
{
	const uint32_t *inner =
		*(const uint32_t *const *)((const uint8_t *)object + 44);
	uint32_t first = inner[7], second = inner[8];

	dst[0] = first;
	dst[1] = second;
}

void ScePafMisc_F262A130(uint32_t *dst, const void *object)
{
	const uint32_t *inner =
		*(const uint32_t *const *)((const uint8_t *)object + 44);
	uint32_t first = inner[5], second = inner[6];

	dst[0] = first;
	dst[1] = second;
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

void ScePafMisc_61B990CD(const uint32_t *object, uint32_t *first,
			uint32_t *second)
{
	uint32_t a = object[1], b = object[2];

	*first = a;
	*second = b;
}

void ScePafMisc_9A80FBC3(const uint32_t *object, uint32_t *first,
			uint32_t *second)
{
	uint32_t a = object[1], b = object[2];

	*first = a;
	*second = b;
}

void ScePafMisc_4DEC597D(uint32_t *dst, const void *object)
{
	const uint32_t *inner = *(const uint32_t *const *)
		((const uint8_t *)object + 44);
	uint32_t a = inner[9], b = inner[10];
	uint32_t c = inner[11], d = inner[12];

	dst[0] = a;
	dst[1] = b;
	dst[2] = c;
	dst[3] = d;
}

void ScePafMisc_5C89D36F(uint32_t *dst, const void *object)
{
	const uint32_t *inner = *(const uint32_t *const *)
		((const uint8_t *)object + 44);
	uint32_t a = inner[13], b = inner[14];
	uint32_t c = inner[15], d = inner[16];

	dst[0] = a;
	dst[1] = b;
	dst[2] = c;
	dst[3] = d;
}
