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

void ScePafMisc_493F95E4(float *object, float value)
{
	object[2] = value >= 0.0f ? value : 0.0f;
}

void ScePafMisc_824BCB1E(float *object, float value)
{
	object[3] = value >= 0.0f ? value : 0.0f;
}

void ScePafMisc_3291450C(float *object, float width, float height)
{
	object[2] = width >= 0.0f ? width : 0.0f;
	object[3] = height >= 0.0f ? height : 0.0f;
}

void ScePafMisc_6070930D(float *object, float x, float unused_y,
			 float width, float height)
{
	object[0] = x;
	object[1] = 0.0f;
	object[2] = width >= 0.0f ? width : 0.0f;
	object[3] = height >= 0.0f ? height : 0.0f;
}

void ScePafMisc_6377FB6F(float *object, const float *source)
{
	object[0] = source[0];
	object[1] = source[1];
	object[2] = source[2] >= 0.0f ? source[2] : 0.0f;
	object[3] = source[3] >= 0.0f ? source[3] : 0.0f;
}

void ScePafMisc_78B8B32F(float *object, const float *source)
{
	object[0] = source[0];
	object[1] = source[1];
	object[2] = source[2] >= 0.0f ? source[2] : 0.0f;
	object[3] = source[3] >= 0.0f ? source[3] : 0.0f;
}

void ScePafMisc_C6F0C819(float *object, const float *source)
{
	object[0] = source[0];
	object[1] = source[1];
	object[2] = source[2] >= 0.0f ? source[2] : 0.0f;
	object[3] = source[3] >= 0.0f ? source[3] : 0.0f;
}

void ScePafMisc_FDFC29F2(float *object, const float *source)
{
	object[0] = source[0];
	object[1] = source[1];
	object[2] = source[2] >= 0.0f ? source[2] : 0.0f;
	object[3] = source[3] >= 0.0f ? source[3] : 0.0f;
}

void ScePafMisc_669045E4(float *object, const float *position,
			const float *dimensions)
{
	object[0] = position[0];
	object[1] = position[1];
	object[2] = dimensions[0] >= 0.0f ? dimensions[0] : 0.0f;
	object[3] = dimensions[1] >= 0.0f ? dimensions[1] : 0.0f;
}

void ScePafMisc_EEFF1E1C(float *object, const float *position,
			const float *dimensions)
{
	object[0] = position[0];
	object[1] = position[1];
	object[2] = dimensions[0] >= 0.0f ? dimensions[0] : 0.0f;
	object[3] = dimensions[1] >= 0.0f ? dimensions[1] : 0.0f;
}

void ScePafMisc_9EBE483F(float *object, const float *dimensions)
{
	object[2] = dimensions[0] >= 0.0f ? dimensions[0] : 0.0f;
	object[3] = dimensions[1] >= 0.0f ? dimensions[1] : 0.0f;
}

int ScePafMisc_79CFC202(uint8_t *object)
{
	uint32_t first, second;

	if (!object[17])
		return 0;
	first = *(uint32_t *)(object + 8);
	second = *(uint32_t *)(object + 12);
	if (first * second) {
		object[17] = 0;
		object[16] = 0;
	}
	return 1;
}

void ScePafMisc_11D0025A(float *object, const float *position,
			const float *dimensions)
{
	object[0] = position[0];
	object[1] = position[1];
	object[2] = dimensions[0] >= 0.0f ? dimensions[0] : 0.0f;
	object[3] = dimensions[1] >= 0.0f ? dimensions[1] : 0.0f;
}

void ScePafMisc_568EC826(float *object, const float *source)
{
	object[0] = source[0];
	object[1] = source[1];
	object[2] = source[2] >= 0.0f ? source[2] : 0.0f;
	object[3] = source[3] >= 0.0f ? source[3] : 0.0f;
}

int ScePafMisc_00387752(const float *left, const float *right)
{
	return left[0] != right[0] || left[1] != right[1] ||
		left[2] != right[2] || left[3] != right[3];
}

int ScePafMisc_12DB04A6(const float *left, const float *right)
{
	return left[0] != right[0] || left[1] != right[1] ||
		left[2] != right[2] || left[3] != right[3];
}

void ScePafMisc_013C980E(float *object, float value)
{
	float edge = object[0] - object[2] * 0.5f;
	float delta = value - edge;
	if (delta < 0.0f)
		value = edge;
	float sum = edge + value;
	float extent = value - edge;
	object[0] = sum * 0.5f;
	object[2] = extent >= 0.0f ? extent : 0.0f;
}

void ScePafMisc_E0AFE253(float *object, float value)
{
	float edge = object[1] - object[3] * 0.5f;
	float delta = value - edge;
	if (delta < 0.0f)
		value = edge;
	float sum = edge + value;
	float extent = value - edge;
	object[1] = sum * 0.5f;
	object[3] = extent >= 0.0f ? extent : 0.0f;
}

void ScePafMisc_F9FAA877(float *object, float value)
{
	float edge = object[0] + object[2] * 0.5f;
	float delta = edge - value;
	if (delta < 0.0f)
		value = edge;
	float sum = value + edge;
	float extent = edge - value;
	object[0] = sum * 0.5f;
	object[2] = extent >= 0.0f ? extent : 0.0f;
}

void ScePafMisc_77865CCC(float *object, float value)
{
	float edge = object[1] + object[3] * 0.5f;
	float delta = edge - value;
	if (delta < 0.0f)
		value = edge;
	float sum = value + edge;
	float extent = edge - value;
	object[1] = sum * 0.5f;
	object[3] = extent >= 0.0f ? extent : 0.0f;
}

void ScePafMisc_7427A7B9(float *object, float x, float y,
			float width, float height)
{
	object[0] = x;
	object[1] = y;
	object[2] = width >= 0.0f ? width : 0.0f;
	object[3] = height >= 0.0f ? height : 0.0f;
}

void ScePafMisc_8161A692(float *object, float x, float y,
			float width, float height)
{
	object[0] = x;
	object[1] = y;
	object[2] = width >= 0.0f ? width : 0.0f;
	object[3] = height >= 0.0f ? height : 0.0f;
}
