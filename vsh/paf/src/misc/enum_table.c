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

/* paf::image::ImageArray::~ImageArray and Free (image_array.cpp). */
PafGrid *ScePafMisc_3D747A33(PafGrid *grid);
int ScePafMisc_1F7BB8D0(PafGrid *grid);

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

int ScePafMisc_6E026A17(PafDualGrid *self)
{
	int first = ScePafMisc_1F7BB8D0(&self->primary);
	int second = ScePafMisc_1F7BB8D0(&self->secondary);

	return first != 0 && second != 0;
}

