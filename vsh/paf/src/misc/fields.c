/* Native one- and two-instruction ScePafMisc scalar accessors. */
#include <stdint.h>

float ScePafMisc_17260154(const void *object)
{
	return *(const float *)((const uint8_t *)object + 12);
}

uint32_t ScePafMisc_3A9F5294(const void *object)
{
	return *(const uint32_t *)((const uint8_t *)object + 44);
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

void *ScePafMisc_7270DC05(void *object)
{
	return (uint8_t *)object + 12;
}

float ScePafMisc_83A59874(const void *object)
{
	return *(const float *)((const uint8_t *)object + 4);
}

uint32_t ScePafMisc_8830A864(const void *object)
{
	return *(const uint8_t *)((const uint8_t *)object + 17);
}

uint32_t ScePafMisc_BC1135F2(const void *object)
{
	return *(const uint32_t *)((const uint8_t *)object + 96);
}

int ScePafMisc_C8E1B9B2(void)
{
	return 18;
}

uint32_t ScePafMisc_D5DCD40C(const void *object)
{
	return *(const uint32_t *)((const uint8_t *)object + 96);
}

void *ScePafMisc_E904B4E0(void *object)
{
	return (uint8_t *)object + 16;
}

uint32_t ScePafMisc_F58AF47A(const void *object)
{
	return *(const uint32_t *)((const uint8_t *)object + 76);
}

int ScePafMisc_FBD3FF64(void)
{
	return 2;
}
