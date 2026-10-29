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

void ScePafMisc_50D7A1CB(float *object, float value)
{
	float middle = object[1];
	float half_width = object[3] * 0.5f;
	float left = middle + half_width;
	float difference = value - left;

	object[1] = middle + difference;
}

void ScePafMisc_8D824321(float *object, float value)
{
	float middle = object[0];
	float half_width = object[2] * 0.5f;
	float left = middle - half_width;
	float difference = value - left;

	object[0] = middle + difference;
}

void ScePafMisc_A76AD90C(float *object, float value)
{
	float middle = object[1];
	float half_width = object[3] * 0.5f;
	float left = middle - half_width;
	float difference = value - left;

	object[1] = middle + difference;
}

void ScePafMisc_DC12F9E7(float *object, float value)
{
	float middle = object[0];
	float half_width = object[2] * 0.5f;
	float left = middle + half_width;
	float difference = value - left;

	object[0] = middle + difference;
}

void ScePafMisc_5D8D81CB(float *destination, const float *rectangle)
{
	float x = rectangle[0], y = rectangle[1];
	float width = rectangle[2], height = rectangle[3];

	destination[0] = x - width * 0.5f;
	destination[1] = y - height * 0.5f;
}

void ScePafMisc_63F00016(float *destination, const float *rectangle)
{
	float x = rectangle[0], y = rectangle[1];
	float width = rectangle[2], height = rectangle[3];

	destination[0] = x - width * 0.5f;
	destination[1] = y + height * 0.5f;
}

void ScePafMisc_A4F47E0A(float *destination, const float *rectangle)
{
	float x = rectangle[0], y = rectangle[1];
	float width = rectangle[2], height = rectangle[3];

	destination[0] = x + width * 0.5f;
	destination[1] = y - height * 0.5f;
}

void ScePafMisc_B421A294(float *destination, const float *rectangle)
{
	float x = rectangle[0], y = rectangle[1];
	float width = rectangle[2], height = rectangle[3];

	destination[0] = x + width * 0.5f;
	destination[1] = y + height * 0.5f;
}
