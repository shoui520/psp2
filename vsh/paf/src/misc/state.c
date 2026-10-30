/* Miscellaneous record state transitions. */
#include <stdint.h>
#include <sce_atomic.h>

typedef struct PafReferenceBlock {
	uint32_t reserved;
	volatile uint32_t strongCount;
} PafReferenceBlock;

typedef struct PafReferencePair {
	void *object;
	PafReferenceBlock *block;
} PafReferencePair;

typedef struct PafReferenceSource {
	uint8_t reserved[72];
	void *object;
	PafReferenceBlock *block;
} PafReferenceSource;


volatile uint32_t *ScePafMisc_679B51BB(PafReferencePair *destination,
					const PafReferenceSource *source)
{
	PafReferenceBlock *block = source->block;

	destination->object = source->object;
	destination->block = block;
	if (!block)
		return 0;
	sceAtomicIncrement32AcqRel((volatile int32_t *)&block->strongCount);
	return &block->strongCount;
}

typedef struct PafRecordState {
	uint32_t kind;
	float sourceValue;
	uint32_t unused1;
	float computedValue;
	uint32_t flags;
	uint32_t unused2;
	uint32_t weight;
} PafRecordState;

void ScePafMisc_7CF66185(PafRecordState *state)
{
	uint32_t flags = state->flags;

	if ((flags & 0xf000U) == 0x3000U)
		return;
	state->flags = (flags & ~0xf000U) | 0x2000U;
	if ((flags & 0xf00U) == 0xc00U) {
		state->computedValue = 0.0f;
		state->weight = 0;
	} else {
		state->computedValue = state->sourceValue;
		state->weight = 0x3f800000U;
	}
}

typedef struct PafNestedState {
	uint8_t reserved[28];
	int32_t width;
	int32_t height;
	int32_t previousWidth;
	int32_t previousHeight;
	int32_t originWidth;
	int32_t originHeight;
	uint8_t reserved2[16];
	int32_t mode;
	uint8_t reserved3[12];
	uint32_t flags;
} PafNestedState;

typedef struct PafNestedOwner {
	uint8_t reserved[44];
	PafNestedState *state;
} PafNestedOwner;

int ScePafMisc_1E66D069(const PafNestedOwner *owner,
			 const int32_t *size, uint32_t flags)
{
	PafNestedState *state;
	int32_t mode;

	if (size[0] <= 0 || size[1] <= 0)
		return 0;
	state = owner->state;
	mode = state->mode;
	if (mode == 0 || mode == 6 || mode == 7 || mode == 8)
		return 0;
	state->width = size[0];
	state->height = size[1];
	state->previousWidth = 0;
	state->previousHeight = 0;
	state->originWidth = size[0];
	state->originHeight = size[1];
	state->flags = flags;
	return 1;
}
