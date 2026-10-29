/* Pair copies and conditional coordinate insertion. */
#include <stdint.h>

extern void ScePafMisc_447B094F(void *dst, const void *src);

typedef struct PafCoordinateStore {
	uint32_t reserved0;
	int32_t firstLimit[2];
	uint32_t reserved1[3];
	int32_t secondLimit[2];
	uint32_t reserved2[4];
	int32_t firstPrevious[2];
	int32_t secondPrevious[2];
} PafCoordinateStore;

void ScePafMisc_D50E863E(uint32_t *dst, const PafCoordinateStore *src)
{
	uint32_t pair[2];

	ScePafMisc_447B094F(pair, src->firstLimit);
	dst[0] = pair[0];
	dst[1] = pair[1];
}

void ScePafMisc_5F20E13E(uint32_t *dst, const PafCoordinateStore *src)
{
	uint32_t pair[2];

	ScePafMisc_447B094F(pair, src->secondLimit);
	dst[0] = pair[0];
	dst[1] = pair[1];
}

int ScePafMisc_AFAF03B3(PafCoordinateStore *store,
			const int32_t *point)
{
	uint32_t pair[2];

	if (point[0] < 0 || point[1] < 0)
		return 0;
	ScePafMisc_447B094F(pair, store->secondLimit);
	if (point[0] >= (int32_t)pair[0] || point[1] >= (int32_t)pair[1])
		return 0;
	store->secondPrevious[0] = point[0];
	store->secondPrevious[1] = point[1];
	return 1;
}

int ScePafMisc_FBE1B9F7(PafCoordinateStore *store,
			const int32_t *point)
{
	uint32_t pair[2];

	if (point[0] < 0 || point[1] < 0)
		return 0;
	ScePafMisc_447B094F(pair, store->firstLimit);
	if (point[0] >= (int32_t)pair[0] || point[1] >= (int32_t)pair[1])
		return 0;
	store->firstPrevious[0] = point[0];
	store->firstPrevious[1] = point[1];
	return 1;
}
