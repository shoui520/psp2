/* Positive power-of-two validation for dimension setters. */
#include <stdint.h>

typedef struct PafPower2Dimensions {
	uint8_t reserved[80];
	uint16_t width;
	uint16_t height;
} PafPower2Dimensions;

typedef struct PafPower2Owner {
	uint8_t reserved[44];
	PafPower2Dimensions *dimensions;
} PafPower2Owner;

static int IsPositivePowerOfTwo(int32_t value)
{
	uint32_t bits = (uint32_t)value;

	return value > 0 && (bits & (bits - 1)) == 0;
}

int ScePafMisc_C875DB31(const void *unused, int32_t value)
{
	(void)unused;
	return IsPositivePowerOfTwo(value);
}

int ScePafMisc_0C98E962(PafPower2Owner *owner, int32_t value)
{
	if (!IsPositivePowerOfTwo(value))
		return 0;
	owner->dimensions->width = (uint16_t)value;
	return 1;
}

int ScePafMisc_BF9CCCBF(PafPower2Owner *owner, int32_t value)
{
	if (!IsPositivePowerOfTwo(value))
		return 0;
	owner->dimensions->height = (uint16_t)value;
	return 1;
}
