/* Native one- and two-instruction ScePafMisc scalar accessors. */
#include <stdint.h>

float ScePafMisc_17260154(const void *object)
{
	return *(const float *)((const uint8_t *)object + 12);
}

float ScePafMisc_488437AD(const void *object)
{
	return *(const float *)((const uint8_t *)object + 8);
}

void ScePafMisc_617E1590(void *object, float value)
{
	*(float *)object = value;
}

void ScePafMisc_652628EF(void *object, float value)
{
	*(float *)((uint8_t *)object + 4) = value;
}

float ScePafMisc_6E5A46EC(const void *object)
{
	return *(const float *)object;
}

float ScePafMisc_83A59874(const void *object)
{
	return *(const float *)((const uint8_t *)object + 4);
}

uint32_t ScePafMisc_8830A864(const void *object)
{
	return *(const uint8_t *)((const uint8_t *)object + 17);
}

int ScePafMisc_C8E1B9B2(void)
{
	return 18;
}

uint32_t ScePafMisc_F58AF47A(const void *object)
{
	return *(const uint32_t *)((const uint8_t *)object + 76);
}

float ScePafMisc_133C6F11(const float *object)
{
	return object[1] + object[3] * 0.5f;
}

float ScePafMisc_298EA287(const float *object)
{
	return object[0] - object[2] * 0.5f;
}

float ScePafMisc_2AC7BCC1(const float *object)
{
	return object[1] - object[3] * 0.5f;
}

float ScePafMisc_AAE49140(const float *object)
{
	return object[2] * object[3];
}

void ScePafMisc_C5D73892(float *object, float first, float second)
{
	object[0] = first;
	object[1] = second;
}

void ScePafMisc_BAD25161(float *dst, const float *src)
{
	dst[0] = src[0];
	dst[1] = src[1];
}

void ScePafMisc_4FA4979E(float *dst, const float *src)
{
	dst[0] = src[2];
	dst[1] = src[3];
}

void ScePafMisc_CF61842A(float *dst, const float *src)
{
	dst[0] = src[0];
	dst[1] = src[1];
}
