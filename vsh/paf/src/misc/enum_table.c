/* Element-size lookups and grid entry operations. */
#include <stdint.h>
#include "../memory/heap_allocator.h"
#include "../std/stdlib.h"
#include "../std/new.h"

typedef struct PafEnumDescriptor {
	uint32_t reserved;
	int32_t firstFormat;
	int32_t secondFormat;
	uint32_t unused;
	uint32_t count;
} PafEnumDescriptor;

typedef struct PafGridEntry {
	void *object;
	uint32_t elementSize;
} PafGridEntry;

typedef struct PafGridInterface PafGridInterface;

typedef struct PafGridInterfaceMethods {
	void *reserved[2];
	int32_t (*queryCapacity)(PafGridInterface *object);
	int32_t (*querySize)(PafGridInterface *object);
} PafGridInterfaceMethods;

struct PafGridInterface {
	const PafGridInterfaceMethods *methods;
};

int32_t ScePafMisc_54E90DE0(PafGridInterface *const *source)
{
	PafGridInterface *object = *source;

	return object ? object->methods->queryCapacity(object) : 0;
}

int32_t ScePafMisc_C63958CC(PafGridInterface *const *source)
{
	PafGridInterface *object = *source;

	return object ? object->methods->querySize(object) : 0;
}

typedef struct PafGrid {
	PafGridEntry *entries;
	HeapAllocator *allocator;
	int32_t width;
	int32_t height;
	uint8_t ownsElements;
	uint8_t cleared;
} PafGrid;

typedef struct PafDualGrid {
	uint32_t reserved;
	PafGrid primary;
	PafGrid secondary;
} PafDualGrid;

typedef struct PafGridOwner {
	void *primaryObject;
	PafGrid primary;
	PafGrid secondary;
	void *secondaryObject;
	uint8_t reserved[24];
	void *buffer;
	uint32_t bufferLength;
	HeapAllocator *allocator;
} PafGridOwner;

__attribute__((noinline)) PafGrid *ScePafMisc_3D747A33(PafGrid *grid);

_Static_assert(sizeof(PafGrid) == 20, "PafGrid size");
_Static_assert(offsetof(PafDualGrid, primary) == 4, "first grid offset");
_Static_assert(offsetof(PafDualGrid, secondary) == 24, "second grid offset");
_Static_assert(offsetof(PafGridOwner, secondaryObject) == 44,
	       "owned grid secondary object offset");
_Static_assert(offsetof(PafGridOwner, buffer) == 72,
	       "owned grid buffer offset");

PafGridOwner *ScePafMisc_05ECDDBF(PafGridOwner *self)
{
	void *object = self->secondaryObject;

	if (object) {
		((void (*)(void *))((void **) *(void **)object)[5])(object);
		self->secondaryObject = 0;
	}
	object = self->primaryObject;
	if (object) {
		((void (*)(void *))((void **) *(void **)object)[5])(object);
		self->primaryObject = 0;
	}
	if (self->buffer) {
		HeapAllocator *allocator = self->allocator;

		if (allocator)
			HeapAllocatorFree(allocator, self->buffer);
		else
			sce_paf_free(self->buffer);
		self->buffer = 0;
		self->bufferLength = 0;
	}
	ScePafMisc_3D747A33(&self->secondary);
	ScePafMisc_3D747A33(&self->primary);
	return self;
}

/* This grid can own the buffers stored in each entry, independently of its
 * own entry-array allocation.  The native loop rechecks its dimensions after
 * each free, since allocator callbacks can affect the owner. */
int ScePafMisc_1F7BB8D0(PafGrid *grid)
{
	if (!grid->cleared) {
		uint32_t count = (uint32_t)grid->width * (uint32_t)grid->height;
		uint32_t index;

		if (grid->ownsElements) {
			for (index = 0; index < count; index++) {
				PafGridEntry *entry = &grid->entries[index];
				HeapAllocator *allocator = grid->allocator;

				if (allocator)
					HeapAllocatorFree(allocator, entry->object);
				else
					sce_paf_free(entry->object);
				entry->elementSize = 0;
				entry->object = 0;
				count = (uint32_t)grid->width * (uint32_t)grid->height;
			}
		} else {
			for (index = 0; index < count; index++) {
				grid->entries[index].object = 0;
				grid->entries[index].elementSize = 0;
			}
		}
		grid->ownsElements = 0;
		grid->cleared = 1;
	}
	return 1;
}

int ScePafMisc_6E026A17(PafDualGrid *self)
{
	int first = ScePafMisc_1F7BB8D0(&self->primary);
	int second = ScePafMisc_1F7BB8D0(&self->secondary);

	return first != 0 && second != 0;
}

PafGrid *ScePafMisc_3D747A33(PafGrid *grid)
{
	ScePafMisc_1F7BB8D0(grid);
	PafDeleteArray(grid->entries);
	grid->entries = 0;
	return grid;
}

PafGrid *ScePafMisc_823265B6(PafGrid *grid)
{
	ScePafMisc_1F7BB8D0(grid);
	PafDeleteArray(grid->entries);
	grid->entries = 0;
	return grid;
}

int ScePafMisc_4D549043(PafGrid *grid)
{
	ScePafMisc_1F7BB8D0(grid);
	PafDeleteArray(grid->entries);
	grid->entries = 0;
	return 1;
}

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
