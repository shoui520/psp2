/* Element-size lookups and grid entry operations. */
#include <stdint.h>

typedef struct PafEnumDescriptor {
	uint32_t reserved;
	int32_t firstFormat;
	int32_t secondFormat;
	uint32_t unused;
	uint32_t count;
} PafEnumDescriptor;

typedef struct PafGridEntry {
	uint32_t object;
	uint32_t elementSize;
} PafGridEntry;

typedef struct PafGrid {
	PafGridEntry *entries;
	uint32_t reserved;
	int32_t width;
	int32_t height;
} PafGrid;

uint32_t ScePafMisc_B207334F(int32_t format)
{
	switch (format) {
	case 0:
	case 1:
	case 2:
	case 23:
		return 16;
	case 3:
	case 4:
	case 6:
	case 24:
		return 32;
	case 14:
	case 15:
	case 18:
	case 20:
	case 22:
	case 29:
	case 30:
	case 31:
	case 32:
		return 8;
	case 16:
	case 21:
		return 4;
	case 25:
		return 24;
	default:
		return 0;
	}
}

uint32_t ScePafMisc_24015182(const PafEnumDescriptor *desc)
{
	return ScePafMisc_B207334F(desc->firstFormat);
}

uint32_t ScePafMisc_BD8A2E25(const PafEnumDescriptor *desc)
{
	return ScePafMisc_B207334F(desc->secondFormat);
}

uint32_t ScePafMisc_A48D0607(const PafEnumDescriptor *desc)
{
	return ScePafMisc_B207334F(desc->secondFormat) >> 3;
}

uint32_t ScePafMisc_C07E8C83(const PafEnumDescriptor *desc)
{
	return ScePafMisc_B207334F(desc->firstFormat) >> 3;
}

int ScePafMisc_2FEF71B0(PafGrid *grid, const PafEnumDescriptor *desc)
{
	uint32_t elementSize = ScePafMisc_A48D0607(desc) * desc->count;
	uint32_t count = (uint32_t)grid->width * (uint32_t)grid->height;
	uint32_t i;

	for (i = 0; i < count; i++)
		grid->entries[i].elementSize = elementSize;
	return 1;
}

int ScePafMisc_B7CA61AD(PafGrid *grid, const int32_t *position,
			 uint32_t elementSize)
{
	int32_t x = position[0], y = position[1];

	if (x < 0 || y < 0 || x >= grid->width || y >= grid->height)
		return 0;
	grid->entries[y * grid->width + x].elementSize = elementSize;
	return 1;
}
